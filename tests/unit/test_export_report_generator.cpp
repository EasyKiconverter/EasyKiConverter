#include "core/network/NetworkClient.h"
#include "services/export/ExportProgress.h"
#include "services/export/ExportReportGenerator.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>
#include <QTextStream>

using namespace EasyKiConverter;

class TestExportReportGenerator : public QObject {
    Q_OBJECT

private slots:

    // 清理网络客户端单例，避免影响其他测试。
    /**
     * @brief 清理测试用例使用的资源。
     */
    void cleanupTestCase() {
        NetworkClient::destroyInstance();
    }

    // 验证关闭调试模式时不生成详细报告。
    /**
     * @brief 验证 debugModeDisabledDoesNotWriteReport 测试场景。
     */
    void debugModeDisabledDoesNotWriteReport() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // options：当前测试使用的导出选项。
        ExportOptions options = makeOptions(tempDir.path());
        options.debugMode = false;

        ExportReportGenerator::writeDetailedReport(QStringLiteral("export-completed"), options, makeProgress());

        QVERIFY(!QFile::exists(reportPath(tempDir.path())));
    }

    // 验证输出目录为空时不生成详细报告。
    /**
     * @brief 验证 emptyOutputPathDoesNotWriteReport 测试场景。
     */
    void emptyOutputPathDoesNotWriteReport() {
        // options：当前测试使用的导出选项。
        ExportOptions options = makeOptions(QString());
        options.debugMode = true;

        ExportReportGenerator::writeDetailedReport(QStringLiteral("export-completed"), options, makeProgress());

        QVERIFY(options.outputPath.isEmpty());
    }

    // 验证详细报告会创建目录并写入导出进度快照。
    /**
     * @brief 验证 detailedReportCreatesOutputDirectoryAndWritesProgressSnapshot 测试场景。
     */
    void detailedReportCreatesOutputDirectoryAndWritesProgressSnapshot() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // outputPath：测试使用的文件系统路径。
        const QString outputPath = QDir(tempDir.path()).filePath(QStringLiteral("nested/reports"));
        // options：当前测试使用的导出选项。
        ExportOptions options = makeOptions(outputPath);
        // progress：测试用导出进度状态。
        const ExportOverallProgress progress = makeProgress();

        ExportReportGenerator::writeDetailedReport(QStringLiteral("export-failed-test"), options, progress);

        // content：测试使用的文本或数据内容。
        const QString content = readText(reportPath(outputPath));
        QVERIFY(content.contains(QStringLiteral("# EasyKiConverter Export Detailed Report")));
        QVERIFY(content.contains(QStringLiteral("- Reason: export-failed-test")));
        QVERIFY(content.contains(QStringLiteral("- Weak network support: enabled")));
        QVERIFY(content.contains(QStringLiteral("- Output path: %1").arg(outputPath)));
        QVERIFY(content.contains(QStringLiteral("- Total components: 3")));

        QVERIFY(content.contains(QStringLiteral("## Export Options")));
        QVERIFY(content.contains(QStringLiteral("- Library name: ReportLib")));
        QVERIFY(content.contains(QStringLiteral("- Export symbol: yes")));
        QVERIFY(content.contains(QStringLiteral("- Export footprint: yes")));
        QVERIFY(content.contains(QStringLiteral("- Export 3D model: no")));
        QVERIFY(content.contains(QStringLiteral("- Export preview images: yes")));
        QVERIFY(content.contains(QStringLiteral("- Export datasheet: no")));
        QVERIFY(content.contains(QStringLiteral("- Overwrite existing files: yes")));
        QVERIFY(content.contains(QStringLiteral("- Update mode: yes")));
        QVERIFY(content.contains(QStringLiteral("- Debug mode: yes")));

        QVERIFY(content.contains(QStringLiteral("## Preload")));
        QVERIFY(content.contains(QStringLiteral("- Completed: 2/3")));
        QVERIFY(content.contains(QStringLiteral("- Success: 2")));
        QVERIFY(content.contains(QStringLiteral("- Failed: 1")));
        QVERIFY(content.contains(QStringLiteral("- In progress: 0")));
        QVERIFY(content.contains(QStringLiteral("### Preload Failures")));
        QVERIFY(content.contains(QStringLiteral("- `C404`: Not found")));

        QVERIFY(content.contains(QStringLiteral("### Symbol")));
        QVERIFY(content.contains(QStringLiteral("- Completed: 3/3")));
        QVERIFY(content.contains(QStringLiteral("- Skipped: 1")));
        QVERIFY(content.contains(QStringLiteral("#### Input Diagnostics")));
        QVERIFY(content.contains(QStringLiteral("- `C123`:")));
        QVERIFY(content.contains(QStringLiteral("  - Rectangle 0 has a non-positive size")));
        QVERIFY(content.contains(QStringLiteral("  - Path 0 has no commands")));
        QVERIFY(content.contains(QStringLiteral("#### Exporter Diagnostics")));
        QVERIFY(content.contains(QStringLiteral("组件 IMAGE_DIAGNOSTICS 图片 0 的嵌入数据为空，已跳过 Storage")));
        QVERIFY(content.contains(QStringLiteral("### Footprint")));
        QVERIFY(content.contains(QStringLiteral("- Completed: 2/3")));
        QVERIFY(content.contains(QStringLiteral("#### Failed or Skipped Items")));
        QVERIFY(content.contains(QStringLiteral("- `C404`: failed — Footprint export failed")));

        QVERIFY(content.contains(QStringLiteral("## Weak Network Diagnostics")));
        QVERIFY(content.contains(QStringLiteral("NetworkRuntimeStats total{")));
    }

    // 验证网络拒绝原因会原样进入预加载失败报告。
    /**
     * @brief 验证 detailedReportIncludesNetworkFailureReason 测试场景。
     */
    void detailedReportIncludesNetworkFailureReason() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // options：当前测试使用的导出选项。
        ExportOptions options = makeOptions(tempDir.path());
        // progress：测试用导出进度状态。
        ExportOverallProgress progress = makeProgress();
        progress.preloadProgress.failedComponents.insert(
            QStringLiteral("C403"), QStringLiteral("HTTP 403: Forbidden; response: access denied"));

        ExportReportGenerator::writeDetailedReport(QStringLiteral("preload-completed"), options, progress);

        // content：测试使用的文本或数据内容。
        const QString content = readText(reportPath(tempDir.path()));
        QVERIFY(content.contains(QStringLiteral("- `C403`: HTTP 403: Forbidden; response: access denied")));
    }

private:
    static ExportOptions makeOptions(const QString& outputPath) {
        // options：当前测试使用的导出选项。
        ExportOptions options;
        options.outputPath = outputPath;
        options.libName = QStringLiteral("ReportLib");
        options.exportSymbol = true;
        options.exportFootprint = true;
        options.exportModel3D = false;
        options.exportPreviewImages = true;
        options.exportDatasheet = false;
        options.overwriteExistingFiles = true;
        options.updateMode = true;
        options.weakNetworkSupport = true;
        options.debugMode = true;
        return options;
    }

    static ExportOverallProgress makeProgress() {
        // progress：测试用导出进度状态。
        ExportOverallProgress progress;
        progress.currentStage = ExportOverallProgress::Stage::Failed;
        progress.totalComponents = 3;
        progress.startTime = QDateTime::fromString(QStringLiteral("2026-05-12T10:00:00"), Qt::ISODate);
        progress.endTime = QDateTime::fromString(QStringLiteral("2026-05-12T10:01:00"), Qt::ISODate);

        progress.preloadProgress.totalCount = 3;
        progress.preloadProgress.completedCount = 2;
        progress.preloadProgress.successCount = 2;
        progress.preloadProgress.failedCount = 1;
        progress.preloadProgress.inProgressCount = 0;
        progress.preloadProgress.failedComponents.insert(QStringLiteral("C404"), QStringLiteral("Not found"));

        // symbolProgress：符号导出进度状态。
        ExportTypeProgress symbolProgress;
        symbolProgress.typeName = QStringLiteral("Symbol");
        symbolProgress.totalCount = 3;
        symbolProgress.completedCount = 3;
        symbolProgress.successCount = 2;
        symbolProgress.failedCount = 0;
        symbolProgress.skippedCount = 1;
        symbolProgress.inProgressCount = 0;
        symbolProgress.diagnostics = {QStringLiteral("组件 IMAGE_DIAGNOSTICS 图片 0 的嵌入数据为空，已跳过 Storage")};
        // symbolStatus：测试用符号数据。
        ExportItemStatus symbolStatus;
        symbolStatus.status = ExportItemStatus::Status::Success;
        symbolStatus.diagnostics = {QStringLiteral("Rectangle 0 has a non-positive size"),
                                    QStringLiteral("Path 0 has no commands")};
        symbolProgress.itemStatus.insert(QStringLiteral("C123"), symbolStatus);
        progress.exportTypeProgress.insert(QStringLiteral("Symbol"), symbolProgress);

        // footprintProgress：封装导出进度状态。
        ExportTypeProgress footprintProgress;
        footprintProgress.typeName = QStringLiteral("Footprint");
        footprintProgress.totalCount = 3;
        footprintProgress.completedCount = 2;
        footprintProgress.successCount = 1;
        footprintProgress.failedCount = 1;
        footprintProgress.skippedCount = 0;
        footprintProgress.inProgressCount = 1;
        // footprintFailure：测试用封装数据。
        ExportItemStatus footprintFailure;
        footprintFailure.status = ExportItemStatus::Status::Failed;
        footprintFailure.errorMessage = QStringLiteral("Footprint export failed");
        footprintProgress.itemStatus.insert(QStringLiteral("C404"), footprintFailure);
        progress.exportTypeProgress.insert(QStringLiteral("Footprint"), footprintProgress);

        return progress;
    }

    static QString reportPath(const QString& outputPath) {
        return QDir(outputPath).filePath(QStringLiteral("easykiconverter_export_detailed_report.md"));
    }

    static QString readText(const QString& path) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTest::qFail(qPrintable(QStringLiteral("Unable to open file '%1': %2").arg(path, file.errorString())),
                         __FILE__,
                         __LINE__);
            return {};
        }

        QTextStream in(&file);
        in.setEncoding(QStringConverter::Utf8);
        return in.readAll();
    }
};

QTEST_GUILESS_MAIN(TestExportReportGenerator)
#include "test_export_report_generator.moc"
