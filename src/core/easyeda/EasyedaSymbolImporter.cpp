/**
 * @file EasyedaSymbolImporter.cpp
 * @brief EasyedaSymbolImporter 的实现。
 */
#include "EasyedaSymbolImporter.h"

#include "EasyedaUtils.h"
#include "SvgPathParser.h"

#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QUrl>

namespace EasyKiConverter {

EasyedaSymbolImporter::EasyedaSymbolImporter() {}

/**
 * @brief 从 EasyEDA CAD JSON 导入符号元数据、单部分或多部分图元及原始顺序。
 * @param cadData EasyEDA CAD JSON 对象。
 * @return 导入后的符号数据对象。
 */
QSharedPointer<SymbolData> EasyedaSymbolImporter::importSymbolData(const QJsonObject& cadData) {
    auto symbolData = QSharedPointer<SymbolData>::create();  // 待填充的符号数据对象。

    SymbolInfo info;  // 待写入符号元数据的结构体。
    info.uuid = cadData["uuid"].toString();
    info.title = cadData["title"].toString();
    info.description = cadData["description"].toString();
    info.docType = QString::number(cadData["docType"].toInt());
    info.type = QString::number(cadData["type"].toInt());
    info.thumb = cadData["thumb"].toString();
    info.datastrid = cadData["datastrid"].toString();
    info.jlcOnSale = cadData["jlcOnSale"].toBool(false);
    info.writable = cadData["writable"].toBool(false);
    info.isFavorite = cadData["isFavorite"].toBool(false);
    info.verify = cadData["verify"].toBool(false);
    info.smt = cadData["SMT"].toBool(false);
    info.updateTime = cadData["updateTime"].toVariant().toLongLong();
    info.updatedAt = cadData["updated_at"].toString();

    // 兼容 API 返回的 aliases 数组或逗号分隔字符串。
    const QJsonValue aliasesValue = cadData.value(QStringLiteral("aliases"));
    if (aliasesValue.isArray()) {
        for (const QJsonValue& alias : aliasesValue.toArray())
            if (!alias.toString().trimmed().isEmpty())
                info.aliases.append(alias.toString().trimmed());
    } else if (aliasesValue.isString()) {
        for (const QString& alias : aliasesValue.toString().split(',', Qt::SkipEmptyParts))
            if (!alias.trimmed().isEmpty())
                info.aliases.append(alias.trimmed());
    }

    // 导入符号信息（从 dataStr.head.c_para 中获取）
    if (cadData.contains("dataStr")) {
        QJsonObject dataStr = cadData["dataStr"].toObject();  // 符号详细数据对象。
        if (dataStr.contains("head")) {
            QJsonObject head = dataStr["head"].toObject();  // 符号头部元数据对象。

            info.editorVersion = head["editorVersion"].toString();

            // 项目信息
            info.puuid = head["puuid"].toString();
            info.utime = head["utime"].toVariant().toLongLong();
            info.importFlag = head["importFlag"].toBool(false);
            info.hasIdFlag = head["hasIdFlag"].toBool(false);

            if (head.contains("c_para")) {
                QJsonObject c_para = head["c_para"].toObject();  // 符号附加参数。
                info.name = c_para["name"].toString();
                info.prefix = c_para["pre"].toString();
                info.package = c_para["package"].toString();
                info.manufacturer = c_para["Manufacturer"].toString();
                info.lcscId = c_para["Supplier Part"].toString();
                info.jlcId = c_para["JLCPCB Part Class"].toString();

                // 附加参数
                info.timeStamp = c_para["timeStamp"].toString();
                info.subpartNo = c_para["subpart_no"].toString();
                info.supplierPart = c_para["Supplier Part"].toString();
                info.supplier = c_para["Supplier"].toString();
                info.manufacturerPart = c_para["Manufacturer Part"].toString();
                info.jlcpcbPartClass = c_para["JLCPCB Part Class"].toString();
                const QString alias = c_para.value(QStringLiteral("Alias")).toString().trimmed();  // 附加的符号别名。
                if (!alias.isEmpty() && !info.aliases.contains(alias))
                    info.aliases.append(alias);
            }
        }

        if (cadData.contains("lcsc")) {
            QJsonObject lcsc = cadData["lcsc"].toObject();  // LCSC 元件关联信息。
            info.datasheet = lcsc["url"].toString();
        }
    }

    symbolData->setInfo(info);

    if (cadData.contains("dataStr")) {
        QJsonObject dataStr = cadData["dataStr"].toObject();  // 用于读取边界框的详细数据对象。
        SymbolBBox symbolBbox;  // 待导入的符号边界框。
        if (dataStr.contains("BBox")) {
            QJsonObject bbox = dataStr["BBox"].toObject();  // EasyEDA 提供的边界框对象。
            symbolBbox.x = bbox["x"].toDouble();
            symbolBbox.y = bbox["y"].toDouble();
            symbolBbox.width = bbox["width"].toDouble();
            symbolBbox.height = bbox["height"].toDouble();
        }
        if (dataStr.contains("head")) {
            QJsonObject head = dataStr["head"].toObject();  // 用于回退中心坐标的头部对象。
            if (head.contains("x") && head.contains("y")) {
                symbolBbox.headX = head["x"].toDouble();
                symbolBbox.headY = head["y"].toDouble();
                symbolBbox.hasHeadCenter = true;
            }
            if (symbolBbox.width <= 0.0 || symbolBbox.height <= 0.0) {
                symbolBbox.x = symbolBbox.headX;
                symbolBbox.y = symbolBbox.headY;
                qWarning() << "Symbol BBox not found in dataStr, using head.x/y as center point";
            }
        }
        symbolData->setBbox(symbolBbox);
    }

    qDebug() << "=== EasyedaImporter::importSymbolData - Starting geometry data import ===";
    if (cadData.contains("dataStr")) {
        QJsonObject dataStr = cadData["dataStr"].toObject();  // 符号几何详细数据对象。
        qDebug() << "dataStr found, keys:" << dataStr.keys();

        qDebug() << "Checking for subparts field in cadData...";
        qDebug() << "cadData contains subparts:" << cadData.contains("subparts");

        // 检查是否有子部分（多部分符号）
        if (cadData.contains("subparts") && cadData["subparts"].isArray()) {
            QJsonArray subparts = cadData["subparts"].toArray();  // 多部分符号的子部分数组。
            qDebug() << "Found subparts field in cadData, size:" << subparts.size();

            // 如果 subparts 数组为空，则按单部分符号处理
            if (subparts.isEmpty()) {
                qDebug() << "Subparts array is empty, treating as single-part symbol";
            } else {
                qDebug() << "Found" << subparts.size() << "subparts in symbol";

                for (int i = 0; i < subparts.size(); ++i) {
                    QJsonObject subpart = subparts[i].toObject();  // 当前子部分对象。
                    if (subpart.contains("dataStr")) {
                        QJsonObject subpartDataStr = subpart["dataStr"].toObject();  // 当前子部分的详细数据。

                        SymbolPart part;  // 当前子部分的导入结果。
                        part.unitNumber = i;
                        part.commonToAllParts = subpart.value(QStringLiteral("commonToAllParts")).toBool(false) ||
                                                subpart.value(QStringLiteral("common_to_all_parts")).toBool(false) ||
                                                subpart.value(QStringLiteral("isCommon")).toBool(false) ||
                                                subpart.value(QStringLiteral("is_common")).toBool(false);

                        // 设置子部分的坐标原点（从 EasyEDA head 字段）
                        if (subpartDataStr.contains("head")) {
                            QJsonObject head = subpartDataStr["head"].toObject();  // 子部分头部坐标对象。
                            part.originX = head["x"].toDouble(0.0);
                            part.originY = head["y"].toDouble(0.0);
                            qDebug() << "Subpart" << i << "origin: (" << part.originX << "," << part.originY << ")";
                        }

                        qDebug() << "Importing subpart" << i << "from" << subpart["title"].toString();

                        // 导入子部分的图形元素
                        if (subpartDataStr.contains("shape")) {
                            QJsonArray shapes = subpartDataStr["shape"].toArray();  // 子部分图元记录数组。

                            for (const QJsonValue& shapeValue : shapes) {
                                QString shapeString = shapeValue.toString();  // 当前子部分图元字符串。
                                QStringList parts = shapeString.split("~");  // 拆分后的图元字段。

                                if (parts.isEmpty())
                                    continue;

                                QString designator = parts[0];  // 当前图元类型标识。

                                if (designator == "P") {
                                    // 导入引脚
                                    SymbolPin pin = importPinData(shapeString);  // 解析后的引脚图元。
                                    part.graphicOrder.append({designator, static_cast<int>(part.pins.size())});
                                    part.pins.append(pin);
                                } else if (designator == "R") {
                                    // 导入矩形
                                    SymbolRectangle rectangle = importRectangleData(shapeString);  // 解析后的矩形图元。
                                    part.graphicOrder.append({designator, static_cast<int>(part.rectangles.size())});
                                    part.rectangles.append(rectangle);
                                } else if (designator == "C") {
                                    SymbolCircle circle = importCircleData(shapeString);  // 解析后的圆形图元。
                                    part.graphicOrder.append({designator, static_cast<int>(part.circles.size())});
                                    part.circles.append(circle);
                                } else if (designator == "A") {
                                    SymbolArc arc = importArcData(shapeString);  // 解析后的圆弧图元。
                                    part.graphicOrder.append({designator, static_cast<int>(part.arcs.size())});
                                    part.arcs.append(arc);
                                } else if (designator == "PL") {
                                    SymbolPolyline polyline = importPolylineData(shapeString);  // 解析后的折线图元。
                                    part.graphicOrder.append({designator, static_cast<int>(part.polylines.size())});
                                    part.polylines.append(polyline);
                                } else if (designator == "PG") {
                                    SymbolPolygon polygon = importPolygonData(shapeString);  // 解析后的多边形图元。
                                    part.graphicOrder.append({designator, static_cast<int>(part.polygons.size())});
                                    part.polygons.append(polygon);
                                } else if (designator == "PT") {
                                    SymbolPath path = importPathData(shapeString);  // 解析后的路径图元。
                                    part.graphicOrder.append({designator, static_cast<int>(part.paths.size())});
                                    part.paths.append(path);
                                } else if (designator == "I") {
                                    SymbolImage image = importImageData(shapeString);  // 解析后的图片图元。
                                    part.graphicOrder.append({designator, static_cast<int>(part.images.size())});
                                    part.images.append(image);
                                } else if (designator == "T") {
                                    SymbolText text = importTextData(shapeString);  // 解析后的文本图元。
                                    part.graphicOrder.append({designator, static_cast<int>(part.texts.size())});
                                    part.texts.append(text);
                                } else if (designator == "E") {
                                    SymbolEllipse ellipse = importEllipseData(shapeString);  // 解析后的椭圆图元。
                                    part.graphicOrder.append({designator, static_cast<int>(part.ellipses.size())});
                                    part.ellipses.append(ellipse);
                                } else {
                                    const QString diagnostic =
                                        QStringLiteral("EasyEDA 符号 Part %1 包含不支持的图元类型 %2")
                                            .arg(i)
                                            .arg(designator);
                                    qWarning().noquote() << diagnostic;
                                    // 保留无效顺序引用，使后续 validationErrors() 和导出诊断能够指出
                                    // 被忽略的源图元，而不是让它静默消失。
                                    part.graphicOrder.append({designator, -1});
                                }
                            }
                        }

                        symbolData->addPart(part);
                        qDebug() << "Imported subpart" << i << "with" << part.pins.size() << "pins,"
                                 << part.rectangles.size() << "rectangles";
                    }
                }

                if (!subparts.isEmpty()) {
                    qDebug() << "Multi-part symbol processed, skipping single-part symbol processing";
                    return symbolData;
                }
            }
        } else {
            qDebug() << "No subparts field found in cadData, processing as single-part symbol";
        }

        // 单部分符号：导入主符号的图形元素
        qDebug() << "=== Starting single-part symbol processing ===";
        if (dataStr.contains("shape")) {
            QJsonArray shapes = dataStr["shape"].toArray();  // 单部分符号图元记录数组。
            qDebug() << "=== Symbol Shape Parsing ===";
            qDebug() << "Total shapes in dataStr.shape:" << shapes.size();

            for (const QJsonValue& shapeValue : shapes) {
                QString shapeString = shapeValue.toString();  // 当前单部分图元字符串。
                QStringList parts = shapeString.split("~");  // 拆分后的图元字段。

                if (parts.isEmpty())
                    continue;

                QString designator = parts[0];  // 当前图元类型标识。
                qDebug() << "Processing shape with designator:" << designator;

                if (designator == "P") {
                    qDebug() << "  -> Processing pin data...";
                    // 导入引脚
                    SymbolPin pin = importPinData(shapeString);
                    qDebug() << "  -> Pin parsed, name:" << pin.name.text;
                    symbolData->addGraphicOrder({designator, static_cast<int>(symbolData->pins().size())});
                    symbolData->addPin(pin);
                    qDebug() << "  -> Added pin:" << pin.name.text;
                } else if (designator == "R") {
                    // 导入矩形
                    SymbolRectangle rectangle = importRectangleData(shapeString);
                    symbolData->addGraphicOrder({designator, static_cast<int>(symbolData->rectangles().size())});
                    symbolData->addRectangle(rectangle);
                    qDebug() << "  -> Added rectangle";
                } else if (designator == "C") {
                    SymbolCircle circle = importCircleData(shapeString);  // 解析后的圆形图元。
                    symbolData->addGraphicOrder({designator, static_cast<int>(symbolData->circles().size())});
                    symbolData->addCircle(circle);
                    qDebug() << "  -> Added circle";
                } else if (designator == "A") {
                    SymbolArc arc = importArcData(shapeString);  // 解析后的圆弧图元。
                    symbolData->addGraphicOrder({designator, static_cast<int>(symbolData->arcs().size())});
                    symbolData->addArc(arc);
                    qDebug() << "  -> Added arc";
                } else if (designator == "PL") {
                    SymbolPolyline polyline = importPolylineData(shapeString);  // 解析后的折线图元。
                    symbolData->addGraphicOrder({designator, static_cast<int>(symbolData->polylines().size())});
                    symbolData->addPolyline(polyline);
                    qDebug() << "  -> Added polyline";
                } else if (designator == "PG") {
                    SymbolPolygon polygon = importPolygonData(shapeString);  // 解析后的多边形图元。
                    symbolData->addGraphicOrder({designator, static_cast<int>(symbolData->polygons().size())});
                    symbolData->addPolygon(polygon);
                    qDebug() << "  -> Added polygon";
                } else if (designator == "PT") {
                    SymbolPath path = importPathData(shapeString);  // 解析后的路径图元。
                    symbolData->addGraphicOrder({designator, static_cast<int>(symbolData->paths().size())});
                    symbolData->addPath(path);
                    qDebug() << "  -> Added path";
                } else if (designator == "I") {
                    SymbolImage image = importImageData(shapeString);  // 解析后的图片图元。
                    symbolData->addGraphicOrder({designator, static_cast<int>(symbolData->images().size())});
                    symbolData->addImage(image);
                    qDebug() << "  -> Added image";
                } else if (designator == "T") {
                    SymbolText text = importTextData(shapeString);  // 解析后的文本图元。
                    symbolData->addGraphicOrder({designator, static_cast<int>(symbolData->texts().size())});
                    symbolData->addText(text);
                    qDebug() << "  -> Added text:" << text.text;
                } else if (designator == "E") {
                    SymbolEllipse ellipse = importEllipseData(shapeString);  // 解析后的椭圆图元。
                    symbolData->addGraphicOrder({designator, static_cast<int>(symbolData->ellipses().size())});
                    symbolData->addEllipse(ellipse);
                    qDebug() << "  -> Added ellipse";
                } else {
                    // 将不支持图元保留为诊断，避免导入结果静默丢失源记录。
                    const QString diagnostic = QStringLiteral("EasyEDA 符号包含不支持的图元类型 %1").arg(designator);
                    qWarning().noquote() << diagnostic;
                    // 保留无效顺序引用，使后续 validationErrors() 和导出诊断能够指出
                    // 被忽略的源图元，而不是让它静默消失。
                    symbolData->addGraphicOrder({designator, -1});
                }
            }

            qDebug() << "Symbol shape parsing completed";
            qDebug() << "Final counts - Pins:" << symbolData->pins().size()
                     << "Rectangles:" << symbolData->rectangles().size() << "Circles:" << symbolData->circles().size();
            qDebug() << "============================";
        } else {
            qDebug() << "WARNING: dataStr does not contain 'shape' field!";
            qDebug() << "Available keys in dataStr:" << dataStr.keys();
        }
    }

    return symbolData;
}

/**
 * @brief 解析引脚记录中的名称、编号、方向、电气类型和显示属性。
 * @param pinData EasyEDA 引脚记录字符串。
 * @return 解析后的符号引脚。
 */
SymbolPin EasyedaSymbolImporter::importPinData(const QString& pinData) {
    SymbolPin pin;  // 待填充的符号引脚数据。
    QList<QStringList> segments = parsePinDataString(pinData);  // 按段拆分后的引脚记录。

    if (segments.size() >= 7) {
        QStringList settings = segments[0];  // 引脚主设置字段。
        if (settings.size() >= 8) {
            pin.settings.isDisplayed = (settings[1] == "show");
            pin.settings.type = static_cast<PinType>(settings[2].toInt());
            pin.settings.spicePinNumber = settings[3];
            pin.settings.posX = settings[4].toDouble();
            pin.settings.posY = settings[5].toDouble();
            pin.settings.rotation = settings[6].toInt();
            pin.settings.id = settings[7];
            pin.settings.isLocked = (settings.size() > 8 ? EasyedaUtils::stringToBool(settings[8]) : false);
        }

        if (segments[1].size() >= 2) {
            pin.pinDot.dotX = segments[1][0].toDouble();
            pin.pinDot.dotY = segments[1][1].toDouble();
        }

        if (segments[2].size() >= 2) {
            pin.pinPath.path = segments[2][0];
            pin.pinPath.color = segments[2][1];
        }

        if (segments[3].size() >= 8) {
            pin.name.isDisplayed = (segments[3][0] == "show");
            pin.name.posX = segments[3][1].toDouble();
            pin.name.posY = segments[3][2].toDouble();
            pin.name.rotation = segments[3][3].toInt();
            pin.name.text = segments[3][4];
            pin.name.textAnchor = segments[3][5];
            pin.name.font = segments[3][6];
            pin.name.fontSize = segments[3][7].toDouble();
        }

        if (segments[4].size() >= 5) {
            qDebug() << "Pin Segment 4 data:" << segments[4];
            qDebug() << "  Segment 4[0] (dotShow):" << segments[4][0];

            pin.dot.isDisplayed = false;

            pin.dot.circleX = segments[4][1].toDouble();
            pin.dot.circleY = segments[4][2].toDouble();
            // Segment 4 的首字段描述 Pin Dot 状态，不直接等同于 Pin Number 显示状态。
            // 只有存在编号文本时才建立可导出的编号文本元数据。
            pin.number.isDisplayed = false;
            pin.number.posX = pin.dot.circleX;
            pin.number.posY = pin.dot.circleY;
            pin.number.rotation = pin.settings.rotation;
            pin.number.textAnchor = QStringLiteral("middle");

            qDebug() << "  dot.isDisplayed set to: false (always hide)";

            QString pinNumberDisplayText = segments[4][4];  // EasyEDA 提供的引脚编号显示文本。
            if (!pinNumberDisplayText.isEmpty()) {
                pin.settings.spicePinNumber = pinNumberDisplayText;
                pin.number.text = pinNumberDisplayText;
                pin.number.isDisplayed = true;
                qDebug() << "Pin Number Display Text extracted from Segment 4:" << pinNumberDisplayText << "for pin"
                         << pin.name.text << "(replaced spicePinNumber)";
            }
        }

        if (segments[6].size() >= 2) {
            pin.clock.isDisplayed = (segments[6][0] == "show");
            pin.clock.path = segments[6][1];
        }
    }

    return pin;
}

/**
 * @brief 解析符号矩形图元的边界、线宽、填充和部件归属。
 * @param rectangleData EasyEDA 矩形图元记录字符串。
 * @return 解析后的矩形图元。
 */
SymbolRectangle EasyedaSymbolImporter::importRectangleData(const QString& rectangleData) {
    SymbolRectangle rectangle;  // 待填充的矩形图元。
    QStringList fields = EasyedaUtils::parseDataString(rectangleData);  // 拆分后的矩形字段。

    if (fields.size() >= 13) {
        rectangle.posX = fields[1].toDouble();
        rectangle.posY = fields[2].toDouble();
        rectangle.rx = fields[3].isEmpty() ? 0.0 : fields[3].toDouble();
        rectangle.ry = fields[4].isEmpty() ? 0.0 : fields[4].toDouble();
        rectangle.width = fields[5].toDouble();
        rectangle.height = fields[6].toDouble();
        rectangle.strokeColor = fields[7];
        rectangle.strokeWidth = fields[8].toDouble();
        rectangle.strokeStyle = fields[9];
        rectangle.fillColor = fields[10];
        rectangle.id = fields[11];
        rectangle.isLocked = EasyedaUtils::stringToBool(fields[12]);
    }

    return rectangle;
}

/**
 * @brief 解析符号圆形图元的圆心、半径和绘制属性。
 * @param circleData EasyEDA 圆形图元记录字符串。
 * @return 解析后的圆形图元。
 */
SymbolCircle EasyedaSymbolImporter::importCircleData(const QString& circleData) {
    SymbolCircle circle;  // 待填充的圆形图元。
    QStringList fields = EasyedaUtils::parseDataString(circleData);  // 拆分后的圆形字段。

    if (fields.size() >= 10) {
        circle.centerX = fields[0].toDouble();
        circle.centerY = fields[1].toDouble();
        circle.radius = fields[2].toDouble();
        circle.strokeColor = fields[3];
        circle.strokeWidth = fields[4].toDouble();
        circle.strokeStyle = fields[5];
        circle.fillColor = EasyedaUtils::stringToBool(fields[6]);
        circle.id = fields[7];
        circle.isLocked = EasyedaUtils::stringToBool(fields[8]);
    }

    return circle;
}

/**
 * @brief 解析符号弧线图元的圆心、半径、角度和绘制属性。
 * @param arcData EasyEDA 圆弧图元记录字符串。
 * @return 解析后的圆弧图元。
 */
SymbolArc EasyedaSymbolImporter::importArcData(const QString& arcData) {
    SymbolArc arc;  // 待填充的圆弧图元。
    QStringList fields = EasyedaUtils::parseDataString(arcData);  // 拆分后的圆弧字段。

    if (fields.size() >= 8) {
        arc.helperDots = fields[1];
        arc.strokeColor = fields[2];
        arc.strokeWidth = fields[3].toDouble();
        arc.strokeStyle = fields[4];
        arc.fillColor = EasyedaUtils::stringToBool(fields[5]);
        arc.id = fields[6];
        arc.isLocked = EasyedaUtils::stringToBool(fields[7]);

        // 解析 helperDots 中的 SVG 弧线路径，获取完整路径点
        // 格式如: "M 401 313 A 3 3 0 0 0 407 313"
        if (!arc.helperDots.isEmpty()) {
            QString pathStr = arc.helperDots.trimmed();  // 清洗后的 SVG 弧线路径字符串。
            // 使用 SvgPathParser 解析完整的 SVG 路径（包括弧线的所有点）
            QList<QPointF> parsedPoints = SvgPathParser::parsePath(pathStr);  // 解析后的弧线路径点。
            if (!parsedPoints.isEmpty()) {
                arc.path = parsedPoints;
            }
        }
    }

    return arc;
}

/**
 * @brief 解析符号椭圆图元的中心、半径、角度和部件归属。
 * @param ellipseData EasyEDA 椭圆图元记录字符串。
 * @return 解析后的椭圆图元。
 */
SymbolEllipse EasyedaSymbolImporter::importEllipseData(const QString& ellipseData) {
    SymbolEllipse ellipse;  // 待填充的椭圆图元。
    QStringList fields = EasyedaUtils::parseDataString(ellipseData);  // 拆分后的椭圆字段。

    if (fields.size() >= 10) {
        ellipse.centerX = fields[1].toDouble();
        ellipse.centerY = fields[2].toDouble();
        ellipse.radiusX = fields[3].toDouble();
        ellipse.radiusY = fields[4].toDouble();
        ellipse.strokeColor = fields[5];
        ellipse.strokeWidth = fields[6].toDouble();
        ellipse.strokeStyle = fields[7];
        ellipse.fillColor = EasyedaUtils::stringToBool(fields[8]);
        ellipse.id = fields[9];
        ellipse.isLocked = fields.size() > 10 ? EasyedaUtils::stringToBool(fields[10]) : false;
    }

    return ellipse;
}

/**
 * @brief 解析符号折线图元的顶点、线宽、样式和填充属性。
 * @param polylineData EasyEDA 折线图元记录字符串。
 * @return 解析后的折线图元。
 */
SymbolPolyline EasyedaSymbolImporter::importPolylineData(const QString& polylineData) {
    SymbolPolyline polyline;  // 待填充的折线图元。
    QStringList fields = EasyedaUtils::parseDataString(polylineData);  // 拆分后的折线字段。

    if (fields.size() >= 8) {
        polyline.points = fields[1];
        polyline.strokeColor = fields[2];
        polyline.strokeWidth = fields[3].toDouble();
        polyline.strokeStyle = fields[4];
        polyline.fillColor = EasyedaUtils::stringToBool(fields[5]);
        polyline.id = fields[6];
        polyline.isLocked = fields.size() > 7 ? EasyedaUtils::stringToBool(fields[7]) : false;
    }

    return polyline;
}

/**
 * @brief 解析符号多边形图元的顶点、线型、颜色和部件归属。
 * @param polygonData EasyEDA 多边形图元记录字符串。
 * @return 解析后的多边形图元。
 */
SymbolPolygon EasyedaSymbolImporter::importPolygonData(const QString& polygonData) {
    SymbolPolygon polygon;  // 待填充的多边形图元。
    QStringList fields = EasyedaUtils::parseDataString(polygonData);  // 拆分后的多边形字段。

    if (fields.size() >= 8) {
        polygon.points = fields[1];
        polygon.strokeColor = fields[2];
        polygon.strokeWidth = fields[3].toDouble();
        polygon.strokeStyle = fields[4];
        polygon.fillColor = EasyedaUtils::stringToBool(fields[5]);
        polygon.id = fields[6];
        polygon.isLocked = fields.size() > 7 ? EasyedaUtils::stringToBool(fields[7]) : false;
    }

    return polygon;
}

/**
 * @brief 解析 SVG 路径图元并保留其原始路径字符串和绘制属性。
 * @param pathData EasyEDA 路径图元记录字符串。
 * @return 解析后的路径图元。
 */
SymbolPath EasyedaSymbolImporter::importPathData(const QString& pathData) {
    SymbolPath path;  // 待填充的路径图元。
    QStringList fields = EasyedaUtils::parseDataString(pathData);  // 拆分后的路径字段。

    if (fields.size() >= 8) {
        path.paths = fields[1];
        path.strokeColor = fields[2];
        path.strokeWidth = fields[3].toDouble();
        path.strokeStyle = fields[4];
        path.fillColor = EasyedaUtils::stringToBool(fields[5]);
        path.id = fields[6];
        path.isLocked = fields.size() > 7 ? EasyedaUtils::stringToBool(fields[7]) : false;
    }

    return path;
}

/**
 * @brief 解析符号图片图元的位置、边界、数据 URL 和显示属性。
 * @param imageData EasyEDA 图片图元记录字符串。
 * @return 解析后的图片图元。
 */
SymbolImage EasyedaSymbolImporter::importImageData(const QString& imageData) {
    SymbolImage image;  // 待填充的图片图元。
    const QStringList fields = EasyedaUtils::parseDataString(imageData);  // 拆分后的图片字段。
    if (fields.size() < 7)
        return image;

    image.posX = fields[1].toDouble();
    image.posY = fields[2].toDouble();
    image.width = fields[3].toDouble();
    image.height = fields[4].toDouble();
    image.rotation = fields[5].toDouble();
    image.source = fields[6];
    image.isLocked = fields.size() > 7 ? EasyedaUtils::stringToBool(fields[7]) : false;

    if (image.source.startsWith(QStringLiteral("data:"), Qt::CaseInsensitive)) {
        const int comma = image.source.indexOf(',');  // data URL 元数据与负载之间的分隔位置。
        if (comma > 5) {
            const QString metadata = image.source.mid(5, comma - 5);  // data URL 的媒体类型和编码声明。
            const QByteArray payload = image.source.mid(comma + 1).toUtf8();  // data URL 的编码负载。
            if (metadata.contains(QStringLiteral(";base64"), Qt::CaseInsensitive))
                image.data = QByteArray::fromBase64(payload);
            else
                image.data = QUrl::fromPercentEncoding(payload).toUtf8();

            const QString mimeType = metadata.section(';', 0, 0).toLower();  // 图片媒体类型。
            QString extension;  // 根据媒体类型推导的文件扩展名。
            if (mimeType == QStringLiteral("image/svg+xml"))
                extension = QStringLiteral("svg");
            else if (mimeType.startsWith(QStringLiteral("image/")))
                extension = mimeType.mid(6);
            image.fileName =
                extension.isEmpty() ? QStringLiteral("image.bin") : QStringLiteral("image.%1").arg(extension);
        }
    } else {
        image.fileName = QUrl(image.source).fileName();
        if (image.fileName.isEmpty())
            image.fileName = image.source;
    }
    return image;
}

/**
 * @brief 解析符号文本图元的内容、字体、位置、方向和显示属性。
 * @param textData EasyEDA 文本图元记录字符串。
 * @return 解析后的文本图元。
 */
SymbolText EasyedaSymbolImporter::importTextData(const QString& textData) {
    SymbolText text;  // 待填充的文本图元。
    QStringList fields = EasyedaUtils::parseDataString(textData);  // 拆分后的文本字段。

    // EasyEDA T 记录布局：字段 10 为基线，字段 11 为文本类型，
    // 字段 12 才是实际显示文本；旧实现将字段 11 误写入 text，
    // 导致所有符号文本在 Altium 中显示为 "comment"。
    if (fields.size() >= 16) {
        text.mark = fields[0];
        text.posX = fields[1].toDouble();
        text.posY = fields[2].toDouble();
        text.rotation = fields[3].toInt();
        text.color = fields[4];
        text.font = fields[5];
        text.textSize = fields[6].toDouble();
        text.bold = EasyedaUtils::stringToBool(fields[7]);
        text.italic = fields[8];
        text.baseline = fields[10];
        text.type = fields[11];
        text.text = fields[12];
        text.visible = EasyedaUtils::stringToBool(fields[13]);
        text.anchor = fields[14];
        text.id = fields[15];
        text.isLocked = fields.size() > 16 ? EasyedaUtils::stringToBool(fields[16]) : false;
    }

    return text;
}

/**
 * @brief 将引脚复合字段拆分为可独立读取的键值列表，并保留字段顺序。
 * @param pinData EasyEDA 引脚复合字段字符串。
 * @return 按复合字段和字段分隔符拆分后的列表。
 */
QList<QStringList> EasyedaSymbolImporter::parsePinDataString(const QString& pinData) const {
    QList<QStringList> result;  // 拆分后的引脚字段列表。
    QStringList segments;  // 由双插入符分隔的引脚段列表。
    int start = 0;  // 当前字段段的起始位置。
    int pos = 0;  // 当前扫描位置。

    while (pos < pinData.length()) {
        if (pinData[pos] == '^' && pos + 1 < pinData.length() && pinData[pos + 1] == '^') {
            segments.append(pinData.mid(start, pos - start));
            pos += 2;
            start = pos;
            continue;
        }

        pos++;
    }

    if (start < pinData.length()) {
        segments.append(pinData.mid(start));
    }

    for (const QString& segment : segments) {
        QStringList subSegments = segment.split("~", Qt::KeepEmptyParts);  // 当前引脚段的子字段列表。
        result.append(subSegments);
    }

    return result;
}

}  // namespace EasyKiConverter
