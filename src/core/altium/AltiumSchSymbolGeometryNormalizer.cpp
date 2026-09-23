#include "AltiumSchSymbolGeometryNormalizer.h"

#include "utils/AltiumSymbolConversionUtils.h"

#include <QPointF>

#include <algorithm>
#include <climits>
#include <cmath>

namespace EasyKiConverter {

namespace {

using AltiumSymbolConversionUtils::quantizeSchematicOffset;

/** @brief 将坐标量化到 Altium SchLib 的 10 mil 吸附网格。 */
int quantizeToSnapGrid(int value) {
    constexpr int kSnapGrid = 100000;
    if (value >= 0)
        return ((value + kSnapGrid / 2) / kSnapGrid) * kSnapGrid;
    return ((value - kSnapGrid / 2) / kSnapGrid) * kSnapGrid;
}

/**
 * @brief 计算引脚的外部连接端。
 * @details Altium 的引脚位置是主体端，连接端沿引脚方向偏移引脚长度。
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

/** @brief 将单个引脚连接端吸附到网格并反推主体端位置。 */
void quantizePinConnectionPoint(AltiumSchPin& pin) {
    const QPointF connection = computePinConnectionPoint(pin);
    const int quantizedConnectionX = quantizeToSnapGrid(static_cast<int>(connection.x()));
    const int quantizedConnectionY = quantizeToSnapGrid(static_cast<int>(connection.y()));
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
 */
void quantizePinConnectionGroups(QList<AltiumSchPin>& pins) {
    constexpr int kSnapGrid = 100000;
    for (int side = 0; side < 4; ++side) {
        QList<int> indices;
        for (int i = 0; i < pins.size(); ++i) {
            if (static_cast<int>(pins[i].orientation) == side)
                indices.append(i);
        }
        std::sort(indices.begin(), indices.end(), [&](int lhs, int rhs) {
            const QPointF left = computePinConnectionPoint(pins[lhs]);
            const QPointF right = computePinConnectionPoint(pins[rhs]);
            const bool horizontal = side == static_cast<int>(AltiumModels::PinOrientation::Right) ||
                                    side == static_cast<int>(AltiumModels::PinOrientation::Left);
            return horizontal ? (left.y() < right.y()) : (left.x() < right.x());
        });

        int previousTangent = INT_MIN;
        for (int index : indices) {
            quantizePinConnectionPoint(pins[index]);
            const QPointF connection = computePinConnectionPoint(pins[index]);
            const bool horizontal = side == static_cast<int>(AltiumModels::PinOrientation::Right) ||
                                    side == static_cast<int>(AltiumModels::PinOrientation::Left);
            int tangent = horizontal ? static_cast<int>(connection.y()) : static_cast<int>(connection.x());
            if (previousTangent != INT_MIN && tangent <= previousTangent) {
                tangent = previousTangent + kSnapGrid;
                // 水平引脚沿 Y 方向、垂直引脚沿 X 方向错开连接端。
                switch (pins[index].orientation) {
                    case AltiumModels::PinOrientation::Right:
                    case AltiumModels::PinOrientation::Left:
                        pins[index].locationY = tangent;
                        break;
                    case AltiumModels::PinOrientation::Up:
                    case AltiumModels::PinOrientation::Down:
                        pins[index].locationX = tangent;
                        break;
                }
            }
            previousTangent = tangent;
        }
    }
}

}  // namespace

/**
 * @brief 计算包围盒并平移所有符号图元。
 * @details 使用图形包围盒中心作为统一原点，同时保留引脚和文本的相对位置。
 */
void AltiumSchSymbolGeometryNormalizer::normalize(AltiumSchComponent& component) {
    int minX = INT_MAX, minY = INT_MAX, maxX = INT_MIN, maxY = INT_MIN;
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
        const QPointF connection = computePinConnectionPoint(pin);
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
    const int offsetX = quantizeSchematicOffset((minX + maxX) / 2);
    int offsetY = quantizeSchematicOffset((minY + maxY) / 2);
    // Altium 符号原点相对 EasyEDA 几何中心存在 10 mil 的 Y 基准偏置。
    constexpr int kAltiumSymbolOriginYBias = 1000000;
    offsetY -= kAltiumSymbolOriginYBias;
    const int offsetPolyX = offsetX / 1000;
    const int offsetPolyY = offsetY / 1000;

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
    int bodyMinX = INT_MAX, bodyMinY = INT_MAX;
    int bodyMaxX = INT_MIN, bodyMaxY = INT_MIN;
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

    // 汇总平移后的图形边界，文本和参数字段必须以此边界为基准布局。
    int graphicMinX = INT_MAX, graphicMinY = INT_MAX;
    int graphicMaxX = INT_MIN, graphicMaxY = INT_MIN;
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
            const QPointF connection = computePinConnectionPoint(pin);
            includeGraphic(static_cast<int>(connection.x()), static_cast<int>(connection.y()));
        }
    }

    // 普通可见文本和可见参数统一居中放到图形下方；引脚标签仍由其引脚位置控制。
    QList<int> visibleTextIndices;
    for (int i = 0; i < component.texts.size(); ++i) {
        if (!component.texts[i].isPinLabel && component.texts[i].isDisplayed && !component.texts[i].isHidden)
            visibleTextIndices.append(i);
    }
    QList<int> visibleParameterIndices;
    for (int i = 0; i < component.parameters.size(); ++i) {
        if (!component.parameters[i].isHidden)
            visibleParameterIndices.append(i);
    }
    if (graphicMinX == INT_MAX || (visibleTextIndices.isEmpty() && visibleParameterIndices.isEmpty()))
        return;
    const int centerX = (graphicMinX + graphicMaxX) / 2;
    constexpr int kFieldGap = 300000;
    constexpr int kFieldSpacing = 2000000;
    int nextY = graphicMinY - kFieldGap;
    for (int order = 0; order < visibleTextIndices.size(); ++order) {
        auto& text = component.texts[visibleTextIndices[order]];
        text.locationX = centerX;
        text.locationY = nextY - order * kFieldSpacing;
        text.orientation = 0;
    }
    for (int order = 0; order < visibleParameterIndices.size(); ++order) {
        auto& parameter = component.parameters[visibleParameterIndices[order]];
        parameter.locationX = centerX;
        parameter.locationY = nextY - (visibleTextIndices.size() + order) * kFieldSpacing;
        parameter.orientation = 0;
    }
}

}  // namespace EasyKiConverter
