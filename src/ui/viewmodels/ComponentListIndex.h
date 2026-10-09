#ifndef COMPONENTLISTINDEX_H
#define COMPONENTLISTINDEX_H

#include "models/ComponentListItemData.h"

#include <QHash>
#include <QList>
#include <QString>

namespace EasyKiConverter {

/**
 * @brief 管理元件列表编号到列表位置的索引。
 *
 * 该类不负责线程同步，调用方需要在保护元件列表的同一把锁内使用它，
 * 以确保索引和列表内容始终保持一致。
 */
class ComponentListIndex final {
public:
    /**
     * @brief 判断指定编号是否已建立索引。
     * @param componentId 元件编号。
     * @return 已建立索引时返回 true。
     */
    bool contains(const QString& componentId) const;

    /**
     * @brief 返回元件编号对应的列表位置。
     * @param componentId 元件编号。
     * @return 对应位置，未找到时返回 -1。
     */
    int indexOf(const QString& componentId) const;

    /**
     * @brief 写入元件编号对应的列表位置。
     * @param componentId 元件编号。
     * @param index 列表位置。
     */
    void insert(const QString& componentId, int index);

    /**
     * @brief 删除指定元件编号的索引。
     * @param componentId 元件编号。
     */
    void remove(const QString& componentId);

    /**
     * @brief 清空全部索引。
     */
    void clear();

    /**
     * @brief 根据当前元件列表重新建立索引。
     * @param items 当前元件列表。
     */
    void rebuild(const QList<ComponentListItemData*>& items);

    /**
     * @brief 返回当前索引条目数量。
     */
    int size() const;

private:
    QHash<QString, int> m_indices;  ///< 保存元器件编号到列表索引的映射。
};

}  // namespace EasyKiConverter

#endif  // COMPONENTLISTINDEX_H
