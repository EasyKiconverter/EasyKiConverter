#pragma once

#include "SymbolData.h"

namespace EasyKiConverter {

/**
 * @brief KiCad符号几何形状的JSON序列化工具
 *
 * 负责以下类型的序列化：
 * - SymbolBBox (边界框)
 * - SymbolRectangle (矩形)
 * - SymbolCircle (圆形)
 * - SymbolArc (圆弧)
 * - SymbolEllipse (椭圆)
 * - SymbolPolyline (多段线)
 * - SymbolPolygon (多边形)
 * - SymbolPath (路径)
 * - SymbolImage (图片)
 * - SymbolText (文本)
 */
class SymbolShapeSerializer {
public:
    static QJsonObject toJson(const SymbolBBox& bbox);
    static bool fromJson(SymbolBBox& bbox, const QJsonObject& json);

    static QJsonObject toJson(const SymbolRectangle& rect);
    static bool fromJson(SymbolRectangle& rect, const QJsonObject& json);

    static QJsonObject toJson(const SymbolCircle& circle);
    static bool fromJson(SymbolCircle& circle, const QJsonObject& json);

    static QJsonObject toJson(const SymbolArc& arc);
    static bool fromJson(SymbolArc& arc, const QJsonObject& json);

    static QJsonObject toJson(const SymbolEllipse& ellipse);
    static bool fromJson(SymbolEllipse& ellipse, const QJsonObject& json);

    static QJsonObject toJson(const SymbolPolyline& polyline);
    static bool fromJson(SymbolPolyline& polyline, const QJsonObject& json);

    static QJsonObject toJson(const SymbolPolygon& polygon);
    static bool fromJson(SymbolPolygon& polygon, const QJsonObject& json);

    static QJsonObject toJson(const SymbolPath& path);
    static bool fromJson(SymbolPath& path, const QJsonObject& json);

    static QJsonObject toJson(const SymbolImage& image);
    static bool fromJson(SymbolImage& image, const QJsonObject& json);

    static QJsonObject toJson(const SymbolText& text);
    static bool fromJson(SymbolText& text, const QJsonObject& json);
};

}  // namespace EasyKiConverter
