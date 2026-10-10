#include "CacheMemoryStore.h"

#include <QMutexLocker>

namespace EasyKiConverter {

/**
 * @brief 初始化 QCache 的最大成本，成本由调用方按数据字节数传入。
 * @param maxCost 内存缓存最大成本。
 */
CacheMemoryStore::CacheMemoryStore(int maxCost) {
    m_cache.setMaxCost(maxCost);
}

/**
 * @brief 清空 LRU 缓存并释放其拥有的数据。
 */
void CacheMemoryStore::clear() {
    QMutexLocker locker(&m_mutex);
    m_cache.clear();
}

/**
 * @brief 在线程安全地读取键是否存在。
 * @param key 缓存键。
 * @return 键存在时返回 true。
 */
bool CacheMemoryStore::contains(const QString& key) const {
    QMutexLocker locker(&m_mutex);
    return m_cache.contains(key);
}

/**
 * @brief 返回缓存数据副本，避免把 QCache 内部指针暴露给调用方。
 * @param key 缓存键。
 * @return 缓存数据副本，未命中时返回空数组。
 */
QByteArray CacheMemoryStore::value(const QString& key) const {
    QMutexLocker locker(&m_mutex);
    const QByteArray* data = m_cache.object(key);  // QCache 管理的缓存数据指针。
    return data ? *data : QByteArray();
}

/**
 * @brief 写入数据并返回写入后的成本统计。
 * @param key 缓存键。
 * @param data 待缓存的数据。
 * @return 写入后的缓存总成本。
 */
qint64 CacheMemoryStore::insert(const QString& key, const QByteArray& data) {
    QMutexLocker locker(&m_mutex);
    m_cache.insert(key, new QByteArray(data), data.size());
    return m_cache.totalCost();
}

/**
 * @brief 删除键集合并返回删除后的成本统计。
 * @param keys 待删除的缓存键列表。
 * @return 删除后的缓存总成本。
 */
qint64 CacheMemoryStore::remove(const QStringList& keys) {
    QMutexLocker locker(&m_mutex);
    for (const QString& key : keys) {
        delete m_cache.take(key);
    }
    return m_cache.totalCost();
}

/**
 * @brief 读取当前缓存占用成本。
 * @return 当前缓存总成本。
 */
qint64 CacheMemoryStore::totalCost() const {
    QMutexLocker locker(&m_mutex);
    return m_cache.totalCost();
}

/**
 * @brief 更新最大成本，QCache 会自动淘汰超出限制的旧数据。
 * @param maxCost 新的最大成本。
 */
void CacheMemoryStore::setMaxCost(int maxCost) {
    QMutexLocker locker(&m_mutex);
    m_cache.setMaxCost(maxCost);
}

/**
 * @brief 返回当前最大成本限制。
 * @return 当前最大成本。
 */
int CacheMemoryStore::maxCost() const {
    QMutexLocker locker(&m_mutex);
    return m_cache.maxCost();
}

}  // namespace EasyKiConverter
