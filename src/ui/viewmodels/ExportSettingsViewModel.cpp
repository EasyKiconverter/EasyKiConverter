/*
 * 我发现我在学校，不谈恋爱也不学习，不打游戏也不逃课，每天就是处于认真听课，然后因为听不懂走神了，
 * 反应过来以后再继续认真听课的循环，进入这种状态的时候，我一般就是左脑和右脑已经聊美了，
 * 完全就是一个放飞自我，意淫我有多少多少家产，然后怕班里有人会读心术紧急撤回一条意淫，
 * 那这在恋爱小说里我不就是那种…背景板吗，就每天在学校挂机任务就完成的npc
 */

#include "ExportSettingsViewModel.h"

#include "ExportOptionsBuilder.h"
#include "services/ComponentCacheService.h"
#include "services/export/ParallelExportService.h"
#include "utils/FileUtils.h"
#include "utils/PathSecurity.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>

namespace EasyKiConverter {

ExportSettingsViewModel::ExportSettingsViewModel(ParallelExportService* exportService, QObject* parent)
    : QObject(parent)
    , m_exportService(exportService)
    , m_configService(ConfigService::instance())
    , m_outputPath("")
    , m_libName("MyLibrary")
    , m_exportSymbol(true)
    , m_exportFootprint(true)
    , m_exportModel3D(true)
    , m_exportModel3DFormat(ExportOptions::MODEL_3D_FORMAT_BOTH)
    , m_exportModel3DPathMode(ExportOptions::MODEL_3D_PATH_RELATIVE)
    , m_exportPreviewImages(false)
    , m_exportDatasheet(false)
    , m_overwriteExistingFiles(false)
    , m_weakNetworkSupport(false)
    , m_exportMode(0)
    , m_debugMode(false)
    , m_exportSymbolDescription(true)
    , m_exportFootprintDescription(true)
    , m_symbolLibraryDescription("")
    , m_footprintLibraryDescription("")
    , m_footprintLibraryKeywords("")
    , m_cacheDir("")
    , m_diskCacheLimitMB(ConfigService::DEFAULT_DISK_CACHE_LIMIT_MB)
    , m_isExporting(false)
    , m_status("Ready") {
    // 初始化时检查调试模式（命令行参数优先，环境变量向后兼容）
    bool envDebugMode = qEnvironmentVariableIsSet("EASYKICONVERTER_DEBUG_MODE");
    if (envDebugMode) {
        QString debugValue = qEnvironmentVariable("EASYKICONVERTER_DEBUG_MODE", "false").toLower();
        m_debugMode = (debugValue == "true" || debugValue == "1" || debugValue == "yes");
    } else {
        m_debugMode = m_configService->getDebugMode();
    }

    loadFromConfig();

    if (m_exportService) {
        connect(m_exportService,
                &ParallelExportService::preloadProgressChanged,
                this,
                &ExportSettingsViewModel::handlePreloadProgressChanged);
        connect(m_exportService,
                &ParallelExportService::preloadCompleted,
                this,
                &ExportSettingsViewModel::handlePreloadCompleted);
        connect(m_exportService,
                &ParallelExportService::progressChanged,
                this,
                &ExportSettingsViewModel::handleProgressChanged);
        connect(m_exportService,
                &ParallelExportService::typeCompleted,
                this,
                &ExportSettingsViewModel::handleTypeCompleted);
        connect(m_exportService, &ParallelExportService::completed, this, &ExportSettingsViewModel::handleCompleted);
        connect(m_exportService, &ParallelExportService::cancelled, this, &ExportSettingsViewModel::handleCancelled);
        connect(m_exportService, &ParallelExportService::failed, this, &ExportSettingsViewModel::handleFailed);
        qDebug() << "ExportSettingsViewModel: Connected to ParallelExportService signals";
    }
}

ExportSettingsViewModel::~ExportSettingsViewModel() {}

// 设置输出目录并持久化配置。
void ExportSettingsViewModel::setOutputPath(const QString& path) {
    if (m_outputPath != path) {
        m_outputPath = path;
        m_configService->setOutputPath(path);
        emit outputPathChanged();
    }
}

// 清理库名称后更新并持久化配置。
void ExportSettingsViewModel::setLibName(const QString& name) {
    // 清洗 libName，防止路径穿越（如 ../foo）和非法字符
    const QString safeName = PathSecurity::sanitizeFilename(name);
    if (m_libName != safeName) {
        m_libName = safeName;
        m_configService->setLibName(safeName);
        emit libNameChanged();
    }
}

// 设置是否导出符号库。
void ExportSettingsViewModel::setExportSymbol(bool enabled) {
    if (enabled && m_targetModel && m_targetModel->currentIndex() == static_cast<int>(TargetEdaFormat::Allegro))
        enabled = false;
    if (m_exportSymbol != enabled) {
        m_exportSymbol = enabled;
        m_configService->setExportSymbol(enabled);
        emit exportSymbolChanged();
    }
}

// 设置是否导出封装库。
void ExportSettingsViewModel::setExportFootprint(bool enabled) {
    if (m_exportFootprint != enabled) {
        m_exportFootprint = enabled;
        m_configService->setExportFootprint(enabled);
        emit exportFootprintChanged();
    }
}

// 设置是否导出三维模型，并应用目标格式限制。
void ExportSettingsViewModel::setExportModel3D(bool enabled) {
    if (enabled && m_targetModel && m_targetModel->currentIndex() == static_cast<int>(TargetEdaFormat::Xpedition)) {
        enabled = false;
    }
    if (m_exportModel3D != enabled) {
        m_exportModel3D = enabled;
        m_configService->setExportModel3D(enabled);
        emit exportModel3DChanged();
    }
}

// 设置三维模型导出格式。
void ExportSettingsViewModel::setExportModel3DFormat(int format) {
    if (m_exportModel3DFormat != format) {
        m_exportModel3DFormat = format;
        m_configService->setExportModel3DFormat(format);
        emit exportModel3DFormatChanged();
    }
}

// 设置三维模型路径模式并规范化取值。
void ExportSettingsViewModel::setExportModel3DPathMode(int mode) {
    const int normalizedMode = ExportOptions::normalizePathMode(mode);
    if (m_exportModel3DPathMode != normalizedMode) {
        m_exportModel3DPathMode = normalizedMode;
        m_configService->setExportModel3DPathMode(normalizedMode);
        emit exportModel3DPathModeChanged();
    }
}

// 设置是否导出预览图片。
void ExportSettingsViewModel::setExportPreviewImages(bool enabled) {
    if (m_exportPreviewImages != enabled) {
        m_exportPreviewImages = enabled;
        m_configService->setExportPreviewImages(enabled);
        emit exportPreviewImagesChanged();
    }
}

// 设置是否导出数据手册。
void ExportSettingsViewModel::setExportDatasheet(bool enabled) {
    if (m_exportDatasheet != enabled) {
        m_exportDatasheet = enabled;
        m_configService->setExportDatasheet(enabled);
        emit exportDatasheetChanged();
    }
}

// 设置是否覆盖已有导出文件。
void ExportSettingsViewModel::setOverwriteExistingFiles(bool enabled) {
    if (m_overwriteExistingFiles != enabled) {
        m_overwriteExistingFiles = enabled;
        m_configService->setOverwriteExistingFiles(enabled);
        emit overwriteExistingFilesChanged();
    }
}

// 设置弱网络适配选项。
void ExportSettingsViewModel::setWeakNetworkSupport(bool enabled) {
    if (m_weakNetworkSupport != enabled) {
        m_weakNetworkSupport = enabled;
        m_configService->setWeakNetworkSupport(enabled);
        emit weakNetworkSupportChanged();
    }
}

// 设置库导出的追加或更新模式。
void ExportSettingsViewModel::setExportMode(int mode) {
    if (m_exportMode != mode) {
        m_exportMode = mode;
        m_configService->setExportMode(mode);
        emit exportModeChanged();
        qDebug() << "Export mode changed to:" << mode << "(0=append, 1=update)";
    }
}

// 设置调试模式，环境变量存在时由环境变量优先控制。
void ExportSettingsViewModel::setDebugMode(bool enabled) {
    bool envDebugMode = qEnvironmentVariableIsSet("EASYKICONVERTER_DEBUG_MODE");

    if (envDebugMode) {
        qDebug()
            << "Debug mode is controlled by environment variable EASYKICONVERTER_DEBUG_MODE, ignoring manual setting";
        return;
    }

    if (m_debugMode != enabled) {
        m_debugMode = enabled;
        emit debugModeChanged();
        m_configService->setDebugMode(enabled);
    }
}

// 绑定目标格式模型并同步目标格式相关限制。
void ExportSettingsViewModel::setTargetModel(ExportTargetModel* model) {
    if (m_targetModel == model)
        return;
    m_targetModel = model;
    if (m_targetModel) {
        connect(m_targetModel, &ExportTargetModel::currentTargetChanged, this, [this]() {
            if (m_targetModel && m_targetModel->currentIndex() == static_cast<int>(TargetEdaFormat::Xpedition)) {
                setExportModel3D(false);
                return;
            }
            if (m_targetModel && m_targetModel->currentIndex() == static_cast<int>(TargetEdaFormat::Allegro)) {
                setExportSymbol(false);
                if (m_exportModel3D)
                    setExportModel3DFormat(ExportOptions::MODEL_3D_FORMAT_STEP);
            }
            if (m_targetModel && m_targetModel->currentIndex() == static_cast<int>(TargetEdaFormat::Pcad)) {
                setExportSymbol(false);
            }
            if (m_targetModel && m_targetModel->currentIndex() == static_cast<int>(TargetEdaFormat::Altium) &&
                (m_exportModel3DFormat & ExportOptions::MODEL_3D_FORMAT_WRL)) {
                // Altium PcbLib 只能可靠嵌入 STEP，切换目标时移除 WRL 位。
                setExportModel3DFormat(ExportOptions::MODEL_3D_FORMAT_STEP);
            }
        });
        if (m_targetModel->currentIndex() == static_cast<int>(TargetEdaFormat::Xpedition))
            setExportModel3D(false);
        if (m_targetModel->currentIndex() == static_cast<int>(TargetEdaFormat::Allegro)) {
            setExportSymbol(false);
            if (m_exportModel3D)
                setExportModel3DFormat(ExportOptions::MODEL_3D_FORMAT_STEP);
        }
        if (m_targetModel->currentIndex() == static_cast<int>(TargetEdaFormat::Pcad)) {
            setExportSymbol(false);
        }
        if (m_targetModel->currentIndex() == static_cast<int>(TargetEdaFormat::Altium) &&
            (m_exportModel3DFormat & ExportOptions::MODEL_3D_FORMAT_WRL)) {
            setExportModel3DFormat(ExportOptions::MODEL_3D_FORMAT_STEP);
        }
    }
}

// 设置是否导出符号描述信息。
void ExportSettingsViewModel::setExportSymbolDescription(bool enabled) {
    if (m_exportSymbolDescription != enabled) {
        m_exportSymbolDescription = enabled;
        emit exportSymbolDescriptionChanged();
    }
}

// 设置是否导出封装描述信息。
void ExportSettingsViewModel::setExportFootprintDescription(bool enabled) {
    if (m_exportFootprintDescription != enabled) {
        m_exportFootprintDescription = enabled;
        emit exportFootprintDescriptionChanged();
    }
}

// 设置符号库描述文本。
void ExportSettingsViewModel::setSymbolLibraryDescription(const QString& desc) {
    if (m_symbolLibraryDescription != desc) {
        m_symbolLibraryDescription = desc;
        emit symbolLibraryDescriptionChanged();
    }
}

// 设置封装库描述文本。
void ExportSettingsViewModel::setFootprintLibraryDescription(const QString& desc) {
    if (m_footprintLibraryDescription != desc) {
        m_footprintLibraryDescription = desc;
        emit footprintLibraryDescriptionChanged();
    }
}

// 设置封装库关键词文本。
void ExportSettingsViewModel::setFootprintLibraryKeywords(const QString& keywords) {
    if (m_footprintLibraryKeywords != keywords) {
        m_footprintLibraryKeywords = keywords;
        emit footprintLibraryKeywordsChanged();
    }
}

// 设置缓存目录，持久化配置后迁移已有缓存。
void ExportSettingsViewModel::setCacheDir(const QString& path) {
    const QString normalizedPath = QDir::cleanPath(path);
    if (m_cacheDir != normalizedPath) {
        m_cacheDir = normalizedPath;
        // 先持久化配置，再迁移文件：即使迁移中断，重启后仍使用新路径（旧目录文件保留）
        m_configService->setCacheDir(normalizedPath);
        ComponentCacheService::instance()->setCacheDir(normalizedPath, /*migrateExistingCache=*/true);
        emit cacheDirChanged();
    }
}

// 设置磁盘缓存上限并限制在允许范围内。
void ExportSettingsViewModel::setDiskCacheLimitMB(int maxSizeMB) {
    const int normalizedSize = qBound(1, maxSizeMB, ConfigService::MAX_DISK_CACHE_LIMIT_MB);
    if (m_diskCacheLimitMB != normalizedSize) {
        m_diskCacheLimitMB = normalizedSize;
        m_configService->setDiskCacheLimitMB(normalizedSize);
        ComponentCacheService::instance()->setDiskCacheLimit(normalizedSize);
        emit diskCacheLimitMBChanged();
    }
}

// 构建导出选项、预加载组件数据并启动导出。
void ExportSettingsViewModel::startExport(const QStringList& componentIds) {
    qDebug() << "Starting export for" << componentIds.size() << "components";

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

    // Save component IDs for after preload
    m_pendingComponentIds = componentIds;

    // Build and set export options
    buildExportOptions();

    setIsExporting(true);
    setStatus("Preloading component data...");

    // Start preload first
    m_exportService->startPreload(componentIds);
}

// 将界面设置转换为导出服务使用的选项对象。
void ExportSettingsViewModel::buildExportOptions() {
    const ExportOptions options = ExportOptionsBuilder::build(*this);

    m_exportService->setOptions(options);
    m_exportService->setOutputPath(options.outputPath);
}

// 请求导出服务取消当前导出并更新界面状态。
void ExportSettingsViewModel::cancelExport() {
    qDebug() << "Cancelling export";

    if (!m_exportService) {
        return;
    }

    m_exportService->cancelExport();
    setIsExporting(false);
    setStatus("Export cancelled");
}

// 创建并打开当前输出目录。
bool ExportSettingsViewModel::openOutputFolder() {
    if (m_outputPath.isEmpty()) {
        return false;
    }

    QString pathToOpen = m_outputPath;
    if (!QDir(pathToOpen).exists()) {
        QDir().mkpath(pathToOpen);
    }

    FileUtils utils;
    return utils.openFolder(pathToOpen);
}

// 将预加载进度转换为界面状态文本。
void ExportSettingsViewModel::handlePreloadProgressChanged(const PreloadProgress& progress) {
    Q_UNUSED(progress);
    setStatus(QString("Preloading... %1/%2").arg(progress.completedCount).arg(progress.totalCount));
}

// 处理预加载完成信号并进入实际导出阶段。
void ExportSettingsViewModel::handlePreloadCompleted(int successCount, int failedCount) {
    qDebug() << "Preload completed: success=" << successCount << "failed=" << failedCount;

    if (failedCount > 0) {
        setStatus(QString("Preload completed with errors: %1 failed").arg(failedCount));
    }

    // Now start the actual export
    setStatus("Starting export...");
    emit exportStarted();
    m_exportService->startExport();
}

// 处理整体导出进度变化并更新状态文本。
void ExportSettingsViewModel::handleProgressChanged(const ExportOverallProgress& progress) {
    Q_UNUSED(progress);
    setStatus("Exporting...");
}

// 记录单个导出类型完成后的统计信息。
void ExportSettingsViewModel::handleTypeCompleted(const QString& typeName,
                                                  int successCount,
                                                  int failedCount,
                                                  int skippedCount) {
    Q_UNUSED(skippedCount);
    qDebug() << "Type completed:" << typeName << "success=" << successCount << "failed=" << failedCount;
}

// 处理全部导出完成信号并更新成功或失败状态。
void ExportSettingsViewModel::handleCompleted(int successCount, int failedCount) {
    qDebug() << "Export completed: success=" << successCount << "failed=" << failedCount;
    setIsExporting(false);

    if (failedCount > 0) {
        setStatus(QString("Export completed with errors: %1 failed").arg(failedCount));
    } else {
        setStatus("Export completed successfully");
    }
}

// 处理导出取消信号并恢复空闲状态。
void ExportSettingsViewModel::handleCancelled() {
    qDebug() << "Export cancelled";
    setIsExporting(false);
    setStatus("Export cancelled");
}

// 处理导出失败信号并向界面展示错误信息。
void ExportSettingsViewModel::handleFailed(const QString& error) {
    qWarning() << "Export failed:" << error;
    setIsExporting(false);
    setStatus(QString("Export failed: %1").arg(error));
}

// 更新导出中状态并通知 QML 绑定。
void ExportSettingsViewModel::setIsExporting(bool exporting) {
    if (m_isExporting != exporting) {
        m_isExporting = exporting;
        emit isExportingChanged();
    }
}

// 更新状态文本并通知 QML 绑定。
void ExportSettingsViewModel::setStatus(const QString& status) {
    if (m_status != status) {
        m_status = status;
        emit statusChanged();
    }
}

// 从配置服务加载设置并仅对发生变化的属性发出通知。
void ExportSettingsViewModel::loadFromConfig() {
    // 保存旧值以便比较，仅在值变化时发射信号（避免不必要的 QML 绑定刷新）
    const QString oldOutputPath = m_outputPath;
    const QString oldLibName = m_libName;
    const bool oldExportSymbol = m_exportSymbol;
    const bool oldExportFootprint = m_exportFootprint;
    const bool oldExportModel3D = m_exportModel3D;
    const int oldExportModel3DFormat = m_exportModel3DFormat;
    const int oldExportModel3DPathMode = m_exportModel3DPathMode;
    const bool oldExportPreviewImages = m_exportPreviewImages;
    const bool oldExportDatasheet = m_exportDatasheet;
    const bool oldOverwriteExistingFiles = m_overwriteExistingFiles;
    const bool oldWeakNetworkSupport = m_weakNetworkSupport;
    const int oldExportMode = m_exportMode;
    const bool oldDebugMode = m_debugMode;
    const QString oldCacheDir = m_cacheDir;
    const int oldDiskCacheLimitMB = m_diskCacheLimitMB;

    m_outputPath = m_configService->getOutputPath();
    m_libName = PathSecurity::sanitizeFilename(m_configService->getLibName());
    m_exportSymbol = m_configService->getExportSymbol();
    m_exportFootprint = m_configService->getExportFootprint();
    m_exportModel3D = m_configService->getExportModel3D();
    m_exportModel3DFormat = m_configService->getExportModel3DFormat();
    m_exportModel3DPathMode = m_configService->getExportModel3DPathMode();
    m_exportPreviewImages = m_configService->getExportPreviewImages();
    m_exportDatasheet = m_configService->getExportDatasheet();
    m_overwriteExistingFiles = m_configService->getOverwriteExistingFiles();
    m_weakNetworkSupport = m_configService->getWeakNetworkSupport();
    m_exportMode = m_configService->getExportMode();
    m_cacheDir = m_configService->getCacheDir();
    m_diskCacheLimitMB = m_configService->getDiskCacheLimitMB();

    bool envDebugMode = qEnvironmentVariableIsSet("EASYKICONVERTER_DEBUG_MODE");
    if (envDebugMode) {
        QString debugValue = qEnvironmentVariable("EASYKICONVERTER_DEBUG_MODE", "false").toLower();
        m_debugMode = (debugValue == "true" || debugValue == "1" || debugValue == "yes");
    } else {
        m_debugMode = m_configService->getDebugMode();
    }

    // 以下字段 ConfigService 暂未支持持久化，使用默认值
    m_exportSymbolDescription = true;
    m_exportFootprintDescription = true;
    m_symbolLibraryDescription.clear();
    m_footprintLibraryDescription.clear();
    m_footprintLibraryKeywords.clear();

    // 仅在值变化时发射信号
    if (m_outputPath != oldOutputPath)
        emit outputPathChanged();
    if (m_libName != oldLibName)
        emit libNameChanged();
    if (m_exportSymbol != oldExportSymbol)
        emit exportSymbolChanged();
    if (m_exportFootprint != oldExportFootprint)
        emit exportFootprintChanged();
    if (m_exportModel3D != oldExportModel3D)
        emit exportModel3DChanged();
    if (m_exportModel3DFormat != oldExportModel3DFormat)
        emit exportModel3DFormatChanged();
    if (m_exportModel3DPathMode != oldExportModel3DPathMode)
        emit exportModel3DPathModeChanged();
    if (m_exportPreviewImages != oldExportPreviewImages)
        emit exportPreviewImagesChanged();
    if (m_exportDatasheet != oldExportDatasheet)
        emit exportDatasheetChanged();
    if (m_overwriteExistingFiles != oldOverwriteExistingFiles)
        emit overwriteExistingFilesChanged();
    if (m_weakNetworkSupport != oldWeakNetworkSupport)
        emit weakNetworkSupportChanged();
    if (m_exportMode != oldExportMode)
        emit exportModeChanged();
    if (m_debugMode != oldDebugMode)
        emit debugModeChanged();
    // 非持久化字段始终发射（首次加载时需要通知 QML）
    emit exportSymbolDescriptionChanged();
    emit exportFootprintDescriptionChanged();
    emit symbolLibraryDescriptionChanged();
    emit footprintLibraryDescriptionChanged();
    emit footprintLibraryKeywordsChanged();
    if (m_cacheDir != oldCacheDir)
        emit cacheDirChanged();
    if (m_diskCacheLimitMB != oldDiskCacheLimitMB)
        emit diskCacheLimitMBChanged();
}

// 请求配置服务保存当前设置。
void ExportSettingsViewModel::saveConfig() {
    m_configService->saveConfig();
}

// 恢复界面设置的默认值并同步配置服务。
void ExportSettingsViewModel::resetConfig() {
    m_outputPath = "";
    m_libName = "MyLibrary";
    m_exportSymbol = true;
    m_exportFootprint = true;
    m_exportModel3D = true;
    m_exportModel3DFormat = ExportOptions::MODEL_3D_FORMAT_BOTH;
    m_exportModel3DPathMode = ExportOptions::MODEL_3D_PATH_RELATIVE;
    m_exportPreviewImages = false;
    m_exportDatasheet = false;
    m_overwriteExistingFiles = false;
    m_weakNetworkSupport = false;
    m_exportMode = 0;
    m_debugMode = false;
    m_exportSymbolDescription = true;
    m_exportFootprintDescription = true;
    m_symbolLibraryDescription.clear();
    m_footprintLibraryDescription.clear();
    m_footprintLibraryKeywords.clear();
    m_cacheDir = m_configService->getCacheDir();
    m_diskCacheLimitMB = m_configService->getDiskCacheLimitMB();

    m_configService->beginBatchUpdate();
    m_configService->setOutputPath(m_outputPath);
    m_configService->setLibName(m_libName);
    m_configService->setExportSymbol(m_exportSymbol);
    m_configService->setExportFootprint(m_exportFootprint);
    m_configService->setExportModel3D(m_exportModel3D);
    m_configService->setExportModel3DFormat(m_exportModel3DFormat);
    m_configService->setExportModel3DPathMode(m_exportModel3DPathMode);
    m_configService->setExportPreviewImages(m_exportPreviewImages);
    m_configService->setExportDatasheet(m_exportDatasheet);
    m_configService->setOverwriteExistingFiles(m_overwriteExistingFiles);
    m_configService->setWeakNetworkSupport(m_weakNetworkSupport);
    m_configService->setExportMode(m_exportMode);
    m_configService->setDebugMode(m_debugMode);
    m_configService->setCacheDir(m_cacheDir);
    m_configService->setDiskCacheLimitMB(m_diskCacheLimitMB);
    m_configService->endBatchUpdate();
    ComponentCacheService::instance()->setCacheDir(m_cacheDir);
    ComponentCacheService::instance()->setDiskCacheLimit(m_diskCacheLimitMB);

    emit outputPathChanged();
    emit libNameChanged();
    emit exportSymbolChanged();
    emit exportFootprintChanged();
    emit exportModel3DChanged();
    emit exportModel3DFormatChanged();
    emit exportModel3DPathModeChanged();
    emit exportPreviewImagesChanged();
    emit exportDatasheetChanged();
    emit overwriteExistingFilesChanged();
    emit weakNetworkSupportChanged();
    emit exportModeChanged();
    emit debugModeChanged();
    emit exportSymbolDescriptionChanged();
    emit exportFootprintDescriptionChanged();
    emit symbolLibraryDescriptionChanged();
    emit footprintLibraryDescriptionChanged();
    emit footprintLibraryKeywordsChanged();
    emit cacheDirChanged();
    emit diskCacheLimitMBChanged();
}

}  // namespace EasyKiConverter

// 《生活千疮百孔，好透气》《人生一波三折，好便宜》《生活一地鸡毛，好蓬松》
// 《想蒙上被子哭一场，刚蒙上就睡着了》《生活给了我一巴掌，我说没有上次响》
// 《是金子总会发光，奈何我是老铁》《生活给了我一拳，我一躺就是一整天》
// 《路见不平，绕道而行》《他们都看不起我，偏偏我也不争气》
// 《风吹哪页读哪页，哪页不懂撕哪页》
