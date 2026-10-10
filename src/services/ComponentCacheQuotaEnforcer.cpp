#include "ComponentCacheQuotaEnforcer.h"

#include "CachePruner.h"
#include "ComponentCacheService.h"

#include <QMutexLocker>

namespace EasyKiConverter {

/**
 * @brief 创建绑定到缓存服务的配额执行器。
 * @param owner 所属缓存服务。
 */
ComponentCacheQuotaEnforcer::ComponentCacheQuotaEnforcer(ComponentCacheService& owner) : m_owner(owner) {}

/**
 * @brief 在冷却策略允许时扫描并清理二级磁盘缓存。
 * @details 读取配置和重置计时器保持在同一锁区间内，实际目录扫描在锁外执行。
 * @param bypassCooldown 是否跳过冷却时间。
 */
void ComponentCacheQuotaEnforcer::enforce(bool bypassCooldown) {
    constexpr qint64 kCooldownMs = 3000;  // 两次自动配额清理之间的最小间隔。
    QString cacheDir;  // 当前缓存目录。
    qint64 targetSizeBytes = 0;  // 目标缓存大小，单位为字节。
    {
        QMutexLocker locker(&m_owner.m_mutex);  // 保护冷却计时器和缓存配置读取。
        if (!bypassCooldown && m_owner.m_lastEnforceTimer.elapsed() < kCooldownMs) {
            return;
        }
        cacheDir = m_owner.cacheDir();
        targetSizeBytes = static_cast<qint64>(m_owner.m_diskCacheLimitMB) * 1024 * 1024;
        m_owner.m_lastEnforceTimer.restart();
    }

    if (targetSizeBytes <= 0 || cacheDir.isEmpty()) {
        return;
    }

    CachePruner pruner(cacheDir);  // 当前缓存目录的配额清理器。
    const qint64 newSize = pruner.pruneTo(targetSizeBytes);  // 清理后的缓存大小。
    emit m_owner.cacheSizeChanged(newSize);
}

}  // namespace EasyKiConverter
