#include "FootprintGraphicsGenerator.h"

#include "KiCadTypeMap.h"

#include <QDebug>
#include <QRegularExpression>

#include <cmath>

namespace EasyKiConverter {

/**
 * @brief 将 IR 焊盘转换为 KiCad 封装焊盘文本。
 * @param pad 待转换的 IR 焊盘。
 * @param originX 封装原点 X 坐标。
 * @param originY 封装原点 Y 坐标。
 * @return KiCad 焊盘文本。
 */
QString FootprintGraphicsGenerator::generatePad(const IR::FootprintPadIR& pad, double originX, double originY) const {
    QString content;  // 当前焊盘序列化结果。

    // 坐标已为 mm，计算相对位置
    double x = roundTo2(pad.position.x() - originX);  // 焊盘相对原点的 X 坐标。
    double y = roundTo2(pad.position.y() - originY);  // 焊盘相对原点的 Y 坐标。
    double width = roundTo2(pad.size.width());  // 焊盘宽度。
    double height = roundTo2(pad.size.height());  // 焊盘高度。
    double holeRadius = roundTo2(pad.holeSize / 2.0);  // 通孔半径。

    QString padNumber = pad.number;  // 经过括号清理后的焊盘编号。
    if (padNumber.contains("(") && padNumber.contains(")")) {
        int start = padNumber.indexOf("(");  // 编号中左括号的位置。
        int end = padNumber.indexOf(")");  // 编号中右括号的位置。
        padNumber = padNumber.mid(start + 1, end - start - 1);
    }

    double rotation = pad.rotation;  // 焊盘旋转角度。
    if (rotation > 180.0) {
        rotation = rotation - 360.0;
    }

    // 使用 IR 映射获取 KiCad 焊盘形状
    bool isCustomShape = (pad.shape == IR::PadShape::Polygon);  // 是否使用自定义多边形焊盘。
    QString polygonStr;  // 自定义多边形的 KiCad 文本。

    if (isCustomShape) {
        if (pad.customShapePoints.isEmpty()) {
            qWarning() << "Pad" << pad.number << "is a custom polygon, but has no points defined";
        } else {
            width = 0.005;
            height = 0.005;
            rotation = 0.0;

            QString path;  // 多边形顶点序列化文本。
            // customShapePoints 已经是相对于焊盘中心的坐标（在 FootprintDataConverter 中计算）
            for (const QPointF& pt : pad.customShapePoints) {
                double relX = roundTo2(pt.x());  // 顶点相对焊盘中心的 X 坐标。
                double relY = roundTo2(pt.y());  // 顶点相对焊盘中心的 Y 坐标。
                path += QString("(xy %1 %2) ").arg(relX, 0, 'f', 2).arg(relY, 0, 'f', 2);
            }

            polygonStr = QString(
                             "\n    (primitives\n      (gr_poly\n        (pts %1)\n        (width 0.1)\n        (fill "
                             "yes)\n      )\n    )\n  ")
                             .arg(path);
        }
    } else {
        width = qMax(width, 0.01);
        height = qMax(height, 0.01);
    }

    QString kicadShape = KiCadTypeMap::toKicadPadShape(pad.shape);  // KiCad 焊盘形状名称。

    QString kicadType = padTypeToKicad(pad);  // KiCad 焊盘电气类型。
    QString layers = padLayersToKicad(pad.layer);  // KiCad 焊盘层列表。

    QString drillStr;  // 钻孔描述文本。
    if (pad.isThroughHole()) {
        double holeRadiusMm = roundTo2(pad.holeSize / 2.0);  // 毫米单位的钻孔半径。
        if (holeRadiusMm > 0 && pad.holeLength > 0) {
            double holeLengthMm = roundTo2(pad.holeLength);  // 槽孔长度。
            double maxDistanceHole = qMax(holeRadiusMm * 2, holeLengthMm);  // 槽孔最大外形尺寸。
            double pos0 = height - maxDistanceHole;  // 槽孔沿水平轴放置时的剩余间距。
            double pos90 = width - maxDistanceHole;  // 槽孔沿垂直轴放置时的剩余间距。
            double maxDistance = qMax(pos0, pos90);  // 用于判断槽孔主轴方向的间距。

            if (qAbs(maxDistance - pos0) < qAbs(maxDistance - pos90)) {
                drillStr = QString(" (drill oval %1 %2)").arg(holeRadiusMm * 2, 0, 'f', 2).arg(holeLengthMm, 0, 'f', 2);
            } else {
                drillStr = QString(" (drill oval %1 %2)").arg(holeLengthMm, 0, 'f', 2).arg(holeRadiusMm * 2, 0, 'f', 2);
            }
        } else if (holeRadiusMm > 0) {
            double drillDiameter = holeRadiusMm * 2;  // 圆孔直径。
            drillStr = QString(" (drill %1)").arg(drillDiameter, 0, 'f', 2);
        }
    }

    content += QString("  (pad %1 %2 %3 (at %4 %5 %6) (size %7 %8) (layers %9)%10%11)\n")
                   .arg(padNumber)
                   .arg(kicadType)
                   .arg(kicadShape)
                   .arg(x, 0, 'f', 2)
                   .arg(y, 0, 'f', 2)
                   .arg(rotation, 0, 'f', 2)
                   .arg(width, 0, 'f', 2)
                   .arg(height, 0, 'f', 2)
                   .arg(layers)
                   .arg(drillStr)
                   .arg(polygonStr);

    return content;
}

/**
 * @brief 将 IR 走线转换为 KiCad 线段文本。
 * @param track 待转换的 IR 走线。
 * @param originX 封装原点 X 坐标。
 * @param originY 封装原点 Y 坐标。
 * @return KiCad 走线文本。
 */
QString FootprintGraphicsGenerator::generateTrack(const IR::FootprintTrackIR& track,
                                                  double originX,
                                                  double originY) const {
    QString content;  // 当前走线序列化结果。

    // IR 中坐标已解析为 QList<QPointF>
    for (int i = 0; i + 1 < track.points.size(); ++i) {
        double startX = roundTo2(track.points[i].x() - originX);  // 当前线段起点 X 坐标。
        double startY = roundTo2(track.points[i].y() - originY);  // 当前线段起点 Y 坐标。
        double endX = roundTo2(track.points[i + 1].x() - originX);  // 当前线段终点 X 坐标。
        double endY = roundTo2(track.points[i + 1].y() - originY);  // 当前线段终点 Y 坐标。

        content += QString("  (fp_line (start %1 %2) (end %3 %4) (layer %5) (width %6))\n")
                       .arg(startX, 0, 'f', 2)
                       .arg(startY, 0, 'f', 2)
                       .arg(endX, 0, 'f', 2)
                       .arg(endY, 0, 'f', 2)
                       .arg(layerTypeToKicad(track.layer))
                       .arg(roundTo2(track.width), 0, 'f', 2);
    }

    return content;
}

/**
 * @brief 将 IR 机械孔转换为 KiCad 非电气焊盘文本。
 * @param hole 待转换的 IR 机械孔。
 * @param originX 封装原点 X 坐标。
 * @param originY 封装原点 Y 坐标。
 * @return KiCad 机械孔文本。
 */
QString FootprintGraphicsGenerator::generateHole(const IR::FootprintHoleIR& hole,
                                                 double originX,
                                                 double originY) const {
    QString content;  // 当前机械孔序列化结果。
    double cx = roundTo2(hole.center.x() - originX);  // 机械孔中心的相对 X 坐标。
    double cy = roundTo2(hole.center.y() - originY);  // 机械孔中心的相对 Y 坐标。
    double radius = roundTo2(hole.radius);  // 机械孔半径。

    content += QString("  (pad \"\" thru_hole circle (at %1 %2) (size %3 %3) (drill %3) (layers *.Cu *.Mask))\n")
                   .arg(cx, 0, 'f', 2)
                   .arg(cy, 0, 'f', 2)
                   .arg(radius * 2, 0, 'f', 2);

    return content;
}

/**
 * @brief 将 IR 圆形图元转换为 KiCad 圆形文本。
 * @param circle 待转换的 IR 圆形图元。
 * @param originX 封装原点 X 坐标。
 * @param originY 封装原点 Y 坐标。
 * @return KiCad 圆形文本。
 */
QString FootprintGraphicsGenerator::generateCircle(const IR::FootprintCircleIR& circle,
                                                   double originX,
                                                   double originY) const {
    QString content;  // 当前圆形图元序列化结果。
    double cx = roundTo2(circle.center.x() - originX);  // 圆心相对原点的 X 坐标。
    double cy = roundTo2(circle.center.y() - originY);  // 圆心相对原点的 Y 坐标。
    double radius = roundTo2(circle.radius);  // 圆形图元半径。

    content += QString("  (fp_circle (center %1 %2) (end %3 %4) (layer %5) (width %6))\n")
                   .arg(cx, 0, 'f', 2)
                   .arg(cy, 0, 'f', 2)
                   .arg(cx + radius, 0, 'f', 2)
                   .arg(cy, 0, 'f', 2)
                   .arg(layerTypeToKicad(circle.layer))
                   .arg(circle.strokeWidth, 0, 'f', 2);

    return content;
}

/**
 * @brief 将 IR 矩形图元转换为 KiCad 四条边线文本。
 * @param rectangle 待转换的 IR 矩形图元。
 * @param originX 封装原点 X 坐标。
 * @param originY 封装原点 Y 坐标。
 * @return KiCad 矩形边线文本。
 */
QString FootprintGraphicsGenerator::generateRectangle(const IR::FootprintRectangleIR& rectangle,
                                                      double originX,
                                                      double originY) const {
    QString content;  // 当前矩形图元序列化结果。
    double x = roundTo2(rectangle.bounds.left() - originX);  // 矩形左上角相对 X 坐标。
    double y = roundTo2(rectangle.bounds.top() - originY);  // 矩形左上角相对 Y 坐标。
    double width = roundTo2(rectangle.bounds.width());  // 矩形宽度。
    double height = roundTo2(rectangle.bounds.height());  // 矩形高度。
    QString layer = layerTypeToKicad(rectangle.layer);  // 矩形目标图层名称。
    double strokeWidth = roundTo2(rectangle.strokeWidth);  // 矩形边线宽度。

    content += QString("  (fp_line (start %1 %2) (end %3 %2) (layer %4) (width %5))\n")
                   .arg(x, 0, 'f', 2)
                   .arg(y, 0, 'f', 2)
                   .arg(x + width, 0, 'f', 2)
                   .arg(layer)
                   .arg(strokeWidth, 0, 'f', 2);

    content += QString("  (fp_line (start %1 %2) (end %1 %3) (layer %4) (width %5))\n")
                   .arg(x + width, 0, 'f', 2)
                   .arg(y, 0, 'f', 2)
                   .arg(y + height, 0, 'f', 2)
                   .arg(layer)
                   .arg(strokeWidth, 0, 'f', 2);

    content += QString("  (fp_line (start %1 %2) (end %3 %2) (layer %4) (width %5))\n")
                   .arg(x, 0, 'f', 2)
                   .arg(y + height, 0, 'f', 2)
                   .arg(x + width, 0, 'f', 2)
                   .arg(layer)
                   .arg(strokeWidth, 0, 'f', 2);

    content += QString("  (fp_line (start %1 %2) (end %1 %3) (layer %4) (width %5))\n")
                   .arg(x, 0, 'f', 2)
                   .arg(y, 0, 'f', 2)
                   .arg(y + height, 0, 'f', 2)
                   .arg(layer)
                   .arg(strokeWidth, 0, 'f', 2);

    return content;
}

/**
 * @brief 将 IR 圆弧转换为 KiCad 三点弧线文本。
 * @param arc 待转换的 IR 圆弧。
 * @param originX 封装原点 X 坐标。
 * @param originY 封装原点 Y 坐标。
 * @return KiCad 圆弧文本。
 */
QString FootprintGraphicsGenerator::generateArc(const IR::FootprintArcIR& arc, double originX, double originY) const {
    QString content;  // 当前圆弧序列化结果。

    // IR 弧线已解析为 center/radius/startAngle/endAngle
    if (arc.radius > 0) {
        double cx = roundTo2(arc.center.x() - originX);  // 圆心相对原点的 X 坐标。
        double cy = roundTo2(arc.center.y() - originY);  // 圆心相对原点的 Y 坐标。

        double startAngle = arc.startAngle;  // 当前分段的起始角度。
        double sweepAngle = arc.endAngle - arc.startAngle;  // 尚未序列化的扫掠角度。
        if (sweepAngle < 0) {
            startAngle += sweepAngle;
            sweepAngle = -sweepAngle;
        }

        const QString layer = layerTypeToKicad(arc.layer);  // 圆弧目标图层名称。
        const double strokeWidth = roundTo2(arc.width);  // 圆弧线宽。
        while (sweepAngle > 0.1) {
            const double step = qMin(180.0, sweepAngle);  // 当前三点弧线分段角度。
            const double midAngle = startAngle + step / 2.0;  // 当前分段中点角度。
            const double endAngle = startAngle + step;  // 当前分段终点角度。
            const double startRad = startAngle * M_PI / 180.0;  // 起始角度对应的弧度。
            const double midRad = midAngle * M_PI / 180.0;  // 中点角度对应的弧度。
            const double endRad = endAngle * M_PI / 180.0;  // 终点角度对应的弧度。
            const double startX =
                roundTo2(arc.center.x() + arc.radius * std::cos(startRad) - originX);  // 起点 X 坐标。
            const double startY =
                roundTo2(arc.center.y() + arc.radius * std::sin(startRad) - originY);  // 起点 Y 坐标。
            const double midX = roundTo2(arc.center.x() + arc.radius * std::cos(midRad) - originX);  // 中点 X 坐标。
            const double midY = roundTo2(arc.center.y() + arc.radius * std::sin(midRad) - originY);  // 中点 Y 坐标。
            const double endX = roundTo2(arc.center.x() + arc.radius * std::cos(endRad) - originX);  // 终点 X 坐标。
            const double endY = roundTo2(arc.center.y() + arc.radius * std::sin(endRad) - originY);  // 终点 Y 坐标。

            // KiCad 8/9 的封装弧线使用三点语法，不再接受旧的 angle 字段。
            content +=
                QString(
                    "  (fp_arc (start %1 %2) (mid %3 %4) (end %5 %6) (stroke (width %7) (type default)) (layer %8))\n")
                    .arg(startX, 0, 'f', 2)
                    .arg(startY, 0, 'f', 2)
                    .arg(midX, 0, 'f', 2)
                    .arg(midY, 0, 'f', 2)
                    .arg(endX, 0, 'f', 2)
                    .arg(endY, 0, 'f', 2)
                    .arg(strokeWidth, 0, 'f', 2)
                    .arg(layer);

            sweepAngle -= step;
            startAngle = endAngle;
        }
    } else {
        qWarning() << "Warning: Arc has zero radius, skipping";
    }

    return content;
}

/**
 * @brief 将 IR 文本转换为 KiCad 文本或轮廓线。
 * @param text 待转换的 IR 文本。
 * @param originX 封装原点 X 坐标。
 * @param originY 封装原点 Y 坐标。
 * @return KiCad 文本或非 ASCII 轮廓线文本。
 */
QString FootprintGraphicsGenerator::generateText(const IR::FootprintTextIR& text,
                                                 double originX,
                                                 double originY) const {
    QString content;  // 当前文本或轮廓序列化结果。
    double x = roundTo2(text.position.x() - originX);  // 文本相对原点的 X 坐标。
    double y = roundTo2(text.position.y() - originY);  // 文本相对原点的 Y 坐标。

    QString layer = layerTypeToKicad(text.layer);  // 文本目标图层名称。

    // 装配层文本映射
    if (text.isFabrication) {
        layer = layer.replace(".SilkS", ".Fab");
    }

    QString displayStr = text.isDisplayed ? "" : " hide";  // 文本隐藏标记。

    bool isNonASCII = false;  // 是否包含需要转为轮廓的非 ASCII 字符。
    for (int i = 0; i < text.text.length(); ++i) {
        if (text.text[i].unicode() > 127) {
            isNonASCII = true;
            break;
        }
    }
    if (isNonASCII && !text.textPathPoints.isEmpty()) {
        qWarning() << "Warning: Converting non-ASCII text to polygon:" << text.text;

        // IR 中已解析为 QList<QPointF>
        const QList<QPointF>& points = text.textPathPoints;  // 文本轮廓顶点。

        if (points.size() >= 2) {
            // 计算相对坐标
            QList<QPointF> relPoints;  // 相对封装原点的轮廓顶点。
            for (const QPointF& pt : points) {
                relPoints.append(QPointF(roundTo2(pt.x() - originX), roundTo2(pt.y() - originY)));
            }

            for (int i = 1; i < relPoints.size(); ++i) {
                content += QString("  (fp_line (start %1 %2) (end %3 %4) (layer %5) (width %6))\n")
                               .arg(relPoints[i - 1].x(), 0, 'f', 2)
                               .arg(relPoints[i - 1].y(), 0, 'f', 2)
                               .arg(relPoints[i].x(), 0, 'f', 2)
                               .arg(relPoints[i].y(), 0, 'f', 2)
                               .arg(layer)
                               .arg(roundTo2(text.strokeWidth) * 0.8, 0, 'f', 2);
            }
        }
    } else {
        bool isBottomLayer = layer.startsWith("B");  // 是否位于底层。
        QString mirrorStr = isBottomLayer ? " mirror" : "";  // 底层文本的镜像标记。

        content += QString("  (fp_text user %1 (at %2 %3 %4) (layer %5)%6\n")
                       .arg(text.text)
                       .arg(x, 0, 'f', 2)
                       .arg(y, 0, 'f', 2)
                       .arg(text.rotation, 0, 'f', 2)
                       .arg(layer)
                       .arg(displayStr);

        // IR 中 fontSize 和 strokeWidth 已为 mm
        double fontSize = roundTo2(text.fontSize);  // 文本字体高度。
        double thickness = roundTo2(text.strokeWidth);  // 文本笔画宽度。
        fontSize = qMax(fontSize, 1.0);
        thickness = qMax(thickness, 0.01);
        content += QString("    (effects (font (size %1 %2) (thickness %3)) (justify left%4))\n")
                       .arg(fontSize, 0, 'f', 2)
                       .arg(fontSize, 0, 'f', 2)
                       .arg(thickness, 0, 'f', 2)
                       .arg(mirrorStr);
        content += "  )\n";
    }

    return content;
}

/**
 * @brief 将 IR 区域转换为 KiCad 实心区域或禁止区边线。
 * @param region 待转换的 IR 区域。
 * @param originX 封装原点 X 坐标。
 * @param originY 封装原点 Y 坐标。
 * @return KiCad 区域文本。
 */
QString FootprintGraphicsGenerator::generateSolidRegion(const IR::FootprintRegionIR& region,
                                                        double originX,
                                                        double originY) const {
    QString content;  // 当前区域序列化结果。

    QString layer = layerTypeToKicad(region.layer);  // 区域目标图层名称。

    // IR 中顶点已解析为 QList<QPointF>
    QList<QPointF> points;  // 相对封装原点的区域顶点。
    for (const QPointF& pt : region.vertices) {
        points.append(QPointF(roundTo2(pt.x() - originX), roundTo2(pt.y() - originY)));
    }

    // 闭合多边形
    if (!points.isEmpty() && points.first() != points.last()) {
        points.append(points.first());
    }

    if (points.size() >= 2) {
        if (region.layer == IR::LayerType::KeepOut || region.isKeepOut) {
            for (int i = 1; i < points.size(); ++i) {
                content += QString("  (fp_line (start %1 %2) (end %3 %4) (layer %5) (width 0.05))\n")
                               .arg(points[i - 1].x(), 0, 'f', 2)
                               .arg(points[i - 1].y(), 0, 'f', 2)
                               .arg(points[i].x(), 0, 'f', 2)
                               .arg(points[i].y(), 0, 'f', 2)
                               .arg(layer);
            }
        } else {
            content += "  (fp_poly\n";
            content += "    (pts";
            for (const QPointF& pt : points) {
                content += QString(" (xy %1 %2)").arg(pt.x(), 0, 'f', 2).arg(pt.y(), 0, 'f', 2);
            }
            content += ")\n";
            content += QString("    (layer %1)\n").arg(layer);
            content += "    (width 0.1)\n";
            content += "    (fill solid)\n";
            content += "  )\n";
        }
    }

    return content;
}

/**
 * @brief 根据边界框生成 KiCad 封装庭院边线。
 * @param x1 边界左侧 X 坐标。
 * @param y1 边界顶部 Y 坐标。
 * @param x2 边界右侧 X 坐标。
 * @param y2 边界底部 Y 坐标。
 * @return KiCad 庭院边线文本。
 */
QString FootprintGraphicsGenerator::generateCourtyardFromBBox(double x1, double y1, double x2, double y2) const {
    QString content;  // 当前庭院边线序列化结果。

    content += QString("  (fp_line (start %1 %2) (end %3 %2) (layer F.CrtYd) (width 0.05))\n")
                   .arg(x1, 0, 'f', 2)
                   .arg(y1, 0, 'f', 2)
                   .arg(x2, 0, 'f', 2);
    content += QString("  (fp_line (start %3 %2) (end %3 %4) (layer F.CrtYd) (width 0.05))\n")
                   .arg(x2, 0, 'f', 2)
                   .arg(y1, 0, 'f', 2)
                   .arg(y2, 0, 'f', 2);
    content += QString("  (fp_line (start %3 %4) (end %1 %4) (layer F.CrtYd) (width 0.05))\n")
                   .arg(x1, 0, 'f', 2)
                   .arg(x2, 0, 'f', 2)
                   .arg(y2, 0, 'f', 2);
    content += QString("  (fp_line (start %1 %4) (end %1 %2) (layer F.CrtYd) (width 0.05))\n")
                   .arg(x1, 0, 'f', 2)
                   .arg(y1, 0, 'f', 2)
                   .arg(y2, 0, 'f', 2);

    return content;
}

/**
 * @brief 将 IR 三维模型转换为 KiCad 模型节点。
 * @param model3D 待转换的 IR 三维模型。
 * @param model3DPath 导出后使用的模型路径。
 * @return KiCad 三维模型文本。
 */
QString FootprintGraphicsGenerator::generateModel3D(const IR::Model3DIR& model3D, const QString& model3DPath) const {
    QString content;  // 当前三维模型节点序列化结果。

    QString finalPath = model3DPath.isEmpty() ? model3D.name() : model3DPath;  // 最终写入的模型路径。

    // IR 中 translation 和 rotation 已为 mm/度
    double z = -model3D.translation().z;  // KiCad 坐标系下的 Z 偏移。

    double rotX = fmod(360.0 - model3D.rotation().x, 360.0);  // 绕 X 轴的 KiCad 旋转角度。
    if (rotX < 0.0)
        rotX += 360.0;

    double rotY = fmod(360.0 - model3D.rotation().y, 360.0);  // 绕 Y 轴的 KiCad 旋转角度。
    if (rotY < 0.0)
        rotY += 360.0;

    double rotZ = fmod(360.0 - model3D.rotation().z, 360.0);  // 绕 Z 轴的 KiCad 旋转角度。
    if (rotZ < 0.0)
        rotZ += 360.0;

    // STEP 文件需要额外的 stepOffsetMm 补偿
    double finalOffsetX = 0.0;  // STEP 模型的 X 轴补偿偏移。
    double finalOffsetY = 0.0;  // STEP 模型的 Y 轴补偿偏移。

    if (finalPath.endsWith(QStringLiteral(".step"), Qt::CaseInsensitive) ||
        finalPath.endsWith(QStringLiteral(".stp"), Qt::CaseInsensitive)) {
        finalOffsetX = model3D.stepOffsetMm().x;
        finalOffsetY = model3D.stepOffsetMm().y;
        z += model3D.stepOffsetMm().z;
    }

    content += QString("  (model \"%1\"\n").arg(finalPath);

    content += QString("    (offset (xyz %1 %2 %3))\n")
                   .arg(finalOffsetX, 0, 'f', 3)
                   .arg(finalOffsetY, 0, 'f', 3)
                   .arg(z, 0, 'f', 3);

    content += "    (scale (xyz 1 1 1))\n";

    content += QString("    (rotate (xyz %1 %2 %3))\n").arg(rotX, 0, 'f', 0).arg(rotY, 0, 'f', 0).arg(rotZ, 0, 'f', 0);

    content += "  )\n";

    return content;
}

/**
 * @brief 向下截断数值到两位小数。
 * @param value 待处理的数值。
 * @return 向下截断后的数值。
 */
double FootprintGraphicsGenerator::roundTo2(double value) {
    return std::floor(value * 100.0) / 100.0;
}

/**
 * @brief 将 IR 图层类型映射为 KiCad 图层名称。
 * @param layerType IR 图层类型。
 * @return KiCad 图层名称。
 */
QString FootprintGraphicsGenerator::layerTypeToKicad(IR::LayerType layerType) {
    // 按 IR 层语义选择 KiCad 的目标图层名称，未知层统一降级到装配层并记录警告。
    switch (layerType) {
        case IR::LayerType::TopCopper:
            return "F.Cu";
        case IR::LayerType::BottomCopper:
            return "B.Cu";
        case IR::LayerType::InnerCopper1:
            return "In1.Cu";
        case IR::LayerType::InnerCopper2:
            return "In2.Cu";
        case IR::LayerType::InnerCopper3:
            return "In3.Cu";
        case IR::LayerType::InnerCopper4:
            return "In4.Cu";
        case IR::LayerType::TopSilk:
            return "F.SilkS";
        case IR::LayerType::BottomSilk:
            return "B.SilkS";
        case IR::LayerType::TopPaste:
            return "F.Paste";
        case IR::LayerType::BottomPaste:
            return "B.Paste";
        case IR::LayerType::TopMask:
            return "F.Mask";
        case IR::LayerType::BottomMask:
            return "B.Mask";
        case IR::LayerType::TopOverlay:
            return "F.SilkS";
        case IR::LayerType::BottomOverlay:
            return "B.SilkS";
        case IR::LayerType::TopAssembly:
            return "F.Fab";
        case IR::LayerType::BottomAssembly:
            return "B.Fab";
        case IR::LayerType::MultiLayer:
            return "F.Cu";
        case IR::LayerType::KeepOut:
            return "F.CrtYd";
        case IR::LayerType::EdgeCuts:
            return "Edge.Cuts";
        case IR::LayerType::Mechanical1:
            return "Dwgs.User";
        case IR::LayerType::Mechanical2:
            return "Dwgs.User";
        case IR::LayerType::Mechanical3:
            return "Eco1.User";
        case IR::LayerType::Mechanical4:
            return "Eco2.User";
        case IR::LayerType::UserDefined:
            return "Dwgs.User";
        default:
            qWarning() << "Unknown layer type, defaulting to F.Fab";
            return "F.Fab";
    }
}

/**
 * @brief 将 IR 焊盘层类型映射为 KiCad 层列表。
 * @param layer IR 焊盘层类型。
 * @return KiCad 焊盘层列表。
 */
QString FootprintGraphicsGenerator::padLayersToKicad(IR::LayerType layer) {
    // 按焊盘层行为生成 KiCad 的铜层、阻焊层和锡膏层组合。
    switch (layer) {
        case IR::LayerType::TopCopper:
            return "F.Cu F.Paste F.Mask";
        case IR::LayerType::BottomCopper:
            return "B.Cu B.Paste B.Mask";
        case IR::LayerType::TopSilk:
            return "F.SilkS";
        case IR::LayerType::BottomSilk:
            return "B.SilkS";
        case IR::LayerType::MultiLayer:
            return "*.Cu *.Mask";
        case IR::LayerType::TopAssembly:
            return "F.Fab";
        case IR::LayerType::BottomAssembly:
            return "B.Fab";
        case IR::LayerType::UserDefined:
            return "Dwgs.User";
        default:
            qWarning() << "Unknown pad layer type, using default thru-hole configuration";
            return "*.Cu *.Mask";
    }
}

/**
 * @brief 将 IR 焊盘类型映射为 KiCad 焊盘类型名称。
 * @param pad IR 焊盘。
 * @return KiCad 焊盘类型名称。
 */
QString FootprintGraphicsGenerator::padTypeToKicad(const IR::FootprintPadIR& pad) {
    if (pad.isSmd()) {
        return "smd";
    } else if (pad.isThroughHole()) {
        return "thru_hole";
    }
    return "thru_hole";
}

}  // namespace EasyKiConverter
