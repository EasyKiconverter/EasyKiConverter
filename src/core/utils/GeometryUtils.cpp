#include "GeometryUtils.h"

#include "SvgPathParser.h"

#include <QDebug>
#include <QRegularExpression>

#include <cmath>
#include <limits>

#ifndef M_PI
#    define M_PI 3.14159265358979323846
#endif

namespace EasyKiConverter {

/**
 * @brief 计算圆弧中点坐标。
 * @param centerX 圆心 X 坐标。
 * @param centerY 圆心 Y 坐标。
 * @param radius 圆弧半径。
 * @param angleStart 起始角度（弧度）。
 * @param angleEnd 结束角度（弧度）。
 * @return 圆弧中点坐标。
 */
QPointF GeometryUtils::getMiddleArcPos(double centerX,
                                       double centerY,
                                       double radius,
                                       double angleStart,
                                       double angleEnd) {
    double middleX = centerX + radius * std::cos((angleStart + angleEnd) / 2.0);  // 中点 X 坐标。
    double middleY = centerY + radius * std::sin((angleStart + angleEnd) / 2.0);  // 中点 Y 坐标。
    return QPointF(middleX, middleY);
}

/**
 * @brief 根据端点、半径和旋转方向计算圆弧圆心。
 * @param startX 起点 X 坐标。
 * @param startY 起点 Y 坐标。
 * @param endX 终点 X 坐标。
 * @param endY 终点 Y 坐标。
 * @param rotationDirection 旋转方向符号。
 * @param radius 圆弧半径。
 * @return 计算得到的圆心；参数无效时返回退化中心。
 */
QPointF GeometryUtils::getArcCenter(double startX,
                                    double startY,
                                    double endX,
                                    double endY,
                                    int rotationDirection,
                                    double radius) {
    double arcDistance = distance(startX, startY, endX, endY);  // 端点间距离。

    // 记录端点之间的距离，用于归一化圆弧方向向量。
    if (arcDistance < std::numeric_limits<double>::epsilon()) {
        qWarning() << "Arc distance is too small, cannot calculate center";
        return QPointF(startX, startY);
    }

    double mX = (startX + endX) / 2.0;  // 端点中点 X 坐标。
    double mY = (startY + endY) / 2.0;  // 端点中点 Y 坐标。

    double u = (endX - startX) / arcDistance;  // 端点方向的 X 单位分量。
    double v = (endY - startY) / arcDistance;  // 端点方向的 Y 单位分量。

    double hSquared = radius * radius - (arcDistance * arcDistance) / 4.0;  // 圆心到弦中点距离的平方。

    // 检查半径是否足以连接两个端点。
    if (hSquared < 0) {
        qWarning() << "Radius is too small for the given arc distance";
        return QPointF(mX, mY);
    }

    double h = std::sqrt(hSquared);  // 圆心到弦中点的距离。

    double centerX = mX - rotationDirection * h * v;  // 圆心 X 坐标。
    double centerY = mY + rotationDirection * h * u;  // 圆心 Y 坐标。

    return QPointF(centerX, centerY);
}

/**
 * @brief 根据终点横坐标和大圆弧标志计算圆弧终止角。
 * @param centerX 圆心 X 坐标。
 * @param endX 终点 X 坐标。
 * @param radius 圆弧半径。
 * @param flagLargeArc 是否选择大圆弧。
 * @return 圆弧终止角（弧度）。
 */
double GeometryUtils::getArcAngleEnd(double centerX, double endX, double radius, bool flagLargeArc) {
    // 计算相对于圆心的角度
    double dx = endX - centerX;  // 终点相对圆心的 X 分量。
    double angle = std::acos(dx / radius);  // 由 X 分量计算的终止角。

    // 根据大圆弧标志调整角
    if (flagLargeArc) {
        angle = 2.0 * M_PI - angle;
    }

    return angle;
}

/**
 * @brief 将 EasyEDA 像素单位换算为 mil。
 * @param dim 输入像素值。
 * @return 换算后的整数 mil 值。
 */
int GeometryUtils::pxToMil(double dim) {
    return static_cast<int>(10.0 * dim);
}

/**
 * @brief 将 EasyEDA 像素单位换算为毫米。
 * @param dim 输入像素值。
 * @return 换算后的毫米值。
 */
double GeometryUtils::pxToMm(double dim) {
    return 10.0 * dim * 0.0254;
}

/**
 * @brief 将输入尺寸换算为毫米并检查异常值。
 * @param dim 输入尺寸。
 * @return 换算后的毫米值；非有限输入返回 0。
 */
double GeometryUtils::convertToMm(double dim) {
    // 检查输入是否为 NaN。
    if (std::isnan(dim)) {
        qWarning() << "convertToMm: Input is NaN, returning 0.0";
        return 0.0;
    }

    if (std::isinf(dim)) {
        qWarning() << "convertToMm: Input is infinite, returning 0.0";
        return 0.0;
    }

    // 执行像素到毫米的单位换算。
    double result = dim * 10.0 * 0.0254;

    // 检查结果是否合理（大多数元器件尺寸不会超过1米）
    constexpr double MAX_REASONABLE_SIZE = 1000.0;  // 允许的最大尺寸（毫米）。
    if (std::abs(result) > MAX_REASONABLE_SIZE) {
        qWarning() << QString(
                          "convertToMm: Converted value (%1mm) exceeds reasonable range, "
                          "original value: %2")
                          .arg(result)
                          .arg(dim);
    }

    return result;
}

/**
 * @brief 计算两个平面点之间的欧氏距离。
 * @param x1 第一个点的 X 坐标。
 * @param y1 第一个点的 Y 坐标。
 * @param x2 第二个点的 X 坐标。
 * @param y2 第二个点的 Y 坐标。
 * @return 两点之间的欧氏距离。
 */
double GeometryUtils::distance(double x1, double y1, double x2, double y2) {
    double dx = x2 - x1;  // 两点的 X 方向差值。
    double dy = y2 - y1;  // 两点的 Y 方向差值。
    return std::sqrt(dx * dx + dy * dy);
}

/**
 * @brief 将角度值转换为弧度值。
 * @param degrees 角度值。
 * @return 对应的弧度值。
 */
double GeometryUtils::degreesToRadians(double degrees) {
    return degrees * M_PI / 180.0;
}

/**
 * @brief 将弧度值转换为角度值。
 * @param radians 弧度值。
 * @return 对应的角度值。
 */
double GeometryUtils::radiansToDegrees(double radians) {
    return radians * 180.0 / M_PI;
}

/**
 * @brief 将弧度角规范化到 [0, 2π) 范围。
 * @param angle 待规范化的弧度角。
 * @return 规范化后的弧度角。
 */
double GeometryUtils::normalizeAngle(double angle) {
    // 将角度规范化到 [0, 2π) 范围。
    while (angle < 0.0) {
        angle += 2.0 * M_PI;
    }
    while (angle >= 2.0 * M_PI) {
        angle -= 2.0 * M_PI;
    }
    return angle;
}

/**
 * @brief 根据 SVG 椭圆弧参数计算圆心和扫过角度。
 * @param startX 起点 X 坐标。
 * @param startY 起点 Y 坐标。
 * @param radiusX 椭圆 X 半径。
 * @param radiusY 椭圆 Y 半径。
 * @param angle 椭圆旋转角度（度）。
 * @param largeArcFlag 是否选择大弧。
 * @param sweepFlag 是否顺时针扫过。
 * @param endX 终点 X 坐标。
 * @param endY 终点 Y 坐标。
 * @param centerX 输出圆心 X 坐标。
 * @param centerY 输出圆心 Y 坐标。
 * @param angleExtent 输出扫过角度（度）。
 */
void GeometryUtils::computeArc(double startX,
                               double startY,
                               double radiusX,
                               double radiusY,
                               double angle,
                               bool largeArcFlag,
                               bool sweepFlag,
                               double endX,
                               double endY,
                               double& centerX,
                               double& centerY,
                               double& angleExtent) {
    // 计算起点和终点中点坐标系中的半弦长分量。
    double dx2 = (startX - endX) / 2.0;  // 半弦向量 X 分量。
    double dy2 = (startY - endY) / 2.0;  // 半弦向量 Y 分量。

    // 将椭圆旋转角从度转换为弧度。
    angle = degreesToRadians(std::fmod(angle, 360.0));
    double cosAngle = std::cos(angle);  // 旋转角余弦。
    double sinAngle = std::sin(angle);  // 旋转角正弦。

    // 第一步：将端点变换到未旋转的椭圆坐标系。
    double x1 = cosAngle * dx2 + sinAngle * dy2;  // 局部坐标 X 分量。
    double y1 = -sinAngle * dx2 + cosAngle * dy2;  // 局部坐标 Y 分量。

    // 取半径绝对值，保证后续计算使用非负半径。
    radiusX = std::abs(radiusX);
    radiusY = std::abs(radiusY);
    double PradiusX = radiusX * radiusX;  // X 半径平方。
    double PradiusY = radiusY * radiusY;  // Y 半径平方。
    double Px1 = x1 * x1;  // 局部 X 分量平方。
    double Py1 = y1 * y1;  // 局部 Y 分量平方。

    // 检查当前半径是否足以覆盖端点。
    double radiiCheck = 0.0;
    if (PradiusX != 0.0 && PradiusY != 0.0) {
        radiiCheck = Px1 / PradiusX + Py1 / PradiusY;
    }

    if (radiiCheck > 1.0) {
        radiusX = std::sqrt(radiiCheck) * radiusX;
        radiusY = std::sqrt(radiiCheck) * radiusY;
        PradiusX = radiusX * radiusX;
        PradiusY = radiusY * radiusY;
    }

    // 第二步：计算未旋转坐标系中的圆心偏移。
    int sign = (largeArcFlag == sweepFlag) ? -1 : 1;  // 圆心选择方向符号。
    double sq = 0.0;  // 圆心系数平方。
    if (PradiusX * Py1 + PradiusY * Px1 > 0.0) {
        sq = (PradiusX * PradiusY - PradiusX * Py1 - PradiusY * Px1) / (PradiusX * Py1 + PradiusY * Px1);
    }
    sq = std::max(sq, 0.0);
    double coef = sign * std::sqrt(sq);  // 圆心系数。
    double cx1 = coef * ((radiusX * y1) / radiusY);  // 局部圆心 X 偏移。
    double cy1 = (radiusX != 0.0) ? coef * -((radiusY * x1) / radiusX) : 0.0;  // 局部圆心 Y 偏移。

    // 第三步：将局部圆心变换回原坐标系。
    double sx2 = (startX + endX) / 2.0;  // 端点中点 X 坐标。
    double sy2 = (startY + endY) / 2.0;  // 端点中点 Y 坐标。
    centerX = sx2 + (cosAngle * cx1 - sinAngle * cy1);
    centerY = sy2 + (sinAngle * cx1 + cosAngle * cy1);

    // 第四步：根据起止向量计算扫过角度。
    double ux = (radiusX != 0.0) ? (x1 - cx1) / radiusX : 0.0;  // 起点单位向量 X 分量。
    double uy = (radiusY != 0.0) ? (y1 - cy1) / radiusY : 0.0;  // 起点单位向量 Y 分量。
    double vx = (radiusX != 0.0) ? (-x1 - cx1) / radiusX : 0.0;  // 终点单位向量 X 分量。
    double vy = (radiusY != 0.0) ? (-y1 - cy1) / radiusY : 0.0;  // 终点单位向量 Y 分量。

    // 计算两向量之间的夹角。
    double n = std::sqrt((ux * ux + uy * uy) * (vx * vx + vy * vy));  // 向量模长乘积。
    double p = ux * vx + uy * vy;  // 向量点积。
    sign = ((ux * vy - uy * vx) < 0.0) ? -1 : 1;

    if (n != 0.0) {
        double ratio = p / n;  // 夹角余弦。
        if (std::abs(ratio) < 1.0) {
            angleExtent = radiansToDegrees(sign * std::acos(ratio));
        } else {
            angleExtent = 360.0 + 359.0;
        }
    } else {
        angleExtent = 360.0 + 359.0;
    }

    if (!sweepFlag && angleExtent > 0.0) {
        angleExtent -= 360.0;
    } else if (sweepFlag && angleExtent < 0.0) {
        angleExtent += 360.0;
    }

    int angleExtentSign = (angleExtent < 0.0) ? 1 : -1;  // 保留扫掠方向的符号。
    angleExtent = (std::abs(angleExtent) / 360.0) * 360.0 * angleExtentSign;
}

/**
 * @brief 解析 SVG 椭圆弧参数并计算几何结果。
 * @param param SVG 椭圆弧命令参数字符串。
 * @return 解析得到的椭圆弧结果；输入无效时返回默认结果。
 */
GeometryUtils::SvgArcResult GeometryUtils::solveSvgArc(const QString& param) {
    SvgArcResult res;  // SVG 椭圆弧解析结果。

    // 解析 SVG 弧参数字符串
    // 格式: "M x1 y1 A rx ry phi fa fs x2 y2"
    QStringList args = param.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);  // 分隔后的弧参数。

    if (args.size() < 10) {
        qWarning() << "Invalid SVG arc parameter:" << param;
        return res;
    }

    if (args[0] != "M" && args[0] != "m") {
        qWarning() << "SVG arc must start with M command:" << param;
        return res;
    }

    if (args[3] != "A" && args[3] != "a") {
        qWarning() << "Expected A command in SVG arc:" << param;
        return res;
    }

    double x1 = args[1].toDouble();  // 起点 X 坐标。
    double y1 = args[2].toDouble();  // 起点 Y 坐标。
    double rx = args[4].toDouble();  // 椭圆 X 半径。
    double ry = args[5].toDouble();  // 椭圆 Y 半径。
    double phiAngle = args[6].toDouble();  // 椭圆旋转角度。
    int fa = args[7].toInt();  // 大弧标志。
    int fs = args[8].toInt();  // 扫掠方向标志。
    double x2 = args[9].toDouble();  // 终点 X 坐标。
    double y2 = (args.size() > 10) ? args[10].toDouble() : 0.0;  // 终点 Y 坐标。

    res.rx = rx;
    res.ry = ry;
    res.startPt.x = x1;
    res.startPt.y = y1;
    res.endPt.x = x2;
    res.endPt.y = y2;
    res.xRotate = phiAngle;

    // 如果半径，退化为直线
    if (rx == 0 || ry == 0) {
        res.cx = (x1 + x2) / 2.0;  // 退化弧的中心 X 坐标。
        res.cy = (y1 + y2) / 2.0;  // 退化弧的中心 Y 坐标。
        res.deltaAngle = 360.0;  // 退化弧使用整圆范围标记。
        return res;
    }

    // 将椭圆旋转角度转换为弧度。
    double phi = degreesToRadians(phiAngle);  // 椭圆旋转角（弧度）。
    double cos_phi = std::cos(phi);  // 椭圆旋转角余弦。
    double sin_phi = std::sin(phi);  // 椭圆旋转角正弦。

    // 计算端点中点坐标系中的半弦向量。
    double dx = (x1 - x2) / 2.0;  // 半弦向量 X 分量。
    double dy = (y1 - y2) / 2.0;  // 半弦向量 Y 分量。

    // 将半弦向量变换到未旋转坐标系。
    double x1_ = cos_phi * dx + sin_phi * dy;  // 未旋转坐标系 X 分量。
    double y1_ = -sin_phi * dx + cos_phi * dy;  // 未旋转坐标系 Y 分量。

    // 按端点位置放大不足以覆盖端点的半径。
    double rx_sq = rx * rx;  // X 半径平方。
    double ry_sq = ry * ry;  // Y 半径平方。
    double x1_sq = x1_ * x1_;  // 局部 X 分量平方。
    double y1_sq = y1_ * y1_;  // 局部 Y 分量平方。

    double lambda = x1_sq / rx_sq + y1_sq / ry_sq;  // 端点覆盖比例。
    if (lambda > 1.0) {
        double sqrtLambda = std::sqrt(lambda);  // 半径放大比例。
        rx *= sqrtLambda;
        ry *= sqrtLambda;
        rx_sq = rx * rx;
        ry_sq = ry * ry;
    }

    // 根据 SVG 标志计算局部圆心。
    double temp =  // 局部圆心系数。
        std::sqrt(std::max(0.0, (rx_sq * ry_sq - rx_sq * y1_sq - ry_sq * x1_sq) / (rx_sq * y1_sq + ry_sq * x1_sq)));

    int sign = (fa == fs) ? -1 : 1;  // 圆心选择方向符号。
    double cx_ = sign * temp * rx * y1_ / ry;  // 局部圆心 X 坐标。
    double cy_ = -sign * temp * ry * x1_ / rx;  // 局部圆心 Y 坐标。

    // 将局部圆心变换回原坐标系。
    res.cx = cos_phi * cx_ - sin_phi * cy_ + (x1 + x2) / 2.0;
    res.cy = sin_phi * cx_ + cos_phi * cy_ + (y1 + y2) / 2.0;

    // 计算椭圆弧的起始角度和扫过角度。
    double angle1 = getAngle(1.0, 0.0, (x1_ - cx_) / rx, (y1_ - cy_) / ry);  // 起始角度。
    double angle2 = getAngle((x1_ - cx_) / rx, (y1_ - cy_) / ry, (-x1_ - cx_) / rx, (-y1_ - cy_) / ry);  // 扫过角度。

    res.startAngle = radiansToDegrees(angle1);
    res.deltaAngle = radiansToDegrees(angle2);

    // 将起始角度规范化到一整圆范围内。
    while (res.startAngle < 0)
        res.startAngle += 360.0;
    while (res.startAngle >= 360.0)
        res.startAngle -= 360.0;
    res.startAngle = std::abs(res.startAngle);

    // 根据扫掠方向调整扫过角度符号。
    if (fs != 0) {
        if (res.deltaAngle < 0)
            res.deltaAngle += 360.0;
    } else {
        if (res.deltaAngle > 0)
            res.deltaAngle -= 360.0;
    }

    return res;
}

/**
 * @brief 计算两个向量之间带方向的夹角。
 * @param x1 第一个向量 X 分量。
 * @param y1 第一个向量 Y 分量。
 * @param x2 第二个向量 X 分量。
 * @param y2 第二个向量 Y 分量。
 * @return 带方向的夹角（弧度）。
 */
double GeometryUtils::getAngle(double x1, double y1, double x2, double y2) {
    // angle = arccos( (U dot V) / (|U|*|V|) )
    double factor = (x1 * y2 - y1 * x2 > 0) ? 1.0 : -1.0;  // 叉积确定的方向符号。
    double dot = x1 * x2 + y1 * y2;  // 两向量点积。
    double mag1 = std::sqrt(x1 * x1 + y1 * y1);  // 第一个向量模长。
    double mag2 = std::sqrt(x2 * x2 + y2 * y2);  // 第二个向量模长。

    if (mag1 == 0 || mag2 == 0)
        return 0.0;

    double ratio = dot / (mag1 * mag2);  // 夹角余弦。
    ratio = std::clamp(ratio, -1.0, 1.0);  // 防止数值误

    return factor * std::acos(ratio);
}

/**
 * @brief 根据椭圆弧参数计算采样起点和终点。
 * @param arc 椭圆弧参数。
 * @return 椭圆弧起点和终点。
 */
GeometryUtils::SvgArcEndpoints GeometryUtils::calcSvgArc(const SvgArcResult& arc) {
    SvgArcEndpoints res;  // 椭圆弧端点结果。

    double phi = degreesToRadians(arc.xRotate);  // 椭圆旋转角（弧度）。
    double theta = -degreesToRadians(arc.startAngle);  // 起始参数角度。
    double dTheta = -degreesToRadians(arc.deltaAngle);  // 参数角度增量。

    // 计算起点
    res.x1 = std::cos(phi) * std::cos(theta) * arc.rx - std::sin(phi) * std::sin(theta) * arc.rx + arc.cx;
    res.y1 = std::sin(phi) * std::cos(theta) * arc.ry - std::cos(phi) * std::sin(theta) * arc.ry + arc.cy;

    // 计算终点
    theta += dTheta;
    res.x2 = std::cos(phi) * std::cos(theta) * arc.rx - std::sin(phi) * std::sin(theta) * arc.rx + arc.cx;
    res.y2 = std::sin(phi) * std::cos(theta) * arc.ry - std::cos(phi) * std::sin(theta) * arc.ry + arc.cy;

    return res;
}

/**
 * @brief 将椭圆弧按固定角度步长离散为路径点。
 * @param arc 椭圆弧参数。
 * @param includeStart 是否包含弧线起点。
 * @return 离散后的路径点列表。
 */
QList<GeometryUtils::SvgPoint> GeometryUtils::arcToPath(const SvgArcResult& arc, bool includeStart) {
    QList<SvgPoint> res;  // 离散后的路径点。

    const int splitCount = 32;  // 将整圆划分为 32 段以控制圆弧采样精度。
    double step = 360.0 / splitCount;  // 当前采样步长。

    double startAngle = arc.startAngle;  // 当前采样起始角度。
    double deltaAngle = arc.deltaAngle;  // 尚未采样的角度范围。

    if (deltaAngle < 0)
        step = -step;

    while (std::abs(deltaAngle) > 0.1) {
        if (std::abs(deltaAngle) < std::abs(step)) {
            step = deltaAngle;
        }

        SvgArcResult tempArc = arc;  // 当前采样段的弧参数副本。
        tempArc.startAngle = startAngle;  // 当前采样段起始角度。
        tempArc.deltaAngle = step;  // 当前采样段扫过角度。

        SvgArcEndpoints pt = calcSvgArc(tempArc);  // 当前采样段的端点。

        if (res.isEmpty() && includeStart) {
            res.append(SvgPoint(pt.x1, pt.y1));
        }
        res.append(SvgPoint(pt.x2, pt.y2));

        deltaAngle -= step;
        startAngle += step;
    }

    return res;
}

/**
 * @brief 解析 SVG 路径并转换为 GeometryUtils 的点结构。
 * @param path SVG 路径字符串。
 * @return 解析后的路径点列表。
 */
QList<GeometryUtils::SvgPoint> GeometryUtils::parseSvgPath(const QString& path) {
    QList<SvgPoint> res;  // 解析后的 SVG 点列表。

    // 委托给 SvgPathParser 进行路径解析。
    QList<QPointF> points = SvgPathParser::parsePath(path);  // 解析得到的二维点。

    // 转换 QPointF 到 SvgPoint
    for (const QPointF& pt : points) {  // 将每个通用点转换为 SVG 点。
        res.append(SvgPoint(pt.x(), pt.y()));
    }

    return res;
}

/**
 * @brief 将像素单位换算为保留两位小数且向下取整的毫米值。
 * @param px 输入像素值。
 * @return 向下取整到两位小数后的毫米值。
 */
double GeometryUtils::pxToMmFloor(double px) {
    // 使用 floor 而非 round，保持单位换算结果向下取整。
    // 1 unit = 10 mil = 0.254 mm。
    double mm = px * 0.254;  // 将像素单位转换为毫米。
    return std::floor(mm * 100.0) / 100.0;  // 保留2位小
}

/**
 * @brief 判断字符串是否只包含 ASCII 字符。
 * @param str 待检查的字符串。
 * @return 字符串仅含 ASCII 字符时返回 true。
 */
bool GeometryUtils::isASCII(const QString& str) {
    for (int i = 0; i < str.length(); ++i) {
        if (str[i].unicode() > 127) {
            return false;
        }
    }
    return true;
}

}  // namespace EasyKiConverter
