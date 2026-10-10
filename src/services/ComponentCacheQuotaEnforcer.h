#ifndef COMPONENTCACHEQUOTAENFORCER_H
#define COMPONENTCACHEQUOTAENFORCER_H

namespace EasyKiConverter {

class ComponentCacheService;

/**
 * @brief 按冷却策略执行二级磁盘缓存配额清理。
 *
 * 该类只协调配额读取、扫描冷却和 CachePruner 调用，缓存服务仍负责
 * 保存配置、发出缓存大小变化信号以及管理缓存目录生命周期。
 */
class ComponentCacheQuotaEnforcer final {
public:
    /**
     * @brief 创建绑定到元器件缓存服务的配额执行器。
     * @param owner 所属缓存服务。
     */
    explicit ComponentCacheQuotaEnforcer(ComponentCacheService& owner);

    /**
     * @brief 按当前磁盘缓存限制执行清理。
     * @param bypassCooldown 是否跳过批量写入使用的冷却时间。
     */
    void enforce(bool bypassCooldown = false);

private:
    /**
     * @brief 保存协作者所属的缓存服务。
     */
    ComponentCacheService& m_owner;
};

}  // namespace EasyKiConverter

#endif  // COMPONENTCACHEQUOTAENFORCER_H
