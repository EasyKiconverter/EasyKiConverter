#pragma once

#include <QSharedPointer>
#include <QString>

namespace EasyKiConverter {

class ComponentCacheService;
class ComponentData;

/**
 * @brief 协调元器件缓存的完整性校验和磁盘读取。
 *
 * 该协调器只负责读取路径的锁边界、元数据校验和 CAD 数据检查；一级缓存、
 * 写入策略、目录迁移和公开信号仍由 ComponentCacheService 负责。
 */
class ComponentCacheReadCoordinator final {
public:
    /**
     * @brief 判断指定元器件是否具有可用的完整磁盘缓存。
     * @param owner 所属缓存服务。
     * @param componentId 元器件编号。
     * @return 缓存完整且可用时返回 true。
     */
    static bool isCacheValid(const ComponentCacheService& owner, const QString& componentId);

    /**
     * @brief 从磁盘缓存读取完整元器件数据。
     * @param owner 所属缓存服务。
     * @param componentId 元器件编号。
     * @return 读取成功时返回元器件数据，否则返回空指针。
     */
    static QSharedPointer<ComponentData> loadComponentData(const ComponentCacheService& owner,
                                                           const QString& componentId);

    /**
     * @brief 检查符号、封装和 CAD 原始数据缓存是否可用。
     * @param owner 所属缓存服务。
     * @param componentId 元器件编号。
     * @return CAD 缓存有效时返回 true。
     */
    static bool hasSymbolFootprintCache(const ComponentCacheService& owner, const QString& componentId);
};

}  // namespace EasyKiConverter
