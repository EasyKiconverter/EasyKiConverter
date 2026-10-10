#ifndef SVGPATHPARSER_H
#define SVGPATHPARSER_H

#include <QList>
#include <QPointF>
#include <QString>

namespace EasyKiConverter {

/**
 * @brief SVG 路径中的可导出几何段
 * @details 二次 Bézier、圆形弧和未旋转椭圆弧保留为独立段；旋转椭圆弧
 *          使用分段三次 Bézier 近似，以保留曲线形状并兼容目标格式。
 */
struct SvgPathSegment {
    enum class Type { Line, QuadraticBezier, CubicBezier, CircularArc, EllipticalArc };

    Type type = Type::Line;  ///< 几何段类型。
    QPointF start;  ///< 几何段起点。
    QPointF control1;  ///< Bézier 第一控制点。
    QPointF control2;  ///< 三次 Bézier 第二控制点。
    QPointF arcMid;  ///< 圆弧中间点。
    QPointF arcCenter;  ///< 圆弧或椭圆弧中心。
    double radiusX = 0.0;  ///< 椭圆弧 X 方向半径。
    double radiusY = 0.0;  ///< 椭圆弧 Y 方向半径。
    double arcStartAngle = 0.0;  ///< 弧段起始角度（弧度）。
    double arcEndAngle = 0.0;  ///< 弧段结束角度（弧度）。
    QPointF end;  ///< 几何段终点。
};

/**
 * @brief SVG 路径解析。
 *
 * 用于解析 SVG 路径字符串并转换为点列表。
 */
class SvgPathParser {
public:
    /**
     * @brief 解析SVG路径字符
         *
     * @param path SVG路径字符
         * @return QList<QPointF> 解析后的点列
         */
    static QList<QPointF> parsePath(const QString& path);

    /**
     * @brief 解析 SVG 路径为直线、二次/三次 Bézier 和圆弧段
     * @param path SVG 路径字符串
     * @return 按源路径顺序排列的几何段
     */
    static QList<SvgPathSegment> parseSegments(const QString& path);

private:
    /**
     * @brief 分割SVG路径字符
         *
     * @param path 路径字符
         * @return QStringList 分割后的标记列表
     */
    static QStringList splitPath(const QString& path);

    /**
     * @brief 创建SVG路径
         *
     * @param x X坐标
     * @param y Y坐标
     * @param relative 是否为相对坐
         * @param currentX 当前X坐标
     * @param currentY 当前Y坐标
     * @return QPointF 创建的点
     */
    static QPointF createPoint(double x, double y, bool relative, double& currentX, double& currentY);

    /**
     * @brief 解析圆弧命令
     *
     * @param startPoint 起始
         * @param rx X轴半
         * @param ry Y轴半
     * @param xRotation X 轴旋转角。
         * @param largeArcFlag 大弧标志
     * @param sweepFlag 扫描标志
     * @param endPoint 终点
     * @return QList<QPointF> 圆弧上的点列
         */
    static QList<QPointF> parseArc(const QPointF& startPoint,
                                   double rx,
                                   double ry,
                                   double xRotation,
                                   bool largeArcFlag,
                                   bool sweepFlag,
                                   const QPointF& endPoint);

    /**
     * @brief 计算圆弧上的
         *
     * @param cx 圆心X坐标
     * @param cy 圆心Y坐标
     * @param rx X轴半
         * @param ry Y轴半
         * @param startAngle 起始角度
     * @param deltaAngle 角度增量
     * @param xRotation X轴旋转角
         * @return QList<QPointF> 圆弧上的点列
         */
    static QList<QPointF>
    calcArcPoints(double cx, double cy, double rx, double ry, double startAngle, double deltaAngle, double xRotation);

    /**
     * @brief 计算角度
     *
     * @param x1 第一个点的X坐标
     * @param y1 第一个点的Y坐标
     * @param x2 第二个点的X坐标
     * @param y2 第二个点的Y坐标
     * @return double 角度（弧度）
     */
    static double getAngle(double x1, double y1, double x2, double y2);

    static QList<QPointF> bezierToPolyline(
        double startX,
        double startY,
        double cp1X,
        double cp1Y,
        double cp2X,
        double cp2Y,
        double endX,
        double endY,
        int segments = 16);  // 16 segments provides good smoothness for most use cases
};

}  // namespace EasyKiConverter

#endif  // SVGPATHPARSER_H
