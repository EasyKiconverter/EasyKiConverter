#ifndef COMPONENTCACHELOADWORKER_H
#define COMPONENTCACHELOADWORKER_H

#include "models/ComponentData.h"

#include <QByteArray>
#include <QList>
#include <QPair>
#include <QString>
#include <QStringList>

namespace EasyKiConverter {

class ComponentCacheService;
class FootprintData;
class SymbolData;

/**
 * @brief 表示后台缓存加载任务的结果。
 * @details 结果只携带后台读取和解析数据，不负责代次校验或 Qt 信号派发。
 */
struct ComponentCacheLoadResult {
    QString componentId;  ///< 元器件编号。
    bool success = false;  ///< 缓存读取和解析是否成功。
    QSharedPointer<ComponentData> cachedData;  ///< 解析后的元器件数据。
    QByteArray cadDataJson;  ///< CAD JSON 原始数据。
    QList<QPair<int, QByteArray>> previewImageData;  ///< 带索引的预览图数据。
    QStringList encodedPreviewImages;  ///< Base64 编码的预览图列表。
    QByteArray datasheetData;  ///< 数据手册数据。
    QSharedPointer<SymbolData> symbolData;  ///< 解析后的符号数据。
    QSharedPointer<FootprintData> footprintData;  ///< 解析后的封装数据。
};

/**
 * @brief 执行元件缓存的后台读取和解析。
 * @details 该类不持有服务生命周期，只通过传入的缓存服务读取数据。
 */
class ComponentCacheLoadWorker final {
public:
    /**
     * @brief 从缓存读取并预解析元件数据。
     * @param componentId 规范化后的元件编号
     * @param fetch3DModel 是否预加载 OBJ 模型
     * @param cache 缓存服务
     * @return 缓存加载结果
     */
    static ComponentCacheLoadResult load(const QString& componentId, bool fetch3DModel, ComponentCacheService* cache);

    /**
     * @brief 从封装数据补齐组件的三维模型元数据。
     * @param component 需要补齐的组件
     * @param footprint 已解析的封装数据
     */
    static void restoreModel3DFromFootprint(ComponentData& component, const QSharedPointer<FootprintData>& footprint);
};

}  // namespace EasyKiConverter

#endif  // COMPONENTCACHELOADWORKER_H
