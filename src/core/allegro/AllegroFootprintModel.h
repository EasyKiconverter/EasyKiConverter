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
    QString className;
    QString subclassName;
    QString sourceSemantic;
};

/** @brief 规范化的 Allegro Padstack 描述。 */
struct AllegroPadstackModel {
    QString name;
    IR::PadShape shape = IR::PadShape::Rect;
    IR::PadType padType = IR::PadType::Smd;
    QSizeF size;
    QSizeF drill;
    double slotLength = 0.0;
    double rotation = 0.0;
    bool plated = true;
    IR::LayerType layer = IR::LayerType::TopCopper;
    QList<QPointF> polygon;
    QString canonicalKey;
};

/** @brief Allegro 封装引脚与 Padstack 的关联。 */
struct AllegroPinModel {
    QString number;
    QString padstackName;
    QPointF position;
    double rotation = 0.0;
    bool mechanical = false;
};

/** @brief Allegro 封装级几何图元的目标语义。 */
struct AllegroGeometryModel {
    QString primitive;
    AllegroLayerAssignment layer;
    QList<QPointF> points;
    QRectF bounds;
    QPointF center;
    double radius = 0.0;
    double rotation = 0.0;
    double width = 0.0;
    QString text;
};

/** @brief 可序列化的 Allegro 封装目标模型。 */
struct AllegroPackageModel {
    QString name;
    QString description;
    double height = 0.0;
    QList<AllegroPadstackModel> padstacks;
    QList<AllegroPinModel> pins;
    QList<AllegroGeometryModel> geometry;
    QList<QString> stepFiles;
    QJsonArray stepTransforms;
    bool usedPlaceBoundFallback = false;
};

}  // namespace EasyKiConverter
