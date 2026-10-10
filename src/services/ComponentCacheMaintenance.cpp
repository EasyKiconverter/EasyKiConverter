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

/**
 * @brief 创建绑定到缓存服务的维护协调器。
 * @param owner 所属缓存服务。
 * @param trash 回收站移动策略。
 */
ComponentCacheMaintenance::ComponentCacheMaintenance(ComponentCacheService& owner,
                                                     const CacheSafety::TrashFunction& trash)
    : m_owner(owner), m_trash(trash) {}

/**
 * @brief 删除指定元器件的一级和二级缓存。
 * @param componentId 元器件编号。
 * @details 先递增代次并锁定 tombstone，再删除磁盘和内存内容，避免旧异步回调复活缓存。
 */
void ComponentCacheMaintenance::remove(const QString& componentId) {
    const QString normalizedId = componentId.toUpper();  // 规范化后的元器件编号。
    if (!BomParser::validateId(normalizedId)) {
        qWarning() << "removeCache: invalid lcscId, ignoring:" << componentId;
        return;
    }

    m_owner.m_cacheGeneration.fetch_add(1);
    {
        // 锁顺序保持为 disk 后 tombstone，与缓存写入策略一致。
        QMutexLocker diskLocker(&m_owner.m_diskWriteMutex);  // 保护磁盘缓存维护。
        m_owner.m_tombstones.blockComponent(normalizedId);
        const QString dirPath = m_owner.componentCacheDir(normalizedId);  // 组件缓存目录。
        if (dirPath.isEmpty())
            return;
        const QStringList files = CacheSafety::ownedComponentFiles(m_owner.cacheDir(), dirPath);  // 可处理文件。
        bool allFilesMoved = true;  // 是否所有已验证文件均已移入回收站。
        for (const QString& filePath : files) {
            QString error;  // 当前回收站操作的错误文本。
            if (!CacheSafety::moveToTrash(filePath, &error, m_trash)) {
                allFilesMoved = false;
                emit m_owner.cacheMaintenanceWarning(error);
            }
        }
        if (allFilesMoved && !files.isEmpty() &&
            QDir(dirPath).entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot).isEmpty()) {
            QString error;  // 组件目录回收站操作的错误文本。
            if (!CacheSafety::moveToTrash(dirPath, &error, m_trash))
                emit m_owner.cacheMaintenanceWarning(error);
        }
        if (!files.isEmpty())
            LOG_DEBUG(LogModule::Core, "Moved owned disk cache files to trash for: {}", normalizedId);
    }

    qint64 sizeAfterUpdate = 0;  // 删除后的一级缓存大小。
    {
        QMutexLocker locker(&m_owner.m_mutex);  // 保护一级缓存删除。
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
        QMutexLocker diskLocker(&m_owner.m_diskWriteMutex);  // 保护磁盘缓存清空。
        m_owner.m_tombstones.blockAll();
        for (const QString& componentDir : CacheSafety::ownedComponentDirectories(m_owner.cacheDir())) {
            const QStringList files =
                CacheSafety::ownedComponentFiles(m_owner.cacheDir(), componentDir);  // 可处理文件。
            bool allFilesMoved = true;  // 是否所有文件均已移入回收站。
            for (const QString& filePath : files) {
                QString error;  // 当前回收站操作的错误文本。
                if (!CacheSafety::moveToTrash(filePath, &error, m_trash)) {
                    allFilesMoved = false;
                    emit m_owner.cacheMaintenanceWarning(error);
                }
            }
            if (allFilesMoved && !files.isEmpty() &&
                QDir(componentDir).entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot).isEmpty()) {
                QString error;  // 组件目录回收站操作的错误文本。
                if (!CacheSafety::moveToTrash(componentDir, &error, m_trash))
                    emit m_owner.cacheMaintenanceWarning(error);
            }
        }
        for (const QString& entry : CacheSafety::ownedModel3DFiles(m_owner.cacheDir())) {
            QString error;  // 当前三维模型回收站操作的错误文本。
            if (!CacheSafety::moveToTrash(entry, &error, m_trash))
                emit m_owner.cacheMaintenanceWarning(error);
        }
        LOG_DEBUG(LogModule::Core, "Moved owned disk cache entries to trash");
    }

    m_owner.m_memoryCache.clear();
    emit m_owner.memoryCacheSizeChanged(0);
    emit m_owner.cacheSizeChanged(0);
}

/**
 * @brief 清空一级内存缓存并递增缓存代次。
 */
void ComponentCacheMaintenance::clearMemory() {
    m_owner.m_cacheGeneration.fetch_add(1);
    m_owner.m_tombstones.reset();
    m_owner.m_memoryCache.clear();
    LOG_DEBUG(LogModule::Core, "Cleared memory cache");
    emit m_owner.memoryCacheSizeChanged(0);
}

/**
 * @brief 枚举有效的元器件缓存目录。
 * @param owner 所属缓存服务。
 * @details 目录枚举与元数据读取共享磁盘锁，并过滤 model3d 和损坏或身份不匹配的目录。
 */
QStringList ComponentCacheMaintenance::cachedComponentIds(const ComponentCacheService& owner) {
    QMutexLocker diskLocker(&owner.m_diskWriteMutex);
    QStringList result;  // 有效元器件编号结果。
    QSet<QString> seenIds;  // 已加入结果的规范化编号。
    const QDir dir(owner.cacheDir());  // 缓存根目录。
    if (!dir.exists())
        return result;

    for (const QString& entry : dir.entryList(QDir::Dirs)) {
        if (entry == QStringLiteral(".") || entry == QStringLiteral("..") || entry == QStringLiteral("model3d") ||
            entry == CacheSafety::ownershipMarkerName())
            continue;
        const QString normalizedId = entry.toUpper();  // 规范化后的目录编号。
        const QJsonObject metadata = CacheMetadataStore::read(owner.metadataPath(entry));  // 组件元数据。
        const QJsonValue metadataId = metadata.value(QStringLiteral("lcscId"));  // 元数据中的编号。
        const bool matchesEntry =  // 元数据编号是否与目录名一致。
            metadataId.isString() && metadataId.toString().compare(entry, Qt::CaseInsensitive) == 0;
        if (CacheSafety::isOwnedComponentDirectory(owner.cacheDir(), dir.filePath(entry)) && matchesEntry &&
            CacheMetadataStore::hasValidModel3D(metadata) && !seenIds.contains(normalizedId)) {
            result.append(normalizedId);
            seenIds.insert(normalizedId);
        }
    }
    return result;
}

/**
 * @brief 在磁盘锁保护下递归统计缓存目录大小。
 * @param owner 所属缓存服务。
 * @return 已验证归属的缓存文件总大小。
 */
qint64 ComponentCacheMaintenance::cacheSize(const ComponentCacheService& owner) {
    QMutexLocker diskLocker(&owner.m_diskWriteMutex);  // 保护缓存目录统计。
    qint64 size = 0;  // 已验证文件的累计大小。
    for (const QString& componentDir : CacheSafety::ownedComponentDirectories(owner.cacheDir())) {
        for (const QString& filePath : CacheSafety::ownedComponentFiles(owner.cacheDir(), componentDir))
            size += QFileInfo(filePath).size();
    }
    for (const QString& filePath : CacheSafety::ownedModel3DFiles(owner.cacheDir()))
        size += QFileInfo(filePath).size();
    return size;
}

}  // namespace EasyKiConverter
