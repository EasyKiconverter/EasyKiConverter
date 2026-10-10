#ifndef SYMBOLDATASERIALIZER_H
#define SYMBOLDATASERIALIZER_H

#include "SymbolData.h"

#include <QJsonObject>

namespace EasyKiConverter {

class SymbolPinSerializer;
class SymbolShapeSerializer;

class SymbolDataSerializer {
public:
    static QJsonObject toJson(const SymbolData& data);
    static bool fromJson(SymbolData& data, const QJsonObject& json);

    // Root-level struct serializers
    static QJsonObject toJson(const SymbolInfo& info);
    static bool fromJson(SymbolInfo& info, const QJsonObject& json);

    static QJsonObject toJson(const SymbolPin& pin);
    static bool fromJson(SymbolPin& pin, const QJsonObject& json);

    static QJsonObject toJson(const SymbolPart& part);
    static bool fromJson(SymbolPart& part, const QJsonObject& json);

    // Shape types delegate to SymbolShapeSerializer
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

private:
    // Internal helpers delegate to specialized serializers
    static QJsonObject toJson(const SymbolPinSettings& settings);
    static bool fromJson(SymbolPinSettings& settings, const QJsonObject& json);

    static QJsonObject toJson(const SymbolPinDot& dot);
    static bool fromJson(SymbolPinDot& dot, const QJsonObject& json);

    static QJsonObject toJson(const SymbolPinPath& path);
    static bool fromJson(SymbolPinPath& path, const QJsonObject& json);

    static QJsonObject toJson(const SymbolPinName& name);
    static bool fromJson(SymbolPinName& name, const QJsonObject& json);

    static QJsonObject toJson(const SymbolPinDotBis& dot);
    static bool fromJson(SymbolPinDotBis& dot, const QJsonObject& json);

    static QJsonObject toJson(const SymbolPinClock& clock);
    static bool fromJson(SymbolPinClock& clock, const QJsonObject& json);
};

}  // namespace EasyKiConverter

#endif  // SYMBOLDATASERIALIZER_H
