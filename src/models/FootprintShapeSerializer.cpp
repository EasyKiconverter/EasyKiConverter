#include "FootprintShapeSerializer.h"

namespace EasyKiConverter {

/**
 * @brief 保持封装序列化接口与 JSON 字段映射兼容。
 * @param bbox 符号或封装边界框。
 */
QJsonObject FootprintShapeSerializer::toJson(const FootprintBBox& bbox) {
    return {{"x", bbox.x}, {"y", bbox.y}, {"width", bbox.width}, {"height", bbox.height}};
}

/**
 * @brief 保持封装序列化接口与 JSON 字段映射兼容。
 * @param bbox 符号或封装边界框。
 * @param json 待读取或写入的 JSON 对象。
 */
bool FootprintShapeSerializer::fromJson(FootprintBBox& bbox, const QJsonObject& json) {
    bbox.x = json["x"].toDouble();
    bbox.y = json["y"].toDouble();
    bbox.width = json["width"].toDouble();
    bbox.height = json["height"].toDouble();
    return true;
}

/**
 * @brief 保持封装序列化接口与 JSON 字段映射兼容。
 * @param pad 焊盘图元。
 */
QJsonObject FootprintShapeSerializer::toJson(const FootprintPad& pad) {
    return {{"shape", pad.shape},
            {"center_x", pad.centerX},
            {"center_y", pad.centerY},
            {"width", pad.width},
            {"height", pad.height},
            {"layer_id", pad.layerId},
            {"net", pad.net},
            {"number", pad.number},
            {"hole_radius", pad.holeRadius},
            {"points", pad.points},
            {"rotation", pad.rotation},
            {"id", pad.id},
            {"hole_length", pad.holeLength},
            {"hole_point", pad.holePoint},
            {"is_plated", pad.isPlated},
            {"is_locked", pad.isLocked}};
}

/**
 * @brief 保持封装序列化接口与 JSON 字段映射兼容。
 * @param pad 焊盘图元。
 * @param json 待读取或写入的 JSON 对象。
 */
bool FootprintShapeSerializer::fromJson(FootprintPad& pad, const QJsonObject& json) {
    pad.shape = json["shape"].toString();
    pad.centerX = json["center_x"].toDouble();
    pad.centerY = json["center_y"].toDouble();
    pad.width = json["width"].toDouble();
    pad.height = json["height"].toDouble();
    pad.layerId = json["layer_id"].toInt();
    pad.net = json["net"].toString();
    pad.number = json["number"].toString();
    pad.holeRadius = json["hole_radius"].toDouble();
    pad.points = json["points"].toString();
    pad.rotation = json["rotation"].toDouble();
    pad.id = json["id"].toString();
    pad.holeLength = json["hole_length"].toDouble();
    pad.holePoint = json["hole_point"].toString();
    pad.isPlated = json["is_plated"].toBool();
    pad.isLocked = json["is_locked"].toBool();
    return true;
}

/**
 * @brief 保持封装序列化接口与 JSON 字段映射兼容。
 * @param track 走线图元。
 */
QJsonObject FootprintShapeSerializer::toJson(const FootprintTrack& track) {
    return {{"stroke_width", track.strokeWidth},
            {"layer_id", track.layerId},
            {"net", track.net},
            {"points", track.points},
            {"id", track.id},
            {"is_locked", track.isLocked}};
}

/**
 * @brief 保持封装序列化接口与 JSON 字段映射兼容。
 * @param track 走线图元。
 * @param json 待读取或写入的 JSON 对象。
 */
bool FootprintShapeSerializer::fromJson(FootprintTrack& track, const QJsonObject& json) {
    track.strokeWidth = json["stroke_width"].toDouble();
    track.layerId = json["layer_id"].toInt();
    track.net = json["net"].toString();
    track.points = json["points"].toString();
    track.id = json["id"].toString();
    track.isLocked = json["is_locked"].toBool();
    return true;
}

/**
 * @brief 保持封装序列化接口与 JSON 字段映射兼容。
 * @param hole 机械孔图元。
 */
QJsonObject FootprintShapeSerializer::toJson(const FootprintHole& hole) {
    return {{"center_x", hole.centerX},
            {"center_y", hole.centerY},
            {"radius", hole.radius},
            {"id", hole.id},
            {"is_locked", hole.isLocked}};
}

/**
 * @brief 保持封装序列化接口与 JSON 字段映射兼容。
 * @param hole 机械孔图元。
 * @param json 待读取或写入的 JSON 对象。
 */
bool FootprintShapeSerializer::fromJson(FootprintHole& hole, const QJsonObject& json) {
    hole.centerX = json["center_x"].toDouble();
    hole.centerY = json["center_y"].toDouble();
    hole.radius = json["radius"].toDouble();
    hole.id = json["id"].toString();
    hole.isLocked = json["is_locked"].toBool();
    return true;
}

/**
 * @brief 保持封装序列化接口与 JSON 字段映射兼容。
 * @param circle 圆形图元。
 */
QJsonObject FootprintShapeSerializer::toJson(const FootprintCircle& circle) {
    return {{"cx", circle.cx},
            {"cy", circle.cy},
            {"radius", circle.radius},
            {"stroke_width", circle.strokeWidth},
            {"layer_id", circle.layerId},
            {"id", circle.id},
            {"is_locked", circle.isLocked}};
}

/**
 * @brief 保持封装序列化接口与 JSON 字段映射兼容。
 * @param circle 圆形图元。
 * @param json 待读取或写入的 JSON 对象。
 */
bool FootprintShapeSerializer::fromJson(FootprintCircle& circle, const QJsonObject& json) {
    circle.cx = json["cx"].toDouble();
    circle.cy = json["cy"].toDouble();
    circle.radius = json["radius"].toDouble();
    circle.strokeWidth = json["stroke_width"].toDouble();
    circle.layerId = json["layer_id"].toInt();
    circle.id = json["id"].toString();
    circle.isLocked = json["is_locked"].toBool();
    return true;
}

/**
 * @brief 保持封装序列化接口与 JSON 字段映射兼容。
 * @param rect 矩形图元。
 */
QJsonObject FootprintShapeSerializer::toJson(const FootprintRectangle& rect) {
    return {{"x", rect.x},
            {"y", rect.y},
            {"width", rect.width},
            {"height", rect.height},
            {"stroke_width", rect.strokeWidth},
            {"id", rect.id},
            {"layer_id", rect.layerId},
            {"is_locked", rect.isLocked}};
}

/**
 * @brief 保持封装序列化接口与 JSON 字段映射兼容。
 * @param rect 矩形图元。
 * @param json 待读取或写入的 JSON 对象。
 */
bool FootprintShapeSerializer::fromJson(FootprintRectangle& rect, const QJsonObject& json) {
    rect.x = json["x"].toDouble();
    rect.y = json["y"].toDouble();
    rect.width = json["width"].toDouble();
    rect.height = json["height"].toDouble();
    rect.strokeWidth = json["stroke_width"].toDouble();
    rect.id = json["id"].toString();
    rect.layerId = json["layer_id"].toInt();
    rect.isLocked = json["is_locked"].toBool();
    return true;
}

/**
 * @brief 保持封装序列化接口与 JSON 字段映射兼容。
 * @param arc 圆弧图元。
 */
QJsonObject FootprintShapeSerializer::toJson(const FootprintArc& arc) {
    return {{"stroke_width", arc.strokeWidth},
            {"layer_id", arc.layerId},
            {"net", arc.net},
            {"path", arc.path},
            {"helper_dots", arc.helperDots},
            {"id", arc.id},
            {"is_locked", arc.isLocked}};
}

/**
 * @brief 保持封装序列化接口与 JSON 字段映射兼容。
 * @param arc 圆弧图元。
 * @param json 待读取或写入的 JSON 对象。
 */
bool FootprintShapeSerializer::fromJson(FootprintArc& arc, const QJsonObject& json) {
    arc.strokeWidth = json["stroke_width"].toDouble();
    arc.layerId = json["layer_id"].toInt();
    arc.net = json["net"].toString();
    arc.path = json["path"].toString();
    arc.helperDots = json["helper_dots"].toString();
    arc.id = json["id"].toString();
    arc.isLocked = json["is_locked"].toBool();
    return true;
}

/**
 * @brief 保持封装序列化接口与 JSON 字段映射兼容。
 * @param text 文本图元。
 */
QJsonObject FootprintShapeSerializer::toJson(const FootprintText& text) {
    return {{"type", text.type},
            {"center_x", text.centerX},
            {"center_y", text.centerY},
            {"stroke_width", text.strokeWidth},
            {"rotation", text.rotation},
            {"mirror", text.mirror},
            {"layer_id", text.layerId},
            {"net", text.net},
            {"font_size", text.fontSize},
            {"text", text.text},
            {"text_path", text.textPath},
            {"is_displayed", text.isDisplayed},
            {"id", text.id},
            {"is_locked", text.isLocked}};
}

/**
 * @brief 保持封装序列化接口与 JSON 字段映射兼容。
 * @param text 文本图元。
 * @param json 待读取或写入的 JSON 对象。
 */
bool FootprintShapeSerializer::fromJson(FootprintText& text, const QJsonObject& json) {
    text.type = json["type"].toString();
    text.centerX = json["center_x"].toDouble();
    text.centerY = json["center_y"].toDouble();
    text.strokeWidth = json["stroke_width"].toDouble();
    text.rotation = json["rotation"].toInt();
    text.mirror = json["mirror"].toString();
    text.layerId = json["layer_id"].toInt();
    text.net = json["net"].toString();
    text.fontSize = json["font_size"].toDouble();
    text.text = json["text"].toString();
    text.textPath = json["text_path"].toString();
    text.isDisplayed = json["is_displayed"].toBool();
    text.id = json["id"].toString();
    text.isLocked = json["is_locked"].toBool();
    return true;
}

/**
 * @brief 保持封装序列化接口与 JSON 字段映射兼容。
 * @param region 实心区域图元。
 */
QJsonObject FootprintShapeSerializer::toJson(const FootprintSolidRegion& region) {
    return {{"path", region.path},
            {"layer_id", region.layerId},
            {"fill_style", region.fillStyle},
            {"id", region.id},
            {"is_keep_out", region.isKeepOut},
            {"is_locked", region.isLocked}};
}

/**
 * @brief 保持封装序列化接口与 JSON 字段映射兼容。
 * @param region 实心区域图元。
 * @param json 待读取或写入的 JSON 对象。
 */
bool FootprintShapeSerializer::fromJson(FootprintSolidRegion& region, const QJsonObject& json) {
    region.path = json["path"].toString();
    region.layerId = json["layer_id"].toInt();
    region.fillStyle = json["fill_style"].toString();
    region.id = json["id"].toString();
    region.isKeepOut = json["is_keep_out"].toBool();
    region.isLocked = json["is_locked"].toBool();
    return true;
}

/**
 * @brief 保持封装序列化接口与 JSON 字段映射兼容。
 * @param outline 封装外形图元。
 */
QJsonObject FootprintShapeSerializer::toJson(const FootprintOutline& outline) {
    return {{"path", outline.path},
            {"layer_id", outline.layerId},
            {"stroke_width", outline.strokeWidth},
            {"id", outline.id},
            {"is_locked", outline.isLocked}};
}

/**
 * @brief 保持封装序列化接口与 JSON 字段映射兼容。
 * @param outline 封装外形图元。
 * @param json 待读取或写入的 JSON 对象。
 */
bool FootprintShapeSerializer::fromJson(FootprintOutline& outline, const QJsonObject& json) {
    outline.path = json["path"].toString();
    outline.layerId = json["layer_id"].toInt();
    outline.strokeWidth = json["stroke_width"].toDouble();
    outline.id = json["id"].toString();
    outline.isLocked = json["is_locked"].toBool();
    return true;
}

/**
 * @brief 保持封装序列化接口与 JSON 字段映射兼容。
 * @param layer 层定义。
 */
QJsonObject FootprintShapeSerializer::toJson(const LayerDefinition& layer) {
    return {{"layer_id", layer.layerId},
            {"name", layer.name},
            {"color", layer.color},
            {"is_visible", layer.isVisible},
            {"is_used_for_manufacturing", layer.isUsedForManufacturing},
            {"expansion", layer.expansion}};
}

/**
 * @brief 保持封装序列化接口与 JSON 字段映射兼容。
 * @param layer 层定义。
 * @param json 待读取或写入的 JSON 对象。
 */
bool FootprintShapeSerializer::fromJson(LayerDefinition& layer, const QJsonObject& json) {
    layer.layerId = json["layer_id"].toInt();
    layer.name = json["name"].toString();
    layer.color = json["color"].toString();
    layer.isVisible = json["is_visible"].toBool();
    layer.isUsedForManufacturing = json["is_used_for_manufacturing"].toBool();
    layer.expansion = json["expansion"].toDouble();
    return true;
}

/**
 * @brief 保持封装序列化接口与 JSON 字段映射兼容。
 * @param visibility 图元可见性配置。
 */
QJsonObject FootprintShapeSerializer::toJson(const ObjectVisibility& visibility) {
    return {{"object_type", visibility.objectType},
            {"is_enabled", visibility.isEnabled},
            {"is_visible", visibility.isVisible}};
}

/**
 * @brief 保持封装序列化接口与 JSON 字段映射兼容。
 * @param visibility 图元可见性配置。
 * @param json 待读取或写入的 JSON 对象。
 */
bool FootprintShapeSerializer::fromJson(ObjectVisibility& visibility, const QJsonObject& json) {
    visibility.objectType = json["object_type"].toString();
    visibility.isEnabled = json["is_enabled"].toBool();
    visibility.isVisible = json["is_visible"].toBool();
    return true;
}

}  // namespace EasyKiConverter
