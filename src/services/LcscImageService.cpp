#include "LcscImageService.h"

#include "ConfigService.h"
#include "LcscProductParser.h"
#include "core/network/NetworkClient.h"
#include "core/utils/UrlUtils.h"

#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSharedPointer>
#include <QTimer>
#include <QUrlQuery>
#include <QtConcurrent>

namespace EasyKiConverter {

namespace {

/**
 * @brief 读取当前配置中的弱网络适配开关。
 * @return 启用弱网络策略时返回 true
 */
bool isWeakNetworkEnabled() {
    return ConfigService::instance()->getWeakNetworkSupport();
}

}  // namespace

/**
 * @brief 初始化图片缓存线程池和取消状态。
 * @param parent Qt 对象父级，可为空
 */
LcscImageService::LcscImageService(QObject* parent)
    // 初始化线程池和请求取消标志。
    : QObject(parent), m_cacheThreadPool(new QThreadPool(this)), m_isCancelled(0) {
    m_cacheThreadPool->setMaxThreadCount(MAX_CONCURRENT_REQUESTS);
}

/**
 * @brief 请求单个元器件的预览图，并优先尝试磁盘缓存。
 * @param componentId 元器件编号
 */
void LcscImageService::fetchPreviewImages(const QString& componentId) {
    if (componentId.isEmpty()) {
        return;
    }
    m_isCancelled = 0;
    // 将调用方编号规范化为大写，作为后续缓存和请求状态的键。
    const QString normalizedId = componentId.toUpper();

    if (m_requestedComponents.contains(normalizedId)) {
        qDebug() << "Component" << normalizedId << "already requested, skipping duplicate request";
        return;
    }
    // 为本次预览图请求分配单调递增令牌。
    const quint64 requestToken = beginRequest(normalizedId);

    if (tryLoadCachedPreviewImages(normalizedId)) {
        m_requestedComponents.insert(normalizedId);
        QTimer::singleShot(0, this, [this, normalizedId, requestToken]() {
            loadCachedPreviewImagesAsync(normalizedId, ComponentCacheService::instance(), requestToken);
        });
        return;
    }

    m_requestedComponents.insert(normalizedId);
    performApiSearch(normalizedId, requestToken);
}

/**
 * @brief 批量请求多个元器件的预览图，并复用已有缓存。
 * @param componentIds 元器件编号列表
 */
void LcscImageService::fetchBatchPreviewImages(const QStringList& componentIds) {
    m_isCancelled = 0;

    for (const QString& componentId : componentIds) {
        if (componentId.isEmpty()) {
            continue;
        }
        // 将当前调用方编号规范化为大写，统一批量请求状态键。
        const QString normalizedId = componentId.toUpper();

        if (m_requestedComponents.contains(normalizedId)) {
            continue;
        }
        // 为当前批量项分配独立的请求令牌。
        const quint64 requestToken = beginRequest(normalizedId);

        if (tryLoadCachedPreviewImages(normalizedId)) {
            m_requestedComponents.insert(normalizedId);
            QTimer::singleShot(0, this, [this, normalizedId, requestToken]() {
                loadCachedPreviewImagesAsync(normalizedId, ComponentCacheService::instance(), requestToken);
            });
            continue;
        }

        m_requestedComponents.insert(normalizedId);
        performApiSearch(normalizedId, requestToken);
    }
}

/**
 * @brief 清理图片请求状态，保留磁盘缓存供后续请求使用。
 */
void LcscImageService::clearCache() {
    qDebug() << "LcscImageService: Clearing all cache data";

    m_requestedComponents.clear();
    m_requestTokens.clear();
    m_datasheetTokens.clear();
    m_downloadCounts.clear();
    m_expectedCounts.clear();

    qDebug() << "LcscImageService: Cache cleared, ready for new requests";
}

/**
 * @brief 取消所有正在进行的预览图请求并清空请求状态。
 */
void LcscImageService::cancelAll() {
    qDebug() << "LcscImageService: Cancelling all pending preview image fetches";

    m_isCancelled = 1;
    m_requestedComponents.clear();
    m_requestTokens.clear();
    m_datasheetTokens.clear();
    m_downloadCounts.clear();
    m_expectedCounts.clear();

    for (const auto& request : m_activeAsyncRequests) {
        if (request && !request->isFinished()) {
            request->cancel();
        }
    }
    m_activeAsyncRequests.clear();

    qDebug() << "LcscImageService: All pending preview image fetches cancelled";
}

/**
 * @brief 取消指定元器件的请求并忽略其后续回调。
 * @param componentId 元器件编号
 */
void LcscImageService::cancelRequestForComponent(const QString& componentId) {
    qDebug() << "LcscImageService: Cancelling request for component" << componentId;

    // 从请求组件列表中移除
    // 将待取消的编号规范化为请求状态键。
    const QString normalizedId = componentId.toUpper();
    m_requestTokens.remove(normalizedId);
    m_datasheetTokens.remove(normalizedId);
    m_requestedComponents.remove(normalizedId);
    m_downloadCounts.remove(normalizedId);
    m_expectedCounts.remove(normalizedId);

    // 取消与该组件相关的异步请求
    // 注意：由于AsyncNetworkRequest没有暴露componentId信息，我们无法精确取消特定组件的请求
    // 但通过从m_requestedComponents中移除，响应到达时会被忽略

    qDebug() << "LcscImageService: Request cancelled for component" << componentId;
}

/**
 * @brief 请求下载指定元器件的数据手册。
 * @param componentId 元器件编号
 * @param datasheetUrl 数据手册 URL
 */
void LcscImageService::fetchDatasheet(const QString& componentId, const QString& datasheetUrl) {
    if (componentId.isEmpty() || datasheetUrl.isEmpty()) {
        qWarning() << "LcscImageService::fetchDatasheet called with empty componentId or datasheetUrl";
        return;
    }

    m_isCancelled = 0;

    // 将数据手册请求编号规范化为缓存状态键。
    const QString normalizedId = componentId.toUpper();
    // 为本次数据手册请求分配单调递增令牌。
    const quint64 requestToken = beginDatasheetRequest(normalizedId);
    qDebug() << "LcscImageService: fetchDatasheet called for" << normalizedId;
    performDatasheetDownload(normalizedId, datasheetUrl, requestToken);
}

/**
 * @brief 检查指定元器件是否存在预览图缓存，并安排异步读取。
 * @param componentId 元器件编号
 * @return 存在至少一张缓存图片时返回 true
 */
bool LcscImageService::tryLoadCachedPreviewImages(const QString& componentId) {
    // 访问统一缓存服务，查询已保存的预览图文件。
    ComponentCacheService* cache = ComponentCacheService::instance();
    if (!cache) {
        return false;
    }

    // 统计当前元器件已有的预览图缓存数量。
    int cachedCount = 0;
    for (int i = 0; i < MAX_IMAGES_PER_COMPONENT; ++i) {
        // 解析当前预览图序号对应的缓存路径。
        QString path = cache->previewImagePath(componentId, i);
        if (QFileInfo::exists(path)) {
            cachedCount++;
        }
    }

    if (cachedCount == 0) {
        qDebug() << "LcscImageService: No cached preview images for" << componentId;
        return false;
    }

    qDebug() << "LcscImageService: Found" << cachedCount << "cached preview images for" << componentId
             << ", scheduling async cache load...";

    return true;
}

/**
 * @brief 在后台线程读取预览图缓存，并在全部读取结束后统一处理。
 * @param componentId 元器件编号
 * @param cache 缓存服务
 * @param requestToken 当前请求令牌
 */
void LcscImageService::loadCachedPreviewImagesAsync(const QString& componentId,
                                                    ComponentCacheService* cache,
                                                    quint64 requestToken) {
    if (m_isCancelled || !isCurrentRequest(componentId, requestToken)) {
        return;
    }

    // 保存需要在后台线程读取的预览图序号和路径。
    QList<QPair<int, QString>> pathsToLoad;
    for (int i = 0; i < MAX_IMAGES_PER_COMPONENT; ++i) {
        // 解析当前预览图缓存路径。
        QString path = cache->previewImagePath(componentId, i);
        if (QFileInfo::exists(path)) {
            pathsToLoad.append({i, path});
        }
    }

    if (pathsToLoad.isEmpty()) {
        qDebug() << "LcscImageService: No cached images found for" << componentId;
        m_requestedComponents.remove(componentId);
        m_requestTokens.remove(componentId);
        checkDownloadCompletion(componentId, requestToken);
        return;
    }

    m_expectedCounts[componentId] = pathsToLoad.size();
    m_downloadCounts[componentId] = 0;

    // 按图片序号保存后台读取到的图片数据。
    auto componentImageData = QSharedPointer<QMap<int, QByteArray>>::create();
    // 统计已完成读取的图片数量，并由回调共享更新。
    auto loadedCount = QSharedPointer<int>::create(0);
    // 保存本次需要读取的图片总数，用于判断批量读取是否结束。
    const int totalImages = pathsToLoad.size();

    for (const auto& [imageIndex, path] : pathsToLoad) {
        // 在缓存专用线程池中异步读取当前图片文件。
        QFuture<QByteArray> future = QtConcurrent::run(m_cacheThreadPool, [path]() {
            QFile file(path);
            if (file.open(QIODevice::ReadOnly)) {
                return file.readAll();
            }
            return QByteArray();
        });

        // 监听当前图片读取任务，并在所属线程回调中汇总结果。
        auto* watcher = new QFutureWatcher<QByteArray>(this);
        connect(watcher,
                &QFutureWatcher<QByteArray>::finished,
                this,
                [this, watcher, componentId, imageIndex, componentImageData, loadedCount, totalImages, requestToken]() {
                    // 获取当前异步读取任务返回的图片数据。
                    QByteArray imageData = watcher->result();
                    if (!isCurrentRequest(componentId, requestToken)) {
                        m_pendingImageWatchers.removeOne(watcher);
                        watcher->deleteLater();
                        return;
                    }
                    if (!imageData.isEmpty()) {
                        (*componentImageData)[imageIndex] = imageData;
                    }

                    ++(*loadedCount);
                    if (*loadedCount >= totalImages) {
                        m_downloadCounts[componentId] = totalImages;
                        checkDownloadCompletion(componentId, requestToken);
                    }

                    m_pendingImageWatchers.removeOne(watcher);
                    watcher->deleteLater();
                });

        m_pendingImageWatchers.append(watcher);
        watcher->setFuture(future);
    }
}

/**
 * @brief 根据规范化后的地址启动预览图下载任务。
 * @param componentId 元器件编号
 * @param imageUrls 规范化后的预览图地址列表
 * @param requestToken 当前请求令牌
 */
void LcscImageService::startPreviewImageDownloads(const QString& componentId,
                                                  const QStringList& imageUrls,
                                                  quint64 requestToken) {
    if (!isCurrentRequest(componentId, requestToken)) {
        return;
    }
    // 去重并规范化图片地址，避免重复下载同一资源。
    const QStringList normalizedUrls = UrlUtils::deduplicateAndNormalizeUrls(imageUrls);
    if (normalizedUrls.isEmpty()) {
        m_requestedComponents.remove(componentId);
        m_requestTokens.remove(componentId);
        emit error(componentId, "No preview image URLs available");
        return;
    }

    m_expectedCounts[componentId] = normalizedUrls.size();
    m_downloadCounts[componentId] = 0;

    for (int i = 0; i < normalizedUrls.size(); ++i) {
        performDownload(componentId, normalizedUrls[i], i, requestToken);
    }
}

/**
 * @brief 向产品搜索接口发起请求并解析匹配产品信息。
 * @param componentId 元器件编号
 * @param requestToken 当前请求令牌
 */
void LcscImageService::performApiSearch(const QString& componentId, quint64 requestToken) {
    if (m_isCancelled) {
        return;
    }

    // 保存产品搜索请求的表单编码数据。
    QByteArray postData;
    // 构造产品搜索请求的查询参数。
    QUrlQuery query;
    query.addQueryItem("keyword", componentId);
    query.addQueryItem("currPage", "1");
    query.addQueryItem("pageSize", "50");
    query.addQueryItem("needAggs", "true");
    postData = query.toString(QUrl::FullyEncoded).toUtf8();

    // 使用产品搜索接口的默认网络策略，并覆盖本服务的超时和重试配置。
    RequestProfile profile = RequestProfiles::productSearch();
    profile.connectTimeoutMs = 30000;
    profile.readTimeoutMs = 30000;
    profile.maxRetries = MAX_RETRY_COUNT;

    // 启动异步产品搜索请求，完成后解析精确匹配的产品信息。
    AsyncNetworkRequest* request =
        NetworkClient::instance().postAsync(QUrl("https://pro.lceda.cn/api/v2/eda/product/search"),
                                            postData,
                                            ResourceType::ProductSearch,
                                            RetryPolicy::fromProfile(profile, isWeakNetworkEnabled()));
    trackAsyncRequest(request);

    QObject::connect(
        request,
        &AsyncNetworkRequest::finished,
        this,
        [this, request, componentId, requestToken](const NetworkResult& result) {
            untrackAsyncRequest(request);

            if (m_isCancelled) {
                request->deleteLater();
                return;
            }

            // 检查组件是否已被取消
            if (!isCurrentRequest(componentId, requestToken)) {
                request->deleteLater();
                return;
            }

            if (!result.success) {
                qWarning() << "LcscImageService: API search failed for component:" << componentId
                           << "error:" << result.error;
                request->deleteLater();
                m_requestedComponents.remove(componentId);
                m_requestTokens.remove(componentId);
                // 不再触发fallback，直接报告无预览图
                emit error(componentId, "No preview image available from API");
                return;
            }

            // 解析搜索响应中的精确匹配产品信息。
            const std::optional<LcscProductInfo> productInfo = LcscProductParser::parse(componentId, result.data);
            if (!productInfo.has_value()) {
                qWarning() << "LcscImageService: No matching product found for component:" << componentId;
                request->deleteLater();
                m_requestedComponents.remove(componentId);
                m_requestTokens.remove(componentId);
                // 不再触发 fallback，直接报告无预览图。
                emit error(componentId, "No preview image available from API");
                return;
            }

            // 提取产品提供的预览图地址并通知调用方。
            const QStringList imageUrls = productInfo->imageUrls;
            emit lcscDataReady(componentId, productInfo->manufacturerPart, productInfo->datasheetUrl, imageUrls);

            if (!imageUrls.isEmpty()) {
                startPreviewImageDownloads(componentId, imageUrls, requestToken);
            } else {
                m_requestedComponents.remove(componentId);
                m_requestTokens.remove(componentId);
                emit error(componentId, "No images available");
            }

            request->deleteLater();
        });
}

/**
 * @brief 下载单张预览图并写入经过校验的磁盘缓存。
 * @param componentId 元器件编号
 * @param imageUrl 预览图地址
 * @param imageIndex 预览图序号
 * @param requestToken 当前请求令牌
 */
void LcscImageService::performDownload(const QString& componentId,
                                       const QString& imageUrl,
                                       int imageIndex,
                                       quint64 requestToken) {
    if (m_isCancelled) {
        return;
    }

    // 捕获当前缓存代次，防止目录切换后旧响应写入新缓存。
    const uint64_t gen = ComponentCacheService::instance()->currentGeneration();
    // 启动当前预览图的异步下载请求。
    AsyncNetworkRequest* request = NetworkClient::instance().getAsync(
        QUrl(imageUrl),
        ResourceType::PreviewImage,
        RetryPolicy::fromProfile(RequestProfiles::previewImage(), isWeakNetworkEnabled()));
    trackAsyncRequest(request);
    connect(request,
            &AsyncNetworkRequest::finished,
            this,
            [this, request, componentId, imageIndex, gen, requestToken](const NetworkResult& result) {
                untrackAsyncRequest(request);

                if (m_isCancelled) {
                    request->deleteLater();
                    return;
                }

                // 缓存代次变化表示该请求已经过期，不能再通知当前组件状态。
                if (ComponentCacheService::instance()->currentGeneration() != gen) {
                    request->deleteLater();
                    return;
                }

                // 检查组件是否已被取消
                if (!isCurrentRequest(componentId, requestToken)) {
                    request->deleteLater();
                    return;
                }

                if (!result.success) {
                    m_downloadCounts[componentId]++;
                    checkDownloadCompletion(componentId, requestToken);
                    request->deleteLater();
                    return;
                }

                // 读取网络响应内容，后续检查是否为有效图片而非错误页。
                const QByteArray imageData = result.data;
                if (!imageData.isEmpty() && !QString::fromUtf8(imageData).contains("<!DOCTYPE", Qt::CaseInsensitive)) {
                    ComponentCacheService::instance()->savePreviewImage(componentId, imageData, imageIndex, gen);
                    m_downloadCounts[componentId]++;
                    emit imageReady(componentId, imageData, imageIndex);
                    checkDownloadCompletion(componentId, requestToken);
                } else {
                    qWarning() << "LcscImageService: Downloaded invalid data for" << componentId << "index"
                               << imageIndex << "- may be blocked (403), falling back to API";
                    m_downloadCounts[componentId]++;
                    checkDownloadCompletion(componentId, requestToken);
                    emit error(componentId, "Image blocked (403), please retry");
                }

                request->deleteLater();
            });
}

/**
 * @brief 检查当前元器件的预览图下载任务是否全部结束。
 * @param componentId 元器件编号
 * @param requestToken 当前请求令牌
 */
void LcscImageService::checkDownloadCompletion(const QString& componentId, quint64 requestToken) {
    if (!isCurrentRequest(componentId, requestToken)) {
        return;
    }
    if (!m_expectedCounts.contains(componentId)) {
        return;
    }

    // 获取当前组件预期下载的图片数量。
    int expectedCount = m_expectedCounts[componentId];
    // 获取当前组件已经完成下载的图片数量。
    int downloadedCount = m_downloadCounts.value(componentId, 0);

    if (downloadedCount >= expectedCount) {
        emitAllImagesReady(componentId, requestToken);
    }
}

/**
 * @brief 汇总并校验预览图缓存，然后发出完成或失败信号。
 * @param componentId 元器件编号
 * @param requestToken 当前请求令牌
 */
void LcscImageService::emitAllImagesReady(const QString& componentId, quint64 requestToken) {
    if (!isCurrentRequest(componentId, requestToken)) {
        return;
    }
    // 访问统一缓存服务，重新校验最终可用的图片文件。
    ComponentCacheService* cache = ComponentCacheService::instance();
    // 保存通过内容校验并将通知给调用方的图片路径。
    QStringList imagePaths;
    for (int i = 0; i < MAX_IMAGES_PER_COMPONENT; ++i) {
        // 解析当前图片序号对应的缓存路径。
        const QString path = cache->previewImagePath(componentId, i);
        // 最终通知前重新读取并校验缓存，避免错误页或损坏图片仅因文件存在而被上层使用。
        if (!cache->loadPreviewImage(componentId, i).isEmpty()) {
            imagePaths.append(path);
        }
    }

    if (imagePaths.isEmpty()) {
        emit error(componentId, "No images downloaded");
    } else {
        emit allImagesReady(componentId, imagePaths);
    }

    m_downloadCounts.remove(componentId);
    m_expectedCounts.remove(componentId);
    m_requestedComponents.remove(componentId);
    m_requestTokens.remove(componentId);
}

/**
 * @brief 下载数据手册并将有效内容写入缓存。
 * @param componentId 元器件编号
 * @param datasheetUrl 数据手册 URL
 * @param requestToken 当前请求令牌
 */
void LcscImageService::performDatasheetDownload(const QString& componentId,
                                                const QString& datasheetUrl,
                                                quint64 requestToken) {
    if (m_isCancelled) {
        return;
    }

    // 捕获当前缓存代次，防止目录切换后旧响应写入新缓存。
    const uint64_t gen = ComponentCacheService::instance()->currentGeneration();
    // 启动当前数据手册的异步下载请求。
    AsyncNetworkRequest* request = NetworkClient::instance().getAsync(
        QUrl(datasheetUrl),
        ResourceType::Datasheet,
        RetryPolicy::fromProfile(RequestProfiles::datasheet(), isWeakNetworkEnabled()));
    trackAsyncRequest(request);
    connect(request,
            &AsyncNetworkRequest::finished,
            this,
            [this, request, componentId, datasheetUrl, gen, requestToken](const NetworkResult& result) {
                untrackAsyncRequest(request);

                if (m_isCancelled) {
                    request->deleteLater();
                    return;
                }

                // 缓存代次变化表示该请求已经过期，不能再通知当前组件状态。
                if (ComponentCacheService::instance()->currentGeneration() != gen) {
                    request->deleteLater();
                    return;
                }

                if (!isCurrentDatasheetRequest(componentId, requestToken)) {
                    request->deleteLater();
                    return;
                }

                if (!result.success) {
                    emit error(componentId, QString("Datasheet download failed: %1").arg(result.error));
                    m_datasheetTokens.remove(componentId);
                    request->deleteLater();
                    return;
                }

                // 读取网络响应中的数据手册内容。
                const QByteArray datasheetData = result.data;
                if (!datasheetData.isEmpty()) {
                    // 根据地址后缀推断缓存格式，供后续内容校验使用。
                    QString format = datasheetUrl.toLower().contains(".html") ? "html" : "pdf";
                    ComponentCacheService::instance()->saveDatasheet(componentId, datasheetData, format, gen);
                    // 从磁盘重新读取实际保存的数据，确保通知内容与缓存一致。
                    QByteArray savedData = ComponentCacheService::instance()->loadDatasheet(componentId);
                    emit datasheetReady(componentId, savedData);
                    m_datasheetTokens.remove(componentId);
                } else {
                    emit error(componentId, "Failed to read datasheet data");
                    m_datasheetTokens.remove(componentId);
                }

                request->deleteLater();
            });
}

/**
 * @brief 跟踪尚未完成的异步网络请求。
 * @param request 异步网络请求对象
 */
void LcscImageService::trackAsyncRequest(AsyncNetworkRequest* request) {
    if (!request) {
        return;
    }
    m_activeAsyncRequests.append(QPointer<AsyncNetworkRequest>(request));
}

/**
 * @brief 移除已经完成或取消的异步网络请求。
 * @param request 异步网络请求对象
 */
void LcscImageService::untrackAsyncRequest(AsyncNetworkRequest* request) {
    m_activeAsyncRequests.removeOne(QPointer<AsyncNetworkRequest>(request));
}

/**
 * @brief 为新的组件媒体请求分配单调递增令牌。
 * @param componentId 元器件编号
 * @return 新分配的请求令牌
 */
quint64 LcscImageService::beginRequest(const QString& componentId) {
    // 为新请求分配下一个唯一令牌。
    const quint64 requestToken = ++m_nextRequestToken;
    m_requestTokens[componentId] = requestToken;
    return requestToken;
}

/**
 * @brief 判断异步回调是否仍属于当前组件请求。
 * @param componentId 元器件编号
 * @param requestToken 待验证的请求令牌
 * @return 回调仍属于当前请求时返回 true
 */
bool LcscImageService::isCurrentRequest(const QString& componentId, quint64 requestToken) const {
    return m_requestTokens.value(componentId, 0) == requestToken && m_requestedComponents.contains(componentId);
}

/**
 * @brief 为新的数据手册请求分配单调递增令牌。
 * @param componentId 元器件编号
 * @return 新分配的请求令牌
 */
quint64 LcscImageService::beginDatasheetRequest(const QString& componentId) {
    // 为新数据手册请求分配下一个唯一令牌。
    const quint64 requestToken = ++m_nextRequestToken;
    m_datasheetTokens[componentId] = requestToken;
    return requestToken;
}

/**
 * @brief 判断数据手册异步回调是否仍属于当前请求。
 * @param componentId 元器件编号
 * @param requestToken 待验证的请求令牌
 * @return 回调仍属于当前请求时返回 true
 */
bool LcscImageService::isCurrentDatasheetRequest(const QString& componentId, quint64 requestToken) const {
    return m_datasheetTokens.value(componentId, 0) == requestToken;
}

}  // namespace EasyKiConverter
