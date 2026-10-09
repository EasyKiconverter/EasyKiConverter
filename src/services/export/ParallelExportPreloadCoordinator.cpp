#include "ParallelExportPreloadCoordinator.h"

#include "ParallelExportService.h"
#include "services/ComponentService.h"
#include "services/export/ExportWorkerHelpers.h"

#include <QDebug>
#include <QMutexLocker>
#include <QTimer>

namespace EasyKiConverter {

/**
 * @brief 按批次读取磁盘缓存并收敛预加载进度。
 * @param service 并行导出服务。
 */
void ParallelExportPreloadCoordinator::processNextBatch(ParallelExportService& service) {
    // 网络服务缺失时按批次读取磁盘缓存，避免一次性阻塞事件循环。
    if (service.m_cancelRequested || service.m_progress.currentStage != ExportOverallProgress::Stage::Preloading) {
        return;
    }

    constexpr int kBatchSize = 8;  ///< 无网络服务时每轮处理的缓存组件数量。
    int processedInBatch = 0;  ///< 当前批次已处理的组件数量。
    while (processedInBatch < kBatchSize && service.m_nextPreloadIndex < service.m_componentIds.size() &&
           !service.m_cancelRequested) {
        const QString componentId = service.m_componentIds.at(service.m_nextPreloadIndex++);  ///< 当前预加载组件编号。
        {
            QMutexLocker locker(&service.m_progressMutex);
            service.m_progress.preloadProgress.currentComponentId = componentId;
            service.m_progress.preloadProgress.inProgressCount = 1;
        }

        ComponentData data;  ///< 从服务或磁盘缓存取得的组件数据。
        if (service.m_componentService) {
            data = service.m_componentService->getComponentData(componentId);
            const QSharedPointer<ComponentData> diskCachedData =  // 磁盘缓存中的组件数据。
                ExportWorkerHelpers::loadDiskCachedComponentData(componentId);
            ExportWorkerHelpers::mergeComponentData(data, diskCachedData);
        } else {
            const QSharedPointer<ComponentData> diskCachedData =  // 磁盘缓存中的组件数据。
                ExportWorkerHelpers::loadDiskCachedComponentData(componentId);
            if (diskCachedData)
                data = *diskCachedData;
        }

        {
            QMutexLocker locker(&service.m_progressMutex);
            // 数据。
            const bool hasValidData = !data.lcscId().isEmpty() && data.isValid() && data.symbolData() != nullptr &&
                                      data.footprintData() != nullptr;
            if (hasValidData) {
                service.m_cachedData[componentId] = QSharedPointer<ComponentData>::create(data);
                service.m_progress.preloadProgress.successCount++;
                qDebug() << "ParallelExportService: Loaded data for" << componentId;
            } else {
                service.m_progress.preloadProgress.failedCount++;
                service.m_progress.preloadProgress.failedComponents[componentId] =
                    QStringLiteral("Incomplete component data");
                qWarning() << "ParallelExportService: No valid data found for component:" << componentId;
            }
            service.m_progress.preloadProgress.completedCount++;
            service.m_progress.preloadProgress.inProgressCount = 0;
        }
        ++processedInBatch;
    }

    const bool finished = service.m_nextPreloadIndex >= service.m_componentIds.size();  ///< 是否已处理完全部组件。
    if (finished) {
        QMutexLocker locker(&service.m_progressMutex);
        service.m_progress.preloadProgress.currentComponentId.clear();
        service.m_progress.currentStage = ExportOverallProgress::Stage::Idle;
        service.m_progress.endTime = QDateTime::currentDateTime();
        service.m_preloadCompleted = true;
    }

    qDebug() << "ParallelExportService: Preload batch completed. Success:"
             << service.m_progress.preloadProgress.successCount
             << "Failed:" << service.m_progress.preloadProgress.failedCount;
    emit service.preloadProgressChanged(service.m_progress.preloadProgress);
    service.updateOverallProgress();

    if (finished) {
        emit service.preloadCompleted(service.m_progress.preloadProgress.successCount,
                                      service.m_progress.preloadProgress.failedCount);
        return;
    }
    QTimer::singleShot(0, &service, [&service]() { processNextBatch(service); });
}

/**
 * @brief 收敛网络批量获取结果并更新预加载状态。
 * @param service 并行导出服务。
 * @param componentDataList 成功获取的组件数据列表。
 * @param failedComponents 获取失败的组件及错误信息。
 */
void ParallelExportPreloadCoordinator::handleCollectedData(ParallelExportService& service,
                                                           const QList<ComponentData>& componentDataList,
                                                           const QMap<QString, QString>& failedComponents) {
    // 回调只处理当前预加载，取消后的迟到结果必须被忽略。
    if (service.m_componentService) {
        QObject::disconnect(
            service.m_componentService, &ComponentService::allComponentsDataCollectedWithErrors, &service, nullptr);
    }
    {
        QMutexLocker locker(&service.m_progressMutex);
        if (service.m_cancelRequested) {
            qDebug() << "ParallelExportService: Ignoring late callback after cancel requested";
            return;
        }
    }

    int successCount = 0;  ///< 成功预加载的组件数量。
    int failedCount = failedComponents.size();  ///< 初始获取失败的组件数量。
    for (auto it = failedComponents.cbegin(); it != failedComponents.cend(); ++it)  // 当前失败组件记录。
        service.m_progress.preloadProgress.failedComponents[it.key()] = it.value();

    for (const ComponentData& data : componentDataList) {  // 当前成功获取的组件数据。
        const QString componentId = data.lcscId();  ///< 当前组件编号。
        if (componentId.isEmpty()) {
            ++failedCount;
            continue;
        }

        // 数据。
        const bool hasValidData = data.isValid() && data.symbolData() != nullptr &&
                                  data.footprintData() != nullptr;  ///< 组件数据是否满足导出前置条件。
        if (hasValidData) {
            service.m_cachedData[componentId] = QSharedPointer<ComponentData>::create(data);
            ++successCount;
            qDebug() << "ParallelExportService: Cached data for" << componentId;
        } else {
            ++failedCount;
            service.m_progress.preloadProgress.failedComponents[componentId] = QStringLiteral("No valid data found");
            qWarning() << "ParallelExportService: No valid data for component:" << componentId;
        }
    }

    {
        QMutexLocker locker(&service.m_progressMutex);
        service.m_progress.preloadProgress.successCount = successCount;
        service.m_progress.preloadProgress.failedCount = failedCount;
        service.m_progress.preloadProgress.completedCount = successCount + failedCount;
        service.m_progress.preloadProgress.inProgressCount = 0;
        service.m_progress.preloadProgress.currentComponentId.clear();
        service.m_progress.currentStage = ExportOverallProgress::Stage::Idle;
        service.m_progress.endTime = QDateTime::currentDateTime();
    }

    qDebug() << "ParallelExportService: Parallel preload completed. Success:" << successCount
             << "Failed:" << failedCount;
    service.logNetworkRuntimeStats(QStringLiteral("preload-completed"));
    service.writeExportDetailedReport(QStringLiteral("preload-completed"));
    service.m_preloadCompleted = true;
    emit service.preloadProgressChanged(service.m_progress.preloadProgress);
    emit service.preloadCompleted(successCount, failedCount);
}

}  // namespace EasyKiConverter
