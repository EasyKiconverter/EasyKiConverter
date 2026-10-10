#include "ParallelExportService.h"

#include "ComponentService.h"
#include "DatasheetExportStage.h"
#include "ExportProgressAggregator.h"
#include "ExportRunPlan.h"
#include "ExportStageLaunchCoordinator.h"
#include "FootprintExportStage.h"
#include "Model3DExportStage.h"
#include "ParallelExportPreloadCoordinator.h"
#include "PreviewImagesExportStage.h"
#include "SymbolExportStage.h"
#include "core/network/NetworkClient.h"
#include "services/ComponentCacheService.h"
#include "services/export/ExportReportGenerator.h"

#include <QDebug>
#include <QMutexLocker>
#include <QPointer>
#include <QSet>
#include <QTimer>

namespace EasyKiConverter {

ParallelExportService::ParallelExportService(QObject* parent) : QObject(parent), m_progress() {
    m_progress.currentStage = ExportOverallProgress::Stage::Idle;
}

ParallelExportService::~ParallelExportService() {
    m_cancelRequested = true;
    ++m_activeRunGeneration;
    NetworkClient::instance().cancelAllRequests();

    QList<ExportTypeStage*> stagesToStop;  ///< 析构时需要停止的唯一阶段列表。
    const auto appendUniqueStage = [&stagesToStop](ExportTypeStage* stage) {
        if (stage && !stagesToStop.contains(stage)) {
            stagesToStop.append(stage);
        }
    };

    for (auto* stage : std::as_const(m_exportStages)) {
        appendUniqueStage(stage);
    }
    for (const QPointer<ExportTypeStage>& stage : std::as_const(m_retiredExportStages)) {
        appendUniqueStage(stage.data());
    }

    m_exportStages.clear();
    m_retiredExportStages.clear();

    for (ExportTypeStage* stage : std::as_const(stagesToStop)) {
        stage->cancel();
    }
    for (ExportTypeStage* stage : std::as_const(stagesToStop)) {
        stage->waitForFinished(30000);
        delete stage;
    }
}

/**
 * @brief 清理已经完成或需要退出的导出阶段。
 */
void ParallelExportService::cleanupExportStages() {
    for (auto* stage : m_exportStages) {
        if (stage) {
            if (stage->isRunning() || stage->hasActiveWorkers()) {
                retireExportStage(stage);
            } else {
                stage->deleteLater();
            }
        }
    }
    m_exportStages.clear();
    cleanupRetiredExportStages();
}

/**
 * @brief 从阶段表中移除指定阶段并安排安全销毁。
 * @param typeName 类型名称。
 * @param stage 导出阶段。
 */
void ParallelExportService::discardExportStage(const QString& typeName, ExportTypeStage* stage) {
    if (m_exportStages.value(typeName) == stage) {
        m_exportStages.remove(typeName);
    }
    if (stage) {
        if (stage->isRunning() || stage->hasActiveWorkers()) {
            retireExportStage(stage);
        } else {
            for (int i = m_retiredExportStages.size() - 1; i >= 0; --i) {
                if (m_retiredExportStages.at(i).data() == stage || m_retiredExportStages.at(i).isNull()) {
                    m_retiredExportStages.removeAt(i);
                }
            }
            stage->deleteLater();
        }
    }
    cleanupRetiredExportStages();
}

/**
 * @brief 将仍有活动任务的阶段转入延迟回收列表。
 * @param stage 导出阶段。
 */
void ParallelExportService::retireExportStage(ExportTypeStage* stage) {
    if (!stage) {
        return;
    }

    stage->setParent(this);
    for (const QPointer<ExportTypeStage>& retiredStage : std::as_const(m_retiredExportStages)) {
        if (retiredStage.data() == stage) {
            return;
        }
    }
    m_retiredExportStages.append(QPointer<ExportTypeStage>(stage));

    QPointer<ExportTypeStage> stagePtr(stage);
    connect(stage, &ExportTypeStage::completed, this, [this, stagePtr](int, int, int) {
        if (!stagePtr) {
            cleanupRetiredExportStages();
            return;
        }
        discardExportStage(stagePtr->typeName(), stagePtr.data());
    });
}

/**
 * @brief 回收已经停止且没有活动任务的阶段。
 */
void ParallelExportService::cleanupRetiredExportStages() {
    for (int i = m_retiredExportStages.size() - 1; i >= 0; --i) {  // 从后向前清理阶段。
        ExportTypeStage* stage = m_retiredExportStages.at(i).data();  ///< 当前待回收的阶段对象。
        if (!stage) {
            m_retiredExportStages.removeAt(i);
            continue;
        }
        if (stage->isRunning() || stage->hasActiveWorkers()) {
            continue;
        }
        m_retiredExportStages.removeAt(i);
        stage->deleteLater();
    }
}

/**
 * @brief 设置后续预加载和导出流程使用的选项。
 * @param options 处理选项。
 */
void ParallelExportService::setOptions(const ExportOptions& options) {
    m_options = options;
}

/**
 * @brief 更新导出输出目录。
 * @param path 目标路径。
 */
void ParallelExportService::setOutputPath(const QString& path) {
    m_options.outputPath = path;
}

/**
 * @brief 注入负责网络和组件数据获取的服务。
 * @param componentService 元器件服务。
 */
void ParallelExportService::setComponentService(ComponentService* componentService) {
    m_componentService = componentService;
}

/**
 * @brief 启动组件预加载，并在没有网络服务时使用磁盘缓存回退。
 * @param componentIds 元器件编号列表。
 */
void ParallelExportService::startPreload(const QStringList& componentIds) {
    if (m_progress.currentStage == ExportOverallProgress::Stage::Preloading) {
        qWarning() << "ParallelExportService: Preload already in progress";
        return;
    }

    if (m_componentService) {
        disconnect(m_componentService, &ComponentService::allComponentsDataCollected, this, nullptr);
        disconnect(m_componentService, &ComponentService::allComponentsDataCollectedWithErrors, this, nullptr);
    }

    QStringList normalizedComponentIds;  ///< 规范化并去重后的元器件编号列表。
    QSet<QString> seenComponentIds;  ///< 已见过的规范化元器件编号集合。
    for (const QString& componentId : componentIds) {  // 当前输入元器件编号。
        const QString normalizedId = componentId.trimmed().toUpper();  ///< 规范化后的元器件编号。
        if (!normalizedId.isEmpty() && !seenComponentIds.contains(normalizedId)) {
            seenComponentIds.insert(normalizedId);
            normalizedComponentIds.append(normalizedId);
        }
    }

    cleanupExportStages();
    m_runningExportStages = 0;
    ++m_activeRunGeneration;
    m_cancelRequested = false;
    /**
     * @brief 新一轮验证/导出开始，解除全局 tombstone
     * @brief 旧回调靠 generation token 被拒绝，新请求携带新 token 可正常写入
     */
    ComponentCacheService::instance()->clearGlobalTombstone();
    qDebug() << "ParallelExportService: Starting preload for" << normalizedComponentIds.size() << "components";

    m_componentIds = normalizedComponentIds;
    m_progress.currentStage = ExportOverallProgress::Stage::Preloading;
    m_progress.totalComponents = m_componentIds.size();
    m_progress.startTime = QDateTime::currentDateTime();
    m_progress.preloadProgress = PreloadProgress();
    m_progress.preloadProgress.totalCount = m_componentIds.size();
    m_cachedData.clear();
    m_preloadCompleted = false;
    m_nextPreloadIndex = 0;

    {
        QMutexLocker locker(&m_progressMutex);
        m_progress.preloadProgress.currentComponentId.clear();
        m_progress.preloadProgress.inProgressCount = 0;
    }

    if (m_componentIds.isEmpty()) {
        m_progress.currentStage = ExportOverallProgress::Stage::Idle;
        m_progress.endTime = QDateTime::currentDateTime();
        m_preloadCompleted = true;
        emit preloadProgressChanged(m_progress.preloadProgress);
        updateOverallProgress();
        emit preloadCompleted(0, 0);
        return;
    }

    emit preloadProgressChanged(m_progress.preloadProgress);
    updateOverallProgress();
    logNetworkRuntimeStats(QStringLiteral("preload-start"));

    // 使用 ComponentService 的并行获取能力来加载数据
    // 这样可以利用配置的并发数（默认10个并行请求）
    if (m_componentService) {
        // 连接到 ComponentService 的信号以接收数据
        connect(m_componentService,
                &ComponentService::allComponentsDataCollectedWithErrors,
                this,
                &ParallelExportService::onAllComponentDataCollectedWithErrors,
                Qt::UniqueConnection);

        // 启动并行获取
        m_componentService->fetchMultipleComponentsData(m_componentIds, m_options.exportModel3D);

        qDebug() << "ParallelExportService: Started parallel fetch for" << m_componentIds.size() << "components";
    } else {
        // 如果没有 ComponentService，直接从本地缓存读取
        qWarning() << "ParallelExportService: ComponentService not set, using sync cache fallback";
        processNextPreloadBatch();
    }
}

/**
 * @brief 取消当前预加载并收敛其进度状态。
 */
void ParallelExportService::cancelPreload() {
    qDebug() << "ParallelExportService: Cancelling preload";
    ++m_activeRunGeneration;
    m_cancelRequested = true;
    m_nextPreloadIndex = m_componentIds.size();
    if (m_componentService) {
        disconnect(m_componentService, &ComponentService::allComponentsDataCollected, this, nullptr);
        disconnect(m_componentService, &ComponentService::allComponentsDataCollectedWithErrors, this, nullptr);
        m_componentService->abortBatchFetch();
    }
    {
        QMutexLocker locker(&m_progressMutex);
        m_progress.preloadProgress.inProgressCount = 0;
        m_progress.preloadProgress.currentComponentId.clear();
        m_progress.currentStage = ExportOverallProgress::Stage::Cancelled;
        m_progress.endTime = QDateTime::currentDateTime();
    }
    updateOverallProgress();
    emit cancelled();
    logNetworkRuntimeStats(QStringLiteral("preload-cancelled"));
    writeExportDetailedReport(QStringLiteral("preload-cancelled"));
}

/**
 * @brief 注册导出阶段的信号并启动阶段。
 * @param stage 待启动的导出阶段。
 * @param typeName 导出类型名称。
 * @param componentIds 待导出的元器件编号列表。
 * @param runGeneration 当前运行代次。
 */
void ParallelExportService::registerStageAndStart(ExportTypeStage* stage,
                                                  const QString& typeName,
                                                  const QStringList& componentIds,
                                                  quint64 runGeneration) {
    QPointer<ExportTypeStage> stagePtr(stage);  ///< 防止异步回调访问已销毁的阶段对象。
    m_exportStages[typeName] = stage;

    connect(stage,
            &ExportTypeStage::itemStatusChanged,
            this,
            [this, typeName, runGeneration](const QString& componentId, const ExportItemStatus& status) {
                if (runGeneration != m_activeRunGeneration) {
                    return;
                }
                onExportItemStatusChanged(componentId, typeName, status);
            });
    connect(stage,
            &ExportTypeStage::progressChanged,
            this,
            [this, typeName, runGeneration](const ExportTypeProgress& progress) {
                if (runGeneration != m_activeRunGeneration) {
                    return;
                }
                onExportTypeProgressChanged(typeName, progress);
            });
    connect(stage,
            &ExportTypeStage::completed,
            this,
            [this, stagePtr, typeName, runGeneration](int success, int failed, int skipped) {
                if (runGeneration != m_activeRunGeneration) {
                    if (stagePtr) {
                        discardExportStage(typeName, stagePtr.data());
                    }
                    return;
                }
                onExportTypeCompleted(typeName, success, failed, skipped);
            });

    if (typeName == QStringLiteral("Footprint") && m_options.exportModel3D &&
        (m_options.targetFormat == TargetEdaFormat::Altium || m_options.targetFormat == TargetEdaFormat::Allegro ||
         m_options.targetFormat == TargetEdaFormat::LibrePcb || m_options.targetFormat == TargetEdaFormat::Horizon)) {
        if (auto* footprintStage = qobject_cast<FootprintExportStage*>(stage)) {
            connect(footprintStage,
                    &FootprintExportStage::embeddedModel3DStatusChanged,
                    this,
                    [this, runGeneration](const QString& componentId, const ExportItemStatus& status) {
                        if (runGeneration == m_activeRunGeneration) {
                            onExportItemStatusChanged(componentId, QStringLiteral("Model3D"), status);
                        }
                    });
        }
    }
    stage->start(componentIds, m_cachedData);
}

/**
 * @brief 根据预加载结果启动各导出阶段。
 */
void ParallelExportService::startExport() {
    ExportStageLaunchCoordinator::start(*this);
}

/**
 * @brief 取消预加载或导出，并回收仍在运行的阶段。
 */
void ParallelExportService::cancelExport() {
    // 空闲或已结束时没有可取消的任务，避免退出流程产生误导性的取消信号和报告。
    const ExportOverallProgress progressSnapshot = getProgress();  ///< 取消前的整体进度快照。
    const bool hasTrackedProgress = !progressSnapshot.exportTypeProgress.isEmpty();  ///< 是否已经创建过导出阶段。
    if (!isRunning() && !hasTrackedProgress) {
        return;
    }

    qDebug() << "ParallelExportService: Cancelling export";
    ++m_activeRunGeneration;
    m_cancelRequested = true;

    if (m_progress.currentStage == ExportOverallProgress::Stage::Preloading) {
        cancelPreload();
        return;
    }

    // 重置 ComponentService 批处理上下文
    if (m_componentService) {
        m_componentService->abortBatchFetch();
    }

    m_progress.currentStage = ExportOverallProgress::Stage::Cancelled;
    m_progress.endTime = QDateTime::currentDateTime();
    emit cancelled();

    QList<QPointer<ExportTypeStage>> stagesToCancel;  ///< 待取消的阶段列表。
    for (auto* stage : m_exportStages) {  // 当前活动阶段。
        if (stage) {
            disconnect(stage, nullptr, this, nullptr);
            retireExportStage(stage);
            stagesToCancel.append(QPointer<ExportTypeStage>(stage));
        }
    }
    m_exportStages.clear();
    m_runningExportStages = 0;

    NetworkClient::instance().cancelAllRequests();
    for (const QPointer<ExportTypeStage>& stage : std::as_const(stagesToCancel)) {
        if (stage) {
            stage->cancel();
        }
    }
    cleanupRetiredExportStages();

    QTimer::singleShot(0, this, [this]() {
        logNetworkRuntimeStats(QStringLiteral("export-cancelled"));
        writeExportDetailedReport(QStringLiteral("export-cancelled"));
    });
}

/**
 * @brief 获取线程安全的整体进度快照。
 */
ExportOverallProgress ParallelExportService::getProgress() const {
    QMutexLocker locker(&m_progressMutex);
    return m_progress;
}

/**
 * @brief 获取指定导出类型的线程安全进度快照。
 * @param typeName 类型名称。
 */
ExportTypeProgress ParallelExportService::getTypeProgress(const QString& typeName) const {
    QMutexLocker locker(&m_progressMutex);
    return m_progress.exportTypeProgress.value(typeName);
}

/**
 * @brief 判断预加载或导出流程是否仍在运行。
 */
bool ParallelExportService::isRunning() const {
    return m_progress.currentStage == ExportOverallProgress::Stage::Preloading ||
           m_progress.currentStage == ExportOverallProgress::Stage::Exporting;
}

/**
 * @brief 接收单个预加载项的完成结果并更新统计。
 * @param componentId 元器件编号。
 * @param success 是否成功。
 * @param error 错误信息。
 */
void ParallelExportService::onPreloadItemCompleted(const QString& componentId, bool success, const QString& error) {
    QMutexLocker locker(&m_progressMutex);

    m_progress.preloadProgress.inProgressCount--;

    if (success) {
        m_progress.preloadProgress.successCount++;
    } else {
        m_progress.preloadProgress.failedCount++;
        m_progress.preloadProgress.failedComponents[componentId] = error;
    }

    m_progress.preloadProgress.completedCount++;

    locker.unlock();
    emit preloadProgressChanged(m_progress.preloadProgress);

    if (m_progress.preloadProgress.completedCount >= m_progress.preloadProgress.totalCount) {
        m_preloadCompleted = true;
        m_progress.currentStage = ExportOverallProgress::Stage::Idle;
        emit preloadCompleted(m_progress.preloadProgress.successCount, m_progress.preloadProgress.failedCount);
    }
}

/**
 * @brief 接收单个导出阶段的进度快照。
 * @param typeName 类型名称。
 * @param progress 导出进度。
 */
void ParallelExportService::onExportTypeProgressChanged(const QString& typeName, const ExportTypeProgress& progress) {
    if (m_cancelRequested || m_progress.currentStage == ExportOverallProgress::Stage::Cancelled) {
        return;
    }

    ExportTypeProgress normalizedProgress = progress;  ///< 补齐类型名称后的进度快照。
    normalizedProgress.typeName = typeName;
    {
        QMutexLocker locker(&m_progressMutex);
        m_progress.exportTypeProgress[typeName] = normalizedProgress;
    }

    updateOverallProgress();
}

/**
 * @brief 接收单个导出阶段的完成统计并触发总流程检查。
 * @param typeName 类型名称。
 * @param successCount 成功数量。
 * @param failedCount 失败数量。
 * @param skippedCount 跳过数量。
 */
void ParallelExportService::onExportTypeCompleted(const QString& typeName,
                                                  int successCount,
                                                  int failedCount,
                                                  int skippedCount) {
    Q_UNUSED(successCount);
    Q_UNUSED(failedCount);
    Q_UNUSED(skippedCount);
    {
        QMutexLocker locker(&m_progressMutex);

        ExportTypeProgress typeProgress = m_progress.exportTypeProgress.value(typeName);  ///< 当前类型的可写进度副本。
        /**
         * @brief 完成信号的阶段计数只覆盖实际启动的任务，必须保留预加载失败项的逐项状态。
         * @brief 以 itemStatus 重新计算，确保统计与界面逐项结果保持一致。
         */
        ExportProgressAggregator::finalizeTypeProgress(typeProgress, typeName, m_componentIds.size());

        m_progress.exportTypeProgress[typeName] = typeProgress;

        if (m_runningExportStages > 0) {
            m_runningExportStages--;
        }
    }

    emit typeCompleted(typeName, successCount, failedCount, skippedCount);

    if (auto* finishedStage = m_exportStages.value(typeName)) {
        discardExportStage(typeName, finishedStage);
    }

    checkAllExportCompleted();
}

/**
 * @brief 合并单个元器件的导出状态并同步整体进度。
 * @param componentId 元器件编号。
 * @param typeName 类型名称。
 * @param status 状态值。
 */
void ParallelExportService::onExportItemStatusChanged(const QString& componentId,
                                                      const QString& typeName,
                                                      const ExportItemStatus& status) {
    if (m_cancelRequested || m_progress.currentStage == ExportOverallProgress::Stage::Cancelled) {
        return;
    }

    bool shouldMirrorAltiumModel3DStatus = false;  ///< 是否需要将封装结果镜像为 Model3D 状态。
    QMutexLocker locker(&m_progressMutex);

    if (!m_progress.exportTypeProgress.contains(typeName)) {
        m_progress.exportTypeProgress[typeName] = ExportTypeProgress();
        m_progress.exportTypeProgress[typeName].typeName = typeName;
        m_progress.exportTypeProgress[typeName].totalCount = m_componentIds.size();
    }

    ExportTypeProgress& typeProgress = m_progress.exportTypeProgress[typeName];  ///< 当前类型的进度记录。
    ExportProgressAggregator::mergeItemStatus(typeProgress, componentId, status);

    // FootprintExportStage 会先发出真实的嵌入结果，再发出封装结果。
    // 只有在 3D 尚未收到独立结果时才使用封装状态兜底，避免封装成功覆盖
    // STEP 缺失等真实的 Model3D 失败状态。
    if (typeName == QStringLiteral("Footprint") && m_options.exportModel3D &&
        (m_options.targetFormat == TargetEdaFormat::Altium || m_options.targetFormat == TargetEdaFormat::Allegro ||
         m_options.targetFormat == TargetEdaFormat::LibrePcb || m_options.targetFormat == TargetEdaFormat::Horizon)) {
        const auto modelProgress =
            m_progress.exportTypeProgress.constFind(QStringLiteral("Model3D"));  ///< 独立 Model3D 阶段进度迭代器。
        shouldMirrorAltiumModel3DStatus =
            modelProgress == m_progress.exportTypeProgress.cend() || !modelProgress->itemStatus.contains(componentId);
    }

    locker.unlock();
    emit itemStatusChanged(componentId, typeName, status);

    // Altium、Allegro 和 LibrePCB 的 STEP 都在封装阶段写入目标库，没有独立的 Model3D stage。
    // 将封装结果镜像到 Model3D，确保 UI 状态、成功率和总体完成判定一致。
    if (shouldMirrorAltiumModel3DStatus) {
        onExportItemStatusChanged(componentId, QStringLiteral("Model3D"), status);
    }

    // 更新整体进度
    updateOverallProgress();
}

/**
 * @brief 处理一批预加载任务，网络服务缺失时直接读取磁盘缓存。
 */
void ParallelExportService::processNextPreloadBatch() {
    ParallelExportPreloadCoordinator::processNextBatch(*this);
}

/**
 * @brief 接收网络批量获取结果并完成预加载统计。
 * @param componentDataList 元器件数据列表。
 */
void ParallelExportService::onAllComponentDataCollected(const QList<ComponentData>& componentDataList) {
    onAllComponentDataCollectedWithErrors(componentDataList, {});
}

/**
 * @brief 接收网络失败原因并完成预加载统计。
 * @param componentDataList 元器件数据列表。
 * @param failedComponents 失败元器件列表。
 */
void ParallelExportService::onAllComponentDataCollectedWithErrors(const QList<ComponentData>& componentDataList,
                                                                  const QMap<QString, QString>& failedComponents) {
    ParallelExportPreloadCoordinator::handleCollectedData(*this, componentDataList, failedComponents);
}

/**
 * @brief 发布当前导出总进度快照。
 */
void ParallelExportService::updateOverallProgress() {
    ExportOverallProgress progressSnapshot;  ///< 对外发布的整体进度快照。
    {
        QMutexLocker locker(&m_progressMutex);
        progressSnapshot = m_progress;
    }

    emit progressChanged(progressSnapshot);
}

/**
 * @brief 检查所有导出阶段和元器件是否已经完成。
 */
void ParallelExportService::checkAllExportCompleted() {
    if (m_runningExportStages > 0) {
        return;
    }

    if (m_cancelRequested || m_progress.currentStage == ExportOverallProgress::Stage::Cancelled) {
        qDebug() << "ParallelExportService: Ignoring completion because export was cancelled";
        cleanupExportStages();
        return;
    }

    qDebug() << "ParallelExportService: All export stages completed";

    m_progress.currentStage = ExportOverallProgress::Stage::Completed;
    m_progress.endTime = QDateTime::currentDateTime();

    const ExportCompletionTotals totals =
        ExportProgressAggregator::countCompletedComponents(m_componentIds, m_progress.exportTypeProgress);

    logNetworkRuntimeStats(QStringLiteral("export-completed"));
    writeExportDetailedReport(QStringLiteral("export-completed"));
    emit completed(totals.successCount, totals.failedCount);
    cleanupExportStages();
}

/**
 * @brief 记录指定阶段的网络运行诊断。
 * @param context Qt 日志上下文。
 */
void ParallelExportService::logNetworkRuntimeStats(const QString& context) const {
    ExportReportGenerator::logNetworkStats(context);
}

/**
 * @brief 生成包含状态、诊断和网络信息的详细导出报告。
 * @param reason 失败原因。
 */
void ParallelExportService::writeExportDetailedReport(const QString& reason) const {
    // 获取进度快照（线程安全）
    ExportOverallProgress progressSnapshot;
    {
        QMutexLocker locker(&m_progressMutex);
        progressSnapshot = m_progress;
    }
    ExportReportGenerator::writeDetailedReport(reason, m_options, progressSnapshot);
}

}  // namespace EasyKiConverter
