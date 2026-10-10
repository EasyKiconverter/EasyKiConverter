#include "SymbolDataSerializer.h"

#include "SymbolPinSerializer.h"
#include "SymbolShapeSerializer.h"

#include <QDebug>
#include <QJsonArray>

namespace EasyKiConverter {

// Pin sub-type helpers delegate to SymbolPinSerializer
static QJsonObject toJson(const SymbolPinSettings& settings) {
    return SymbolPinSerializer::toJson(settings);
}

static bool fromJson(SymbolPinSettings& settings, const QJsonObject& json) {
    return SymbolPinSerializer::fromJson(settings, json);
}

static QJsonObject toJson(const SymbolPinDot& dot) {
    return SymbolPinSerializer::toJson(dot);
}

static bool fromJson(SymbolPinDot& dot, const QJsonObject& json) {
    return SymbolPinSerializer::fromJson(dot, json);
}

static QJsonObject toJson(const SymbolPinPath& path) {
    return SymbolPinSerializer::toJson(path);
}

static bool fromJson(SymbolPinPath& path, const QJsonObject& json) {
    return SymbolPinSerializer::fromJson(path, json);
}

static QJsonObject toJson(const SymbolPinName& name) {
    return SymbolPinSerializer::toJson(name);
}

static bool fromJson(SymbolPinName& name, const QJsonObject& json) {
    return SymbolPinSerializer::fromJson(name, json);
}

static QJsonObject toJson(const SymbolPinDotBis& dot) {
    return SymbolPinSerializer::toJson(dot);
}

static bool fromJson(SymbolPinDotBis& dot, const QJsonObject& json) {
    return SymbolPinSerializer::fromJson(dot, json);
}

static QJsonObject toJson(const SymbolPinClock& clock) {
    return SymbolPinSerializer::toJson(clock);
}

static bool fromJson(SymbolPinClock& clock, const QJsonObject& json) {
    return SymbolPinSerializer::fromJson(clock, json);
}

// Shape helpers delegate to SymbolShapeSerializer
static QJsonObject toJson(const SymbolBBox& bbox) {
    return SymbolShapeSerializer::toJson(bbox);
}

static bool fromJson(SymbolBBox& bbox, const QJsonObject& json) {
    return SymbolShapeSerializer::fromJson(bbox, json);
}

static QJsonObject toJson(const SymbolRectangle& rect) {
    return SymbolShapeSerializer::toJson(rect);
}

static bool fromJson(SymbolRectangle& rect, const QJsonObject& json) {
    return SymbolShapeSerializer::fromJson(rect, json);
}

static QJsonObject toJson(const SymbolCircle& circle) {
    return SymbolShapeSerializer::toJson(circle);
}

static bool fromJson(SymbolCircle& circle, const QJsonObject& json) {
    return SymbolShapeSerializer::fromJson(circle, json);
}

static QJsonObject toJson(const SymbolArc& arc) {
    return SymbolShapeSerializer::toJson(arc);
}

static bool fromJson(SymbolArc& arc, const QJsonObject& json) {
    return SymbolShapeSerializer::fromJson(arc, json);
}

static QJsonObject toJson(const SymbolEllipse& ellipse) {
    return SymbolShapeSerializer::toJson(ellipse);
}

static bool fromJson(SymbolEllipse& ellipse, const QJsonObject& json) {
    return SymbolShapeSerializer::fromJson(ellipse, json);
}

static QJsonObject toJson(const SymbolPolyline& polyline) {
    return SymbolShapeSerializer::toJson(polyline);
}

static bool fromJson(SymbolPolyline& polyline, const QJsonObject& json) {
    return SymbolShapeSerializer::fromJson(polyline, json);
}

static QJsonObject toJson(const SymbolPolygon& polygon) {
    return SymbolShapeSerializer::toJson(polygon);
}

static bool fromJson(SymbolPolygon& polygon, const QJsonObject& json) {
    return SymbolShapeSerializer::fromJson(polygon, json);
}

static QJsonObject toJson(const SymbolPath& path) {
    return SymbolShapeSerializer::toJson(path);
}

static bool fromJson(SymbolPath& path, const QJsonObject& json) {
    return SymbolShapeSerializer::fromJson(path, json);
}

static QJsonObject toJson(const SymbolText& text) {
    return SymbolShapeSerializer::toJson(text);
}

static bool fromJson(SymbolText& text, const QJsonObject& json) {
    return SymbolShapeSerializer::fromJson(text, json);
}

/**
 * @brief 将符号图元绘制顺序转换为 JSON。
 * @param order 待序列化的图元顺序。
 * @return 图元类型和索引组成的 JSON 对象。
 */
static QJsonObject graphicOrderToJson(const SymbolGraphicOrder& order) {
    QJsonObject json;  // 保存图元绘制顺序的 JSON 对象。
    json["type"] = order.type;
    json["index"] = order.index;
    return json;
}

/**
 * @brief 从 JSON 恢复符号图元绘制顺序。
 * @param order 接收反序列化结果的图元顺序。
 * @param json 源 JSON 对象。
 * @return 图元类型和索引有效时返回 true。
 */
static bool graphicOrderFromJson(SymbolGraphicOrder& order, const QJsonObject& json) {
    order.type = json["type"].toString();
    order.index = json["index"].toInt(-1);
    // 保留 -1 等无效索引，供 validationErrors() 报告源数据中的损坏引用，
    // 避免缓存往返时把未知或未解析的图元静默丢弃。
    return !order.type.isEmpty() && order.index >= -1;
}

/**
 * @brief 将符号引脚委托给专用序列化器。
 * @param pin 待序列化的符号引脚。
 * @return 引脚 JSON 对象。
 */
QJsonObject SymbolDataSerializer::toJson(const SymbolPin& pin) {
    return SymbolPinSerializer::toJson(pin);
}

/**
 * @brief 委托引脚序列化器恢复符号引脚及其显示属性。
 * @param pin 接收反序列化结果的符号引脚。
 * @param json 源 JSON 对象。
 * @return JSON 结构可读取时返回 true。
 */
bool SymbolDataSerializer::fromJson(SymbolPin& pin, const QJsonObject& json) {
    return SymbolPinSerializer::fromJson(pin, json);
}

// ==================== SymbolInfo ====================

/**
 * @brief 将符号基本信息序列化为 JSON。
 * @param info 待序列化的符号基本信息。
 * @return 包含符号来源和供应商字段的 JSON 对象。
 */
QJsonObject SymbolDataSerializer::toJson(const SymbolInfo& info) {
    QJsonObject json;  // 保存符号基本信息的 JSON 对象。
    json["name"] = info.name;
    json["prefix"] = info.prefix;
    json["package"] = info.package;
    json["manufacturer"] = info.manufacturer;
    json["description"] = info.description;
    json["datasheet"] = info.datasheet;
    json["lcsc_id"] = info.lcscId;
    json["jlc_id"] = info.jlcId;
    json["aliases"] = QJsonArray::fromStringList(info.aliases);

    // EasyEDA API 原始字段
    json["uuid"] = info.uuid;
    json["title"] = info.title;
    json["doc_type"] = info.docType;
    json["type"] = info.type;
    json["thumb"] = info.thumb;
    json["datastrid"] = info.datastrid;
    json["jlc_on_sale"] = info.jlcOnSale;
    json["writable"] = info.writable;
    json["is_favorite"] = info.isFavorite;
    json["verify"] = info.verify;
    json["smt"] = info.smt;

    // 时间
    json["update_time"] = info.updateTime;
    json["updated_at"] = info.updatedAt;

    // 编辑器信
    json["editor_version"] = info.editorVersion;

    // 项目信息
    json["puuid"] = info.puuid;
    json["utime"] = info.utime;
    json["import_flag"] = info.importFlag;
    json["has_id_flag"] = info.hasIdFlag;

    // 附加参数
    json["time_stamp"] = info.timeStamp;
    json["subpart_no"] = info.subpartNo;
    json["supplier_part"] = info.supplierPart;
    json["supplier"] = info.supplier;
    json["manufacturer_part"] = info.manufacturerPart;
    json["jlcpcb_part_class"] = info.jlcpcbPartClass;

    return json;
}

/**
 * @brief 从 JSON 恢复符号基本信息、来源字段和供应商字段。
 * @param info 接收反序列化结果的符号基本信息。
 * @param json 源 JSON 对象。
 * @return JSON 结构可读取时返回 true。
 */
bool SymbolDataSerializer::fromJson(SymbolInfo& info, const QJsonObject& json) {
    info.name = json["name"].toString();
    info.prefix = json["prefix"].toString();
    info.package = json["package"].toString();
    info.manufacturer = json["manufacturer"].toString();
    info.description = json["description"].toString();
    info.datasheet = json["datasheet"].toString();
    info.lcscId = json["lcsc_id"].toString();
    info.jlcId = json["jlc_id"].toString();
    info.aliases.clear();
    for (const QJsonValue& alias : json["aliases"].toArray())
        info.aliases.append(alias.toString());

    // EasyEDA API 原始字段
    info.uuid = json["uuid"].toString();
    info.title = json["title"].toString();
    info.docType = json["doc_type"].toString();
    info.type = json["type"].toString();
    info.thumb = json["thumb"].toString();
    info.datastrid = json["datastrid"].toString();
    info.jlcOnSale = json["jlc_on_sale"].toBool(false);
    info.writable = json["writable"].toBool(false);
    info.isFavorite = json["is_favorite"].toBool(false);
    info.verify = json["verify"].toBool(false);
    info.smt = json["smt"].toBool(false);

    // 时间
    info.updateTime = json["update_time"].toVariant().toLongLong();
    info.updatedAt = json["updated_at"].toString();

    // 编辑器信
    info.editorVersion = json["editor_version"].toString();

    // 项目信息
    info.puuid = json["puuid"].toString();
    info.utime = json["utime"].toVariant().toLongLong();
    info.importFlag = json["import_flag"].toBool(false);
    info.hasIdFlag = json["has_id_flag"].toBool(false);

    // 附加参数
    info.timeStamp = json["time_stamp"].toString();
    info.subpartNo = json["subpart_no"].toString();
    info.supplierPart = json["supplier_part"].toString();
    info.supplier = json["supplier"].toString();
    info.manufacturerPart = json["manufacturer_part"].toString();
    info.jlcpcbPartClass = json["jlcpcb_part_class"].toString();

    return true;
}

// ==================== SymbolPart ====================

/**
 * @brief 将多部分符号及其图元序列化为 JSON。
 * @param part 待序列化的符号部件。
 * @return 包含部件元数据、图元和绘制顺序的 JSON 对象。
 */
QJsonObject SymbolDataSerializer::toJson(const SymbolPart& part) {
    QJsonObject json;  // 保存符号部件的 JSON 对象。
    json["unit_number"] = part.unitNumber;
    json["origin_x"] = part.originX;
    json["origin_y"] = part.originY;
    json["common_to_all_parts"] = part.commonToAllParts;

    QJsonArray pinsArray;  // 符号部件引脚数组。
    for (const SymbolPin& pin : part.pins) {
        pinsArray.append(toJson(pin));
    }
    json["pins"] = pinsArray;

    QJsonArray rectanglesArray;  // 符号部件矩形数组。
    for (const SymbolRectangle& rect : part.rectangles) {
        rectanglesArray.append(toJson(rect));
    }
    json["rectangles"] = rectanglesArray;

    QJsonArray circlesArray;  // 符号部件圆数组。
    for (const SymbolCircle& circle : part.circles) {
        circlesArray.append(toJson(circle));
    }
    json["circles"] = circlesArray;

    QJsonArray arcsArray;  // 符号部件圆弧数组。
    for (const SymbolArc& arc : part.arcs) {
        arcsArray.append(toJson(arc));
    }
    json["arcs"] = arcsArray;

    QJsonArray ellipsesArray;  // 符号部件椭圆数组。
    for (const SymbolEllipse& ellipse : part.ellipses) {
        ellipsesArray.append(toJson(ellipse));
    }
    json["ellipses"] = ellipsesArray;

    QJsonArray polylinesArray;  // 符号部件折线数组。
    for (const SymbolPolyline& polyline : part.polylines) {
        polylinesArray.append(toJson(polyline));
    }
    json["polylines"] = polylinesArray;

    QJsonArray polygonsArray;  // 符号部件多边形数组。
    for (const SymbolPolygon& polygon : part.polygons) {
        polygonsArray.append(toJson(polygon));
    }
    json["polygons"] = polygonsArray;

    QJsonArray pathsArray;  // 符号部件路径数组。
    for (const SymbolPath& path : part.paths) {
        pathsArray.append(toJson(path));
    }
    json["paths"] = pathsArray;

    QJsonArray imagesArray;  // 符号部件图片数组。
    for (const SymbolImage& image : part.images)
        imagesArray.append(toJson(image));
    json["images"] = imagesArray;

    QJsonArray textsArray;  // 符号部件文本数组。
    for (const SymbolText& text : part.texts) {
        textsArray.append(toJson(text));
    }
    json["texts"] = textsArray;

    QJsonArray graphicOrderArray;  // 符号部件绘制顺序数组。
    for (const SymbolGraphicOrder& order : part.graphicOrder)
        graphicOrderArray.append(graphicOrderToJson(order));
    json["graphic_order"] = graphicOrderArray;

    return json;
}

/**
 * @brief 从 JSON 恢复多部分符号的元数据、图元集合和绘制顺序。
 * @param part 接收反序列化结果的符号部件。
 * @param json 源 JSON 对象。
 * @return JSON 结构可读取时返回 true。
 */
bool SymbolDataSerializer::fromJson(SymbolPart& part, const QJsonObject& json) {
    part.unitNumber = json["unit_number"].toInt(0);
    part.originX = json["origin_x"].toDouble(0.0);
    part.originY = json["origin_y"].toDouble(0.0);
    part.commonToAllParts = json["common_to_all_parts"].toBool(false);

    if (json.contains("pins")) {
        QJsonArray pinsArray = json["pins"].toArray();  // 引脚 JSON 数组。
        part.pins.clear();  // 清空旧引脚数据。
        for (const QJsonValue& value : pinsArray) {
            SymbolPin pin;  // 当前符号引脚。
            if (fromJson(pin, value.toObject())) {
                part.pins.append(pin);
            }
        }
    }

    if (json.contains("rectangles")) {
        QJsonArray rectanglesArray = json["rectangles"].toArray();  // 矩形 JSON 数组。
        part.rectangles.clear();
        for (const QJsonValue& value : rectanglesArray) {
            SymbolRectangle rect;  // 当前矩形。
            if (fromJson(rect, value.toObject())) {
                part.rectangles.append(rect);
            }
        }
    }

    if (json.contains("circles")) {
        QJsonArray circlesArray = json["circles"].toArray();  // 圆 JSON 数组。
        part.circles.clear();
        for (const QJsonValue& value : circlesArray) {
            SymbolCircle circle;  // 当前圆。
            if (fromJson(circle, value.toObject())) {
                part.circles.append(circle);
            }
        }
    }

    if (json.contains("arcs")) {
        QJsonArray arcsArray = json["arcs"].toArray();  // 圆弧 JSON 数组。
        part.arcs.clear();
        for (const QJsonValue& value : arcsArray) {
            SymbolArc arc;  // 当前圆弧。
            if (fromJson(arc, value.toObject())) {
                part.arcs.append(arc);
            }
        }
    }

    if (json.contains("ellipses")) {
        QJsonArray ellipsesArray = json["ellipses"].toArray();  // 椭圆 JSON 数组。
        part.ellipses.clear();
        for (const QJsonValue& value : ellipsesArray) {
            SymbolEllipse ellipse;  // 当前椭圆。
            if (fromJson(ellipse, value.toObject())) {
                part.ellipses.append(ellipse);
            }
        }
    }

    if (json.contains("polylines")) {
        QJsonArray polylinesArray = json["polylines"].toArray();  // 折线 JSON 数组。
        part.polylines.clear();
        for (const QJsonValue& value : polylinesArray) {
            SymbolPolyline polyline;  // 当前折线。
            if (fromJson(polyline, value.toObject())) {
                part.polylines.append(polyline);
            }
        }
    }

    if (json.contains("polygons")) {
        QJsonArray polygonsArray = json["polygons"].toArray();  // 多边形 JSON 数组。
        part.polygons.clear();
        for (const QJsonValue& value : polygonsArray) {
            SymbolPolygon polygon;  // 当前多边形。
            if (fromJson(polygon, value.toObject())) {
                part.polygons.append(polygon);
            }
        }
    }

    if (json.contains("paths")) {
        QJsonArray pathsArray = json["paths"].toArray();  // 路径 JSON 数组。
        part.paths.clear();
        for (const QJsonValue& value : pathsArray) {
            SymbolPath path;  // 当前路径。
            if (fromJson(path, value.toObject())) {
                part.paths.append(path);
            }
        }
    }

    if (json.contains("images")) {
        QJsonArray imagesArray = json["images"].toArray();  // 图片 JSON 数组。
        part.images.clear();
        for (const QJsonValue& value : imagesArray) {
            SymbolImage image;  // 当前图片。
            if (fromJson(image, value.toObject()))
                part.images.append(image);
        }
    }

    if (json.contains("texts")) {
        QJsonArray textsArray = json["texts"].toArray();  // 文本 JSON 数组。
        part.texts.clear();
        for (const QJsonValue& value : textsArray) {
            SymbolText text;  // 当前文本。
            if (fromJson(text, value.toObject())) {
                part.texts.append(text);
            }
        }
    }

    if (json.contains("graphic_order") && json["graphic_order"].isArray()) {
        part.graphicOrder.clear();
        for (const QJsonValue& value : json["graphic_order"].toArray()) {
            SymbolGraphicOrder order;  // 当前图元绘制顺序。
            if (value.isObject() && graphicOrderFromJson(order, value.toObject()))
                part.graphicOrder.append(order);
        }
    }

    return true;
}

// ==================== SymbolData ====================

/**
 * @brief 将完整符号数据序列化为 JSON。
 * @param data 待序列化的符号数据。
 * @return 包含符号全部部件和图元的 JSON 对象。
 */
QJsonObject SymbolDataSerializer::toJson(const SymbolData& data) {
    QJsonObject json;  // 保存完整符号数据的 JSON 对象。
    json["info"] = toJson(data.info());
    json["bbox"] = toJson(data.bbox());

    QJsonArray pinsArray;  // 符号引脚数组。
    for (const SymbolPin& pin : data.pins()) {
        pinsArray.append(toJson(pin));
    }
    json["pins"] = pinsArray;

    QJsonArray rectanglesArray;  // 符号矩形数组。
    for (const SymbolRectangle& rect : data.rectangles()) {
        rectanglesArray.append(toJson(rect));
    }
    json["rectangles"] = rectanglesArray;

    QJsonArray circlesArray;  // 符号圆数组。
    for (const SymbolCircle& circle : data.circles()) {
        circlesArray.append(toJson(circle));
    }
    json["circles"] = circlesArray;

    QJsonArray arcsArray;  // 符号圆弧数组。
    for (const SymbolArc& arc : data.arcs()) {
        arcsArray.append(toJson(arc));
    }
    json["arcs"] = arcsArray;

    QJsonArray ellipsesArray;  // 符号椭圆数组。
    for (const SymbolEllipse& ellipse : data.ellipses()) {
        ellipsesArray.append(toJson(ellipse));
    }
    json["ellipses"] = ellipsesArray;

    QJsonArray polylinesArray;  // 符号折线数组。
    for (const SymbolPolyline& polyline : data.polylines()) {
        polylinesArray.append(toJson(polyline));
    }
    json["polylines"] = polylinesArray;

    QJsonArray polygonsArray;  // 符号多边形数组。
    for (const SymbolPolygon& polygon : data.polygons()) {
        polygonsArray.append(toJson(polygon));
    }
    json["polygons"] = polygonsArray;

    QJsonArray pathsArray;  // 符号路径数组。
    for (const SymbolPath& path : data.paths()) {
        pathsArray.append(toJson(path));
    }
    json["paths"] = pathsArray;

    QJsonArray imagesArray;  // 符号图片数组。
    for (const SymbolImage& image : data.images())
        imagesArray.append(toJson(image));
    json["images"] = imagesArray;

    QJsonArray textsArray;  // 符号文本数组。
    for (const SymbolText& text : data.texts()) {
        textsArray.append(toJson(text));
    }
    json["texts"] = textsArray;

    QJsonArray graphicOrderArray;  // 符号绘制顺序数组。
    for (const SymbolGraphicOrder& order : data.graphicOrder())
        graphicOrderArray.append(graphicOrderToJson(order));
    json["graphic_order"] = graphicOrderArray;

    QJsonArray partsArray;  // 符号部件数组。
    for (const SymbolPart& part : data.parts()) {
        partsArray.append(toJson(part));
    }
    json["parts"] = partsArray;

    return json;
}

/**
 * @brief 按数据类别恢复完整符号，并保留各类图元的结构关系。
 * @param data 接收反序列化结果的符号数据。
 * @param json 源 JSON 对象。
 * @return JSON 结构可读取时返回 true。
 */
bool SymbolDataSerializer::fromJson(SymbolData& data, const QJsonObject& json) {
    // 读取信息
    if (json.contains("info") && json["info"].isObject()) {
        SymbolInfo info;  // 反序列化后的符号基本信息。
        if (!fromJson(info, json["info"].toObject())) {
            qWarning() << "Failed to parse symbol info";
            return false;
        }
        data.setInfo(info);
    }

    // 读取边界
    if (json.contains("bbox") && json["bbox"].isObject()) {
        SymbolBBox bbox;  // 反序列化后的符号边界。
        if (!fromJson(bbox, json["bbox"].toObject())) {
            qWarning() << "Failed to parse symbol bbox";
            return false;
        }
        data.setBbox(bbox);
    }

    // 读取引脚
    if (json.contains("pins") && json["pins"].isArray()) {
        QJsonArray pinsArray = json["pins"].toArray();  // 引脚 JSON 数组。
        QList<SymbolPin> pins;  // 反序列化后的引脚列表。
        for (const QJsonValue& value : pinsArray) {
            if (value.isObject()) {
                SymbolPin pin;  // 当前符号引脚。
                if (fromJson(pin, value.toObject())) {
                    pins.append(pin);
                }
            }
        }
        data.setPins(pins);
    }

    // 读取矩形
    if (json.contains("rectangles") && json["rectangles"].isArray()) {
        QJsonArray rectanglesArray = json["rectangles"].toArray();  // 矩形 JSON 数组。
        QList<SymbolRectangle> rectangles;  // 反序列化后的矩形列表。
        for (const QJsonValue& value : rectanglesArray) {
            if (value.isObject()) {
                SymbolRectangle rect;  // 当前矩形。
                if (fromJson(rect, value.toObject())) {
                    rectangles.append(rect);
                }
            }
        }
        data.setRectangles(rectangles);
    }

    // 读取
    if (json.contains("circles") && json["circles"].isArray()) {
        QJsonArray circlesArray = json["circles"].toArray();  // 圆 JSON 数组。
        QList<SymbolCircle> circles;  // 反序列化后的圆列表。
        for (const QJsonValue& value : circlesArray) {
            if (value.isObject()) {
                SymbolCircle circle;  // 当前圆。
                if (fromJson(circle, value.toObject())) {
                    circles.append(circle);
                }
            }
        }
        data.setCircles(circles);
    }

    // 读取圆弧
    if (json.contains("arcs") && json["arcs"].isArray()) {
        QJsonArray arcsArray = json["arcs"].toArray();  // 圆弧 JSON 数组。
        QList<SymbolArc> arcs;  // 反序列化后的圆弧列表。
        for (const QJsonValue& value : arcsArray) {
            if (value.isObject()) {
                SymbolArc arc;  // 当前圆弧。
                if (fromJson(arc, value.toObject())) {
                    arcs.append(arc);
                }
            }
        }
        data.setArcs(arcs);
    }

    // 读取椭圆
    if (json.contains("ellipses") && json["ellipses"].isArray()) {
        QJsonArray ellipsesArray = json["ellipses"].toArray();  // 椭圆 JSON 数组。
        QList<SymbolEllipse> ellipses;  // 反序列化后的椭圆列表。
        for (const QJsonValue& value : ellipsesArray) {
            if (value.isObject()) {
                SymbolEllipse ellipse;  // 当前椭圆。
                if (fromJson(ellipse, value.toObject())) {
                    ellipses.append(ellipse);
                }
            }
        }
        data.setEllipses(ellipses);
    }

    // 读取多段
    if (json.contains("polylines") && json["polylines"].isArray()) {
        QJsonArray polylinesArray = json["polylines"].toArray();  // 折线 JSON 数组。
        QList<SymbolPolyline> polylines;  // 反序列化后的折线列表。
        for (const QJsonValue& value : polylinesArray) {
            if (value.isObject()) {
                SymbolPolyline polyline;  // 当前折线。
                if (fromJson(polyline, value.toObject())) {
                    polylines.append(polyline);
                }
            }
        }
        data.setPolylines(polylines);
    }

    // 读取多边
    if (json.contains("polygons") && json["polygons"].isArray()) {
        QJsonArray polygonsArray = json["polygons"].toArray();  // 多边形 JSON 数组。
        QList<SymbolPolygon> polygons;  // 反序列化后的多边形列表。
        for (const QJsonValue& value : polygonsArray) {
            if (value.isObject()) {
                SymbolPolygon polygon;  // 当前多边形。
                if (fromJson(polygon, value.toObject())) {
                    polygons.append(polygon);
                }
            }
        }
        data.setPolygons(polygons);
    }

    // 读取路径
    if (json.contains("paths") && json["paths"].isArray()) {
        QJsonArray pathsArray = json["paths"].toArray();  // 路径 JSON 数组。
        QList<SymbolPath> paths;  // 反序列化后的路径列表。
        for (const QJsonValue& value : pathsArray) {
            if (value.isObject()) {
                SymbolPath path;  // 当前路径。
                if (fromJson(path, value.toObject())) {
                    paths.append(path);
                }
            }
        }
        data.setPaths(paths);
    }

    // 读取图片
    if (json.contains("images") && json["images"].isArray()) {
        QList<SymbolImage> images;  // 反序列化后的图片列表。
        for (const QJsonValue& value : json["images"].toArray()) {
            if (value.isObject()) {
                SymbolImage image;  // 当前图片。
                if (fromJson(image, value.toObject()))
                    images.append(image);
            }
        }
        data.setImages(images);
    }

    // 读取文本
    if (json.contains("texts") && json["texts"].isArray()) {
        QJsonArray textsArray = json["texts"].toArray();  // 文本 JSON 数组。
        QList<SymbolText> texts;  // 反序列化后的文本列表。
        for (const QJsonValue& value : textsArray) {
            if (value.isObject()) {
                SymbolText text;  // 当前文本。
                if (fromJson(text, value.toObject())) {
                    texts.append(text);
                }
            }
        }
        data.setTexts(texts);
    }

    if (json.contains("graphic_order") && json["graphic_order"].isArray()) {
        QList<SymbolGraphicOrder> graphicOrder;  // 反序列化后的绘制顺序列表。
        for (const QJsonValue& value : json["graphic_order"].toArray()) {
            SymbolGraphicOrder order;  // 当前图元绘制顺序。
            if (value.isObject() && graphicOrderFromJson(order, value.toObject()))
                graphicOrder.append(order);
        }
        data.setGraphicOrder(graphicOrder);
    }

    // 读取多部分符号的部分
    if (json.contains("parts") && json["parts"].isArray()) {
        QJsonArray partsArray = json["parts"].toArray();  // 部件 JSON 数组。
        QList<SymbolPart> parts;  // 反序列化后的部件列表。
        for (const QJsonValue& value : partsArray) {
            if (value.isObject()) {
                SymbolPart part;  // 当前符号部件。
                if (fromJson(part, value.toObject())) {
                    parts.append(part);
                }
            }
        }
        data.setParts(parts);
    }

    return true;
}

}  // namespace EasyKiConverter
