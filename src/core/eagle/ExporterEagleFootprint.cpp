#include "ExporterEagleFootprint.h"

#include <QFile>
#include <QFileInfo>
#include <QLineF>
#include <QSet>
#include <QTextStream>
#include <QXmlStreamWriter>
#include <QtMath>

#include <cmath>
#include <optional>

namespace EasyKiConverter {
namespace {

/** 将用户名称清洗为 Eagle XML 可安全使用的库对象名称。 */
QString safeName(const QString& value) {
    QString result;
    for (const QChar character : value) {
        const ushort code = character.unicode();
        if ((code >= 'A' && code <= 'Z') || (code >= 'a' && code <= 'z') || (code >= '0' && code <= '9') ||
            character == QChar('_') || character == QChar('-'))
            result.append(character);
        else
            result.append(QChar('_'));
    }
    return result.left(64);
}

/** 使用固定小数精度序列化 Eagle XML 数值。 */
QString number(double value) {
    return QString::number(value, 'f', 6);
}

/** 将角度和镜像标志转换为 Eagle 旋转属性。 */
QString rotation(double value, bool mirror = false) {
    return QStringLiteral("%1%2").arg(mirror ? QStringLiteral("MR") : QStringLiteral("R"), number(value));
}

/** 将统一 IR 图层语义映射为 Eagle 图层编号。 */
std::optional<int> layerNumber(IR::LayerType layer) {
    // Eagle 的图层编号由目标格式固定定义，未知语义必须返回空值。
    switch (layer) {
        case IR::LayerType::TopCopper:
            return 1;
        case IR::LayerType::BottomCopper:
            return 16;
        case IR::LayerType::TopSilk:
            return 21;
        case IR::LayerType::BottomSilk:
            return 22;
        case IR::LayerType::TopMask:
            return 29;
        case IR::LayerType::BottomMask:
            return 30;
        case IR::LayerType::TopPaste:
            return 31;
        case IR::LayerType::BottomPaste:
            return 32;
        case IR::LayerType::TopAssembly:
            return 51;
        case IR::LayerType::BottomAssembly:
            return 52;
        case IR::LayerType::KeepOut:
            return 39;
        case IR::LayerType::EdgeCuts:
            return 46;
        default:
            return std::nullopt;
    }
}

/** 将统一 IR 圆弧转换为 Eagle XML 的带 curve 属性的 wire。 */
bool writeArc(QXmlStreamWriter& xml, const IR::FootprintArcIR& arc, QStringList& diagnostics) {
    if (!std::isfinite(arc.center.x()) || !std::isfinite(arc.center.y()) || !std::isfinite(arc.radius) ||
        !std::isfinite(arc.startAngle) || !std::isfinite(arc.endAngle) || !std::isfinite(arc.width) ||
        arc.radius <= 0.0 || arc.width < 0.0) {
        diagnostics.append(QStringLiteral("Eagle: 圆弧包含非法圆心、半径、角度或线宽"));
        return false;
    }
    const auto layer = layerNumber(arc.layer);
    if (!layer.has_value()) {
        diagnostics.append(QStringLiteral("Eagle: 圆弧使用无法映射的图层"));
        return false;
    }

    double sweep = arc.endAngle - arc.startAngle;
    while (sweep <= -360.0)
        sweep += 360.0;
    while (sweep > 360.0)
        sweep -= 360.0;
    if (qFuzzyIsNull(sweep)) {
        diagnostics.append(QStringLiteral("Eagle: 圆弧起止角度相同，无法区分零弧和整圆"));
        return false;
    }

    const double startRadians = qDegreesToRadians(arc.startAngle);
    const double endRadians = qDegreesToRadians(arc.startAngle + sweep);
    const QPointF start =
        arc.center + QPointF(arc.radius * std::cos(startRadians), arc.radius * std::sin(startRadians));
    const QPointF end = arc.center + QPointF(arc.radius * std::cos(endRadians), arc.radius * std::sin(endRadians));

    xml.writeEmptyElement(QStringLiteral("wire"));
    xml.writeAttribute(QStringLiteral("x1"), number(start.x()));
    xml.writeAttribute(QStringLiteral("y1"), number(start.y()));
    xml.writeAttribute(QStringLiteral("x2"), number(end.x()));
    xml.writeAttribute(QStringLiteral("y2"), number(end.y()));
    xml.writeAttribute(QStringLiteral("width"), number(arc.width));
    xml.writeAttribute(QStringLiteral("layer"), QString::number(*layer));
    xml.writeAttribute(QStringLiteral("curve"), number(sweep));
    return true;
}

/** 将统一封装 IR 写入 Eagle package，并拒绝无法无损表达的字段。 */
bool writePackage(QXmlStreamWriter& xml, const IR::FootprintComponentIR& footprint, QStringList& diagnostics) {
    const QString name = safeName(footprint.name);
    if (name.isEmpty()) {
        diagnostics.append(QStringLiteral("Eagle: 封装名称清洗后为空"));
        return false;
    }
    for (const IR::FootprintPadIR& pad : footprint.pads) {
        if (pad.number.isEmpty()) {
            diagnostics.append(QStringLiteral("Eagle: 封装 %1 存在空焊盘编号").arg(name));
            return false;
        }
        if (!std::isfinite(pad.position.x()) || !std::isfinite(pad.position.y()) || !std::isfinite(pad.size.width()) ||
            !std::isfinite(pad.size.height())) {
            diagnostics.append(QStringLiteral("Eagle: 焊盘 %1 包含非法坐标或尺寸").arg(pad.number));
            return false;
        }
        if (pad.shape == IR::PadShape::Polygon || pad.shape == IR::PadShape::Trapezoid || pad.holeLength > 0.0) {
            diagnostics.append(QStringLiteral("Eagle: 焊盘 %1 的异形或槽孔语义当前无法无损表达").arg(pad.number));
            return false;
        }
        if (pad.isSmd() && pad.layer != IR::LayerType::TopCopper && pad.layer != IR::LayerType::BottomCopper) {
            diagnostics.append(QStringLiteral("Eagle: SMD 焊盘 %1 使用无法映射到 Eagle 铜层的图层").arg(pad.number));
            return false;
        }
        if (pad.isThroughHole() && pad.shape != IR::PadShape::Rect && pad.shape != IR::PadShape::Ellipse &&
            pad.shape != IR::PadShape::Oval) {
            diagnostics.append(QStringLiteral("Eagle: 通孔焊盘 %1 的形状无法无损表达").arg(pad.number));
            return false;
        }
        if (pad.isThroughHole() && (pad.shape == IR::PadShape::Ellipse || pad.shape == IR::PadShape::Oval) &&
            qAbs(pad.size.width() - pad.size.height()) > 1e-6) {
            diagnostics.append(
                QStringLiteral("Eagle: 通孔焊盘 %1 的非圆形孔盘无法由 Eagle pad 无损表达").arg(pad.number));
            return false;
        }
    }
    for (const IR::FootprintTrackIR& track : footprint.tracks) {
        if (!layerNumber(track.layer).has_value()) {
            diagnostics.append(QStringLiteral("Eagle: 走线使用无法映射的图层"));
            return false;
        }
    }
    if (!footprint.models3d.isEmpty())
        diagnostics.append(
            QStringLiteral("Eagle: package XML 不包含三维模型关联，已跳过 %1 个模型").arg(footprint.models3d.size()));

    xml.writeStartElement(QStringLiteral("package"));
    xml.writeAttribute(QStringLiteral("name"), name);
    for (const IR::FootprintCircleIR& circle : footprint.circles) {
        const auto layer = layerNumber(circle.layer);
        if (!layer.has_value()) {
            diagnostics.append(QStringLiteral("Eagle: 圆形图元使用无法映射的图层"));
            return false;
        }
        xml.writeStartElement(QStringLiteral("circle"));
        xml.writeAttribute(QStringLiteral("x"), number(circle.center.x()));
        xml.writeAttribute(QStringLiteral("y"), number(circle.center.y()));
        xml.writeAttribute(QStringLiteral("radius"), number(circle.radius));
        xml.writeAttribute(QStringLiteral("width"), number(circle.strokeWidth));
        xml.writeAttribute(QStringLiteral("layer"), QString::number(*layer));
        xml.writeEndElement();
    }
    for (const IR::FootprintArcIR& arc : footprint.arcs) {
        if (!writeArc(xml, arc, diagnostics))
            return false;
    }
    for (const IR::FootprintRectangleIR& rectangle : footprint.rectangles) {
        const auto layer = layerNumber(rectangle.layer);
        if (!layer.has_value() || !std::isfinite(rectangle.rotation)) {
            diagnostics.append(QStringLiteral("Eagle: 矩形图元使用无法映射的图层"));
            return false;
        }
        const QRectF bounds = rectangle.bounds;
        const QPointF center = bounds.center();
        const double radians = qDegreesToRadians(rectangle.rotation);
        const double cosine = std::cos(radians);
        const double sine = std::sin(radians);
        const auto rotate = [center, cosine, sine](const QPointF& point) {
            const QPointF offset = point - center;
            return center + QPointF(offset.x() * cosine - offset.y() * sine, offset.x() * sine + offset.y() * cosine);
        };
        const QList<QPointF> corners = {rotate(bounds.topLeft()),
                                        rotate(bounds.topRight()),
                                        rotate(bounds.bottomRight()),
                                        rotate(bounds.bottomLeft()),
                                        rotate(bounds.topLeft())};
        for (int index = 0; index < corners.size() - 1; ++index) {
            xml.writeStartElement(QStringLiteral("wire"));
            xml.writeAttribute(QStringLiteral("x1"), number(corners.at(index).x()));
            xml.writeAttribute(QStringLiteral("y1"), number(corners.at(index).y()));
            xml.writeAttribute(QStringLiteral("x2"), number(corners.at(index + 1).x()));
            xml.writeAttribute(QStringLiteral("y2"), number(corners.at(index + 1).y()));
            xml.writeAttribute(QStringLiteral("width"), number(rectangle.strokeWidth));
            xml.writeAttribute(QStringLiteral("layer"), QString::number(*layer));
            xml.writeEndElement();
        }
    }
    for (const IR::FootprintTrackIR& track : footprint.tracks) {
        if (track.points.isEmpty())
            continue;
        if (track.points.size() < 2) {
            diagnostics.append(QStringLiteral("Eagle: 走线至少需要两个点，不能静默丢弃"));
            return false;
        }
        const int layer = *layerNumber(track.layer);
        for (int index = 0; index < track.points.size() - 1; ++index) {
            const QPointF& first = track.points.at(index);
            const QPointF& second = track.points.at(index + 1);
            xml.writeStartElement(QStringLiteral("wire"));
            xml.writeAttribute(QStringLiteral("x1"), number(first.x()));
            xml.writeAttribute(QStringLiteral("y1"), number(first.y()));
            xml.writeAttribute(QStringLiteral("x2"), number(second.x()));
            xml.writeAttribute(QStringLiteral("y2"), number(second.y()));
            xml.writeAttribute(QStringLiteral("width"), number(track.width));
            xml.writeAttribute(QStringLiteral("layer"), QString::number(layer));
            xml.writeEndElement();
        }
    }
    for (const IR::FootprintRegionIR& region : footprint.regions) {
        if (region.vertices.isEmpty())
            continue;
        if (region.vertices.size() < 3) {
            diagnostics.append(QStringLiteral("Eagle: 区域至少需要三个顶点，不能静默丢弃"));
            return false;
        }
        const auto layer = layerNumber(region.layer);
        if (!layer.has_value()) {
            diagnostics.append(QStringLiteral("Eagle: 区域使用无法映射的图层"));
            return false;
        }
        for (int index = 0; index < region.vertices.size(); ++index) {
            const QPointF& first = region.vertices.at(index);
            const QPointF& second = region.vertices.at((index + 1) % region.vertices.size());
            xml.writeStartElement(QStringLiteral("wire"));
            xml.writeAttribute(QStringLiteral("x1"), number(first.x()));
            xml.writeAttribute(QStringLiteral("y1"), number(first.y()));
            xml.writeAttribute(QStringLiteral("x2"), number(second.x()));
            xml.writeAttribute(QStringLiteral("y2"), number(second.y()));
            xml.writeAttribute(QStringLiteral("width"), QStringLiteral("0"));
            xml.writeAttribute(QStringLiteral("layer"), QString::number(*layer));
            xml.writeEndElement();
        }
    }
    for (const IR::FootprintTextIR& text : footprint.texts) {
        if (!text.isDisplayed || text.text.isEmpty())
            continue;
        const auto layer = layerNumber(text.layer);
        if (!layer.has_value()) {
            diagnostics.append(QStringLiteral("Eagle: 文本使用无法映射的图层"));
            return false;
        }
        xml.writeStartElement(QStringLiteral("text"));
        xml.writeAttribute(QStringLiteral("x"), number(text.position.x()));
        xml.writeAttribute(QStringLiteral("y"), number(text.position.y()));
        xml.writeAttribute(QStringLiteral("size"), number(text.fontSize > 0.0 ? text.fontSize : 1.0));
        xml.writeAttribute(QStringLiteral("layer"), QString::number(*layer));
        xml.writeAttribute(QStringLiteral("rot"), rotation(text.rotation, text.mirror));
        xml.writeCharacters(text.text);
        xml.writeEndElement();
    }
    for (const IR::FootprintHoleIR& hole : footprint.holes) {
        xml.writeStartElement(QStringLiteral("hole"));
        xml.writeAttribute(QStringLiteral("x"), number(hole.center.x()));
        xml.writeAttribute(QStringLiteral("y"), number(hole.center.y()));
        xml.writeAttribute(QStringLiteral("drill"), number(hole.radius * 2.0));
        xml.writeEndElement();
    }
    for (const IR::FootprintPadIR& pad : footprint.pads) {
        xml.writeStartElement(pad.isThroughHole() ? QStringLiteral("pad") : QStringLiteral("smd"));
        xml.writeAttribute(QStringLiteral("name"), pad.number);
        xml.writeAttribute(QStringLiteral("x"), number(pad.position.x()));
        xml.writeAttribute(QStringLiteral("y"), number(pad.position.y()));
        xml.writeAttribute(QStringLiteral("rot"), rotation(pad.rotation));
        if (pad.isThroughHole()) {
            xml.writeAttribute(QStringLiteral("drill"), number(pad.holeSize));
            xml.writeAttribute(QStringLiteral("diameter"), number(qMax(pad.size.width(), pad.size.height())));
            xml.writeAttribute(QStringLiteral("shape"),
                               pad.shape == IR::PadShape::Rect ? QStringLiteral("square") : QStringLiteral("round"));
        } else {
            xml.writeAttribute(QStringLiteral("dx"), number(pad.size.width()));
            xml.writeAttribute(QStringLiteral("dy"), number(pad.size.height()));
            xml.writeAttribute(QStringLiteral("layer"),
                               pad.layer == IR::LayerType::BottomCopper ? QStringLiteral("16") : QStringLiteral("1"));
            if (pad.shape == IR::PadShape::RoundRect)
                xml.writeAttribute(QStringLiteral("roundness"), QStringLiteral("100"));
            else if (pad.shape == IR::PadShape::Ellipse || pad.shape == IR::PadShape::Oval)
                xml.writeAttribute(QStringLiteral("roundness"), QStringLiteral("100"));
            else if (pad.shape != IR::PadShape::Rect)
                xml.writeAttribute(QStringLiteral("roundness"), QStringLiteral("0"));
        }
        xml.writeEndElement();
    }
    xml.writeEndElement();
    return !xml.hasError();
}

// 写入 Eagle XML 中所有本导出器会使用的标准图层定义。
void writeLayerTable(QXmlStreamWriter& xml) {
    xml.writeStartElement(QStringLiteral("layers"));
    const QList<QPair<int, QString>> layers = {{1, "Top"},
                                               {16, "Bottom"},
                                               {21, "tPlace"},
                                               {22, "bPlace"},
                                               {29, "tStop"},
                                               {30, "bStop"},
                                               {31, "tCream"},
                                               {32, "bCream"},
                                               {39, "tKeepout"},
                                               {46, "Milling"},
                                               {51, "tDocu"},
                                               {52, "bDocu"},
                                               {94, "Symbols"},
                                               {95, "Names"},
                                               {96, "Values"}};
    for (const auto& layer : layers) {
        xml.writeEmptyElement(QStringLiteral("layer"));
        xml.writeAttribute(QStringLiteral("number"), QString::number(layer.first));
        xml.writeAttribute(QStringLiteral("name"), layer.second);
        xml.writeAttribute(QStringLiteral("color"), QStringLiteral("4"));
        xml.writeAttribute(QStringLiteral("fill"), QStringLiteral("1"));
        xml.writeAttribute(QStringLiteral("visible"), QStringLiteral("yes"));
        xml.writeAttribute(QStringLiteral("active"), QStringLiteral("yes"));
    }
    xml.writeEndElement();
}

/** 将统一 IR 的符号方向转换为 Eagle pin 的旋转文本。 */
QString symbolPinRotation(IR::PinDirection direction) {
    // Eagle 使用角度字符串表示引脚朝向，统一 IR 的右向作为默认方向。
    switch (direction) {
        case IR::PinDirection::Left:
            return QStringLiteral("R180");
        case IR::PinDirection::Up:
            return QStringLiteral("R90");
        case IR::PinDirection::Down:
            return QStringLiteral("R270");
        case IR::PinDirection::Right:
        default:
            return QStringLiteral("R0");
    }
}

/** 将三点圆弧转换为 Eagle Symbol wire 所需的端点和扫掠角。 */
bool writeSymbolArc(QXmlStreamWriter& xml, const IR::SymbolArcIR& arc, QStringList& diagnostics) {
    if (arc.isFilled || arc.strokeStyle != IR::StrokeStyle::Solid) {
        diagnostics.append(QStringLiteral("Eagle: 符号圆弧的填充或非实线样式无法由 XML wire 无损表达"));
        return false;
    }
    const QPointF& start = arc.startPoint;
    const QPointF& middle = arc.midPoint;
    const QPointF& end = arc.endPoint;
    const double determinant = 2.0 * (start.x() * (middle.y() - end.y()) + middle.x() * (end.y() - start.y()) +
                                      end.x() * (start.y() - middle.y()));
    if (!std::isfinite(determinant) || qFuzzyIsNull(determinant) || !std::isfinite(arc.strokeWidth) ||
        arc.strokeWidth < 0.0) {
        diagnostics.append(QStringLiteral("Eagle: 符号圆弧三点共线或包含非法线宽"));
        return false;
    }

    const double startSquare = QPointF::dotProduct(start, start);
    const double middleSquare = QPointF::dotProduct(middle, middle);
    const double endSquare = QPointF::dotProduct(end, end);
    const QPointF center((startSquare * (middle.y() - end.y()) + middleSquare * (end.y() - start.y()) +
                          endSquare * (start.y() - middle.y())) /
                             determinant,
                         (startSquare * (end.x() - middle.x()) + middleSquare * (start.x() - end.x()) +
                          endSquare * (middle.x() - start.x())) /
                             determinant);
    const double radius = QLineF(center, start).length();
    const double middleRadius = QLineF(center, middle).length();
    if (!std::isfinite(radius) || !std::isfinite(middleRadius) || radius <= 0.0 ||
        qAbs(radius - middleRadius) > qMax(1e-6, radius * 1e-6)) {
        diagnostics.append(QStringLiteral("Eagle: 符号圆弧三点无法构成有效圆"));
        return false;
    }

    const auto angle = [&center](const QPointF& point) {
        return qRadiansToDegrees(std::atan2(point.y() - center.y(), point.x() - center.x()));
    };
    const double startAngle = angle(start);
    const double middleAngle = angle(middle);
    const double endAngle = angle(end);
    double counterClockwise = std::fmod(endAngle - startAngle + 360.0, 360.0);
    if (qFuzzyIsNull(counterClockwise))
        counterClockwise = 360.0;
    const double middleSweep = std::fmod(middleAngle - startAngle + 360.0, 360.0);
    const double sweep = middleSweep <= counterClockwise ? counterClockwise : counterClockwise - 360.0;

    xml.writeEmptyElement(QStringLiteral("wire"));
    xml.writeAttribute(QStringLiteral("x1"), number(start.x()));
    xml.writeAttribute(QStringLiteral("y1"), number(start.y()));
    xml.writeAttribute(QStringLiteral("x2"), number(end.x()));
    xml.writeAttribute(QStringLiteral("y2"), number(end.y()));
    xml.writeAttribute(QStringLiteral("width"), number(arc.strokeWidth));
    xml.writeAttribute(QStringLiteral("layer"), QStringLiteral("94"));
    xml.writeAttribute(QStringLiteral("curve"), number(sweep));
    return true;
}

// 写入 Eagle symbol 的可直接表达图元，并拒绝无法无损转换的曲线。
bool writeSymbol(QXmlStreamWriter& xml,
                 const IR::SymbolComponentIR& symbol,
                 int partIndex,
                 const QString& symbolName,
                 QStringList& diagnostics) {
    xml.writeStartElement(QStringLiteral("symbol"));
    xml.writeAttribute(QStringLiteral("name"), symbolName);
    if (!symbol.description.isEmpty()) {
        xml.writeStartElement(QStringLiteral("description"));
        xml.writeCharacters(symbol.description);
        xml.writeEndElement();
    }

    const auto writeWire = [&xml](const QPointF& first, const QPointF& second, double width) {
        xml.writeEmptyElement(QStringLiteral("wire"));
        xml.writeAttribute(QStringLiteral("x1"), number(first.x()));
        xml.writeAttribute(QStringLiteral("y1"), number(first.y()));
        xml.writeAttribute(QStringLiteral("x2"), number(second.x()));
        xml.writeAttribute(QStringLiteral("y2"), number(second.y()));
        xml.writeAttribute(QStringLiteral("width"), number(width));
        xml.writeAttribute(QStringLiteral("layer"), QStringLiteral("94"));
    };

    for (const IR::SymbolRectangleIR& rectangle : symbol.rectangles) {
        if (rectangle.partIndex != partIndex)
            continue;
        if (rectangle.isFilled || rectangle.strokeStyle != IR::StrokeStyle::Solid) {
            diagnostics.append(QStringLiteral("Eagle: 符号矩形的填充或非实线样式无法由 XML wire 无损表达"));
            return false;
        }
        writeWire({rectangle.x0, rectangle.y0}, {rectangle.x1, rectangle.y0}, rectangle.strokeWidth);
        writeWire({rectangle.x1, rectangle.y0}, {rectangle.x1, rectangle.y1}, rectangle.strokeWidth);
        writeWire({rectangle.x1, rectangle.y1}, {rectangle.x0, rectangle.y1}, rectangle.strokeWidth);
        writeWire({rectangle.x0, rectangle.y1}, {rectangle.x0, rectangle.y0}, rectangle.strokeWidth);
    }
    for (const IR::SymbolCircleIR& circle : symbol.circles) {
        if (circle.partIndex != partIndex)
            continue;
        if (circle.isFilled || circle.strokeStyle != IR::StrokeStyle::Solid) {
            diagnostics.append(QStringLiteral("Eagle: 符号圆形的填充或非实线样式无法由 XML circle 无损表达"));
            return false;
        }
        xml.writeEmptyElement(QStringLiteral("circle"));
        xml.writeAttribute(QStringLiteral("x"), number(circle.center.x()));
        xml.writeAttribute(QStringLiteral("y"), number(circle.center.y()));
        xml.writeAttribute(QStringLiteral("radius"), number(circle.radius));
        xml.writeAttribute(QStringLiteral("width"), number(circle.strokeWidth));
        xml.writeAttribute(QStringLiteral("layer"), QStringLiteral("94"));
    }
    const auto writePointList = [&writeWire](const QList<QPointF>& points, double width) {
        for (int index = 0; index + 1 < points.size(); ++index)
            writeWire(points.at(index), points.at(index + 1), width);
    };
    for (const IR::SymbolPolylineIR& polyline : symbol.polylines) {
        if (polyline.partIndex == partIndex && (polyline.isFilled || polyline.strokeStyle != IR::StrokeStyle::Solid)) {
            diagnostics.append(QStringLiteral("Eagle: 符号折线的填充或非实线样式无法由 XML wire 无损表达"));
            return false;
        }
        if (polyline.partIndex == partIndex)
            writePointList(polyline.points, polyline.strokeWidth);
    }
    for (const IR::SymbolPolygonIR& polygon : symbol.polygons) {
        if (polygon.partIndex != partIndex || polygon.points.size() < 2)
            continue;
        if (polygon.isFilled || polygon.strokeStyle != IR::StrokeStyle::Solid) {
            diagnostics.append(QStringLiteral("Eagle: 符号多边形的填充或非实线样式无法由 XML wire 无损表达"));
            return false;
        }
        QList<QPointF> closed = polygon.points;
        closed.append(polygon.points.first());
        writePointList(closed, polygon.strokeWidth);
    }
    for (const IR::SymbolPathIR& path : symbol.paths) {
        if (path.partIndex == partIndex && !path.segments.isEmpty()) {
            diagnostics.append(
                QStringLiteral("Eagle: 符号 %1 含路径曲线，当前 XML writer 无法无损表达").arg(symbol.name));
            return false;
        }
        if (path.partIndex == partIndex && (path.isFilled || path.strokeStyle != IR::StrokeStyle::Solid)) {
            diagnostics.append(QStringLiteral("Eagle: 符号路径的填充或非实线样式无法由 XML wire 无损表达"));
            return false;
        }
        if (path.partIndex == partIndex)
            writePointList(path.points, path.strokeWidth);
    }
    for (const IR::SymbolArcIR& arc : symbol.arcs) {
        if (arc.partIndex == partIndex) {
            if (!writeSymbolArc(xml, arc, diagnostics))
                return false;
        }
    }
    for (const IR::SymbolPinIR& pin : symbol.pins) {
        if (pin.partIndex != partIndex && !pin.commonToAllParts)
            continue;
        if (pin.name.isEmpty() || pin.designator.isEmpty()) {
            diagnostics.append(QStringLiteral("Eagle: 符号 %1 存在空引脚名称或编号").arg(symbol.name));
            return false;
        }
        xml.writeEmptyElement(QStringLiteral("pin"));
        xml.writeAttribute(QStringLiteral("name"), pin.name);
        xml.writeAttribute(QStringLiteral("x"), number(pin.position.x()));
        xml.writeAttribute(QStringLiteral("y"), number(pin.position.y()));
        xml.writeAttribute(QStringLiteral("length"), number(pin.length > 0.0 ? pin.length : 2.54));
        xml.writeAttribute(QStringLiteral("rot"), symbolPinRotation(pin.direction));
    }
    for (const IR::SymbolTextIR& text : symbol.texts) {
        if (!text.visible || text.partIndex != partIndex || text.text.isEmpty())
            continue;
        xml.writeStartElement(QStringLiteral("text"));
        xml.writeAttribute(QStringLiteral("x"), number(text.position.x()));
        xml.writeAttribute(QStringLiteral("y"), number(text.position.y()));
        xml.writeAttribute(QStringLiteral("size"), number(text.fontSizeMm > 0.0 ? text.fontSizeMm : 1.27));
        xml.writeAttribute(QStringLiteral("layer"), QStringLiteral("94"));
        xml.writeAttribute(QStringLiteral("rot"), rotation(text.rotation));
        xml.writeCharacters(text.text);
        xml.writeEndElement();
    }
    xml.writeEndElement();
    return !xml.hasError();
}

// 写入包含符号、封装和引脚映射的 Eagle 完整 XML 库。
bool writeComponentLibrary(QXmlStreamWriter& xml, const QList<IR::ComponentIR>& components, QStringList& diagnostics) {
    QSet<QString> symbolNames;
    QSet<QString> packageNames;
    QSet<QString> deviceNames;
    for (const IR::ComponentIR& component : components) {
        const QString baseSymbolName = safeName(component.symbol.name);
        const QString packageName = safeName(component.footprint.name);
        const QString deviceName = safeName(component.name.isEmpty() ? component.symbol.name : component.name);
        if (baseSymbolName.isEmpty() || packageName.isEmpty() || deviceName.isEmpty() ||
            packageNames.contains(packageName) || deviceNames.contains(deviceName)) {
            diagnostics.append(QStringLiteral("Eagle: 符号或封装名称为空或清洗后冲突：%1").arg(component.name));
            return false;
        }
        if (component.symbol.partCount < 1) {
            diagnostics.append(QStringLiteral("Eagle: 组件 %1 的符号部件数非法").arg(component.name));
            return false;
        }
        if (!component.hasSymbol() || !component.hasFootprint()) {
            diagnostics.append(QStringLiteral("Eagle: 组件 %1 缺少符号或封装，无法建立 DeviceSet").arg(component.name));
            return false;
        }
        for (int partIndex = 0; partIndex < component.symbol.partCount; ++partIndex) {
            const QString symbolName =
                partIndex == 0 ? baseSymbolName : baseSymbolName + QStringLiteral("_P%1").arg(partIndex + 1);
            if (symbolNames.contains(symbolName)) {
                diagnostics.append(QStringLiteral("Eagle: 符号名称冲突：%1").arg(symbolName));
                return false;
            }
            symbolNames.insert(symbolName);
        }
        packageNames.insert(packageName);
        deviceNames.insert(deviceName);
    }

    xml.writeStartElement(QStringLiteral("symbols"));
    for (const IR::ComponentIR& component : components) {
        for (int partIndex = 0; partIndex < component.symbol.partCount; ++partIndex) {
            const QString symbolName =
                partIndex == 0 ? safeName(component.symbol.name)
                               : safeName(component.symbol.name) + QStringLiteral("_P%1").arg(partIndex + 1);
            if (!writeSymbol(xml, component.symbol, partIndex, symbolName, diagnostics))
                return false;
        }
    }
    xml.writeEndElement();

    xml.writeStartElement(QStringLiteral("devicesets"));
    for (const IR::ComponentIR& component : components) {
        const QString deviceName = safeName(component.name.isEmpty() ? component.symbol.name : component.name);
        xml.writeStartElement(QStringLiteral("deviceset"));
        xml.writeAttribute(QStringLiteral("name"), deviceName);
        xml.writeAttribute(
            QStringLiteral("prefix"),
            component.symbol.designatorPrefix.isEmpty() ? QStringLiteral("U") : component.symbol.designatorPrefix);
        xml.writeStartElement(QStringLiteral("gates"));
        for (int partIndex = 0; partIndex < component.symbol.partCount; ++partIndex) {
            const QString gateName = QStringLiteral("G$%1").arg(partIndex + 1);
            const QString symbolName =
                partIndex == 0 ? safeName(component.symbol.name)
                               : safeName(component.symbol.name) + QStringLiteral("_P%1").arg(partIndex + 1);
            xml.writeEmptyElement(QStringLiteral("gate"));
            xml.writeAttribute(QStringLiteral("name"), gateName);
            xml.writeAttribute(QStringLiteral("symbol"), symbolName);
            xml.writeAttribute(QStringLiteral("x"), QStringLiteral("0"));
            xml.writeAttribute(QStringLiteral("y"), QStringLiteral("0"));
        }
        xml.writeEndElement();
        xml.writeStartElement(QStringLiteral("devices"));
        xml.writeStartElement(QStringLiteral("device"));
        xml.writeAttribute(QStringLiteral("name"), QString());
        xml.writeAttribute(QStringLiteral("package"), safeName(component.footprint.name));
        xml.writeStartElement(QStringLiteral("connects"));
        for (int partIndex = 0; partIndex < component.symbol.partCount; ++partIndex) {
            const QString gateName = QStringLiteral("G$%1").arg(partIndex + 1);
            for (const IR::SymbolPinIR& pin : component.symbol.pins) {
                if (pin.partIndex != partIndex && !pin.commonToAllParts)
                    continue;
                bool padExists = false;
                for (const IR::FootprintPadIR& pad : component.footprint.pads) {
                    if (pad.number == pin.designator) {
                        padExists = true;
                        break;
                    }
                }
                if (!padExists) {
                    diagnostics.append(
                        QStringLiteral("Eagle: 组件 %1 的引脚 %2 找不到对应焊盘").arg(component.name, pin.designator));
                    return false;
                }
                xml.writeEmptyElement(QStringLiteral("connect"));
                xml.writeAttribute(QStringLiteral("gate"), gateName);
                xml.writeAttribute(QStringLiteral("pin"), pin.name);
                xml.writeAttribute(QStringLiteral("pad"), pin.designator);
            }
        }
        xml.writeEndElement();
        xml.writeStartElement(QStringLiteral("technologies"));
        xml.writeEmptyElement(QStringLiteral("technology"));
        xml.writeAttribute(QStringLiteral("name"), QString());
        xml.writeEndElement();
        xml.writeEndElement();
        xml.writeEndElement();
        xml.writeEndElement();
        xml.writeEndElement();
    }
    xml.writeEndElement();
    return !xml.hasError();
}

bool writeLibrary(const QList<IR::FootprintComponentIR>& footprints,
                  const QString& filePath,
                  const QString& description,
                  QStringList& diagnostics) {
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        diagnostics.append(QStringLiteral("Eagle: 无法写入文件 %1").arg(filePath));
        return false;
    }
    QXmlStreamWriter xml(&file);
    xml.setAutoFormatting(true);
    xml.writeStartDocument();
    xml.writeStartElement(QStringLiteral("eagle"));
    xml.writeAttribute(QStringLiteral("version"), QStringLiteral("9.6.2"));
    xml.writeStartElement(QStringLiteral("drawing"));
    xml.writeStartElement(QStringLiteral("settings"));
    xml.writeEmptyElement(QStringLiteral("setting"));
    xml.writeAttribute(QStringLiteral("alwaysvectorfont"), QStringLiteral("no"));
    xml.writeEndElement();
    writeLayerTable(xml);
    xml.writeStartElement(QStringLiteral("library"));
    xml.writeStartElement(QStringLiteral("description"));
    xml.writeCharacters(description);
    xml.writeEndElement();
    xml.writeStartElement(QStringLiteral("packages"));
    for (const auto& footprint : footprints) {
        if (!writePackage(xml, footprint, diagnostics))
            return false;
    }
    xml.writeEndElement();
    xml.writeEndElement();
    xml.writeEndElement();
    xml.writeEndElement();
    xml.writeEndDocument();
    return !xml.hasError();
}

}  // namespace

/** 返回 Eagle XML 组合库的文件扩展名。 */
QString ExporterEagleFootprint::libraryFileExtension() const {
    return QStringLiteral(".lbr");
}

/** Eagle XML 组合库使用单文件输出，而不是目录输出。 */
bool ExporterEagleFootprint::isDirectoryOutput() const {
    return false;
}

bool ExporterEagleFootprint::exportFootprint(const IR::FootprintComponentIR& footprint,
                                             const QString& filePath,
                                             const QString&) {
    m_diagnostics.clear();
    return writeLibrary({footprint}, filePath, footprint.description, m_diagnostics);
}

bool ExporterEagleFootprint::exportFootprintLibrary(const QList<IR::FootprintComponentIR>& footprints,
                                                    const QString&,
                                                    const QString& filePath,
                                                    bool,
                                                    bool,
                                                    const QString& libraryDescription,
                                                    const QString&,
                                                    bool,
                                                    const QString&) {
    m_diagnostics.clear();
    if (footprints.isEmpty()) {
        m_diagnostics.append(QStringLiteral("Eagle: 没有可导出的封装"));
        return false;
    }
    QSet<QString> names;
    for (const auto& footprint : footprints) {
        const QString name = safeName(footprint.name);
        if (name.isEmpty() || names.contains(name)) {
            m_diagnostics.append(QStringLiteral("Eagle: 封装名称清洗后冲突或为空：%1").arg(footprint.name));
            return false;
        }
        names.insert(name);
    }
    return writeLibrary(footprints, filePath, libraryDescription, m_diagnostics);
}

bool ExporterEagleFootprint::exportSymbolLibrary(const QList<IR::SymbolComponentIR>& symbols,
                                                 const QString& libName,
                                                 const QString& filePath) {
    m_diagnostics.clear();
    if (symbols.isEmpty()) {
        m_diagnostics.append(QStringLiteral("Eagle: 没有可导出的符号"));
        return false;
    }

    QSet<QString> symbolNames;
    for (const IR::SymbolComponentIR& symbol : symbols) {
        const QString baseName = safeName(symbol.name);
        if (baseName.isEmpty() || symbolNames.contains(baseName) || symbol.partCount < 1) {
            m_diagnostics.append(QStringLiteral("Eagle: 符号名称为空、清洗后冲突或部件数非法：%1").arg(symbol.name));
            return false;
        }
        for (int partIndex = 0; partIndex < symbol.partCount; ++partIndex) {
            const QString name = partIndex == 0 ? baseName : baseName + QStringLiteral("_P%1").arg(partIndex + 1);
            if (symbolNames.contains(name)) {
                m_diagnostics.append(QStringLiteral("Eagle: 符号部件名称冲突：%1").arg(name));
                return false;
            }
            symbolNames.insert(name);
        }
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        m_diagnostics.append(QStringLiteral("Eagle: 无法写入符号库：%1").arg(filePath));
        return false;
    }

    QXmlStreamWriter xml(&file);
    xml.setAutoFormatting(true);
    xml.writeStartDocument();
    xml.writeStartElement(QStringLiteral("eagle"));
    xml.writeAttribute(QStringLiteral("version"), QStringLiteral("9.6.2"));
    xml.writeStartElement(QStringLiteral("drawing"));
    xml.writeStartElement(QStringLiteral("settings"));
    xml.writeEmptyElement(QStringLiteral("setting"));
    xml.writeAttribute(QStringLiteral("alwaysvectorfont"), QStringLiteral("no"));
    xml.writeEndElement();
    writeLayerTable(xml);
    xml.writeStartElement(QStringLiteral("library"));
    xml.writeStartElement(QStringLiteral("description"));
    xml.writeCharacters(libName);
    xml.writeEndElement();
    xml.writeStartElement(QStringLiteral("symbols"));
    for (const IR::SymbolComponentIR& symbol : symbols) {
        const QString baseName = safeName(symbol.name);
        for (int partIndex = 0; partIndex < symbol.partCount; ++partIndex) {
            const QString name = partIndex == 0 ? baseName : baseName + QStringLiteral("_P%1").arg(partIndex + 1);
            if (!writeSymbol(xml, symbol, partIndex, name, m_diagnostics))
                return false;
        }
    }
    xml.writeEndElement();
    xml.writeEndElement();
    xml.writeEndElement();
    xml.writeEndElement();
    xml.writeEndDocument();
    return !xml.hasError();
}

/** 返回最近一次 Eagle 导出的诊断信息。 */
QStringList ExporterEagleFootprint::diagnostics() const {
    return m_diagnostics;
}

bool ExporterEagleFootprint::exportComponentLibrary(const QList<IR::ComponentIR>& components,
                                                    const QString& libName,
                                                    const QString& filePath,
                                                    bool exportModel3D,
                                                    const QString&) {
    m_diagnostics.clear();
    if (components.isEmpty()) {
        m_diagnostics.append(QStringLiteral("Eagle: 没有可导出的完整组件"));
        return false;
    }
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        m_diagnostics.append(QStringLiteral("Eagle: 无法写入完整库：%1").arg(filePath));
        return false;
    }

    QXmlStreamWriter xml(&file);
    xml.setAutoFormatting(true);
    xml.writeStartDocument();
    xml.writeStartElement(QStringLiteral("eagle"));
    xml.writeAttribute(QStringLiteral("version"), QStringLiteral("9.6.2"));
    xml.writeStartElement(QStringLiteral("drawing"));
    xml.writeStartElement(QStringLiteral("settings"));
    xml.writeEmptyElement(QStringLiteral("setting"));
    xml.writeAttribute(QStringLiteral("alwaysvectorfont"), QStringLiteral("no"));
    xml.writeEndElement();
    writeLayerTable(xml);
    xml.writeStartElement(QStringLiteral("library"));
    xml.writeStartElement(QStringLiteral("description"));
    xml.writeCharacters(libName);
    xml.writeEndElement();
    xml.writeStartElement(QStringLiteral("packages"));
    for (const IR::ComponentIR& component : components) {
        if (!writePackage(xml, component.footprint, m_diagnostics))
            return false;
    }
    xml.writeEndElement();
    if (!writeComponentLibrary(xml, components, m_diagnostics))
        return false;
    if (exportModel3D) {
        // Eagle 的 package3d 关联需要受管 URN 或经过 Eagle 生成的包描述，不能用外部文件路径伪造。
        for (const IR::ComponentIR& component : components) {
            if (component.hasModel3D())
                m_diagnostics.append(
                    QStringLiteral("Eagle: 组件 %1 的 3D 文件由独立 Model3D 阶段输出，未写入受管 package3d 关联")
                        .arg(component.name));
        }
    }
    xml.writeEndElement();
    xml.writeEndElement();
    xml.writeEndElement();
    xml.writeEndDocument();
    return !xml.hasError();
}

}  // namespace EasyKiConverter
