#include "ComponentParallelFetchCoordinator.h"

#include "ComponentQueueManager.h"
#include "ComponentService.h"

#include <QDebug>
#include <QMutexLocker>

namespace EasyKiConverter {

/**
 * @brief 将成功结果写入并行上下文并通知队列释放一个请求槽位。
 * @param service 持有并行上下文和请求队列的组件服务。
 * @param componentId 元器件编号。
 * @param data 已收集的组件数据。
 */
void ComponentParallelFetchCoordinator::handleDataCollected(ComponentService& service,
                                                            const QString& componentId,
                                                            const ComponentData& data) {
    Q_UNUSED(data);
    qDebug() << "Parallel data collected for:" << componentId;

    ComponentData completedData;  // 从活动请求状态复制的完整组件数据。
    {
        QMutexLocker locker(&service.m_fetchingComponentsMutex);
        if (service.m_fetchingComponents.contains(componentId)) {
            completedData = service.m_fetchingComponents[componentId].data;
        }
    }

    ParallelFetchContext* parallelContext = nullptr;  // 当前并行获取上下文。
    {
        QMutexLocker locker(&service.m_parallelContextMutex);
        parallelContext = service.m_parallelContext;
    }
    if (parallelContext != nullptr) {
        parallelContext->markCompleted(componentId, completedData);
    }

    if (service.m_queueManager != nullptr) {
        service.m_queueManager->requestCompleted(componentId);
    }
}

/**
 * @brief 将失败结果写入并行上下文并通知队列释放一个请求槽位。
 * @param service 持有并行上下文和请求队列的组件服务。
 * @param componentId 元器件编号。
 * @param error 获取失败原因。
 */
void ComponentParallelFetchCoordinator::handleFetchError(ComponentService& service,
                                                         const QString& componentId,
                                                         const QString& error) {
    qDebug() << "Parallel fetch error for:" << componentId << error;

    ParallelFetchContext* parallelContext = nullptr;  // 当前并行获取上下文。
    {
        QMutexLocker locker(&service.m_parallelContextMutex);
        parallelContext = service.m_parallelContext;
    }
    if (parallelContext != nullptr) {
        parallelContext->markFailed(componentId, error);
    }

    if (service.m_queueManager != nullptr) {
        service.m_queueManager->requestCompleted(componentId);
    }
}

/**
 * @brief 处理超时批量请求并发布已经收集的数据和错误。
 * @param service 持有并行上下文和请求队列的组件服务。
 */
void ComponentParallelFetchCoordinator::handleQueueTimeout(ComponentService& service) {
    qWarning() << "Queue timeout reached";

    if (service.m_parallelContext != nullptr) {
        const int completedCount = service.m_parallelContext->completedCount();  // 已完成的组件数量。
        const int totalCount = service.m_parallelContext->totalCount();  // 批量请求总数量。
        qWarning() << "Queue timeout - Completed:" << completedCount << "Total:" << totalCount;

        if (completedCount > 0) {
            const QList<ComponentData> allData = service.m_parallelContext->collectedData();  // 已收集的数据。
            // 保存失败组件及其原因，供错误信号一并发布。
            const QMap<QString, QString> failedComponents = service.m_parallelContext->failedComponents();
            emit service.allComponentsDataCollected(allData);
            emit service.allComponentsDataCollectedWithErrors(allData, failedComponents);
        }

        reset(service);
    }
}

/**
 * @brief 停止队列、清零活动请求并延迟销毁并行上下文。
 * @param service 持有并行上下文和请求队列的组件服务。
 */
void ComponentParallelFetchCoordinator::reset(ComponentService& service) {
    service.m_queueManager->stop();
    service.m_activeRequestCount = 0;

    {
        QMutexLocker locker(&service.m_parallelContextMutex);
        if (service.m_parallelContext != nullptr) {
            service.m_parallelContext->deleteLater();
            service.m_parallelContext = nullptr;
        }
    }

    qDebug() << "Queue state reset completed";
}

}  // namespace EasyKiConverter
