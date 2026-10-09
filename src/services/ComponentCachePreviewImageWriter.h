#ifndef COMPONENTCACHEPREVIEWIMAGEWRITER_H
#define COMPONENTCACHEPREVIEWIMAGEWRITER_H

#include <QByteArray>
#include <QString>

#include <cstdint>

namespace EasyKiConverter {

class ComponentCacheService;

/**
 * @brief 协调预览图缓存文件的校验、写入和配额维护。
 *
 * 该类只负责二级缓存中的预览图写入，缓存目录、写入代次和 tombstone
 * 状态仍由 ComponentCacheService 统一管理。
 */
class ComponentCachePreviewImageWriter final {
public:
    /**
     * @brief 创建绑定到元器件缓存服务的预览图写入器。
     * @param owner 所属缓存服务。
     */
    explicit ComponentCachePreviewImageWriter(ComponentCacheService& owner);

    /**
     * @brief 写入指定元件的预览图。
     * @param componentId 元件编号。
     * @param imageData 已编码的图片数据。
     * @param imageIndex 预览图序号。
     * @param expectedGeneration 调用方捕获的缓存代次。
     */
    void write(const QString& componentId, const QByteArray& imageData, int imageIndex, uint64_t expectedGeneration);

private:
    /** @brief 保存协作者所属的缓存服务。 */
    ComponentCacheService& m_owner;
};

}  // namespace EasyKiConverter

#endif  // COMPONENTCACHEPREVIEWIMAGEWRITER_H
