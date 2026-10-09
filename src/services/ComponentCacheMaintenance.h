#pragma once

#include "CacheSafety.h"

#include <QStringList>

namespace EasyKiConverter {

class ComponentCacheService;

/**
 * @brief 协调元器件缓存的删除、清理和容量查询。
 * @details 负责缓存代次、tombstone、磁盘锁、一级缓存和服务信号之间的顺序，
 *          不直接改变缓存文件格式。
 */
class ComponentCacheMaintenance final {
public:
    /**
     * @brief 创建绑定到指定缓存服务的维护协调器。
     * @param owner 所属缓存服务。
     * @param trash 回收站移动策略。
     */
    explicit ComponentCacheMaintenance(ComponentCacheService& owner,
                                       const CacheSafety::TrashFunction& trash = CacheSafety::TrashFunction());

    /**
     * @brief 删除指定元器件的一级和二级缓存。
     * @param componentId 元器件编号。
     */
    void remove(const QString& componentId);

    /** @brief 清空一级和二级缓存，并使旧异步写入失效。 */
    void clearAll();

    /** @brief 清空一级内存缓存并使旧异步写入失效。 */
    void clearMemory();

    /**
     * @brief 枚举当前缓存目录中具有有效元数据的元器件编号。
     * @param owner 所属缓存服务。
     * @return 有效元器件编号列表。
     */
    static QStringList cachedComponentIds(const ComponentCacheService& owner);

    /**
     * @brief 统计当前缓存目录的磁盘占用大小。
     * @param owner 所属缓存服务。
     * @return 已验证归属的缓存文件占用字节数。
     */
    static qint64 cacheSize(const ComponentCacheService& owner);

private:
    ComponentCacheService& m_owner;  ///< 所属缓存服务。
    CacheSafety::TrashFunction m_trash;  ///< 回收站移动策略。
};

}  // namespace EasyKiConverter
