#include "ComponentCacheFileReadCoordinator.h"

#include "CacheFileLayout.h"
#include "CacheMetadataStore.h"
#include "ComponentCacheBinaryFileStore.h"
#include "ComponentCacheService.h"

#include <QMutexLocker>

namespace EasyKiConverter {

/**
 * @brief 先查询一级缓存，再在目录迁移锁内读取符号文件。
 * @param owner 所属缓存服务。
 * @param componentId 元器件编号。
 * @return 符号缓存数据。
 */
QByteArray ComponentCacheFileReadCoordinator::loadSymbolData(const ComponentCacheService& owner,
                                                             const QString& componentId) {
    const QByteArray memoryData = owner.loadSymbolDataFromMemory(componentId);  // 一级缓存数据。
    if (!memoryData.isEmpty()) {
        return memoryData;
    }

    QMutexLocker diskLocker(&owner.m_diskWriteMutex);  // 保护目录迁移期间的读取。
    const QString path = CacheFileLayout::symbolFile(owner.componentCacheDir(componentId));  // 符号文件路径。
    return ComponentCacheBinaryFileStore::readCadData(path);
}

/**
 * @brief 先查询一级缓存，再在目录迁移锁内读取封装文件。
 * @param owner 所属缓存服务。
 * @param componentId 元器件编号。
 * @return 封装缓存数据。
 */
QByteArray ComponentCacheFileReadCoordinator::loadFootprintData(const ComponentCacheService& owner,
                                                                const QString& componentId) {
    const QByteArray memoryData = owner.loadFootprintDataFromMemory(componentId);  // 一级缓存数据。
    if (!memoryData.isEmpty()) {
        return memoryData;
    }

    QMutexLocker diskLocker(&owner.m_diskWriteMutex);  // 保护目录迁移期间的读取。
    const QString path = CacheFileLayout::footprintFile(owner.componentCacheDir(componentId));  // 封装文件路径。
    return ComponentCacheBinaryFileStore::readCadData(path);
}

/**
 * @brief 在目录迁移锁内读取并校验原始 CAD JSON 文件。
 * @param owner 所属缓存服务。
 * @param componentId 元器件编号。
 * @return CAD JSON 数据。
 */
QByteArray ComponentCacheFileReadCoordinator::loadCadDataJson(const ComponentCacheService& owner,
                                                              const QString& componentId) {
    QMutexLocker diskLocker(&owner.m_diskWriteMutex);  // 保护目录迁移期间的读取。
    const QString path = CacheFileLayout::cadDataFile(owner.componentCacheDir(componentId));  // CAD JSON 路径。
    return ComponentCacheBinaryFileStore::readCadData(path);
}

/**
 * @brief 在目录迁移锁内读取并校验指定预览图文件。
 * @param owner 所属缓存服务。
 * @param componentId 元器件编号。
 * @param imageIndex 预览图索引。
 * @return 预览图数据。
 */
QByteArray ComponentCacheFileReadCoordinator::loadPreviewImage(const ComponentCacheService& owner,
                                                               const QString& componentId,
                                                               int imageIndex) {
    if (imageIndex < 0 || imageIndex >= 3) {
        return QByteArray();
    }

    QMutexLocker diskLocker(&owner.m_diskWriteMutex);  // 保护目录迁移期间的读取。
    QString path;  // 预览图文件路径。
    {
        QMutexLocker locker(&owner.m_mutex);  // 保护缓存路径计算。
        path = owner.previewImagePath(componentId, imageIndex);
    }
    return ComponentCacheBinaryFileStore::readPreviewImage(path);
}

/**
 * @brief 在目录迁移锁内读取并解析元数据 JSON。
 * @param owner 所属缓存服务。
 * @param componentId 元器件编号。
 * @return 元数据对象。
 */
QJsonObject ComponentCacheFileReadCoordinator::loadMetadata(const ComponentCacheService& owner,
                                                            const QString& componentId) {
    QMutexLocker diskLocker(&owner.m_diskWriteMutex);
    return CacheMetadataStore::read(owner.metadataPath(componentId));
}

}  // namespace EasyKiConverter
