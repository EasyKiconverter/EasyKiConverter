#include "CacheDirectoryCoordinator.h"

#include "CacheDirectoryMigrator.h"
#include "CacheSafety.h"
#include "ComponentCacheService.h"
#include "utils/logging/LogMacros.h"

#include <QDir>
#include <QMutexLocker>

namespace EasyKiConverter {

/**
 * @brief 保存缓存服务引用。
 * @param owner 所属缓存服务。
 */
CacheDirectoryCoordinator::CacheDirectoryCoordinator(ComponentCacheService& owner) : m_owner(owner) {}

/**
 * @brief 在统一锁边界内完成缓存目录切换。
 * @details 切换目录会使旧代次写入失效，并清空没有目录归属信息的一级缓存。
 * @param cacheDir 新缓存根目录。
 * @param migrateExistingCache 是否迁移旧目录内容。
 * @param error 可选的错误信息输出指针。
 * @return 切换成功时返回 true。
 */
bool CacheDirectoryCoordinator::setDirectory(const QString& cacheDir, bool migrateExistingCache, QString* error) {
    const auto fail = [error](const QString& reason) {
        if (error)
            *error = reason;
        return false;
    };
    QString newCacheDir;  // 通过安全校验后的新缓存根目录。
    QString validationError;  // 目录选择或所有权校验的具体错误。
    if (!CacheSafety::validateSelection(cacheDir, &newCacheDir, &validationError) ||
        !CacheSafety::ensureOwnedRoot(newCacheDir, &validationError)) {
        emit m_owner.cacheMaintenanceWarning(validationError);
        return fail(validationError);
    }
    QString oldCacheDir;  // 当前生效的旧缓存根目录。
    {
        QMutexLocker locker(&m_owner.m_cacheDirMutex);
        oldCacheDir = m_owner.m_cacheDir;
    }

    const bool cacheDirChanged = oldCacheDir != newCacheDir;  // 是否需要执行目录切换和一级缓存失效。
    {
        QMutexLocker diskLocker(&m_owner.m_diskWriteMutex);
        if (cacheDirChanged) {
            // 先使切换前排队的异步写入失效，再进行目录迁移，避免旧请求污染新目录。
            m_owner.m_cacheGeneration.fetch_add(1);
            m_owner.m_tombstones.reset();
        }

        // 迁移期间持有磁盘写锁，避免异步写入与目录迁移交错。
        if (migrateExistingCache && !oldCacheDir.isEmpty() && cacheDirChanged) {
            QString model3dError;  // 新目录三维模型子目录所有权错误。
            if (!CacheSafety::ensureOwnedModel3DDirectory(newCacheDir, &model3dError)) {
                emit m_owner.cacheMaintenanceWarning(model3dError);
                return fail(model3dError);
            }
            QString migrationError;  // 目录迁移失败的具体原因。
            if (!CacheDirectoryMigrator::migrate(oldCacheDir, newCacheDir, &migrationError)) {
                // 优先保留迁移器提供的具体错误，避免覆盖可操作的失败原因。
                const QString message = migrationError.isEmpty()
                                            ? QStringLiteral("缓存目录迁移失败，源目录已保留：%1").arg(oldCacheDir)
                                            : migrationError;
                emit m_owner.cacheMaintenanceWarning(message);
                return fail(message);
            }
        }

        // 目录创建也在锁内，确保切换期间读写使用完整的目录结构。
        QDir dir;
        if (!dir.exists(newCacheDir)) {
            dir.mkpath(newCacheDir);
        }
        QString model3dError;  // 新目录三维模型子目录所有权错误。
        if (!CacheSafety::ensureOwnedModel3DDirectory(newCacheDir, &model3dError)) {
            emit m_owner.cacheMaintenanceWarning(model3dError);
            return fail(model3dError);
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
