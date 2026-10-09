#include "ComponentCacheReadCoordinator.h"

#include "CacheComponentDataReader.h"
#include "CacheDataValidator.h"
#include "CacheFileLayout.h"
#include "CacheMetadataStore.h"
#include "ComponentCacheBinaryFileStore.h"
#include "ComponentCacheService.h"
#include "utils/logging/LogMacros.h"

#include <QFile>
#include <QFileInfo>
#include <QMutexLocker>

namespace EasyKiConverter {

namespace {

/**
 * @brief 从磁盘读取并校验 CAD 原始缓存文件。
 * @param path CAD 缓存文件路径。
 * @return 文件存在且内容有效时返回 true。
 */
bool hasValidCadDataFile(const QString& path) {
    QFile file(path);  // 待读取的 CAD 缓存文件。
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    const QByteArray data = file.readAll();  // CAD 缓存文件内容。
    file.close();
    return CacheDataValidator::isValidCadData(data);
}

}  // namespace

/**
 * @brief 在目录迁移锁内校验元数据身份、三维模型和 CAD 文件。
 * @param owner 所属缓存服务。
 * @param componentId 元器件编号。
 * @return 缓存完整且可用时返回 true。
 */
bool ComponentCacheReadCoordinator::isCacheValid(const ComponentCacheService& owner, const QString& componentId) {
    QMutexLocker diskLocker(&owner.m_diskWriteMutex);  // 保护目录迁移期间的校验。
    const QJsonObject metadata = CacheMetadataStore::read(owner.metadataPath(componentId));  // 元数据对象。
    if (metadata.isEmpty()) {
        return false;
    }

    if (metadata.contains(QStringLiteral("lcscId")) &&
        (!metadata.value(QStringLiteral("lcscId")).isString() ||
         metadata.value(QStringLiteral("lcscId")).toString().compare(componentId, Qt::CaseInsensitive) != 0)) {
        return false;
    }

    if (!CacheMetadataStore::hasValidModel3D(metadata)) {
        return false;
    }

    const bool hasCadJson =  // CAD JSON 是否有效。
        hasValidCadDataFile(CacheFileLayout::cadDataFile(owner.componentCacheDir(componentId)));
    const bool hasBasicIdentity =  // 元数据是否包含基本身份信息。
        !metadata.value(QStringLiteral("lcscId")).toString().isEmpty() ||
        !metadata.value(QStringLiteral("name")).toString().isEmpty();
    return hasCadJson || hasBasicIdentity;
}

/**
 * @brief 在磁盘读取锁和内存缓存锁内解析完整元器件数据。
 * @param owner 所属缓存服务。
 * @param componentId 元器件编号。
 * @return 解析后的元器件数据；缓存无效时返回空指针。
 */
QSharedPointer<ComponentData> ComponentCacheReadCoordinator::loadComponentData(const ComponentCacheService& owner,
                                                                               const QString& componentId) {
    QMutexLocker diskLocker(&owner.m_diskWriteMutex);  // 保护目录迁移期间的读取。
    const QString metaPath = owner.metadataPath(componentId);  // 元数据文件路径。
    if (!QFileInfo::exists(metaPath)) {
        return nullptr;
    }

    QMutexLocker locker(&owner.m_mutex);  // 保护一级缓存相关状态。
    const QJsonObject metadata = CacheMetadataStore::read(metaPath);  // 元数据对象。
    if (metadata.isEmpty()) {
        return nullptr;
    }

    if (!CacheMetadataStore::hasValidModel3D(metadata)) {
        LOG_WARN(LogModule::Core, "Rejected component cache with invalid 3D metadata: {}", componentId);
        return nullptr;
    }

    const auto componentData = CacheComponentDataReader::read(componentId, metadata);  // 解析后的组件数据。
    if (!componentData) {
        return nullptr;
    }

    LOG_DEBUG(LogModule::Core, "Loaded component data from disk cache: {}", componentId);
    return componentData;
}

/**
 * @brief 在目录迁移锁内验证 CAD 原始 JSON 是否可解析。
 * @param owner 所属缓存服务。
 * @param componentId 元器件编号。
 * @return CAD 数据有效时返回 true。
 */
bool ComponentCacheReadCoordinator::hasSymbolFootprintCache(const ComponentCacheService& owner,
                                                            const QString& componentId) {
    QMutexLocker diskLocker(&owner.m_diskWriteMutex);  // 保护目录迁移期间的读取。
    const QString cadDataPath =  // CAD JSON 文件路径。
        CacheFileLayout::cadDataFile(owner.componentCacheDir(componentId));
    const bool hasMemoryMetadata = owner.m_memoryCache.containsMetadata(componentId);  // 一级缓存是否命中。
    const bool valid = hasValidCadDataFile(cadDataPath);  // CAD JSON 是否有效。
    if (hasMemoryMetadata) {
        LOG_DEBUG(LogModule::Core,
                  "hasSymbolFootprintCache: memory hit for {}, cad_data.json exists: {}",
                  componentId,
                  valid);
    } else if (!valid) {
        LOG_DEBUG(LogModule::Core, "hasSymbolFootprintCache: no cad_data.json for {}", componentId);
    } else {
        LOG_DEBUG(LogModule::Core, "hasSymbolFootprintCache: valid CAD data for {}", componentId);
    }
    return valid;
}

}  // namespace EasyKiConverter
