#include "SymbolDataSerializer.h"
#include "SymbolShapeSerializer.h"

namespace EasyKiConverter {

/**
 * @brief 兼容保留 SymbolDataSerializer 的图形类型接口，实际实现统一委托给图形序列化器。
 * @param bbox 符号或封装边界框。
 */
QJsonObject SymbolDataSerializer::toJson(const SymbolBBox& bbox) {
    return SymbolShapeSerializer::toJson(bbox);
}

/**
 * @brief 委托统一图形序列化器恢复符号边界框。
 * @param bbox 符号或封装边界框。
 * @param json 待读取或写入的 JSON 对象。
 */
bool SymbolDataSerializer::fromJson(SymbolBBox& bbox, const QJsonObject& json) {
    return SymbolShapeSerializer::fromJson(bbox, json);
}

/**
 * @brief 委托统一图形序列化器序列化矩形图元。
 * @param rect 矩形图元。
 */
QJsonObject SymbolDataSerializer::toJson(const SymbolRectangle& rect) {
    return SymbolShapeSerializer::toJson(rect);
}

/**
 * @brief 委托统一图形序列化器恢复矩形图元。
 * @param rect 矩形图元。
 * @param json 待读取或写入的 JSON 对象。
 */
bool SymbolDataSerializer::fromJson(SymbolRectangle& rect, const QJsonObject& json) {
    return SymbolShapeSerializer::fromJson(rect, json);
}

/**
 * @brief 委托统一图形序列化器序列化圆形图元。
 * @param circle 圆形图元。
 */
QJsonObject SymbolDataSerializer::toJson(const SymbolCircle& circle) {
    return SymbolShapeSerializer::toJson(circle);
}

/**
 * @brief 委托统一图形序列化器恢复圆形图元。
 * @param circle 圆形图元。
 * @param json 待读取或写入的 JSON 对象。
 */
bool SymbolDataSerializer::fromJson(SymbolCircle& circle, const QJsonObject& json) {
    return SymbolShapeSerializer::fromJson(circle, json);
}

/**
 * @brief 委托统一图形序列化器序列化圆弧图元。
 * @param arc 圆弧图元。
 */
QJsonObject SymbolDataSerializer::toJson(const SymbolArc& arc) {
    return SymbolShapeSerializer::toJson(arc);
}

/**
 * @brief 委托统一图形序列化器恢复圆弧图元。
 * @param arc 圆弧图元。
 * @param json 待读取或写入的 JSON 对象。
 */
bool SymbolDataSerializer::fromJson(SymbolArc& arc, const QJsonObject& json) {
    return SymbolShapeSerializer::fromJson(arc, json);
}

/**
 * @brief 委托统一图形序列化器序列化椭圆图元。
 * @param ellipse 椭圆图元。
 */
QJsonObject SymbolDataSerializer::toJson(const SymbolEllipse& ellipse) {
    return SymbolShapeSerializer::toJson(ellipse);
}

/**
 * @brief 委托统一图形序列化器恢复椭圆图元。
 * @param ellipse 椭圆图元。
 * @param json 待读取或写入的 JSON 对象。
 */
bool SymbolDataSerializer::fromJson(SymbolEllipse& ellipse, const QJsonObject& json) {
    return SymbolShapeSerializer::fromJson(ellipse, json);
}

/**
 * @brief 委托统一图形序列化器序列化折线图元。
 * @param polyline 折线图元。
 */
QJsonObject SymbolDataSerializer::toJson(const SymbolPolyline& polyline) {
    return SymbolShapeSerializer::toJson(polyline);
}

/**
 * @brief 委托统一图形序列化器恢复折线图元。
 * @param polyline 折线图元。
 * @param json 待读取或写入的 JSON 对象。
 */
bool SymbolDataSerializer::fromJson(SymbolPolyline& polyline, const QJsonObject& json) {
    return SymbolShapeSerializer::fromJson(polyline, json);
}

/**
 * @brief 委托统一图形序列化器序列化多边形图元。
 * @param polygon 多边形图元。
 */
QJsonObject SymbolDataSerializer::toJson(const SymbolPolygon& polygon) {
    return SymbolShapeSerializer::toJson(polygon);
}

/**
 * @brief 委托统一图形序列化器恢复多边形图元。
 * @param polygon 多边形图元。
 * @param json 待读取或写入的 JSON 对象。
 */
bool SymbolDataSerializer::fromJson(SymbolPolygon& polygon, const QJsonObject& json) {
    return SymbolShapeSerializer::fromJson(polygon, json);
}

/**
 * @brief 委托统一图形序列化器序列化路径图元。
 * @param path 路径图元。
 */
QJsonObject SymbolDataSerializer::toJson(const SymbolPath& path) {
    return SymbolShapeSerializer::toJson(path);
}

/**
 * @brief 委托统一图形序列化器恢复路径图元。
 * @param path 路径图元。
 * @param json 待读取或写入的 JSON 对象。
 */
bool SymbolDataSerializer::fromJson(SymbolPath& path, const QJsonObject& json) {
    return SymbolShapeSerializer::fromJson(path, json);
}

/**
 * @brief 委托统一图形序列化器序列化图片图元。
 * @param image 图片图元。
 */
QJsonObject SymbolDataSerializer::toJson(const SymbolImage& image) {
    return SymbolShapeSerializer::toJson(image);
}

/**
 * @brief 委托统一图形序列化器恢复图片图元。
 * @param image 图片图元。
 * @param json 待读取或写入的 JSON 对象。
 */
bool SymbolDataSerializer::fromJson(SymbolImage& image, const QJsonObject& json) {
    return SymbolShapeSerializer::fromJson(image, json);
}

/**
 * @brief 委托统一图形序列化器序列化文本图元。
 * @param text 文本图元。
 */
QJsonObject SymbolDataSerializer::toJson(const SymbolText& text) {
    return SymbolShapeSerializer::toJson(text);
}

/**
 * @brief 委托统一图形序列化器恢复文本图元。
 * @param text 文本图元。
 * @param json 待读取或写入的 JSON 对象。
 */
bool SymbolDataSerializer::fromJson(SymbolText& text, const QJsonObject& json) {
    return SymbolShapeSerializer::fromJson(text, json);
}

}  // namespace EasyKiConverter
