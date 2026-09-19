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
};

QTEST_GUILESS_MAIN(TestPcadParser)
#include "test_pcad_parser.moc"
