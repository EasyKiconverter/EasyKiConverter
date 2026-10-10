#include "ComponentDataMemoryStore.h"

#include <QMutexLocker>

namespace EasyKiConverter {

/**
 * @brief 在线程安全地判断组件数据是否已缓存。
 * @param normalizedId 已规范化的组件编号。
 * @return 缓存存在时返回 true。
 */
bool ComponentDataMemoryStore::contains(const QString& normalizedId) const {
    QMutexLocker locker(&m_mutex);  // 保护缓存查询。
    return m_data.contains(normalizedId);
}

/**
 * @brief 返回组件数据副本，避免暴露容器内部引用。
 * @param normalizedId 已规范化的组件编号。
 * @return 组件数据副本。
 */
ComponentData ComponentDataMemoryStore::value(const QString& normalizedId) const {
    QMutexLocker locker(&m_mutex);
    return m_data.value(normalizedId, ComponentData());
}

/**
 * @brief 写入或覆盖指定组件的数据。
 * @param normalizedId 已规范化的组件编号。
 * @param data 组件数据。
 */
void ComponentDataMemoryStore::set(const QString& normalizedId, const ComponentData& data) {
    QMutexLocker locker(&m_mutex);
    m_data[normalizedId] = data;
}

/**
 * @brief 在缓存项存在时原子替换其数据。
 * @param normalizedId 已规范化的组件编号。
 * @param data 新组件数据。
 * @return 缓存项存在并完成替换时返回 true。
 */
bool ComponentDataMemoryStore::replaceIfPresent(const QString& normalizedId, const ComponentData& data) {
    QMutexLocker locker(&m_mutex);
    auto it = m_data.find(normalizedId);  // 当前缓存项迭代器。
    if (it == m_data.end()) {
        return false;
    }
    it.value() = data;
    return true;
}

/**
 * @brief 更新符号和封装中的描述字段。
 * @param normalizedId 已规范化的组件编号。
 * @param description 新描述。
 * @return 缓存项存在时返回 true。
 */
bool ComponentDataMemoryStore::updateDescription(const QString& normalizedId, const QString& description) {
    QMutexLocker locker(&m_mutex);
    auto it = m_data.find(normalizedId);  // 当前缓存项迭代器。
    if (it == m_data.end()) {
        return false;
    }

    if (it->symbolData()) {
        SymbolInfo info = it->symbolData()->info();  // 当前符号信息。
        info.description = description;
        it->symbolData()->setInfo(info);
    }
    if (it->footprintData()) {
        FootprintInfo info = it->footprintData()->info();  // 当前封装信息。
        info.description = description;
        it->footprintData()->setInfo(info);
    }
    return true;
}

/**
 * @brief 清空组件服务的全部内存缓存。
 */
void ComponentDataMemoryStore::clear() {
    QMutexLocker locker(&m_mutex);
    m_data.clear();
}

/**
 * @brief 只删除无效组件缓存，保留仍可复用的有效数据。
 * @param normalizedId 已规范化的组件编号。
 */
void ComponentDataMemoryStore::removeIfInvalid(const QString& normalizedId) {
    QMutexLocker locker(&m_mutex);
    const auto it = m_data.find(normalizedId);  // 当前缓存项迭代器。
    if (it != m_data.end() && !it.value().isValid()) {
        m_data.erase(it);
    }
}

}  // namespace EasyKiConverter
