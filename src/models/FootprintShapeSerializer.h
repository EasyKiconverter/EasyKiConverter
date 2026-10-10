#pragma once

#include "FootprintData.h"

#include <QJsonObject>

namespace EasyKiConverter {

/**
 * @brief 封装图形、层定义及对象可见性的 JSON 序列化工具
 *
 * 负责保持封装模型几何数据与现有 JSON 字段之间的稳定映射。
 */
class FootprintShapeSerializer {
public:
    static QJsonObject toJson(const FootprintBBox& bbox);
    static bool fromJson(FootprintBBox& bbox, const QJsonObject& json);

    static QJsonObject toJson(const FootprintPad& pad);
    static bool fromJson(FootprintPad& pad, const QJsonObject& json);

    static QJsonObject toJson(const FootprintTrack& track);
    static bool fromJson(FootprintTrack& track, const QJsonObject& json);

    static QJsonObject toJson(const FootprintHole& hole);
    static bool fromJson(FootprintHole& hole, const QJsonObject& json);

    static QJsonObject toJson(const FootprintCircle& circle);
    static bool fromJson(FootprintCircle& circle, const QJsonObject& json);

    static QJsonObject toJson(const FootprintRectangle& rect);
    static bool fromJson(FootprintRectangle& rect, const QJsonObject& json);

    static QJsonObject toJson(const FootprintArc& arc);
    static bool fromJson(FootprintArc& arc, const QJsonObject& json);

    static QJsonObject toJson(const FootprintText& text);
    static bool fromJson(FootprintText& text, const QJsonObject& json);

    static QJsonObject toJson(const FootprintSolidRegion& region);
    static bool fromJson(FootprintSolidRegion& region, const QJsonObject& json);

    static QJsonObject toJson(const FootprintOutline& outline);
    static bool fromJson(FootprintOutline& outline, const QJsonObject& json);

    static QJsonObject toJson(const LayerDefinition& layer);
    static bool fromJson(LayerDefinition& layer, const QJsonObject& json);

    static QJsonObject toJson(const ObjectVisibility& visibility);
    static bool fromJson(ObjectVisibility& visibility, const QJsonObject& json);
};

}  // namespace EasyKiConverter
