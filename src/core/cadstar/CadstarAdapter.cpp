#include "CadstarAdapter.h"

#include <QLineF>
#include <QtMath>

namespace EasyKiConverter {

namespace {

/** @brief 将 Cadstar 源单位坐标转换为毫米。 */
QPointF millimeters(const QPointF& point, Parser::LengthUnit unit) {
    return {Parser::UnitConverter::toMillimeters(point.x(), unit),
            Parser::UnitConverter::toMillimeters(point.y(), unit)};
}

/** @brief 将 Cadstar 源单位长度转换为毫米。 */
double millimeters(double value, Parser::LengthUnit unit) {
    return Parser::UnitConverter::toMillimeters(value, unit);
}

/** @brief 根据两个端点计算统一 IR 的方向。 */
IR::PinDirection directionFor(const QPointF& start, const QPointF& end) {
    const QPointF delta = end - start;
    if (qAbs(delta.x()) >= qAbs(delta.y()))
        return delta.x() < 0.0 ? IR::PinDirection::Right : IR::PinDirection::Left;
    return delta.y() < 0.0 ? IR::PinDirection::Up : IR::PinDirection::Down;
}

/** @brief 将 Cadstar 电气类型映射为统一 IR 类型。 */
IR::PinElectricalType electricalTypeFor(const QString& value) {
    const QString type = value.trimmed().toUpper();
    if (type == QStringLiteral("IN") || type == QStringLiteral("INPUT"))
        return IR::PinElectricalType::Input;
    if (type == QStringLiteral("OUT") || type == QStringLiteral("OUTPUT"))
        return IR::PinElectricalType::Output;
    if (type == QStringLiteral("BI") || type == QStringLiteral("BIDIRECTIONAL"))
        return IR::PinElectricalType::Bidirectional;
    if (type == QStringLiteral("PASSIVE"))
        return IR::PinElectricalType::Passive;
    if (type == QStringLiteral("POWER") || type == QStringLiteral("PWR"))
        return IR::PinElectricalType::Power;
    return IR::PinElectricalType::Unspecified;
}

/** @brief 依据名称查找唯一焊盘定义。 */
const Parser::CadstarPad* findPad(const Parser::CadstarLibrary& library,
                                  const QString& name,
                                  Parser::ParseDiagnostics* diagnostics) {
    if (library.isPadAmbiguous(name)) {
        if (diagnostics)
            diagnostics->add(Parser::ParseSeverity::Error,
                             Parser::ParseScope::Footprint,
                             QStringLiteral("Cadstar 焊盘引用存在歧义：%1").arg(name),
                             name);
        return nullptr;
    }
    const Parser::CadstarPad* result = nullptr;
    for (const Parser::CadstarPad& pad : library.pads) {
        if (pad.name != name)
            continue;
        if (result != nullptr) {
            if (diagnostics)
                diagnostics->add(Parser::ParseSeverity::Error,
                                 Parser::ParseScope::Footprint,
                                 QStringLiteral("Cadstar 焊盘引用存在歧义：%1").arg(name),
                                 name);
            return nullptr;
        }
        result = &pad;
    }
    if (result == nullptr && diagnostics)
        diagnostics->add(Parser::ParseSeverity::Error,
                         Parser::ParseScope::Footprint,
                         QStringLiteral("Cadstar 封装引用了不存在的焊盘：%1").arg(name),
                         name);
    return result;
}

/** @brief 将 Cadstar 焊盘定义映射为统一焊盘形状和尺寸。 */
IR::FootprintPadIR makePad(const Parser::CadstarPad& source,
                           const Parser::CadstarPackagePin& pin,
                           Parser::LengthUnit unit,
                           Parser::ParseDiagnostics* diagnostics) {
    IR::FootprintPadIR result;
    result.number = pin.number;
    result.position = millimeters(pin.position, unit);
    result.position += millimeters(QPointF(source.offsetX, source.offsetY), unit);
    result.rotation = pin.rotation;
    if (source.holeDiameter > 0.0 || source.holeWidth > 0.0 || source.holeHeight > 0.0) {
        result.padType = IR::PadType::ThroughHole;
        result.layer = IR::LayerType::MultiLayer;
        result.holeSize = millimeters(source.holeDiameter, unit);
        result.holeLength = millimeters(qMax(source.holeWidth, source.holeHeight), unit);
    }
    // 按来源形状设置统一焊盘形状和尺寸。
    switch (source.shape) {
        case Parser::CadstarPadShape::Round:
            result.shape = IR::PadShape::Ellipse;
            result.size = QSizeF(millimeters(source.diameter, unit), millimeters(source.diameter, unit));
            break;
        case Parser::CadstarPadShape::Square:
            result.shape = IR::PadShape::Rect;
            result.size = QSizeF(millimeters(source.width > 0.0 ? source.width : source.diameter, unit),
                                 millimeters(source.height > 0.0 ? source.height : source.diameter, unit));
            break;
        case Parser::CadstarPadShape::Rectangle:
            result.shape = IR::PadShape::Rect;
            result.size = QSizeF(millimeters(source.width, unit), millimeters(source.height, unit));
            break;
        case Parser::CadstarPadShape::Oblong:
            result.shape = IR::PadShape::Oval;
            result.size = QSizeF(millimeters(source.width, unit), millimeters(source.height, unit));
            break;
        case Parser::CadstarPadShape::Octagon:
            result.shape = IR::PadShape::Polygon;
            result.size = QSizeF(millimeters(source.diameter, unit), millimeters(source.diameter, unit));
            break;
        case Parser::CadstarPadShape::Custom:
            result.shape = IR::PadShape::Polygon;
            result.customShapePoints.reserve(source.polygon.size());
            for (const QPointF& point : source.polygon)
                result.customShapePoints.append(millimeters(point, unit));
            break;
        case Parser::CadstarPadShape::Unknown:
            result.shape = IR::PadShape::Ellipse;
            if (diagnostics)
                diagnostics->add(Parser::ParseSeverity::Warning,
                                 Parser::ParseScope::Footprint,
                                 QStringLiteral("Cadstar 未知焊盘形状已降级为圆形：%1").arg(source.name),
                                 source.name);
            break;
    }
    return result;
}

/** @brief 将 Cadstar 图形追加到封装 IR。 */
void appendFootprintGraphic(const Parser::CadstarGraphic& source,
                            Parser::LengthUnit unit,
                            IR::FootprintComponentIR& result) {
    QList<QPointF> converted;
    for (const QPointF& point : source.points)
        converted.append(millimeters(point, unit));
    // 按图形类型写入封装 IR，所有坐标已经统一为毫米。
    switch (source.type) {
        case Parser::CadstarGraphicType::Line:
        case Parser::CadstarGraphicType::Polyline: {
            IR::FootprintTrackIR track;
            track.points = converted;
            track.width = millimeters(source.width, unit);
            track.layer = IR::LayerType::TopSilk;
            result.tracks.append(track);
            break;
        }
        case Parser::CadstarGraphicType::Rectangle:
            if (converted.size() >= 2) {
                IR::FootprintRectangleIR rectangle;
                rectangle.bounds = QRectF(converted.at(0), converted.at(1));
                rectangle.strokeWidth = millimeters(source.width, unit);
                rectangle.layer = IR::LayerType::TopSilk;
                result.rectangles.append(rectangle);
            }
            break;
        case Parser::CadstarGraphicType::Circle:
            if (!converted.isEmpty()) {
                IR::FootprintCircleIR circle;
                circle.center = converted.first();
                circle.radius = millimeters(source.radius, unit);
                circle.strokeWidth = millimeters(source.width, unit);
                circle.layer = IR::LayerType::TopSilk;
                result.circles.append(circle);
            }
            break;
        case Parser::CadstarGraphicType::Arc:
            if (converted.size() >= 3) {
                const QPointF& start = converted.at(0);
                const QPointF& center = converted.at(1);
                const QPointF& end = converted.at(2);
                IR::FootprintArcIR arc;
                arc.center = center;
                arc.radius = QLineF(center, start).length();
                arc.startAngle = qRadiansToDegrees(qAtan2(-(start.y() - center.y()), start.x() - center.x()));
                arc.endAngle = qRadiansToDegrees(qAtan2(-(end.y() - center.y()), end.x() - center.x()));
                arc.width = millimeters(source.width, unit);
                arc.layer = IR::LayerType::TopSilk;
                result.arcs.append(arc);
            }
            break;
        case Parser::CadstarGraphicType::Polygon: {
            IR::FootprintRegionIR region;
            region.vertices = converted;
            region.layer = IR::LayerType::TopSilk;
            result.regions.append(region);
            break;
        }
    }
}

/** @brief 将 Cadstar 图形追加到符号 IR。 */
void appendSymbolGraphic(const Parser::CadstarGraphic& source, Parser::LengthUnit unit, IR::SymbolComponentIR& result) {
    QList<QPointF> converted;
    for (const QPointF& point : source.points)
        converted.append(millimeters(point, unit));
    // 符号图形使用现有 IR 图元，不把 Cadstar 字段带入 IR。
    switch (source.type) {
        case Parser::CadstarGraphicType::Line:
        case Parser::CadstarGraphicType::Polyline: {
            IR::SymbolPolylineIR polyline;
            polyline.points = converted;
            result.polylines.append(polyline);
            break;
        }
        case Parser::CadstarGraphicType::Polygon: {
            IR::SymbolPolygonIR polygon;
            polygon.points = converted;
            polygon.isFilled = true;
            result.polygons.append(polygon);
            break;
        }
        case Parser::CadstarGraphicType::Rectangle:
            if (converted.size() >= 2) {
                const QPointF& first = converted.at(0);
                const QPointF& second = converted.at(1);
                IR::SymbolRectangleIR rectangle;
                rectangle.x0 = first.x();
                rectangle.y0 = first.y();
                rectangle.x1 = second.x();
                rectangle.y1 = second.y();
                result.rectangles.append(rectangle);
            }
            break;
        case Parser::CadstarGraphicType::Circle:
            if (!converted.isEmpty())
                result.circles.append({converted.first(), millimeters(source.radius, unit)});
            break;
        case Parser::CadstarGraphicType::Arc:
            if (converted.size() >= 3)
                result.arcs.append({converted.at(0), converted.at(1), converted.at(2)});
            break;
    }
}

}  // namespace

IR::FootprintComponentIR CadstarAdapter::toFootprint(const Parser::CadstarLibrary& library,
                                                     const Parser::CadstarPackage& packageModel,
                                                     Parser::ParseDiagnostics* diagnostics) {
    IR::FootprintComponentIR result;
    result.name = packageModel.name;
    result.description = packageModel.description;
    for (const Parser::CadstarPackagePin& sourcePin : packageModel.pins) {
        const Parser::CadstarPad* pad = findPad(library, sourcePin.padName, diagnostics);
        if (pad == nullptr)
            continue;
        result.pads.append(makePad(*pad, sourcePin, library.unit, diagnostics));
    }
    for (const Parser::CadstarGraphic& graphic : packageModel.graphics)
        appendFootprintGraphic(graphic, library.unit, result);
    return result;
}

IR::SymbolComponentIR CadstarAdapter::toSymbol(const Parser::CadstarLibrary& library,
                                               const Parser::CadstarComponent& component,
                                               Parser::ParseDiagnostics* diagnostics) {
    Q_UNUSED(diagnostics);
    IR::SymbolComponentIR result;
    result.name = component.name;
    result.partCount = 1;
    result.designatorPrefix = component.properties.value(QStringLiteral("REFDES"));
    for (auto iterator = component.properties.cbegin(); iterator != component.properties.cend(); ++iterator)
        result.sourceMetadata.insert(iterator.key(), iterator.value());
    for (const Parser::CadstarComponentPin& sourcePin : component.pins) {
        IR::SymbolPinIR pin;
        pin.designator = sourcePin.numbers.join(QStringLiteral(","));
        pin.name = sourcePin.label;
        pin.position = millimeters(sourcePin.start, library.unit);
        pin.length =
            QLineF(sourcePin.start, sourcePin.end).length() * Parser::UnitConverter::toMillimeters(1.0, library.unit);
        pin.direction = directionFor(sourcePin.start, sourcePin.end);
        pin.electricalType = electricalTypeFor(sourcePin.pinType);
        pin.style.inverted = sourcePin.inverted;
        pin.display.showName = sourcePin.labelVisible;
        pin.display.showDesignator = sourcePin.numberVisible;
        pin.hasNamePosition = !sourcePin.label.isEmpty();
        pin.namePosition = millimeters(sourcePin.labelPosition, library.unit);
        result.pins.append(pin);
    }
    for (const Parser::CadstarGraphic& graphic : component.graphics)
        appendSymbolGraphic(graphic, library.unit, result);
    return result;
}

CadstarConversionResult CadstarAdapter::toIR(const Parser::CadstarLibrary& library,
                                             Parser::ParseDiagnostics* diagnostics) {
    CadstarConversionResult result;
    for (const Parser::CadstarPackage& packageModel : library.packages)
        result.footprints.append(toFootprint(library, packageModel, diagnostics));
    for (const Parser::CadstarComponent& component : library.components)
        result.symbols.append(toSymbol(library, component, diagnostics));

    for (const Parser::CadstarPart& part : library.parts) {
        const bool componentAmbiguous = library.isComponentAmbiguous(part.componentName);
        const bool packageAmbiguous = library.isPackageAmbiguous(part.packageName);
        bool componentFound = false;
        bool packageFound = false;
        for (const Parser::CadstarComponent& component : library.components)
            componentFound = componentFound || component.name == part.componentName;
        for (const Parser::CadstarPackage& packageModel : library.packages)
            packageFound = packageFound || packageModel.name == part.packageName;
        if (diagnostics && componentAmbiguous)
            diagnostics->add(Parser::ParseSeverity::Error,
                             Parser::ParseScope::Component,
                             QStringLiteral("Cadstar Part 符号关联存在歧义：%1").arg(part.componentName),
                             part.name);
        if (diagnostics && packageAmbiguous)
            diagnostics->add(Parser::ParseSeverity::Error,
                             Parser::ParseScope::Footprint,
                             QStringLiteral("Cadstar Part 封装关联存在歧义：%1").arg(part.packageName),
                             part.name);
        if (diagnostics && !componentFound)
            diagnostics->add(Parser::ParseSeverity::Error,
                             Parser::ParseScope::Component,
                             QStringLiteral("Cadstar Part 缺少符号关联：%1").arg(part.componentName),
                             part.name);
        if (diagnostics && !packageFound)
            diagnostics->add(Parser::ParseSeverity::Error,
                             Parser::ParseScope::Footprint,
                             QStringLiteral("Cadstar Part 缺少封装关联：%1").arg(part.packageName),
                             part.name);

        if (componentAmbiguous || packageAmbiguous || !componentFound || !packageFound)
            continue;

        const Parser::CadstarComponent* sourceComponent = nullptr;
        const Parser::CadstarPackage* sourcePackage = nullptr;
        for (const Parser::CadstarComponent& component : library.components) {
            if (component.name == part.componentName) {
                sourceComponent = &component;
                break;
            }
        }
        for (const Parser::CadstarPackage& packageModel : library.packages) {
            if (packageModel.name == part.packageName) {
                sourcePackage = &packageModel;
                break;
            }
        }
        if (sourceComponent == nullptr || sourcePackage == nullptr)
            continue;

        IR::ComponentIR component;
        component.name = part.name;
        component.description = part.description;
        component.package = part.packageName;
        component.symbol = toSymbol(library, *sourceComponent, diagnostics);
        component.footprint = toFootprint(library, *sourcePackage, diagnostics);
        for (auto iterator = part.properties.cbegin(); iterator != part.properties.cend(); ++iterator)
            component.sourceMetadata.insert(iterator.key(), iterator.value());
        result.components.append(component);
    }
    return result;
}

}  // namespace EasyKiConverter
