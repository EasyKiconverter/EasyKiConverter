#include "ComponentCacheMetadataWriter.h"

#include "CacheMetadataStore.h"
#include "ComponentCacheService.h"
#include "ComponentCacheWritePolicy.h"
#include "utils/logging/LogMacros.h"

#include <QMutexLocker>

namespace EasyKiConverter {

/**
 * @brief 创建绑定到缓存服务的元数据写入器。
 * @param owner 所属缓存服务。
 */
ComponentCacheMetadataWriter::ComponentCacheMetadataWriter(ComponentCacheService& owner) : m_owner(owner) {}

/**
 * @brief 在统一写锁内完成元数据快照合并、内存更新和磁盘持久化。
 * @param componentId 元器件编号。
 * @param data 元器件数据。
 * @param expectedGeneration 调用方捕获的缓存代次。
 * @param replaceModel3DMetadata 是否清理已移除的三维模型字段。
 * @return 提交后的一级缓存大小；写入被丢弃时返回空值。
 */
std::optional<qint64> ComponentCacheMetadataWriter::write(const QString& componentId,
                                                          const ComponentData& data,
                                                          uint64_t expectedGeneration,
                                                          bool replaceModel3DMetadata) {
    QJsonObject metadata = CacheMetadataStore::build(componentId, data);  // 根据当前数据构建的元数据。
    qint64 sizeAfterUpdate = 0;  // 提交后的一级缓存大小。

    // 旧快照读取、代次校验、合并和双层提交必须保持原子顺序，避免并发更新互相覆盖。
    {
        QMutexLocker diskLocker(&m_owner.m_diskWriteMutex);  // 保护磁盘快照与内存更新的一致性。
        if (!ComponentCacheWritePolicy::isAllowed(
                m_owner.m_cacheGeneration.load(), expectedGeneration, m_owner.m_tombstones, componentId)) {
            LOG_DEBUG(LogModule::Core, "Discarded stale metadata write for {}", componentId);
            return std::nullopt;
        }

        const QJsonObject existingMetadata =  // 磁盘中已有的元数据快照。
            CacheMetadataStore::read(m_owner.metadataPath(componentId));
        metadata = CacheMetadataStore::merge(existingMetadata, metadata);
        if (replaceModel3DMetadata && !CacheMetadataStore::hasModel3D(data)) {
            metadata.remove(QStringLiteral("model3duuid"));
            metadata.remove(QStringLiteral("model3dName"));
            metadata.remove(QStringLiteral("model3dTranslation"));
            metadata.remove(QStringLiteral("model3dRotation"));
        }

        sizeAfterUpdate = m_owner.m_memoryCache.saveMetadata(componentId, metadata);
        m_owner.saveMetadata(componentId, metadata);
    }

    return sizeAfterUpdate;
}

}  // namespace EasyKiConverter
