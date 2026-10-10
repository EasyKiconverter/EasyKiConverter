#include "ExporterAltiumSymbol.h"

#include "AltiumSchSymbolGeometryNormalizer.h"
#include "AltiumSymbolAnnotationConverter.h"
#include "AltiumSymbolCurveConverter.h"
#include "AltiumSymbolImplementationConverter.h"
#include "AltiumSymbolParameterConverter.h"
#include "AltiumSymbolPathConverter.h"
#include "AltiumSymbolPinConverter.h"
#include "AltiumSymbolPinTextConverter.h"
#include "AltiumSymbolPrimitiveConverter.h"
#include "utils/AltiumCoord.h"
#include "utils/AltiumSymbolConversionUtils.h"

#include <QDebug>
#include <QFile>

#include <algorithm>
#include <climits>
#include <cmath>

namespace EasyKiConverter {

namespace {

using AltiumSymbolConversionUtils::toAltiumColor;
using AltiumSymbolConversionUtils::toAltiumLineStyle;
using AltiumSymbolConversionUtils::toAltiumOrientation;
using AltiumSymbolConversionUtils::toAltiumOwnerPartId;

}  // namespace

/**
 * @brief 导出单个符号。
 * @param symbol 待导出的符号 IR。
 * @param filePath 输出文件路径。
 * @return 导出是否成功。
 */
bool ExporterAltiumSymbol::exportSymbol(const IR::SymbolComponentIR& symbol, const QString& filePath) {
    m_diagnostics.clear();
    QList<AltiumSchComponent> components;  // 待写入单个符号文件的组件列表。
    components.append(convertSymbol(symbol));
    bool ok = m_writer.write(components, filePath);  // SchLib 写入结果。
    m_diagnostics.append(m_writer.diagnostics());
    if (!ok) {
        qWarning() << "ExporterAltiumSymbol: Failed to write symbol to" << filePath;
    }
    return ok;
}

/**
 * @brief 导出符号库。
 * @param symbols 待导出的符号列表。
 * @param libName 输出库名称。
 * @param filePath 输出文件路径。
 * @param appendMode 是否请求追加模式。
 * @param updateMode 是否请求更新模式。
 * @param libraryDescription 库描述。
 * @return 导出是否成功。
 */
bool ExporterAltiumSymbol::exportSymbolLibrary(const QList<IR::SymbolComponentIR>& symbols,
                                               const QString& libName,
                                               const QString& filePath,
                                               bool appendMode,
                                               bool updateMode,
                                               const QString& libraryDescription) {
    m_diagnostics.clear();
    if ((appendMode || updateMode) && QFile::exists(filePath)) {
        const QString diagnostic =
            QStringLiteral("Altium SchLib 暂不支持在已有库上追加或更新，已拒绝覆盖: %1").arg(filePath);
        m_diagnostics.append(diagnostic);
        qWarning() << "ExporterAltiumSymbol:" << diagnostic;
        return false;
    }

    QList<AltiumSchComponent> components;  // 待写入符号库的组件列表。
    for (const IR::SymbolComponentIR& symbol : symbols) {
        components.append(convertSymbol(symbol));
    }
    bool ok = m_writer.write(components, filePath, libName);  // SchLib 写入结果。
    m_diagnostics.append(m_writer.diagnostics());
    if (!ok) {
        qWarning() << "ExporterAltiumSymbol: Failed to write symbol library to" << filePath;
    }
    return ok;
}

/**
 * @brief 将 SymbolComponentIR 转换为 AltiumSchComponent。
 * @param data 符号组件 IR。
 * @return 转换后的 Altium 符号组件。
 */
AltiumSchComponent ExporterAltiumSymbol::convertSymbol(const IR::SymbolComponentIR& data) {
    AltiumSchComponent component;  // 转换后的 Altium 符号组件。
    component.name = data.name;
    component.description = data.description;
    component.designatorPrefix = data.designatorPrefix;
    component.partCount = data.partCount;
    component.sourceMetadata = data.sourceMetadata;
    component.aliases = data.aliases;
    for (const IR::SymbolGraphicOrderIR& order : data.graphicOrder)
        component.graphicOrder.append({order.type, order.index, order.partIndex});
    const auto sourceIndexForPart = [](const auto& values, int index, int partIndex) {
        int localIndex = 0;  // 当前部件内已遍历图元数量。
        for (int i = 0; i < index; ++i) {
            if (values.at(i).partIndex == partIndex)
                ++localIndex;
        }
        return localIndex;
    };
    const auto isFinitePoint = [](const QPointF& point) {
        return std::isfinite(point.x()) && std::isfinite(point.y());
    };
    const auto hasFinitePoints = [&](const QList<QPointF>& points, int minimum) {
        if (points.size() < minimum)
            return false;
        return std::all_of(points.cbegin(), points.cend(), isFinitePoint);
    };
    const auto isValidStrokeWidth = [](double width) { return std::isfinite(width) && width >= 0.0; };
    const auto isValidBounds = [](double x0, double y0, double x1, double y1) {
        return std::isfinite(x0) && std::isfinite(y0) && std::isfinite(x1) && std::isfinite(y1) && x0 != x1 && y0 != y1;
    };
    const auto normalizeTextAnchor = [this, &data](const QString& anchor, const QString& context) {
        const QString normalized = anchor.trimmed().toLower();  // 规范化后的文本锚点。
        if (normalized.isEmpty())
            return QStringLiteral("middle");
        if (!QStringList{QStringLiteral("start"), QStringLiteral("middle"), QStringLiteral("end")}.contains(
                normalized)) {
            m_diagnostics.append(QStringLiteral("符号 %1 %2 的对齐锚点无效: %3，已回退为 middle")
                                     .arg(data.name)
                                     .arg(context)
                                     .arg(anchor));
            return QStringLiteral("middle");
        }
        return normalized;
    };

    for (const IR::SymbolParameterIR& parameter : data.parameters) {
        if (!isFinitePoint(parameter.position) || !std::isfinite(parameter.rotation) ||
            parameter.name.trimmed().isEmpty()) {
            m_diagnostics.append(QStringLiteral("符号 %1 参数 %2 的位置、旋转角度或名称无效，已跳过")
                                     .arg(data.name)
                                     .arg(parameter.name));
            continue;
        }
        if (!std::isfinite(parameter.fontSizeMm) || parameter.fontSizeMm < 0.0) {
            m_diagnostics.append(
                QStringLiteral("符号 %1 参数 %2 的字体大小无效，已跳过").arg(data.name).arg(parameter.name));
            continue;
        }
        component.parameters.append(AltiumSymbolParameterConverter::convert(parameter));
    }

    // 转换引脚
    for (const IR::SymbolPinIR& pin : data.pins) {
        if (!isFinitePoint(pin.position) || !std::isfinite(pin.length)) {
            m_diagnostics.append(
                QStringLiteral("符号 %1 引脚 %2 的位置或长度无效，已跳过").arg(data.name).arg(pin.designator));
            continue;
        }
        component.pins.append(convertPin(pin));
        QList<AltiumSchText> pinTexts =
            AltiumSymbolPinTextConverter::convert(pin, data.name, &m_diagnostics);  // 当前引脚生成的文本。
        const int sourcePinIndex = component.pins.size() - 1;  // 当前引脚在组件列表中的索引。
        for (AltiumSchText& pinText : pinTexts) {
            if (pinText.isPinLabel)
                pinText.sourcePinIndex = sourcePinIndex;
        }
        component.texts.append(pinTexts);
    }

    // 转换图形元素
    for (int i = 0; i < data.rectangles.size(); ++i) {
        const IR::SymbolRectangleIR& r = data.rectangles.at(i);  // 当前源矩形图元。
        if (!isValidBounds(r.x0, r.y0, r.x1, r.y1)) {
            m_diagnostics.append(QStringLiteral("符号 %1 矩形图元 %2 的边界无效，已跳过").arg(data.name).arg(i));
            continue;
        }
        if (!std::isfinite(r.cornerRadiusX) || !std::isfinite(r.cornerRadiusY) || r.cornerRadiusX < 0.0 ||
            r.cornerRadiusY < 0.0) {
            m_diagnostics.append(
                QStringLiteral("符号 %1 矩形图元 %2 的圆角半径无效，已钳制为非负值").arg(data.name).arg(i));
        }
        if (!isValidStrokeWidth(r.strokeWidth)) {
            m_diagnostics.append(QStringLiteral("符号 %1 矩形图元 %2 的线宽无效，已跳过").arg(data.name).arg(i));
            continue;
        }
        if (r.cornerRadiusX > 0.0 || r.cornerRadiusY > 0.0) {
            AltiumSchRoundRectangle rectangle = convertRoundRectangle(r);  // 转换后的圆角矩形记录。
            rectangle.sourceGraphicIndex = sourceIndexForPart(data.rectangles, i, r.partIndex);
            rectangle.sourcePartIndex = r.partIndex;
            component.roundRectangles.append(rectangle);
        } else {
            AltiumSchRectangle rectangle = convertRectangle(r);  // 转换后的矩形记录。
            rectangle.sourceGraphicIndex = sourceIndexForPart(data.rectangles, i, r.partIndex);
            rectangle.sourcePartIndex = r.partIndex;
            component.rectangles.append(rectangle);
        }
    }
    for (int i = 0; i < data.circles.size(); ++i) {
        const IR::SymbolCircleIR& circle = data.circles.at(i);  // 当前源圆图元。
        if (!isFinitePoint(circle.center) || !isValidStrokeWidth(circle.strokeWidth)) {
            m_diagnostics.append(QStringLiteral("符号 %1 圆图元 %2 的中心或线宽无效，已跳过").arg(data.name).arg(i));
            continue;
        }
        if (!std::isfinite(circle.radius) || circle.radius < 0.0) {
            m_diagnostics.append(QStringLiteral("符号 %1 圆图元 %2 的半径无效，已跳过").arg(data.name).arg(i));
            continue;
        }
        AltiumSchEllipse ellipse = convertCircle(circle);  // 转换后的圆形记录。
        if (ellipse.radiusX <= 0 || ellipse.radiusY <= 0) {
            m_diagnostics.append(QStringLiteral("符号 %1 圆图元 %2 半径量化后无效，已跳过").arg(data.name).arg(i));
            continue;
        }
        ellipse.sourceGraphicType = QStringLiteral("C");
        ellipse.sourceGraphicIndex = sourceIndexForPart(data.circles, i, data.circles.at(i).partIndex);
        ellipse.sourcePartIndex = data.circles.at(i).partIndex;
        component.ellipses.append(ellipse);
    }
    for (int i = 0; i < data.arcs.size(); ++i) {
        const IR::SymbolArcIR& sourceArc = data.arcs.at(i);  // 当前源圆弧图元。
        if (!isFinitePoint(sourceArc.startPoint) || !isFinitePoint(sourceArc.midPoint) ||
            !isFinitePoint(sourceArc.endPoint) || !std::isfinite(sourceArc.strokeWidth) ||
            sourceArc.strokeWidth < 0.0) {
            m_diagnostics.append(QStringLiteral("符号 %1 圆弧图元 %2 的点列或线宽无效，已跳过").arg(data.name).arg(i));
            continue;
        }
        const double determinant =
            2.0 *
            (sourceArc.startPoint.x() * (sourceArc.midPoint.y() - sourceArc.endPoint.y()) +
             sourceArc.midPoint.x() * (sourceArc.endPoint.y() - sourceArc.startPoint.y()) +
             sourceArc.endPoint.x() * (sourceArc.startPoint.y() - sourceArc.midPoint.y()));  // 三点圆心方程行列式。
        if (!std::isfinite(determinant) || std::abs(determinant) <= 1e-12) {
            m_diagnostics.append(
                QStringLiteral("符号 %1 圆弧图元 %2 三点退化，已使用安全回退圆心").arg(data.name).arg(i));
        }
        AltiumSchArc arc = convertArc(sourceArc);  // 转换后的圆弧记录。
        if (arc.radius <= 0) {
            m_diagnostics.append(QStringLiteral("符号 %1 圆弧图元 %2 半径量化后无效，已跳过").arg(data.name).arg(i));
            continue;
        }
        arc.sourceGraphicType = QStringLiteral("A");
        arc.sourceGraphicIndex = sourceIndexForPart(data.arcs, i, data.arcs.at(i).partIndex);
        arc.sourcePartIndex = data.arcs.at(i).partIndex;
        component.arcs.append(arc);
    }
    for (int i = 0; i < data.polygons.size(); ++i) {
        const IR::SymbolPolygonIR& sourcePolygon = data.polygons.at(i);  // 当前源多边形图元。
        if (!hasFinitePoints(sourcePolygon.points, 3)) {
            m_diagnostics.append(QStringLiteral("符号 %1 多边形图元 %2 的点列无效，已跳过").arg(data.name).arg(i));
            continue;
        }
        if (!isValidStrokeWidth(sourcePolygon.strokeWidth)) {
            m_diagnostics.append(QStringLiteral("符号 %1 多边形图元 %2 的线宽无效，已跳过").arg(data.name).arg(i));
            continue;
        }
        AltiumSchPolygon polygon = convertPolygon(sourcePolygon);  // 转换后的多边形记录。
        polygon.sourceGraphicIndex = sourceIndexForPart(data.polygons, i, sourcePolygon.partIndex);
        polygon.sourcePartIndex = sourcePolygon.partIndex;
        component.polygons.append(polygon);
    }
    for (int i = 0; i < data.polylines.size(); ++i) {
        const IR::SymbolPolylineIR& sourcePolyline = data.polylines.at(i);  // 当前源折线图元。
        if (!hasFinitePoints(sourcePolyline.points, 2)) {
            m_diagnostics.append(QStringLiteral("符号 %1 折线图元 %2 的点列无效，已跳过").arg(data.name).arg(i));
            continue;
        }
        if (!isValidStrokeWidth(sourcePolyline.strokeWidth)) {
            m_diagnostics.append(QStringLiteral("符号 %1 折线图元 %2 的线宽无效，已跳过").arg(data.name).arg(i));
            continue;
        }
        AltiumSchPolyline polyline = convertPolyline(sourcePolyline);  // 转换后的折线记录。
        polyline.sourceGraphicIndex = sourceIndexForPart(data.polylines, i, sourcePolyline.partIndex);
        polyline.sourcePartIndex = sourcePolyline.partIndex;
        component.polylines.append(polyline);
    }
    AltiumSymbolPathConverter::append(data, component, m_diagnostics);
    for (int bezierIndex = 0; bezierIndex < data.beziers.size(); ++bezierIndex) {
        const IR::SymbolBezierIR& b = data.beziers.at(bezierIndex);  // 当前 Bézier 图元。
        if (b.controlPoints.size() == 4 && hasFinitePoints(b.controlPoints, 4) && isValidStrokeWidth(b.strokeWidth)) {
            component.beziers.append(convertBezier(b));
        } else if (b.controlPoints.size() != 4 || !hasFinitePoints(b.controlPoints, 4)) {
            m_diagnostics.append(QStringLiteral("符号 %1 Bézier 图元 %2 的控制点参数无效（数量为 %3），已跳过")
                                     .arg(data.name)
                                     .arg(bezierIndex)
                                     .arg(b.controlPoints.size()));
        } else {
            m_diagnostics.append(
                QStringLiteral("符号 %1 Bézier 图元 %2 的线宽无效，已跳过").arg(data.name).arg(bezierIndex));
        }
    }
    for (int i = 0; i < data.ieeeSymbols.size(); ++i) {
        const IR::SymbolIeeeIR& ieee = data.ieeeSymbols.at(i);  // 当前 IEEE 图元。
        if (!isFinitePoint(ieee.position)) {
            m_diagnostics.append(QStringLiteral("符号 %1 IEEE 图形 %2 的位置无效，已跳过").arg(data.name).arg(i));
            continue;
        }
        component.ieeeSymbols.append(convertIeee(ieee));
    }
    for (int i = 0; i < data.texts.size(); ++i) {
        const IR::SymbolTextIR& sourceText = data.texts.at(i);  // 当前源文本图元。
        if (sourceText.text.trimmed().isEmpty() || !isFinitePoint(sourceText.position) ||
            !std::isfinite(sourceText.rotation) ||
            (sourceText.visible && (!std::isfinite(sourceText.fontSizeMm) || sourceText.fontSizeMm < 0.0))) {
            m_diagnostics.append(
                QStringLiteral("符号 %1 文本图元 %2 的内容或几何参数无效，已跳过").arg(data.name).arg(i));
            continue;
        }
        AltiumSchText text = convertText(sourceText);  // 转换后的 Altium 文本记录。
        text.anchor = normalizeTextAnchor(sourceText.anchor, QStringLiteral("文本图元 %1").arg(i));
        text.sourceGraphicIndex = sourceIndexForPart(data.texts, i, sourceText.partIndex);
        text.sourcePartIndex = sourceText.partIndex;
        component.texts.append(text);
    }
    for (int i = 0; i < data.textFrames.size(); ++i) {
        const IR::SymbolTextFrameIR& frame = data.textFrames.at(i);  // 当前源文本框图元。
        if (!isValidBounds(frame.x0, frame.y0, frame.x1, frame.y1) || !std::isfinite(frame.textMargin) ||
            frame.textMargin < 0.0 || !isValidStrokeWidth(frame.strokeWidth)) {
            m_diagnostics.append(QStringLiteral("符号 %1 文本框图元 %2 的几何参数无效，已跳过").arg(data.name).arg(i));
            continue;
        }
        component.textFrames.append(convertTextFrame(frame));
    }
    for (int i = 0; i < data.images.size(); ++i) {
        const IR::SymbolImageIR& image = data.images.at(i);  // 当前源图片图元。
        // 图片必须具有有限且非退化的矩形边界。
        const bool validBounds = std::isfinite(image.x0) && std::isfinite(image.y0) && std::isfinite(image.x1) &&
                                 std::isfinite(image.y1) && image.x0 != image.x1 && image.y0 != image.y1;
        const QString imageSource = image.fileName.trimmed();  // 图片文件名或来源 URL。
        const bool hasSource = !imageSource.isEmpty() || !image.data.isEmpty();  // 是否存在图片来源。
        const bool hasUnparsedDataUrl =
            imageSource.startsWith(QStringLiteral("data:"), Qt::CaseInsensitive);  // 是否仍为未解析的 data URL。
        if (!validBounds || !hasSource || hasUnparsedDataUrl || !std::isfinite(image.rotation) ||
            !std::isfinite(image.strokeWidth) || image.strokeWidth < 0.0) {
            if (hasUnparsedDataUrl) {
                m_diagnostics.append(
                    QStringLiteral("符号 %1 图片图元 %2 包含未解析的 data URL，已跳过").arg(data.name).arg(i));
            } else {
                m_diagnostics.append(
                    QStringLiteral("符号 %1 图片图元 %2 的边界、线宽或资源无效，已跳过").arg(data.name).arg(i));
            }
            continue;
        }
        AltiumSchImage altiumImage = convertImage(image);  // 转换后的 Altium 图片记录。
        altiumImage.sourceGraphicIndex = sourceIndexForPart(data.images, i, image.partIndex);
        altiumImage.sourcePartIndex = image.partIndex;
        component.images.append(altiumImage);
    }
    for (int i = 0; i < data.ellipses.size(); ++i) {
        const IR::SymbolEllipseIR& sourceEllipse = data.ellipses.at(i);  // 当前源椭圆图元。
        if (!isFinitePoint(sourceEllipse.center) || !std::isfinite(sourceEllipse.radiusX) ||
            !std::isfinite(sourceEllipse.radiusY) || sourceEllipse.radiusX <= 0.0 || sourceEllipse.radiusY <= 0.0 ||
            !isValidStrokeWidth(sourceEllipse.strokeWidth)) {
            m_diagnostics.append(QStringLiteral("符号 %1 椭圆图元 %2 的几何参数无效，已跳过").arg(data.name).arg(i));
            continue;
        }
        AltiumSchEllipse ellipse = convertEllipse(sourceEllipse);  // 转换后的 Altium 椭圆记录。
        if (ellipse.radiusX <= 0 || ellipse.radiusY <= 0) {
            m_diagnostics.append(QStringLiteral("符号 %1 椭圆图元 %2 半径量化后无效，已跳过").arg(data.name).arg(i));
            continue;
        }
        ellipse.sourceGraphicType = QStringLiteral("E");
        ellipse.sourceGraphicIndex = sourceIndexForPart(data.ellipses, i, sourceEllipse.partIndex);
        ellipse.sourcePartIndex = sourceEllipse.partIndex;
        component.ellipses.append(ellipse);
    }
    for (int i = 0; i < data.pies.size(); ++i) {
        const IR::SymbolPieIR& sourcePie = data.pies.at(i);  // 当前源扇形图元。
        if (!isFinitePoint(sourcePie.center) || !std::isfinite(sourcePie.radius) || sourcePie.radius <= 0.0 ||
            !std::isfinite(sourcePie.startAngle) || !std::isfinite(sourcePie.endAngle) ||
            !isValidStrokeWidth(sourcePie.strokeWidth)) {
            m_diagnostics.append(QStringLiteral("符号 %1 扇形图元 %2 的几何参数无效，已跳过").arg(data.name).arg(i));
            continue;
        }
        AltiumSchPie pie = convertPie(sourcePie);  // 转换后的 Altium 扇形记录。
        if (pie.radius <= 0) {
            m_diagnostics.append(QStringLiteral("符号 %1 扇形图元 %2 半径量化后无效，已跳过").arg(data.name).arg(i));
            continue;
        }
        component.pies.append(pie);
    }
    for (int i = 0; i < data.ellipticalArcs.size(); ++i) {
        const IR::SymbolEllipticalArcIR& sourceArc = data.ellipticalArcs.at(i);  // 当前源椭圆弧图元。
        if (!isFinitePoint(sourceArc.center) || !std::isfinite(sourceArc.radiusX) ||
            !std::isfinite(sourceArc.radiusY) || sourceArc.radiusX <= 0.0 || sourceArc.radiusY <= 0.0 ||
            !std::isfinite(sourceArc.startAngle) || !std::isfinite(sourceArc.endAngle) ||
            !isValidStrokeWidth(sourceArc.strokeWidth)) {
            m_diagnostics.append(QStringLiteral("符号 %1 椭圆弧图元 %2 的几何参数无效，已跳过").arg(data.name).arg(i));
            continue;
        }
        AltiumSchEllipticalArc ellipticalArc = convertEllipticalArc(sourceArc);  // 转换后的椭圆弧记录。
        if (ellipticalArc.radiusX <= 0 || ellipticalArc.radiusY <= 0) {
            m_diagnostics.append(QStringLiteral("符号 %1 椭圆弧图元 %2 半径量化后无效，已跳过").arg(data.name).arg(i));
            continue;
        }
        component.ellipticalArcs.append(ellipticalArc);
    }

    component.implementations = AltiumSymbolImplementationConverter::convert(data);

    // 坐标归一化：将符号中心移到原点
    centerComponent(component);

    return component;
}

/**
 * @brief 将 SymbolPinIR 转换为 AltiumSchPin。
 * @param pin 符号引脚 IR。
 * @return 转换后的 Altium 引脚。
 */
AltiumSchPin ExporterAltiumSymbol::convertPin(const IR::SymbolPinIR& pin) {
    return AltiumSymbolPinConverter::convert(pin);
}

/**
 * @brief 将 SymbolRectangleIR 转换为 AltiumSchRectangle。
 * @param rect 矩形图元 IR。
 * @return 转换后的 Altium 矩形。
 */
AltiumSchRectangle ExporterAltiumSymbol::convertRectangle(const IR::SymbolRectangleIR& rect) {
    return AltiumSymbolPrimitiveConverter::convertRectangle(rect);
}

/**
 * @brief 将 SymbolRectangleIR 转换为 AltiumSchRoundRectangle。
 * @param rect 圆角矩形图元 IR。
 * @return 转换后的 Altium 圆角矩形。
 */
AltiumSchRoundRectangle ExporterAltiumSymbol::convertRoundRectangle(const IR::SymbolRectangleIR& rect) {
    return AltiumSymbolPrimitiveConverter::convertRoundRectangle(rect);
}

/**
 * @brief 将 SymbolCircleIR 转换为 AltiumSchEllipse。
 * @param circle 圆形图元 IR。
 * @return 转换后的 Altium 椭圆。
 */
AltiumSchEllipse ExporterAltiumSymbol::convertCircle(const IR::SymbolCircleIR& circle) {
    return AltiumSymbolPrimitiveConverter::convertCircle(circle);
}

/**
 * @brief 将 SymbolArcIR 转换为 AltiumSchArc。
 * @param arc 圆弧图元 IR。
 * @return 转换后的 Altium 圆弧。
 */
AltiumSchArc ExporterAltiumSymbol::convertArc(const IR::SymbolArcIR& arc) {
    return AltiumSymbolCurveConverter::convertArc(arc);
}

/**
 * @brief 将 SymbolPolygonIR 转换为 AltiumSchPolygon。
 * @param polygon 多边形图元 IR。
 * @return 转换后的 Altium 多边形。
 */
AltiumSchPolygon ExporterAltiumSymbol::convertPolygon(const IR::SymbolPolygonIR& polygon) {
    return AltiumSymbolCurveConverter::convertPolygon(polygon);
}

/**
 * @brief 将 SymbolPolylineIR 转换为 AltiumSchPolyline。
 * @param polyline 折线图元 IR。
 * @return 转换后的 Altium 折线。
 */
AltiumSchPolyline ExporterAltiumSymbol::convertPolyline(const IR::SymbolPolylineIR& polyline) {
    return AltiumSymbolCurveConverter::convertPolyline(polyline);
}

/**
 * @brief 将 SymbolPathIR 转换为 AltiumSchPath。
 * @param path 路径图元 IR。
 * @return 转换后的 Altium 路径。
 */
AltiumSchPath ExporterAltiumSymbol::convertPath(const IR::SymbolPathIR& path) {
    return AltiumSymbolCurveConverter::convertPath(path);
}

/**
 * @brief 将 SymbolBezierIR 转换为 AltiumSchBezier。
 * @param bezier Bézier 图元 IR。
 * @return 转换后的 Altium Bézier 曲线。
 */
AltiumSchBezier ExporterAltiumSymbol::convertBezier(const IR::SymbolBezierIR& bezier) {
    return AltiumSymbolCurveConverter::convertBezier(bezier);
}

/**
 * @brief 将 SymbolIeeeIR 转换为 AltiumSchIeee。
 * @param ieee IEEE 图元 IR。
 * @return 转换后的 Altium IEEE 图元。
 */
AltiumSchIeee ExporterAltiumSymbol::convertIeee(const IR::SymbolIeeeIR& ieee) {
    return AltiumSymbolPrimitiveConverter::convertIeee(ieee);
}

/**
 * @brief 将 SymbolTextIR 转换为 AltiumSchText。
 * @param text 文本图元 IR。
 * @return 转换后的 Altium 文本。
 */
AltiumSchText ExporterAltiumSymbol::convertText(const IR::SymbolTextIR& text) {
    return AltiumSymbolAnnotationConverter::convertText(text);
}

/**
 * @brief 将 SymbolTextFrameIR 转换为 AltiumSchTextFrame。
 * @param frame 文本框图元 IR。
 * @return 转换后的 Altium 文本框。
 */
AltiumSchTextFrame ExporterAltiumSymbol::convertTextFrame(const IR::SymbolTextFrameIR& frame) {
    return AltiumSymbolAnnotationConverter::convertTextFrame(frame);
}

/**
 * @brief 将 SymbolImageIR 转换为 AltiumSchImage。
 * @param image 图片图元 IR。
 * @return 转换后的 Altium 图片。
 */
AltiumSchImage ExporterAltiumSymbol::convertImage(const IR::SymbolImageIR& image) {
    return AltiumSymbolAnnotationConverter::convertImage(image);
}

/**
 * @brief 将 SymbolEllipseIR 转换为 AltiumSchEllipse。
 * @param ellipse 椭圆图元 IR。
 * @return 转换后的 Altium 椭圆。
 */
AltiumSchEllipse ExporterAltiumSymbol::convertEllipse(const IR::SymbolEllipseIR& ellipse) {
    return AltiumSymbolCurveConverter::convertEllipse(ellipse);
}

/**
 * @brief 将 SymbolPieIR 转换为 AltiumSchPie。
 * @param pie 扇形图元 IR。
 * @return 转换后的 Altium 扇形。
 */
AltiumSchPie ExporterAltiumSymbol::convertPie(const IR::SymbolPieIR& pie) {
    return AltiumSymbolCurveConverter::convertPie(pie);
}

/**
 * @brief 将 SymbolEllipticalArcIR 转换为 AltiumSchEllipticalArc。
 * @param arc 椭圆弧图元 IR。
 * @return 转换后的 Altium 椭圆弧。
 */
AltiumSchEllipticalArc ExporterAltiumSymbol::convertEllipticalArc(const IR::SymbolEllipticalArcIR& arc) {
    return AltiumSymbolCurveConverter::convertEllipticalArc(arc);
}

/**
 * @brief 将符号图元委托给独立的几何归一化器。
 * @param component 待归一化的 Altium 符号组件。
 */
void ExporterAltiumSymbol::centerComponent(AltiumSchComponent& component) {
    AltiumSchSymbolGeometryNormalizer::normalize(component);
}

}  // namespace EasyKiConverter
