#include "core/kicad/FootprintGraphicsGenerator.h"
#include "core/kicad/SymbolGraphicsGenerator.h"

#include <QTest>

using namespace EasyKiConverter;

class TestKiCadGraphicsGenerators : public QObject {
    Q_OBJECT

private slots:

    /**
     * @brief 验证 generatePadHandlesSmdRect 测试场景。
     */
    void generatePadHandlesSmdRect() {
        // pad：测试用焊盘数据。
        IR::FootprintPadIR pad;
        pad.shape = IR::PadShape::Rect;
        pad.position = QPointF(10.0, -5.0);  // mm
        pad.size = QSizeF(20.0, 10.0);  // mm
        pad.layer = IR::LayerType::TopCopper;
        pad.number = QStringLiteral("1");
        pad.rotation = 270;
        pad.padType = IR::PadType::Smd;

        // output：测试得到的实际结果数据。
        const QString output = FootprintGraphicsGenerator().generatePad(pad, 0, 0);

        QVERIFY(output.contains(QStringLiteral("(pad 1 smd rect")));
        QVERIFY(output.contains(QStringLiteral("(at 10.00 -5.00 -90.00)")));
        QVERIFY(output.contains(QStringLiteral("(size 20.00 10.00)")));
        QVERIFY(output.contains(QStringLiteral("(layers F.Cu F.Paste F.Mask)")));
    }

    /**
     * @brief 验证 generatePadHandlesThroughHoleOvalDrill 测试场景。
     */
    void generatePadHandlesThroughHoleOvalDrill() {
        // pad：测试用焊盘数据。
        IR::FootprintPadIR pad;
        pad.shape = IR::PadShape::Oval;
        pad.position = QPointF(0, 0);  // mm
        pad.size = QSizeF(12.0, 20.0);  // mm
        pad.layer = IR::LayerType::MultiLayer;
        pad.number = QStringLiteral("2");
        pad.padType = IR::PadType::ThroughHole;
        pad.holeSize = 4.0;  // diameter mm
        pad.holeLength = 8.0;  // mm

        // output：测试得到的实际结果数据。
        const QString output = FootprintGraphicsGenerator().generatePad(pad, 0, 0);

        QVERIFY(output.contains(QStringLiteral("(pad 2 thru_hole oval")));
        QVERIFY(output.contains(QStringLiteral("(layers *.Cu *.Mask)")));
        QVERIFY(output.contains(QStringLiteral("(drill oval 4.00 8.00)")));
    }

    /**
     * @brief 验证 generateCustomPadCreatesPrimitivePolygon 测试场景。
     */
    void generateCustomPadCreatesPrimitivePolygon() {
        // pad：测试用焊盘数据。
        IR::FootprintPadIR pad;
        pad.shape = IR::PadShape::Polygon;
        pad.position = QPointF(10.0, 0.0);  // mm
        pad.size = QSizeF(4.0, 4.0);  // mm
        pad.layer = IR::LayerType::TopCopper;
        pad.number = QStringLiteral("3");
        pad.padType = IR::PadType::Smd;
        // 自定义形状点（已解析为 mm）
        pad.customShapePoints = {QPointF(8.0, -2.0), QPointF(12.0, -2.0), QPointF(12.0, 2.0), QPointF(8.0, 2.0)};

        // output：测试得到的实际结果数据。
        const QString output = FootprintGraphicsGenerator().generatePad(pad, 0, 0);

        QVERIFY(output.contains(QStringLiteral("(pad 3 smd custom")));
        QVERIFY(output.contains(QStringLiteral("(primitives")));
        QVERIFY(output.contains(QStringLiteral("(gr_poly")));
    }

    /**
     * @brief 验证 generateArcUsesModernThreePointSyntax 测试场景。
     */
    void generateArcUsesModernThreePointSyntax() {
        // arc：测试用圆弧图元。
        IR::FootprintArcIR arc;
        arc.center = QPointF(0.0, 0.0);
        arc.radius = 1.0;
        arc.startAngle = 0.0;
        arc.endAngle = 90.0;
        arc.width = 0.2;
        arc.layer = IR::LayerType::TopSilk;

        // output：测试得到的实际结果数据。
        const QString output = FootprintGraphicsGenerator().generateArc(arc, 0, 0);
        QVERIFY(output.contains(QStringLiteral("(fp_arc (start 1.00 0.00) (mid 0.70 0.70) (end 0.00 1.00)")));
        QVERIFY(output.contains(QStringLiteral("(stroke (width 0.20) (type default))")));
        QVERIFY(!output.contains(QStringLiteral("(angle ")));
    }

    /**
     * @brief 验证 generatePinMapsTypesAndDirections 测试场景。
     */
    void generatePinMapsTypesAndDirections() {
        // generator：测试用生成器对象。
        SymbolGraphicsGenerator generator;
        generator.setCurrentOrigin(0.0, 0.0);

        // pin：测试用引脚数据。
        IR::SymbolPinIR pin;
        pin.position = QPointF(10.0, -10.0);  // mm (已翻转 Y)
        pin.direction = IR::PinDirection::Up;  // 对应 rotation=90
        pin.designator = QStringLiteral("1");
        pin.electricalType = IR::PinElectricalType::Bidirectional;
        pin.length = 20.0 * 0.0254;  // 从 px 转换后的 mm 值
        pin.name = QStringLiteral("INA");

        // output：测试得到的实际结果数据。
        const QString output = generator.generatePin(pin);

        QVERIFY(output.contains(QStringLiteral("(pin bidirectional line")));
        QVERIFY(output.contains(QStringLiteral("(at 10.00 -10.00 270)")));
        QVERIFY(output.contains(QStringLiteral("(length 0.51)")));
        QVERIFY(output.contains(QStringLiteral("(name \"INA\"")));
        QVERIFY(output.contains(QStringLiteral("(number \"1\"")));
    }

    /**
     * @brief 验证 generatePinUsesOriginalNamePositionAndNativeNumber 测试场景。
     */
    void generatePinUsesOriginalNamePositionAndNativeNumber() {
        // generator：测试用生成器对象。
        SymbolGraphicsGenerator generator;
        // pin：测试用引脚数据。
        IR::SymbolPinIR pin;
        pin.position = QPointF(0.0, 0.0);
        pin.length = 2.54;
        pin.name = QStringLiteral("IN");
        pin.designator = QStringLiteral("1");
        pin.hasNamePosition = true;
        pin.namePosition = QPointF(-5.08, 2.54);
        pin.nameRotation = 90.0;
        pin.nameAnchor = QStringLiteral("start");
        pin.hasNumberPosition = true;
        pin.numberPosition = QPointF(5.08, -2.54);
        pin.numberAnchor = QStringLiteral("end");

        // output：测试得到的实际结果数据。
        const QString output = generator.generatePin(pin);

        QVERIFY(output.contains(QStringLiteral("(name \"IN\" (effects (font (size 1.27 1.27) (thickness 0)) hide))")));
        QVERIFY(output.contains(QStringLiteral("(number \"1\" (effects (font (size 1.27 1.27) (thickness 0) )))")));
        QVERIFY(output.contains(QStringLiteral("(at -5.08 2.54 270)")));
        QVERIFY(output.contains(QStringLiteral("(justify left)")));
        QVERIFY(!output.contains(QStringLiteral("\n      \"1\"\n")));
    }

    /**
     * @brief 验证 generatePolylineAndPathReturnKiCadPolylines 测试场景。
     */
    void generatePolylineAndPathReturnKiCadPolylines() {
        // generator：测试用生成器对象。
        SymbolGraphicsGenerator generator;
        generator.setCurrentOrigin(0.0, 0.0);

        // IR 折线坐标已为 mm
        IR::SymbolPolylineIR polyline;
        polyline.points = {QPointF(0.0, 0.0), QPointF(10.0, 0.0), QPointF(10.0, 10.0)};
        polyline.strokeWidth = 0.0;
        polyline.isFilled = true;

        // polylineOutput：测试得到的实际结果数据。
        const QString polylineOutput = generator.generatePolyline(polyline);
        QVERIFY(polylineOutput.contains(QStringLiteral("(polyline")));
        QVERIFY(polylineOutput.contains(QStringLiteral("(xy 0.00 0.00) (xy 10.00 0.00) (xy 10.00 10.00)")));
        QVERIFY(polylineOutput.contains(QStringLiteral("(fill (type background))")));

        // IR 路径坐标已为 mm
        IR::SymbolPathIR path;
        path.points = {QPointF(0.0, 0.0), QPointF(10.0, 0.0), QPointF(10.0, 10.0)};
        path.strokeWidth = 0.0;
        path.isFilled = false;

        // pathOutput：测试使用的文件系统路径。
        const QString pathOutput = generator.generatePath(path);
        QVERIFY(pathOutput.contains(QStringLiteral("(polyline")));
        QVERIFY(pathOutput.contains(QStringLiteral("(fill (type none))")));
        QVERIFY(pathOutput.contains(QStringLiteral("(xy 0.00 0.00)")));
    }
};

QTEST_GUILESS_MAIN(TestKiCadGraphicsGenerators)
#include "test_kicad_graphics_generators.moc"
