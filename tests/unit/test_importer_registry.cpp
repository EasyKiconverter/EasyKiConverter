#include "core/parser/ConversionReport.h"
#include "core/parser/ImporterRegistry.h"

#include <QTest>

using namespace EasyKiConverter::Parser;

class TestImporterRegistry : public QObject {
    Q_OBJECT

private slots:

    // 验证注册表拒绝无效描述和重复 ID，并保留合法注册顺序。
    void registersUniqueImporters() {
        ImporterRegistry registry;
        const ImporterDescriptor descriptor{
            QStringLiteral("pcad"),
            QStringLiteral("P-CAD ASCII"),
            DetectedFormat::PcadSExpression,
            {QStringLiteral(".pcb")},
            [](const QString& fileName, const QByteArray&) { return fileName.endsWith(QStringLiteral(".pcb")); }};
        QVERIFY(registry.registerImporter(descriptor));
        QVERIFY(!registry.registerImporter(descriptor));
        QVERIFY(!registry.registerImporter({{}, {}, DetectedFormat::Unknown, {}, {}}));
        QCOMPARE(registry.importers().size(), 1);
        QCOMPARE(registry.detect(QStringLiteral("board.pcb"), {})->id, QStringLiteral("pcad"));
        QVERIFY(registry.detect(QStringLiteral("board.txt"), {}) == nullptr);
    }

    // 验证报告可以合并解析诊断、记录进度，并保留取消后的部分成功状态。
    void reportsPartialProgressAndCancellation() {
        ConversionReport report;
        report.setProgress(2, 5, QStringLiteral("part-2"));
        QCOMPARE(report.progress().completed, 2);
        QCOMPARE(report.progress().total, 5);
        QCOMPARE(report.status(), ConversionStatus::Ok);

        ParseDiagnostic warning;
        warning.severity = ParseSeverity::Warning;
        warning.scope = ParseScope::Component;
        warning.entity = QStringLiteral("U1");
        warning.message = QStringLiteral("测试警告");
        report.add(warning);
        QCOMPARE(report.status(), ConversionStatus::Warning);

        report.markCancelled();
        QVERIFY(report.isCancelled());
        QCOMPARE(report.status(), ConversionStatus::Skipped);
        QCOMPARE(report.progress().currentItem, QStringLiteral("part-2"));
    }
};

QTEST_GUILESS_MAIN(TestImporterRegistry)
#include "test_importer_registry.moc"
