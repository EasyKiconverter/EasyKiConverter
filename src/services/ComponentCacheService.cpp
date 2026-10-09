#include "ComponentCacheService.h"

#include "CacheDataValidator.h"
#include "CacheDirectoryCoordinator.h"
#include "CacheFileLayout.h"
#include "CacheHealthManager.h"
#include "CacheMetadataStore.h"
#include "CachePathResolver.h"
#include "CachePruner.h"
#include "CacheSafety.h"
#include "ComponentCacheBinaryFileStore.h"
#include "ComponentCacheCadDataWriter.h"
#include "ComponentCacheFileReadCoordinator.h"
#include "ComponentCacheMaintenance.h"
#include "ComponentCacheMetadataWriter.h"
#include "ComponentCacheModel3DCoordinator.h"
#include "ComponentCachePreviewImageWriter.h"
#include "ComponentCacheQuotaEnforcer.h"
#include "ComponentCacheReadCoordinator.h"
#include "ComponentCacheWritePolicy.h"
#include "ConfigService.h"
#include "DatasheetCacheFileStore.h"
#include "DatasheetDownloadService.h"
#include "PreviewImageDownloadService.h"
#include "core/kicad/Exporter3DModel.h"
#include "core/network/NetworkClient.h"
#include "core/utils/UrlUtils.h"
#include "services/BomParser.h"
#include "services/ComponentService.h"
#include "utils/logging/LogMacros.h"

#include <QAtomicInt>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QUrl>
#include <QtConcurrent>

namespace EasyKiConverter {

std::unique_ptr<ComponentCacheService> ComponentCacheService::s_instance;

/**
 * @brief 初始化默认容量、缓存目录和缓存生命周期计时器。
 */
ComponentCacheService::ComponentCacheService(QObject* parent) /* 初始化默认容量和缓存目录。 */
    // 初始化 Qt 对象和 L1 缓存容量。
    : QObject(parent), m_diskCacheLimitMB(ConfigService::DEFAULT_DISK_CACHE_LIMIT_MB), m_memoryCache(50 * 1024 * 1024) {
    // 启动磁盘配额检查的冷却计时器，避免频繁写入时重复扫描缓存目录。
    m_lastEnforceTimer.start();
    setCacheDir(ConfigService::defaultCacheDir());
}

ComponentCacheService::~ComponentCacheService() = default;

/**
 * @brief 返回进程内唯一的缓存服务实例。
 */
ComponentCacheService* ComponentCacheService::instance() {
    if (!s_instance) {
        s_instance = std::unique_ptr<ComponentCacheService>(new ComponentCacheService());
    }
    return s_instance.get();
}

/**
 * @brief 设置缓存目录，并按需迁移旧目录中的缓存内容。
 * @param cacheDir 新缓存目录路径
 * @param migrateExistingCache 是否迁移旧缓存
 * @param error 输出失败原因，可为空
 */
bool ComponentCacheService::setCacheDir(const QString& cacheDir, bool migrateExistingCache, QString* error) {
    CacheDirectoryCoordinator coordinator(*this);
    return coordinator.setDirectory(cacheDir, migrateExistingCache, error);
}

/**
 * @brief 获取当前缓存根目录。
 */
QString ComponentCacheService::cacheDir() const {
    QMutexLocker locker(&m_cacheDirMutex);
    return m_cacheDir;
}

/**
 * @brief 获取指定元器件的缓存目录。
 * @param lcscId 元器件编号
 */
QString ComponentCacheService::componentCacheDir(const QString& lcscId) const {
    QMutexLocker locker(&m_cacheDirMutex);
    // 保存经过安全校验的元器件缓存目录路径。
    const QString path = CachePathResolver::componentDir(m_cacheDir, lcscId);
    if (path.isEmpty()) {
        qWarning() << "componentCacheDir: invalid lcscId, rejecting:" << lcscId;
    }
    return path;
}

/**
 * @brief 确保指定元器件的缓存目录存在。
 * @param lcscId 元器件编号
 */
QString ComponentCacheService::ensureComponentDir(const QString& lcscId) const {
    // 注意：不再获取锁，因为：
    // 1. componentCacheDir 只是构建路径字符串（只读操作）
    // 2. dir.mkpath 是线程安全的
    // 3. savePreviewImage 已经在持有 m_mutex 的情况下调用此函数
    QString dirPath = componentCacheDir(lcscId);
    if (dirPath.isEmpty()) {
        return QString();
    }
    QDir dir(dirPath);
    if (!dir.exists()) {
        if (!dir.mkpath(dirPath))
            return QString();
    } else if (!CacheSafety::isOwnedComponentDirectory(cacheDir(), dirPath) &&
               !dir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot).isEmpty()) {
        qWarning() << "ensureComponentDir: refusing to reuse non-empty unowned directory:" << dirPath;
        return QString();
    }
    return dirPath;
}

/**
 * @brief 确保公共三维模型缓存目录存在。
 */
QString ComponentCacheService::ensureModel3DCacheDir() const {
    // 构造公共三维模型缓存目录的预期路径。
    const QString dirPath = cacheDir() + "/model3d";
    // 接收所有权目录校验或创建失败时的具体原因。
    QString error;
    return CacheSafety::ensureOwnedModel3DDirectory(cacheDir(), &error) ? dirPath : QString();
}

/**
 * @brief 判断元器件是否具有可用的完整磁盘缓存。
 * @param lcscId 元器件编号
 */
bool ComponentCacheService::hasCache(const QString& lcscId) const {
    // 统一由完整性检查负责路径读取，避免目录迁移期间出现先检查后失效。
    return isCacheValid(lcscId);
}

/**
 * @brief 校验元数据、身份字段和 CAD 文件是否组成有效缓存。
 * @param lcscId 元器件编号
 */
bool ComponentCacheService::isCacheValid(const QString& lcscId) const {
    return ComponentCacheReadCoordinator::isCacheValid(*this, lcscId);
}

/**
 * @brief 判断元器件元数据是否存在于一级内存缓存。
 * @param lcscId 元器件编号
 */
bool ComponentCacheService::hasInMemoryCache(const QString& lcscId) const {
    return m_memoryCache.containsMetadata(lcscId);
}

// ==================== L1 内存缓存操作 ====================

/**
 * @brief 从一级内存缓存读取元器件元数据。
 * @param lcscId 元器件编号
 */
QJsonObject ComponentCacheService::loadMetadataFromMemory(const QString& lcscId) const {
    return m_memoryCache.loadMetadata(lcscId);
}

/**
 * @brief 将元器件元数据写入一级内存缓存。
 * @param lcscId 元器件编号
 * @param metadata 元数据对象
 */
void ComponentCacheService::saveMetadataToMemory(const QString& lcscId, const QJsonObject& metadata) {
    // 保存写入元数据后一级内存缓存的最新占用量。
    const qint64 sizeAfterUpdate = m_memoryCache.saveMetadata(lcscId, metadata);
    // 锁外发送信号
    emit memoryCacheSizeChanged(sizeAfterUpdate);
}

/**
 * @brief 从一级内存缓存读取符号数据。
 * @param lcscId 元器件编号
 */
QByteArray ComponentCacheService::loadSymbolDataFromMemory(const QString& lcscId) const {
    return m_memoryCache.loadSymbol(lcscId);
}

/**
 * @brief 将符号数据写入一级内存缓存。
 * @param lcscId 元器件编号
 * @param data 符号数据
 */
void ComponentCacheService::saveSymbolDataToMemory(const QString& lcscId, const QByteArray& data) {
    // 保存写入符号数据后一级内存缓存的可选占用量。
    const std::optional<qint64> sizeAfterUpdate = m_memoryCache.saveSymbol(lcscId, data);
    if (!sizeAfterUpdate.has_value()) {
        return;
    }
    // 锁外发送信号
    emit memoryCacheSizeChanged(*sizeAfterUpdate);
}

/**
 * @brief 从一级内存缓存读取封装数据。
 * @param lcscId 元器件编号
 */
QByteArray ComponentCacheService::loadFootprintDataFromMemory(const QString& lcscId) const {
    return m_memoryCache.loadFootprint(lcscId);
}

/**
 * @brief 将封装数据写入一级内存缓存。
 * @param lcscId 元器件编号
 * @param data 封装数据
 */
void ComponentCacheService::saveFootprintDataToMemory(const QString& lcscId, const QByteArray& data) {
    // 保存写入封装数据后一级内存缓存的可选占用量。
    const std::optional<qint64> sizeAfterUpdate = m_memoryCache.saveFootprint(lcscId, data);
    if (!sizeAfterUpdate.has_value()) {
        return;
    }
    // 锁外发送信号
    emit memoryCacheSizeChanged(*sizeAfterUpdate);
}

// ==================== L2 磁盘缓存操作 ====================

/**
 * @brief 从二级磁盘缓存读取完整元器件数据。
 * @param lcscId 元器件编号
 */
QSharedPointer<ComponentData> ComponentCacheService::loadComponentData(const QString& lcscId) const {
    return ComponentCacheReadCoordinator::loadComponentData(*this, lcscId);
}

/**
 * @brief 合并并持久化元器件元数据，同时保留有效的三维模型关联。
 * @param componentId 元器件编号
 * @param data 元器件数据
 * @param expectedGeneration 请求创建时捕获的缓存代次
 * @param replaceModel3DMetadata 是否替换而非合并三维模型元数据
 */
void ComponentCacheService::saveComponentMetadata(const QString& componentId,
                                                  const ComponentData& data,
                                                  uint64_t expectedGeneration,
                                                  bool replaceModel3DMetadata) {
    ComponentCacheMetadataWriter metadataWriter(*this);
    const std::optional<qint64> sizeAfterUpdate =
        metadataWriter.write(componentId, data, expectedGeneration, replaceModel3DMetadata);
    if (!sizeAfterUpdate.has_value())
        return;

    enforceDiskCacheLimit();
    emit memoryCacheSizeChanged(*sizeAfterUpdate);
    emit cacheSaved(componentId);
    LOG_DEBUG(LogModule::Core, "Saved component metadata to cache: {}", componentId);
}

/**
 * @brief 异步保存元器件元数据并校验缓存代次。
 * @param componentId 元器件编号。
 * @param data 元器件数据。
 * @param expectedGeneration 期望的缓存代次。
 * @param replaceModel3DMetadata 是否替换三维模型元数据。
 */
void ComponentCacheService::saveComponentMetadataAsync(const QString& componentId,
                                                       const ComponentData& data,
                                                       uint64_t expectedGeneration,
                                                       bool replaceModel3DMetadata) {
    // 异步版本：在后台线程执行文件I/O，不阻塞UI
    // 复制需要的数据以供后台线程使用
    const ComponentData dataCopy = data;
    // 如果调用方没有传入 generation，则在入队时捕获当前值
    const uint64_t generation = (expectedGeneration != 0) ? expectedGeneration : m_cacheGeneration.load();

    (void)QtConcurrent::run([this, componentId, dataCopy, generation, replaceModel3DMetadata]() {
        // 写入前检查代次：如果 clearAllCache 已调用，丢弃本次写入
        if (m_cacheGeneration.load() != generation) {
            return;
        }
        saveComponentMetadata(componentId, dataCopy, generation, replaceModel3DMetadata);
    });
}

/**
 * @brief 将符号数据原子写入二级磁盘缓存。
 * @param lcscId 元器件编号
 * @param data 符号数据
 * @param expectedGeneration 请求创建时捕获的缓存代次
 */
void ComponentCacheService::saveSymbolData(const QString& lcscId, const QByteArray& data, uint64_t expectedGeneration) {
    ComponentCacheCadDataWriter writer(*this);
    writer.writeSymbol(lcscId, data, expectedGeneration);
}

/**
 * @brief 从二级磁盘缓存读取符号数据。
 * @param lcscId 元器件编号
 */
QByteArray ComponentCacheService::loadSymbolData(const QString& lcscId) const {
    return ComponentCacheFileReadCoordinator::loadSymbolData(*this, lcscId);
}

/**
 * @brief 将封装数据原子写入二级磁盘缓存。
 * @param lcscId 元器件编号
 * @param data 封装数据
 * @param expectedGeneration 请求创建时捕获的缓存代次
 */
void ComponentCacheService::saveFootprintData(const QString& lcscId,
                                              const QByteArray& data,
                                              uint64_t expectedGeneration) {
    ComponentCacheCadDataWriter writer(*this);
    writer.writeFootprint(lcscId, data, expectedGeneration);
}

/**
 * @brief 从二级磁盘缓存读取封装数据。
 * @param lcscId 元器件编号
 */
QByteArray ComponentCacheService::loadFootprintData(const QString& lcscId) const {
    return ComponentCacheFileReadCoordinator::loadFootprintData(*this, lcscId);
}

/**
 * @brief 将 CAD 原始 JSON 原子写入二级磁盘缓存。
 * @param lcscId 元器件编号
 * @param cadData CAD JSON 数据
 * @param expectedGeneration 请求创建时捕获的缓存代次
 */
void ComponentCacheService::saveCadDataJson(const QString& lcscId,
                                            const QByteArray& cadData,
                                            uint64_t expectedGeneration) {
    ComponentCacheCadDataWriter writer(*this);
    writer.writeCadJson(lcscId, cadData, expectedGeneration);
}

/**
 * @brief 从二级磁盘缓存读取 CAD 原始 JSON。
 * @param lcscId 元器件编号
 */
QByteArray ComponentCacheService::loadCadDataJson(const QString& lcscId) const {
    return ComponentCacheFileReadCoordinator::loadCadDataJson(*this, lcscId);
}

/**
 * @brief 判断符号、封装和 CAD 数据缓存是否完整。
 * @param lcscId 元器件编号
 */
bool ComponentCacheService::hasSymbolFootprintCache(const QString& lcscId) const {
    return ComponentCacheReadCoordinator::hasSymbolFootprintCache(*this, lcscId);
}

/**
 * @brief 从二级磁盘缓存读取指定预览图。
 * @param lcscId 元器件编号
 * @param imageIndex 预览图序号
 */
QByteArray ComponentCacheService::loadPreviewImage(const QString& lcscId, int imageIndex) const {
    return ComponentCacheFileReadCoordinator::loadPreviewImage(*this, lcscId, imageIndex);
}

/**
 * @brief 使用 Qt 图片解码器拒绝错误页、截断文件和其他非图片缓存内容。
 * @param imageData 预览图原始数据
 */
bool ComponentCacheService::isValidPreviewImageData(const QByteArray& imageData) {
    return CacheDataValidator::isValidPreviewImage(imageData);
}

/**
 * @brief 将预览图写入磁盘缓存。
 * @param lcscId LCSC 元器件编号。
 * @param imageData 预览图数据。
 * @param imageIndex 预览图索引。
 * @param expectedGeneration 期望的缓存代次。
 */
void ComponentCacheService::savePreviewImage(const QString& lcscId,
                                             const QByteArray& imageData,
                                             int imageIndex,
                                             uint64_t expectedGeneration) {
    ComponentCachePreviewImageWriter imageWriter(*this);
    imageWriter.write(lcscId, imageData, imageIndex, expectedGeneration);
}

/**
 * @brief 下载预览图并返回其内容。
 * @param lcscId LCSC 元器件编号。
 * @param imageUrl 预览图 URL。
 * @param imageIndex 预览图索引。
 * @param diag 网络诊断输出。
 * @param cancelled 取消标志。
 * @param weakNetwork 是否启用弱网策略。
 * @param expectedGeneration 期望的缓存代次。
 * @return 下载到的预览图数据。
 */
QByteArray ComponentCacheService::downloadPreviewImage(const QString& lcscId,
                                                       const QString& imageUrl,
                                                       int imageIndex,
                                                       ComponentExportStatus::NetworkDiagnostics* diag,
                                                       QAtomicInt* cancelled,
                                                       bool weakNetwork,
                                                       uint64_t expectedGeneration) {
    PreviewImageDownloadService downloader(*this);
    return downloader.download(lcscId, imageUrl, imageIndex, diag, cancelled, weakNetwork, expectedGeneration);
}

/**
 * @brief 从二级磁盘缓存读取数据手册。
 */
QByteArray ComponentCacheService::loadDatasheet(const QString& lcscId) const {
    return DatasheetCacheFileStore::load(*this, lcscId);
}

/**
 * @brief 校验 PDF 签名或 HTML 文档标记，拒绝被错误响应污染的数据手册缓存。
 * @param datasheetData 数据手册原始数据
 * @param format 数据手册格式
 */
bool ComponentCacheService::isValidDatasheetData(const QByteArray& datasheetData, const QString& format) {
    return CacheDataValidator::isValidDatasheet(datasheetData, format);
}

/**
 * @brief 将数据手册写入磁盘缓存。
 * @param lcscId LCSC 元器件编号。
 * @param datasheetData 数据手册内容。
 * @param format 数据手册格式。
 * @param expectedGeneration 期望的缓存代次。
 */
void ComponentCacheService::saveDatasheet(const QString& lcscId,
                                          const QByteArray& datasheetData,
                                          const QString& format,
                                          uint64_t expectedGeneration) {
    DatasheetCacheFileStore fileStore(*this);
    fileStore.save(lcscId, datasheetData, format, expectedGeneration);
}

/**
 * @brief 下载数据手册并返回其内容。
 * @param lcscId LCSC 元器件编号。
 * @param datasheetUrl 数据手册 URL。
 * @param format 输出的数据手册格式。
 * @param diag 网络诊断输出。
 * @param cancelled 取消标志。
 * @param weakNetwork 是否启用弱网策略。
 * @param expectedGeneration 期望的缓存代次。
 * @return 下载到的数据手册内容。
 */
QByteArray ComponentCacheService::downloadDatasheet(const QString& lcscId,
                                                    const QString& datasheetUrl,
                                                    QString* format,
                                                    ComponentExportStatus::NetworkDiagnostics* diag,
                                                    QAtomicInt* cancelled,
                                                    bool weakNetwork,
                                                    uint64_t expectedGeneration) {
    DatasheetDownloadService downloader(*this);
    return downloader.download(lcscId, datasheetUrl, format, diag, cancelled, weakNetwork, expectedGeneration);
}

/**
 * @brief 判断指定格式的三维模型文件是否存在且非空。
 * @param uuid 三维模型 UUID
 * @param extension 文件扩展名
 * @return 缓存文件存在且非空时返回 true
 */
bool ComponentCacheService::hasModel3DCached(const QString& uuid, const QString& extension) const {
    // 协调器只读取缓存服务状态；const_cast 仅用于复用统一的私有锁边界，不会修改服务数据。
    ComponentCacheModel3DCoordinator coordinator(const_cast<ComponentCacheService&>(*this));
    return coordinator.hasCached(uuid, extension);
}

/**
 * @brief 从公共三维模型缓存读取指定格式的数据。
 * @param uuid 三维模型 UUID
 * @param extension 文件扩展名
 * @return 模型数据，不存在或无效时为空
 */
QByteArray ComponentCacheService::loadModel3D(const QString& uuid, const QString& extension) const {
    // 协调器只读取缓存服务状态；const_cast 仅用于复用统一的私有锁边界，不会修改服务数据。
    ComponentCacheModel3DCoordinator coordinator(const_cast<ComponentCacheService&>(*this));
    return coordinator.load(uuid, extension);
}

/**
 * @brief 将三维模型写入公共缓存。
 * @param uuid 三维模型 UUID。
 * @param data 三维模型文件内容。
 * @param extension 文件扩展名。
 * @param expectedGeneration 期望的缓存代次。
 */
void ComponentCacheService::saveModel3D(const QString& uuid,
                                        const QByteArray& data,
                                        const QString& extension,
                                        uint64_t expectedGeneration) {
    ComponentCacheModel3DCoordinator coordinator(*this);
    coordinator.save(uuid, data, extension, expectedGeneration);
}

/**
 * @brief 将缓存中的三维模型复制到指定文件。
 * @param uuid 三维模型 UUID。
 * @param extension 文件扩展名。
 * @param destinationPath 目标文件路径。
 * @return 复制是否成功。
 */
bool ComponentCacheService::copyModel3DToFile(const QString& uuid,
                                              const QString& extension,
                                              const QString& destinationPath) const {
    // 协调器只读取缓存服务状态；const_cast 仅用于复用统一的私有锁边界，不会修改服务数据。
    ComponentCacheModel3DCoordinator coordinator(const_cast<ComponentCacheService&>(*this));
    return coordinator.copyToFile(uuid, extension, destinationPath);
}

// ==================== 缓存管理 ====================

/**
 * @brief 删除指定元器件的可验证缓存。
 * @param lcscId 元器件编号
 */
void ComponentCacheService::removeCache(const QString& lcscId) {
    ComponentCacheMaintenance maintenance(*this);
    maintenance.remove(lcscId);
}

/**
 * @brief 清除指定元器件的旧请求屏蔽标记。
 * @param lcscId 元器件编号
 */
void ComponentCacheService::clearTombstone(const QString& lcscId) {
    m_tombstones.clearComponent(lcscId);
}

/**
 * @brief 清除全局旧请求屏蔽标记。
 */
void ComponentCacheService::clearGlobalTombstone() {
    m_tombstones.clearGlobal();
}

/**
 * @brief 判断元器件是否仍被旧请求屏蔽。
 * @param lcscId 元器件编号
 * @return 被屏蔽时返回 true
 */
bool ComponentCacheService::isTombstoned(const QString& lcscId) const {
    return m_tombstones.isBlocked(lcscId);
}

/**
 * @brief 清空一级和二级缓存，并使旧异步写入失效。
 */
void ComponentCacheService::clearAllCache() {
    ComponentCacheMaintenance maintenance(*this);
    maintenance.clearAll();
}

/**
 * @brief 清空一级内存缓存并使清理前创建的异步写入失效。
 */
void ComponentCacheService::clearMemoryCache() {
    ComponentCacheMaintenance maintenance(*this);
    maintenance.clearMemory();
}

/**
 * @brief 枚举当前缓存目录中具有有效元数据的元器件编号。
 */
QStringList ComponentCacheService::getCachedComponentIds() const {
    return ComponentCacheMaintenance::cachedComponentIds(*this);
}

/**
 * @brief 统计当前缓存目录的磁盘占用大小。
 */
qint64 ComponentCacheService::getCacheSize() const {
    return ComponentCacheMaintenance::cacheSize(*this);
}

/**
 * @brief 统计清理操作可以安全处理的缓存条目，不把未知用户文件计入范围。
 */
int ComponentCacheService::getOwnedCacheEntryCount() const {
    QMutexLocker locker(&m_diskWriteMutex);
    // 获取当前缓存根目录，后续只统计可验证归属的条目。
    const QString root = cacheDir();
    return CacheSafety::ownedComponentDirectories(root).size() + CacheSafety::ownedModel3DFiles(root).size();
}

/**
 * @brief 返回一级内存缓存的当前占用大小。
 */
qint64 ComponentCacheService::getMemoryCacheSize() const {
    return m_memoryCache.totalCost();
}

/**
 * @brief 递归计算指定目录及其子目录的文件大小。
 * @param dirPath 目录路径
 * @return 目录内容大小（字节）
 */
qint64 ComponentCacheService::calculateDirSize(const QString& dirPath) const {
    // 累计当前目录及所有受控子目录中的文件大小。
    qint64 size = 0;
    QDir dir(dirPath);
    if (!dir.exists()) {
        return size;
    }

    for (const QFileInfo& info : dir.entryInfoList(QDir::Files)) {
        size += info.size();
    }

    for (const QString& subDir : dir.entryList(QDir::Dirs)) {
        if (subDir != "." && subDir != "..") {
            size += calculateDirSize(dirPath + "/" + subDir);
        }
    }

    return size;
}

/**
 * @brief 将磁盘缓存裁剪到指定大小以内。
 * @param targetSizeBytes 目标缓存大小（字节）
 */
void ComponentCacheService::pruneCache(qint64 targetSizeBytes) {
    // 手动清理必须与目录迁移串行化，避免清理旧目录或新目录中的部分内容。
    qint64 newSize = 0;
    {
        QMutexLocker diskLocker(&m_diskWriteMutex);
        CachePruner pruner(cacheDir());
        newSize = pruner.pruneTo(targetSizeBytes);
    }
    emit cacheSizeChanged(newSize);
}

/**
 * @brief 设置一级内存缓存的最大容量。
 * @param maxSizeMB 最大容量（MB）
 */
void ComponentCacheService::setMemoryCacheLimit(int maxSizeMB) {
    m_memoryCache.setMaxCost(maxSizeMB * 1024 * 1024);
    LOG_DEBUG(LogModule::Core, "Memory cache limit set to: {} MB", maxSizeMB);
}

/**
 * @brief 获取一级内存缓存的最大容量。
 */
int ComponentCacheService::memoryCacheLimit() const {
    return m_memoryCache.maxCost() / (1024 * 1024);
}

/**
 * @brief 设置二级磁盘缓存的最大容量并立即触发清理。
 * @param maxSizeMB 最大容量（MB）
 */
void ComponentCacheService::setDiskCacheLimit(int maxSizeMB) {
    {
        QMutexLocker locker(&m_mutex);
        m_diskCacheLimitMB = qMax(1, maxSizeMB);
    }

    // 用户主动修改限制时绕过冷却机制，立即执行清理
    enforceDiskCacheLimit(/*bypassCooldown=*/true);
    LOG_DEBUG(LogModule::Core, "Disk cache limit set to: {} MB", maxSizeMB);
}

/**
 * @brief 获取二级磁盘缓存的最大容量。
 */
int ComponentCacheService::diskCacheLimit() const {
    QMutexLocker locker(&m_mutex);
    return m_diskCacheLimitMB;
}

/**
 * @brief 按冷却策略执行二级磁盘缓存容量限制。
 * @param bypassCooldown 是否忽略冷却时间
 */
void ComponentCacheService::enforceDiskCacheLimit(bool bypassCooldown) {
    ComponentCacheQuotaEnforcer quotaEnforcer(*this);
    quotaEnforcer.enforce(bypassCooldown);
}

/**
 * @brief 从二级磁盘缓存读取元器件元数据。
 * @param lcscId 元器件编号
 * @return 元数据对象，不存在或无效时为空
 */
QJsonObject ComponentCacheService::loadMetadata(const QString& lcscId) const {
    return ComponentCacheFileReadCoordinator::loadMetadata(*this, lcscId);
}

/**
 * @brief 将元器件元数据原子写入二级磁盘缓存。
 * @param lcscId 元器件编号
 * @param metadata 元数据对象
 */
void ComponentCacheService::saveMetadata(const QString& lcscId, const QJsonObject& metadata) {
    if (ensureComponentDir(lcscId).isEmpty()) {
        return;
    }
    // 获取元器件元数据文件的目标路径。
    QString metaPath = metadataPath(lcscId);
    // 复制调用方元数据并附加本项目缓存所有权标记。
    QJsonObject ownedMetadata = metadata;
    ownedMetadata[QStringLiteral("cacheOwner")] = QStringLiteral("EasyKiConverter");
    ownedMetadata[QStringLiteral("cacheEntryVersion")] = 1;
    QJsonDocument doc(ownedMetadata);
    if (!CacheMetadataStore::writeAtomically(metaPath, doc.toJson(QJsonDocument::Indented))) {
        LOG_WARN(LogModule::Core, "Failed to open metadata file for writing: {}", metaPath);
    }
}

/**
 * @brief 根据格式和现有文件解析数据手册的实际路径。
 * @param lcscId 元器件编号
 * @param preferredFormat 优先使用的数据格式
 * @param forWrite 是否解析用于写入的路径
 * @return 数据手册缓存文件路径
 */
QString ComponentCacheService::resolveDatasheetPath(const QString& lcscId,
                                                    const QString& preferredFormat,
                                                    bool forWrite) const {
    // 计算元器件数据手册的无扩展名基础路径。
    const QString basePath = datasheetPath(lcscId);
    // 统一格式字符串，便于识别 pdf 或 html 请求。
    const QString normalizedFormat = preferredFormat.toLower();

    if (normalizedFormat.contains("pdf")) {
        if (!forWrite && !QFileInfo::exists(basePath + ".pdf") && QFileInfo::exists(basePath + ".html")) {
            return basePath + ".html";
        }
        return basePath + ".pdf";
    }
    if (normalizedFormat.contains("html")) {
        if (!forWrite && !QFileInfo::exists(basePath + ".html") && QFileInfo::exists(basePath + ".pdf")) {
            return basePath + ".pdf";
        }
        return basePath + ".html";
    }

    // 构造两种受支持格式对应的候选缓存路径。
    const QString pdfPath = basePath + ".pdf";
    // 构造 HTML 数据手册的候选缓存路径。
    const QString htmlPath = basePath + ".html";

    if (!forWrite) {
        if (QFileInfo::exists(pdfPath)) {
            return pdfPath;
        }
        if (QFileInfo::exists(htmlPath)) {
            return htmlPath;
        }
    }

    return pdfPath;
}

/**
 * @brief 执行缓存目录的完整性修复。
 */
void ComponentCacheService::selfHealCache() {
    CacheHealthManager healer(cacheDir());
    healer.healAll();
}

/**
 * @brief 获取指定元器件缓存目录的最后修改时间。
 * @param lcscId 元器件编号
 * @return 缓存目录最后修改时间
 */
QDateTime ComponentCacheService::getCacheAccessTime(const QString& lcscId) const {
    // 获取元器件缓存目录路径，用于读取其最后修改时间。
    QString dirPath = componentCacheDir(lcscId);
    QFileInfo info(dirPath);
    if (info.exists()) {
        return info.lastModified();
    }
    return QDateTime();
}

/**
 * @brief 构造元器件元数据文件路径。
 * @param lcscId 元器件编号
 * @return 元数据文件路径
 */
QString ComponentCacheService::metadataPath(const QString& lcscId) const {
    return CachePathResolver::metadataPath(cacheDir(), lcscId);
}

/**
 * @brief 构造元器件预览图文件路径。
 * @param lcscId 元器件编号
 * @param index 预览图序号
 * @return 预览图缓存文件路径
 */
QString ComponentCacheService::previewImagePath(const QString& lcscId, int index) const {
    return CachePathResolver::previewImagePath(cacheDir(), lcscId, index);
}

/**
 * @brief 构造元器件数据手册基础路径。
 * @param lcscId 元器件编号
 * @return 数据手册基础路径
 */
QString ComponentCacheService::datasheetPath(const QString& lcscId) const {
    return CachePathResolver::datasheetPath(cacheDir(), lcscId);
}

/**
 * @brief 根据经过校验的模型标识构造三维模型缓存路径。
 * @param uuid 三维模型 UUID
 * @param extension 文件扩展名
 * @return 三维模型缓存路径，无效输入时为空
 */
QString ComponentCacheService::model3DPath(const QString& uuid, const QString& extension) const {
    // 保存经过路径解析器校验的三维模型缓存路径。
    const QString path = CachePathResolver::model3DPath(cacheDir(), uuid, extension);
    if (path.isEmpty()) {
        qWarning() << "model3DPath: invalid uuid or extension, rejecting:" << uuid << extension;
    }
    return path;
}

}  // namespace EasyKiConverter

// 我攻略了你之后，别人也可以攻略你[惊讶]？！
// 不是这啥情况啊[疑惑]？我记得旮旯game不是单机游戏吗，
// 什么时候联网的[思考]？而就算你是玩家[给心心]！
// 我是这个攻略对象，也不能这样子吧[傲娇]，
// 是不是有点不道德啊[生气]？他是黑客[doge]。
// 什么黑客[思考]哪里黑了[大哭]！？哦还是国际服，
// 我不玩了[灵魂出窍]。
