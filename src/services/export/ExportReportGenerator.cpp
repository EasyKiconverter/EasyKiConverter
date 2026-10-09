#include "services/export/ExportReportGenerator.h"

#include "ExportProgress.h"
#include "core/network/NetworkClient.h"
#include "utils/logging/LogMacros.h"

#include <QDateTime>
#include <QDir>
#include <QSaveFile>
#include <QStringConverter>
#include <QTextStream>

namespace EasyKiConverter {

/**
 * @brief 记录网络运行时统计，帮助导出报告关联网络阶段行为。
 * @param context 当前导出阶段上下文。
 */
void ExportReportGenerator::logNetworkStats(const QString& context) {
    const QString snapshot = NetworkClient::instance().formatRuntimeStats();  ///< 当前网络运行时统计快照。
    qInfo().noquote() << QStringLiteral("ParallelExportService network runtime stats [%1]\n%2").arg(context, snapshot);
}

/**
 * @brief 在调试模式下写入导出详细报告。
 * @param reason 触发报告写入的原因。
 * @param options 当前导出选项。
 * @param progress 当前整体导出进度。
 */
void ExportReportGenerator::writeDetailedReport(const QString& reason,
                                                const ExportOptions& options,
                                                const ExportOverallProgress& progress) {
    // 只在调试模式下生成详细报告
    if (!options.debugMode) {
        return;
    }

    if (options.outputPath.trimmed().isEmpty()) {
        return;
    }

    QDir outputDir(options.outputPath);  ///< 详细报告输出目录对象。
    if (!outputDir.exists() && !outputDir.mkpath(QStringLiteral("."))) {
        qWarning() << "ParallelExportService: Failed to create diagnostics report directory:" << outputDir.path();
        return;
    }

    const QString reportPath =
        outputDir.filePath(QStringLiteral("easykiconverter_export_detailed_report.md"));  ///< 详细报告文件路径。
    QSaveFile file(reportPath);  ///< 以原子方式写入详细报告的文件对象。
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "ParallelExportService: Failed to open export detailed report for writing:" << reportPath;
        return;
    }

    QTextStream out(&file);  ///< 详细报告文本输出流。
    out.setEncoding(QStringConverter::Utf8);
    out << "# EasyKiConverter Export Detailed Report\n\n";
    out << "- Generated: " << QDateTime::currentDateTime().toString(Qt::ISODate) << "\n";
    out << "- Reason: " << reason << "\n";
    out << "- Weak network support: " << (options.weakNetworkSupport ? "enabled" : "disabled") << "\n";
    out << "- Output path: " << options.outputPath << "\n";
    out << "- Current stage: " << static_cast<int>(progress.currentStage) << "\n";
    out << "- Start time: " << progress.startTime.toString(Qt::ISODate) << "\n";
    out << "- End time: " << progress.endTime.toString(Qt::ISODate) << "\n";
    out << "- Total components: " << progress.totalComponents << "\n\n";

    out << "## Export Options\n\n";
    out << "- Library name: " << options.libName << "\n";
    out << "- Export symbol: " << (options.exportSymbol ? "yes" : "no") << "\n";
    out << "- Export footprint: " << (options.exportFootprint ? "yes" : "no") << "\n";
    out << "- Export 3D model: " << (options.exportModel3D ? "yes" : "no") << "\n";
    out << "- Export preview images: " << (options.exportPreviewImages ? "yes" : "no") << "\n";
    out << "- Export datasheet: " << (options.exportDatasheet ? "yes" : "no") << "\n";
    out << "- Overwrite existing files: " << (options.overwriteExistingFiles ? "yes" : "no") << "\n";
    out << "- Update mode: " << (options.updateMode ? "yes" : "no") << "\n";
    out << "- Debug mode: " << (options.debugMode ? "yes" : "no") << "\n\n";

    out << "## Preload\n\n";
    out << "- Completed: " << progress.preloadProgress.completedCount << "/" << progress.preloadProgress.totalCount
        << "\n";
    out << "- Success: " << progress.preloadProgress.successCount << "\n";
    out << "- Failed: " << progress.preloadProgress.failedCount << "\n";
    out << "- In progress: " << progress.preloadProgress.inProgressCount << "\n";
    if (!progress.preloadProgress.failedComponents.isEmpty()) {
        out << "\n### Preload Failures\n\n";
        for (auto it = progress.preloadProgress.failedComponents.cbegin();
             it != progress.preloadProgress.failedComponents.cend();
             ++it) {
            out << "- `" << it.key() << "`: " << it.value() << "\n";
        }
    }

    out << "\n## Export Type Progress\n\n";
    for (auto it = progress.exportTypeProgress.cbegin(); it != progress.exportTypeProgress.cend(); ++it) {
        const ExportTypeProgress& typeProgress = it.value();  ///< 当前导出类型的进度。
        out << "### " << it.key() << "\n\n";
        out << "- Completed: " << typeProgress.completedCount << "/" << typeProgress.totalCount << "\n";
        out << "- Success: " << typeProgress.successCount << "\n";
        out << "- Failed: " << typeProgress.failedCount << "\n";
        out << "- Skipped: " << typeProgress.skippedCount << "\n";
        out << "- In progress: " << typeProgress.inProgressCount << "\n";
        if (!typeProgress.diagnostics.isEmpty()) {
            out << "\n#### Exporter Diagnostics\n\n";
            for (const QString& diagnostic : typeProgress.diagnostics)
                out << "- " << diagnostic << "\n";
        }
        bool hasNonSuccessItems = false;  ///< 当前导出类型是否存在失败或跳过项。
        for (auto statusIt = typeProgress.itemStatus.cbegin(); statusIt != typeProgress.itemStatus.cend();
             ++statusIt) {  ///< 当前元器件状态。
            const auto status = statusIt.value().status;  ///< 当前元器件的结果状态。
            if (status == ExportItemStatus::Status::Failed || status == ExportItemStatus::Status::Skipped) {
                hasNonSuccessItems = true;
                break;
            }
        }
        if (hasNonSuccessItems) {
            out << "\n#### Failed or Skipped Items\n\n";
            for (auto statusIt = typeProgress.itemStatus.cbegin(); statusIt != typeProgress.itemStatus.cend();
                 ++statusIt) {
                const ExportItemStatus& status = statusIt.value();  ///< 当前失败或跳过的元器件状态。
                QString statusName;  ///< 面向报告的状态名称。
                if (status.status == ExportItemStatus::Status::Failed)
                    statusName = QStringLiteral("failed");
                else if (status.status == ExportItemStatus::Status::Skipped)
                    statusName = QStringLiteral("skipped");
                else
                    continue;

                // 文本。
                const QString reasonText = status.errorMessage.isEmpty()
                                               ? QStringLiteral("未提供原因")
                                               : status.errorMessage;  ///< 面向报告的失败原因。
                out << "- `" << statusIt.key() << "`: " << statusName << " — " << reasonText << "\n";
            }
        }
        bool hasDiagnostics = false;  ///< 当前导出类型是否存在输入诊断。
        for (auto statusIt = typeProgress.itemStatus.cbegin(); statusIt != typeProgress.itemStatus.cend(); ++statusIt) {
            if (!statusIt.value().diagnostics.isEmpty()) {
                hasDiagnostics = true;
                break;
            }
        }
        if (hasDiagnostics) {
            out << "\n#### Input Diagnostics\n\n";
            for (auto statusIt = typeProgress.itemStatus.cbegin(); statusIt != typeProgress.itemStatus.cend();
                 ++statusIt) {
                if (statusIt.value().diagnostics.isEmpty())
                    continue;
                out << "- `" << statusIt.key() << "`:\n";
                for (const QString& diagnostic : statusIt.value().diagnostics)
                    out << "  - " << diagnostic << "\n";
            }
        }
    }

    out << "\n## Weak Network Diagnostics\n\n";
    out << "### Network Runtime Stats\n\n```\n";
    out << NetworkClient::instance().formatRuntimeStats() << "\n";
    out << "```\n";

    if (!file.commit()) {
        qWarning() << "ParallelExportService: Failed to commit export detailed report:" << reportPath;
        return;
    }

    qInfo() << "ParallelExportService: Export detailed report written to" << reportPath;
}

}  // namespace EasyKiConverter
