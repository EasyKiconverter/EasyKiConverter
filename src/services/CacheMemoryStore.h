#ifndef CACHEMEMORYSTORE_H
#define CACHEMEMORYSTORE_H

#include <QByteArray>
#include <QCache>
#include <QMutex>
#include <QString>
#include <QStringList>

namespace EasyKiConverter {

/**
 * @brief 提供线程安全的 L1 内存缓存存储。
 *
 * 该类封装 QCache 的所有权、LRU 淘汰和成本统计，调用方只需要处理缓存键及数据校验。
 */
class CacheMemoryStore final {
public:
    /**
     * @brief 创建带有指定容量的内存缓存。
     * @param maxCost 最大成本，通常以字节数表示。
     */
    explicit CacheMemoryStore(int maxCost);

    /** @brief 清空全部内存缓存。 */
    void clear();

    /**
     * @brief 判断缓存是否包含指定键。
     * @param key 缓存键。
     */
    bool contains(const QString& key) const;

    /**
     * @brief 读取指定键的数据副本。
     * @param key 缓存键。
     * @return 缓存数据，未命中时为空数组。
     */
    QByteArray value(const QString& key) const;

    /**
     * @brief 写入指定键的数据。
     * @param key 缓存键。
     * @param data 缓存数据。
     * @return 写入后的当前总成本。
     */
    qint64 insert(const QString& key, const QByteArray& data);

    /**
     * @brief 删除指定键集合。
     * @param keys 待删除的缓存键。
     * @return 删除后的当前总成本。
     */
    qint64 remove(const QStringList& keys);

    /** @brief 返回当前缓存总成本。 */
    qint64 totalCost() const;

    /**
     * @brief 设置缓存最大成本。
     * @param maxCost 最大成本。
     */
    void setMaxCost(int maxCost);

    /** @brief 返回缓存最大成本。 */
    int maxCost() const;

private:
    mutable QMutex m_mutex;  ///< 保护内存缓存读写的互斥锁。
    QCache<QString, QByteArray> m_cache;  ///< 按缓存键管理数据并负责 LRU 淘汰的 Qt 缓存。
};

}  // namespace EasyKiConverter

#endif  // CACHEMEMORYSTORE_H
