#ifndef CACHEMETADATASTORE_H
#define CACHEMETADATASTORE_H

#include "models/ComponentData.h"

#include <QByteArray>
#include <QJsonObject>
#include <QString>

namespace EasyKiConverter {

/**
 * @brief 负责元器件缓存元数据的读取、构建、合并和原子写入。
 *
 * 该类不管理缓存目录、不持有缓存服务锁，也不负责缓存生命周期；它只处理
 * 元数据在 ComponentData、JSON 和磁盘文件之间的转换。
 */
class CacheMetadataStore final {
public:
    /**
     * @brief 从磁盘读取元数据 JSON。
     * @param metadataPath 元数据文件路径。
     * @return 解析成功的对象，文件不存在或内容无效时返回空对象。
     */
    static QJsonObject read(const QString& metadataPath);

    /**
     * @brief 校验元数据中的三维模型字段。
     * @param metadata 待校验的元数据。
     * @return 字段完整且类型正确时返回 true。
     */
    static bool hasValidModel3D(const QJsonObject& metadata);

    /**
     * @brief 从元器件数据构建可持久化的元数据。
     * @param componentId 元器件编号。
     * @param data 元器件数据。
     * @return 可写入缓存的元数据对象。
     */
    static QJsonObject build(const QString& componentId, const ComponentData& data);

    /**
     * @brief 判断元器件是否关联可持久化的三维模型。
     * @param data 元器件数据。
     * @return 独立模型或封装模型包含有效 UUID 时返回 true。
     */
    static bool hasModel3D(const ComponentData& data);

    /**
     * @brief 合并新旧元数据并保留未被空值覆盖的字段。
     * @param existing 已有缓存元数据。
     * @param incoming 新产生的元数据。
     * @return 合并后的元数据。
     */
    static QJsonObject merge(const QJsonObject& existing, const QJsonObject& incoming);

    /**
     * @brief 使用临时文件原子写入元数据或其他缓存文件。
     * @param path 目标文件路径。
     * @param data 要写入的数据。
     * @return 写入并提交成功时返回 true。
     */
    static bool writeAtomically(const QString& path, const QByteArray& data);
};

}  // namespace EasyKiConverter

#endif  // CACHEMETADATASTORE_H
