#pragma once

#include "services/ComponentCacheService.h"
#include "services/ComponentService.h"
#include "services/export/ParallelExportService.h"
#include "ui/viewmodels/ComponentListViewModel.h"

#include <QHash>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QUrl>
#include <QVariantList>

namespace EasyKiConverter {

struct ExportOverallProgress;
struct ExportTypeProgress;
struct ExportItemStatus;
struct ExportStatistics;
class ExportProgressResultsCoordinator;
class ExportProgressRetryCoordinator;
class ExportProgressStageCoordinator;

/**
 * @brief 导出进度视图模型
 *
 * 负责显示和管理导出进度的 UI 状态和操作
 */
class ExportProgressViewModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(int progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(bool isExporting READ isExporting NOTIFY isExportingChanged)
    Q_PROPERTY(int successCount READ successCount NOTIFY successCountChanged)
    Q_PROPERTY(int failureCount READ failureCount NOTIFY failureCountChanged)
    Q_PROPERTY(int totalCount READ totalCount NOTIFY totalCountChanged)
    Q_PROPERTY(QString filterMode READ filterMode WRITE setFilterMode NOTIFY filterModeChanged)
    Q_PROPERTY(QVariantList resultsList READ resultsList NOTIFY resultsListChanged)
    Q_PROPERTY(QVariantList filteredResultsList READ filteredResultsList NOTIFY filteredResultsListChanged)
    Q_PROPERTY(int filteredSuccessCount READ filteredSuccessCount NOTIFY filteredResultsListChanged)
    Q_PROPERTY(int filteredFailedCount READ filteredFailedCount NOTIFY filteredResultsListChanged)
    Q_PROPERTY(int filteredPendingCount READ filteredPendingCount NOTIFY filteredResultsListChanged)
    Q_PROPERTY(int fetchProgress READ fetchProgress NOTIFY stageProgressChanged)
    Q_PROPERTY(int processProgress READ processProgress NOTIFY stageProgressChanged)
    Q_PROPERTY(int writeProgress READ writeProgress NOTIFY stageProgressChanged)
    Q_PROPERTY(bool isStopping READ isStopping NOTIFY isStoppingChanged)
    Q_PROPERTY(int statisticsTotal READ statisticsTotal NOTIFY totalCountChanged)
    Q_PROPERTY(bool hasCompletedExport READ hasCompletedExport NOTIFY hasCompletedExportChanged)
    Q_PROPERTY(bool retrySupported READ retrySupported NOTIFY retrySupportedChanged)
    Q_PROPERTY(QString cacheDirUrl READ cacheDirUrl CONSTANT)
    Q_PROPERTY(int symbolSuccessCount READ symbolSuccessCount NOTIFY resultsListChanged)
    Q_PROPERTY(int footprintSuccessCount READ footprintSuccessCount NOTIFY resultsListChanged)
    Q_PROPERTY(int model3DSuccessCount READ model3DSuccessCount NOTIFY resultsListChanged)
    Q_PROPERTY(int previewSuccessCount READ previewSuccessCount NOTIFY resultsListChanged)
    Q_PROPERTY(int datasheetSuccessCount READ datasheetSuccessCount NOTIFY resultsListChanged)

public:
    /**
     * @brief 创建导出进度视图模型。
     * @param exportService 并行导出服务实例。
     * @param componentService 元器件数据服务实例。
     * @param componentListViewModel 元件列表视图模型。
     * @param parent Qt 父对象。
     */
    explicit ExportProgressViewModel(ParallelExportService* exportService,
                                     ComponentService* componentService,
                                     ComponentListViewModel* componentListViewModel,
                                     QObject* parent = nullptr);
    ~ExportProgressViewModel() override;

    /**
     * @brief 返回总体导出进度百分比。
     */
    int progress() const {
        return m_progress;
    }

    /**
     * @brief 返回当前导出状态文本。
     */
    QString status() const {
        return m_status;
    }

    /**
     * @brief 返回是否正在导出。
     */
    bool isExporting() const {
        return m_isExporting;
    }

    /**
     * @brief 返回成功结果数量。
     */
    int successCount() const {
        return m_successCount;
    }

    /**
     * @brief 返回失败结果数量。
     */
    int failureCount() const {
        return m_failureCount;
    }

    /**
     * @brief 返回结果总数量。
     */
    int totalCount() const {
        return m_totalCount;
    }

    /**
     * @brief 返回当前结果过滤模式。
     */
    QString filterMode() const {
        return m_filterMode;
    }

    /**
     * @brief 返回全部导出结果列表。
     */
    QVariantList resultsList() const {
        return m_resultsList;
    }

    QVariantList filteredResultsList() const;
    int filteredSuccessCount() const;
    int filteredFailedCount() const;
    int filteredPendingCount() const;

    /**
     * @brief 返回抓取阶段进度。
     */
    int fetchProgress() const {
        return m_fetchProgress;
    }

    /**
     * @brief 返回处理阶段进度。
     */
    int processProgress() const {
        return m_processProgress;
    }

    /**
     * @brief 返回写入阶段进度。
     */
    int writeProgress() const {
        return m_writeProgress;
    }

    /**
     * @brief 返回是否正在停止导出。
     */
    bool isStopping() const {
        return m_isStopping;
    }

    /**
     * @brief 返回统计面板显示的总数量。
     */
    int statisticsTotal() const {
        return m_totalCount;
    }

    /**
     * @brief 返回最近一次导出是否已完成。
     */
    bool hasCompletedExport() const {
        return m_hasCompletedExport;
    }

    /**
     * @brief 返回当前目标是否支持失败项重试。
     */
    bool retrySupported() const {
        return m_retrySupported;
    }

    int symbolSuccessCount() const;
    int footprintSuccessCount() const;
    int model3DSuccessCount() const;
    int previewSuccessCount() const;
    int datasheetSuccessCount() const;

    /**
     * @brief 返回供 QML 使用的缓存目录 URL。
     */
    QString cacheDirUrl() const {
        auto cacheDir = ComponentCacheService::instance()->cacheDir();  // 当前缓存目录。
        if (cacheDir.isEmpty()) {
            return QString();
        }
        return QUrl::fromLocalFile(cacheDir).toString();
    }

    Q_INVOKABLE QString getLastExportedPath() const;
    Q_INVOKABLE bool openLastExportedFolder();
    Q_INVOKABLE void clearCache();
    /**
     * @brief 返回清空缓存确认对话框中的可处理条目数量。
     */
    Q_INVOKABLE int cacheEntryCount() const;
    Q_INVOKABLE void resetExport();
    Q_INVOKABLE void setFilterMode(const QString& mode);
    Q_INVOKABLE void retryComponent(const QString& componentId);
    Q_INVOKABLE void retryFailedComponents();
    Q_INVOKABLE void removeResult(const QString& componentId);
    Q_INVOKABLE QVariantMap getComponentExportStatus(const QString& componentId) const;

public slots:
    /**
     * @brief 按当前设置启动指定元件的导出。
     * @param componentIds 元器件编号列表。
     * @param outputPath 导出输出目录。
     * @param libName 输出库名称。
     * @param exportSymbol 是否导出符号库。
     * @param exportFootprint 是否导出封装库。
     * @param exportModel3D 是否导出三维模型。
     * @param exportModel3DFormat 三维模型格式。
     * @param exportModel3DPathMode 三维模型路径模式。
     * @param exportPreviewImages 是否导出预览图。
     * @param exportDatasheet 是否导出数据手册。
     * @param overwriteExistingFiles 是否覆盖已有文件。
     * @param updateMode 是否启用更新模式。
     * @param debugMode 是否启用调试模式。
     * @param symbolLibraryDescription 符号库描述。
     * @param footprintLibraryDescription 封装库描述。
     * @param footprintLibraryKeywords 封装库关键词。
     * @param targetFormat 目标格式枚举值。
     */
    void startExport(const QStringList& componentIds,
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
                     const QString& symbolLibraryDescription = QString(),
                     const QString& footprintLibraryDescription = QString(),
                     const QString& footprintLibraryKeywords = QString(),
                     int targetFormat = 0);
    void cancelExport();
    void handleCloseRequest();
    void updateComponentExportStatus(const QString& componentId, int previewImageExported, int datasheetExported);

signals:
    void progressChanged();
    void statusChanged();
    void isExportingChanged();
    void isStoppingChanged();
    void hasCompletedExportChanged();
    /**
     * @brief 当前目标的重试能力发生变化。
     */
    void retrySupportedChanged();
    void successCountChanged();
    void failureCountChanged();
    void totalCountChanged();
    void resultsListChanged();
    void filterModeChanged();
    void filteredResultsListChanged();
    void stageProgressChanged();

private slots:
    void handlePreloadProgressChanged(const PreloadProgress& progress);
    void handlePreloadCompleted(int successCount, int failedCount);
    void handleProgressChanged(const ExportOverallProgress& progress);
    void handleItemStatusChanged(const QString& componentId, const QString& typeName, const ExportItemStatus& status);
    void handleTypeCompleted(const QString& typeName, int successCount, int failedCount, int skippedCount);
    void handleCompleted(int successCount, int failedCount);
    void handleCancelled();
    void handleFailed(const QString& error);
    void flushPendingUpdates();

private:
    friend class ExportProgressResultsCoordinator;
    /**
     * @brief 允许阶段进度协作者复用三段式进度状态和通知方法。
     */
    friend class ExportProgressStageCoordinator;
    /**
     * @brief 允许重试协作者复用结果索引、导出选项和状态刷新流程。
     */
    friend class ExportProgressRetryCoordinator;

    QString typeStatusKey(const QString& typeName) const;
    void updateOverallItemStatus(QVariantMap& result) const;
    void resetItemForRetry(QVariantMap& result) const;
    void beginExportRun(const QStringList& componentIds, const QString& statusText);
    void markResultsDirty();

private:
    void updateProgress();
    void updateResultsList();
    void updateFilteredResults();
    void setStatus(const QString& status);
    void setIsExporting(bool exporting);
    void setProgress(int progress);
    void setHasCompletedExport(bool completed);

private:
    ParallelExportService* m_exportService;  ///< 并行导出服务引用。
    ComponentService* m_componentService;  ///< 元器件数据服务引用。
    ComponentListViewModel* m_componentListViewModel;  ///< 元件列表视图模型引用。
    QString m_status;  ///< 当前导出状态文本。
    int m_progress;  ///< 总体导出进度百分比。
    bool m_isExporting;  ///< 是否正在执行导出。
    QStringList m_componentIds;  ///< 当前导出的元器件编号列表。
    int m_totalCount;  ///< 当前导出的元器件总数。
    int m_successCount;  ///< 已成功完成的项目数量。
    int m_failureCount;  ///< 已失败的项目数量。
    QString m_filterMode;  ///< 结果列表筛选模式。
    QVariantList m_resultsList;  ///< 全部导出结果列表。
    QHash<QString, int> m_idToIndexMap;  ///< 元器件编号到结果索引的映射。
    QTimer* m_throttleTimer;  ///< 结果列表批量刷新定时器。
    bool m_pendingUpdate;  ///< 是否有等待刷新的结果变更。
    int m_fetchProgress;  ///< 数据抓取阶段进度。
    int m_processProgress;  ///< 数据处理阶段进度。
    int m_writeProgress;  ///< 文件写入阶段进度。
    bool m_isStopping;  ///< 是否正在停止导出。
    bool m_hasCompletedExport;  ///< 最近一次导出是否已完成。
    bool m_retrySupported = true;  ///< 当前目标是否支持失败项重试。
    bool m_exportSymbolEnabled;  ///< 是否导出符号库。
    bool m_eagleCombinedLibrary;  ///< 是否使用 Eagle 合并库输出。
    bool m_exportFootprintEnabled;  ///< 是否导出封装库。
    bool m_exportModel3DEnabled;  ///< 是否导出三维模型。
    bool m_exportPreviewEnabled;  ///< 是否导出预览图。
    bool m_exportDatasheetEnabled;  ///< 是否导出数据手册。
};

}  // namespace EasyKiConverter
