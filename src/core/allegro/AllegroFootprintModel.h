#pragma once

/**
 * @file AllegroFootprintModel.h
 * @brief Allegro 符号、封装和三维语义 Import Package 的目标模型。
 */

#include "core/ir/FootprintIR.h"

#include <QJsonArray>
#include <QList>
#include <QPointF>
#include <QString>

namespace EasyKiConverter {

/** @brief Allegro Class/Subclass 的明确映射结果。 */
struct AllegroLayerAssignment {
    QString className;  ///< Allegro 对象类别名称。
    QString subclassName;  ///< Allegro 对象子类别名称。
    QString sourceSemantic;  ///< 源 IR 图层语义。
};

/** @brief 规范化的 Allegro Padstack 描述。 */
struct AllegroPadstackModel {
    QString name;  ///< Padstack 稳定名称。
    IR::PadShape shape = IR::PadShape::Rect;  ///< Pad 几何形状。
    IR::PadType padType = IR::PadType::Smd;  ///< Pad 类型。
    QSizeF size;  ///< Pad 外形尺寸。
    QSizeF drill;  ///< 钻孔尺寸。
    double slotLength = 0.0;  ///< 槽孔长度。
    double rotation = 0.0;  ///< Pad 几何旋转角度。
    bool plated = true;  ///< 钻孔是否镀铜。
    IR::LayerType layer = IR::LayerType::TopCopper;  ///< Pad 所属层语义。
    QList<QPointF> polygon;  ///< 自定义多边形顶点。
    QString canonicalKey;  ///< 用于稳定去重的规范化键。
};

/** @brief Allegro 封装引脚与 Padstack 的关联。 */
struct AllegroPinModel {
    QString number;  ///< 引脚编号或机械焊盘标识。
    QString padstackName;  ///< 关联的 Padstack 名称。
    QPointF position;  ///< 引脚位置。
    double rotation = 0.0;  ///< 引脚旋转角度。
    bool mechanical = false;  ///< 是否为机械引脚。
};

/** @brief Allegro 封装级几何图元的目标语义。 */
struct AllegroGeometryModel {
    QString primitive;  ///< 图元类型名称。
    AllegroLayerAssignment layer;  ///< Class/Subclass 映射结果。
    QList<QPointF> points;  ///< 折线或多边形顶点。
    QRectF bounds;  ///< 图元边界矩形。
    QPointF center;  ///< 圆弧或圆的中心点。
    double radius = 0.0;  ///< 圆形图元半径。
    double rotation = 0.0;  ///< 图元旋转角度。
    double width = 0.0;  ///< 线宽。
    QString text;  ///< 文本内容。
};

/** @brief 可序列化的 Allegro 封装目标模型。 */
struct AllegroPackageModel {
    QString name;  ///< 封装名称。
    QString description;  ///< 封装描述。
    double height = 0.0;  ///< 组件高度。
    QList<AllegroPadstackModel> padstacks;  ///< Padstack 定义列表。
    QList<AllegroPinModel> pins;  ///< 引脚和 Padstack 关联列表。
    QList<AllegroGeometryModel> geometry;  ///< Package Geometry 图元列表。
    QList<QString> stepFiles;  ///< 关联的 STEP 文件路径列表。
    QJsonArray stepTransforms;  ///< STEP 坐标变换数组。
    bool usedPlaceBoundFallback = false;  ///< 是否使用了 Place Bound 回退策略。
};

}  // namespace EasyKiConverter
