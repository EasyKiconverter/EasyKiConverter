#include "ExportProgressViewModel.h"

#include "ExportProgressResultsCoordinator.h"
#include "ExportProgressRetryCoordinator.h"
#include "ExportProgressStageCoordinator.h"
#include "services/export/ExportProgress.h"
#include "services/export/ParallelExportService.h"
#include "utils/FileUtils.h"
#include "utils/logging/LogMacros.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QStandardPaths>
#include <QTimer>
#include <QtGlobal>

namespace EasyKiConverter {

namespace {}  // namespace

/** @brief 创建并连接导出进度视图模型。 */
ExportProgressViewModel::ExportProgressViewModel(ParallelExportService* exportService,
                                                 ComponentService* componentService,
                                                 ComponentListViewModel* componentListViewModel,
                                                 QObject* parent)
    : QObject(parent)
    , m_exportService(exportService)
    , m_componentService(componentService)
    , m_componentListViewModel(componentListViewModel)
    , m_status("Ready")
    , m_progress(0)
    , m_isExporting(false)
    , m_totalCount(0)
    , m_successCount(0)
    , m_failureCount(0)
    , m_filterMode("all")
    , m_pendingUpdate(false)
    , m_fetchProgress(0)
    , m_processProgress(0)
    , m_writeProgress(0)
    , m_isStopping(false)
    , m_hasCompletedExport(false)
    , m_exportSymbolEnabled(true)
    , m_eagleCombinedLibrary(false)
    , m_exportFootprintEnabled(true)
    , m_exportModel3DEnabled(false)
    , m_exportPreviewEnabled(false)
    , m_exportDatasheetEnabled(false) {
    m_throttleTimer = new QTimer(this);
    m_throttleTimer->setInterval(150);
    m_throttleTimer->setSingleShot(true);
    connect(m_throttleTimer, &QTimer::timeout, this, &ExportProgressViewModel::flushPendingUpdates);

    if (m_exportService) {
        connect(m_exportService,
                &ParallelExportService::preloadProgressChanged,
                this,
                &ExportProgressViewModel::handlePreloadProgressChanged);
        connect(m_exportService,
                &ParallelExportService::preloadCompleted,
                this,
                &ExportProgressViewModel::handlePreloadCompleted);
        connect(m_exportService,
                &ParallelExportService::progressChanged,
                this,
                &ExportProgressViewModel::handleProgressChanged);
        connect(m_exportService,
                &ParallelExportService::itemStatusChanged,
                this,
                &ExportProgressViewModel::handleItemStatusChanged);
        connect(m_exportService,
                &ParallelExportService::typeCompleted,
                this,
                &ExportProgressViewModel::handleTypeCompleted);
        connect(m_exportService, &ParallelExportService::completed, this, &ExportProgressViewModel::handleCompleted);
        connect(m_exportService, &ParallelExportService::cancelled, this, &ExportProgressViewModel::handleCancelled);
        connect(m_exportService, &ParallelExportService::failed, this, &ExportProgressViewModel::handleFailed);
    }

    if (m_componentService) {
        connect(m_componentService,
                &ComponentService::cadDataReady,
                this,
                [this](const QString& componentId, const ComponentData& data) {
                    Q_UNUSED(componentId);
                    Q_UNUSED(data);
                });
    }
}

/** @brief 销毁导出进度视图模型。 */
ExportProgressViewModel::~ExportProgressViewModel() {}

/** @brief 初始化一轮导出的结果和阶段状态。 */
void ExportProgressViewModel::beginExportRun(const QStringList& componentIds, const QString& statusText) {
    m_componentIds = componentIds;
    const int newTotalCount = m_resultsList.isEmpty() ? componentIds.size() : m_resultsList.size();
    if (m_totalCount != newTotalCount) {
        m_totalCount = newTotalCount;
        emit totalCountChanged();
    }
    m_isStopping = false;
    m_pendingUpdate = false;
    if (m_throttleTimer) {
        m_throttleTimer->stop();
    }

    setHasCompletedExport(false);
    setIsExporting(true);
    setStatus(statusText);
    m_fetchProgress = 0;
    m_processProgress = 0;
    m_writeProgress = 0;
    setProgress(0);

    emit stageProgressChanged();
    emit isStoppingChanged();
    emit resultsListChanged();
    emit filteredResultsListChanged();
}

/** @brief 校验参数并启动组件预加载及导出流程。 */
void ExportProgressViewModel::startExport(const QStringList& componentIds,
                                          const QString& outputPath,
                                          const QString& libName,
                                          bool exportSymbol,
                                          bool exportFootprint,
                                          bool exportModel3D,
                                          int exportModel3DFormat,
                                          int exportModel3DPathMode,
                                          bool exportPreviewImages,
                                          bool exportDatasheet,
                                          bool overwriteExistingFiles,
                                          bool updateMode,
                                          bool debugMode,
                                          const QString& symbolLibraryDescription,
                                          const QString& footprintLibraryDescription,
                                          const QString& footprintLibraryKeywords,
                                          int targetFormat) {
    qDebug() << "ExportProgressViewModel: Starting export for" << componentIds.size() << "components"
             << "targetFormat:" << targetFormat;

    if (m_isExporting) {
        qWarning() << "Export already in progress";
        return;
    }

    if (!m_exportService) {
        qWarning() << "ParallelExportService is not available";
        setStatus("Export service not available");
        return;
    }

    if (componentIds.isEmpty()) {
        qWarning() << "No components to export";
        setStatus("No components to export");
        return;
    }

    // Store component IDs
    m_eagleCombinedLibrary =
        static_cast<TargetEdaFormat>(targetFormat) == TargetEdaFormat::Eagle && exportSymbol && exportFootprint;
    m_exportSymbolEnabled = exportSymbol;
    m_exportFootprintEnabled = exportFootprint;
    m_exportModel3DEnabled = exportModel3D;
    m_exportPreviewEnabled = exportPreviewImages;
    m_exportDatasheetEnabled = exportDatasheet;

    m_componentIds = componentIds;
    m_successCount = 0;
    m_failureCount = 0;
    m_pendingUpdate = false;
    if (m_throttleTimer) {
        m_throttleTimer->stop();
    }
    setHasCompletedExport(false);
    m_resultsList.clear();
    m_idToIndexMap.clear();

    for (int i = 0; i < componentIds.size(); ++i) {
        m_idToIndexMap[componentIds[i]] = i;
        QVariantMap item;
        item["componentId"] = componentIds[i];
        item["status"] = "pending";
        item["symbolSuccess"] = !m_exportSymbolEnabled;
        item["footprintSuccess"] = !m_exportFootprintEnabled;
        item["model3DSuccess"] = !m_exportModel3DEnabled;
        item["previewSuccess"] = !m_exportPreviewEnabled;
        item["datasheetSuccess"] = !m_exportDatasheetEnabled;
        item["symbolStatus"] = m_exportSymbolEnabled ? "pending" : "disabled";
        item["footprintStatus"] = m_exportFootprintEnabled ? "pending" : "disabled";
        item["model3DStatus"] = m_exportModel3DEnabled ? "pending" : "disabled";
        item["previewStatus"] = m_exportPreviewEnabled ? "pending" : "disabled";
        item["datasheetStatus"] = m_exportDatasheetEnabled ? "pending" : "disabled";
        m_resultsList.append(item);
    }

    // Build export options
    ExportOptions options;
    options.outputPath = outputPath;
    options.libName = libName;
    options.exportSymbol = exportSymbol;
    options.exportFootprint = exportFootprint;
    options.exportModel3D = exportModel3D;
    options.exportModel3DFormat = exportModel3DFormat;
    options.exportModel3DPathMode = ExportOptions::normalizePathMode(exportModel3DPathMode);
    options.exportPreviewImages = exportPreviewImages;
    options.exportDatasheet = exportDatasheet;
    options.overwriteExistingFiles = overwriteExistingFiles;
    options.updateMode = updateMode;
    options.debugMode = debugMode;
    options.exportSymbolDescription = true;
    options.exportFootprintDescription = true;
    options.symbolLibraryDescription = symbolLibraryDescription;
    options.footprintLibraryDescription = footprintLibraryDescription;
    options.footprintLibraryKeywords = footprintLibraryKeywords;
    options.targetFormat = static_cast<TargetEdaFormat>(targetFormat);

    m_exportService->setOptions(options);
    m_exportService->setOutputPath(outputPath);

    beginExportRun(componentIds, QStringLiteral("Preloading component data..."));
    emit successCountChanged();
    emit failureCountChanged();

    // Start preload - actual export will begin after preload completes
    m_exportService->startPreload(componentIds);
}

/** @brief 请求取消当前导出任务。 */
void ExportProgressViewModel::cancelExport() {
    qDebug() << "ExportProgressViewModel: Cancelling export";

    if (!m_exportService) {
        return;
    }

    if (!m_isStopping) {
        m_isStopping = true;
        emit isStoppingChanged();
    }
    m_exportService->cancelExport();
}

/** @brief 更新预览图和数据手册的导出状态。 */
void ExportProgressViewModel::updateComponentExportStatus(const QString& componentId,
                                                          int previewImageExported,
                                                          int datasheetExported) {
    if (!m_idToIndexMap.contains(componentId)) {
        return;
    }

    const int index = m_idToIndexMap.value(componentId);
    QVariantMap result = m_resultsList[index].toMap();

    if (previewImageExported >= 0) {
        result["previewSuccess"] = (previewImageExported > 0);
        result["previewStatus"] = (previewImageExported > 0) ? "success" : "failed";
    }
    if (datasheetExported >= 0) {
        result["datasheetSuccess"] = (datasheetExported > 0);
        result["datasheetStatus"] = (datasheetExported > 0) ? "success" : "failed";
    }

    updateOverallItemStatus(result);
    m_resultsList[index] = result;
    markResultsDirty();
}

/** @brief 响应窗口关闭请求并取消进行中的导出。 */
void ExportProgressViewModel::handleCloseRequest() {
    if (m_isExporting) {
        cancelExport();
    }
}

/** @brief 响应组件预加载进度变化。 */
void ExportProgressViewModel::handlePreloadProgressChanged(const PreloadProgress& progress) {
    ExportProgressStageCoordinator::handlePreloadProgress(*this, progress);
}

/** @brief 响应组件预加载完成并安排正式导出。 */
void ExportProgressViewModel::handlePreloadCompleted(int successCount, int failedCount) {
    qInfo() << "ExportProgressViewModel: Preload completed. Success:" << successCount << "Failed:" << failedCount;

    if (failedCount > 0) {
        setStatus(QString("Preload completed with %1 errors").arg(failedCount));
    } else {
        setStatus("Starting export...");
    }

    // Start the actual export after preload completes
    // Use QTimer::singleShot to defer to event loop, preventing UI freeze during retry
    // This allows pending signals/events to be processed before starting heavy export work
    if (m_exportService) {
        QTimer::singleShot(0, this, [this]() {
            if (m_exportService) {
                m_exportService->startExport();
            }
        });
    }
}

/** @brief 响应导出阶段进度并更新三段式进度条。 */
void ExportProgressViewModel::handleProgressChanged(const ExportOverallProgress& progress) {
    ExportProgressStageCoordinator::handleProgress(*this, progress);
}

/** @brief 响应单个组件的类型导出状态变化。 */
void ExportProgressViewModel::handleItemStatusChanged(const QString& componentId,
                                                      const QString& typeName,
                                                      const ExportItemStatus& status) {
    if (m_idToIndexMap.contains(componentId)) {
        int index = m_idToIndexMap[componentId];
        QVariantMap result = m_resultsList[index].toMap();

        // Update individual type success flags
        if (typeName == "Symbol") {
            result["symbolSuccess"] = (status.status == ExportItemStatus::Status::Success);
        } else if (typeName == "Footprint") {
            result["footprintSuccess"] = (status.status == ExportItemStatus::Status::Success);
        } else if (typeName == "Model3D") {
            result["model3DSuccess"] = (status.status == ExportItemStatus::Status::Success);
        } else if (typeName == "PreviewImages") {
            result["previewSuccess"] = (status.status == ExportItemStatus::Status::Success);
        } else if (typeName == "Datasheet") {
            result["datasheetSuccess"] = (status.status == ExportItemStatus::Status::Success);
        }

        if (status.status == ExportItemStatus::Status::Failed) {
            result["error"] = status.errorMessage;
        }

        QString statusText = "pending";
        // 将内部枚举状态转换为供 QML 使用的稳定字符串。
        switch (status.status) {
            case ExportItemStatus::Status::Pending:
                statusText = "pending";
                break;
            case ExportItemStatus::Status::InProgress:
                statusText = "in_progress";
                break;
            case ExportItemStatus::Status::Success:
                statusText = "success";
                break;
            case ExportItemStatus::Status::Failed:
                statusText = "failed";
                break;
            case ExportItemStatus::Status::Skipped:
                statusText = "skipped";
                break;
        }
        result[typeStatusKey(typeName)] = statusText;
        if (m_eagleCombinedLibrary && typeName == QStringLiteral("Footprint")) {
            // Eagle 的完整 .lbr 在封装阶段同时写入符号；让符号状态跟随同一原子导出结果。
            result["symbolSuccess"] = result["footprintSuccess"];
            result["symbolStatus"] = statusText;
        }

        updateOverallItemStatus(result);

        m_resultsList[index] = result;

        markResultsDirty();
    }
}

/** @brief 记录单个导出类型完成事件。 */
void ExportProgressViewModel::handleTypeCompleted(const QString& typeName,
                                                  int successCount,
                                                  int failedCount,
                                                  int skippedCount) {
    Q_UNUSED(typeName);
    Q_UNUSED(successCount);
    Q_UNUSED(failedCount);
    Q_UNUSED(skippedCount);
    qInfo() << "Type completed:" << typeName << "success=" << successCount << "failed=" << failedCount;
}

/** @brief 响应整个导出流程完成。 */
void ExportProgressViewModel::handleCompleted(int successCount, int failedCount) {
    qInfo() << "Export completed: success=" << successCount << "failed=" << failedCount;
    Q_UNUSED(successCount);
    Q_UNUSED(failedCount);
    if (m_isStopping) {
        m_isStopping = false;
        emit isStoppingChanged();
    }
    setIsExporting(false);
    setHasCompletedExport(true);
    setProgress(100);
    updateResultsList();
    setStatus(m_failureCount > 0 ? QString("Completed with %1 errors").arg(m_failureCount)
                                 : "Export completed successfully");
    flushPendingUpdates();
}

/** @brief 响应导出取消并标记未完成结果。 */
void ExportProgressViewModel::handleCancelled() {
    qInfo() << "Export cancelled";

    // 将所有未完成的项标记为失败，并同步各类型状态，避免小球状态残留
    for (int i = 0; i < m_resultsList.size(); ++i) {
        QVariantMap result = m_resultsList[i].toMap();
        QString currentStatus = result["status"].toString();
        if (currentStatus == "pending" || currentStatus == "in_progress") {
            result["error"] = "Export cancelled";
            const QStringList statusKeys = {
                QStringLiteral("symbolStatus"),
                QStringLiteral("footprintStatus"),
                QStringLiteral("model3DStatus"),
                QStringLiteral("previewStatus"),
                QStringLiteral("datasheetStatus"),
            };
            for (const QString& key : statusKeys) {
                const QString currentTypeStatus = result.value(key).toString();
                if (currentTypeStatus == "pending" || currentTypeStatus == "in_progress") {
                    result[key] = "failed";
                }
            }
            updateOverallItemStatus(result);
            m_resultsList[i] = result;
        }
    }

    if (m_isStopping) {
        m_isStopping = false;
        emit isStoppingChanged();
    }
    setIsExporting(false);
    setHasCompletedExport(false);
    setProgress(0);
    setStatus("Export cancelled");
    m_fetchProgress = 0;
    m_processProgress = 0;
    m_writeProgress = 0;
    updateResultsList();  // 更新失败数量统计
    emit stageProgressChanged();
    flushPendingUpdates();
}

/** @brief 响应导出失败并重置阶段进度。 */
void ExportProgressViewModel::handleFailed(const QString& error) {
    qWarning() << "Export failed:" << error;
    if (m_isStopping) {
        m_isStopping = false;
        emit isStoppingChanged();
    }
    setIsExporting(false);
    setHasCompletedExport(false);
    setProgress(0);
    m_fetchProgress = 0;
    m_processProgress = 0;
    m_writeProgress = 0;
    updateResultsList();  // 更新失败数量统计
    emit stageProgressChanged();
    setStatus(QString("Export failed: %1").arg(error));
}

/** @brief 刷新节流期间积累的结果列表更新。 */
void ExportProgressViewModel::flushPendingUpdates() {
    if (m_pendingUpdate) {
        m_pendingUpdate = false;
        updateResultsList();
        emit resultsListChanged();
        emit filteredResultsListChanged();
    }
}

/** @brief 更新结果统计数量。 */
void ExportProgressViewModel::updateResultsList() {
    ExportProgressResultsCoordinator::updateResultsList(*this);
}

/** @brief 通知过滤结果列表发生变化。 */
void ExportProgressViewModel::updateFilteredResults() {
    ExportProgressResultsCoordinator::updateFilteredResults(*this);
}

/** @brief 返回当前过滤模式下的结果列表。 */
QVariantList ExportProgressViewModel::filteredResultsList() const {
    return ExportProgressResultsCoordinator::filteredResultsList(*this);
}

/** @brief 返回当前过滤模式下的成功数量。 */
int ExportProgressViewModel::filteredSuccessCount() const {
    return ExportProgressResultsCoordinator::filteredSuccessCount(*this);
}

/** @brief 返回当前过滤模式下的失败数量。 */
int ExportProgressViewModel::filteredFailedCount() const {
    return ExportProgressResultsCoordinator::filteredFailedCount(*this);
}

/** @brief 返回当前过滤模式下的待处理数量。 */
int ExportProgressViewModel::filteredPendingCount() const {
    return ExportProgressResultsCoordinator::filteredPendingCount(*this);
}

/** @brief 返回符号导出成功数量。 */
int ExportProgressViewModel::symbolSuccessCount() const {
    return ExportProgressResultsCoordinator::typeSuccessCount(*this, QStringLiteral("symbolStatus"));
}

/** @brief 返回封装导出成功数量。 */
int ExportProgressViewModel::footprintSuccessCount() const {
    return ExportProgressResultsCoordinator::typeSuccessCount(*this, QStringLiteral("footprintStatus"));
}

/** @brief 返回三维模型导出成功数量。 */
int ExportProgressViewModel::model3DSuccessCount() const {
    return ExportProgressResultsCoordinator::typeSuccessCount(*this, QStringLiteral("model3DStatus"));
}

/** @brief 返回预览图导出成功数量。 */
int ExportProgressViewModel::previewSuccessCount() const {
    return ExportProgressResultsCoordinator::typeSuccessCount(*this, QStringLiteral("previewStatus"));
}

/** @brief 返回数据手册导出成功数量。 */
int ExportProgressViewModel::datasheetSuccessCount() const {
    return ExportProgressResultsCoordinator::typeSuccessCount(*this, QStringLiteral("datasheetStatus"));
}

/** @brief 设置结果过滤模式并通知 QML。 */
void ExportProgressViewModel::setFilterMode(const QString& mode) {
    if (m_filterMode != mode) {
        m_filterMode = mode;
        emit filterModeChanged();
        updateFilteredResults();
    }
}

/** @brief 返回最近一次导出的目录。 */
QString ExportProgressViewModel::getLastExportedPath() const {
    if (m_exportService) {
        return m_exportService->outputPath();
    }
    return QString();
}

/** @brief 打开最近一次导出的目录。 */
bool ExportProgressViewModel::openLastExportedFolder() {
    QString path = getLastExportedPath();
    if (path.isEmpty()) {
        return false;
    }

    if (!QDir(path).exists()) {
        QDir().mkpath(path);
    }

    FileUtils utils;
    return utils.openFolder(path);
}

/** @brief 取消当前导出并清理组件缓存。 */
void ExportProgressViewModel::clearCache() {
    // 中止进行中的导出
    if (m_isExporting) {
        cancelExport();
    }
    // 重置 ComponentService 批处理状态
    if (m_componentService) {
        m_componentService->abortBatchFetch();
    }
    // 清空 L1 内存 + L2 磁盘缓存
    ComponentCacheService::instance()->clearAllCache();
}

/** @brief 重置导出结果、进度和过滤状态。 */
void ExportProgressViewModel::resetExport() {
    if (m_throttleTimer) {
        m_throttleTimer->stop();
    }
    m_pendingUpdate = false;
    setIsExporting(false);
    setProgress(0);
    setStatus("Ready");
    m_resultsList.clear();
    m_idToIndexMap.clear();
    m_filterMode = "all";
    m_totalCount = 0;
    m_successCount = 0;
    m_failureCount = 0;
    m_isStopping = false;
    setHasCompletedExport(false);
    m_fetchProgress = 0;
    m_processProgress = 0;
    m_writeProgress = 0;

    emit resultsListChanged();
    emit filterModeChanged();
    emit filteredResultsListChanged();
    emit isStoppingChanged();
    emit successCountChanged();
    emit failureCountChanged();
    emit totalCountChanged();
    emit stageProgressChanged();
}

/** @brief 重试指定组件的失败导出。 */
void ExportProgressViewModel::retryComponent(const QString& componentId) {
    ExportProgressRetryCoordinator::retryComponent(*this, componentId);
}

/** @brief 重试所有失败组件。 */
void ExportProgressViewModel::retryFailedComponents() {
    ExportProgressRetryCoordinator::retryFailedComponents(*this);
}

/** @brief 从结果列表移除指定组件。 */
void ExportProgressViewModel::removeResult(const QString& componentId) {
    if (m_idToIndexMap.contains(componentId)) {
        int index = m_idToIndexMap[componentId];
        m_resultsList.removeAt(index);
        m_idToIndexMap.remove(componentId);
        // Reindex remaining items
        for (int i = index; i < m_resultsList.size(); ++i) {
            QString id = m_resultsList[i].toMap().value("componentId").toString();
            m_idToIndexMap[id] = i;
        }
        // Update counts first, then notify UI of list change
        markResultsDirty();
    }
}

/** @brief 返回指定组件的导出状态。 */
QVariantMap ExportProgressViewModel::getComponentExportStatus(const QString& componentId) const {
    if (m_idToIndexMap.contains(componentId)) {
        int index = m_idToIndexMap[componentId];
        if (index >= 0 && index < m_resultsList.size()) {
            return m_resultsList[index].toMap();
        }
    }
    return {};
}

/** @brief 设置状态文本并发送变化信号。 */
void ExportProgressViewModel::setStatus(const QString& status) {
    if (m_status != status) {
        m_status = status;
        emit statusChanged();
    }
}

/** @brief 设置导出运行状态并发送变化信号。 */
void ExportProgressViewModel::setIsExporting(bool exporting) {
    if (m_isExporting != exporting) {
        m_isExporting = exporting;
        emit isExportingChanged();
    }
}

/** @brief 设置总体进度并发送变化信号。 */
void ExportProgressViewModel::setProgress(int progress) {
    if (m_progress != progress) {
        m_progress = progress;
        emit progressChanged();
    }
}

/** @brief 设置导出完成标记并发送变化信号。 */
void ExportProgressViewModel::setHasCompletedExport(bool completed) {
    if (m_hasCompletedExport != completed) {
        m_hasCompletedExport = completed;
        emit hasCompletedExportChanged();
    }
}

/** @brief 返回导出类型对应的结果状态字段。 */
QString ExportProgressViewModel::typeStatusKey(const QString& typeName) const {
    return ExportProgressResultsCoordinator::typeStatusKey(typeName);
}

/** @brief 根据类型状态更新组件总体状态。 */
void ExportProgressViewModel::updateOverallItemStatus(QVariantMap& result) const {
    ExportProgressResultsCoordinator::updateOverallItemStatus(*this, result);
}

/** @brief 将结果项重置为可重试的初始状态。 */
void ExportProgressViewModel::resetItemForRetry(QVariantMap& result) const {
    ExportProgressResultsCoordinator::resetItemForRetry(*this, result);
}

/** @brief 标记结果列表待刷新并启动节流计时器。 */
void ExportProgressViewModel::markResultsDirty() {
    m_pendingUpdate = true;
    m_throttleTimer->start();
}

}  // namespace EasyKiConverter
