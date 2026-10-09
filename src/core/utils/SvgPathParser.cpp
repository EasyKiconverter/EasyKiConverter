/**
 * @file SvgPathParser.cpp
 * @brief SvgPathParser 的实现。
 */
#include "SvgPathParser.h"

#include <QDebug>
#include <QRegularExpression>

#include <cmath>

namespace EasyKiConverter {

const double PI = 3.14159265358979323846;  ///< 圆周率常量，用于角度与弧度换算。

namespace {

struct SvgArcGeometry {
    bool valid = false;  ///< 圆弧几何参数是否有效。
    QPointF center;  ///< 椭圆圆心。
    double radiusX = 0.0;  ///< 椭圆 X 方向半径。
    double radiusY = 0.0;  ///< 椭圆 Y 方向半径。
    double startAngle = 0.0;  ///< 圆弧起始角度，单位为弧度。
    double deltaAngle = 0.0;  ///< 圆弧扫过角度，单位为弧度。
};

/**
 * @brief 检查点坐标是否为有限数。
 * @param point 待检查的二维点。
 * @return 点的两个坐标均为有限数时返回 true。
 */
bool isFinitePoint(const QPointF& point) {
    return std::isfinite(point.x()) && std::isfinite(point.y());
}

/**
 * @brief 检查路径线段的几何参数是否均为有限数。
 * @param segment 待检查的 SVG 路径线段。
 * @return 线段所有端点、控制点和圆弧参数有效时返回 true。
 */
bool isFiniteSegment(const SvgPathSegment& segment) {
    return isFinitePoint(segment.start) && isFinitePoint(segment.control1) && isFinitePoint(segment.control2) &&
           isFinitePoint(segment.arcMid) && isFinitePoint(segment.arcCenter) && isFinitePoint(segment.end) &&
           std::isfinite(segment.radiusX) && std::isfinite(segment.radiusY) && std::isfinite(segment.arcStartAngle) &&
           std::isfinite(segment.arcEndAngle);
}

/**
 * @brief 根据 SVG 椭圆弧参数计算圆心和扫掠角度。
 * @param start 圆弧起点。
 * @param radiusX X 方向半径。
 * @param radiusY Y 方向半径。
 * @param xRotation 椭圆旋转角度，单位为度。
 * @param largeArc 是否选择大弧段。
 * @param sweep 是否沿正方向扫掠。
 * @param end 圆弧终点。
 * @return 规范化后的圆弧几何参数。
 */
SvgArcGeometry calculateArcGeometry(const QPointF& start,
                                    double radiusX,
                                    double radiusY,
                                    double xRotation,
                                    bool largeArc,
                                    bool sweep,
                                    const QPointF& end) {
    SvgArcGeometry geometry;  // 待计算的圆弧几何结果。
    radiusX = std::abs(radiusX);
    radiusY = std::abs(radiusY);
    if (radiusX <= 0.0 || radiusY <= 0.0 || start == end)
        return geometry;

    const double phi = xRotation * PI / 180.0;  // 椭圆旋转角度，单位为弧度。
    const double cosPhi = std::cos(phi);  // 旋转角度余弦。
    const double sinPhi = std::sin(phi);  // 旋转角度正弦。
    const double dx = (start.x() - end.x()) / 2.0;  // 端点中点坐标的 X 偏移。
    const double dy = (start.y() - end.y()) / 2.0;  // 端点中点坐标的 Y 偏移。
    const double xPrime = cosPhi * dx + sinPhi * dy;  // 旋转坐标系中的 X 坐标。
    const double yPrime = -sinPhi * dx + cosPhi * dy;  // 旋转坐标系中的 Y 坐标。
    double rxSquared = radiusX * radiusX;  // X 半径平方。
    double rySquared = radiusY * radiusY;  // Y 半径平方。
    const double lambda = xPrime * xPrime / rxSquared + yPrime * yPrime / rySquared;  // 半径修正比例。
    if (lambda > 1.0) {
        const double scale = std::sqrt(lambda);  // 保证端点落在椭圆上的半径缩放因子。
        radiusX *= scale;
        radiusY *= scale;
        rxSquared = radiusX * radiusX;
        rySquared = radiusY * radiusY;
    }

    const double denominator = rxSquared * yPrime * yPrime + rySquared * xPrime * xPrime;  // 圆心公式分母。
    if (qFuzzyIsNull(denominator))
        return geometry;
    const double numerator =  // 圆心公式中经过非负裁剪的根式分子。
        std::max(0.0, rxSquared * rySquared - rxSquared * yPrime * yPrime - rySquared * xPrime * xPrime);
    const double factor =  // 决定大弧和扫掠方向的圆心符号因子。
        (largeArc == sweep ? -1.0 : 1.0) * std::sqrt(numerator / denominator);
    const double centerPrimeX = factor * radiusX * yPrime / radiusY;  // 旋转坐标系中的圆心 X 坐标。
    const double centerPrimeY = -factor * radiusY * xPrime / radiusX;  // 旋转坐标系中的圆心 Y 坐标。
    const QPointF center(  // 原始坐标系中的圆心。
        cosPhi * centerPrimeX - sinPhi * centerPrimeY + (start.x() + end.x()) / 2.0,
        sinPhi * centerPrimeX + cosPhi * centerPrimeY + (start.y() + end.y()) / 2.0);
    const double ux = (xPrime - centerPrimeX) / radiusX;  // 起点单位向量 X 分量。
    const double uy = (yPrime - centerPrimeY) / radiusY;  // 起点单位向量 Y 分量。
    const double vx = (-xPrime - centerPrimeX) / radiusX;  // 终点单位向量 X 分量。
    const double vy = (-yPrime - centerPrimeY) / radiusY;  // 终点单位向量 Y 分量。
    double deltaAngle = std::atan2(ux * vy - uy * vx, ux * vx + uy * vy);  // 初始扫掠角度。
    if (sweep && deltaAngle < 0.0)
        deltaAngle += 2.0 * PI;
    else if (!sweep && deltaAngle > 0.0)
        deltaAngle -= 2.0 * PI;

    geometry.valid = true;
    geometry.center = center;
    geometry.radiusX = radiusX;
    geometry.radiusY = radiusY;
    geometry.startAngle = std::atan2(uy, ux);
    geometry.deltaAngle = deltaAngle;
    return geometry;
}

/**
 * @brief 将旋转椭圆圆弧按不超过九十度的区间近似为三次贝塞尔线段。
 * @param geometry 已计算的椭圆弧几何参数。
 * @param xRotation 椭圆旋转角度，单位为度。
 * @return 近似圆弧的三次贝塞尔线段列表。
 */
QList<SvgPathSegment> approximateRotatedEllipse(const SvgArcGeometry& geometry, double xRotation) {
    QList<SvgPathSegment> segments;  // 生成的贝塞尔线段列表。
    const double segmentAngle = PI / 2.0;  // 单段最大扫掠角度。
    const int segmentCount =  // 圆弧需要拆分的线段数量。
        qMax(1, static_cast<int>(std::ceil(std::abs(geometry.deltaAngle) / segmentAngle)));
    const double phi = xRotation * PI / 180.0;  // 椭圆旋转角度，单位为弧度。
    const double cosPhi = std::cos(phi);  // 旋转角度余弦。
    const double sinPhi = std::sin(phi);  // 旋转角度正弦。
    const auto pointAt = [&](double angle) {
        return QPointF(geometry.center.x() + cosPhi * geometry.radiusX * std::cos(angle) -
                           sinPhi * geometry.radiusY * std::sin(angle),
                       geometry.center.y() + sinPhi * geometry.radiusX * std::cos(angle) +
                           cosPhi * geometry.radiusY * std::sin(angle));
    };
    const auto derivativeAt = [&](double angle) {
        return QPointF(-cosPhi * geometry.radiusX * std::sin(angle) - sinPhi * geometry.radiusY * std::cos(angle),
                       -sinPhi * geometry.radiusX * std::sin(angle) + cosPhi * geometry.radiusY * std::cos(angle));
    };

    for (int i = 0; i < segmentCount; ++i) {
        const double startAngle = geometry.startAngle + geometry.deltaAngle * i / segmentCount;  // 当前段起始角度。
        const double endAngle = geometry.startAngle + geometry.deltaAngle * (i + 1) / segmentCount;  // 当前段终止角度。
        const double deltaAngle = endAngle - startAngle;  // 当前段扫掠角度。
        const double controlFactor = 4.0 / 3.0 * std::tan(deltaAngle / 4.0);  // 贝塞尔控制点比例。
        const QPointF start = pointAt(startAngle);  // 当前段起点。
        const QPointF end = pointAt(endAngle);  // 当前段终点。
        const QPointF startDerivative = derivativeAt(startAngle);  // 起点切线方向。
        const QPointF endDerivative = derivativeAt(endAngle);  // 终点切线方向。
        SvgPathSegment segment;  // 当前生成的贝塞尔线段。
        segment.type = SvgPathSegment::Type::CubicBezier;
        segment.start = i == 0 ? start : segments.last().end;
        segment.control1 = start + controlFactor * startDerivative;
        segment.control2 = end - controlFactor * endDerivative;
        segment.end = i == segmentCount - 1 ? pointAt(geometry.startAngle + geometry.deltaAngle) : end;
        segments.append(segment);
    }
    return segments;
}

}  // namespace

/**
 * @brief 解析 SVG 路径并提取兼容旧调用方的折线点集合。
 * @param path SVG 路径文本。
 * @return 路径中的折线点列表。
 */
QList<QPointF> SvgPathParser::parsePath(const QString& path) {
    QList<QPointF> points;  // 解析得到的路径点集合。
    if (path.isEmpty()) {
        return points;
    }

    QStringList tokens = splitPath(path);  // 分词后的 SVG 命令和参数。
    for (const QString& token : tokens) {
        if (token.isEmpty() || token.at(0).isLetter())
            continue;
        bool ok = false;  // 参数转换是否成功。
        const double value = token.toDouble(&ok);  // 当前参数数值。
        if (!ok || !std::isfinite(value))
            return {};
    }
    double currentX = 0.0;  // 当前路径位置的 X 坐标。
    double currentY = 0.0;  // 当前路径位置的 Y 坐标。
    QPointF lastCubicControl;  // 上一个三次贝塞尔控制点。
    QPointF lastQuadraticControl;  // 上一个二次贝塞尔控制点。
    QChar previousCommand;  // 上一个 SVG 路径命令。

    int i = 0;  // 当前命令或参数在分词列表中的索引。
    while (i < tokens.size()) {
        QString cmd = tokens[i];  // 当前 SVG 命令词。
        if (cmd.isEmpty()) {
            i++;
            continue;
        }
        // 参数只能由前面的命令消费；遇到残留数字时跳过，避免把参数误报为命令。
        if (!cmd.at(0).isLetter()) {
            i++;
            continue;
        }

        QChar command = cmd[0].toUpper();  // 规范化后的命令字符。

        // 处理M/m（MoveTo）命
        if (command == 'M') {
            bool relative = (cmd[0] == 'm');  // 当前 MoveTo 是否使用相对坐标。
            i++;
            while (i < tokens.size()) {
                bool okX = false;  // X 参数转换是否成功。
                bool okY = false;  // Y 参数转换是否成功。
                double x = tokens[i].toDouble(&okX);  // 当前点的 X 参数。
                if (!okX)
                    break;
                i++;
                if (i >= tokens.size())
                    break;
                double y = tokens[i].toDouble(&okY);  // 当前点的 Y 参数。
                if (!okY)
                    break;
                i++;

                QPointF pt = createPoint(x, y, relative, currentX, currentY);  // 转换后的路径点。
                points.append(pt);
            }
            previousCommand = 'M';
        }
        // 处理L/l（LineTo）命
        else if (command == 'L') {
            bool relative = (cmd[0] == 'l');  // 当前 LineTo 是否使用相对坐标。
            i++;
            while (i < tokens.size()) {
                bool okX = false;  // X 参数转换是否成功。
                bool okY = false;  // Y 参数转换是否成功。
                double x = tokens[i].toDouble(&okX);  // 当前点的 X 参数。
                if (!okX)
                    break;
                i++;
                if (i >= tokens.size())
                    break;
                double y = tokens[i].toDouble(&okY);  // 当前点的 Y 参数。
                if (!okY)
                    break;
                i++;

                QPointF pt = createPoint(x, y, relative, currentX, currentY);  // 转换后的路径点。
                points.append(pt);
            }
            previousCommand = 'L';
        }
        // 处理H/h（Horizontal LineTo）命
        else if (command == 'H') {
            bool relative = (cmd[0] == 'h');  // 当前 Horizontal LineTo 是否使用相对坐标。
            i++;
            while (i < tokens.size()) {
                bool ok = false;  // 参数转换是否成功。
                double dx = tokens[i].toDouble(&ok);  // 当前水平位移或绝对 X 坐标。
                if (!ok)
                    break;
                i++;

                if (relative) {
                    currentX += dx;
                } else {
                    currentX = dx;
                }
                points.append(QPointF(currentX, currentY));
            }
            previousCommand = 'H';
        }
        // 处理V/v（Vertical LineTo）命
        else if (command == 'V') {
            bool relative = (cmd[0] == 'v');  // 当前 Vertical LineTo 是否使用相对坐标。
            i++;
            while (i < tokens.size()) {
                bool ok = false;  // 参数转换是否成功。
                double dy = tokens[i].toDouble(&ok);  // 当前垂直位移或绝对 Y 坐标。
                if (!ok)
                    break;
                i++;

                if (relative) {
                    currentY += dy;
                } else {
                    currentY = dy;
                }
                points.append(QPointF(currentX, currentY));
            }
            previousCommand = 'V';
        }
        // 处理A/a（Arc）命
        else if (command == 'A') {
            bool relative = (cmd[0] == 'a');  // 当前 Arc 是否使用相对坐标。
            if (points.isEmpty()) {
                qWarning() << "Arc without origin point";
                i++;
                continue;
            }

            QPointF startPoint = points.last();  // 当前圆弧起点。
            i++;

            if (i + 6 >= tokens.size()) {
                qWarning() << "Arc param length error";
                continue;
            }

            // 各圆弧参数的数值转换状态。
            bool okRx = false, okRy = false, okXRot = false, okLarge = false, okSweep = false, okX = false,
                 okY = false;  // 各圆弧参数的数值转换状态。
            double rx = tokens[i].toDouble(&okRx);  // 圆弧 X 半径。
            i++;
            double ry = tokens[i].toDouble(&okRy);  // 圆弧 Y 半径。
            i++;
            double xRotation = tokens[i].toDouble(&okXRot);  // 椭圆旋转角度。
            i++;
            int largeArcFlag = tokens[i].toInt(&okLarge);  // 大弧选择标志。
            i++;
            int sweepFlag = tokens[i].toInt(&okSweep);  // 扫掠方向标志。
            i++;
            double endX = tokens[i].toDouble(&okX);  // 圆弧终点 X 坐标。
            i++;
            double endY = tokens[i].toDouble(&okY);  // 圆弧终点 Y 坐标。
            i++;

            if (!okRx || !okRy || !okXRot || !okLarge || !okSweep || !okX || !okY) {
                qWarning() << "Arc param parse error";
                continue;
            }

            QPointF endPoint(endX, endY);  // 圆弧终点。
            if (relative) {
                endPoint = startPoint + QPointF(endX, endY);
            }

            QList<QPointF> arcPoints =  // 圆弧离散后的路径点。
                parseArc(startPoint, rx, ry, xRotation, largeArcFlag != 0, sweepFlag != 0, endPoint);
            for (const QPointF& point : arcPoints) {
                if (points.isEmpty() || point != points.last())
                    points.append(point);
            }
            currentX = endPoint.x();
            currentY = endPoint.y();
            previousCommand = 'A';
        }
        // 处理C/c（Bezier Curve）命
        else if (command == 'C') {
            bool relative = (cmd[0] == 'c');  // 当前 Cubic Bezier 是否使用相对坐标。
            if (points.isEmpty()) {
                qWarning() << "Bezier without origin point";
                i += 7;
                continue;
            }

            QPointF startPoint = points.last();  // 当前贝塞尔曲线起点。
            i++;

            if (i + 5 >= tokens.size()) {
                qWarning() << "Bezier param length error";
                continue;
            }

            // 各三次贝塞尔参数的数值转换状态。
            bool okCp1x = false, okCp1y = false, okCp2x = false, okCp2y = false, okX = false,
                 okY = false;  // 各三次贝塞尔参数的数值转换状态。
            double cp1x = tokens[i].toDouble(&okCp1x);  // 第一个控制点 X 坐标。
            i++;
            double cp1y = tokens[i].toDouble(&okCp1y);  // 第一个控制点 Y 坐标。
            i++;
            double cp2x = tokens[i].toDouble(&okCp2x);  // 第二个控制点 X 坐标。
            i++;
            double cp2y = tokens[i].toDouble(&okCp2y);  // 第二个控制点 Y 坐标。
            i++;
            double endX = tokens[i].toDouble(&okX);  // 贝塞尔终点 X 坐标。
            i++;
            double endY = tokens[i].toDouble(&okY);  // 贝塞尔终点 Y 坐标。
            i++;

            if (!okCp1x || !okCp1y || !okCp2x || !okCp2y || !okX || !okY) {
                qWarning() << "Bezier param parse error";
                continue;
            }

            if (relative) {
                cp1x += startPoint.x();
                cp1y += startPoint.y();
                cp2x += startPoint.x();
                cp2y += startPoint.y();
                endX += startPoint.x();
                endY += startPoint.y();
            }

            QList<QPointF> bezierPoints =  // 贝塞尔曲线离散后的路径点。
                bezierToPolyline(startPoint.x(), startPoint.y(), cp1x, cp1y, cp2x, cp2y, endX, endY);
            for (const QPointF& point : bezierPoints) {
                if (points.isEmpty() || point != points.last())
                    points.append(point);
            }
            currentX = endX;
            currentY = endY;
            lastCubicControl = QPointF(cp2x, cp2y);
            previousCommand = 'C';
        }
        // 处理S/s（平滑三次贝塞尔曲线）命令
        else if (command == 'S') {
            const bool relative = (cmd[0] == 's');  // 当前平滑三次贝塞尔是否使用相对坐标。
            if (points.isEmpty() || i + 4 >= tokens.size()) {
                qWarning() << "Smooth cubic bezier param length error";
                i++;
                continue;
            }
            const QPointF startPoint = points.last();  // 当前平滑曲线起点。
            const QPointF cp1 =  // 根据前一条曲线计算的反射控制点。
                (previousCommand == 'C' || previousCommand == 'S')
                    ? QPointF(2.0 * startPoint.x() - lastCubicControl.x(), 2.0 * startPoint.y() - lastCubicControl.y())
                    : startPoint;
            bool okCp2x = false, okCp2y = false, okX = false, okY = false;  // 第二控制点和终点坐标的数值转换状态。
            i++;
            double cp2x = tokens[i++].toDouble(&okCp2x);  // 第二控制点 X 坐标。
            double cp2y = tokens[i++].toDouble(&okCp2y);  // 第二控制点 Y 坐标。
            double endX = tokens[i++].toDouble(&okX);  // 曲线终点 X 坐标。
            double endY = tokens[i++].toDouble(&okY);  // 曲线终点 Y 坐标。
            if (!okCp2x || !okCp2y || !okX || !okY)
                continue;
            if (relative) {
                cp2x += startPoint.x();
                cp2y += startPoint.y();
                endX += startPoint.x();
                endY += startPoint.y();
            }
            const QList<QPointF> bezierPoints =
                bezierToPolyline(startPoint.x(), startPoint.y(), cp1.x(), cp1.y(), cp2x, cp2y, endX, endY);
            for (const QPointF& point : bezierPoints) {
                if (point != points.last())
                    points.append(point);
            }
            currentX = endX;
            currentY = endY;
            lastCubicControl = QPointF(cp2x, cp2y);
            previousCommand = 'S';
        }
        // 处理Q/q（二次贝塞尔曲线）命令
        else if (command == 'Q') {
            const bool relative = (cmd[0] == 'q');  // 当前二次贝塞尔是否使用相对坐标。
            if (points.isEmpty() || i + 4 >= tokens.size()) {
                qWarning() << "Quadratic bezier param length error";
                i++;
                continue;
            }

            const QPointF startPoint = points.last();  // 当前二次曲线起点。
            bool okCpX = false, okCpY = false, okEndX = false, okEndY = false;  // 控制点和终点坐标的数值转换状态。
            i++;
            double cpX = tokens[i].toDouble(&okCpX);  // 控制点 X 坐标。
            i++;
            double cpY = tokens[i].toDouble(&okCpY);  // 控制点 Y 坐标。
            i++;
            double endX = tokens[i].toDouble(&okEndX);  // 曲线终点 X 坐标。
            i++;
            double endY = tokens[i++].toDouble(&okEndY);  // 曲线终点 Y 坐标。
            if (!okCpX || !okCpY || !okEndX || !okEndY) {
                qWarning() << "Quadratic bezier param parse error";
                continue;
            }
            if (relative) {
                cpX += startPoint.x();
                cpY += startPoint.y();
                endX += startPoint.x();
                endY += startPoint.y();
            }
            const QPointF cp2(endX + 2.0 * (cpX - endX) / 3.0,
                              endY + 2.0 * (cpY - endY) / 3.0);  // 二次曲线转换后的第二控制点。
            const QPointF cp1(startPoint.x() + 2.0 * (cpX - startPoint.x()) / 3.0,
                              startPoint.y() + 2.0 * (cpY - startPoint.y()) / 3.0);
            const QList<QPointF> bezierPoints =  // 转换后的三次贝塞尔离散点。
                bezierToPolyline(startPoint.x(), startPoint.y(), cp1.x(), cp1.y(), cp2.x(), cp2.y(), endX, endY);
            for (const QPointF& point : bezierPoints) {
                if (points.isEmpty() || point != points.last())
                    points.append(point);
            }
            currentX = endX;
            currentY = endY;
            lastQuadraticControl = QPointF(cpX, cpY);
            previousCommand = 'Q';
        }
        // 处理T/t（平滑二次贝塞尔曲线）命令
        else if (command == 'T') {
            const bool relative = (cmd[0] == 't');  // 当前平滑二次贝塞尔是否使用相对坐标。
            if (points.isEmpty() || i + 2 >= tokens.size()) {
                qWarning() << "Smooth quadratic bezier param length error";
                i++;
                continue;
            }
            const QPointF startPoint = points.last();  // 当前平滑二次曲线起点。
            const QPointF control =  // 根据前一条曲线计算的反射控制点。
                (previousCommand == 'Q' || previousCommand == 'T')
                    ? QPointF(2.0 * startPoint.x() - lastQuadraticControl.x(),
                              2.0 * startPoint.y() - lastQuadraticControl.y())
                    : startPoint;
            bool okX = false, okY = false;  // 曲线终点坐标的数值转换状态。
            i++;
            double endX = tokens[i++].toDouble(&okX);  // 曲线终点 X 坐标。
            double endY = tokens[i++].toDouble(&okY);  // 曲线终点 Y 坐标。
            if (!okX || !okY)
                continue;
            if (relative) {
                endX += startPoint.x();
                endY += startPoint.y();
            }
            const QPointF cp1(startPoint.x() + 2.0 * (control.x() - startPoint.x()) / 3.0,
                              startPoint.y() + 2.0 * (control.y() - startPoint.y()) / 3.0);
            const QPointF cp2(endX + 2.0 * (control.x() - endX) / 3.0,
                              endY + 2.0 * (control.y() - endY) / 3.0);  // 第二控制点。
            const QList<QPointF> bezierPoints =  // 平滑二次曲线转换后的离散点。
                bezierToPolyline(startPoint.x(), startPoint.y(), cp1.x(), cp1.y(), cp2.x(), cp2.y(), endX, endY);
            for (const QPointF& point : bezierPoints) {
                if (point != points.last())
                    points.append(point);
            }
            currentX = endX;
            currentY = endY;
            lastQuadraticControl = control;
            previousCommand = 'T';
        }
        // 处理Z/z（ClosePath）命
        else if (command == 'Z') {
            if (!points.isEmpty()) {
                points.append(points.first());
            }
            i++;
            previousCommand = 'Z';
        }
        // 未知命令
        else {
            qWarning() << "SVG: Unknown cmd:" << command;
            i++;
        }
    }

    for (const QPointF& point : points) {
        if (!isFinitePoint(point))
            return {};
    }
    return points;
}

/**
 * @brief 解析 SVG 路径并保留结构化线段信息。
 * @param path SVG 路径文本。
 * @return 直线、贝塞尔曲线和圆弧组成的路径线段列表。
 */
QList<SvgPathSegment> SvgPathParser::parseSegments(const QString& path) {
    QList<SvgPathSegment> segments;  // 解析得到的结构化路径线段。
    if (path.isEmpty())
        return segments;

    const QStringList tokens = splitPath(path);  // 分词后的 SVG 命令和参数。
    QPointF current;  // 当前路径位置。
    QPointF subpathStart;  // 当前子路径起点。
    QPointF lastCubicControl;  // 上一个三次贝塞尔控制点。
    QPointF lastQuadraticControl;  // 上一个二次贝塞尔控制点。
    QChar previousCommand;  // 上一个路径命令。
    bool hasCurrent = false;  // 是否已有当前路径位置。
    bool hasSubpath = false;  // 是否已建立子路径。

    auto isNumber = [&](int index) {
        if (index >= tokens.size())
            return false;
        bool ok = false;  // 数值转换是否成功。
        const double value = tokens.at(index).toDouble(&ok);  // 待检查的参数值。
        return ok && std::isfinite(value);
    };
    auto readNumbers = [&](int& index, int count, QList<double>& values) {
        if (index + count > tokens.size())
            return false;
        values.clear();
        values.reserve(count);
        for (int n = 0; n < count; ++n) {
            bool ok = false;  // 当前参数转换是否成功。
            const double value = tokens.at(index++).toDouble(&ok);  // 当前参数数值。
            if (!ok || !std::isfinite(value))
                return false;
            values.append(value);
        }
        return true;
    };
    auto addLine = [&](const QPointF& end) {
        if (!hasCurrent) {
            current = end;
            return;
        }
        if (current == end)
            return;
        SvgPathSegment segment;  // 当前新增的直线路径线段。
        segment.type = SvgPathSegment::Type::Line;
        segment.start = current;
        segment.end = end;
        segments.append(segment);
        current = end;
    };
    auto addCubic = [&](const QPointF& control1, const QPointF& control2, const QPointF& end) {
        if (!hasCurrent)
            return;
        SvgPathSegment segment;  // 当前新增的三次贝塞尔线段。
        segment.type = SvgPathSegment::Type::CubicBezier;
        segment.start = current;
        segment.control1 = control1;
        segment.control2 = control2;
        segment.end = end;
        segments.append(segment);
        current = end;
    };
    auto addQuadratic = [&](const QPointF& control, const QPointF& end) {
        if (!hasCurrent)
            return;
        SvgPathSegment segment;  // 当前新增的二次贝塞尔线段。
        segment.type = SvgPathSegment::Type::QuadraticBezier;
        segment.start = current;
        segment.control1 = control;
        segment.end = end;
        segments.append(segment);
        current = end;
    };

    int index = 0;  // 当前命令或参数索引。
    while (index < tokens.size()) {
        const QString commandToken = tokens.at(index++);  // 当前 SVG 命令词。
        if (commandToken.isEmpty() || commandToken.at(0).isDigit() || commandToken.at(0) == '.' ||
            commandToken.at(0) == '+' || commandToken.at(0) == '-') {
            continue;
        }
        const QChar command = commandToken.at(0).toUpper();  // 规范化后的命令字符。
        const bool relative = commandToken.at(0).isLower();  // 当前命令是否使用相对坐标。

        if (command == 'M') {
            bool first = true;  // 当前 MoveTo 参数组是否为子路径首点。
            while (isNumber(index) && isNumber(index + 1)) {
                QList<double> values;  // 当前命令参数组。
                if (!readNumbers(index, 2, values))
                    break;
                QPointF point(values[0], values[1]);  // 当前 MoveTo 目标点。
                if (relative && hasCurrent)
                    point += current;
                if (first) {
                    current = point;
                    subpathStart = point;
                    hasCurrent = true;
                    hasSubpath = true;
                    first = false;
                } else {
                    addLine(point);
                }
            }
            previousCommand = 'M';
            continue;
        }

        if (command == 'Z') {
            if (hasCurrent && hasSubpath)
                addLine(subpathStart);
            previousCommand = 'Z';
            continue;
        }

        if (command == 'L') {
            while (isNumber(index) && isNumber(index + 1)) {
                QList<double> values;  // 当前 LineTo 参数组。
                if (!readNumbers(index, 2, values))
                    break;
                QPointF point(values[0], values[1]);  // 当前 LineTo 目标点。
                if (relative)
                    point += current;
                addLine(point);
            }
            previousCommand = 'L';
            continue;
        }

        if (command == 'H' || command == 'V') {
            while (isNumber(index)) {
                QList<double> values;  // 当前水平或垂直线参数组。
                if (!readNumbers(index, 1, values))
                    break;
                QPointF point = current;  // 当前水平或垂直线目标点。
                if (command == 'H')
                    point.setX(relative ? current.x() + values[0] : values[0]);
                else
                    point.setY(relative ? current.y() + values[0] : values[0]);
                addLine(point);
            }
            previousCommand = command;
            continue;
        }

        const int groupSize =  // 当前命令的一组参数数量。
            command == 'A'                       ? 7
            : command == 'C'                     ? 6
            : (command == 'S' || command == 'Q') ? 4
            : command == 'T'                     ? 2
                                                 : 0;
        if (groupSize == 0) {
            qWarning() << "SVG: Unknown segment command:" << command;
            continue;
        }

        while (isNumber(index)) {
            QList<double> values;  // 当前曲线或圆弧参数组。
            if (!readNumbers(index, groupSize, values)) {
                qWarning() << "SVG: Invalid segment parameters for command:" << command;
                break;
            }
            if (!hasCurrent) {
                qWarning() << "SVG: Segment command without origin point:" << command;
                continue;
            }
            const QPointF start = current;  // 当前曲线或圆弧起点。
            if (command == 'A') {
                QPointF end(values[5], values[6]);  // 圆弧终点。
                if (relative)
                    end += start;
                const QList<QPointF> arcPoints =  // 圆弧离散点或近似折线点。
                    parseArc(start, values[0], values[1], values[2], values[3] != 0, values[4] != 0, end);
                const bool isCircular = values[0] > 0.0 && qFuzzyCompare(values[0], values[1]);  // 是否为圆形圆弧。
                if (isCircular && arcPoints.size() >= 3 && start != end) {
                    SvgPathSegment segment;  // 当前圆形圆弧线段。
                    segment.type = SvgPathSegment::Type::CircularArc;
                    segment.start = start;
                    segment.arcMid = arcPoints.at(arcPoints.size() / 2);
                    segment.end = end;
                    segments.append(segment);
                    current = end;
                } else if (!isCircular && values[0] > 0.0 && values[1] > 0.0) {
                    const SvgArcGeometry geometry =  // 当前椭圆弧几何参数。
                        calculateArcGeometry(
                            start, values[0], values[1], values[2], values[3] != 0, values[4] != 0, end);
                    if (geometry.valid) {
                        if (qFuzzyIsNull(std::sin(values[2] * PI / 180.0))) {
                            SvgPathSegment segment;  // 当前椭圆弧线段。
                            segment.type = SvgPathSegment::Type::EllipticalArc;
                            segment.start = start;
                            segment.arcCenter = geometry.center;
                            segment.radiusX = geometry.radiusX;
                            segment.radiusY = geometry.radiusY;
                            segment.arcStartAngle = geometry.startAngle * 180.0 / PI;
                            segment.arcEndAngle = (geometry.startAngle + geometry.deltaAngle) * 180.0 / PI;
                            segment.end = end;
                            segments.append(segment);
                        } else {
                            segments.append(approximateRotatedEllipse(geometry, values[2]));
                        }
                        current = end;
                    } else {
                        for (const QPointF& point : arcPoints)
                            addLine(point);
                    }
                } else {
                    for (const QPointF& point : arcPoints)
                        addLine(point);
                }
            } else if (command == 'C' || command == 'S' || command == 'Q' || command == 'T') {
                QPointF control1;  // 三次贝塞尔第一个控制点。
                QPointF control2;  // 三次贝塞尔第二个控制点。
                QPointF end;  // 当前曲线终点。
                if (command == 'C') {
                    control1 = QPointF(values[0], values[1]);
                    control2 = QPointF(values[2], values[3]);
                    end = QPointF(values[4], values[5]);
                    if (relative) {
                        control1 += start;
                        control2 += start;
                        end += start;
                    }
                } else if (command == 'S') {
                    control1 =
                        (previousCommand == 'C' || previousCommand == 'S')
                            ? QPointF(2.0 * start.x() - lastCubicControl.x(), 2.0 * start.y() - lastCubicControl.y())
                            : start;
                    control2 = QPointF(values[0], values[1]);
                    end = QPointF(values[2], values[3]);
                    if (relative) {
                        control2 += start;
                        end += start;
                    }
                } else if (command == 'Q' || command == 'T') {
                    QPointF quadraticControl;  // 二次贝塞尔控制点。
                    if (command == 'Q') {
                        quadraticControl = QPointF(values[0], values[1]);
                        end = QPointF(values[2], values[3]);
                        if (relative) {
                            quadraticControl += start;
                            end += start;
                        }
                    } else {
                        quadraticControl = (previousCommand == 'Q' || previousCommand == 'T')
                                               ? QPointF(2.0 * start.x() - lastQuadraticControl.x(),
                                                         2.0 * start.y() - lastQuadraticControl.y())
                                               : start;
                        end = QPointF(values[0], values[1]);
                        if (relative)
                            end += start;
                    }
                    addQuadratic(quadraticControl, end);
                    lastQuadraticControl = quadraticControl;
                }
                if (command == 'C' || command == 'S') {
                    addCubic(control1, control2, end);
                }
                if (command == 'C' || command == 'S')
                    lastCubicControl = control2;
            }
            previousCommand = command;
        }
    }
    for (const SvgPathSegment& segment : segments) {
        if (!isFiniteSegment(segment))
            return {};
    }
    return segments;
}

/**
 * @brief 规范化 SVG 命令和参数分隔符并展开连续参数组。
 * @param path SVG 路径文本。
 * @return 规范化后的命令和参数词元列表。
 */
QStringList SvgPathParser::splitPath(const QString& path) {
    // 将命令字母前后添加空格，然后按空格分
    QString processed = path;  // 经过分隔符规范化的路径文本。
    processed.replace(QRegularExpression("([MmZzLlHhVvCcSsQqTtAa])"), " \\1 ");
    processed.replace(QRegularExpression("(?<=[0-9.])(?=[+-])"), " ");
    const QStringList rawTokens =  // 初步分词结果。
        processed.split(QRegularExpression("[\\s,]+"), Qt::SkipEmptyParts);

    // SVG 允许一个命令后连续跟随多组参数。当前主解析循环按一次命令消费一组
    // 曲线/圆弧参数，因此在分词阶段把后续参数组展开成同类型的重复命令。
    QStringList tokens;  // 展开重复参数组后的词元列表。
    int index = 0;  // 当前原始词元索引。
    while (index < rawTokens.size()) {
        const QString command = rawTokens.at(index++);  // 当前命令或参数词元。
        tokens.append(command);
        if (command.size() != 1 || !command.at(0).isLetter())
            continue;

        const QChar upper = command.at(0).toUpper();  // 规范化后的命令字符。
        int groupSize = 0;  // 当前命令每组参数数量。
        if (upper == 'A')
            groupSize = 7;
        else if (upper == 'C')
            groupSize = 6;
        else if (upper == 'S' || upper == 'Q')
            groupSize = 4;
        else if (upper == 'T')
            groupSize = 2;
        if (groupSize == 0)
            continue;

        int parameterCount = 0;  // 当前参数组已收集的数量。
        while (index < rawTokens.size() && !rawTokens.at(index).at(0).isLetter()) {
            if (parameterCount == groupSize) {
                tokens.append(command);
                parameterCount = 0;
            }
            tokens.append(rawTokens.at(index++));
            ++parameterCount;
        }
    }
    return tokens;
}

/**
 * @brief 根据绝对或相对坐标更新当前路径位置。
 * @param x X 坐标或相对 X 位移。
 * @param y Y 坐标或相对 Y 位移。
 * @param relative 是否使用相对坐标。
 * @param currentX 当前路径 X 坐标。
 * @param currentY 当前路径 Y 坐标。
 * @return 更新后的路径点。
 */
QPointF SvgPathParser::createPoint(double x, double y, bool relative, double& currentX, double& currentY) {
    if (relative) {
        currentX += x;
        currentY += y;
    } else {
        currentX = x;
        currentY = y;
    }
    return QPointF(currentX, currentY);
}

/**
 * @brief 按 SVG 椭圆弧参数生成离散路径点。
 * @param startPoint 圆弧起点。
 * @param rx X 方向半径。
 * @param ry Y 方向半径。
 * @param xRotation 椭圆旋转角度，单位为度。
 * @param largeArcFlag 大弧选择标志。
 * @param sweepFlag 扫掠方向标志。
 * @param endPoint 圆弧终点。
 * @return 圆弧离散后的路径点列表。
 */
QList<QPointF> SvgPathParser::parseArc(const QPointF& startPoint,
                                       double rx,
                                       double ry,
                                       double xRotation,
                                       bool largeArcFlag,
                                       bool sweepFlag,
                                       const QPointF& endPoint) {
    QList<QPointF> points;  // 圆弧离散后的路径点。

    // SVG 规范规定起点和终点相同时不绘制弧线，避免后续圆心计算出现 0/0。
    if (startPoint == endPoint) {
        points.append(startPoint);
        return points;
    }

    // 如果半径，直接返回起点和终点
    if (rx <= 0 || ry <= 0) {
        points.append(startPoint);
        points.append(endPoint);
        return points;
    }

    // 将角度转换为弧度
    double phi = xRotation * PI / 180.0;  // 椭圆旋转角度，单位为弧度。

    // 计算中点
    double dx = (startPoint.x() - endPoint.x()) / 2.0;  // 端点中点 X 偏移。
    double dy = (startPoint.y() - endPoint.y()) / 2.0;  // 端点中点 Y 偏移。

    // 旋转坐标
    double x1 = cos(phi) * dx + sin(phi) * dy;  // 旋转坐标系中的起点 X 坐标。
    double y1 = -sin(phi) * dx + cos(phi) * dy;  // 旋转坐标系中的起点 Y 坐标。

    // 计算临时变量
    double rxSq = rx * rx;  // X 半径平方。
    double rySq = ry * ry;  // Y 半径平方。
    double x1Sq = x1 * x1;  // 旋转坐标 X 平方。
    double y1Sq = y1 * y1;  // 旋转坐标 Y 平方。

    // 计算校正因子
    double temp =  // 圆心计算使用的半径修正因子。
        (rxSq * rySq - rxSq * y1Sq - rySq * x1Sq) / (rxSq * y1Sq + rySq * x1Sq);
    if (temp < 0)
        temp = 0;
    temp = sqrt(temp);

    // 根据largeArcFlag和sweepFlag确定符号
    double factor = (largeArcFlag == sweepFlag) ? -1.0 : 1.0;  // 大弧和扫掠方向对应的圆心符号。
    double cx1 = factor * temp * rx * y1 / ry;  // 旋转坐标系中的圆心 X 坐标。
    double cy1 = -factor * temp * ry * x1 / rx;  // 旋转坐标系中的圆心 Y 坐标。

    // 计算圆心
    double cx = cos(phi) * cx1 - sin(phi) * cy1 + (startPoint.x() + endPoint.x()) / 2.0;  // 圆心 X 坐标。
    double cy = sin(phi) * cx1 + cos(phi) * cy1 + (startPoint.y() + endPoint.y()) / 2.0;  // 圆心 Y 坐标。

    // 计算起始角度和角度增
    double startAngle = getAngle(1.0, 0.0, (x1 - cx1) / rx, (y1 - cy1) / ry);  // 起始角度，单位为弧度。
    double deltaAngle =  // 扫掠角度，单位为弧度。
        getAngle((x1 - cx1) / rx, (y1 - cy1) / ry, (-x1 - cx1) / rx, (-y1 - cy1) / ry);

    // 规范化角
    while (startAngle < 0)
        startAngle += 2 * PI;
    while (startAngle >= 2 * PI)
        startAngle -= 2 * PI;

    // 根据sweepFlag调整角度增量
    if (sweepFlag) {
        if (deltaAngle < 0)
            deltaAngle += 2 * PI;
    } else {
        if (deltaAngle > 0)
            deltaAngle -= 2 * PI;
    }

    // 将弧度转换为角度
    double startAngleDeg = startAngle * 180.0 / PI;  // 起始角度，单位为度。
    double deltaAngleDeg = deltaAngle * 180.0 / PI;  // 扫掠角度，单位为度。

    // 计算圆弧上的
    points = calcArcPoints(cx, cy, rx, ry, startAngleDeg, deltaAngleDeg, xRotation);

    return points;
}

/**
 * @brief 按椭圆参数离散生成圆弧点。
 * @param cx 圆心 X 坐标。
 * @param cy 圆心 Y 坐标。
 * @param rx X 方向半径。
 * @param ry Y 方向半径。
 * @param startAngle 起始角度，单位为度。
 * @param deltaAngle 扫掠角度，单位为度。
 * @param xRotation 椭圆旋转角度，单位为度。
 * @return 离散后的圆弧点列表。
 */
QList<QPointF> SvgPathParser::calcArcPoints(double cx,
                                            double cy,
                                            double rx,
                                            double ry,
                                            double startAngle,
                                            double deltaAngle,
                                            double xRotation) {
    QList<QPointF> points;  // 圆弧离散后的点列表。
    const int splitCount = 32;  // 分割32段
    double step = deltaAngle / splitCount;  // 每个离散区间的角度步长。

    double phi = xRotation * PI / 180.0;  // 椭圆旋转角度，单位为弧度。

    for (int i = 0; i <= splitCount; i++) {
        double theta = (startAngle + i * step) * PI / 180.0;  // 当前离散点角度，单位为弧度。
        double cosTheta = cos(theta);  // 当前角度余弦。
        double sinTheta = sin(theta);  // 当前角度正弦。

        double x = cos(phi) * rx * cosTheta - sin(phi) * ry * sinTheta + cx;  // 当前点 X 坐标。
        double y = sin(phi) * rx * cosTheta + cos(phi) * ry * sinTheta + cy;  // 当前点 Y 坐标。

        points.append(QPointF(x, y));
    }

    return points;
}

/**
 * @brief 根据两个向量的点积和叉积计算带方向的夹角。
 * @param x1 第一个向量 X 分量。
 * @param y1 第一个向量 Y 分量。
 * @param x2 第二个向量 X 分量。
 * @param y2 第二个向量 Y 分量。
 * @return 两个向量之间的有向角度，单位为弧度。
 */
double SvgPathParser::getAngle(double x1, double y1, double x2, double y2) {
    // 计算向量点积和叉
    double dot = x1 * x2 + y1 * y2;  // 两个向量的点积。
    double cross = x1 * y2 - y1 * x2;  // 两个向量的二维叉积。

    // 计算角度
    double angle = atan2(cross, dot);  // 根据点积和叉积得到的有向角度。
    return angle;
}

/**
 * @brief 将三次贝塞尔曲线离散为折线点。
 * @param startX 曲线起点 X 坐标。
 * @param startY 曲线起点 Y 坐标。
 * @param cp1X 第一个控制点 X 坐标。
 * @param cp1Y 第一个控制点 Y 坐标。
 * @param cp2X 第二个控制点 X 坐标。
 * @param cp2Y 第二个控制点 Y 坐标。
 * @param endX 曲线终点 X 坐标。
 * @param endY 曲线终点 Y 坐标。
 * @param segments 离散线段数量。
 * @return 离散后的折线点列表。
 */
QList<QPointF> SvgPathParser::bezierToPolyline(double startX,
                                               double startY,
                                               double cp1X,
                                               double cp1Y,
                                               double cp2X,
                                               double cp2Y,
                                               double endX,
                                               double endY,
                                               int segments) {
    QList<QPointF> res;  // 三次贝塞尔曲线离散后的点列表。

    // 三次贝塞尔曲线公式
    // B(t) = (1-t)^3 * P0 + 3*(1-t)^2*t * P1 + 3*(1-t)*t^2 * P2 + t^3 * P3

    for (int i = 0; i <= segments; ++i) {
        double t = static_cast<double>(i) / segments;  // 当前曲线参数。
        double t2 = t * t;  // 曲线参数平方。
        double t3 = t2 * t;  // 曲线参数立方。
        double mt = 1.0 - t;  // 曲线参数补值。
        double mt2 = mt * mt;  // 曲线参数补值平方。
        double mt3 = mt2 * mt;  // 曲线参数补值立方。

        double x = mt3 * startX + 3.0 * mt2 * t * cp1X + 3.0 * mt * t2 * cp2X + t3 * endX;  // 当前点 X 坐标。
        double y = mt3 * startY + 3.0 * mt2 * t * cp1Y + 3.0 * mt * t2 * cp2Y + t3 * endY;  // 当前点 Y 坐标。

        res.append(QPointF(x, y));
    }

    return res;
}

}  // namespace EasyKiConverter
