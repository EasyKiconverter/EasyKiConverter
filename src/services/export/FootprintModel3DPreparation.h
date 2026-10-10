#ifndef FOOTPRINTMODEL3DPREPARATION_H
#define FOOTPRINTMODEL3DPREPARATION_H

#include <QSharedPointer>
#include <QString>

#include <cstdint>

namespace EasyKiConverter {

class ComponentData;
class FootprintData;

namespace FootprintModel3DPreparation {

/**
 * @brief 准备封装导出所需的 STEP 模型和坐标偏移。
 * @param footprint 待更新的封装数据
 * @param componentData 已获取的元器件数据
 * @param componentId 元器件编号，用于日志和 CAD 缓存回退
 * @param generation 当前缓存代次
 */
void prepare(FootprintData& footprint,
             const QSharedPointer<ComponentData>& componentData,
             const QString& componentId,
             uint64_t generation);

}  // namespace FootprintModel3DPreparation
}  // namespace EasyKiConverter

#endif  // FOOTPRINTMODEL3DPREPARATION_H
