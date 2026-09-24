#pragma once

#include <QString>

namespace EasyKiConverter {

class ComponentCacheService;

/**
 * @brief 协调缓存根目录切换、迁移和切换后的自修复。
 *
 * 目录切换涉及磁盘写锁、缓存代次、tombstone 和一级缓存失效，统一放在
 * 一个协作者中可以避免公开缓存服务混入目录生命周期细节。
 */
class CacheDirectoryCoordinator final {
public:
    /** @brief 创建绑定到缓存服务的目录协调器。 */
    explicit CacheDirectoryCoordinator(ComponentCacheService& owner);

    /**
     * @brief 切换缓存根目录并按需迁移旧目录。
     * @param cacheDir 新缓存根目录
     * @param migrateExistingCache 是否迁移旧目录内容
     */
    bool setDirectory(const QString& cacheDir, bool migrateExistingCache);

private:
    /** @brief 保存缓存服务引用，不负责其生命周期。 */
    ComponentCacheService& m_owner;
};

}  // namespace EasyKiConverter
