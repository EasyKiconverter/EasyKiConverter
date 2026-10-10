#include "XpeditionSymbolAdapter.h"

#include <QLineF>
#include <QtMath>

namespace EasyKiConverter {

namespace {

constexpr double kThousandthInchMm = 0.0254;  ///< 千分之一英寸到毫米的换算系数。

/**
 * @brief 将 Xpedition 千分之一英寸坐标转换为毫米。
 * @param point 输入坐标。
 * @return 换算后的毫米坐标。
 */
QPointF toMillimeters(const QPointF& point) {
    return point * kThousandthInchMm;
}

/**
 * @brief 根据引脚起点和终点推导统一 IR 的主方向。
 * @param start 引脚起点。
 * @param end 引脚终点。
 * @return 统一 IR 引脚方向。
 */
IR::PinDirection directionFor(const QPointF& start, const QPointF& end) {
    const QPointF delta = end - start;  // 引脚方向向量。
    if (qAbs(delta.x()) >= qAbs(delta.y()))
        return delta.x() < 0.0 ? IR::PinDirection::Right : IR::PinDirection::Left;
    return delta.y() < 0.0 ? IR::PinDirection::Up : IR::PinDirection::Down;
}

/**
 * @brief 将 Xpedition 引脚类型映射为统一 IR 电气类型。
 * @param value Xpedition 引脚类型文本。
 * @return 统一 IR 电气类型。
 */
IR::PinElectricalType electricalTypeFor(const QString& value) {
    const QString type = value.trimmed().toUpper();  // 规范化后的类型文本。
    if (type == QStringLiteral("IN"))
        return IR::PinElectricalType::Input;
    if (type == QStringLiteral("OUT"))
        return IR::PinElectricalType::Output;
    if (type == QStringLiteral("BI"))
        return IR::PinElectricalType::Bidirectional;
    if (type == QStringLiteral("PASSIVE"))
        return IR::PinElectricalType::Passive;
    if (type == QStringLiteral("PWR"))
        return IR::PinElectricalType::Power;
    if (type == QStringLiteral("OCL"))
        return IR::PinElectricalType::OpenCollector;
    if (type == QStringLiteral("OEM"))
        return IR::PinElectricalType::OpenEmitter;
    return IR::PinElectricalType::Unspecified;
}

}  // namespace

/**
 * @brief 将 Xpedition 符号文档转换为统一符号 IR。
 * @param document Xpedition 符号文档。
 * @param diagnostics 可选的解析诊断输出。
 * @return 转换后的符号组件 IR。
 */
IR::SymbolComponentIR XpeditionSymbolAdapter::toIR(const Parser::XpeditionSymbolDocument& document,
                                                   Parser::ParseDiagnostics* diagnostics) {
    IR::SymbolComponentIR result;  // 输出符号组件 IR。
    const Parser::XpeditionSymbolModel& source = document.model;  // Xpedition 符号模型。
    result.name = source.name;
    result.partCount = qMax(1, source.partCount);
    result.footprintName = source.footprint;
    result.footprintNames = source.footprint.isEmpty() ? QStringList() : QStringList{source.footprint};
    result.designatorPrefix = source.properties.value(QStringLiteral("REFDES"));
    result.description = source.properties.value(QStringLiteral("DESC"));
    result.sourceMetadata.insert(QStringLiteral("xpeditionVersion"), QString::number(source.version));
    for (auto iterator = source.properties.cbegin(); iterator != source.properties.cend(); ++iterator)
        result.sourceMetadata.insert(iterator.key(), iterator.value());

    for (const Parser::XpeditionSymbolPin& sourcePin : source.pins) {
        IR::SymbolPinIR pin;  // 当前引脚 IR。
        pin.designator = sourcePin.numbers.join(QStringLiteral(","));
        pin.name = sourcePin.name;
        pin.position = toMillimeters(sourcePin.start);
        pin.length = QLineF(sourcePin.start, sourcePin.end).length() * kThousandthInchMm;
        pin.direction = directionFor(sourcePin.start, sourcePin.end);
        pin.electricalType = electricalTypeFor(sourcePin.pinType);
        pin.style.inverted = sourcePin.inverted;
        pin.hasNamePosition = !sourcePin.name.isEmpty();
        pin.namePosition = toMillimeters(sourcePin.namePosition);
        pin.nameFontSizeMm = sourcePin.nameSize * kThousandthInchMm;
        pin.nameRotation = sourcePin.nameRotation;
        pin.display.showName = sourcePin.nameVisible;
        pin.display.showDesignator = sourcePin.numbersVisible;
        pin.partIndex = sourcePin.partIndex;
        pin.commonToAllParts = sourcePin.partIndex == 0;
        result.pins.append(pin);
    }

    for (const Parser::XpeditionSymbolRectangle& sourceRectangle : source.rectangles) {
        IR::SymbolRectangleIR rectangle;  // 当前矩形 IR。
        const QPointF start = toMillimeters(sourceRectangle.start);  // 矩形起点。
        const QPointF end = toMillimeters(sourceRectangle.end);  // 矩形终点。
        rectangle.x0 = start.x();
        rectangle.y0 = start.y();
        rectangle.x1 = end.x();
        rectangle.y1 = end.y();
        rectangle.partIndex = sourceRectangle.partIndex;
        result.rectangles.append(rectangle);
    }
    for (const Parser::XpeditionSymbolCircle& sourceCircle : source.circles) {
        IR::SymbolCircleIR circle;  // 当前圆形 IR。
        circle.center = toMillimeters(sourceCircle.center);
        circle.radius = sourceCircle.radius * kThousandthInchMm;
        circle.partIndex = sourceCircle.partIndex;
        result.circles.append(circle);
    }
    for (const Parser::XpeditionSymbolArc& sourceArc : source.arcs) {
        IR::SymbolArcIR arc;  // 当前圆弧 IR。
        arc.startPoint = toMillimeters(sourceArc.start);
        arc.midPoint = toMillimeters(sourceArc.center);
        arc.endPoint = toMillimeters(sourceArc.end);
        arc.partIndex = sourceArc.partIndex;
        result.arcs.append(arc);
    }
    for (const Parser::XpeditionSymbolPolyline& sourcePolyline : source.polylines) {
        QList<QPointF> points;  // 折线的毫米坐标点。
        for (const QPointF& sourcePoint : sourcePolyline.points)
            points.append(toMillimeters(sourcePoint));
        if (sourcePolyline.closed) {
            IR::SymbolPolygonIR polygon;  // 闭合折线对应的多边形 IR。
            polygon.points = points;
            polygon.partIndex = sourcePolyline.partIndex;
            result.polygons.append(polygon);
        } else {
            IR::SymbolPolylineIR polyline;  // 非闭合折线 IR。
            polyline.points = points;
            polyline.partIndex = sourcePolyline.partIndex;
            result.polylines.append(polyline);
        }
    }
    for (const Parser::XpeditionSymbolText& sourceText : source.texts) {
        IR::SymbolTextIR text;  // 当前文本 IR。
        text.text = sourceText.text;
        text.position = toMillimeters(sourceText.position);
        text.fontSizeMm = sourceText.size * kThousandthInchMm;
        text.rotation = sourceText.rotation;
        text.partIndex = sourceText.partIndex;
        result.texts.append(text);
    }

    if (source.symbolType == 0 && diagnostics)
        diagnostics->add(Parser::ParseSeverity::Warning,
                         Parser::ParseScope::Symbol,
                         QStringLiteral("Xpedition 符号缺少类型字段"),
                         result.name);
    return result;
}

}  // namespace EasyKiConverter
