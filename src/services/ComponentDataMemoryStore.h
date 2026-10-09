#ifndef COMPONENTDATAMEMORYSTORE_H
#define COMPONENTDATAMEMORYSTORE_H

#include "models/ComponentData.h"

#include <QMap>
#include <QMutex>
#include <QString>

namespace EasyKiConverter {

/**
 * @brief 管理组件服务使用的线程安全内存数据缓存。
 *
 * 该类只负责缓存容器和并发保护，不负责网络请求、缓存持久化或请求代次判断。
 */
class ComponentDataMemoryStore final {
public:
    /**
     * @brief 判断缓存中是否存在指定组件。
     * @param normalizedId 已规范化的组件编号。
     */
    bool contains(const QString& normalizedId) const;

    /**
     * @brief 读取组件数据副本。
     * @param normalizedId 已规范化的组件编号。
     * @return 缓存数据，未命中时返回默认对象。
     */
    ComponentData value(const QString& normalizedId) const;

    /**
     * @brief 写入或覆盖组件数据。
     * @param normalizedId 已规范化的组件编号。
     * @param data 组件数据。
     */
    void set(const QString& normalizedId, const ComponentData& data);

    /**
     * @brief 仅在已有缓存项时替换组件数据。
     * @param normalizedId 已规范化的组件编号。
     * @param data 新组件数据。
     * @return 缓存项存在并完成替换时返回 true。
     */
    bool replaceIfPresent(const QString& normalizedId, const ComponentData& data);

    /**
     * @brief 更新缓存组件的符号和封装描述。
     * @param normalizedId 已规范化的组件编号。
     * @param description 新描述。
     * @return 缓存项存在时返回 true。
     */
    bool updateDescription(const QString& normalizedId, const QString& description);

    /** @brief 清空全部组件数据缓存。 */
    void clear();

    /**
     * @brief 删除指定编号对应的无效缓存项。
     * @param normalizedId 已规范化的组件编号。
     */
    void removeIfInvalid(const QString& normalizedId);

private:
    mutable QMutex m_mutex;  ///< 保护组件数据缓存的互斥锁。
    QMap<QString, ComponentData> m_data;  ///< 按规范化编号存储的组件数据。
};

}  // namespace EasyKiConverter

#endif  // COMPONENTDATAMEMORYSTORE_H
