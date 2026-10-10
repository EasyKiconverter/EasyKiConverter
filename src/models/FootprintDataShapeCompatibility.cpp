#include "FootprintDataSerializer.h"
#include "FootprintShapeSerializer.h"

namespace EasyKiConverter {

/**
 * @brief 保持封装序列化接口与 JSON 字段映射兼容。
 * @param value 待序列化或恢复的封装图形值。
 */
QJsonObject FootprintDataSerializer::toJson(const FootprintBBox& value) {
    return FootprintShapeSerializer::toJson(value);
}

/**
 * @brief 保持封装序列化接口与 JSON 字段映射兼容。
 * @param value 待序列化或恢复的封装图形值。
 * @param json 待读取或写入的 JSON 对象。
 */
bool FootprintDataSerializer::fromJson(FootprintBBox& value, const QJsonObject& json) {
    return FootprintShapeSerializer::fromJson(value, json);
}

/**
 * @brief 保持封装序列化接口与 JSON 字段映射兼容。
 * @param value 待序列化或恢复的封装图形值。
 */
QJsonObject FootprintDataSerializer::toJson(const FootprintPad& value) {
    return FootprintShapeSerializer::toJson(value);
}

/**
 * @brief 保持封装序列化接口与 JSON 字段映射兼容。
 * @param value 待序列化或恢复的封装图形值。
 * @param json 待读取或写入的 JSON 对象。
 */
bool FootprintDataSerializer::fromJson(FootprintPad& value, const QJsonObject& json) {
    return FootprintShapeSerializer::fromJson(value, json);
}

/**
 * @brief 保持封装序列化接口与 JSON 字段映射兼容。
 * @param value 待序列化或恢复的封装图形值。
 */
QJsonObject FootprintDataSerializer::toJson(const FootprintTrack& value) {
    return FootprintShapeSerializer::toJson(value);
}

/**
 * @brief 保持封装序列化接口与 JSON 字段映射兼容。
 * @param value 待序列化或恢复的封装图形值。
 * @param json 待读取或写入的 JSON 对象。
 */
bool FootprintDataSerializer::fromJson(FootprintTrack& value, const QJsonObject& json) {
    return FootprintShapeSerializer::fromJson(value, json);
}

/**
 * @brief 保持封装序列化接口与 JSON 字段映射兼容。
 * @param value 待序列化或恢复的封装图形值。
 */
QJsonObject FootprintDataSerializer::toJson(const FootprintHole& value) {
    return FootprintShapeSerializer::toJson(value);
}

/**
 * @brief 保持封装序列化接口与 JSON 字段映射兼容。
 * @param value 待序列化或恢复的封装图形值。
 * @param json 待读取或写入的 JSON 对象。
 */
bool FootprintDataSerializer::fromJson(FootprintHole& value, const QJsonObject& json) {
    return FootprintShapeSerializer::fromJson(value, json);
}

/**
 * @brief 保持封装序列化接口与 JSON 字段映射兼容。
 * @param value 待序列化或恢复的封装图形值。
 */
QJsonObject FootprintDataSerializer::toJson(const FootprintCircle& value) {
    return FootprintShapeSerializer::toJson(value);
}

/**
 * @brief 保持封装序列化接口与 JSON 字段映射兼容。
 * @param value 待序列化或恢复的封装图形值。
 * @param json 待读取或写入的 JSON 对象。
 */
bool FootprintDataSerializer::fromJson(FootprintCircle& value, const QJsonObject& json) {
    return FootprintShapeSerializer::fromJson(value, json);
}

/**
 * @brief 保持封装序列化接口与 JSON 字段映射兼容。
 * @param value 待序列化或恢复的封装图形值。
 */
QJsonObject FootprintDataSerializer::toJson(const FootprintRectangle& value) {
    return FootprintShapeSerializer::toJson(value);
}

/**
 * @brief 保持封装序列化接口与 JSON 字段映射兼容。
 * @param value 待序列化或恢复的封装图形值。
 * @param json 待读取或写入的 JSON 对象。
 */
bool FootprintDataSerializer::fromJson(FootprintRectangle& value, const QJsonObject& json) {
    return FootprintShapeSerializer::fromJson(value, json);
}

/**
 * @brief 保持封装序列化接口与 JSON 字段映射兼容。
 * @param value 待序列化或恢复的封装图形值。
 */
QJsonObject FootprintDataSerializer::toJson(const FootprintArc& value) {
    return FootprintShapeSerializer::toJson(value);
}

/**
 * @brief 保持封装序列化接口与 JSON 字段映射兼容。
 * @param value 待序列化或恢复的封装图形值。
 * @param json 待读取或写入的 JSON 对象。
 */
bool FootprintDataSerializer::fromJson(FootprintArc& value, const QJsonObject& json) {
    return FootprintShapeSerializer::fromJson(value, json);
}

/**
 * @brief 保持封装序列化接口与 JSON 字段映射兼容。
 * @param value 待序列化或恢复的封装图形值。
 */
QJsonObject FootprintDataSerializer::toJson(const FootprintText& value) {
    return FootprintShapeSerializer::toJson(value);
}

/**
 * @brief 保持封装序列化接口与 JSON 字段映射兼容。
 * @param value 待序列化或恢复的封装图形值。
 * @param json 待读取或写入的 JSON 对象。
 */
bool FootprintDataSerializer::fromJson(FootprintText& value, const QJsonObject& json) {
    return FootprintShapeSerializer::fromJson(value, json);
}

/**
 * @brief 保持封装序列化接口与 JSON 字段映射兼容。
 * @param value 待序列化或恢复的封装图形值。
 */
QJsonObject FootprintDataSerializer::toJson(const FootprintSolidRegion& value) {
    return FootprintShapeSerializer::toJson(value);
}

/**
 * @brief 保持封装序列化接口与 JSON 字段映射兼容。
 * @param value 待序列化或恢复的封装图形值。
 * @param json 待读取或写入的 JSON 对象。
 */
bool FootprintDataSerializer::fromJson(FootprintSolidRegion& value, const QJsonObject& json) {
    return FootprintShapeSerializer::fromJson(value, json);
}

/**
 * @brief 保持封装序列化接口与 JSON 字段映射兼容。
 * @param value 待序列化或恢复的封装图形值。
 */
QJsonObject FootprintDataSerializer::toJson(const FootprintOutline& value) {
    return FootprintShapeSerializer::toJson(value);
}

/**
 * @brief 保持封装序列化接口与 JSON 字段映射兼容。
 * @param value 待序列化或恢复的封装图形值。
 * @param json 待读取或写入的 JSON 对象。
 */
bool FootprintDataSerializer::fromJson(FootprintOutline& value, const QJsonObject& json) {
    return FootprintShapeSerializer::fromJson(value, json);
}

/**
 * @brief 保持封装序列化接口与 JSON 字段映射兼容。
 * @param value 待序列化或恢复的封装图形值。
 */
QJsonObject FootprintDataSerializer::toJson(const LayerDefinition& value) {
    return FootprintShapeSerializer::toJson(value);
}

/**
 * @brief 保持封装序列化接口与 JSON 字段映射兼容。
 * @param value 待序列化或恢复的封装图形值。
 * @param json 待读取或写入的 JSON 对象。
 */
bool FootprintDataSerializer::fromJson(LayerDefinition& value, const QJsonObject& json) {
    return FootprintShapeSerializer::fromJson(value, json);
}

/**
 * @brief 保持封装序列化接口与 JSON 字段映射兼容。
 * @param value 待序列化或恢复的封装图形值。
 */
QJsonObject FootprintDataSerializer::toJson(const ObjectVisibility& value) {
    return FootprintShapeSerializer::toJson(value);
}

/**
 * @brief 保持封装序列化接口与 JSON 字段映射兼容。
 * @param value 待序列化或恢复的封装图形值。
 * @param json 待读取或写入的 JSON 对象。
 */
bool FootprintDataSerializer::fromJson(ObjectVisibility& value, const QJsonObject& json) {
    return FootprintShapeSerializer::fromJson(value, json);
}

}  // namespace EasyKiConverter
