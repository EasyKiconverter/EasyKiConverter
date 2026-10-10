#include "core/ir/GeometryNormalizer.h"
#include "core/utils/SvgPathParser.h"

#include <QTest>

#include <cmath>

using namespace EasyKiConverter;

class TestSvgPathParser : public QObject {
    Q_OBJECT

private slots:

    // 验证绝对移动、水平线、垂直线、直线和闭合命令。
    /**
     * @brief 验证 parsesMoveLineHorizontalVerticalAndClose 测试场景。
     */
    void parsesMoveLineHorizontalVerticalAndClose() {
        // points：测试用几何顶点集合。
        const QList<QPointF> points = SvgPathParser::parsePath(QStringLiteral("M 0 0 H 10 V 5 L 0 5 Z"));

        QCOMPARE(points.size(), 5);
        QCOMPARE(points.at(0), QPointF(0, 0));
        QCOMPARE(points.at(1), QPointF(10, 0));
        QCOMPARE(points.at(2), QPointF(10, 5));
        QCOMPARE(points.at(3), QPointF(0, 5));
        QCOMPARE(points.at(4), QPointF(0, 0));
    }

    // 验证相对路径命令会基于当前点计算坐标。
    /**
     * @brief 验证 parsesRelativeCommands 测试场景。
     */
    void parsesRelativeCommands() {
        // points：测试用几何顶点集合。
        const QList<QPointF> points = SvgPathParser::parsePath(QStringLiteral("M 10 10 l 5 0 h 5 v 5"));

        QCOMPARE(points.size(), 4);
        QCOMPARE(points.at(0), QPointF(10, 10));
        QCOMPARE(points.at(1), QPointF(15, 10));
        QCOMPARE(points.at(2), QPointF(20, 10));
        QCOMPARE(points.at(3), QPointF(20, 15));
    }

    // 验证重复参数组会生成连续的线段、曲线和圆弧。
    /**
     * @brief 验证 parsesRepeatedParameterGroups 测试场景。
     */
    void parsesRepeatedParameterGroups() {
        // linePoints：测试用几何图元集合。
        const QList<QPointF> linePoints = SvgPathParser::parsePath(QStringLiteral("M 0 0 L 10 0 10 10 0 10 Z"));
        QCOMPARE(linePoints.size(), 5);
        QCOMPARE(linePoints.last(), QPointF(0, 0));

        // curvePoints：测试用几何图元集合。
        const QList<QPointF> curvePoints = SvgPathParser::parsePath(QStringLiteral("M 0 0 Q 10 20 20 0 30 -20 40 0"));
        QVERIFY(curvePoints.size() > 25);
        QCOMPARE(curvePoints.at(8), QPointF(10, 10));
        QCOMPARE(curvePoints.last(), QPointF(40, 0));

        const QList<QPointF> arcPoints =
            SvgPathParser::parsePath(QStringLiteral("M 0 0 A 10 10 0 0 1 10 10 10 10 0 0 1 20 0"));
        QVERIFY(arcPoints.size() > 60);
        QCOMPARE(arcPoints.last(), QPointF(20, 0));
    }

    // 验证坐标解析保留科学计数法表示的数值。
    /**
     * @brief 验证 preservesScientificNotationInCoordinates 测试场景。
     */
    void preservesScientificNotationInCoordinates() {
        // points：测试用几何顶点集合。
        const QList<QPointF> points = SvgPathParser::parsePath(QStringLiteral("M 0 0 L 1e-3 2e-3"));

        QCOMPARE(points.size(), 2);
        QVERIFY(qAbs(points.last().x() - 0.001) < 1e-12);
        QVERIFY(qAbs(points.last().y() - 0.002) < 1e-12);
    }

    // 验证平滑曲线会保留反射控制点的几何关系。
    /**
     * @brief 验证 smoothCurvesPreserveReflectedControlPoints 测试场景。
     */
    void smoothCurvesPreserveReflectedControlPoints() {
        const QList<QPointF> cubicPoints =
            SvgPathParser::parsePath(QStringLiteral("M 0 0 C 0 10 10 10 10 0 S 20 -10 20 0"));
        QCOMPARE(cubicPoints.at(24), QPointF(15, -7.5));

        // quadraticPoints：测试用几何图元集合。
        const QList<QPointF> quadraticPoints = SvgPathParser::parsePath(QStringLiteral("M 0 0 Q 10 20 20 0 T 40 0"));
        QCOMPARE(quadraticPoints.at(24), QPointF(30, -10));
    }

    // 验证三次贝塞尔曲线会生成包含终点的折线。
    /**
     * @brief 验证 cubicBezierProducesPolylineIncludingEndpoint 测试场景。
     */
    void cubicBezierProducesPolylineIncludingEndpoint() {
        // points：测试用几何顶点集合。
        const QList<QPointF> points = SvgPathParser::parsePath(QStringLiteral("M 0 0 C 0 10 10 10 10 0"));

        QVERIFY(points.size() > 3);
        QCOMPARE(points.first(), QPointF(0, 0));
        QCOMPARE(points.last(), QPointF(10, 0));
    }

    // 验证圆弧会生成中间采样点并保留终点。
    /**
     * @brief 验证 arcProducesIntermediatePointsAndEndpoint 测试场景。
     */
    void arcProducesIntermediatePointsAndEndpoint() {
        // points：测试用几何顶点集合。
        const QList<QPointF> points = SvgPathParser::parsePath(QStringLiteral("M 0 0 A 10 10 0 0 1 10 10"));

        QVERIFY(points.size() > 3);
        QCOMPARE(points.first(), QPointF(0, 0));
        QCOMPARE(points.last(), QPointF(10, 10));
    }

    // 验证圆弧起点和终点重合时不会产生非数值坐标。
    /**
     * @brief 验证 coincidentArcEndpointsDoNotProduceNaN 测试场景。
     */
    void coincidentArcEndpointsDoNotProduceNaN() {
        // points：测试用几何顶点集合。
        const QList<QPointF> points = SvgPathParser::parsePath(QStringLiteral("M 0 0 A 10 10 0 0 1 0 0"));

        QCOMPARE(points, QList<QPointF>({QPointF(0, 0)}));
        for (const QPointF& point : points) {
            QVERIFY(std::isfinite(point.x()));
            QVERIFY(std::isfinite(point.y()));
        }
    }

    // 验证归一化圆弧只会使用几何坐标而不会引入标志位。
    /**
     * @brief 验证 normalizedArcUsesOnlyGeometryCoordinates 测试场景。
     */
    void normalizedArcUsesOnlyGeometryCoordinates() {
        // points：测试用几何顶点集合。
        const QList<QPointF> points = IR::GeometryNormalizer::parseSimpleSvgPath(
            QStringLiteral("M 0 0 A 10 10 0 0 1 10 10 A 10 10 0 0 1 20 0 Z"));

        QVERIFY(points.size() > 10);
        for (const QPointF& point : points) {
            QVERIFY2(qAbs(point.x()) < 10.0, "arc parser produced an implausibly large X coordinate");
            QVERIFY2(qAbs(point.y()) < 10.0, "arc parser produced an implausibly large Y coordinate");
        }
        QCOMPARE(points.first(), points.last());
    }

    // 验证归一化二次曲线会生成正确的终点。
    /**
     * @brief 验证 normalizedQuadraticProducesEndpoint 测试场景。
     */
    void normalizedQuadraticProducesEndpoint() {
        // points：测试用几何顶点集合。
        const QList<QPointF> points = IR::GeometryNormalizer::parseSimpleSvgPath(QStringLiteral("M 0 0 Q 10 20 20 0"));

        QVERIFY(points.size() > 3);
        QCOMPARE(points.first(), QPointF(0, 0));
        QCOMPARE(points.last(), QPointF(5.08, 0));
        QVERIFY(points.at(points.size() / 2).y() > 2.0);
    }

    // 验证连续的平滑曲线会生成有效的起点和终点。
    /**
     * @brief 验证 smoothCurvesProduceValidEndpoints 测试场景。
     */
    void smoothCurvesProduceValidEndpoints() {
        const QList<QPointF> points =
            SvgPathParser::parsePath(QStringLiteral("M 0 0 C 0 10 10 10 10 0 S 20 -10 20 0 Q 30 10 40 0 T 60 0"));

        QVERIFY(points.size() > 20);
        QCOMPARE(points.first(), QPointF(0, 0));
        QCOMPARE(points.last(), QPointF(60, 0));
    }

    // 验证原生直线、三次曲线和二次曲线段会保留类型及控制点。
    /**
     * @brief 验证 preservesNativeLineAndCubicSegments 测试场景。
     */
    void preservesNativeLineAndCubicSegments() {
        const QList<SvgPathSegment> segments =
            SvgPathParser::parseSegments(QStringLiteral("M 0 0 C 0 10 10 10 10 0 L 20 0 Q 25 10 30 0 Z"));

        QCOMPARE(segments.size(), 4);
        QCOMPARE(segments.at(0).type, SvgPathSegment::Type::CubicBezier);
        QCOMPARE(segments.at(0).start, QPointF(0, 0));
        QCOMPARE(segments.at(0).control1, QPointF(0, 10));
        QCOMPARE(segments.at(0).control2, QPointF(10, 10));
        QCOMPARE(segments.at(0).end, QPointF(10, 0));
        QCOMPARE(segments.at(1).type, SvgPathSegment::Type::Line);
        QCOMPARE(segments.at(1).end, QPointF(20, 0));
        QCOMPARE(segments.at(2).type, SvgPathSegment::Type::QuadraticBezier);
        QCOMPARE(segments.at(2).control1, QPointF(25, 10));
        QCOMPARE(segments.at(2).end, QPointF(30, 0));
        QCOMPARE(segments.at(3).type, SvgPathSegment::Type::Line);
        QCOMPARE(segments.at(3).end, QPointF(0, 0));
    }

    // 验证二次曲线和平滑二次曲线段会保留控制点和终点。
    /**
     * @brief 验证 preservesQuadraticAndSmoothQuadraticSegments 测试场景。
     */
    void preservesQuadraticAndSmoothQuadraticSegments() {
        const QList<SvgPathSegment> segments =
            SvgPathParser::parseSegments(QStringLiteral("M 0 0 Q 10 20 20 0 T 40 0 q 10 -20 20 0"));

        QCOMPARE(segments.size(), 3);
        QCOMPARE(segments.at(0).type, SvgPathSegment::Type::QuadraticBezier);
        QCOMPARE(segments.at(0).control1, QPointF(10, 20));
        QCOMPARE(segments.at(0).end, QPointF(20, 0));
        QCOMPARE(segments.at(1).type, SvgPathSegment::Type::QuadraticBezier);
        QCOMPARE(segments.at(1).control1, QPointF(30, -20));
        QCOMPARE(segments.at(1).end, QPointF(40, 0));
        QCOMPARE(segments.at(2).type, SvgPathSegment::Type::QuadraticBezier);
        QCOMPARE(segments.at(2).control1, QPointF(50, -20));
        QCOMPARE(segments.at(2).end, QPointF(60, 0));
    }

    // 验证圆弧、椭圆弧及旋转椭圆弧的分段类型和坐标。
    /**
     * @brief 验证 preservesCircularArcSegmentsAndEllipsesAndFallsBackForRotatedEllipses 测试场景。
     */
    void preservesCircularArcSegmentsAndEllipsesAndFallsBackForRotatedEllipses() {
        const QList<SvgPathSegment> circular =
            SvgPathParser::parseSegments(QStringLiteral("M 0 0 A 10 10 0 0 1 10 10"));
        QCOMPARE(circular.size(), 1);
        QCOMPARE(circular.first().type, SvgPathSegment::Type::CircularArc);
        QCOMPARE(circular.first().start, QPointF(0, 0));
        QCOMPARE(circular.first().end, QPointF(10, 10));
        QVERIFY(circular.first().arcMid.x() > 6.0);
        QVERIFY(circular.first().arcMid.y() < 5.0);

        const QList<SvgPathSegment> elliptical =
            SvgPathParser::parseSegments(QStringLiteral("M 0 0 A 10 5 0 0 1 10 10"));
        QCOMPARE(elliptical.size(), 1);
        QCOMPARE(elliptical.first().type, SvgPathSegment::Type::EllipticalArc);
        QCOMPARE(elliptical.first().start, QPointF(0, 0));
        QCOMPARE(elliptical.first().end, QPointF(10, 10));
        QVERIFY(elliptical.first().radiusX > 10.0);
        QVERIFY(elliptical.first().radiusY > 5.0);
        QVERIFY(!qFuzzyIsNull(elliptical.first().arcEndAngle - elliptical.first().arcStartAngle));

        const QList<SvgPathSegment> rotatedElliptical =
            SvgPathParser::parseSegments(QStringLiteral("M 0 0 A 10 5 30 0 1 10 10"));
        QVERIFY(rotatedElliptical.size() >= 2);
        QVERIFY(rotatedElliptical.size() < 10);
        QCOMPARE(rotatedElliptical.first().type, SvgPathSegment::Type::CubicBezier);
        QCOMPARE(rotatedElliptical.first().start, QPointF(0, 0));
        QVERIFY(qAbs(rotatedElliptical.last().end.x() - 10.0) < 1e-9);
        QVERIFY(qAbs(rotatedElliptical.last().end.y() - 10.0) < 1e-9);
        for (const SvgPathSegment& segment : rotatedElliptical)
            QCOMPARE(segment.type, SvgPathSegment::Type::CubicBezier);
    }

    // 验证空路径、缺少参数和溢出坐标会返回空结果。
    /**
     * @brief 验证 invalidOrEmptyPathsReturnNoPoints 测试场景。
     */
    void invalidOrEmptyPathsReturnNoPoints() {
        QVERIFY(SvgPathParser::parsePath(QString()).isEmpty());
        QVERIFY(SvgPathParser::parsePath(QStringLiteral("Q 1 2")).isEmpty());
        QVERIFY(SvgPathParser::parsePath(QStringLiteral("M 0 0 L 1e309 2")).isEmpty());
        QVERIFY(SvgPathParser::parsePath(QStringLiteral("M 1e308 0 l 1e308 0")).isEmpty());
        QVERIFY(SvgPathParser::parseSegments(QString()).isEmpty());
        QVERIFY(SvgPathParser::parseSegments(QStringLiteral("M 0 0 L 1e309 2")).isEmpty());
        QVERIFY(SvgPathParser::parseSegments(QStringLiteral("M 1e308 0 l 1e308 0")).isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestSvgPathParser)
#include "test_svg_path_parser.moc"
