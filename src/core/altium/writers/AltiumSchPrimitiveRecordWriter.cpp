#include "AltiumSchPrimitiveRecordWriter.h"

#include "AltiumSchLibWriter.h"
#include "utils/AltiumCoord.h"

namespace EasyKiConverter {

/**
 * @brief 创建 SchLib 图元记录写入器。
 * @param owner 所属的 SchLib 主写入器。
 */
AltiumSchPrimitiveRecordWriter::AltiumSchPrimitiveRecordWriter(AltiumSchLibWriter& owner) : m_owner(owner) {}

/**
 * @brief 写入矩形记录的坐标、线型、颜色和填充参数。
 * @param writer 目标二进制写入器。
 * @param rectangle 矩形模型。
 */
void AltiumSchPrimitiveRecordWriter::writeRectangle(AltiumBinaryWriter& writer, const AltiumSchRectangle& rectangle) {
    QMap<QString, QString> params;  // 当前矩形的 RECORD=14 参数。
    params["RECORD"] = "14";
    m_owner.addOwnerParams(params, rectangle.ownerPartId);
    m_owner.addCoordParam(params, "Location.X", rectangle.locationX);
    m_owner.addCoordParam(params, "Location.Y", rectangle.locationY);
    m_owner.addCoordParam(params, "Corner.X", rectangle.cornerX);
    m_owner.addCoordParam(params, "Corner.Y", rectangle.cornerY);
    if (rectangle.lineWidth != 0)
        params["LineWidth"] = QString::number(rectangle.lineWidth);
    if (rectangle.lineStyle != 0)
        params["LineStyleExt"] = QString::number(rectangle.lineStyle);
    m_owner.addColorParam(params, "Color", rectangle.color);
    if (rectangle.areaColor != 0xFFFFFF)
        params["AreaColor"] = QString::number(rectangle.areaColor);
    if (rectangle.isSolid)
        params["IsSolid"] = "T";
    m_owner.addUniqueID(params);
    writer.writeCStringParameterBlock(params);
}

/**
 * @brief 写入圆角矩形记录的边界、圆角半径和填充参数。
 * @param writer 目标二进制写入器。
 * @param rectangle 圆角矩形模型。
 */
void AltiumSchPrimitiveRecordWriter::writeRoundRectangle(AltiumBinaryWriter& writer,
                                                         const AltiumSchRoundRectangle& rectangle) {
    QMap<QString, QString> params;  // 当前圆角矩形的 RECORD=10 参数。
    params["RECORD"] = "10";
    m_owner.addOwnerParams(params, rectangle.ownerPartId);
    m_owner.addCoordParam(params, "Location.X", rectangle.locationX);
    m_owner.addCoordParam(params, "Location.Y", rectangle.locationY);
    m_owner.addCoordParam(params, "Corner.X", rectangle.cornerX);
    m_owner.addCoordParam(params, "Corner.Y", rectangle.cornerY);
    m_owner.addCoordParam(params, "CornerXRadius", rectangle.cornerXRadius);
    m_owner.addCoordParam(params, "CornerYRadius", rectangle.cornerYRadius);
    if (rectangle.lineWidth != 0)
        params["LineWidth"] = QString::number(rectangle.lineWidth);
    if (rectangle.lineStyle != 0)
        params["LineStyle"] = QString::number(rectangle.lineStyle);
    m_owner.addColorParam(params, "Color", rectangle.color);
    if (rectangle.areaColor != 0xFFFFFF)
        params["AreaColor"] = QString::number(rectangle.areaColor);
    if (rectangle.isSolid)
        params["IsSolid"] = "T";
    m_owner.addUniqueID(params);
    writer.writeCStringParameterBlock(params);
}

/**
 * @brief 写入线段记录的端点、线宽、线型和颜色。
 * @param writer 目标二进制写入器。
 * @param line 线段模型。
 */
void AltiumSchPrimitiveRecordWriter::writeLine(AltiumBinaryWriter& writer, const AltiumSchLine& line) {
    QMap<QString, QString> params;  // 当前线段的 RECORD=13 参数。
    params["RECORD"] = "13";
    m_owner.addOwnerParams(params, line.ownerPartId);
    m_owner.addCoordParam(params, "Location.X", line.locationX);
    m_owner.addCoordParam(params, "Location.Y", line.locationY);
    m_owner.addCoordParam(params, "Corner.X", line.cornerX);
    m_owner.addCoordParam(params, "Corner.Y", line.cornerY);
    params["LineWidth"] = QString::number(AltiumCoord::lineWidthToIndex(line.lineWidth));
    if (line.lineStyle != 0)
        params["LineStyle"] = QString::number(line.lineStyle);
    m_owner.addColorParam(params, "Color", line.color);
    m_owner.addUniqueID(params);
    writer.writeCStringParameterBlock(params);
}

/**
 * @brief 写入圆弧记录并规范化起止角度。
 * @param writer 目标二进制写入器。
 * @param arc 圆弧模型。
 */
void AltiumSchPrimitiveRecordWriter::writeArc(AltiumBinaryWriter& writer, const AltiumSchArc& arc) {
    const double startAngle =
        m_owner.normalizeFiniteAngle(arc.startAngle, 0.0, QStringLiteral("圆弧起始角度"));  // 规范化后的起始角度。
    const double endAngle =
        m_owner.normalizeFiniteAngle(arc.endAngle, 360.0, QStringLiteral("圆弧结束角度"));  // 规范化后的结束角度。
    QMap<QString, QString> params;  // 当前圆弧的 RECORD=12 参数。
    params["RECORD"] = "12";
    m_owner.addOwnerParams(params, arc.ownerPartId);
    m_owner.addCoordParam(params, "Location.X", arc.centerX);
    m_owner.addCoordParam(params, "Location.Y", arc.centerY);
    m_owner.addCoordParam(params, "Radius", arc.radius);
    if (arc.lineWidth != 0)
        params["LineWidth"] = QString::number(arc.lineWidth);
    if (arc.lineStyle != 0)
        params["LineStyle"] = QString::number(arc.lineStyle);
    if (startAngle != 0.0)
        params["StartAngle"] = QString::number(startAngle, 'f', 3);
    params["EndAngle"] = QString::number(endAngle, 'f', 3);
    m_owner.addColorParam(params, "Color", arc.color);
    m_owner.addUniqueID(params);
    writer.writeCStringParameterBlock(params);
}

/**
 * @brief 写入多边形记录及其所有顶点。
 * @param writer 目标二进制写入器。
 * @param polygon 多边形模型。
 */
void AltiumSchPrimitiveRecordWriter::writePolygon(AltiumBinaryWriter& writer, const AltiumSchPolygon& polygon) {
    QMap<QString, QString> params;  // 当前多边形的 RECORD=7 参数。
    params["RECORD"] = "7";
    m_owner.addOwnerParams(params, polygon.ownerPartId);
    params["LineWidth"] = QString::number(polygon.lineWidth);
    if (polygon.lineStyle != 0)
        params["LineStyle"] = QString::number(polygon.lineStyle);
    m_owner.addColorParam(params, "Color", polygon.color);
    if (polygon.areaColor != 0xFFFFFF)
        params["AreaColor"] = QString::number(polygon.areaColor);
    if (polygon.isSolid)
        params["IsSolid"] = "T";
    params["LocationCount"] = QString::number(polygon.vertices.size());
    for (int i = 0; i < polygon.vertices.size(); ++i) {
        const int index = i + 1;  // Altium 多边形顶点编号。
        const int32_t x = AltiumCoord::toSchematicUnits(static_cast<int>(polygon.vertices[i].x()));  // 顶点 X 坐标。
        const int32_t y = AltiumCoord::toSchematicUnits(static_cast<int>(polygon.vertices[i].y()));  // 顶点 Y 坐标。
        if (x != 0)
            params[QString("X%1").arg(index)] = QString::number(x);
        if (y != 0)
            params[QString("Y%1").arg(index)] = QString::number(y);
    }
    m_owner.addUniqueID(params);
    writer.writeCStringParameterBlock(params);
}

/**
 * @brief 写入椭圆记录的两个半径和填充参数。
 * @param writer 目标二进制写入器。
 * @param ellipse 椭圆模型。
 */
void AltiumSchPrimitiveRecordWriter::writeEllipse(AltiumBinaryWriter& writer, const AltiumSchEllipse& ellipse) {
    QMap<QString, QString> params;  // 当前椭圆的 RECORD=8 参数。
    params["RECORD"] = "8";
    m_owner.addOwnerParams(params, ellipse.ownerPartId);
    m_owner.addCoordParam(params, "Location.X", ellipse.centerX);
    m_owner.addCoordParam(params, "Location.Y", ellipse.centerY);
    m_owner.addCoordParam(params, "Radius", ellipse.radiusX);
    m_owner.addCoordParam(params, "SecondaryRadius", ellipse.radiusY);
    if (ellipse.lineWidth != 0)
        params["LineWidth"] = QString::number(ellipse.lineWidth);
    if (ellipse.lineStyle != 0)
        params["LineStyle"] = QString::number(ellipse.lineStyle);
    m_owner.addColorParam(params, "Color", ellipse.color);
    if (ellipse.areaColor != 0xFFFFFF)
        params["AreaColor"] = QString::number(ellipse.areaColor);
    if (ellipse.isSolid)
        params["IsSolid"] = "T";
    m_owner.addUniqueID(params);
    writer.writeCStringParameterBlock(params);
}

/**
 * @brief 写入扇形记录并规范化起止角度。
 * @param writer 目标二进制写入器。
 * @param pie 扇形模型。
 */
void AltiumSchPrimitiveRecordWriter::writePie(AltiumBinaryWriter& writer, const AltiumSchPie& pie) {
    const double startAngle =
        m_owner.normalizeFiniteAngle(pie.startAngle, 0.0, QStringLiteral("扇形起始角度"));  // 规范化后的起始角度。
    const double endAngle =
        m_owner.normalizeFiniteAngle(pie.endAngle, 360.0, QStringLiteral("扇形结束角度"));  // 规范化后的结束角度。
    QMap<QString, QString> params;  // 当前扇形的 RECORD=9 参数。
    params["RECORD"] = "9";
    m_owner.addOwnerParams(params, pie.ownerPartId);
    m_owner.addCoordParam(params, "Location.X", pie.centerX);
    m_owner.addCoordParam(params, "Location.Y", pie.centerY);
    m_owner.addCoordParam(params, "Radius", pie.radius);
    if (pie.lineWidth != 0)
        params["LineWidth"] = QString::number(pie.lineWidth);
    if (pie.lineStyle != 0)
        params["LineStyle"] = QString::number(pie.lineStyle);
    if (startAngle != 0.0)
        params["StartAngle"] = QString::number(startAngle, 'f', 3);
    params["EndAngle"] = QString::number(endAngle, 'f', 3);
    m_owner.addColorParam(params, "Color", pie.color);
    if (pie.areaColor != 0xFFFFFF)
        params["AreaColor"] = QString::number(pie.areaColor);
    if (pie.isSolid)
        params["IsSolid"] = "T";
    writer.writeCStringParameterBlock(params);
}

/**
 * @brief 写入椭圆弧记录并规范化起止角度。
 * @param writer 目标二进制写入器。
 * @param arc 椭圆弧模型。
 */
void AltiumSchPrimitiveRecordWriter::writeEllipticalArc(AltiumBinaryWriter& writer, const AltiumSchEllipticalArc& arc) {
    const double startAngle =
        m_owner.normalizeFiniteAngle(arc.startAngle, 0.0, QStringLiteral("椭圆弧起始角度"));  // 规范化后的起始角度。
    const double endAngle =
        m_owner.normalizeFiniteAngle(arc.endAngle, 360.0, QStringLiteral("椭圆弧结束角度"));  // 规范化后的结束角度。
    QMap<QString, QString> params;  // 当前椭圆弧的 RECORD=11 参数。
    params["RECORD"] = "11";
    m_owner.addOwnerParams(params, arc.ownerPartId);
    m_owner.addCoordParam(params, "Location.X", arc.centerX);
    m_owner.addCoordParam(params, "Location.Y", arc.centerY);
    m_owner.addCoordParam(params, "Radius", arc.radiusX);
    m_owner.addCoordParam(params, "SecondaryRadius", arc.radiusY);
    if (arc.lineWidth != 0)
        params["LineWidth"] = QString::number(arc.lineWidth);
    if (arc.lineStyle != 0)
        params["LineStyle"] = QString::number(arc.lineStyle);
    if (startAngle != 0.0)
        params["StartAngle"] = QString::number(startAngle, 'f', 3);
    params["EndAngle"] = QString::number(endAngle, 'f', 3);
    m_owner.addColorParam(params, "Color", arc.color);
    if (arc.areaColor != 0xFFFFFF)
        params["AreaColor"] = QString::number(arc.areaColor);
    m_owner.addUniqueID(params);
    writer.writeCStringParameterBlock(params);
}

/**
 * @brief 写入折线记录及其所有顶点。
 * @param writer 目标二进制写入器。
 * @param polyline 折线模型。
 */
void AltiumSchPrimitiveRecordWriter::writePolyline(AltiumBinaryWriter& writer, const AltiumSchPolyline& polyline) {
    QMap<QString, QString> params;  // 当前折线的 RECORD=6 参数。
    params["RECORD"] = "6";
    m_owner.addOwnerParams(params, polyline.ownerPartId);
    params["LineWidth"] = QString::number(polyline.lineWidth);
    if (polyline.lineStyle != 0)
        params["LineStyle"] = QString::number(polyline.lineStyle);
    m_owner.addColorParam(params, "Color", polyline.color);
    params["LocationCount"] = QString::number(polyline.vertices.size());
    for (int i = 0; i < polyline.vertices.size(); ++i) {
        const int index = i + 1;  // Altium 折线顶点编号。
        const int32_t x = AltiumCoord::toSchematicUnits(static_cast<int>(polyline.vertices[i].x()));  // 顶点 X 坐标。
        const int32_t y = AltiumCoord::toSchematicUnits(static_cast<int>(polyline.vertices[i].y()));  // 顶点 Y 坐标。
        if (x != 0)
            params[QString("X%1").arg(index)] = QString::number(x);
        if (y != 0)
            params[QString("Y%1").arg(index)] = QString::number(y);
    }
    m_owner.addUniqueID(params);
    writer.writeCStringParameterBlock(params);
}

/**
 * @brief 将路径模型转换为同等字段的折线模型并写入。
 * @param writer 目标二进制写入器。
 * @param path 路径模型。
 */
void AltiumSchPrimitiveRecordWriter::writePath(AltiumBinaryWriter& writer, const AltiumSchPath& path) {
    AltiumSchPolyline polyline;  // 用于复用折线序列化逻辑的临时模型。
    polyline.vertices = path.vertices;
    polyline.lineWidth = path.lineWidth;
    polyline.lineStyle = path.lineStyle;
    polyline.color = path.color;
    polyline.ownerPartId = path.ownerPartId;
    writePolyline(writer, polyline);
}

/**
 * @brief 写入三次 Bezier 曲线及其四个控制点。
 * @param writer 目标二进制写入器。
 * @param bezier Bezier 曲线模型。
 */
void AltiumSchPrimitiveRecordWriter::writeBezier(AltiumBinaryWriter& writer, const AltiumSchBezier& bezier) {
    if (bezier.controlPoints.size() != 4) {
        m_owner.m_diagnostics.append(QStringLiteral("Altium SchLib Bézier 图元控制点数量无效（数量为 %1），已跳过")
                                         .arg(bezier.controlPoints.size()));
        return;
    }
    QMap<QString, QString> params;  // 当前 Bezier 曲线的 RECORD=5 参数。
    params["RECORD"] = "5";
    m_owner.addOwnerParams(params, bezier.ownerPartId);
    params["LineWidth"] = QString::number(bezier.lineWidth);
    m_owner.addColorParam(params, "Color", bezier.color);
    params["LocationCount"] = "4";
    for (int i = 0; i < 4; ++i) {
        const int32_t x =
            AltiumCoord::toSchematicUnits(static_cast<int>(bezier.controlPoints[i].x()));  // 控制点 X 坐标。
        const int32_t y =
            AltiumCoord::toSchematicUnits(static_cast<int>(bezier.controlPoints[i].y()));  // 控制点 Y 坐标。
        if (x != 0)
            params[QString("X%1").arg(i + 1)] = QString::number(x);
        if (y != 0)
            params[QString("Y%1").arg(i + 1)] = QString::number(y);
    }
    m_owner.addUniqueID(params);
    writer.writeCStringParameterBlock(params);
}

/**
 * @brief 写入 IEEE 图形符号及其变换参数。
 * @param writer 目标二进制写入器。
 * @param ieee IEEE 图形模型。
 */
void AltiumSchPrimitiveRecordWriter::writeIeee(AltiumBinaryWriter& writer, const AltiumSchIeee& ieee) {
    QMap<QString, QString> params;  // 当前 IEEE 图形的 RECORD=3 参数。
    params["RECORD"] = "3";
    m_owner.addOwnerParams(params, ieee.ownerPartId);
    params["Symbol"] = QString::number(ieee.symbol);
    m_owner.addCoordParam(params, "Location.X", ieee.locationX);
    m_owner.addCoordParam(params, "Location.Y", ieee.locationY);
    params["ScaleFactor"] = QString::number(qMax(1, ieee.scaleFactor));
    if (ieee.orientation != 0)
        params["Orientation"] = QString::number(ieee.orientation);
    params["LineWidth"] = QString::number(qMax(0, ieee.lineWidth));
    if (ieee.mirrored)
        params["Mirror"] = "T";
    m_owner.addColorParam(params, "Color", ieee.color);
    writer.writeCStringParameterBlock(params);
}

}  // namespace EasyKiConverter
