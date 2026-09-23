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
    Q_PROPERTY(QString cacheDirUrl READ cacheDirUrl CONSTANT)
    Q_PROPERTY(int symbolSuccessCount READ symbolSuccessCount NOTIFY resultsListChanged)
    Q_PROPERTY(int footprintSuccessCount READ footprintSuccessCount NOTIFY resultsListChanged)
    Q_PROPERTY(int model3DSuccessCount READ model3DSuccessCount NOTIFY resultsListChanged)
    Q_PROPERTY(int previewSuccessCount READ previewSuccessCount NOTIFY resultsListChanged)
    Q_PROPERTY(int datasheetSuccessCount READ datasheetSuccessCount NOTIFY resultsListChanged)

public:
    explicit ExportProgressViewModel(ParallelExportService* exportService,
                                     ComponentService* componentService,
                                     ComponentListViewModel* componentListViewModel,
                                     QObject* parent = nullptr);
    ~ExportProgressViewModel() override;

    /** @brief 返回总体导出进度百分比。 */
    int progress() const {
        return m_progress;
    }

    /** @brief 返回当前导出状态文本。 */
    QString status() const {
        return m_status;
    }

    /** @brief 返回是否正在导出。 */
    bool isExporting() const {
        return m_isExporting;
    }

    /** @brief 返回成功结果数量。 */
    int successCount() const {
        return m_successCount;
    }

    /** @brief 返回失败结果数量。 */
    int failureCount() const {
        return m_failureCount;
    }

    /** @brief 返回结果总数量。 */
    int totalCount() const {
        return m_totalCount;
    }

    /** @brief 返回当前结果过滤模式。 */
    QString filterMode() const {
        return m_filterMode;
    }

    /** @brief 返回全部导出结果列表。 */
    QVariantList resultsList() const {
        return m_resultsList;
    }

    QVariantList filteredResultsList() const;
    int filteredSuccessCount() const;
    int filteredFailedCount() const;
    int filteredPendingCount() const;

    /** @brief 返回抓取阶段进度。 */
    int fetchProgress() const {
        return m_fetchProgress;
    }

    /** @brief 返回处理阶段进度。 */
    int processProgress() const {
        return m_processProgress;
    }

    /** @brief 返回写入阶段进度。 */
    int writeProgress() const {
        return m_writeProgress;
    }

    /** @brief 返回是否正在停止导出。 */
    bool isStopping() const {
        return m_isStopping;
    }

    /** @brief 返回统计面板显示的总数量。 */
    int statisticsTotal() const {
        return m_totalCount;
    }

    /** @brief 返回最近一次导出是否已完成。 */
    bool hasCompletedExport() const {
        return m_hasCompletedExport;
    }

    int symbolSuccessCount() const;
    int footprintSuccessCount() const;
    int model3DSuccessCount() const;
    int previewSuccessCount() const;
    int datasheetSuccessCount() const;

    /** @brief 返回供 QML 使用的缓存目录 URL。 */
    QString cacheDirUrl() const {
        auto cacheDir = ComponentCacheService::instance()->cacheDir();
        if (cacheDir.isEmpty()) {
            return QString();
        }
        return QUrl::fromLocalFile(cacheDir).toString();
    }

    Q_INVOKABLE QString getLastExportedPath() const;
    Q_INVOKABLE bool openLastExportedFolder();
    Q_INVOKABLE void clearCache();
    /** @brief 返回清空缓存确认对话框中的可处理条目数量。 */
    Q_INVOKABLE int cacheEntryCount() const;
    Q_INVOKABLE void resetExport();
    Q_INVOKABLE void setFilterMode(const QString& mode);
    Q_INVOKABLE void retryComponent(const QString& componentId);
    Q_INVOKABLE void retryFailedComponents();
    Q_INVOKABLE void removeResult(const QString& componentId);
    Q_INVOKABLE QVariantMap getComponentExportStatus(const QString& componentId) const;

public slots:
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
    /** @brief 允许阶段进度协作者复用三段式进度状态和通知方法。 */
    friend class ExportProgressStageCoordinator;
    /** @brief 允许重试协作者复用结果索引、导出选项和状态刷新流程。 */
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
    ParallelExportService* m_exportService;
    ComponentService* m_componentService;
    ComponentListViewModel* m_componentListViewModel;
    QString m_status;
    int m_progress;
    bool m_isExporting;
    QStringList m_componentIds;
    int m_totalCount;
    int m_successCount;
    int m_failureCount;
    QString m_filterMode;
    QVariantList m_resultsList;
    QHash<QString, int> m_idToIndexMap;
    QTimer* m_throttleTimer;
    bool m_pendingUpdate;
    int m_fetchProgress;
    int m_processProgress;
    int m_writeProgress;
    bool m_isStopping;
    bool m_hasCompletedExport;
    bool m_exportSymbolEnabled;
    bool m_eagleCombinedLibrary;
    bool m_exportFootprintEnabled;
    bool m_exportModel3DEnabled;
    bool m_exportPreviewEnabled;
    bool m_exportDatasheetEnabled;
};

}  // namespace EasyKiConverter
