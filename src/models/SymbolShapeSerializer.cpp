#include "SymbolShapeSerializer.h"

#include <QJsonArray>

namespace EasyKiConverter {

// ==================== SymbolBBox ====================

/**
 * @brief 将符号边界框序列化为 JSON。
 * @param bbox 待序列化的边界框。
 */
QJsonObject SymbolShapeSerializer::toJson(const SymbolBBox& bbox) {
    QJsonObject json;  // 保存边界框字段的 JSON 对象。
    json["x"] = bbox.x;
    json["y"] = bbox.y;
    json["width"] = bbox.width;
    json["height"] = bbox.height;
    json["head_x"] = bbox.headX;
    json["head_y"] = bbox.headY;
    json["has_head_center"] = bbox.hasHeadCenter;
    return json;
}

// 从 JSON 恢复符号边界框及其可选的头部中心。
/**
 * @brief 从 JSON 恢复符号边界框。
 * @param bbox 接收反序列化结果的边界框。
 * @param json 源 JSON 对象。
 */
bool SymbolShapeSerializer::fromJson(SymbolBBox& bbox, const QJsonObject& json) {
    bbox.x = json["x"].toDouble(0.0);
    bbox.y = json["y"].toDouble(0.0);
    bbox.width = json["width"].toDouble(0.0);
    bbox.height = json["height"].toDouble(0.0);
    bbox.headX = json["head_x"].toDouble(0.0);
    bbox.headY = json["head_y"].toDouble(0.0);
    bbox.hasHeadCenter = json["has_head_center"].toBool(false);
    return true;
}

// ==================== SymbolRectangle ====================

/**
 * @brief 将符号矩形序列化为 JSON。
 * @param rect 待序列化的矩形。
 */
QJsonObject SymbolShapeSerializer::toJson(const SymbolRectangle& rect) {
    QJsonObject json;  // 保存矩形字段的 JSON 对象。
    json["pos_x"] = rect.posX;
    json["pos_y"] = rect.posY;
    json["rx"] = rect.rx;
    json["ry"] = rect.ry;
    json["width"] = rect.width;
    json["height"] = rect.height;
    json["stroke_color"] = rect.strokeColor;
    json["stroke_width"] = rect.strokeWidth;
    json["stroke_style"] = rect.strokeStyle;
    json["fill_color"] = rect.fillColor;
    json["id"] = rect.id;
    json["is_locked"] = rect.isLocked;
    return json;
}

// 从 JSON 恢复矩形的位置、圆角、描边和填充属性。
/**
 * @brief 从 JSON 恢复符号矩形。
 * @param rect 接收反序列化结果的矩形。
 * @param json 源 JSON 对象。
 */
bool SymbolShapeSerializer::fromJson(SymbolRectangle& rect, const QJsonObject& json) {
    rect.posX = json["pos_x"].toDouble(0.0);
    rect.posY = json["pos_y"].toDouble(0.0);
    rect.rx = json["rx"].toDouble(0.0);
    rect.ry = json["ry"].toDouble(0.0);
    rect.width = json["width"].toDouble(0.0);
    rect.height = json["height"].toDouble(0.0);
    rect.strokeColor = json["stroke_color"].toString();
    rect.strokeWidth = json["stroke_width"].toDouble(0.0);
    rect.strokeStyle = json["stroke_style"].toString();
    rect.fillColor = json["fill_color"].toString();
    rect.id = json["id"].toString();
    rect.isLocked = json["is_locked"].toBool(false);
    return true;
}

// ==================== SymbolCircle ====================

/**
 * @brief 将符号圆形序列化为 JSON。
 * @param circle 待序列化的圆形。
 */
QJsonObject SymbolShapeSerializer::toJson(const SymbolCircle& circle) {
    QJsonObject json;  // 保存圆形字段的 JSON 对象。
    json["center_x"] = circle.centerX;
    json["center_y"] = circle.centerY;
    json["radius"] = circle.radius;
    json["stroke_color"] = circle.strokeColor;
    json["stroke_width"] = circle.strokeWidth;
    json["stroke_style"] = circle.strokeStyle;
    json["fill_color"] = circle.fillColor;
    json["id"] = circle.id;
    json["is_locked"] = circle.isLocked;
    return json;
}

// 从 JSON 恢复圆形的中心、半径、描边和填充属性。
/**
 * @brief 从 JSON 恢复符号圆形。
 * @param circle 接收反序列化结果的圆形。
 * @param json 源 JSON 对象。
 */
bool SymbolShapeSerializer::fromJson(SymbolCircle& circle, const QJsonObject& json) {
    circle.centerX = json["center_x"].toDouble(0.0);
    circle.centerY = json["center_y"].toDouble(0.0);
    circle.radius = json["radius"].toDouble(0.0);
    circle.strokeColor = json["stroke_color"].toString();
    circle.strokeWidth = json["stroke_width"].toDouble(0.0);
    circle.strokeStyle = json["stroke_style"].toString();
    circle.fillColor = json["fill_color"].toBool(false);
    circle.id = json["id"].toString();
    circle.isLocked = json["is_locked"].toBool(false);
    return true;
}

// ==================== SymbolArc ====================

/**
 * @brief 将符号圆弧序列化为 JSON。
 * @param arc 待序列化的圆弧。
 */
QJsonObject SymbolShapeSerializer::toJson(const SymbolArc& arc) {
    QJsonObject json;  // 保存圆弧字段的 JSON 对象。
    QJsonArray pathArray;  // 保存圆弧路径点的 JSON 数组。
    for (const QPointF& point : arc.path) {
        QJsonObject pointObj;  // 当前路径点的 JSON 对象。
        pointObj["x"] = point.x();
        pointObj["y"] = point.y();
        pathArray.append(pointObj);
    }
    json["path"] = pathArray;
    json["helper_dots"] = arc.helperDots;
    json["stroke_color"] = arc.strokeColor;
    json["stroke_width"] = arc.strokeWidth;
    json["stroke_style"] = arc.strokeStyle;
    json["fill_color"] = arc.fillColor;
    json["id"] = arc.id;
    json["is_locked"] = arc.isLocked;
    return json;
}

// 从 JSON 恢复圆弧路径、辅助点和图形样式属性。
/**
 * @brief 从 JSON 恢复符号圆弧。
 * @param arc 接收反序列化结果的圆弧。
 * @param json 源 JSON 对象。
 */
bool SymbolShapeSerializer::fromJson(SymbolArc& arc, const QJsonObject& json) {
    if (json.contains("path") && json["path"].isArray()) {
        QJsonArray pathArray = json["path"].toArray();  // 圆弧路径 JSON 数组。
        arc.path.clear();
        for (const QJsonValue& value : pathArray) {
            if (value.isObject()) {
                QJsonObject pointObj = value.toObject();  // 当前路径点 JSON 对象。
                QPointF point(pointObj["x"].toDouble(0.0), pointObj["y"].toDouble(0.0));  // 当前路径点。
                arc.path.append(point);
            }
        }
    }
    arc.helperDots = json["helper_dots"].toString();
    arc.strokeColor = json["stroke_color"].toString();
    arc.strokeWidth = json["stroke_width"].toDouble(0.0);
    arc.strokeStyle = json["stroke_style"].toString();
    arc.fillColor = json["fill_color"].toBool(false);
    arc.id = json["id"].toString();
    arc.isLocked = json["is_locked"].toBool(false);
    return true;
}

// ==================== SymbolEllipse ====================

/**
 * @brief 将符号椭圆序列化为 JSON。
 * @param ellipse 待序列化的椭圆。
 */
QJsonObject SymbolShapeSerializer::toJson(const SymbolEllipse& ellipse) {
    QJsonObject json;  // 保存椭圆字段的 JSON 对象。
    json["center_x"] = ellipse.centerX;
    json["center_y"] = ellipse.centerY;
    json["radius_x"] = ellipse.radiusX;
    json["radius_y"] = ellipse.radiusY;
    json["stroke_color"] = ellipse.strokeColor;
    json["stroke_width"] = ellipse.strokeWidth;
    json["stroke_style"] = ellipse.strokeStyle;
    json["fill_color"] = ellipse.fillColor;
    json["id"] = ellipse.id;
    json["is_locked"] = ellipse.isLocked;
    return json;
}

// 从 JSON 恢复椭圆中心、半径和图形样式属性。
/**
 * @brief 从 JSON 恢复符号椭圆。
 * @param ellipse 接收反序列化结果的椭圆。
 * @param json 源 JSON 对象。
 */
bool SymbolShapeSerializer::fromJson(SymbolEllipse& ellipse, const QJsonObject& json) {
    ellipse.centerX = json["center_x"].toDouble(0.0);
    ellipse.centerY = json["center_y"].toDouble(0.0);
    ellipse.radiusX = json["radius_x"].toDouble(0.0);
    ellipse.radiusY = json["radius_y"].toDouble(0.0);
    ellipse.strokeColor = json["stroke_color"].toString();
    ellipse.strokeWidth = json["stroke_width"].toDouble(0.0);
    ellipse.strokeStyle = json["stroke_style"].toString();
    ellipse.fillColor = json["fill_color"].toBool(false);
    ellipse.id = json["id"].toString();
    ellipse.isLocked = json["is_locked"].toBool(false);
    return true;
}

// ==================== SymbolPolyline ====================

/**
 * @brief 将符号折线序列化为 JSON。
 * @param polyline 待序列化的折线。
 */
QJsonObject SymbolShapeSerializer::toJson(const SymbolPolyline& polyline) {
    QJsonObject json;  // 保存折线字段的 JSON 对象。
    json["points"] = polyline.points;
    json["stroke_color"] = polyline.strokeColor;
    json["stroke_width"] = polyline.strokeWidth;
    json["stroke_style"] = polyline.strokeStyle;
    json["fill_color"] = polyline.fillColor;
    json["id"] = polyline.id;
    json["is_locked"] = polyline.isLocked;
    return json;
}

// 从 JSON 恢复折线坐标文本和图形样式属性。
/**
 * @brief 从 JSON 恢复符号折线。
 * @param polyline 接收反序列化结果的折线。
 * @param json 源 JSON 对象。
 */
bool SymbolShapeSerializer::fromJson(SymbolPolyline& polyline, const QJsonObject& json) {
    polyline.points = json["points"].toString();
    polyline.strokeColor = json["stroke_color"].toString();
    polyline.strokeWidth = json["stroke_width"].toDouble(0.0);
    polyline.strokeStyle = json["stroke_style"].toString();
    polyline.fillColor = json["fill_color"].toBool(false);
    polyline.id = json["id"].toString();
    polyline.isLocked = json["is_locked"].toBool(false);
    return true;
}

// ==================== SymbolPolygon ====================

/**
 * @brief 将符号多边形序列化为 JSON。
 * @param polygon 待序列化的多边形。
 */
QJsonObject SymbolShapeSerializer::toJson(const SymbolPolygon& polygon) {
    QJsonObject json;  // 保存多边形字段的 JSON 对象。
    json["points"] = polygon.points;
    json["stroke_color"] = polygon.strokeColor;
    json["stroke_width"] = polygon.strokeWidth;
    json["stroke_style"] = polygon.strokeStyle;
    json["fill_color"] = polygon.fillColor;
    json["id"] = polygon.id;
    json["is_locked"] = polygon.isLocked;
    return json;
}

// 从 JSON 恢复多边形坐标文本和图形样式属性。
/**
 * @brief 从 JSON 恢复符号多边形。
 * @param polygon 接收反序列化结果的多边形。
 * @param json 源 JSON 对象。
 */
bool SymbolShapeSerializer::fromJson(SymbolPolygon& polygon, const QJsonObject& json) {
    polygon.points = json["points"].toString();
    polygon.strokeColor = json["stroke_color"].toString();
    polygon.strokeWidth = json["stroke_width"].toDouble(0.0);
    polygon.strokeStyle = json["stroke_style"].toString();
    polygon.fillColor = json["fill_color"].toBool(false);
    polygon.id = json["id"].toString();
    polygon.isLocked = json["is_locked"].toBool(false);
    return true;
}

// ==================== SymbolPath ====================

/**
 * @brief 将符号路径序列化为 JSON。
 * @param path 待序列化的路径。
 */
QJsonObject SymbolShapeSerializer::toJson(const SymbolPath& path) {
    QJsonObject json;  // 保存路径字段的 JSON 对象。
    json["paths"] = path.paths;
    json["stroke_color"] = path.strokeColor;
    json["stroke_width"] = path.strokeWidth;
    json["stroke_style"] = path.strokeStyle;
    json["fill_color"] = path.fillColor;
    json["id"] = path.id;
    json["is_locked"] = path.isLocked;
    return json;
}

// 从 JSON 恢复路径命令文本和图形样式属性。
/**
 * @brief 从 JSON 恢复符号路径。
 * @param path 接收反序列化结果的路径。
 * @param json 源 JSON 对象。
 */
bool SymbolShapeSerializer::fromJson(SymbolPath& path, const QJsonObject& json) {
    path.paths = json["paths"].toString();
    path.strokeColor = json["stroke_color"].toString();
    path.strokeWidth = json["stroke_width"].toDouble(0.0);
    path.strokeStyle = json["stroke_style"].toString();
    path.fillColor = json["fill_color"].toBool(false);
    path.id = json["id"].toString();
    path.isLocked = json["is_locked"].toBool(false);
    return true;
}

// ==================== SymbolImage ====================

/**
 * @brief 将符号图片序列化为 JSON。
 * @param image 待序列化的图片。
 */
QJsonObject SymbolShapeSerializer::toJson(const SymbolImage& image) {
    QJsonObject json;  // 保存图片字段的 JSON 对象。
    json["pos_x"] = image.posX;
    json["pos_y"] = image.posY;
    json["width"] = image.width;
    json["height"] = image.height;
    json["rotation"] = image.rotation;
    json["source"] = image.source;
    json["file_name"] = image.fileName;
    json["data_base64"] = QString::fromLatin1(image.data.toBase64());
    json["is_locked"] = image.isLocked;
    return json;
}

// 从 JSON 恢复图片位置、资源标识和嵌入数据。
/**
 * @brief 从 JSON 恢复符号图片。
 * @param image 接收反序列化结果的图片。
 * @param json 源 JSON 对象。
 */
bool SymbolShapeSerializer::fromJson(SymbolImage& image, const QJsonObject& json) {
    image.posX = json["pos_x"].toDouble(0.0);
    image.posY = json["pos_y"].toDouble(0.0);
    image.width = json["width"].toDouble(0.0);
    image.height = json["height"].toDouble(0.0);
    image.rotation = json["rotation"].toDouble(0.0);
    image.source = json["source"].toString();
    image.fileName = json["file_name"].toString();
    image.data = QByteArray::fromBase64(json["data_base64"].toString().toLatin1());
    image.isLocked = json["is_locked"].toBool(false);
    return true;
}

// ==================== SymbolText ====================

/**
 * @brief 将符号文本序列化为 JSON。
 * @param text 待序列化的文本。
 */
QJsonObject SymbolShapeSerializer::toJson(const SymbolText& text) {
    QJsonObject json;  // 保存文本字段的 JSON 对象。
    json["mark"] = text.mark;
    json["pos_x"] = text.posX;
    json["pos_y"] = text.posY;
    json["rotation"] = text.rotation;
    json["color"] = text.color;
    json["font"] = text.font;
    json["text_size"] = text.textSize;
    json["bold"] = text.bold;
    json["italic"] = text.italic;
    json["baseline"] = text.baseline;
    json["type"] = text.type;
    json["text"] = text.text;
    json["visible"] = text.visible;
    json["anchor"] = text.anchor;
    json["id"] = text.id;
    json["is_locked"] = text.isLocked;
    return json;
}

// 从 JSON 恢复文本内容、排版参数和显示状态。
/**
 * @brief 从 JSON 恢复符号文本。
 * @param text 接收反序列化结果的文本。
 * @param json 源 JSON 对象。
 */
bool SymbolShapeSerializer::fromJson(SymbolText& text, const QJsonObject& json) {
    text.mark = json["mark"].toString();
    text.posX = json["pos_x"].toDouble(0.0);
    text.posY = json["pos_y"].toDouble(0.0);
    text.rotation = json["rotation"].toInt(0);
    text.color = json["color"].toString();
    text.font = json["font"].toString();
    text.textSize = json["text_size"].toDouble(10.0);
    text.bold = json["bold"].toBool(false);
    text.italic = json["italic"].toString();
    text.baseline = json["baseline"].toString();
    text.type = json["type"].toString();
    text.text = json["text"].toString();
    text.visible = json["visible"].toBool(true);
    text.anchor = json["anchor"].toString();
    text.id = json["id"].toString();
    text.isLocked = json["is_locked"].toBool(false);
    return true;
}

}  // namespace EasyKiConverter
