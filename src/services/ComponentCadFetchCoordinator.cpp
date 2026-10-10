#include "ComponentCadFetchCoordinator.h"

#include "ComponentCacheService.h"
#include "ComponentService.h"

#include <QDebug>
#include <QFutureWatcher>
#include <QJsonDocument>
#include <QMutexLocker>
#include <QtConcurrent>

namespace EasyKiConverter {

/**
 * @brief 启动 CAD 响应的后台解析并将结果交回服务线程。
 * @param service 持有请求状态和缓存服务的组件服务。
 * @param componentId 元器件编号。
 * @param data CAD API 返回的数据对象。
 */
void ComponentCadFetchCoordinator::handleDataFetched(ComponentService& service,
                                                     const QString& componentId,
                                                     const QJsonObject& data) {
    uint64_t generation = 0;  // 当前请求的缓存代次。
    {
        QMutexLocker locker(&service.m_fetchingComponentsMutex);
        const auto it = service.m_fetchingComponents.find(componentId.toUpper());  // 活动请求状态。
        if (it != service.m_fetchingComponents.end()) {
            generation = it->cacheGeneration;
        }
    }

    if (generation == 0) {
        // 找不到请求状态时，说明回调已经晚于请求生命周期，不能再写入缓存。
        qDebug() << "handleCadDataFetched: No FetchingComponent for" << componentId << ", discarding stale callback";
        return;
    }

    auto future =  // 后台 CAD 解析任务。
        QtConcurrent::run([componentId, data]() { return CadDataLoader::parseCadPayload(componentId, data); });
    auto* watcher = new QFutureWatcher<CadParseResult>(&service);  // 解析任务完成监听器。
    QObject::connect(watcher, &QFutureWatcher<CadParseResult>::finished, &service, [&service, watcher, generation]() {
        const CadParseResult parsed = watcher->result();  // CAD 解析结果。
        watcher->deleteLater();

        CadFetchTaskResult result;  // 转交服务层的 CAD 任务结果。
        result.componentId = parsed.componentId;
        result.parsed = parsed;
        result.errorMessage = parsed.errorMessage;
        result.success = parsed.success;
        ComponentCadFetchCoordinator::handleFetchResult(service, result, generation);
    });
    watcher->setFuture(future);
}

/**
 * @brief 校验请求代次并提交成功或失败的 CAD 结果。
 * @param service 持有请求状态和缓存服务的组件服务。
 * @param result 后台 CAD 解析结果。
 * @param expectedGeneration 请求创建时的缓存代次。
 */
void ComponentCadFetchCoordinator::handleFetchResult(ComponentService& service,
                                                     const CadFetchTaskResult& result,
                                                     uint64_t expectedGeneration) {
    {
        QMutexLocker locker(&service.m_fetchingComponentsMutex);
        const auto it = service.m_fetchingComponents.find(result.componentId);  // 待校验的活动请求。
        if (it == service.m_fetchingComponents.end() || it->cacheGeneration != expectedGeneration) {
            qDebug() << "ComponentService: Discarding stale CAD result for" << result.componentId;
            return;
        }
    }

    if (!result.success) {
        service.emitFetchErrorAndClearState(result.componentId, result.errorMessage, expectedGeneration);
        return;
    }

    {
        QMutexLocker locker(&service.m_fetchingComponentsMutex);
        auto it = service.m_fetchingComponents.find(result.componentId);  // 待更新的活动请求。
        if (it == service.m_fetchingComponents.end() || it->cacheGeneration != expectedGeneration) {
            qDebug() << "ComponentService: Discarding stale CAD result for" << result.componentId;
            return;
        }
        it->data = result.parsed.componentData;
        it->hasCadData = true;
        it->requestActive = false;
    }

    service.updateComponentCache(result.componentId, result.parsed.componentData);
    emit service.cadDataReady(result.componentId, result.parsed.componentData);
    // 使用异步保存，不阻塞 UI。
    ComponentCacheService::instance()->saveComponentMetadataAsync(
        result.componentId, result.parsed.componentData, expectedGeneration, /*replaceModel3DMetadata=*/true);
    ComponentCacheService::instance()->saveCadDataJson(
        result.componentId, QJsonDocument(result.parsed.resultData).toJson(QJsonDocument::Compact), expectedGeneration);

    if (service.m_parallelContext != nullptr) {
        service.handleParallelDataCollected(result.componentId, result.parsed.componentData);
    }
}

}  // namespace EasyKiConverter
