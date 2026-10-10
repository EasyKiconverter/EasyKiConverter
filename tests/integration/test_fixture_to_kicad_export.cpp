#include "core/altium/ExporterAltiumSymbol.h"
#include "core/altium/readers/AltiumSchLibReader.h"
#include "core/easyeda/EasyedaFootprintImporter.h"
#include "core/easyeda/EasyedaSymbolImporter.h"
#include "core/ir/FootprintDataConverter.h"
#include "core/ir/SymbolDataConverter.h"
#include "core/kicad/ExporterFootprint.h"
#include "core/kicad/ExporterSymbol.h"
#include "tests/common/TestPaths.hpp"

#include <QFileInfo>
#include <QSet>
#include <QTemporaryDir>
#include <QTest>

using namespace EasyKiConverter;
using namespace EasyKiConverter::Test;

class TestFixtureToKiCadExport : public QObject {
    Q_OBJECT

private slots:

    // 验证 EasyEDA 符号和封装夹具能够完整导出为 KiCad 文件。
    /**
     * @brief 验证 testFixtureDataExportsKiCadFiles 测试场景。
     */
    void testFixtureDataExportsKiCadFiles() {
        // error：测试期间收集的错误或诊断信息。
        QString error;
        const QJsonObject symbolFixture =
            TestPaths::readJsonObject(TestPaths::fixturePath(QStringLiteral("easyeda/symbol_basic.json")), &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));

        const QJsonObject footprintFixture =
            TestPaths::readJsonObject(TestPaths::fixturePath(QStringLiteral("easyeda/footprint_basic.json")), &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));

        // symbolImporter：负责读取测试输入的导入器实例。
        EasyedaSymbolImporter symbolImporter;
        // footprintImporter：负责读取测试输入的导入器实例。
        EasyedaFootprintImporter footprintImporter;
        // symbol：测试用符号数据。
        const QSharedPointer<SymbolData> symbol = symbolImporter.importSymbolData(symbolFixture);
        // footprint：测试用封装数据。
        const QSharedPointer<FootprintData> footprint = footprintImporter.importFootprintData(footprintFixture);
        QVERIFY(symbol);
        QVERIFY(footprint);

        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // symbolLibraryPath：测试使用的文件系统路径。
        const QString symbolLibraryPath = tempDir.filePath(QStringLiteral("FixtureLib.kicad_sym"));
        // footprintPath：测试使用的文件系统路径。
        const QString footprintPath = tempDir.filePath(QStringLiteral("FIXTURE_FOOTPRINT.kicad_mod"));
        // altiumSymbolPath：测试使用的文件系统路径。
        const QString altiumSymbolPath = tempDir.filePath(QStringLiteral("FixtureLib.SchLib"));

        // symbolExporter：负责生成测试输出的导出器实例。
        ExporterSymbol symbolExporter;
        // irSymbol：测试用符号数据。
        auto irSymbol = IR::toSymbolIR(*symbol);
        QVERIFY(symbolExporter.exportSymbolLibrary(
            {irSymbol}, QStringLiteral("FixtureLib"), symbolLibraryPath, false, false));

        // footprintExporter：负责生成测试输出的导出器实例。
        ExporterFootprint footprintExporter;
        // irFootprint：测试用封装数据。
        auto irFootprint = IR::toFootprintIR(*footprint);
        QVERIFY(footprintExporter.exportFootprint(irFootprint, footprintPath));

        // altiumSymbolExporter：负责生成测试输出的导出器实例。
        ExporterAltiumSymbol altiumSymbolExporter;
        QVERIFY2(altiumSymbolExporter.exportSymbolLibrary(
                     {irSymbol}, QStringLiteral("FixtureLib"), altiumSymbolPath, false, false),
                 qPrintable(altiumSymbolExporter.diagnostics().join('\n')));

        QVERIFY2(QFileInfo::exists(symbolLibraryPath), qPrintable(symbolLibraryPath));
        QVERIFY2(QFileInfo::exists(footprintPath), qPrintable(footprintPath));
        QVERIFY2(QFileInfo::exists(altiumSymbolPath), qPrintable(altiumSymbolPath));

        // symbolContent：测试使用的文本或数据内容。
        const QString symbolContent = TestPaths::readText(symbolLibraryPath, &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QVERIFY(symbolContent.contains(QStringLiteral("(kicad_symbol_lib")));
        QVERIFY(symbolContent.contains(QStringLiteral("(symbol \"FIXTURE_SYMBOL\"")));
        QVERIFY(symbolContent.contains(QStringLiteral("\"Footprint\"")));
        QVERIFY(symbolContent.contains(QStringLiteral("\"FixtureLib:FIXTURE_FOOTPRINT\"")));
        QVERIFY(symbolContent.contains(QStringLiteral("(pin input line")));

        // footprintContent：测试使用的文本或数据内容。
        const QString footprintContent = TestPaths::readText(footprintPath, &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QVERIFY(footprintContent.contains(QStringLiteral("(footprint easykiconverter:FIXTURE_FOOTPRINT")));
        QVERIFY(footprintContent.contains(QStringLiteral("(pad 1 smd rect")));
        QVERIFY(footprintContent.contains(QStringLiteral("(fp_line")));

        // altiumReader：负责回读生成内容的 reader 实例。
        AltiumSchLibReader altiumReader;
        QVERIFY2(altiumReader.open(altiumSymbolPath), qPrintable(altiumReader.errorString()));
        // altiumComponents：测试使用的缓存或元器件对象。
        const auto altiumComponents = altiumReader.components();
        QCOMPARE(altiumComponents.size(), 1);
        QCOMPARE(altiumComponents.first().name, QStringLiteral("FIXTURE_SYMBOL"));
        // altiumRecords：测试解析得到的条目集合。
        QVector<AltiumSchLibReader::Record> altiumRecords;
        QVERIFY2(altiumReader.readComponentRecords(0, &altiumRecords), qPrintable(altiumReader.errorString()));
        // altiumRecordTypes：测试使用的配置值或状态。
        QSet<int> altiumRecordTypes;
        for (const auto& record : altiumRecords)
            altiumRecordTypes.insert(record.recordType);
        QVERIFY(altiumRecordTypes.contains(1));  // Component
        QVERIFY(altiumRecordTypes.contains(2));  // Pin
        QVERIFY(altiumRecordTypes.contains(4));  // Text
        QVERIFY(altiumRecordTypes.contains(10) || altiumRecordTypes.contains(14));  // Rectangle/rounded rectangle
        QVERIFY(altiumRecordTypes.contains(5));  // Native cubic Bézier from SVG curve
        QVERIFY(altiumRecordTypes.contains(30));  // Image
    }
};

QTEST_GUILESS_MAIN(TestFixtureToKiCadExport)
#include "test_fixture_to_kicad_export.moc"
