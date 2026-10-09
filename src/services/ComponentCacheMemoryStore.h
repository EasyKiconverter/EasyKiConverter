#pragma once

#include "CacheMemoryStore.h"

#include <QByteArray>
#include <QJsonObject>
#include <QString>

#include <optional>

namespace EasyKiConverter {

/**
 * @brief 管理元器件缓存的一级内存数据。
 * @details 负责复合键、元数据 JSON 编解码和 CAD 数据校验，不负责信号、磁盘锁或二级缓存写入。
 */
class ComponentCacheMemoryStore final {
public:
    /**
     * @brief 创建带有指定容量的元器件内存缓存。
     * @param maxCost 最大缓存成本。
     */
    explicit ComponentCacheMemoryStore(int maxCost);

    /**
     * @brief 判断指定元器件是否存在元数据缓存。
     * @param componentId 元器件编号。
     * @return 存在元数据时返回 true。
     */
    bool containsMetadata(const QString& componentId) const;

    /**
     * @brief 读取一级缓存中的元数据。
     * @param componentId 元器件编号。
     * @return 元数据对象。
     */
    QJsonObject loadMetadata(const QString& componentId) const;

    /**
     * @brief 写入一级缓存中的元数据并返回当前成本。
     * @param componentId 元器件编号。
     * @param metadata 元数据对象。
     * @return 写入后的缓存成本。
     */
    qint64 saveMetadata(const QString& componentId, const QJsonObject& metadata);

    /**
     * @brief 读取一级缓存中的符号数据。
     * @param componentId 元器件编号。
     * @return 符号数据。
     */
    QByteArray loadSymbol(const QString& componentId) const;

    /**
     * @brief 校验并写入一级缓存中的符号数据。
     * @param componentId 元器件编号。
     * @param data 符号 CAD 数据。
     * @return 写入后的缓存成本；校验失败时返回空值。
     */
    std::optional<qint64> saveSymbol(const QString& componentId, const QByteArray& data);

    /**
     * @brief 读取一级缓存中的封装数据。
     * @param componentId 元器件编号。
     * @return 封装数据。
     */
    QByteArray loadFootprint(const QString& componentId) const;

    /**
     * @brief 校验并写入一级缓存中的封装数据。
     * @param componentId 元器件编号。
     * @param data 封装 CAD 数据。
     * @return 写入后的缓存成本；校验失败时返回空值。
     */
    std::optional<qint64> saveFootprint(const QString& componentId, const QByteArray& data);

    /**
     * @brief 删除指定元器件的一级缓存并返回当前成本。
     * @param componentId 元器件编号。
     * @return 删除后的缓存成本。
     */
    qint64 removeComponent(const QString& componentId);

    /** @brief 清空全部一级缓存。 */
    void clear();

    /** @brief 返回当前一级缓存成本。 */
    qint64 totalCost() const;

    /**
     * @brief 设置一级缓存最大成本。
     * @param maxCost 最大缓存成本。
     */
    void setMaxCost(int maxCost);

    /** @brief 返回一级缓存最大成本。 */
    int maxCost() const;

private:
    /**
     * @brief 生成统一的一级缓存复合键。
     * @param componentId 元器件编号。
     * @param type 缓存数据类型。
     * @return 规范化后的复合键。
     */
    static QString makeKey(const QString& componentId, const QString& type);

    CacheMemoryStore m_store;  ///< 一级缓存实际存储器。
};

}  // namespace EasyKiConverter
