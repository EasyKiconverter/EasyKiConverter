#include "AltiumFootprintGeometryNormalizer.h"

#include <QtGlobal>

#include <cmath>
#include <limits>

namespace EasyKiConverter {

namespace {

/**
 * @brief 将有限浮点区域坐标安全转换为 PcbLib 使用的整数坐标。
 * @param value 待转换的浮点坐标。
 * @return 截断并限制在 qint64 范围内的坐标值。
 */
qint64 toBoundedCoordinate(double value) {
    if (!std::isfinite(value))
        return qint64(0);
    constexpr double maxValue = static_cast<double>(std::numeric_limits<qint64>::max());  // qint64 可表示的最大坐标。
    constexpr double minValue = static_cast<double>(std::numeric_limits<qint64>::min());  // qint64 可表示的最小坐标。
    if (value >= maxValue)
        return std::numeric_limits<qint64>::max();
    if (value <= minValue)
        return std::numeric_limits<qint64>::min();
    return static_cast<qint64>(value);
}

/**
 * @brief 将一个整数坐标加入包围盒。
 * @param bounds 待更新的包围盒。
 * @param x 坐标的 X 分量。
 * @param y 坐标的 Y 分量。
 */
void includePoint(AltiumFootprintBounds& bounds, qint64 x, qint64 y) {
    if (!bounds.valid) {
        bounds.minX = bounds.maxX = x;
        bounds.minY = bounds.maxY = y;
        bounds.valid = true;
        return;
    }
    bounds.minX = qMin(bounds.minX, x);
    bounds.minY = qMin(bounds.minY, y);
    bounds.maxX = qMax(bounds.maxX, x);
    bounds.maxY = qMax(bounds.maxY, y);
}

}  // namespace

/**
 * @brief 计算封装所有二维图元的包围盒。
 * @details 区域坐标的转换策略由调用方指定，以保持原点归一化和三维轮廓生成的既有边界行为。
 * @param component 待计算的 PcbLib 封装。
 * @param clampRegionCoordinates 是否将区域顶点有限值安全转换为整数。
 * @return 图元存在时返回有效包围盒。
 */
AltiumFootprintBounds AltiumFootprintGeometryNormalizer::computeBounds(const AltiumPcbComponent& component,
                                                                       bool clampRegionCoordinates) {
    AltiumFootprintBounds bounds;  // 当前封装所有二维图元的累计包围盒。
    for (const auto& pad : component.pads)
        includePoint(bounds, pad.locationX, pad.locationY);
    for (const auto& track : component.tracks) {
        includePoint(bounds, qMin(track.startX, track.endX), qMin(track.startY, track.endY));
        includePoint(bounds, qMax(track.startX, track.endX), qMax(track.startY, track.endY));
    }
    for (const auto& arc : component.arcs) {
        includePoint(
            bounds, static_cast<qint64>(arc.centerX) - arc.radius, static_cast<qint64>(arc.centerY) - arc.radius);
        includePoint(
            bounds, static_cast<qint64>(arc.centerX) + arc.radius, static_cast<qint64>(arc.centerY) + arc.radius);
    }
    for (const auto& fill : component.fills) {
        includePoint(bounds, qMin(fill.corner1X, fill.corner2X), qMin(fill.corner1Y, fill.corner2Y));
        includePoint(bounds, qMax(fill.corner1X, fill.corner2X), qMax(fill.corner1Y, fill.corner2Y));
    }
    for (const auto& region : component.regions) {
        for (const QPointF& vertex : region.vertices) {
            // 将区域顶点转换为用于包围盒计算的整数 X 坐标。
            const qint64 x = clampRegionCoordinates ? toBoundedCoordinate(vertex.x()) : static_cast<int>(vertex.x());
            // 将区域顶点转换为用于包围盒计算的整数 Y 坐标。
            const qint64 y = clampRegionCoordinates ? toBoundedCoordinate(vertex.y()) : static_cast<int>(vertex.y());
            includePoint(bounds, x, y);
        }
    }
    return bounds;
}

/**
 * @brief 将封装中的焊盘、线段、弧线、区域、文本和模型统一平移。
 * @param component 待平移的 PcbLib 封装。
 * @param offsetX X 方向偏移量。
 * @param offsetY Y 方向偏移量。
 */
void AltiumFootprintGeometryNormalizer::translate(AltiumPcbComponent& component, int offsetX, int offsetY) {
    for (auto& pad : component.pads) {
        pad.locationX -= offsetX;
        pad.locationY -= offsetY;
    }
    for (auto& track : component.tracks) {
        track.startX -= offsetX;
        track.startY -= offsetY;
        track.endX -= offsetX;
        track.endY -= offsetY;
    }
    for (auto& arc : component.arcs) {
        arc.centerX -= offsetX;
        arc.centerY -= offsetY;
    }
    for (auto& fill : component.fills) {
        fill.corner1X -= offsetX;
        fill.corner1Y -= offsetY;
        fill.corner2X -= offsetX;
        fill.corner2Y -= offsetY;
    }
    for (auto& region : component.regions) {
        for (QPointF& vertex : region.vertices)
            vertex = QPointF(vertex.x() - offsetX, vertex.y() - offsetY);
    }
    for (auto& text : component.texts) {
        text.locationX -= offsetX;
        text.locationY -= offsetY;
    }
    for (auto& model : component.models) {
        model.x -= offsetX;
        model.y -= offsetY;
    }
}

}  // namespace EasyKiConverter
