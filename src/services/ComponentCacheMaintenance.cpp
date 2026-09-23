#include "ComponentCacheMaintenance.h"

#include "BomParser.h"
#include "CacheMetadataStore.h"
#include "ComponentCacheService.h"
#include "utils/logging/LogMacros.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutexLocker>
#include <QSet>

namespace EasyKiConverter {

/** @brief 保存维护协调器所属的缓存服务。 */
ComponentCacheMaintenance::ComponentCacheMaintenance(ComponentCacheService& owner,
                                                     const CacheSafety::TrashFunction& trash)
    : m_owner(owner), m_trash(trash) {}

/**
 * @brief 删除指定元器件的一级和二级缓存。
 * @details 先递增代次并锁定 tombstone，再删除磁盘和内存内容，避免旧异步回调复活缓存。
 */
void ComponentCacheMaintenance::remove(const QString& componentId) {
    const QString normalizedId = componentId.toUpper();
    if (!BomParser::validateId(normalizedId)) {
        qWarning() << "removeCache: invalid lcscId, ignoring:" << componentId;
        return;
    }

    m_owner.m_cacheGeneration.fetch_add(1);
    {
        // 锁顺序保持为 disk 后 tombstone，与缓存写入策略一致。
        QMutexLocker diskLocker(&m_owner.m_diskWriteMutex);
        m_owner.m_tombstones.blockComponent(normalizedId);
        const QString dirPath = m_owner.componentCacheDir(normalizedId);
        if (dirPath.isEmpty())
            return;
        const QStringList files = CacheSafety::ownedComponentFiles(m_owner.cacheDir(), dirPath);
        bool allFilesMoved = true;
        for (const QString& filePath : files) {
            QString error;
            if (!CacheSafety::moveToTrash(filePath, &error, m_trash)) {
                allFilesMoved = false;
                emit m_owner.cacheMaintenanceWarning(error);
            }
        }
        if (allFilesMoved && !files.isEmpty() &&
            QDir(dirPath).entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot).isEmpty()) {
            QString error;
            if (!CacheSafety::moveToTrash(dirPath, &error, m_trash))
                emit m_owner.cacheMaintenanceWarning(error);
        }
        if (!files.isEmpty())
            LOG_DEBUG(LogModule::Core, "Moved owned disk cache files to trash for: {}", normalizedId);
    }

    qint64 sizeAfterUpdate = 0;
    {
        QMutexLocker locker(&m_owner.m_mutex);
        sizeAfterUpdate = m_owner.m_memoryCache.removeComponent(normalizedId);
    }
    emit m_owner.memoryCacheSizeChanged(sizeAfterUpdate);
    emit m_owner.cacheSizeChanged(cacheSize(m_owner));
}

/**
 * @brief 清空一级和二级缓存。
 * @details 全局 tombstone 在磁盘清理期间保持有效，直到调用方明确解除，防止旧任务写回。
 */
void ComponentCacheMaintenance::clearAll() {
    m_owner.m_cacheGeneration.fetch_add(1);
    {
        QMutexLocker diskLocker(&m_owner.m_diskWriteMutex);
        m_owner.m_tombstones.blockAll();
        for (const QString& componentDir : CacheSafety::ownedComponentDirectories(m_owner.cacheDir())) {
            const QStringList files = CacheSafety::ownedComponentFiles(m_owner.cacheDir(), componentDir);
            bool allFilesMoved = true;
            for (const QString& filePath : files) {
                QString error;
                if (!CacheSafety::moveToTrash(filePath, &error, m_trash)) {
                    allFilesMoved = false;
                    emit m_owner.cacheMaintenanceWarning(error);
                }
            }
            if (allFilesMoved && !files.isEmpty() &&
                QDir(componentDir).entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot).isEmpty()) {
                QString error;
                if (!CacheSafety::moveToTrash(componentDir, &error, m_trash))
                    emit m_owner.cacheMaintenanceWarning(error);
            }
        }
        for (const QString& entry : CacheSafety::ownedModel3DFiles(m_owner.cacheDir())) {
            QString error;
            if (!CacheSafety::moveToTrash(entry, &error, m_trash))
                emit m_owner.cacheMaintenanceWarning(error);
        }
        LOG_DEBUG(LogModule::Core, "Moved owned disk cache entries to trash");
    }

    m_owner.m_memoryCache.clear();
    emit m_owner.memoryCacheSizeChanged(0);
    emit m_owner.cacheSizeChanged(0);
}

/** @brief 清空一级内存缓存并递增缓存代次。 */
void ComponentCacheMaintenance::clearMemory() {
    m_owner.m_cacheGeneration.fetch_add(1);
    m_owner.m_tombstones.reset();
    m_owner.m_memoryCache.clear();
    LOG_DEBUG(LogModule::Core, "Cleared memory cache");
    emit m_owner.memoryCacheSizeChanged(0);
}

/**
 * @brief 枚举有效的元器件缓存目录。
 * @details 目录枚举与元数据读取共享磁盘锁，并过滤 model3d 和损坏或身份不匹配的目录。
 */
QStringList ComponentCacheMaintenance::cachedComponentIds(const ComponentCacheService& owner) {
    QMutexLocker diskLocker(&owner.m_diskWriteMutex);
    QStringList result;
    QSet<QString> seenIds;
    const QDir dir(owner.cacheDir());
    if (!dir.exists())
        return result;

    for (const QString& entry : dir.entryList(QDir::Dirs)) {
        if (entry == QStringLiteral(".") || entry == QStringLiteral("..") || entry == QStringLiteral("model3d") ||
            entry == CacheSafety::ownershipMarkerName())
            continue;
        const QString normalizedId = entry.toUpper();
        const QJsonObject metadata = CacheMetadataStore::read(owner.metadataPath(entry));
        const QJsonValue metadataId = metadata.value(QStringLiteral("lcscId"));
        const bool matchesEntry =
            metadataId.isString() && metadataId.toString().compare(entry, Qt::CaseInsensitive) == 0;
        if (CacheSafety::isOwnedComponentDirectory(owner.cacheDir(), dir.filePath(entry)) && matchesEntry &&
            CacheMetadataStore::hasValidModel3D(metadata) && !seenIds.contains(normalizedId)) {
            result.append(normalizedId);
            seenIds.insert(normalizedId);
        }
    }
    return result;
}

/** @brief 在磁盘锁保护下递归统计缓存目录大小。 */
qint64 ComponentCacheMaintenance::cacheSize(const ComponentCacheService& owner) {
    QMutexLocker diskLocker(&owner.m_diskWriteMutex);
    qint64 size = 0;
    for (const QString& componentDir : CacheSafety::ownedComponentDirectories(owner.cacheDir())) {
        for (const QString& filePath : CacheSafety::ownedComponentFiles(owner.cacheDir(), componentDir))
            size += QFileInfo(filePath).size();
    }
    for (const QString& filePath : CacheSafety::ownedModel3DFiles(owner.cacheDir()))
        size += QFileInfo(filePath).size();
    return size;
}

}  // namespace EasyKiConverter
