#include "ComponentListItemUpdateBuffer.h"

namespace EasyKiConverter {

/**
 * @brief 在锁内去重保存需要刷新的列表项。
 * @param item 待刷新的列表项。
 */
void ComponentListItemUpdateBuffer::add(ComponentListItemData* item) {
    if (!item) {
        return;
    }

    QMutexLocker locker(&m_mutex);
    const QPointer<ComponentListItemData> guardedItem(item);
    if (!m_items.contains(guardedItem)) {
        m_items.append(guardedItem);
    }
}

/**
 * @brief 在同一锁区间内转移列表项，避免清空时丢失异步回调。
 * @return 待刷新的列表项集合。
 */
QList<QPointer<ComponentListItemData>> ComponentListItemUpdateBuffer::take() {
    QMutexLocker locker(&m_mutex);
    // 转移待刷新的列表项集合。
    QList<QPointer<ComponentListItemData>> items = std::move(m_items);
    return items;
}

/**
 * @brief 在锁内清空全部待刷新列表项。
 */
void ComponentListItemUpdateBuffer::clear() {
    QMutexLocker locker(&m_mutex);
    m_items.clear();
}

}  // namespace EasyKiConverter
