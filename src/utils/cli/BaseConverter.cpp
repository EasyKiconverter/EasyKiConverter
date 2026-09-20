#include "BaseConverter.h"

#include "CliContext.h"
#include "CliExportWaiter.h"
#include "CliPrinter.h"
#include "services/export/ParallelExportService.h"

namespace EasyKiConverter {

/** @brief 创建基类并初始化 CLI 上下文与输出打印器。 */
BaseConverter::BaseConverter(CliContext* context, QObject* parent)
    // 初始化上下文和打印器，后续共享流程依赖这两个对象。
    : QObject(parent), m_context(context), m_printer(new CliPrinter(context->parser().isQuietMode())) {}

/** @brief 输出普通 CLI 消息并遵循静默模式配置。 */
void BaseConverter::printMessage(const QString& message) const {
    if (m_printer) {
        m_printer->println(message);
    }
}

/** @brief 输出 CLI 进度条并遵循静默模式配置。 */
void BaseConverter::printProgressBar(int progress) const {
    if (m_printer) {
        m_printer->printProgressBar(progress);
    }
}

/** @brief 执行所有 CLI 转换器共享的预加载、导出和结果汇总流程。 */
bool BaseConverter::runExport(const QStringList& componentIds) {
    printMessage(QCoreApplication::translate("CliConverter", "找到 %1 个元器件").arg(componentIds.size()));

    ExportOptions options = context()->createExportOptions();
    ParallelExportService* exportService = context()->exportService();
    exportService->setOptions(options);
    exportService->setOutputPath(context()->parser().outputDir());

    connect(exportService, &ParallelExportService::preloadCompleted, this, &BaseConverter::onPreloadCompleted);
    connect(exportService, &ParallelExportService::progressChanged, this, &BaseConverter::onProgressChanged);
    connect(exportService, &ParallelExportService::completed, this, &BaseConverter::onExportCompleted);
    connect(exportService, &ParallelExportService::failed, this, &BaseConverter::onExportFailed);

    CliExportWaiter::waitForPreload(exportService, componentIds);
    if (errorMessage().isEmpty()) {
        printMessage(QCoreApplication::translate("CliConverter", "预加载完成，开始导出..."));
        CliExportWaiter::waitForExport(exportService);
    }

    printMessage(QCoreApplication::translate("CliConverter", "\n转换完成: 成功 %1, 失败 %2")
                     .arg(m_successCount)
                     .arg(m_failedCount));
    return m_exportSuccess;
}

/** @brief 输出预加载阶段的成功和失败数量。 */
void BaseConverter::onPreloadCompleted(int successCount, int failedCount) {
    printMessage(
        QCoreApplication::translate("CliConverter", "预加载完成: 成功 %1, 失败 %2").arg(successCount).arg(failedCount));
}

/** @brief 根据 CLI 选项输出导出进度。 */
void BaseConverter::onProgressChanged(const ExportOverallProgress& progress) {
    if (context()->parser().showProgress()) {
        printProgressBar(progress.overallPercentage());
    }
    for (const ExportTypeProgress& typeProgress : progress.exportTypeProgress) {
        for (const QString& diagnostic : typeProgress.diagnostics) {
            if (m_reportedDiagnostics.contains(diagnostic))
                continue;
            m_reportedDiagnostics.append(diagnostic);
            printMessage(QCoreApplication::translate("CliConverter", "导出诊断: %1").arg(diagnostic));
        }
    }
}

/** @brief 保存导出完成统计并更新最终成功状态。 */
void BaseConverter::onExportCompleted(int successCount, int failedCount) {
    m_successCount = successCount;
    m_failedCount = failedCount;
    m_exportSuccess = failedCount == 0;
}

/** @brief 保存导出失败信息并终止成功状态。 */
void BaseConverter::onExportFailed(const QString& error) {
    setError(error);
    m_exportSuccess = false;
}

}  // namespace EasyKiConverter
