#include "core/parser/PcadModel.h"
#include "core/pcad/PcadAdapter.h"
#include "tests/common/TestPaths.hpp"

#include <QTest>

using namespace EasyKiConverter;
using namespace EasyKiConverter::Parser;

class TestPcadParser : public QObject {
    Q_OBJECT

private slots:

    // 验证真实 P-CAD 样本可以解析 Pad Style、Pattern、图形和器件放置。
    void parsesPcadFixture() {
        QString error;
        const QString content =
            Test::TestPaths::readText(Test::TestPaths::fixturePath(QStringLiteral("pcad/Sample.pcb")), &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        const PcadBoard board = PcadParser::parse(content, QStringLiteral("Sample.pcb"));
        QVERIFY(board.isRecognized());
        QVERIFY(!board.diagnostics.hasErrors());
        QCOMPARE(board.unit, LengthUnit::Millimeter);
        QCOMPARE(board.layers.size(), 1);
        QCOMPARE(board.layers.first().name, QStringLiteral("TOP_COPPER"));
        QCOMPARE(board.padStyles.size(), 2);
        QCOMPARE(board.patterns.size(), 1);
        QCOMPARE(board.patterns.first().pads.size(), 2);
        QCOMPARE(board.placements.size(), 1);
        QCOMPARE(board.graphics.size(), 1);
        QVERIFY(qAbs(board.patterns.first().pads.at(1).rotation - 90.0) < 1e-9);
    }

    // 验证 P-CAD 字节入口统一经过编码检测，并保留 UTF-8 BOM 后的 S-expression。
    void parsesPcadBytesWithBom() {
        const QByteArray data = QByteArray(
            "\xEF\xBB\xBF(ACCEL_ASCII \"B\" (UNITS MM) (LIBRARY "
            "(PADSTYLEDEF \"P\" (PADSHAPE ROUND (SHAPEWIDTH 1) "
            "(SHAPEHEIGHT 1)))) )");
        const PcadBoard board = PcadParser::parseBytes(data, QStringLiteral("bom.pcb"));
        QVERIFY(board.isRecognized());
        QCOMPARE(board.padStyles.size(), 1);
        QVERIFY(!board.diagnostics.hasErrors());
    }

    // 验证 P-CAD Pattern 可以映射为现有 Footprint IR，并保留孔、图形和单位。
    void adaptsPcadPatternToIr() {
        const PcadBoard board =
            PcadParser::parse(QStringLiteral("(ACCEL_ASCII \"B\" (UNITS MIL) (LIBRARY (PADSTYLEDEF \"P\" (HOLEDIAM 10) "
                                             "(PADSHAPE RECT (SHAPEWIDTH 40) (SHAPEHEIGHT 20))) (PATTERNDEF \"K\" "
                                             "(PAD (PADNUM 1) (PADSTYLEREF \"P\") (PT 0 0)))) (PCBDESIGN))"),
                              QStringLiteral("inline.pcb"));
        QVERIFY(!board.diagnostics.hasErrors());
        ParseDiagnostics diagnostics;
        const PcadConversionResult result = PcadAdapter::toIR(board, &diagnostics);
        QCOMPARE(result.footprints.size(), 1);
        QCOMPARE(result.footprints.first().pads.size(), 1);
        QVERIFY(result.footprints.first().pads.first().isThroughHole());
        QVERIFY(qAbs(result.footprints.first().pads.first().size.width() - 1.016) < 1e-9);
        QVERIFY(result.placements.isEmpty());
        QVERIFY(!diagnostics.hasErrors());
    }

    // 验证板级放置会通过通用 IR 保留位置、旋转和镜像语义。
    void adaptsPcadPlacementToIr() {
        const PcadBoard board =
            PcadParser::parse(QStringLiteral("(ACCEL_ASCII \"B\" (UNITS MM) (LIBRARY (PADSTYLEDEF \"P\" "
                                             "(PADSHAPE ROUND (SHAPEWIDTH 1) (SHAPEHEIGHT 1))) (PATTERNDEF \"K\" "
                                             "(PAD (PADNUM 1) (PADSTYLEREF \"P\") (PT 0 0)))) (PCBDESIGN "
                                             "(MULTILAYER (PATTERN (PATTERNREF \"K\") (REFDESREF \"U1\") "
                                             "(PT 10 20) (ROTATION 450) (ISFLIPPED TRUE)))))"),
                              QStringLiteral("placement.pcb"));
        QVERIFY(!board.diagnostics.hasErrors());
        ParseDiagnostics diagnostics;
        const PcadConversionResult result = PcadAdapter::toIR(board, &diagnostics);
        QCOMPARE(result.placements.size(), 1);
        QCOMPARE(result.placements.first().reference, QStringLiteral("U1"));
        QCOMPARE(result.placements.first().footprintName, QStringLiteral("K"));
        QVERIFY(qAbs(result.placements.first().position.x() - 10.0) < 1e-9);
        QVERIFY(qAbs(result.placements.first().position.y() + 20.0) < 1e-9);
        QVERIFY(qAbs(result.placements.first().rotation - 45.0) < 1e-9);
        QVERIFY(result.placements.first().mirrored);
        QVERIFY(!diagnostics.hasErrors());
    }

    // 验证板级图元转换为通用 BoardIR，而不是把 P-CAD 专用模型泄漏到适配结果。
    void adaptsPcadBoardGraphicsToIr() {
        const PcadBoard board =
            PcadParser::parse(QStringLiteral("(ACCEL_ASCII \"B\" (UNITS MM) (LIBRARY) (PCBDESIGN (LAYERCONTENTS "
                                             "(LINE (PT 0 0) (PT 5 0) (WIDTH 0.2) (LAYERNUMREF 1)) "
                                             "(CIRCLE (PT 2 2) (RADIUS 1) (LAYERNUMREF 2)) "
                                             "(TEXT \"NOTE\" (PT 1 1) (HEIGHT 1) (ROTATION 900)))) )"),
                              QStringLiteral("board-graphics.pcb"));
        QVERIFY(!board.diagnostics.hasErrors());
        ParseDiagnostics diagnostics;
        const PcadConversionResult result = PcadAdapter::toIR(board, &diagnostics);
        QCOMPARE(result.board.tracks.size(), 1);
        QCOMPARE(result.board.circles.size(), 1);
        QCOMPARE(result.board.texts.size(), 1);
        QCOMPARE(result.board.texts.first().text, QStringLiteral("NOTE"));
        QVERIFY(result.board.isEmpty() == false);
        QVERIFY(!diagnostics.hasErrors());
    }

    // 验证 P-CAD 文本的坐标、字号和旋转不会在格式模型到 IR 时丢失。
    void adaptsPcadTextGeometryToIr() {
        const PcadBoard board =
            PcadParser::parse(QStringLiteral("(ACCEL_ASCII \"B\" (UNITS MM) (LIBRARY (PATTERNDEF \"K\" "
                                             "(PATTERNGRAPHICS (TEXT \"LABEL\" (PT 3 4) (HEIGHT 1.2) (ROTATION 900) "
                                             "(WIDTH 0.1))))) (PCBDESIGN))"),
                              QStringLiteral("text.pcb"));
        QVERIFY(!board.diagnostics.hasErrors());
        ParseDiagnostics diagnostics;
        const PcadConversionResult result = PcadAdapter::toIR(board, &diagnostics);
        QCOMPARE(result.footprints.size(), 1);
        QCOMPARE(result.footprints.first().texts.size(), 1);
        const IR::FootprintTextIR& text = result.footprints.first().texts.first();
        QCOMPARE(text.text, QStringLiteral("LABEL"));
        QVERIFY(qAbs(text.position.x() - 3.0) < 1e-9);
        QVERIFY(qAbs(text.position.y() + 4.0) < 1e-9);
        QVERIFY(qAbs(text.fontSize - 1.2) < 1e-9);
        QVERIFY(qAbs(text.rotation - 90.0) < 1e-9);
        QVERIFY(!diagnostics.hasErrors());
    }

    // 验证空文件、损坏括号、非法数字和缺失引用均产生可观察诊断。
    void reportsPcadInvalidInputAndMissingReferences() {
        const PcadBoard empty = PcadParser::parse(QString(), QStringLiteral("empty.pcb"));
        QVERIFY(empty.diagnostics.hasErrors());

        const PcadBoard broken =
            PcadParser::parse(QStringLiteral("(ACCEL_ASCII \"B\" (UNITS MM) (LIBRARY (PADSTYLEDEF \"P\" "
                                             "(PADSHAPE RECT (SHAPEWIDTH invalid) (SHAPEHEIGHT 1))) (PATTERNDEF \"K\" "
                                             "(PAD (PADNUM 1) (PADSTYLEREF \"MISSING\") (PT 0 0))))"),
                              QStringLiteral("broken.pcb"));
        QVERIFY(broken.diagnostics.hasErrors());
        ParseDiagnostics diagnostics;
        const PcadConversionResult result = PcadAdapter::toIR(broken, &diagnostics);
        QVERIFY(result.footprints.first().pads.isEmpty());
        QVERIFY(diagnostics.hasErrors());

        const PcadBoard malformed = PcadParser::parse(QStringLiteral("(ACCEL_ASCII"), QStringLiteral("malformed.pcb"));
        QVERIFY(malformed.diagnostics.hasErrors());
    }

    // 验证空 Pattern 名称、重复 Pattern 和缺失放置引用均产生结构级诊断。
    void reportsPcadDefinitionErrors() {
        const PcadBoard board =
            PcadParser::parse(QStringLiteral("(ACCEL_ASCII \"B\" (UNITS MM) (LIBRARY "
                                             "(PATTERNDEF \"P\" (PAD (PADSTYLEREF \"S\"))) "
                                             "(PATTERNDEF \"P\" (PAD (PADNUM 1)))"
                                             ") (PCBDESIGN (MULTILAYER (PATTERN (REFDESREF \"U1\")))))"),
                              QStringLiteral("definition-errors.pcb"));
        QVERIFY(board.diagnostics.hasErrors());
        QVERIFY(board.diagnostics.items().size() >= 4);
    }

    // 验证重复 Pad Style 和 Pattern 不会被引用逻辑静默绑定到首个定义。
    void rejectsAmbiguousPcadReferences() {
        const PcadBoard board = PcadParser::parse(
            QStringLiteral("(ACCEL_ASCII \"B\" (UNITS MM) (LIBRARY "
                           "(PADSTYLEDEF \"P\" (PADSHAPE ROUND (SHAPEWIDTH 1) (SHAPEHEIGHT 1))) "
                           "(PADSTYLEDEF \"P\" (PADSHAPE RECT (SHAPEWIDTH 2) (SHAPEHEIGHT 2))) "
                           "(PATTERNDEF \"K\" (PAD (PADNUM 1) (PADSTYLEREF \"P\") (PT 0 0))) "
                           "(PATTERNDEF \"K\" (PAD (PADNUM 2) (PADSTYLEREF \"P\") (PT 1 1)))) "
                           "(PCBDESIGN (MULTILAYER (PATTERN (PATTERNREF \"K\") (REFDESREF \"U1\")))))"),
            QStringLiteral("ambiguous.pcb"));
        QVERIFY(board.diagnostics.hasErrors());

        ParseDiagnostics diagnostics;
        const PcadConversionResult result = PcadAdapter::toIR(board, &diagnostics);
        QVERIFY(diagnostics.hasErrors());
        QCOMPARE(result.placements.size(), 1);
        QVERIFY(diagnostics.items().size() >= 3);
    }
};

QTEST_GUILESS_MAIN(TestPcadParser)
#include "test_pcad_parser.moc"
