#include "ComponentConverter.h"

#include "CliContext.h"
#include "services/BomParser.h"
#include "services/ComponentService.h"
#include "services/export/ParallelExportService.h"

#include <QCoreApplication>
#include <QEventLoop>

namespace EasyKiConverter {

/**
 * @brief 创建单元件转换器。
 * @param context CLI 上下文。
 * @param parent Qt 对象父级。
 */
ComponentConverter::ComponentConverter(CliContext* context, QObject* parent) : BaseConverter(context, parent) {}

/**
 * @brief 执行单个元器件的预加载和导出。
 * @return 导出成功时返回 true。
 */
bool ComponentConverter::execute() {
    printMessage(QCoreApplication::translate("CliConverter", "开始转换单个元器件..."));

    QString componentId = context()->parser().componentId();  // 当前待转换的 LCSC 编号。
    if (componentId.isEmpty()) {
        setError(QCoreApplication::translate("CliConverter", "未指定元器件编号"));
        return false;
    }

    if (!BomParser::validateId(componentId)) {
        setError(QCoreApplication::translate("CliConverter", "元器件编号格式无效: %1").arg(componentId));
        return false;
    }

    printMessage(QCoreApplication::translate("CliConverter", "元器件编号: %1").arg(componentId));

    // 创建导出选项
    ExportOptions options = context()->createExportOptions();

    // 设置导出服务
    ParallelExportService* exportService = context()->exportService();
    exportService->setOptions(options);
    exportService->setOutputPath(context()->parser().outputDir());

    // 连接信号
    connect(exportService, &ParallelExportService::preloadCompleted, this, &ComponentConverter::onExportCompleted);
    connect(exportService, &ParallelExportService::completed, this, &ComponentConverter::onExportCompleted);
    connect(exportService, &ParallelExportService::failed, this, &ComponentConverter::onExportFailed);

    // 创建事件循环等待预加载完成
    QEventLoop preloadLoop;
    connect(exportService, &ParallelExportService::preloadCompleted, &preloadLoop, &QEventLoop::quit);
    connect(exportService, &ParallelExportService::failed, &preloadLoop, &QEventLoop::quit);

    // 开始预加载
    QStringList componentIds;
    componentIds.append(componentId);
    exportService->startPreload(componentIds);

    // 等待预加载完成
    preloadLoop.exec();

    // 检查是否预加载成功
    if (errorMessage().isEmpty()) {
        printMessage(QCoreApplication::translate("CliConverter", "预加载完成，开始导出..."));

        // 创建事件循环等待导出完成
        QEventLoop exportLoop;
        connect(exportService, &ParallelExportService::completed, &exportLoop, &QEventLoop::quit);
        connect(exportService, &ParallelExportService::failed, &exportLoop, &QEventLoop::quit);

        // 开始导出
        exportService->startExport();

        // 等待导出完成
        exportLoop.exec();
    }

    printMessage(QCoreApplication::translate("CliConverter", "转换完成: 成功 %1, 失败 %2")
                     .arg(m_successCount)
                     .arg(m_failedCount));

    return m_exportSuccess;
}

/**
 * @brief 处理导出完成信号。
 * @param successCount 成功数量。
 * @param failedCount 失败数量。
 */
void ComponentConverter::onExportCompleted(int successCount, int failedCount) {
    m_successCount = successCount;
    m_failedCount = failedCount;
    m_exportSuccess = (failedCount == 0);
    m_exportFinished = true;
}

/**
 * @brief 处理导出失败信号。
 * @param error 导出错误信息。
 */
void ComponentConverter::onExportFailed(const QString& error) {
    setError(error);
    m_exportSuccess = false;
    m_exportFinished = true;
}

}  // namespace EasyKiConverter
