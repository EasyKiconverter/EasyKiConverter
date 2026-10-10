#include "ComponentCacheLoadCoordinator.h"

#include "CadDataLoader.h"
#include "ComponentCacheLoadWorker.h"
#include "ComponentCacheService.h"
#include "ComponentCadFetchCoordinator.h"
#include "ComponentParallelFetchCoordinator.h"
#include "ComponentService.h"

#include <QDebug>
#include <QFutureWatcher>
#include <QMutexLocker>
#include <QTimer>
#include <QtConcurrent>

namespace EasyKiConverter {

/**
 * @brief 启动后台缓存加载并将结果交回服务对象的主线程。
 * @param owner 元器件服务。
 * @param normalizedId 规范化后的元器件编号。
 * @param fetch3DModel 是否获取三维模型。
 * @param cache 缓存服务。
 */
void ComponentCacheLoadCoordinator::load(ComponentService& owner,
                                         const QString& normalizedId,
                                         bool fetch3DModel,
                                         ComponentCacheService* cache) {
    // 从请求状态读取创建时的缓存代次，防止取消后旧任务污染新请求。
    uint64_t generation = 0;  // 本次请求捕获的缓存代次。
    {
        QMutexLocker locker(&owner.m_fetchingComponentsMutex);
        const auto it = owner.m_fetchingComponents.find(normalizedId);  // 当前请求状态。
        if (it != owner.m_fetchingComponents.end()) {
            generation = it->cacheGeneration;
        }
    }
    if (generation == 0) {
        qWarning() << "ComponentCacheLoadCoordinator: No fetching state for" << normalizedId << ", aborting";
        return;
    }

    // 缓存加载属于 I/O 密集型工作，放到后台线程避免阻塞界面线程。
    const QFuture<ComponentCacheLoadResult> future =
        QtConcurrent::run([normalizedId, fetch3DModel, cache]() {  // 后台加载任务。
            return ComponentCacheLoadWorker::load(normalizedId, fetch3DModel, cache);
        });
    auto* watcher = new QFutureWatcher<ComponentCacheLoadResult>(&owner);  // 缓存加载完成观察器。
    QObject::connect(
        watcher,
        &QFutureWatcher<ComponentCacheLoadResult>::finished,
        &owner,
        [&owner, watcher, normalizedId, fetch3DModel, generation]() {
            ComponentCacheLoadResult result = watcher->result();  // 后台缓存加载结果。
            watcher->deleteLater();

            // 取消和重试会替换代次，旧缓存结果只能被丢弃。
            {
                QMutexLocker locker(&owner.m_fetchingComponentsMutex);
                const auto it = owner.m_fetchingComponents.find(normalizedId);  // 当前请求状态。
                if (it == owner.m_fetchingComponents.end() || it->cacheGeneration != generation) {
                    qDebug() << "ComponentService: Discarding stale cache result for" << normalizedId;
                    return;
                }
            }

            if (!result.success || !result.cachedData) {
                qWarning() << "ComponentService: Failed to load cache for" << normalizedId
                           << ", falling back to network fetch";
                uint64_t retryGeneration = 0;  // 网络重试使用的新缓存代次。
                {
                    QMutexLocker locker(&owner.m_fetchingComponentsMutex);
                    const auto it = owner.m_fetchingComponents.find(normalizedId);  // 当前请求状态。
                    if (it == owner.m_fetchingComponents.end() || it->cacheGeneration != generation) {
                        qDebug() << "ComponentService: Discarding stale cache retry for" << normalizedId;
                        return;
                    }
                    ComponentService::FetchingComponent& fetchingComponent = *it;  // 当前元器件请求状态。
                    owner.initializeFetchingComponent(fetchingComponent, normalizedId, fetch3DModel);
                    retryGeneration = fetchingComponent.cacheGeneration;
                }
                if (retryGeneration == 0) {
                    qWarning() << "ComponentCacheLoadCoordinator: No retry generation for" << normalizedId;
                    return;
                }

                const auto retryFuture =  // CAD 网络重试任务。
                    QtConcurrent::run([normalizedId]() { return CadDataLoader::fetchAndParseCadData(normalizedId); });
                auto* retryWatcher = new QFutureWatcher<CadFetchTaskResult>(&owner);  // 网络重试观察器。
                QObject::connect(retryWatcher,
                                 &QFutureWatcher<CadFetchTaskResult>::finished,
                                 &owner,
                                 [&owner, retryWatcher, retryGeneration]() {
                                     const CadFetchTaskResult retryResult = retryWatcher->result();  // 网络重试结果。
                                     retryWatcher->deleteLater();
                                     owner.handleCadFetchResult(retryResult, retryGeneration);
                                 });
                retryWatcher->setFuture(retryFuture);
                return;
            }

            // 将后台解析得到的符号、封装和三维模型关联回缓存数据。
            if (result.symbolData) {
                result.cachedData->setSymbolData(result.symbolData);
            }
            if (result.footprintData) {
                result.cachedData->setFootprintData(result.footprintData);
                ComponentCacheLoadWorker::restoreModel3DFromFootprint(*result.cachedData, result.footprintData);
            }

            {
                QMutexLocker locker(&owner.m_fetchingComponentsMutex);
                const auto it = owner.m_fetchingComponents.find(normalizedId);  // 当前请求状态。
                if (it == owner.m_fetchingComponents.end() || it->cacheGeneration != generation) {
                    qDebug() << "ComponentService: Discarding stale cached component for" << normalizedId;
                    return;
                }
                ComponentService::FetchingComponent& fetchingComponent = *it;  // 当前元器件请求状态。
                fetchingComponent.componentId = normalizedId;
                fetchingComponent.data = *result.cachedData;
                fetchingComponent.fetch3DModel = fetch3DModel;
                fetchingComponent.hasCadData =
                    result.cachedData->symbolData() != nullptr && result.cachedData->footprintData() != nullptr;
                fetchingComponent.requestActive = false;
            }

            qDebug() << "ComponentService: Emitting cadDataReady for" << normalizedId
                     << "with symbolData:" << (result.cachedData->symbolData() != nullptr)
                     << "footprintData:" << (result.cachedData->footprintData() != nullptr);
            owner.updateComponentCache(normalizedId, *result.cachedData);
            emit owner.cadDataReady(normalizedId, *result.cachedData);

            // 并行获取场景需要同步释放队列中的当前请求槽位。
            if (owner.m_parallelContext != nullptr) {
                owner.handleParallelDataCollected(normalizedId, *result.cachedData);
            }

            if (!result.encodedPreviewImages.isEmpty()) {
                const QStringList encodedImages = result.encodedPreviewImages;  // 待派发的 Base64 预览图。
                QTimer::singleShot(0, &owner, [&owner, normalizedId, encodedImages]() {
                    emit owner.previewImagesReady(normalizedId, encodedImages);
                });
            }
            if (!result.datasheetData.isEmpty()) {
                emit owner.datasheetReady(normalizedId, result.datasheetData);
            }
            qDebug() << "ComponentService: Cache loaded successfully for" << normalizedId;
        });
    watcher->setFuture(future);
}

}  // namespace EasyKiConverter
