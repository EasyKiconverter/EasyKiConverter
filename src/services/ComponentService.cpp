#include "ComponentService.h"

#include "BomParser.h"
#include "CadDataLoader.h"
#include "ComponentApiCallbackCoordinator.h"
#include "ComponentCacheLoadCoordinator.h"
#include "ComponentCadFetchCoordinator.h"
#include "ComponentMediaCallbackCoordinator.h"
#include "ComponentParallelFetchCoordinator.h"
#include "ComponentQueueManager.h"
#include "ComponentRequestCancellationCoordinator.h"
#include "ComponentRequestCoordinator.h"
#include "ConfigService.h"
#include "PreviewImageDataEncoder.h"
#include "core/easyeda/EasyedaApi.h"
#include "core/network/NetworkClient.h"
#include "core/utils/UrlUtils.h"

#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QQueue>
#include <QSet>
#include <QTextStream>
#include <QThread>
#include <QTimer>
#include <QUuid>
#include <QtConcurrent>

#include <cstdlib>

namespace EasyKiConverter {

ComponentService::ComponentService(QObject* parent)
    : QObject(parent)
    , m_api(nullptr)
    , m_currentComponentId()
    , m_parallelContext(nullptr)
    , m_imageService(nullptr)
    , m_activeRequestCount(0)
    , m_maxConcurrentRequests(ConfigService::instance()->getValidationConcurrentCount())
    , m_queueManager(nullptr)
    , m_batchFetch3DModel(true) {
    try {
        m_api = new EasyedaApi();
        if (m_api) {
            m_api->setParent(this);
            m_api->setWeakNetworkSupport(ConfigService::instance()->getWeakNetworkSupport());
        }
        m_imageService = new LcscImageService(this);
        initializeQueueManager();
    } catch (const std::bad_alloc& e) {
        qCritical() << "ComponentService: Memory allocation failed:" << e.what();
        std::terminate();
    } catch (...) {
        qCritical() << "ComponentService: Failed to initialize sub-services!";
        std::terminate();
    }

    initializeApiConnections();
    qDebug() << "ComponentService: Initialized successfully.";
}

ComponentService::ComponentService(EasyedaApi* api, QObject* parent)
    : QObject(parent)
    , m_api(api)
    , m_currentComponentId()
    , m_parallelContext(nullptr)
    , m_imageService(nullptr)
    , m_activeRequestCount(0)
    , m_maxConcurrentRequests(ConfigService::instance()->getValidationConcurrentCount())
    , m_queueManager(nullptr)
    , m_batchFetch3DModel(true) {
    if (m_api && !m_api->parent()) {
        m_api->setParent(this);
    }

    m_imageService = new LcscImageService(this);
    initializeQueueManager();

    initializeApiConnections();
    qDebug() << "ComponentService (Injected API): Initialized successfully.";
}

/**
 * @brief 创建队列管理器并连接请求、空队列和超时信号。
 */
void ComponentService::initializeQueueManager() {
    m_queueManager = new ComponentQueueManager(m_maxConcurrentRequests, this);
    connect(m_queueManager, &ComponentQueueManager::requestReady, this, [this](const QString& componentId) {
        fetchComponentDataInternal(componentId, m_batchFetch3DModel);
    });
    connect(m_queueManager, &ComponentQueueManager::queueEmpty, this, [this]() {
        qDebug() << "ComponentService: Queue empty signal received";
    });
    connect(m_queueManager, &ComponentQueueManager::timeout, this, [this]() { handleQueueTimeout(); });
}

/**
 * @brief 连接网络服务与组件服务的异步信号。
 */
void ComponentService::initializeApiConnections() {
    // 连接图片服务信号
    if (m_imageService) {
        connect(m_imageService, &LcscImageService::imageReady, this, &ComponentService::handleImageReady);
        connect(m_imageService, &LcscImageService::lcscDataReady, this, &ComponentService::handleLcscDataReady);
        connect(m_imageService, &LcscImageService::datasheetReady, this, &ComponentService::handleDatasheetReady);
        connect(m_imageService, &LcscImageService::allImagesReady, this, &ComponentService::handleAllImagesReady);
        connect(m_imageService, &LcscImageService::error, this, &ComponentService::handlePreviewImageError);
    }

    // 连接 API 信号
    if (m_api) {
        connect(m_api, &EasyedaApi::componentInfoFetched, this, &ComponentService::handleComponentInfoFetched);
        connect(m_api, &EasyedaApi::cadDataFetched, this, &ComponentService::handleCadDataFetched);
        connect(m_api, qOverload<const QString&>(&EasyedaApi::fetchError), this, &ComponentService::handleFetchError);
        connect(m_api,
                qOverload<const QString&, const QString&>(&EasyedaApi::fetchError),
                this,
                &ComponentService::handleFetchErrorWithId);
    }
}

/**
 * @brief 销毁元器件服务并释放其子服务。
 */
ComponentService::~ComponentService() {}

/**
 * @brief 请求单个元器件的完整数据。
 * @param componentId 元器件编号。
 * @param fetch3DModel 是否同时获取 3D 模型。
 */
void ComponentService::fetchComponentData(const QString& componentId, bool fetch3DModel) {
    fetchComponentDataInternal(componentId, fetch3DModel);
}

/**
 * @brief 初始化单个元器件请求的状态。
 * @param fetchingComponent 待初始化的请求状态。
 * @param componentId 元器件编号。
 * @param fetch3DModel 是否同时获取 3D 模型。
 */
void ComponentService::initializeFetchingComponent(FetchingComponent& fetchingComponent,
                                                   const QString& componentId,
                                                   bool fetch3DModel) {
    fetchingComponent.componentId = componentId;
    fetchingComponent.fetch3DModel = fetch3DModel;
    fetchingComponent.hasComponentInfo = false;
    fetchingComponent.hasCadData = false;
    fetchingComponent.hasTriggeredLcscFetch = false;
    fetchingComponent.requestActive = true;
    fetchingComponent.errorMessage.clear();
    fetchingComponent.pendingAsyncDownloads = 0;
    fetchingComponent.cacheGeneration = ComponentCacheService::instance()->currentGeneration();
}

/**
 * @brief 执行单个元器件数据请求的内部流程。
 * @param componentId 元器件编号。
 * @param fetch3DModel 是否同时获取 3D 模型。
 */
void ComponentService::fetchComponentDataInternal(const QString& componentId, bool fetch3DModel) {
    qDebug() << "Fetching component data (internal) for:" << componentId << "Fetch 3D:" << fetch3DModel
             << "at:" << QDateTime::currentDateTime().toString("hh:mm:ss.zzz");

    // 使用互斥锁保护 m_currentComponentId 的并发访问
    {
        QMutexLocker locker(&m_currentIdMutex);
        m_currentComponentId = componentId.toUpper();
    }
    ComponentRequestCoordinator::start(*this, componentId, fetch3DModel);
}

/**
 * @brief 异步从磁盘缓存加载元器件数据。
 * @param normalizedId 规范化后的元器件编号。
 * @param fetch3DModel 是否同时加载 3D 模型。
 * @param cache 用于读取数据的缓存服务。
 */
void ComponentService::loadComponentDataFromCacheAsync(const QString& normalizedId,
                                                       bool fetch3DModel,
                                                       ComponentCacheService* cache) {
    ComponentCacheLoadCoordinator::load(*this, normalizedId, fetch3DModel, cache);
}

/**
 * @brief 请求元器件预览图。
 * @param componentId 元器件编号。
 */
void ComponentService::fetchLcscPreviewImage(const QString& componentId) {
    qDebug() << "ComponentService: Fetching LCSC preview image for component:" << componentId;
    m_imageService->fetchPreviewImages(componentId);
}

/**
 * @brief 批量请求元器件预览图。
 * @param componentIds 元器件编号列表。
 */
void ComponentService::fetchBatchPreviewImages(const QStringList& componentIds) {
    qDebug() << "ComponentService: Fetching batch preview images for" << componentIds.count() << "components";
    m_imageService->fetchBatchPreviewImages(componentIds);
}

/**
 * @brief 处理单张预览图下载完成事件。
 * @param componentId 元器件编号。
 * @param imageData 预览图数据。
 * @param imageIndex 预览图索引。
 */
void ComponentService::handleImageReady(const QString& componentId, const QByteArray& imageData, int imageIndex) {
    ComponentMediaCallbackCoordinator::handleImageReady(*this, componentId, imageData, imageIndex);
}

/**
 * @brief 处理 LCSC 元器件媒体信息响应。
 * @param componentId 元器件编号。
 * @param manufacturerPart 制造商料号。
 * @param datasheetUrl 数据手册 URL。
 * @param imageUrls 预览图 URL 列表。
 */
void ComponentService::handleLcscDataReady(const QString& componentId,
                                           const QString& manufacturerPart,
                                           const QString& datasheetUrl,
                                           const QStringList& imageUrls) {
    ComponentMediaCallbackCoordinator::handleLcscDataReady(
        *this, componentId, manufacturerPart, datasheetUrl, imageUrls);
}

/**
 * @brief 处理数据手册下载完成事件。
 * @param componentId 元器件编号。
 * @param datasheetData 数据手册内容。
 */
void ComponentService::handleDatasheetReady(const QString& componentId, const QByteArray& datasheetData) {
    ComponentMediaCallbackCoordinator::handleDatasheetReady(*this, componentId, datasheetData);
}

/**
 * @brief 处理预览图下载失败事件。
 * @param componentId 元器件编号。
 * @param error 错误信息。
 */
void ComponentService::handlePreviewImageError(const QString& componentId, const QString& error) {
    ComponentMediaCallbackCoordinator::handlePreviewImageError(*this, componentId, error);
}

/**
 * @brief 处理全部预览图下载完成事件。
 * @param componentId 元器件编号。
 * @param imagePaths 已保存的预览图路径列表。
 */
void ComponentService::handleAllImagesReady(const QString& componentId, const QStringList& imagePaths) {
    ComponentMediaCallbackCoordinator::handleAllImagesReady(*this, componentId, imagePaths);
}

/**
 * @brief 处理元器件基础信息响应。
 * @param componentId 元器件编号。
 * @param data 基础信息 JSON 数据。
 */
void ComponentService::handleComponentInfoFetched(const QString& componentId, const QJsonObject& data) {
    ComponentApiCallbackCoordinator::handleComponentInfoFetched(*this, componentId, data);
}

/**
 * @brief 处理 CAD 数据响应并启动解析流程。
 * @param componentId 元器件编号。
 * @param data CAD 数据 JSON 对象。
 */
void ComponentService::handleCadDataFetched(const QString& componentId, const QJsonObject& data) {
    ComponentCadFetchCoordinator::handleDataFetched(*this, componentId, data);
}

/**
 * @brief 统一处理 CAD 获取结果，避免网络、重试和直接解析路径出现行为漂移。
 * @param result CAD 获取结果。
 * @param expectedGeneration 处理结果所对应的缓存代次。
 */
void ComponentService::handleCadFetchResult(const CadFetchTaskResult& result, uint64_t expectedGeneration) {
    ComponentCadFetchCoordinator::handleFetchResult(*this, result, expectedGeneration);
}

/**
 * @brief 处理未携带元器件编号的请求错误。
 * @param errorMessage 错误信息。
 */
void ComponentService::handleFetchError(const QString& errorMessage) {
    ComponentApiCallbackCoordinator::handleFetchError(*this, errorMessage);
}

/**
 * @brief 处理携带元器件编号或模型 UUID 的请求错误。
 * @param idOrUuid 元器件编号或模型 UUID。
 * @param error 错误信息。
 */
void ComponentService::handleFetchErrorWithId(const QString& idOrUuid, const QString& error) {
    ComponentApiCallbackCoordinator::handleFetchErrorWithId(*this, idOrUuid, error);
}

/**
 * @brief 设置导出输出目录。
 * @param path 输出目录路径。
 */
void ComponentService::setOutputPath(const QString& path) {
    m_outputPath = path;
}

/**
 * @brief 返回当前导出输出目录。
 * @return 当前导出输出目录。
 */
QString ComponentService::getOutputPath() const {
    return m_outputPath;
}

/**
 * @brief 启动多个元器件的并行数据请求。
 * @param componentIds 元器件编号列表。
 * @param fetch3DModel 是否同时获取 3D 模型。
 */
void ComponentService::fetchMultipleComponentsData(const QStringList& componentIds, bool fetch3DModel) {
    m_maxConcurrentRequests = ConfigService::instance()->getValidationConcurrentCount();
    if (m_queueManager != nullptr) {
        m_queueManager->setMaxConcurrentRequests(m_maxConcurrentRequests);
    }

    QStringList normalizedComponentIds;  // 去重并统一大小写后的元器件编号。
    QSet<QString> seenComponentIds;  // 已加入批量请求的元器件编号集合。
    for (const QString& componentId : componentIds) {
        const QString normalizedId = componentId.toUpper();  // 当前编号的规范化形式。
        if (!normalizedId.isEmpty() && !seenComponentIds.contains(normalizedId)) {
            seenComponentIds.insert(normalizedId);
            normalizedComponentIds.append(normalizedId);
        }
    }

    qDebug() << "Fetching data for" << normalizedComponentIds.size()
             << "components with async queue (max concurrent:" << m_maxConcurrentRequests << ")";

    // 防止重复启动批量处理，避免队列状态混乱
    if (m_parallelContext != nullptr) {
        qWarning() << "Batch processing already in progress, ignoring new request";
        return;
    }

    // 初始化并行获取状态
    m_parallelContext = new ParallelFetchContext(this);  // 当前批量请求的并行上下文。
    connect(m_parallelContext, &ParallelFetchContext::allCompleted, this, [this](const QList<ComponentData>& data) {
        const QMap<QString, QString> failedComponents =
            m_parallelContext->failedComponents();  // 批量请求失败的元器件及其错误信息。
        emit allComponentsDataCollected(data);
        emit allComponentsDataCollectedWithErrors(data, failedComponents);
        m_activeRequestCount = 0;
        resetQueueState();
    });
    m_parallelContext->start(normalizedComponentIds.size());
    m_activeRequestCount = 0;
    m_batchFetch3DModel = fetch3DModel;

    // 使用 ComponentQueueManager 管理队列
    m_queueManager->start(normalizedComponentIds);
}

/**
 * @brief 处理并行请求中的单个元器件完成事件。
 * @param componentId 元器件编号。
 * @param data 已获取的元器件数据。
 */
void ComponentService::handleParallelDataCollected(const QString& componentId, const ComponentData& data) {
    ComponentParallelFetchCoordinator::handleDataCollected(*this, componentId, data);
}

/**
 * @brief 处理并行请求中的单个元器件失败事件。
 * @param componentId 元器件编号。
 * @param error 错误信息。
 */
void ComponentService::handleParallelFetchError(const QString& componentId, const QString& error) {
    ComponentParallelFetchCoordinator::handleFetchError(*this, componentId, error);
}

/**
 * @brief 校验元器件编号格式。
 * @param componentId 待校验的元器件编号。
 * @return 编号格式是否有效。
 */
bool ComponentService::validateComponentId(const QString& componentId) const {
    return BomParser::validateId(componentId);
}

/**
 * @brief 从文本中提取元器件编号。
 * @param text 待解析的文本。
 * @return 提取到的元器件编号列表。
 */
QStringList ComponentService::extractComponentIdFromText(const QString& text) const {
    return BomParser::extractIdsFromText(text);
}

/**
 * @brief 解析 BOM 文件中的元器件编号。
 * @param filePath BOM 文件路径。
 * @return 解析到的元器件编号列表。
 */
QStringList ComponentService::parseBomFile(const QString& filePath) {
    BomParser parser;  // 用于读取 BOM 文件中元器件编号的解析器。
    return parser.parse(filePath);
}

/**
 * @brief 从内存缓存中读取元器件数据。
 * @param componentId 元器件编号。
 * @return 缓存的元器件数据。
 */
ComponentData ComponentService::getComponentData(const QString& componentId) const {
    const QString normalizedId = componentId.toUpper();  // 用于缓存索引的规范化编号。
    return m_componentCache.value(normalizedId);
}

/**
 * @brief 更新内存中的元器件缓存。
 * @param componentId 元器件编号。
 * @param data 待缓存的元器件数据。
 */
void ComponentService::updateComponentCache(const QString& componentId, const ComponentData& data) {
    const QString normalizedId = componentId.toUpper();  // 用于缓存索引的规范化编号。
    m_componentCache.set(normalizedId, data);
    qDebug() << "ComponentService: Updated cache for" << normalizedId;
}

/**
 * @brief 更新缓存元器件的描述字段。
 * @param componentId 元器件编号。
 * @param description 新的描述文本。
 */
void ComponentService::updateComponentDescription(const QString& componentId, const QString& description) {
    const QString normalizedId = componentId.toUpper();  // 用于缓存索引的规范化编号。
    m_componentCache.updateDescription(normalizedId, description);
}

/**
 * @brief 清理元器件及其预览图缓存。
 */
void ComponentService::clearCache() {
    m_componentCache.clear();

    // 清空 LCSC 图片服务的缓存
    if (m_imageService) {
        m_imageService->clearCache();
    }

    // 清空 ComponentCacheService 的内存缓存，防止内存累积
    ComponentCacheService::instance()->clearMemoryCache();

    qDebug() << "Component cache cleared (including LCSC image service and memory cache)";
}

/**
 * @brief 取消全部预览图请求。
 */
void ComponentService::cancelAllPreviewImageFetches() {
    qDebug() << "ComponentService: Cancelling all preview image fetches";
    if (m_imageService) {
        m_imageService->cancelAll();
    }
}

/**
 * @brief 取消全部未完成的元器件请求。
 */
void ComponentService::cancelAllPendingRequests() {
    ComponentRequestCancellationCoordinator::cancelAll(*this);
}

/**
 * @brief 取消指定元器件的请求。
 * @param componentId 元器件编号。
 */
void ComponentService::cancelRequestForComponent(const QString& componentId) {
    ComponentRequestCancellationCoordinator::cancelForComponent(*this, componentId);
}

/**
 * @brief 清理失败请求状态并发送错误信号。
 * @param componentId 元器件编号。
 * @param error 错误信息。
 * @param expectedGeneration 处理结果所对应的缓存代次。
 */
void ComponentService::emitFetchErrorAndClearState(const QString& componentId,
                                                   const QString& error,
                                                   uint64_t expectedGeneration) {
    ComponentApiCallbackCoordinator::emitFetchErrorAndClearState(*this, componentId, error, expectedGeneration);
}

/**
 * @brief 判断数据是否具有 PDF 文件签名。
 * @param data 待检查的文件数据。
 * @return 数据是否以 PDF 文件签名开头。
 */
bool ComponentService::isPDF(const QByteArray& data) const {
    // PDF 文件以 %PDF- 开头
    if (data.size() < 5) {
        return false;
    }
    return data.startsWith("%PDF-");
}

// 异步队列管理方法实现
// 使用 ComponentQueueManager 管理队列

/**
 * @brief 处理批量请求队列超时。
 */
void ComponentService::handleQueueTimeout() {
    ComponentParallelFetchCoordinator::handleQueueTimeout(*this);
}

/**
 * @brief 中止当前批量请求并清理状态。
 */
void ComponentService::abortBatchFetch() {
    // 取消所有网络请求（包括预览图、数据手册、CAD数据）
    cancelAllPendingRequests();
    // 重置队列和并行上下文
    resetQueueState();
    qDebug() << "ComponentService: Batch fetch aborted, all state cleared";
}

/**
 * @brief 重置批量请求队列及并行上下文。
 */
void ComponentService::resetQueueState() {
    ComponentParallelFetchCoordinator::reset(*this);
}

}  // namespace EasyKiConverter
