#include "ComponentListIndex.h"

namespace EasyKiConverter {

/**
 * @brief 判断元件编号是否已经映射到列表位置。
 * @param componentId 元器件编号。
 */
bool ComponentListIndex::contains(const QString& componentId) const {
    return m_indices.contains(componentId);
}

/**
 * @brief 返回元件编号对应的位置，未找到时使用统一的无效索引值。
 * @param componentId 元器件编号。
 */
int ComponentListIndex::indexOf(const QString& componentId) const {
    return m_indices.value(componentId, -1);
}

/**
 * @brief 写入元件编号和列表位置的映射。
 * @param componentId 元器件编号。
 * @param index 列表索引。
 */
void ComponentListIndex::insert(const QString& componentId, int index) {
    m_indices.insert(componentId, index);
}

/**
 * @brief 删除指定元件编号的映射。
 * @param componentId 元器件编号。
 */
void ComponentListIndex::remove(const QString& componentId) {
    m_indices.remove(componentId);
}

/**
 * @brief 清空列表索引。
 */
void ComponentListIndex::clear() {
    m_indices.clear();
}

/**
 * @brief 按元件列表的当前顺序重建所有编号映射。
 * @param items 待建立索引的列表项集合。
 */
void ComponentListIndex::rebuild(const QList<ComponentListItemData*>& items) {
    m_indices.clear();
    for (int index = 0; index < items.count(); ++index) {
        // 读取当前索引位置对应的列表项。
        const ComponentListItemData* item = items.at(index);
        if (item) {
            m_indices.insert(item->componentId(), index);
        }
    }
}

/**
 * @brief 返回当前索引的条目数。
 */
int ComponentListIndex::size() const {
    return m_indices.size();
}

}  // namespace EasyKiConverter
