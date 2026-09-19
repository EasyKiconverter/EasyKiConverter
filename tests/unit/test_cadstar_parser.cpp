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
        QCOMPARE(result.parts.size(), 1);
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
};

QTEST_GUILESS_MAIN(TestCadstarParser)
#include "test_cadstar_parser.moc"
