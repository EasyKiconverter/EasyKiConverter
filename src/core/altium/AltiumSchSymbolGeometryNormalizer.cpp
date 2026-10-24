#include "AltiumSchSymbolGeometryNormalizer.h"

#include "utils/AltiumSymbolConversionUtils.h"

#include <QPointF>

#include <algorithm>
#include <climits>
#include <cmath>
#include <numeric>

namespace EasyKiConverter {

namespace {

using AltiumSymbolConversionUtils::quantizeSchematicOffset;

/**
 * @brief 将坐标量化到 Altium SchLib 的 10 mil 吸附网格。
 * @param value 待量化的原始坐标。
 * @return 吸附到 10 mil 网格后的坐标。
 */
int quantizeToSnapGrid(int value) {
    constexpr int kSnapGrid = 100000;  // Altium SchLib 的 10 mil 网格单位。
    if (value >= 0)
        return ((value + kSnapGrid / 2) / kSnapGrid) * kSnapGrid;
    return ((value - kSnapGrid / 2) / kSnapGrid) * kSnapGrid;
}

/**
 * @brief 计算引脚的外部连接端。
 * @details Altium 的引脚位置是主体端，连接端沿引脚方向偏移引脚长度。
 * @param pin 待计算的 Altium 引脚。
 * @return 引脚外部连接端坐标。
 */
QPointF computePinConnectionPoint(const AltiumSchPin& pin) {
    // 根据引脚朝向从主体端计算外部连接端。
    switch (pin.orientation) {
        case AltiumModels::PinOrientation::Right:
            return QPointF(pin.locationX + pin.length, pin.locationY);
        case AltiumModels::PinOrientation::Left:
            return QPointF(pin.locationX - pin.length, pin.locationY);
        case AltiumModels::PinOrientation::Up:
            return QPointF(pin.locationX, pin.locationY + pin.length);
        case AltiumModels::PinOrientation::Down:
            return QPointF(pin.locationX, pin.locationY - pin.length);
    }
    return QPointF(pin.locationX, pin.locationY);
}

/**
 * @brief 将单个引脚连接端吸附到网格并反推主体端位置。
 * @param pin 待调整的 Altium 引脚。
 */
void quantizePinConnectionPoint(AltiumSchPin& pin) {
    const QPointF connection = computePinConnectionPoint(pin);  // 当前引脚的未量化连接端。
    const int quantizedConnectionX = quantizeToSnapGrid(static_cast<int>(connection.x()));  // 量化后的连接端 X 坐标。
    const int quantizedConnectionY = quantizeToSnapGrid(static_cast<int>(connection.y()));  // 量化后的连接端 Y 坐标。
    // 反向计算主体端，确保吸附只改变连接端位置而不改变引脚长度。
    switch (pin.orientation) {
        case AltiumModels::PinOrientation::Right:
            pin.locationX = quantizedConnectionX - pin.length;
            pin.locationY = quantizedConnectionY;
            break;
        case AltiumModels::PinOrientation::Left:
            pin.locationX = quantizedConnectionX + pin.length;
            pin.locationY = quantizedConnectionY;
            break;
        case AltiumModels::PinOrientation::Up:
            pin.locationX = quantizedConnectionX;
            pin.locationY = quantizedConnectionY - pin.length;
            break;
        case AltiumModels::PinOrientation::Down:
            pin.locationX = quantizedConnectionX;
            pin.locationY = quantizedConnectionY + pin.length;
            break;
    }
}

/**
 * @brief 按引脚侧分组量化连接端。
 * @details 同侧连接端重合时沿切线方向逐格错开，避免多个引脚占用同一连接点。
 * @param pins 待量化的符号引脚列表。
 */
void quantizePinConnectionGroups(QList<AltiumSchPin>& pins) {
    constexpr int kSnapGrid = 100000;  // 引脚连接端使用的 10 mil 基础网格。
    constexpr int kPinSpacing = 1000000;  // 同侧引脚之间使用的 100 mil 可读间距。
    for (int side = 0; side < 4; ++side) {
        QList<int> indices;  // 当前方向对应的引脚索引。
        for (int i = 0; i < pins.size(); ++i) {
            if (static_cast<int>(pins[i].orientation) == side)
                indices.append(i);
        }
        std::sort(indices.begin(), indices.end(), [&](int lhs, int rhs) {
            const QPointF left = computePinConnectionPoint(pins[lhs]);  // 左侧排序元素的连接端。
            const QPointF right = computePinConnectionPoint(pins[rhs]);  // 右侧排序元素的连接端。
            // 判断排序方向是否沿水平引脚的切线方向。
            const bool horizontal = side == static_cast<int>(AltiumModels::PinOrientation::Right) ||
                                    side == static_cast<int>(AltiumModels::PinOrientation::Left);
            return horizontal ? (left.y() < right.y()) : (left.x() < right.x());
        });

        QList<int> tangents;  // 当前侧引脚连接端的量化切线坐标。
        for (int index : indices) {
            quantizePinConnectionPoint(pins[index]);
            const QPointF connection = computePinConnectionPoint(pins[index]);  // 当前引脚的连接端。
            // 判断当前引脚是否沿水平侧排列。
            const bool horizontal = side == static_cast<int>(AltiumModels::PinOrientation::Right) ||
                                    side == static_cast<int>(AltiumModels::PinOrientation::Left);
            tangents.append(horizontal ? static_cast<int>(connection.y()) : static_cast<int>(connection.x()));
        }
        if (tangents.isEmpty())
            continue;

        // 以原始连接端的中心为基准扩展间距，避免只向一侧推移导致符号失去平衡。
        const int originalCenter =
            std::accumulate(tangents.cbegin(), tangents.cend(), 0LL) / static_cast<long long>(tangents.size());
        const int firstTangent = quantizeToSnapGrid(
            static_cast<int>(originalCenter - static_cast<long long>(kPinSpacing) * (tangents.size() - 1) / 2));
        for (int order = 0; order < indices.size(); ++order) {
            const int tangent = firstTangent + order * kPinSpacing;  // 当前引脚的规范化切线坐标。
            AltiumSchPin& pin = pins[indices.at(order)];  // 当前待布局的引脚。
            switch (pin.orientation) {
                case AltiumModels::PinOrientation::Right:
                case AltiumModels::PinOrientation::Left:
                    pin.locationY = tangent;
                    break;
                case AltiumModels::PinOrientation::Up:
                case AltiumModels::PinOrientation::Down:
                    pin.locationX = tangent;
                    break;
            }
        }
    }
}

/**
 * @brief 估算引脚名称在 SchLib 中占用的水平宽度。
 * @details 该估算用于确定主体最小宽度，避免长名称侵入符号中心；实际字体仍由 SchLib 字体表渲染。
 * @param text 待估算的引脚名称文本。
 * @return 估算后的原始坐标宽度。
 */
int estimatePinLabelWidth(const AltiumSchText& text) {
    constexpr double kDefaultFontSizeMm = 1.27;  // 未提供有效字号时使用的 50 mil 字号。
    constexpr double kAverageGlyphWidth = 0.58;  // 拉丁字符平均宽度占字号高度的比例。
    const double fontSizeMm = std::isfinite(text.fontSizeMm) && text.fontSizeMm > 0.0
                                  ? qMin(text.fontSizeMm, kDefaultFontSizeMm)
                                  : kDefaultFontSizeMm;  // 用于布局的规范化字号。
    const double widthMm = fontSizeMm * kAverageGlyphWidth * qMax(1, text.text.size());  // 估算的文本宽度。
    return static_cast<int>(std::ceil(widthMm / 0.0254 * 10000.0));
}

/**
 * @brief 根据引脚名称扩展矩形主体的水平边界。
 * @param component 待调整的 Altium 符号组件。
 * @param bodyMinX 主体原始最小 X 坐标。
 * @param bodyMaxX 主体原始最大 X 坐标。
 * @param requiredHalfWidth 引脚名称所需的主体半宽。
 * @return 扩展后的主体边界。
 */
QPair<int, int> expandBodyForPinLabels(AltiumSchComponent& component,
                                       int bodyMinX,
                                       int bodyMaxX,
                                       int requiredHalfWidth) {
    const int centerX = (bodyMinX + bodyMaxX) / 2;  // 主体水平中心。
    const int currentHalfWidth = (bodyMaxX - bodyMinX) / 2;  // 当前主体半宽。
    if (requiredHalfWidth <= currentHalfWidth)
        return {bodyMinX, bodyMaxX};

    const int expandedMinX = centerX - requiredHalfWidth;  // 扩展后的主体最小 X 坐标。
    const int expandedMaxX = centerX + requiredHalfWidth;  // 扩展后的主体最大 X 坐标。
    for (AltiumSchRectangle& rectangle : component.rectangles) {
        const int rectangleMinX = qMin(rectangle.locationX, rectangle.cornerX);  // 当前矩形最小 X 坐标。
        const int rectangleMaxX = qMax(rectangle.locationX, rectangle.cornerX);  // 当前矩形最大 X 坐标。
        if (rectangleMinX != bodyMinX && rectangleMaxX != bodyMaxX)
            continue;
        if (rectangle.locationX <= rectangle.cornerX) {
            if (rectangleMinX == bodyMinX)
                rectangle.locationX = expandedMinX;
            if (rectangleMaxX == bodyMaxX)
                rectangle.cornerX = expandedMaxX;
        } else {
            if (rectangleMinX == bodyMinX)
                rectangle.cornerX = expandedMinX;
            if (rectangleMaxX == bodyMaxX)
                rectangle.locationX = expandedMaxX;
        }
    }
    return {expandedMinX, expandedMaxX};
}

}  // namespace

/**
 * @brief 计算包围盒并平移所有符号图元。
 * @details 使用图形包围盒中心作为统一原点，同时保留引脚和文本的相对位置。
 * @param component 待归一化的 Altium 符号组件。
 */
void AltiumSchSymbolGeometryNormalizer::normalize(AltiumSchComponent& component) {
    int minX = INT_MAX, minY = INT_MAX, maxX = INT_MIN, maxY = INT_MIN;  // 所有图元的原始包围盒边界。
    const auto include = [&](int x, int y) {
        minX = qMin(minX, x);
        minY = qMin(minY, y);
        maxX = qMax(maxX, x);
        maxY = qMax(maxY, y);
    };
    const auto includeSchematicPoint = [&include](const QPointF& point) {
        include(static_cast<int>(std::lround(point.x() * 1000.0)), static_cast<int>(std::lround(point.y() * 1000.0)));
    };
    for (const auto& pin : component.pins) {
        include(pin.locationX, pin.locationY);
        const QPointF connection = computePinConnectionPoint(pin);  // 当前引脚外部连接端。
        include(static_cast<int>(connection.x()), static_cast<int>(connection.y()));
    }
    for (const auto& rect : component.rectangles) {
        include(rect.locationX, rect.locationY);
        include(rect.cornerX, rect.cornerY);
    }
    for (const auto& rect : component.roundRectangles) {
        include(rect.locationX, rect.locationY);
        include(rect.cornerX, rect.cornerY);
    }
    for (const auto& line : component.lines) {
        include(line.locationX, line.locationY);
        include(line.cornerX, line.cornerY);
    }
    for (const auto& arc : component.arcs) {
        include(arc.centerX - arc.radius, arc.centerY - arc.radius);
        include(arc.centerX + arc.radius, arc.centerY + arc.radius);
    }
    for (const auto& ellipse : component.ellipses) {
        include(ellipse.centerX - ellipse.radiusX, ellipse.centerY - ellipse.radiusY);
        include(ellipse.centerX + ellipse.radiusX, ellipse.centerY + ellipse.radiusY);
    }
    for (const auto& pie : component.pies) {
        include(pie.centerX - pie.radius, pie.centerY - pie.radius);
        include(pie.centerX + pie.radius, pie.centerY + pie.radius);
    }
    for (const auto& arc : component.ellipticalArcs) {
        include(arc.centerX - arc.radiusX, arc.centerY - arc.radiusY);
        include(arc.centerX + arc.radiusX, arc.centerY + arc.radiusY);
    }
    for (const auto& polygon : component.polygons)
        for (const QPointF& point : polygon.vertices)
            includeSchematicPoint(point);
    for (const auto& polyline : component.polylines)
        for (const QPointF& point : polyline.vertices)
            includeSchematicPoint(point);
    for (const auto& path : component.paths)
        for (const QPointF& point : path.vertices)
            includeSchematicPoint(point);
    for (const auto& bezier : component.beziers)
        for (const QPointF& point : bezier.controlPoints)
            includeSchematicPoint(point);
    for (const auto& ieee : component.ieeeSymbols)
        include(ieee.locationX, ieee.locationY);
    for (const auto& text : component.texts)
        include(text.locationX, text.locationY);
    for (const auto& frame : component.textFrames) {
        include(frame.locationX, frame.locationY);
        include(frame.cornerX, frame.cornerY);
    }
    for (const auto& image : component.images) {
        include(image.locationX, image.locationY);
        include(image.cornerX, image.cornerY);
    }
    for (const auto& parameter : component.parameters) {
        if (parameter.locationX != 0 || parameter.locationY != 0)
            include(parameter.locationX, parameter.locationY);
    }
    if (minX == INT_MAX)
        return;

    // 统一量化平移量，避免逐个量化图元改变相对间距。
    const int offsetX = quantizeSchematicOffset((minX + maxX) / 2);  // 量化后的 X 方向原点偏移。
    int offsetY = quantizeSchematicOffset((minY + maxY) / 2);  // 量化后的 Y 方向原点偏移。
    // Altium 符号原点相对 EasyEDA 几何中心存在 10 mil 的 Y 基准偏置。
    constexpr int kAltiumSymbolOriginYBias = 1000000;  // Altium 符号原点的 Y 方向基准偏置。
    offsetY -= kAltiumSymbolOriginYBias;
    const int offsetPolyX = offsetX / 1000;  // 多边形坐标使用的 X 方向偏移。
    const int offsetPolyY = offsetY / 1000;  // 多边形坐标使用的 Y 方向偏移。

    for (auto& pin : component.pins) {
        pin.locationX -= offsetX;
        pin.locationY -= offsetY;
    }
    for (auto& rect : component.rectangles) {
        rect.locationX -= offsetX;
        rect.locationY -= offsetY;
        rect.cornerX -= offsetX;
        rect.cornerY -= offsetY;
    }
    for (auto& rect : component.roundRectangles) {
        rect.locationX -= offsetX;
        rect.locationY -= offsetY;
        rect.cornerX -= offsetX;
        rect.cornerY -= offsetY;
    }
    for (auto& line : component.lines) {
        line.locationX -= offsetX;
        line.locationY -= offsetY;
        line.cornerX -= offsetX;
        line.cornerY -= offsetY;
    }
    for (auto& arc : component.arcs) {
        arc.centerX -= offsetX;
        arc.centerY -= offsetY;
    }
    for (auto& ellipse : component.ellipses) {
        ellipse.centerX -= offsetX;
        ellipse.centerY -= offsetY;
    }
    for (auto& pie : component.pies) {
        pie.centerX -= offsetX;
        pie.centerY -= offsetY;
    }
    for (auto& arc : component.ellipticalArcs) {
        arc.centerX -= offsetX;
        arc.centerY -= offsetY;
    }
    for (auto& polygon : component.polygons)
        for (QPointF& point : polygon.vertices)
            point = QPointF(point.x() - offsetPolyX, point.y() - offsetPolyY);
    for (auto& polyline : component.polylines)
        for (QPointF& point : polyline.vertices)
            point = QPointF(point.x() - offsetPolyX, point.y() - offsetPolyY);
    for (auto& path : component.paths)
        for (QPointF& point : path.vertices)
            point = QPointF(point.x() - offsetPolyX, point.y() - offsetPolyY);
    for (auto& bezier : component.beziers)
        for (QPointF& point : bezier.controlPoints)
            point = QPointF(point.x() - offsetPolyX, point.y() - offsetPolyY);
    for (auto& ieee : component.ieeeSymbols) {
        ieee.locationX -= offsetX;
        ieee.locationY -= offsetY;
    }
    for (auto& text : component.texts) {
        text.locationX -= offsetX;
        text.locationY -= offsetY;
    }
    for (auto& frame : component.textFrames) {
        frame.locationX -= offsetX;
        frame.locationY -= offsetY;
        frame.cornerX -= offsetX;
        frame.cornerY -= offsetY;
    }
    for (auto& image : component.images) {
        image.locationX -= offsetX;
        image.locationY -= offsetY;
        image.cornerX -= offsetX;
        image.cornerY -= offsetY;
    }
    for (auto& parameter : component.parameters) {
        parameter.locationX -= offsetX;
        parameter.locationY -= offsetY;
    }

    // 将引脚主体端投影到矩形主体边界，保持连接方向与符号轮廓一致。
    int bodyMinX = INT_MAX, bodyMinY = INT_MAX;  // 矩形主体的最小边界。
    int bodyMaxX = INT_MIN, bodyMaxY = INT_MIN;  // 矩形主体的最大边界。
    for (const auto& rect : component.rectangles) {
        bodyMinX = qMin(bodyMinX, qMin(rect.locationX, rect.cornerX));
        bodyMinY = qMin(bodyMinY, qMin(rect.locationY, rect.cornerY));
        bodyMaxX = qMax(bodyMaxX, qMax(rect.locationX, rect.cornerX));
        bodyMaxY = qMax(bodyMaxY, qMax(rect.locationY, rect.cornerY));
    }
    for (const auto& rect : component.roundRectangles) {
        bodyMinX = qMin(bodyMinX, qMin(rect.locationX, rect.cornerX));
        bodyMinY = qMin(bodyMinY, qMin(rect.locationY, rect.cornerY));
        bodyMaxX = qMax(bodyMaxX, qMax(rect.locationX, rect.cornerX));
        bodyMaxY = qMax(bodyMaxY, qMax(rect.locationY, rect.cornerY));
    }
    if (bodyMinX != INT_MAX) {
        constexpr int kPinTextGap = 200000;  // 引脚主体端与名称之间的 20 mil 间距。
        constexpr int kCenterTextGap = 200000;  // 左右名称列与主体中心之间的 20 mil 间距。
        int longestPinLabelWidth = 0;  // 当前符号中最长引脚名称的估算宽度。
        for (const AltiumSchText& text : component.texts) {
            if (!text.isPinLabel || text.isPinNumber || text.sourcePinIndex < 0 ||
                text.sourcePinIndex >= component.pins.size())
                continue;
            const AltiumSchPin& pin = component.pins.at(text.sourcePinIndex);  // 当前名称对应的引脚。
            if (pin.orientation == AltiumModels::PinOrientation::Left ||
                pin.orientation == AltiumModels::PinOrientation::Right)
                longestPinLabelWidth = qMax(longestPinLabelWidth, estimatePinLabelWidth(text));
        }
        if (longestPinLabelWidth > 0) {
            const int requiredHalfWidth = longestPinLabelWidth + kPinTextGap + kCenterTextGap;
            const QPair<int, int> expandedBounds =
                expandBodyForPinLabels(component, bodyMinX, bodyMaxX, requiredHalfWidth);
            bodyMinX = expandedBounds.first;
            bodyMaxX = expandedBounds.second;
        }
        for (auto& pin : component.pins) {
            // 将引脚主体端投影到对应的矩形边界，保持引脚方向和主体连接一致。
            switch (pin.orientation) {
                case AltiumModels::PinOrientation::Right:
                    pin.locationX = bodyMaxX;
                    break;
                case AltiumModels::PinOrientation::Left:
                    pin.locationX = bodyMinX;
                    break;
                case AltiumModels::PinOrientation::Up:
                    pin.locationY = bodyMaxY;
                    break;
                case AltiumModels::PinOrientation::Down:
                    pin.locationY = bodyMinY;
                    break;
            }
        }
        quantizePinConnectionGroups(component.pins);
    }

    // 引脚文本必须在投影和连接点量化后重新布局，避免沿用源坐标造成文本与引脚分离。
    constexpr int kPinTextGap = 200000;  // 引脚主体端或连接端与文字之间的 20 mil 间距。
    constexpr double kPinLabelFontSizeMm = 1.27;  // 引脚名称和编号使用的最大 50 mil 字号。
    for (AltiumSchText& text : component.texts) {
        if (!text.isPinLabel || text.sourcePinIndex < 0 || text.sourcePinIndex >= component.pins.size())
            continue;
        const AltiumSchPin& pin = component.pins.at(text.sourcePinIndex);  // 当前文本对应的规范化引脚。
        if (!std::isfinite(text.fontSizeMm) || text.fontSizeMm <= 0.0 || text.fontSizeMm > kPinLabelFontSizeMm)
            text.fontSizeMm = kPinLabelFontSizeMm;
        const QPointF connection = computePinConnectionPoint(pin);  // 当前引脚的外部连接端。
        // 名称位于主体内侧，编号位于连接端外侧；两者均保持水平正向显示。
        switch (pin.orientation) {
            case AltiumModels::PinOrientation::Right:
                if (text.isPinNumber) {
                    text.locationX = static_cast<int>(connection.x()) + kPinTextGap;
                    text.anchor = QStringLiteral("start");
                } else {
                    text.locationX = pin.locationX - kPinTextGap;
                    text.anchor = QStringLiteral("end");
                }
                text.locationY = pin.locationY;
                break;
            case AltiumModels::PinOrientation::Left:
                if (text.isPinNumber) {
                    text.locationX = static_cast<int>(connection.x()) - kPinTextGap;
                    text.anchor = QStringLiteral("end");
                } else {
                    text.locationX = pin.locationX + kPinTextGap;
                    text.anchor = QStringLiteral("start");
                }
                text.locationY = pin.locationY;
                break;
            case AltiumModels::PinOrientation::Up:
                text.locationX = pin.locationX;
                text.locationY =
                    text.isPinNumber ? static_cast<int>(connection.y()) + kPinTextGap : pin.locationY - kPinTextGap;
                text.anchor = QStringLiteral("middle");
                break;
            case AltiumModels::PinOrientation::Down:
                text.locationX = pin.locationX;
                text.locationY =
                    text.isPinNumber ? static_cast<int>(connection.y()) - kPinTextGap : pin.locationY + kPinTextGap;
                text.anchor = QStringLiteral("middle");
                break;
        }
        text.orientation = 0;
    }

    // 汇总平移后的图形边界，文本和参数字段必须以此边界为基准布局。
    int graphicMinX = INT_MAX, graphicMinY = INT_MAX;  // 可见图形的最小边界。
    int graphicMaxX = INT_MIN, graphicMaxY = INT_MIN;  // 可见图形的最大边界。
    const auto includeGraphic = [&](int x, int y) {
        graphicMinX = qMin(graphicMinX, x);
        graphicMinY = qMin(graphicMinY, y);
        graphicMaxX = qMax(graphicMaxX, x);
        graphicMaxY = qMax(graphicMaxY, y);
    };
    const auto includeGraphicPoint = [&includeGraphic](const QPointF& point) {
        includeGraphic(static_cast<int>(std::lround(point.x() * 1000.0)),
                       static_cast<int>(std::lround(point.y() * 1000.0)));
    };
    for (const auto& rect : component.rectangles) {
        includeGraphic(qMin(rect.locationX, rect.cornerX), qMin(rect.locationY, rect.cornerY));
        includeGraphic(qMax(rect.locationX, rect.cornerX), qMax(rect.locationY, rect.cornerY));
    }
    for (const auto& rect : component.roundRectangles) {
        includeGraphic(qMin(rect.locationX, rect.cornerX), qMin(rect.locationY, rect.cornerY));
        includeGraphic(qMax(rect.locationX, rect.cornerX), qMax(rect.locationY, rect.cornerY));
    }
    for (const auto& line : component.lines) {
        includeGraphic(qMin(line.locationX, line.cornerX), qMin(line.locationY, line.cornerY));
        includeGraphic(qMax(line.locationX, line.cornerX), qMax(line.locationY, line.cornerY));
    }
    for (const auto& arc : component.arcs) {
        includeGraphic(arc.centerX - arc.radius, arc.centerY - arc.radius);
        includeGraphic(arc.centerX + arc.radius, arc.centerY + arc.radius);
    }
    for (const auto& ellipse : component.ellipses) {
        includeGraphic(ellipse.centerX - ellipse.radiusX, ellipse.centerY - ellipse.radiusY);
        includeGraphic(ellipse.centerX + ellipse.radiusX, ellipse.centerY + ellipse.radiusY);
    }
    for (const auto& pie : component.pies) {
        includeGraphic(pie.centerX - pie.radius, pie.centerY - pie.radius);
        includeGraphic(pie.centerX + pie.radius, pie.centerY + pie.radius);
    }
    for (const auto& arc : component.ellipticalArcs) {
        includeGraphic(arc.centerX - arc.radiusX, arc.centerY - arc.radiusY);
        includeGraphic(arc.centerX + arc.radiusX, arc.centerY + arc.radiusY);
    }
    for (const auto& polygon : component.polygons)
        for (const QPointF& point : polygon.vertices)
            includeGraphicPoint(point);
    for (const auto& polyline : component.polylines)
        for (const QPointF& point : polyline.vertices)
            includeGraphicPoint(point);
    for (const auto& path : component.paths)
        for (const QPointF& point : path.vertices)
            includeGraphicPoint(point);
    for (const auto& bezier : component.beziers)
        for (const QPointF& point : bezier.controlPoints)
            includeGraphicPoint(point);
    for (const auto& ieee : component.ieeeSymbols)
        includeGraphic(ieee.locationX, ieee.locationY);
    for (const auto& image : component.images) {
        includeGraphic(qMin(image.locationX, image.cornerX), qMin(image.locationY, image.cornerY));
        includeGraphic(qMax(image.locationX, image.cornerX), qMax(image.locationY, image.cornerY));
    }
    if (graphicMinX == INT_MAX) {
        // 没有独立图形时，以引脚范围作为最后的安全锚点，避免文本落在未定义位置。
        for (const auto& pin : component.pins) {
            includeGraphic(pin.locationX, pin.locationY);
            const QPointF connection = computePinConnectionPoint(pin);  // 无图形时使用的引脚连接端。
            includeGraphic(static_cast<int>(connection.x()), static_cast<int>(connection.y()));
        }
    }

    // 普通可见文本和可见参数统一居中放到图形下方；引脚标签仍由其引脚位置控制。
    QList<int> visibleTextIndices;  // 可下置的普通可见文本索引。
    for (int i = 0; i < component.texts.size(); ++i) {
        if (!component.texts[i].isPinLabel && component.texts[i].isDisplayed && !component.texts[i].isHidden)
            visibleTextIndices.append(i);
    }
    QList<int> visibleParameterIndices;  // 可下置的可见参数索引。
    for (int i = 0; i < component.parameters.size(); ++i) {
        if (!component.parameters[i].isHidden)
            visibleParameterIndices.append(i);
    }
    if (graphicMinX == INT_MAX || (visibleTextIndices.isEmpty() && visibleParameterIndices.isEmpty()))
        return;
    const int centerX = (graphicMinX + graphicMaxX) / 2;  // 图形边界的水平中心。
    constexpr int kFieldGap = 300000;  // 图形底部与第一个文本字段之间的间距。
    constexpr int kFieldSpacing = 2000000;  // 文本字段之间的垂直间距。
    int nextY = graphicMinY - kFieldGap;  // 下置文本字段的起始 Y 坐标。
    for (int order = 0; order < visibleTextIndices.size(); ++order) {
        auto& text = component.texts[visibleTextIndices[order]];  // 当前待布局的普通文本。
        text.locationX = centerX;
        text.locationY = nextY - order * kFieldSpacing;
        text.orientation = 0;
    }
    for (int order = 0; order < visibleParameterIndices.size(); ++order) {
        auto& parameter = component.parameters[visibleParameterIndices[order]];  // 当前待布局的参数字段。
        parameter.locationX = centerX;
        parameter.locationY = nextY - (visibleTextIndices.size() + order) * kFieldSpacing;
        parameter.orientation = 0;
    }
}

}  // namespace EasyKiConverter
