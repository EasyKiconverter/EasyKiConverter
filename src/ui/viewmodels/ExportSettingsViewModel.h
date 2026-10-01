#pragma once

#include "services/ConfigService.h"
#include "services/export/ParallelExportService.h"
#include "ui/viewmodels/ExportTargetModel.h"

#include <QObject>
#include <QString>

namespace EasyKiConverter {

/**
 * @brief 导出设置视图模型
 *
 * 负责管理导出设置相关的 UI 状态和操作
 */
class ExportSettingsViewModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString outputPath READ outputPath WRITE setOutputPath NOTIFY outputPathChanged)
    Q_PROPERTY(QString libName READ libName WRITE setLibName NOTIFY libNameChanged)
    Q_PROPERTY(bool exportSymbol READ exportSymbol WRITE setExportSymbol NOTIFY exportSymbolChanged)
    Q_PROPERTY(bool exportFootprint READ exportFootprint WRITE setExportFootprint NOTIFY exportFootprintChanged)
    Q_PROPERTY(bool exportModel3D READ exportModel3D WRITE setExportModel3D NOTIFY exportModel3DChanged)
    Q_PROPERTY(
        int exportModel3DFormat READ exportModel3DFormat WRITE setExportModel3DFormat NOTIFY exportModel3DFormatChanged)
    Q_PROPERTY(int exportModel3DPathMode READ exportModel3DPathMode WRITE setExportModel3DPathMode NOTIFY
                   exportModel3DPathModeChanged)
    Q_PROPERTY(bool exportPreviewImages READ exportPreviewImages WRITE setExportPreviewImages NOTIFY
                   exportPreviewImagesChanged)
    Q_PROPERTY(bool exportDatasheet READ exportDatasheet WRITE setExportDatasheet NOTIFY exportDatasheetChanged)
    Q_PROPERTY(bool overwriteExistingFiles READ overwriteExistingFiles WRITE setOverwriteExistingFiles NOTIFY
                   overwriteExistingFilesChanged)
    Q_PROPERTY(
        bool weakNetworkSupport READ weakNetworkSupport WRITE setWeakNetworkSupport NOTIFY weakNetworkSupportChanged)
    Q_PROPERTY(int exportMode READ exportMode WRITE setExportMode NOTIFY exportModeChanged)
    Q_PROPERTY(bool requiresFullReplacement READ requiresFullReplacement NOTIFY requiresFullReplacementChanged)
    Q_PROPERTY(bool debugMode READ debugMode WRITE setDebugMode NOTIFY debugModeChanged)
    Q_PROPERTY(bool exportSymbolDescription READ exportSymbolDescription WRITE setExportSymbolDescription NOTIFY
                   exportSymbolDescriptionChanged)
    Q_PROPERTY(bool exportFootprintDescription READ exportFootprintDescription WRITE setExportFootprintDescription
                   NOTIFY exportFootprintDescriptionChanged)
    Q_PROPERTY(QString symbolLibraryDescription READ symbolLibraryDescription WRITE setSymbolLibraryDescription NOTIFY
                   symbolLibraryDescriptionChanged)
    Q_PROPERTY(QString footprintLibraryDescription READ footprintLibraryDescription WRITE setFootprintLibraryDescription
                   NOTIFY footprintLibraryDescriptionChanged)
    Q_PROPERTY(QString footprintLibraryKeywords READ footprintLibraryKeywords WRITE setFootprintLibraryKeywords NOTIFY
                   footprintLibraryKeywordsChanged)
    Q_PROPERTY(QString cacheDir READ cacheDir WRITE setCacheDir NOTIFY cacheDirChanged)
    Q_PROPERTY(int diskCacheLimitMB READ diskCacheLimitMB WRITE setDiskCacheLimitMB NOTIFY diskCacheLimitMBChanged)
    Q_PROPERTY(int maxDiskCacheLimitMB READ maxDiskCacheLimitMB CONSTANT)
    Q_PROPERTY(bool isExporting READ isExporting NOTIFY isExportingChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)

public:
    explicit ExportSettingsViewModel(ParallelExportService* exportService, QObject* parent = nullptr);
    ~ExportSettingsViewModel() override;

    /**
     * @brief 设置导出目标模型引用，用于同步目标格式状态
     * @param model ExportTargetModel 实例指针
     */
    void setTargetModel(ExportTargetModel* model);

    // Getter 方法
    QString outputPath() const {
        return m_outputPath;
    }

    /** @brief 获取库名称。 */
    QString libName() const {
        return m_libName;
    }

    /** @brief 获取符号导出开关。 */
    bool exportSymbol() const {
        return m_exportSymbol;
    }

    /** @brief 获取封装导出开关。 */
    bool exportFootprint() const {
        return m_exportFootprint;
    }

    /** @brief 获取三维模型导出开关。 */
    bool exportModel3D() const {
        return m_exportModel3D;
    }

    // 3D模型格式(位掩码): 0=NONE, 1=WRL, 2=STEP, 3=BOTH
    int exportModel3DFormat() const {
        return m_exportModel3DFormat;
    }

    /** @brief 获取三维模型路径模式。 */
    int exportModel3DPathMode() const {
        return m_exportModel3DPathMode;
    }

    /** @brief 获取预览图片导出开关。 */
    bool exportPreviewImages() const {
        return m_exportPreviewImages;
    }

    /** @brief 获取数据手册导出开关。 */
    bool exportDatasheet() const {
        return m_exportDatasheet;
    }

    /** @brief 获取覆盖已有文件开关。 */
    bool overwriteExistingFiles() const {
        return m_overwriteExistingFiles;
    }

    /** @brief 获取弱网络适配开关。 */
    bool weakNetworkSupport() const {
        return m_weakNetworkSupport;
    }

    /** @brief 获取导出模式。 */
    int exportMode() const {
        return m_exportMode;
    }

    /** @brief 判断当前目标是否只支持完整覆盖导出。 */
    bool requiresFullReplacement() const;

    /** @brief 获取调试模式开关。 */
    bool debugMode() const {
        return m_debugMode;
    }

    /** @brief 获取符号描述导出开关。 */
    bool exportSymbolDescription() const {
        return m_exportSymbolDescription;
    }

    /** @brief 获取封装描述导出开关。 */
    bool exportFootprintDescription() const {
        return m_exportFootprintDescription;
    }

    /** @brief 获取符号库描述。 */
    QString symbolLibraryDescription() const {
        return m_symbolLibraryDescription;
    }

    /** @brief 获取封装库描述。 */
    QString footprintLibraryDescription() const {
        return m_footprintLibraryDescription;
    }

    /** @brief 获取封装库关键词。 */
    QString footprintLibraryKeywords() const {
        return m_footprintLibraryKeywords;
    }

    /** @brief 获取缓存目录。 */
    QString cacheDir() const {
        return m_cacheDir;
    }

    /** @brief 获取磁盘缓存上限。 */
    int diskCacheLimitMB() const {
        return m_diskCacheLimitMB;
    }

    /** @brief 获取允许的最大磁盘缓存上限。 */
    int maxDiskCacheLimitMB() const {
        return ConfigService::MAX_DISK_CACHE_LIMIT_MB;
    }

    /** @brief 获取当前是否正在导出。 */
    bool isExporting() const {
        return m_isExporting;
    }

    /** @brief 获取当前状态文本。 */
    QString status() const {
        return m_status;
    }

    // Setter 方法（标记为 Q_INVOKABLE 以便QML 中调用）
    Q_INVOKABLE void setOutputPath(const QString& path);
    Q_INVOKABLE void setLibName(const QString& name);
    Q_INVOKABLE void setExportSymbol(bool enabled);
    Q_INVOKABLE void setExportFootprint(bool enabled);
    Q_INVOKABLE void setExportModel3D(bool enabled);
    Q_INVOKABLE void setExportModel3DFormat(int format);
    Q_INVOKABLE void setExportModel3DPathMode(int mode);
    Q_INVOKABLE void setExportPreviewImages(bool enabled);
    Q_INVOKABLE void setExportDatasheet(bool enabled);
    Q_INVOKABLE void setOverwriteExistingFiles(bool enabled);
    Q_INVOKABLE void setWeakNetworkSupport(bool enabled);
    Q_INVOKABLE void setExportMode(int mode);
    Q_INVOKABLE void setDebugMode(bool enabled);
    Q_INVOKABLE void setExportSymbolDescription(bool enabled);
    Q_INVOKABLE void setExportFootprintDescription(bool enabled);
    Q_INVOKABLE void setSymbolLibraryDescription(const QString& desc);
    Q_INVOKABLE void setFootprintLibraryDescription(const QString& desc);
    Q_INVOKABLE void setFootprintLibraryKeywords(const QString& keywords);
    Q_INVOKABLE void setCacheDir(const QString& path);
    Q_INVOKABLE void setDiskCacheLimitMB(int maxSizeMB);

public slots:
    Q_INVOKABLE void saveConfig();
    Q_INVOKABLE void resetConfig();
    Q_INVOKABLE void startExport(const QStringList& componentIds);
    Q_INVOKABLE void cancelExport();
    Q_INVOKABLE bool openOutputFolder();

signals:
    void outputPathChanged();
    void libNameChanged();
    void exportSymbolChanged();
    void exportFootprintChanged();
    void exportModel3DChanged();
    void exportModel3DFormatChanged();
    void exportModel3DPathModeChanged();
    void exportPreviewImagesChanged();
    void exportDatasheetChanged();
    void overwriteExistingFilesChanged();
    void weakNetworkSupportChanged();
    void exportModeChanged();
    /** @brief 当前目标的导出模式限制发生变化。 */
    void requiresFullReplacementChanged();
    void debugModeChanged();
    void exportSymbolDescriptionChanged();
    void exportFootprintDescriptionChanged();
    void symbolLibraryDescriptionChanged();
    void footprintLibraryDescriptionChanged();
    void footprintLibraryKeywordsChanged();
    void cacheDirChanged();
    /**
     * @brief 缓存目录切换被拒绝时通知界面显示具体原因。
     * @param path 用户刚提交的目录路径。
     * @param reason 安全校验或迁移失败的具体原因。
     */
    void cacheDirChangeRejected(const QString& path, const QString& reason);
    void diskCacheLimitMBChanged();
    void isExportingChanged();
    void statusChanged();
    void preloadStarted();
    void exportStarted();

private slots:
    void handlePreloadProgressChanged(const PreloadProgress& progress);
    void handlePreloadCompleted(int successCount, int failedCount);
    void handleProgressChanged(const ExportOverallProgress& progress);
    void handleTypeCompleted(const QString& typeName, int successCount, int failedCount, int skippedCount);
    void handleCompleted(int successCount, int failedCount);
    void handleCancelled();
    void handleFailed(const QString& error);

private:
    friend class ExportOptionsBuilder;

    void buildExportOptions();
    void loadFromConfig();
    void setIsExporting(bool exporting);
    void setStatus(const QString& status);

private:
    ParallelExportService* m_exportService;
    ConfigService* m_configService;
    ExportTargetModel* m_targetModel = nullptr;  ///< 导出目标模型引用（非拥有）
    QString m_outputPath;
    QString m_libName;
    bool m_exportSymbol;
    bool m_exportFootprint;
    bool m_exportModel3D;
    int m_exportModel3DFormat;  // bitmask: 1=WRL, 2=STEP, 3=Both
    int m_exportModel3DPathMode;  // 0=relative, 1=absolute
    bool m_exportPreviewImages;
    bool m_exportDatasheet;
    bool m_overwriteExistingFiles;
    bool m_weakNetworkSupport;
    int m_exportMode;  // 0 = 追加模式, 1 = 更新模式, 2 = 覆盖模式
    bool m_debugMode;
    bool m_exportSymbolDescription;
    bool m_exportFootprintDescription;
    QString m_symbolLibraryDescription;
    QString m_footprintLibraryDescription;
    QString m_footprintLibraryKeywords;
    QString m_cacheDir;
    int m_diskCacheLimitMB;
    bool m_isExporting;
    QString m_status;
    QStringList m_pendingComponentIds;
};

}  // namespace EasyKiConverter
