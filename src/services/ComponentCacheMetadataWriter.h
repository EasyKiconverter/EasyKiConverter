#ifndef COMPONENTCACHEMETADATAWRITER_H
#define COMPONENTCACHEMETADATAWRITER_H

#include <QString>

#include <cstdint>
#include <optional>

namespace EasyKiConverter {

class ComponentData;
class ComponentCacheService;

/**
 * @brief 协调元器件元数据的并发合并与双层缓存写入
 * @details 将 generation、tombstone、磁盘快照合并和 L1/L2 提交保持在同一写入边界内。
 */
class ComponentCacheMetadataWriter final {
public:
    /**
     * @brief 创建绑定到缓存服务的元数据写入协调器
     * @param owner 缓存服务
     */
    explicit ComponentCacheMetadataWriter(ComponentCacheService& owner);

    /**
     * @brief 合并并提交一份元器件元数据
     * @param componentId 元器件编号
     * @param data 元器件数据
     * @param expectedGeneration 调用方捕获的缓存代次，零表示不校验代次
     * @param replaceModel3DMetadata 是否在缺少模型时清理旧模型字段
     * @return 成功提交时返回 L1 缓存大小；写入已因失效策略丢弃时返回空值
     */
    std::optional<qint64> write(const QString& componentId,
                                const ComponentData& data,
                                uint64_t expectedGeneration,
                                bool replaceModel3DMetadata);

private:
    ComponentCacheService& m_owner;  ///< 所属缓存服务。
};

}  // namespace EasyKiConverter

#endif  // COMPONENTCACHEMETADATAWRITER_H
