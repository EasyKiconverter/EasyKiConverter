#include "AltiumSymbolPathConverter.h"

#include "AltiumSymbolCurveConverter.h"
#include "utils/AltiumCoord.h"
#include "utils/AltiumSymbolConversionUtils.h"

#include <cmath>

namespace EasyKiConverter {

namespace {

/**
 * @brief 判断二维点是否包含有限坐标。
 * @param point 待检查的二维点。
 * @return 两个坐标是否均为有限值。
 */
bool isFinitePoint(const QPointF& point) {
    return std::isfinite(point.x()) && std::isfinite(point.y());
}

/**
 * @brief 判断点列满足最小数量且所有坐标均有限。
 * @param points 待检查的点列。
 * @param minimum 允许的最小点数。
 * @return 点列是否满足数量和有限值要求。
 */
bool hasFinitePoints(const QList<QPointF>& points, int minimum) {
    if (points.size() < minimum)
        return false;
    for (const QPointF& point : points) {
        if (!isFinitePoint(point))
            return false;
    }
    return true;
}

/**
 * @brief 判断线宽是否为可写入的有限非负值。
 * @param width 待检查的线宽。
 * @return 线宽是否有效。
 */
bool isValidStrokeWidth(double width) {
    return std::isfinite(width) && width >= 0.0;
}

/** @brief 计算同一部件内图元的来源序号。 */
template <typename Values>
/**
 * @brief 计算同一部件内图元的来源序号。
 * @param values 同类图元集合。
 * @param index 当前图元的全局索引。
 * @param partIndex 目标部件索引。
 * @return 当前图元在该部件内的来源序号。
 */
int sourceIndexForPart(const Values& values, int index, int partIndex) {
    int localIndex = 0;  // 当前部件内已遍历图元的数量。
    for (int i = 0; i < index; ++i) {
        if (values.at(i).partIndex == partIndex)
            ++localIndex;
    }
    return localIndex;
}

}  // namespace

/**
 * @brief 转换路径、原生线段和曲线分段，并保持来源序号与诊断顺序。
 * @param symbol 统一 IR 中的符号组件。
 * @param component 接收转换结果的 Altium 符号组件。
 * @param diagnostics 接收跳过和降级信息的诊断列表。
 */
void AltiumSymbolPathConverter::append(const IR::SymbolComponentIR& symbol,
                                       AltiumSchComponent& component,
                                       QStringList& diagnostics) {
    for (int pathIndex = 0; pathIndex < symbol.paths.size(); ++pathIndex) {
        const IR::SymbolPathIR& pathSource = symbol.paths.at(pathIndex);  // 当前源路径。
        if (!isValidStrokeWidth(pathSource.strokeWidth)) {
            diagnostics.append(
                QStringLiteral("符号 %1 路径图元 %2 的线宽无效，已跳过").arg(symbol.name).arg(pathIndex));
            continue;
        }

        // 非填充路径可直接拆分为 Altium 原生线段和 Bézier 记录，避免曲线被
        // 强制膨胀为大量折线；填充路径仍使用闭合点列以保留填充语义。
        if (!pathSource.isFilled && !pathSource.segments.isEmpty()) {
            int segmentIndex = 0;  // 当前路径段的来源序号。
            for (const IR::SymbolPathSegmentIR& segment : pathSource.segments) {
                bool validSegment = isFinitePoint(segment.start) && isFinitePoint(segment.end);  // 路径段参数是否有效。
                // 按路径段类型校验所需控制点，避免将不完整曲线写入目标库。
                switch (segment.type) {
                    case IR::SymbolPathSegmentIR::Type::QuadraticBezier:
                        validSegment = validSegment && isFinitePoint(segment.control1);
                        break;
                    case IR::SymbolPathSegmentIR::Type::CubicBezier:
                        validSegment =
                            validSegment && isFinitePoint(segment.control1) && isFinitePoint(segment.control2);
                        break;
                    case IR::SymbolPathSegmentIR::Type::CircularArc:
                        validSegment = validSegment && isFinitePoint(segment.arcMid);
                        break;
                    case IR::SymbolPathSegmentIR::Type::EllipticalArc:
                        validSegment = validSegment && isFinitePoint(segment.arcCenter) &&
                                       std::isfinite(segment.radiusX) && std::isfinite(segment.radiusY) &&
                                       segment.radiusX > 0.0 && segment.radiusY > 0.0 &&
                                       std::isfinite(segment.arcStartAngle) && std::isfinite(segment.arcEndAngle);
                        break;
                    case IR::SymbolPathSegmentIR::Type::Line:
                        break;
                }
                if (!validSegment) {
                    diagnostics.append(QStringLiteral("符号 %1 路径图元 %2 的段 %3 参数无效，已跳过")
                                           .arg(symbol.name)
                                           .arg(pathIndex)
                                           .arg(segmentIndex));
                    ++segmentIndex;
                    continue;
                }
                if (segment.type == IR::SymbolPathSegmentIR::Type::Line) {
                    AltiumSchPath path;  // 转换后的 Altium 原生线段记录。
                    path.lineWidth = AltiumCoord::lineWidthMmToIndex(pathSource.strokeWidth);
                    path.lineStyle = AltiumSymbolConversionUtils::toAltiumLineStyle(pathSource.strokeStyle);
                    path.color = AltiumSymbolConversionUtils::toAltiumColor(pathSource.strokeColor);
                    path.ownerPartId = AltiumSymbolConversionUtils::toAltiumOwnerPartId(pathSource.partIndex);
                    path.sourceGraphicType = QStringLiteral("PT");
                    path.sourceGraphicIndex = sourceIndexForPart(symbol.paths, pathIndex, pathSource.partIndex);
                    path.sourceSegmentIndex = segmentIndex;
                    path.sourcePartIndex = pathSource.partIndex;
                    path.vertices.append(QPointF(AltiumCoord::mmToSchematicUnits(segment.start.x()),
                                                 AltiumCoord::mmToSchematicUnits(segment.start.y())));
                    path.vertices.append(QPointF(AltiumCoord::mmToSchematicUnits(segment.end.x()),
                                                 AltiumCoord::mmToSchematicUnits(segment.end.y())));
                    component.paths.append(path);
                } else if (segment.type == IR::SymbolPathSegmentIR::Type::QuadraticBezier ||
                           segment.type == IR::SymbolPathSegmentIR::Type::CubicBezier) {
                    IR::SymbolBezierIR bezier;  // 统一 IR 中间 Bézier 曲线。
                    if (segment.type == IR::SymbolPathSegmentIR::Type::QuadraticBezier) {
                        // Altium SchLib 仅提供三次 Bézier 记录；二次曲线可用
                        // C1=P0+2/3(Q-P0), C2=P1+2/3(Q-P1) 精确表示。
                        const QPointF control1 =
                            segment.start + (segment.control1 - segment.start) * (2.0 / 3.0);  // 三次曲线第一控制点。
                        const QPointF control2 =
                            segment.end + (segment.control1 - segment.end) * (2.0 / 3.0);  // 三次曲线第二控制点。
                        bezier.controlPoints = {segment.start, control1, control2, segment.end};
                    } else {
                        bezier.controlPoints = {segment.start, segment.control1, segment.control2, segment.end};
                    }
                    bezier.strokeColor = pathSource.strokeColor;
                    bezier.strokeWidth = pathSource.strokeWidth;
                    bezier.strokeStyle = pathSource.strokeStyle;
                    bezier.partIndex = pathSource.partIndex;
                    AltiumSchBezier altiumBezier =
                        AltiumSymbolCurveConverter::convertBezier(bezier);  // 转换后的 Altium Bézier 记录。
                    altiumBezier.sourceGraphicType = QStringLiteral("PT");
                    altiumBezier.sourceGraphicIndex = sourceIndexForPart(symbol.paths, pathIndex, pathSource.partIndex);
                    altiumBezier.sourceSegmentIndex = segmentIndex;
                    altiumBezier.sourcePartIndex = pathSource.partIndex;
                    component.beziers.append(altiumBezier);
                } else if (segment.type == IR::SymbolPathSegmentIR::Type::EllipticalArc) {
                    IR::SymbolEllipticalArcIR ellipseArc;  // 统一 IR 中间椭圆弧。
                    ellipseArc.center = segment.arcCenter;
                    ellipseArc.radiusX = segment.radiusX;
                    ellipseArc.radiusY = segment.radiusY;
                    ellipseArc.startAngle = segment.arcStartAngle;
                    ellipseArc.endAngle = segment.arcEndAngle;
                    ellipseArc.strokeColor = pathSource.strokeColor;
                    ellipseArc.strokeWidth = pathSource.strokeWidth;
                    ellipseArc.strokeStyle = pathSource.strokeStyle;
                    ellipseArc.partIndex = pathSource.partIndex;
                    AltiumSchEllipticalArc altiumArc =
                        AltiumSymbolCurveConverter::convertEllipticalArc(ellipseArc);  // 转换后的椭圆弧记录。
                    altiumArc.sourceGraphicType = QStringLiteral("PT");
                    altiumArc.sourceGraphicIndex = sourceIndexForPart(symbol.paths, pathIndex, pathSource.partIndex);
                    altiumArc.sourceSegmentIndex = segmentIndex;
                    altiumArc.sourcePartIndex = pathSource.partIndex;
                    if (altiumArc.radiusX <= 0 || altiumArc.radiusY <= 0) {
                        diagnostics.append(QStringLiteral("符号 %1 路径图元 %2 的段 %3 椭圆弧半径量化后无效，已跳过")
                                               .arg(symbol.name)
                                               .arg(pathIndex)
                                               .arg(segmentIndex));
                        ++segmentIndex;
                        continue;
                    }
                    component.ellipticalArcs.append(altiumArc);
                } else {
                    IR::SymbolArcIR arc;  // 统一 IR 中间圆弧。
                    arc.startPoint = segment.start;
                    arc.midPoint = segment.arcMid;
                    arc.endPoint = segment.end;
                    arc.strokeColor = pathSource.strokeColor;
                    arc.strokeWidth = pathSource.strokeWidth;
                    arc.strokeStyle = pathSource.strokeStyle;
                    arc.partIndex = pathSource.partIndex;
                    AltiumSchArc altiumArc = AltiumSymbolCurveConverter::convertArc(arc);  // 转换后的圆弧记录。
                    if (altiumArc.radius <= 0) {
                        diagnostics.append(QStringLiteral("符号 %1 路径图元 %2 的段 %3 圆弧半径量化后无效，已跳过")
                                               .arg(symbol.name)
                                               .arg(pathIndex)
                                               .arg(segmentIndex));
                        ++segmentIndex;
                        continue;
                    }
                    altiumArc.sourceGraphicType = QStringLiteral("PT");
                    altiumArc.sourceGraphicIndex = sourceIndexForPart(symbol.paths, pathIndex, pathSource.partIndex);
                    altiumArc.sourceSegmentIndex = segmentIndex;
                    altiumArc.sourcePartIndex = pathSource.partIndex;
                    component.arcs.append(altiumArc);
                }
                ++segmentIndex;
            }
        } else {
            const int minimumPointCount = pathSource.isFilled ? 3 : 2;  // 填充路径和开放路径所需的最小点数。
            if (!hasFinitePoints(pathSource.points, minimumPointCount)) {
                diagnostics.append(
                    QStringLiteral("符号 %1 路径图元 %2 的点列无效，已跳过").arg(symbol.name).arg(pathIndex));
                continue;
            }
            AltiumSchPath path = AltiumSymbolCurveConverter::convertPath(pathSource);  // 转换后的路径记录。
            path.sourceGraphicType = QStringLiteral("PT");
            path.sourceGraphicIndex = sourceIndexForPart(symbol.paths, pathIndex, pathSource.partIndex);
            path.sourcePartIndex = pathSource.partIndex;
            component.paths.append(path);
        }
    }
}

}  // namespace EasyKiConverter
