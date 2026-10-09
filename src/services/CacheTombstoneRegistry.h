#ifndef CACHETOMBSTONEREGISTRY_H
#define CACHETOMBSTONEREGISTRY_H

#include <QMutex>
#include <QSet>
#include <QString>

namespace EasyKiConverter {

/**
 * @brief 管理缓存删除操作对旧异步写入的屏蔽状态。
 *
 * 组件级 tombstone 用于屏蔽单个元器件的旧回调，全局 tombstone 用于屏蔽
 * 清空缓存前已经提交的全部旧回调。该类只负责线程安全的状态管理。
 */
class CacheTombstoneRegistry final {
public:
    /** @brief 清空所有屏蔽状态，准备切换到新的缓存上下文。 */
    void reset();

    /**
     * @brief 屏蔽指定元器件的旧异步写入。
     * @param componentId 元器件编号。
     */
    void blockComponent(const QString& componentId);

    /**
     * @brief 解除指定元器件的屏蔽状态。
     * @param componentId 元器件编号。
     */
    void clearComponent(const QString& componentId);

    /** @brief 屏蔽所有元器件的旧异步写入。 */
    void blockAll();

    /** @brief 仅解除全局屏蔽，保留组件级屏蔽状态。 */
    void clearGlobal();

    /**
     * @brief 判断元器件是否被屏蔽。
     * @param componentId 元器件编号。
     * @return 若全局或组件级屏蔽生效则返回 true。
     */
    bool isBlocked(const QString& componentId) const;

private:
    mutable QMutex m_mutex;  ///< 保护组件级和全局屏蔽状态的互斥锁。
    QSet<QString> m_components;  ///< 已被屏蔽的组件编号集合。
    bool m_allBlocked = false;  ///< 是否启用全局屏蔽。
};

}  // namespace EasyKiConverter

#endif  // CACHETOMBSTONEREGISTRY_H
