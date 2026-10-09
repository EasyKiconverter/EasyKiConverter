#include "SymbolPinSerializer.h"

#include "SymbolData.h"

#include <QJsonArray>

namespace EasyKiConverter {

// ==================== SymbolPinSettings ====================

/**
 * @brief 将引脚设置序列化为 JSON。
 * @param settings 待序列化的引脚设置。
 * @return 引脚设置 JSON 对象。
 */
QJsonObject SymbolPinSerializer::toJson(const SymbolPinSettings& settings) {
    QJsonObject json;  // 引脚设置 JSON 对象。
    json["is_displayed"] = settings.isDisplayed;
    json["type"] = static_cast<int>(settings.type);
    json["spice_pin_number"] = settings.spicePinNumber;
    json["pos_x"] = settings.posX;
    json["pos_y"] = settings.posY;
    json["rotation"] = settings.rotation;
    json["id"] = settings.id;
    json["is_locked"] = settings.isLocked;
    return json;
}

/**
 * @brief 从 JSON 反序列化引脚设置。
 * @param settings 接收结果的引脚设置。
 * @param json 输入 JSON 对象。
 * @return JSON 包含必需字段时返回 true。
 */
bool SymbolPinSerializer::fromJson(SymbolPinSettings& settings, const QJsonObject& json) {
    if (!json.contains("type") || !json.contains("spice_pin_number")) {
        return false;
    }
    settings.isDisplayed = json["is_displayed"].toBool(true);
    settings.type = static_cast<PinType>(json["type"].toInt(0));
    settings.spicePinNumber = json["spice_pin_number"].toString();
    settings.posX = json["pos_x"].toDouble(0.0);
    settings.posY = json["pos_y"].toDouble(0.0);
    settings.rotation = json["rotation"].toInt(0);
    settings.id = json["id"].toString();
    settings.isLocked = json["is_locked"].toBool(false);
    return true;
}

// ==================== SymbolPinDot ====================

/**
 * @brief 将引脚连接点序列化为 JSON。
 * @param dot 待序列化的连接点。
 * @return 连接点 JSON 对象。
 */
QJsonObject SymbolPinSerializer::toJson(const SymbolPinDot& dot) {
    QJsonObject json;  // 连接点 JSON 对象。
    json["dot_x"] = dot.dotX;
    json["dot_y"] = dot.dotY;
    return json;
}

/**
 * @brief 从 JSON 反序列化引脚连接点。
 * @param dot 接收结果的连接点。
 * @param json 输入 JSON 对象。
 * @return 反序列化成功时返回 true。
 */
bool SymbolPinSerializer::fromJson(SymbolPinDot& dot, const QJsonObject& json) {
    dot.dotX = json["dot_x"].toDouble(0.0);
    dot.dotY = json["dot_y"].toDouble(0.0);
    return true;
}

// ==================== SymbolPinPath ====================

/**
 * @brief 将引脚图形路径序列化为 JSON。
 * @param path 待序列化的图形路径。
 * @return 图形路径 JSON 对象。
 */
QJsonObject SymbolPinSerializer::toJson(const SymbolPinPath& path) {
    QJsonObject json;  // 图形路径 JSON 对象。
    json["path"] = path.path;
    json["color"] = path.color;
    return json;
}

/**
 * @brief 从 JSON 反序列化引脚图形路径。
 * @param path 接收结果的图形路径。
 * @param json 输入 JSON 对象。
 * @return JSON 包含路径字段时返回 true。
 */
bool SymbolPinSerializer::fromJson(SymbolPinPath& path, const QJsonObject& json) {
    if (!json.contains("path")) {
        return false;
    }
    path.path = json["path"].toString();
    path.color = json["color"].toString();
    return true;
}

// ==================== SymbolPinName ====================

/**
 * @brief 将引脚名称文本序列化为 JSON。
 * @param name 待序列化的引脚名称文本。
 * @return 引脚名称 JSON 对象。
 */
QJsonObject SymbolPinSerializer::toJson(const SymbolPinName& name) {
    QJsonObject json;  // 引脚名称 JSON 对象。
    json["is_displayed"] = name.isDisplayed;
    json["pos_x"] = name.posX;
    json["pos_y"] = name.posY;
    json["rotation"] = name.rotation;
    json["text"] = name.text;
    json["text_anchor"] = name.textAnchor;
    json["font"] = name.font;
    json["font_size"] = name.fontSize;
    return json;
}

/**
 * @brief 从 JSON 反序列化引脚名称文本。
 * @param name 接收结果的引脚名称文本。
 * @param json 输入 JSON 对象。
 * @return JSON 包含文本字段时返回 true。
 */
bool SymbolPinSerializer::fromJson(SymbolPinName& name, const QJsonObject& json) {
    if (!json.contains("text")) {
        return false;
    }
    name.isDisplayed = json["is_displayed"].toBool(true);
    name.posX = json["pos_x"].toDouble(0.0);
    name.posY = json["pos_y"].toDouble(0.0);
    name.rotation = json["rotation"].toInt(0);
    name.text = json["text"].toString();
    name.textAnchor = json["text_anchor"].toString();
    name.font = json["font"].toString();
    name.fontSize = json["font_size"].toDouble(7.0);
    return true;
}

// ==================== SymbolPinDotBis ====================

/**
 * @brief 将双向引脚连接点序列化为 JSON。
 * @param dot 待序列化的双向连接点。
 * @return 双向连接点 JSON 对象。
 */
QJsonObject SymbolPinSerializer::toJson(const SymbolPinDotBis& dot) {
    QJsonObject json;  // 双向连接点 JSON 对象。
    json["is_displayed"] = dot.isDisplayed;
    json["circle_x"] = dot.circleX;
    json["circle_y"] = dot.circleY;
    return json;
}

/**
 * @brief 从 JSON 反序列化双向引脚连接点。
 * @param dot 接收结果的双向连接点。
 * @param json 输入 JSON 对象。
 * @return 反序列化成功时返回 true。
 */
bool SymbolPinSerializer::fromJson(SymbolPinDotBis& dot, const QJsonObject& json) {
    dot.isDisplayed = json["is_displayed"].toBool(false);
    dot.circleX = json["circle_x"].toDouble(0.0);
    dot.circleY = json["circle_y"].toDouble(0.0);
    return true;
}

// ==================== SymbolPinClock ====================

/**
 * @brief 将时钟引脚标记序列化为 JSON。
 * @param clock 待序列化的时钟标记。
 * @return 时钟标记 JSON 对象。
 */
QJsonObject SymbolPinSerializer::toJson(const SymbolPinClock& clock) {
    QJsonObject json;  // 时钟标记 JSON 对象。
    json["is_displayed"] = clock.isDisplayed;
    json["path"] = clock.path;
    return json;
}

/**
 * @brief 从 JSON 反序列化时钟引脚标记。
 * @param clock 接收结果的时钟标记。
 * @param json 输入 JSON 对象。
 * @return 反序列化成功时返回 true。
 */
bool SymbolPinSerializer::fromJson(SymbolPinClock& clock, const QJsonObject& json) {
    clock.isDisplayed = json["is_displayed"].toBool(false);
    clock.path = json["path"].toString();
    return true;
}

// ==================== SymbolPin ====================

/**
 * @brief 将完整引脚序列化为 JSON。
 * @param pin 待序列化的引脚。
 * @return 完整引脚 JSON 对象。
 */
QJsonObject SymbolPinSerializer::toJson(const SymbolPin& pin) {
    QJsonObject json;  // 完整引脚 JSON 对象。
    json["settings"] = toJson(pin.settings);
    json["pin_dot"] = toJson(pin.pinDot);
    json["pin_path"] = toJson(pin.pinPath);
    json["name"] = toJson(pin.name);
    json["number"] = toJson(pin.number);
    json["dot"] = toJson(pin.dot);
    json["clock"] = toJson(pin.clock);
    return json;
}

/**
 * @brief 从 JSON 反序列化完整引脚。
 * @param pin 接收结果的引脚。
 * @param json 输入 JSON 对象。
 * @return JSON 包含必需设置和名称字段时返回 true。
 */
bool SymbolPinSerializer::fromJson(SymbolPin& pin, const QJsonObject& json) {
    if (!json.contains("settings") || !json.contains("name")) {
        return false;
    }
    if (!fromJson(pin.settings, json["settings"].toObject())) {
        return false;
    }
    fromJson(pin.pinDot, json["pin_dot"].toObject());
    fromJson(pin.pinPath, json["pin_path"].toObject());
    fromJson(pin.name, json["name"].toObject());
    if (json.contains("number")) {
        fromJson(pin.number, json["number"].toObject());
    }
    fromJson(pin.dot, json["dot"].toObject());
    fromJson(pin.clock, json["clock"].toObject());
    return true;
}

}  // namespace EasyKiConverter
