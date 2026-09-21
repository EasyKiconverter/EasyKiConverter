#include "services/export/ExportProgress.h"
#include "utils/CommandLineParser.h"
#include "utils/cli/CliContext.h"

#include <QTest>

using namespace EasyKiConverter;

class TestTargetFormatOptions final : public QObject {
    Q_OBJECT

private slots:
    /** @brief 验证 P-CAD CLI 仍保留符号和封装导出选项。 */
    void pcadKeepsSymbolAndFootprintExports();
};

/** @brief 验证 P-CAD 目标不会在 CLI 选项转换阶段关闭符号或封装导出。 */
void TestTargetFormatOptions::pcadKeepsSymbolAndFootprintExports() {
    const QByteArray application = "easykiconverter";
    const QByteArray convert = "convert";
    const QByteArray component = "component";
    const QByteArray componentOption = "-c";
    const QByteArray componentId = "C100";
    const QByteArray outputOption = "-o";
    const QByteArray outputPath = "out";
    const QByteArray targetOption = "--target-format";
    const QByteArray target = "pcad";
    char* argv[] = {const_cast<char*>(application.constData()),
                    const_cast<char*>(convert.constData()),
                    const_cast<char*>(component.constData()),
                    const_cast<char*>(componentOption.constData()),
                    const_cast<char*>(componentId.constData()),
                    const_cast<char*>(outputOption.constData()),
                    const_cast<char*>(outputPath.constData()),
                    const_cast<char*>(targetOption.constData()),
                    const_cast<char*>(target.constData())};
    CommandLineParser parser(static_cast<int>(sizeof(argv) / sizeof(argv[0])), argv);
    QVERIFY(parser.parse());

    CliContext context(parser);
    const ExportOptions options = context.createExportOptions();
    QCOMPARE(options.targetFormat, TargetEdaFormat::Pcad);
    QVERIFY(options.exportSymbol);
    QVERIFY(options.exportFootprint);
}

QTEST_GUILESS_MAIN(TestTargetFormatOptions)
#include "test_target_format_options.moc"
