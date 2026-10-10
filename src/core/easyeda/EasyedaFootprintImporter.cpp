#include "EasyedaFootprintImporter.h"

#include "EasyedaUtils.h"

#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace EasyKiConverter {

EasyedaFootprintImporter::EasyedaFootprintImporter() {}

/**
 * @brief 从 EasyEDA CAD JSON 导入封装元数据、图元、层定义和三维模型关联。
 * @param cadData EasyEDA CAD JSON 对象。
 * @return 导入后的封装数据对象。
 */
QSharedPointer<FootprintData> EasyedaFootprintImporter::importFootprintData(const QJsonObject& cadData) {
    auto footprintData = QSharedPointer<FootprintData>::create();  // 待填充的封装数据对象。

    // 导入封装信息（从 packageDetail 中获取）
    if (cadData.contains("packageDetail")) {
        QJsonObject packageDetail = cadData["packageDetail"].toObject();  // EasyEDA 封装详情对象。

        // packageDetail 顶层字段
        FootprintInfo info;  // 待写入封装元数据的结构体。
        info.uuid = packageDetail["uuid"].toString();
        info.docType = QString::number(packageDetail["docType"].toInt());
        info.datastrid = packageDetail["datastrid"].toString();
        info.writable = packageDetail["writable"].toBool(false);
        info.updateTime = packageDetail["updateTime"].toVariant().toLongLong();

        if (packageDetail.contains("dataStr")) {
            QJsonObject dataStr = packageDetail["dataStr"].toObject();  // 封装详细数据对象。

            if (dataStr.contains("head")) {
                QJsonObject head = dataStr["head"].toObject();  // 封装头部元数据对象。

                info.editorVersion = head["editorVersion"].toString();

                info.puuid = head["puuid"].toString();
                info.utime = head["utime"].toVariant().toLongLong();
                info.importFlag = head["importFlag"].toBool(false);
                info.hasIdFlag = head["hasIdFlag"].toBool(false);
                info.newgId = head["newgId"].toBool(false);

                if (head.contains("c_para")) {
                    QJsonObject c_para = head["c_para"].toObject();  // EasyEDA 封装附加参数。
                    info.name = c_para["package"].toString();

                    bool isSmd = cadData.contains("SMT") && cadData["SMT"].toBool();  // 是否为表面贴装封装。
                    info.type = isSmd ? "smd" : "tht";

                    if (c_para.contains("3DModel")) {
                        info.model3DName = c_para["3DModel"].toString();
                    }

                    // 附加参数
                    info.link = c_para["link"].toString();
                    info.contributor = c_para["Contributor"].toString();
                    info.uuid3d = head["uuid_3d"].toString();
                }

                // 保存画布信息
                if (dataStr.contains("canvas")) {
                    info.canvas = dataStr["canvas"].toString();
                }

                if (dataStr.contains("layers")) {
                    QJsonArray layersArray = dataStr["layers"].toArray();  // 原始层定义数组。
                    QStringList layers;  // 展平后的层定义字符串列表。
                    for (const QJsonValue& layerValue : layersArray) {
                        layers.append(layerValue.toString());
                    }
                    info.layers = layers.join("\n");
                }

                if (dataStr.contains("objects")) {
                    QJsonArray objectsArray = dataStr["objects"].toArray();  // 原始对象可见性数组。
                    QStringList objects;  // 展平后的对象可见性字符串列表。
                    for (const QJsonValue& objectValue : objectsArray) {
                        objects.append(objectValue.toString());
                    }
                    info.objects = objects.join("\n");
                }
            }

            // 导入边界框（包围盒）
            if (dataStr.contains("head")) {
                QJsonObject head = dataStr["head"].toObject();  // 用于回退边界框位置的头部对象。
                FootprintBBox footprintBbox;  // 待导入的封装边界框。
                if (dataStr.contains("BBox")) {
                    QJsonObject bbox = dataStr["BBox"].toObject();  // EasyEDA 提供的边界框对象。
                    footprintBbox.x = bbox["x"].toDouble();
                    footprintBbox.y = bbox["y"].toDouble();
                    footprintBbox.width = bbox["width"].toDouble();
                    footprintBbox.height = bbox["height"].toDouble();
                } else {
                    footprintBbox.x = head["x"].toDouble();
                    footprintBbox.y = head["y"].toDouble();
                    footprintBbox.width = 0;
                    footprintBbox.height = 0;
                }
                footprintData->setBbox(footprintBbox);
            }

            // 导入几何数据（焊盘、走线、孔等）
            if (dataStr.contains("shape")) {
                QJsonArray shapes = dataStr["shape"].toArray();  // 封装图元记录数组。

                for (const QJsonValue& shapeValue : shapes) {
                    QString shapeString = shapeValue.toString();  // 当前图元的 EasyEDA 编码字符串。
                    QStringList parts = shapeString.split("~");  // 按字段分隔符拆分图元记录。

                    if (parts.isEmpty())
                        continue;

                    QString designator = parts[0];  // 当前图元的类型标识。

                    if (designator == "PAD") {
                        FootprintPad pad = importPadData(shapeString);  // 解析后的焊盘图元。
                        footprintData->addPad(pad);
                    } else if (designator == "TRACK") {
                        FootprintTrack track = importTrackData(shapeString);  // 解析后的线路图元。
                        footprintData->addTrack(track);
                    } else if (designator == "HOLE") {
                        FootprintHole hole = importHoleData(shapeString);  // 解析后的机械孔图元。
                        footprintData->addHole(hole);
                    } else if (designator == "CIRCLE") {
                        FootprintCircle circle = importFootprintCircleData(shapeString);  // 解析后的圆形图元。
                        footprintData->addCircle(circle);
                    } else if (designator == "ARC") {
                        FootprintArc arc = importFootprintArcData(shapeString);  // 解析后的圆弧图元。
                        footprintData->addArc(arc);
                    } else if (designator == "RECT") {
                        FootprintRectangle rectangle = importFootprintRectangleData(shapeString);  // 解析后的矩形图元。
                        footprintData->addRectangle(rectangle);
                    } else if (designator == "TEXT") {
                        FootprintText text = importFootprintTextData(shapeString);  // 解析后的文本图元。
                        footprintData->addText(text);
                    } else if (designator == "POLYLINE" || designator == "PL") {
                        FootprintTrack track = importTrackData(shapeString);  // 解析后的折线图元。
                        footprintData->addTrack(track);
                    } else if (designator == "POLYGON" || designator == "PG") {
                        FootprintTrack track = importTrackData(shapeString);  // 解析后的多边形边界图元。
                        footprintData->addTrack(track);
                    } else if (designator == "PATH" || designator == "PT") {
                        FootprintTrack track = importTrackData(shapeString);  // 解析后的路径图元。
                        footprintData->addTrack(track);
                    } else if (designator == "SVGNODE") {
                        importSvgNodeData(shapeString, footprintData);
                    } else if (designator == "SOLIDREGION") {
                        FootprintSolidRegion solidRegion = importSolidRegionData(shapeString);  // 解析后的实心区域。
                        footprintData->addSolidRegion(solidRegion);
                    } else {
                        const QString diagnostic =
                            QStringLiteral("EasyEDA 封装包含不支持的图元类型 %1").arg(designator);
                        qWarning().noquote() << diagnostic;
                        footprintData->addValidationError(diagnostic);
                    }
                }

                if (dataStr.contains("layers")) {
                    QJsonArray layers = dataStr["layers"].toArray();  // 需要转换的层定义数组。
                    for (const QJsonValue& layerValue : layers) {
                        LayerDefinition layer = parseLayerDefinition(layerValue.toString());  // 解析后的层定义。
                        footprintData->addLayer(layer);
                    }
                }

                if (dataStr.contains("objects")) {
                    QJsonArray objects = dataStr["objects"].toArray();  // 需要转换的对象可见性数组。
                    for (const QJsonValue& objectValue : objects) {
                        ObjectVisibility visibility =
                            parseObjectVisibility(objectValue.toString());  // 解析后的可见性配置。
                        footprintData->addObjectVisibility(visibility);
                    }
                }

                bool hasHole = false;  // 是否发现带钻孔的焊盘。
                for (const FootprintPad& pad : footprintData->pads()) {
                    if (pad.holeRadius > 0 || pad.holeLength > 0) {
                        hasHole = true;
                        break;
                    }
                }
                if (hasHole) {
                    info.type = "tht";
                }
            }

            footprintData->setInfo(info);
        }
    }

    return footprintData;
}

namespace {
/**
 * @brief 判断三维模型原点是否已经使用封装坐标系中的绝对位置。
 * @param originX 模型原点 X 坐标。
 * @param originY 模型原点 Y 坐标。
 * @param bbox 封装边界框。
 * @return 原点符合绝对坐标范围时返回 true。
 */
bool isAbsoluteModelOrigin(double originX, double originY, const FootprintBBox& bbox) {
    if (bbox.width <= 0.0 || bbox.height <= 0.0) {
        return qAbs(originX) >= 1000.0 || qAbs(originY) >= 1000.0;
    }

    const double margin = qMax(bbox.width, bbox.height) * 2.0;  // 用于容纳坐标误差的边界余量。
    return originX >= bbox.x - margin && originX <= bbox.x + bbox.width + margin && originY >= bbox.y - margin &&
           originY <= bbox.y + bbox.height + margin;
}

/**
 * @brief 判断三维模型原点是否应解释为相对封装中心的偏移。
 * @param originX 模型原点 X 偏移。
 * @param originY 模型原点 Y 偏移。
 * @param bbox 封装边界框。
 * @return 原点位于相对偏移范围时返回 true。
 */
bool isRelativeModelOrigin(double originX, double originY, const FootprintBBox& bbox) {
    const double limit =
        bbox.width > 0.0 && bbox.height > 0.0 ? qMax(bbox.width, bbox.height) * 2.0 : 1000.0;  // 相对原点允许范围。
    return qAbs(originX) <= limit && qAbs(originY) <= limit;
}

/**
 * @brief 根据封装边界统一三维模型原点的绝对、相对和异常回退情况。
 * @param originX 输入原点 X 坐标或偏移。
 * @param originY 输入原点 Y 坐标或偏移。
 * @param originZ 输入原点 Z 坐标。
 * @param bbox 封装边界框。
 * @return 统一后的三维模型平移变换。
 */
Model3DBase normalizeModelOrigin(double originX, double originY, double originZ, const FootprintBBox& bbox) {
    Model3DBase translation;  // 归一化后的模型平移变换。
    const double bboxCenterX = bbox.x + bbox.width / 2.0;  // 封装边界框中心 X 坐标。
    const double bboxCenterY = bbox.y + bbox.height / 2.0;  // 封装边界框中心 Y 坐标。

    if (isAbsoluteModelOrigin(originX, originY, bbox)) {
        translation.x = originX;
        translation.y = originY;
    } else if (isRelativeModelOrigin(originX, originY, bbox)) {
        translation.x = bboxCenterX + originX;
        translation.y = bboxCenterY + originY;
    } else {
        translation.x = bboxCenterX;
        translation.y = bboxCenterY;
    }

    translation.z = originZ;
    return translation;
}
}  // namespace

/**
 * @brief 解析 PAD 记录中的形状、尺寸、层、孔和锁定状态。
 * @param padData EasyEDA PAD 记录字符串。
 * @return 解析后的焊盘数据。
 */
FootprintPad EasyedaFootprintImporter::importPadData(const QString& padData) {
    FootprintPad pad;  // 待填充的焊盘数据。
    QStringList fields = EasyedaUtils::parseDataString(padData);  // 拆分后的焊盘字段。

    if (fields.size() >= 17) {
        pad.shape = fields[1];
        pad.centerX = fields[2].toDouble();
        pad.centerY = fields[3].toDouble();
        pad.width = fields[4].toDouble();
        pad.height = fields[5].toDouble();
        pad.layerId = fields[6].toInt();
        pad.net = fields[7];
        pad.number = fields[8];
        pad.holeRadius = fields[9].toDouble();
        pad.points = fields[10];
        pad.rotation = fields[11].toDouble();
        pad.id = fields[12];
        pad.holeLength = fields[13].toDouble();
        pad.holePoint = fields[14];
        pad.isPlated = EasyedaUtils::stringToBool(fields[15]);
        pad.isLocked = EasyedaUtils::stringToBool(fields[16]);
    }

    return pad;
}

/**
 * @brief 解析 TRACK、POLYLINE、POLYGON 和 PATH 记录的线宽与路径数据。
 * @param trackData EasyEDA 线路记录字符串。
 * @return 解析后的线路数据。
 */
FootprintTrack EasyedaFootprintImporter::importTrackData(const QString& trackData) {
    FootprintTrack track;  // 待填充的线路数据。
    QStringList fields = EasyedaUtils::parseDataString(trackData);  // 拆分后的线路字段。

    if (fields.size() >= 6) {
        track.strokeWidth = fields[1].toDouble();
        track.layerId = fields[2].toInt();
        track.net = fields[3];
        track.points = fields[4];
        track.id = fields[5];
        track.isLocked = fields.size() > 6 ? EasyedaUtils::stringToBool(fields[6]) : false;
    }

    return track;
}

/**
 * @brief 解析 HOLE 记录的孔径、位置、层和锁定状态。
 * @param holeData EasyEDA 孔记录字符串。
 * @return 解析后的机械孔数据。
 */
FootprintHole EasyedaFootprintImporter::importHoleData(const QString& holeData) {
    FootprintHole hole;  // 待填充的机械孔数据。
    QStringList fields = EasyedaUtils::parseDataString(holeData);  // 拆分后的孔字段。

    if (fields.size() >= 5) {
        hole.centerX = fields[1].toDouble();
        hole.centerY = fields[2].toDouble();
        hole.radius = fields[3].toDouble();
        hole.id = fields[4];
        hole.isLocked = fields.size() > 5 ? EasyedaUtils::stringToBool(fields[5]) : false;
    }

    return hole;
}

/**
 * @brief 解析封装圆形图元的圆心、半径和绘制属性。
 * @param circleData EasyEDA 圆形图元记录字符串。
 * @return 解析后的圆形图元数据。
 */
FootprintCircle EasyedaFootprintImporter::importFootprintCircleData(const QString& circleData) {
    FootprintCircle circle;  // 待填充的圆形图元数据。
    QStringList fields = EasyedaUtils::parseDataString(circleData);  // 拆分后的圆形图元字段。

    if (fields.size() >= 7) {
        circle.cx = fields[1].toDouble();
        circle.cy = fields[2].toDouble();
        circle.radius = fields[3].toDouble();
        circle.strokeWidth = fields[4].toDouble();
        circle.layerId = fields[5].toInt();
        circle.id = fields[6];
        circle.isLocked = fields.size() > 7 ? EasyedaUtils::stringToBool(fields[7]) : false;
    }

    return circle;
}

/**
 * @brief 解析封装矩形图元的边界、层和线宽属性。
 * @param rectangleData EasyEDA 矩形图元记录字符串。
 * @return 解析后的矩形图元数据。
 */
FootprintRectangle EasyedaFootprintImporter::importFootprintRectangleData(const QString& rectangleData) {
    FootprintRectangle rectangle;  // 待填充的矩形图元数据。
    QStringList fields = EasyedaUtils::parseDataString(rectangleData);  // 拆分后的矩形图元字段。

    if (fields.size() >= 8) {
        rectangle.x = fields[1].toDouble();
        rectangle.y = fields[2].toDouble();
        rectangle.width = fields[3].toDouble();
        rectangle.height = fields[4].toDouble();
        rectangle.layerId = fields[5].toInt();
        rectangle.id = fields[6];
        rectangle.strokeWidth = fields[7].toDouble();
        rectangle.isLocked = fields.size() > 8 ? EasyedaUtils::stringToBool(fields[8]) : false;
    }

    return rectangle;
}

/**
 * @brief 解析封装弧线图元的路径、层、网络和锁定状态。
 * @param arcData EasyEDA 圆弧图元记录字符串。
 * @return 解析后的圆弧图元数据。
 */
FootprintArc EasyedaFootprintImporter::importFootprintArcData(const QString& arcData) {
    FootprintArc arc;  // 待填充的圆弧图元数据。
    QStringList fields = EasyedaUtils::parseDataString(arcData);  // 拆分后的圆弧图元字段。

    if (fields.size() >= 7) {
        arc.strokeWidth = fields[1].toDouble();
        arc.layerId = fields[2].toInt();
        arc.net = fields[3];
        arc.path = fields[4];
        arc.helperDots = fields[5];
        arc.id = fields[6];
        arc.isLocked = fields.size() > 7 ? EasyedaUtils::stringToBool(fields[7]) : false;
    }

    return arc;
}

/**
 * @brief 解析封装文本图元的内容、字体、方向、可见性和层信息。
 * @param textData EasyEDA 文本图元记录字符串。
 * @return 解析后的文本图元数据。
 */
FootprintText EasyedaFootprintImporter::importFootprintTextData(const QString& textData) {
    FootprintText text;  // 待填充的文本图元数据。
    QStringList fields = EasyedaUtils::parseDataString(textData);  // 拆分后的文本图元字段。

    if (fields.size() >= 14) {
        text.type = fields[1];
        text.centerX = fields[2].toDouble();
        text.centerY = fields[3].toDouble();
        text.strokeWidth = fields[4].toDouble();
        text.rotation = fields[5].toInt();
        text.mirror = fields[6];
        text.layerId = fields[7].toInt();
        text.net = fields[8];
        text.fontSize = fields[9].toDouble();
        text.text = fields[10];
        text.textPath = fields[11];
        text.isDisplayed = EasyedaUtils::stringToBool(fields[12]);
        text.id = fields[13];
        text.isLocked = fields.size() > 14 ? EasyedaUtils::stringToBool(fields[14]) : false;
    }

    return text;
}

/**
 * @brief 解析实心区域图元，并识别其是否属于禁止布线层。
 * @param solidRegionData EasyEDA 实心区域记录字符串。
 * @return 解析后的实心区域数据。
 */
FootprintSolidRegion EasyedaFootprintImporter::importSolidRegionData(const QString& solidRegionData) {
    FootprintSolidRegion region;  // 待填充的实心区域数据。
    QStringList fields = EasyedaUtils::parseDataString(solidRegionData);  // 拆分后的实心区域字段。

    if (fields.size() >= 6) {
        region.layerId = fields[1].toInt();
        region.path = fields[3];
        region.fillStyle = fields[4];
        region.id = fields[5];
        region.isLocked = fields.size() > 7 ? EasyedaUtils::stringToBool(fields[7]) : false;
        region.isKeepOut = (region.layerId == 99);
    }

    return region;
}

/**
 * @brief 解析 SVGNODE JSON，恢复三维模型变换和轮廓路径信息。
 * @param svgNodeData EasyEDA SVGNODE 记录字符串。
 * @param footprintData 待写入模型和轮廓数据的封装对象。
 */
void EasyedaFootprintImporter::importSvgNodeData(const QString& svgNodeData,
                                                 QSharedPointer<FootprintData> footprintData) {
    if (!footprintData) {
        qWarning() << "importSvgNodeData called with null footprintData";
        return;
    }

    QStringList parts = svgNodeData.split("~");  // SVGNODE 记录字段列表。
    if (parts.size() < 2) {
        qWarning() << "Invalid SVGNODE data format";
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(parts[1].toUtf8());  // SVGNODE 中嵌入的 JSON 文档。
    if (!doc.isObject()) {
        qWarning() << "Failed to parse SVGNODE JSON";
        return;
    }

    QJsonObject root = doc.object();  // SVGNODE 根对象。
    if (!root.contains("attrs")) {
        qWarning() << "SVGNODE missing attrs";
        return;
    }

    QJsonObject attrs = root["attrs"].toObject();  // SVGNODE 属性对象。

    if (attrs.contains("c_etype") && attrs["c_etype"].toString() == "outline3D") {
        Model3DData model3D;  // 轮廓节点关联的三维模型数据。
        if (attrs.contains("uuid")) {
            QString modelUuid = attrs["uuid"].toString();  // 三维模型 UUID。
            model3D.setUuid(modelUuid);
            model3D.setName(attrs.contains("title") ? attrs["title"].toString() : "");

            if (attrs.contains("c_origin")) {
                QString c_origin = attrs["c_origin"].toString();  // EasyEDA 模型原点字符串。
                QStringList originParts = c_origin.split(",");  // 拆分后的原点坐标字段。
                if (originParts.size() >= 2) {
                    const double originX = originParts[0].toDouble();  // 模型原点 X 坐标。
                    const double originY = originParts[1].toDouble();  // 模型原点 Y 坐标。
                    const double originZ = attrs.contains("z") ? attrs["z"].toDouble() : 0.0;  // 模型原点 Z 坐标。
                    const Model3DBase translation =
                        normalizeModelOrigin(originX, originY, originZ, footprintData->bbox());  // 归一化后的平移变换。
                    model3D.setTranslation(translation);
                }
            }

            if (attrs.contains("c_rotation")) {
                QString c_rotation = attrs["c_rotation"].toString();  // EasyEDA 模型旋转字符串。
                QStringList rotationParts = c_rotation.split(",");  // 拆分后的旋转角度字段。
                if (rotationParts.size() >= 3) {
                    Model3DBase rotation;  // 三维模型旋转变换。
                    rotation.x = rotationParts[0].toDouble();
                    rotation.y = rotationParts[1].toDouble();
                    rotation.z = rotationParts[2].toDouble();
                    model3D.setRotation(rotation);
                }
            }

            footprintData->setModel3D(model3D);
        }

        FootprintOutline outline;  // 从 SVGNODE 恢复的封装轮廓。
        outline.id = attrs["id"].toString();
        outline.strokeWidth = attrs.contains("c_width") ? attrs["c_width"].toString().toDouble() : 0.0;
        outline.isLocked = false;

        if (attrs.contains("layerid")) {
            outline.layerId = attrs["layerid"].toString().toInt();
        } else {
            outline.layerId = 19;  // 默认3DModel 层
        }

        if (root.contains("childNodes")) {
            QJsonArray childNodes = root["childNodes"].toArray();  // 轮廓路径的子节点数组。
            for (const QJsonValue& child : childNodes) {
                QJsonObject childObj = child.toObject();  // 当前轮廓子节点。
                if (childObj.contains("attrs")) {
                    QJsonObject childAttrs = childObj["attrs"].toObject();  // 子节点属性对象。
                    if (childAttrs.contains("points")) {
                        outline.path = childAttrs["points"].toString();
                        break;
                    }
                }
            }
        }

        footprintData->addOutline(outline);
    } else {
        Model3DData model3D;  // 普通 SVGNODE 关联的三维模型数据。
        model3D.setName(attrs.contains("title") ? attrs["title"].toString() : "");
        model3D.setUuid(attrs.contains("uuid") ? attrs["uuid"].toString() : "");

        if (attrs.contains("c_origin")) {
            QString c_origin = attrs["c_origin"].toString();  // EasyEDA 模型原点字符串。
            QStringList originParts = c_origin.split(",");  // 拆分后的原点坐标字段。
            if (originParts.size() >= 2) {
                const double originX = originParts[0].toDouble();  // 模型原点 X 坐标。
                const double originY = originParts[1].toDouble();  // 模型原点 Y 坐标。
                const double originZ = attrs.contains("z") ? attrs["z"].toDouble() : 0.0;  // 模型原点 Z 坐标。
                const Model3DBase translation =
                    normalizeModelOrigin(originX, originY, originZ, footprintData->bbox());  // 归一化后的平移变换。
                model3D.setTranslation(translation);
            }
        }

        if (attrs.contains("c_rotation")) {
            QString c_rotation = attrs["c_rotation"].toString();  // EasyEDA 模型旋转字符串。
            QStringList rotationParts = c_rotation.split(",");  // 拆分后的旋转角度字段。
            if (rotationParts.size() >= 3) {
                Model3DBase rotation;  // 三维模型旋转变换。
                rotation.x = rotationParts[0].toDouble();
                rotation.y = rotationParts[1].toDouble();
                rotation.z = rotationParts[2].toDouble();
                model3D.setRotation(rotation);
            }
        }

        footprintData->setModel3D(model3D);
    }
}

/**
 * @brief 解析封装层定义的编号、名称、颜色和可见性字段。
 * @param layerString EasyEDA 层定义记录字符串。
 * @return 解析后的层定义。
 */
LayerDefinition EasyedaFootprintImporter::parseLayerDefinition(const QString& layerString) {
    LayerDefinition layer;  // 待填充的层定义。
    QStringList fields = layerString.split("~");  // 拆分后的层定义字段。

    if (fields.size() >= 5) {
        layer.layerId = fields[0].toInt();
        layer.name = fields[1];
        layer.color = fields[2];
        layer.isVisible = (fields[3] == "true");
        layer.isUsedForManufacturing = (fields[4] == "true");

        if (fields.size() >= 7) {
            layer.expansion = fields[6].toDouble();
        }
    }

    return layer;
}

/**
 * @brief 解析封装对象可见性记录的类型和开关状态。
 * @param objectString EasyEDA 对象可见性记录字符串。
 * @return 解析后的对象可见性配置。
 */
ObjectVisibility EasyedaFootprintImporter::parseObjectVisibility(const QString& objectString) {
    ObjectVisibility visibility;  // 待填充的对象可见性配置。
    QStringList fields = objectString.split("~");  // 拆分后的对象可见性字段。

    if (fields.size() >= 3) {
        visibility.objectType = fields[0];
        visibility.isEnabled = (fields[1] == "true");
        visibility.isVisible = (fields[2] == "true");
    }

    return visibility;
}

}  // namespace EasyKiConverter
