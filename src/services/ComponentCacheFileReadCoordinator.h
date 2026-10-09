#ifndef COMPONENTCACHEFILEREADCOORDINATOR_H
#define COMPONENTCACHEFILEREADCOORDINATOR_H

#include <QByteArray>
#include <QJsonObject>
#include <QString>

namespace EasyKiConverter {

class ComponentCacheService;

/**
 * @brief 协调缓存文件的加锁读取和文件格式校验。
 *
 * 该协调器负责二级缓存文件的统一读取入口，不改变 ComponentCacheService
 * 的公开接口，也不负责完整缓存判定、缓存写入或一级缓存淘汰。
 */
class ComponentCacheFileReadCoordinator final {
public:
    /**
     * @brief 读取元器件符号 CAD 数据。
     * @param owner 所属缓存服务。
     * @param componentId 元器件编号。
     * @return 符号缓存数据。
     */
    static QByteArray loadSymbolData(const ComponentCacheService& owner, const QString& componentId);

    /**
     * @brief 读取元器件封装 CAD 数据。
     * @param owner 所属缓存服务。
     * @param componentId 元器件编号。
     * @return 封装缓存数据。
     */
    static QByteArray loadFootprintData(const ComponentCacheService& owner, const QString& componentId);

    /**
     * @brief 读取元器件原始 CAD JSON 数据。
     * @param owner 所属缓存服务。
     * @param componentId 元器件编号。
     * @return CAD JSON 数据。
     */
    static QByteArray loadCadDataJson(const ComponentCacheService& owner, const QString& componentId);

    /**
     * @brief 读取指定索引的预览图数据。
     * @param owner 所属缓存服务。
     * @param componentId 元器件编号。
     * @param imageIndex 预览图索引。
     * @return 预览图数据。
     */
    static QByteArray loadPreviewImage(const ComponentCacheService& owner, const QString& componentId, int imageIndex);

    /**
     * @brief 读取元器件二级缓存元数据。
     * @param owner 所属缓存服务。
     * @param componentId 元器件编号。
     * @return 元数据对象。
     */
    static QJsonObject loadMetadata(const ComponentCacheService& owner, const QString& componentId);
};

}  // namespace EasyKiConverter

#endif  // COMPONENTCACHEFILEREADCOORDINATOR_H
