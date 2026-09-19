#include "core/cadstar/CadstarAdapter.h"
#include "core/parser/CadstarModel.h"
#include "tests/common/TestPaths.hpp"

#include <QTest>

using namespace EasyKiConverter;
using namespace EasyKiConverter::Parser;

class TestCadstarParser : public QObject {
    Q_OBJECT

private slots:

    // 验证仓库内真实 Cadstar 样本可以解析 Pad、Package、Component 和 Part。
    void parsesCadstarFixture() {
        QString error;
        const QString content =
            Test::TestPaths::readText(Test::TestPaths::fixturePath(QStringLiteral("cadstar/Sample.lib")), &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        const CadstarLibrary library = CadstarParser::parse(content, QStringLiteral("Sample.lib"));
        QVERIFY(library.isRecognized());
        QVERIFY(!library.diagnostics.hasErrors());
        QCOMPARE(library.unit, LengthUnit::Millimeter);
        QCOMPARE(library.pads.size(), 2);
        QCOMPARE(library.packages.size(), 1);
        QCOMPARE(library.packages.first().pins.size(), 2);
        QCOMPARE(library.components.size(), 1);
        QCOMPARE(library.components.first().pins.first().numbers,
                 QStringList({QStringLiteral("1"), QStringLiteral("2")}));
        QCOMPARE(library.parts.size(), 1);
    }

    // 验证 Cadstar Package 和 Component 可以分别映射到现有 Footprint/Symbol IR。
    void adaptsCadstarLibraryToIr() {
        const CadstarLibrary library =
            CadstarParser::parse(QStringLiteral("UNITS MIL\nPAD P\nSHAPE ROUND\nDIAMETER 40\nENDPAD\nPACKAGE PKG\n"
                                                "PIN 1\nPOSITION (0 0)\nPAD P\nENDPIN\nENDPACKAGE\nCOMPONENT C\n"
                                                "PIN 1\nSTART (0 0)\nEND (100 0)\nENDPIN\nENDCOMPONENT\nPART R\n"
                                                "COMPONENT C\nPACKAGE PKG\nENDPART\n"),
                                 QStringLiteral("inline.lib"));
        QVERIFY(!library.diagnostics.hasErrors());
        ParseDiagnostics conversionDiagnostics;
        const CadstarConversionResult result = CadstarAdapter::toIR(library, &conversionDiagnostics);
        QCOMPARE(result.footprints.size(), 1);
        QCOMPARE(result.symbols.size(), 1);
        QCOMPARE(result.components.size(), 1);
        QCOMPARE(result.parts.size(), 1);
        QCOMPARE(result.components.first().name, QStringLiteral("R"));
        QCOMPARE(result.components.first().package, QStringLiteral("PKG"));
        QCOMPARE(result.footprints.first().pads.first().size, QSizeF(1.016, 1.016));
        QVERIFY(qAbs(result.symbols.first().pins.first().length - 2.54) < 1e-9);
        QVERIFY(!conversionDiagnostics.hasErrors());
    }

    // 验证非法数字和缺失关联会产生错误诊断而不是静默归零。
    void reportsCadstarInvalidFieldsAndMissingAssociations() {
        const CadstarLibrary library =
            CadstarParser::parse(QStringLiteral("UNITS MM\nPAD P\nSHAPE RECTANGLE\nWIDTH broken\nENDPAD\nPACKAGE PKG\n"
                                                "PIN 1\nPOSITION (0 0)\nPAD MISSING\nENDPIN\nENDPACKAGE\nPART R\n"
                                                "COMPONENT MISSING_SYMBOL\nPACKAGE PKG\nENDPART\n"),
                                 QStringLiteral("broken.lib"));
        QVERIFY(library.diagnostics.hasErrors());
        ParseDiagnostics conversionDiagnostics;
        CadstarAdapter::toIR(library, &conversionDiagnostics);
        QVERIFY(conversionDiagnostics.hasErrors());
    }

    // 验证重复焊盘名称在 Adapter 阶段不会静默绑定到第一个定义。
    void rejectsAmbiguousCadstarPadReference() {
        const CadstarLibrary library =
            CadstarParser::parse(QStringLiteral("UNITS MM\nPAD P\nSHAPE ROUND\nDIAMETER 1\nENDPAD\nPAD P\nSHAPE ROUND\n"
                                                "DIAMETER 2\nENDPAD\nPACKAGE PKG\nPIN 1\nPAD P\nENDPIN\nENDPACKAGE\n"),
                                 QStringLiteral("duplicate-pad.lib"));
        QVERIFY(library.isPadAmbiguous(QStringLiteral("P")));
        ParseDiagnostics diagnostics;
        const CadstarConversionResult result = CadstarAdapter::toIR(library, &diagnostics);
        QVERIFY(result.footprints.first().pads.isEmpty());
        QVERIFY(diagnostics.hasErrors());
    }

    // 验证多个 Cadstar 库合并后会保留重名候选并阻止错误关联。
    void mergesCadstarLibraries() {
        const CadstarLibrary first = CadstarParser::parse(
            QStringLiteral("UNITS MM\nPAD P\nSHAPE ROUND\nDIAMETER 1\nENDPAD\n"), QStringLiteral("first.lib"));
        const CadstarLibrary second = CadstarParser::parse(
            QStringLiteral("UNITS MM\nPAD P\nSHAPE ROUND\nDIAMETER 2\nENDPAD\n"), QStringLiteral("second.lib"));
        const CadstarLibrary merged = CadstarMerger::merge({first, second}, QStringLiteral("merged.lib"));
        QCOMPARE(merged.pads.size(), 2);
        QCOMPARE(merged.pads.at(1).name, QStringLiteral("P_2"));
        QVERIFY(merged.isPadAmbiguous(QStringLiteral("P")));
        QVERIFY(merged.diagnostics.hasErrors());
    }
};

QTEST_GUILESS_MAIN(TestCadstarParser)
#include "test_cadstar_parser.moc"
