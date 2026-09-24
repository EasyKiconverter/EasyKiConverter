#include "CacheDirectoryCoordinator.h"

#include "CacheDirectoryMigrator.h"
#include "CacheSafety.h"
#include "ComponentCacheService.h"
#include "utils/logging/LogMacros.h"

#include <QDir>
#include <QMutexLocker>

namespace EasyKiConverter {

/** @brief 保存缓存服务引用。 */
CacheDirectoryCoordinator::CacheDirectoryCoordinator(ComponentCacheService& owner) : m_owner(owner) {}

/**
 * @brief 在统一锁边界内完成缓存目录切换。
 * @details 切换目录会使旧代次写入失效，并清空没有目录归属信息的一级缓存。
 */
bool CacheDirectoryCoordinator::setDirectory(const QString& cacheDir, bool migrateExistingCache) {
    QString newCacheDir;
    QString validationError;
    if (!CacheSafety::validateSelection(cacheDir, &newCacheDir, &validationError) ||
        !CacheSafety::ensureOwnedRoot(newCacheDir, &validationError)) {
        emit m_owner.cacheMaintenanceWarning(validationError);
        return false;
    }
    QString oldCacheDir;
    {
        QMutexLocker locker(&m_owner.m_cacheDirMutex);
        oldCacheDir = m_owner.m_cacheDir;
    }

    const bool cacheDirChanged = oldCacheDir != newCacheDir;
    {
        QMutexLocker diskLocker(&m_owner.m_diskWriteMutex);
        if (cacheDirChanged) {
            // 先使切换前排队的异步写入失效，再进行目录迁移，避免旧请求污染新目录。
            m_owner.m_cacheGeneration.fetch_add(1);
            m_owner.m_tombstones.reset();
        }

        // 迁移期间持有磁盘写锁，避免异步写入与目录迁移交错。
        if (migrateExistingCache && !oldCacheDir.isEmpty() && cacheDirChanged) {
            QString model3dError;
            if (!CacheSafety::ensureOwnedModel3DDirectory(newCacheDir, &model3dError)) {
                emit m_owner.cacheMaintenanceWarning(model3dError);
                return false;
            }
            if (!CacheDirectoryMigrator::migrate(oldCacheDir, newCacheDir)) {
                const QString error = QStringLiteral("缓存目录迁移失败，源目录已保留：%1").arg(oldCacheDir);
                emit m_owner.cacheMaintenanceWarning(error);
                return false;
            }
        }

        // 目录创建也在锁内，确保切换期间读写使用完整的目录结构。
        QDir dir;
        if (!dir.exists(newCacheDir)) {
            dir.mkpath(newCacheDir);
        }
        QString model3dError;
        if (!CacheSafety::ensureOwnedModel3DDirectory(newCacheDir, &model3dError)) {
            emit m_owner.cacheMaintenanceWarning(model3dError);
            return false;
        }

        // 原子切换缓存目录指针。
        {
            QMutexLocker locker(&m_owner.m_cacheDirMutex);
            m_owner.m_cacheDir = newCacheDir;
        }
        if (cacheDirChanged) {
            // L1 数据没有目录归属信息，切换 L2 目录后必须全部失效，避免
            // 同一元件 ID 从旧目录泄漏到新目录。
            m_owner.m_memoryCache.clear();
        }
    }
    if (cacheDirChanged) {
        emit m_owner.memoryCacheSizeChanged(0);
    }

    m_owner.selfHealCache();
    LOG_DEBUG(LogModule::Core, "Cache directory set to: {}", newCacheDir);
    return true;
}

}  // namespace EasyKiConverter
