/*
 * 我发现我在学校，不谈恋爱也不学习，不打游戏也不逃课，每天就是处于认真听课，然后因为听不懂走神了，
 * 反应过来以后再继续认真听课的循环，进入这种状态的时候，我一般就是左脑和右脑已经聊美了，
 * 完全就是一个放飞自我，意淫我有多少多少家产，然后怕班里有人会读心术紧急撤回一条意淫，
 * 那这在恋爱小说里我不就是那种…背景板吗，就每天在学校挂机任务就完成的npc
 */

#include "ExportSettingsViewModel.h"

#include "ExportOptionsBuilder.h"
#include "services/CacheSafety.h"
#include "services/ComponentCacheService.h"
#include "services/export/ParallelExportService.h"
#include "utils/FileUtils.h"
#include "utils/PathSecurity.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>

namespace EasyKiConverter {

namespace {

/**
 * @brief 将缓存服务的稳定诊断映射为当前界面的语言，避免英文界面直接显示中文后端错误。
 * @param error 缓存服务返回的错误信息。
 * @return 当前界面语言对应的错误信息。
 */
QString translateCacheDirectoryError(const QString& error) {
    const auto translate = [](const char* source) {
        return QCoreApplication::translate("CacheDirectoryErrors", source);
    };
    const auto translateWithSuffix = [&error, &translate](const QString& prefix, const char* source) {
        return translate(source).arg(error.mid(prefix.size()));
    };

    const QStringList exactMessages = {
        QStringLiteral("缓存目录不能为空"),
        QStringLiteral("缓存目录必须使用绝对路径"),
        QStringLiteral("缓存目录不能包含符号链接路径分量"),
        QStringLiteral("不能选择文件系统根目录或用户主目录"),
        QStringLiteral("缓存路径必须是目录"),
        QStringLiteral("缓存目录必须为空，或已经包含 EasyKiConverter 所有权标记"),
        QStringLiteral("迁移失败"),
    };
    if (exactMessages.contains(error))
        return translate(error.toUtf8().constData());

    const QList<QPair<QString, const char*>> prefixedMessages = {
        {QStringLiteral("无法创建缓存目录："), "无法创建缓存目录：%1"},
        {QStringLiteral("缓存目录不是空目录且无法验证所有权："), "缓存目录不是空目录且无法验证所有权：%1"},
        {QStringLiteral("无法写入缓存所有权标记："), "无法写入缓存所有权标记：%1"},
        {QStringLiteral("无法升级旧缓存元数据："), "无法升级旧缓存元数据：%1"},
        {QStringLiteral("缓存根目录没有有效所有权标记"), "缓存根目录没有有效所有权标记"},
        {QStringLiteral("无法创建三维模型缓存目录："), "无法创建三维模型缓存目录：%1"},
        {QStringLiteral("三维模型缓存目录不是空目录且无法验证所有权："),
         "三维模型缓存目录不是空目录且无法验证所有权：%1"},
        {QStringLiteral("缓存迁移目标已存在同名文件："), "缓存迁移目标已存在同名文件：%1"},
        {QStringLiteral("缓存迁移目标已存在同名三维模型："), "缓存迁移目标已存在同名三维模型：%1"},
        {QStringLiteral("缓存迁移遇到无法验证的组件内容："), "缓存迁移遇到无法验证的组件内容：%1"},
        {QStringLiteral("旧缓存目录包含无法验证归属的数据，源目录已保留："),
         "旧缓存目录包含无法验证归属的数据，源目录已保留：%1"},
        {QStringLiteral("缓存迁移目标没有有效所有权标记："), "缓存迁移目标没有有效所有权标记：%1"},
        {QStringLiteral("缓存迁移目标包含未托管目录："), "缓存迁移目标包含未托管目录：%1"},
        {QStringLiteral("无法创建缓存迁移目标目录："), "无法创建缓存迁移目标目录：%1"},
        {QStringLiteral("无法创建缓存迁移目录："), "无法创建缓存迁移目录：%1"},
        {QStringLiteral("无法迁移缓存文件："), "无法迁移缓存文件：%1"},
        {QStringLiteral("缓存迁移失败，源目录已保留："), "缓存迁移失败，源目录已保留：%1"},
    };
    for (const auto& message : prefixedMessages) {
        if (error.startsWith(message.first))
            return translateWithSuffix(message.first, message.second);
    }

    // 未知诊断不能把中文原文泄漏到英文界面；详细原文仍会写入日志。
    return translate("缓存目录切换失败，请检查日志获取详细原因。");
}

}  // namespace

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
        // 读取环境变量提供的调试模式文本值。
        QString debugValue = qEnvironmentVariable("EASYKICONVERTER_DEBUG_MODE", "false").toLower();
        m_debugMode = (debugValue == "true" || debugValue == "1" || debugValue == "yes");
    } else {
        m_debugMode = m_configService->getDebugMode();
    }

    loadFromConfig();

    connect(ComponentCacheService::instance(),
            &ComponentCacheService::cacheMaintenanceWarning,
            this,
            [this](const QString& message) { setStatus(message); });

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

/**
 * @brief 设置输出目录并持久化配置。
 * @param path 路径或目录路径。
 */
void ExportSettingsViewModel::setOutputPath(const QString& path) {
    if (m_outputPath != path) {
        m_outputPath = path;
        m_configService->setOutputPath(path);
        emit outputPathChanged();
    }
}

/**
 * @brief 清理库名称后更新并持久化配置。
 * @param name 库名称。
 */
void ExportSettingsViewModel::setLibName(const QString& name) {
    // 清洗 libName，防止路径穿越（如 ../foo）和非法字符
    const QString safeName = PathSecurity::sanitizeFilename(name);
    if (m_libName != safeName) {
        m_libName = safeName;
        m_configService->setLibName(safeName);
        emit libNameChanged();
    }
}

/**
 * @brief 设置是否导出符号库。
 * @param enabled 开关状态。
 */
void ExportSettingsViewModel::setExportSymbol(bool enabled) {
    if (enabled && m_targetModel && m_targetModel->currentTargetFormat() == static_cast<int>(TargetEdaFormat::Allegro))
        enabled = false;
    if (m_exportSymbol != enabled) {
        m_exportSymbol = enabled;
        m_configService->setExportSymbol(enabled);
        emit exportSymbolChanged();
    }
}

/**
 * @brief 设置是否导出封装库。
 * @param enabled 开关状态。
 */
void ExportSettingsViewModel::setExportFootprint(bool enabled) {
    if (enabled && m_targetModel && m_targetModel->currentTargetFormat() == static_cast<int>(TargetEdaFormat::Orcad))
        enabled = false;
    if (m_exportFootprint != enabled) {
        m_exportFootprint = enabled;
        m_configService->setExportFootprint(enabled);
        emit exportFootprintChanged();
    }
}

/**
 * @brief 设置是否导出三维模型。
 * @param enabled 开关状态。
 */
void ExportSettingsViewModel::setExportModel3D(bool enabled) {
    if (m_exportModel3D != enabled) {
        m_exportModel3D = enabled;
        m_configService->setExportModel3D(enabled);
        emit exportModel3DChanged();
    }
}

/**
 * @brief 设置三维模型导出格式。
 * @param format 三维模型格式位掩码。
 */
void ExportSettingsViewModel::setExportModel3DFormat(int format) {
    if (m_exportModel3DFormat != format) {
        m_exportModel3DFormat = format;
        m_configService->setExportModel3DFormat(format);
        emit exportModel3DFormatChanged();
    }
}

/**
 * @brief 设置三维模型路径模式并规范化取值。
 * @param mode 三维模型路径模式或导出模式。
 */
void ExportSettingsViewModel::setExportModel3DPathMode(int mode) {
    // 将输入的三维模型路径模式规范化到支持的枚举值。
    const int normalizedMode = ExportOptions::normalizePathMode(mode);
    if (m_exportModel3DPathMode != normalizedMode) {
        m_exportModel3DPathMode = normalizedMode;
        m_configService->setExportModel3DPathMode(normalizedMode);
        emit exportModel3DPathModeChanged();
    }
}

/**
 * @brief 设置是否导出预览图片。
 * @param enabled 开关状态。
 */
void ExportSettingsViewModel::setExportPreviewImages(bool enabled) {
    if (m_exportPreviewImages != enabled) {
        m_exportPreviewImages = enabled;
        m_configService->setExportPreviewImages(enabled);
        emit exportPreviewImagesChanged();
    }
}

/**
 * @brief 设置是否导出数据手册。
 * @param enabled 开关状态。
 */
void ExportSettingsViewModel::setExportDatasheet(bool enabled) {
    if (m_exportDatasheet != enabled) {
        m_exportDatasheet = enabled;
        m_configService->setExportDatasheet(enabled);
        emit exportDatasheetChanged();
    }
}

/**
 * @brief 设置是否覆盖已有导出文件。
 * @param enabled 开关状态。
 */
void ExportSettingsViewModel::setOverwriteExistingFiles(bool enabled) {
    if (m_overwriteExistingFiles != enabled) {
        m_overwriteExistingFiles = enabled;
        m_configService->setOverwriteExistingFiles(enabled);
        emit overwriteExistingFilesChanged();
    }
}

/**
 * @brief 设置弱网络适配选项。
 * @param enabled 开关状态。
 */
void ExportSettingsViewModel::setWeakNetworkSupport(bool enabled) {
    if (m_weakNetworkSupport != enabled) {
        m_weakNetworkSupport = enabled;
        m_configService->setWeakNetworkSupport(enabled);
        emit weakNetworkSupportChanged();
    }
}

/**
 * @brief 设置库导出的追加、更新或覆盖模式。
 * @param mode 三维模型路径模式或导出模式。
 */
void ExportSettingsViewModel::setExportMode(int mode) {
    if (mode < 0 || mode > 2)
        mode = 0;
    if (requiresFullReplacement() && mode != 2) {
        qWarning() << "The selected target only supports full replacement export";
        mode = 2;
    }
    if (m_exportMode != mode) {
        m_exportMode = mode;
        m_configService->setExportMode(mode);
        emit exportModeChanged();
        qDebug() << "Export mode changed to:" << mode << "(0=append, 1=update, 2=overwrite)";
    }
}

/**
 * @brief 判断当前目标格式是否只支持完整替换导出。
 * @return 当前目标不支持追加和更新时返回 true。
 */
bool ExportSettingsViewModel::requiresFullReplacement() const {
    if (!m_targetModel)
        return false;

    const auto target =  // 当前目标格式枚举值。
        static_cast<TargetEdaFormat>(m_targetModel->currentTargetFormat());
    return target == TargetEdaFormat::Xpedition || target == TargetEdaFormat::Allegro ||
           target == TargetEdaFormat::Pads || target == TargetEdaFormat::Eagle || target == TargetEdaFormat::Pcad ||
           target == TargetEdaFormat::Cadstar || target == TargetEdaFormat::Orcad ||
           target == TargetEdaFormat::LibrePcb;
}

/**
 * @brief 设置调试模式，环境变量存在时由环境变量优先控制。
 * @param enabled 开关状态。
 */
void ExportSettingsViewModel::setDebugMode(bool enabled) {
    // 判断调试模式是否由环境变量接管。
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

/**
 * @brief 绑定目标格式模型并同步目标格式相关限制。
 * @param model 导出目标模型实例。
 */
void ExportSettingsViewModel::setTargetModel(ExportTargetModel* model) {
    if (m_targetModel == model)
        return;
    m_targetModel = model;
    if (m_targetModel) {
        connect(m_targetModel, &ExportTargetModel::currentTargetChanged, this, [this]() {
            if (requiresFullReplacement() && m_exportMode != 2)
                setExportMode(2);
            // 读取目标格式，以应用目标格式专属导出限制。
            const auto targetFormat = static_cast<TargetEdaFormat>(m_targetModel->currentTargetFormat());
            if (targetFormat == TargetEdaFormat::Allegro) {
                setExportSymbol(false);
                if (m_exportModel3D)
                    setExportModel3DFormat(ExportOptions::MODEL_3D_FORMAT_STEP);
            }
            if (targetFormat == TargetEdaFormat::Orcad)
                setExportFootprint(false);
            if ((targetFormat == TargetEdaFormat::Altium || targetFormat == TargetEdaFormat::LibrePcb ||
                 targetFormat == TargetEdaFormat::Horizon) &&
                (m_exportModel3DFormat & ExportOptions::MODEL_3D_FORMAT_WRL)) {
                // 这些原生库只接受 STEP，切换目标时移除 WRL 位。
                setExportModel3DFormat(ExportOptions::MODEL_3D_FORMAT_STEP);
            }
            emit requiresFullReplacementChanged();
        });
        if (requiresFullReplacement() && m_exportMode != 2)
            setExportMode(2);
        // 初始化当前目标格式对应的导出限制。
        const auto targetFormat = static_cast<TargetEdaFormat>(m_targetModel->currentTargetFormat());
        if (targetFormat == TargetEdaFormat::Allegro) {
            setExportSymbol(false);
            if (m_exportModel3D)
                setExportModel3DFormat(ExportOptions::MODEL_3D_FORMAT_STEP);
        }
        if (targetFormat == TargetEdaFormat::Orcad)
            setExportFootprint(false);
        if ((targetFormat == TargetEdaFormat::Altium || targetFormat == TargetEdaFormat::LibrePcb ||
             targetFormat == TargetEdaFormat::Horizon) &&
            (m_exportModel3DFormat & ExportOptions::MODEL_3D_FORMAT_WRL)) {
            setExportModel3DFormat(ExportOptions::MODEL_3D_FORMAT_STEP);
        }
        emit requiresFullReplacementChanged();
    }
}

/**
 * @brief 设置是否导出符号描述信息。
 * @param enabled 开关状态。
 */
void ExportSettingsViewModel::setExportSymbolDescription(bool enabled) {
    if (m_exportSymbolDescription != enabled) {
        m_exportSymbolDescription = enabled;
        emit exportSymbolDescriptionChanged();
    }
}

/**
 * @brief 设置是否导出封装描述信息。
 * @param enabled 开关状态。
 */
void ExportSettingsViewModel::setExportFootprintDescription(bool enabled) {
    if (m_exportFootprintDescription != enabled) {
        m_exportFootprintDescription = enabled;
        emit exportFootprintDescriptionChanged();
    }
}

/**
 * @brief 设置符号库描述文本。
 * @param desc 库描述文本。
 */
void ExportSettingsViewModel::setSymbolLibraryDescription(const QString& desc) {
    if (m_symbolLibraryDescription != desc) {
        m_symbolLibraryDescription = desc;
        emit symbolLibraryDescriptionChanged();
    }
}

/**
 * @brief 设置封装库描述文本。
 * @param desc 库描述文本。
 */
void ExportSettingsViewModel::setFootprintLibraryDescription(const QString& desc) {
    if (m_footprintLibraryDescription != desc) {
        m_footprintLibraryDescription = desc;
        emit footprintLibraryDescriptionChanged();
    }
}

/**
 * @brief 设置封装库关键词文本。
 * @param keywords 库关键词。
 */
void ExportSettingsViewModel::setFootprintLibraryKeywords(const QString& keywords) {
    if (m_footprintLibraryKeywords != keywords) {
        m_footprintLibraryKeywords = keywords;
        emit footprintLibraryKeywordsChanged();
    }
}

/**
 * @brief 校验缓存目录后再持久化配置并迁移已有缓存。
 * @param path 路径或目录路径。
 */
void ExportSettingsViewModel::setCacheDir(const QString& path) {
    // 保存安全校验返回的规范化缓存目录路径。
    QString normalizedPath;
    // 保存缓存目录校验或迁移失败的原始错误信息。
    QString error;
    if (!CacheSafety::validateSelection(path, &normalizedPath, &error)) {
        // 将安全校验错误转换为当前界面语言。
        const QString localizedError = translateCacheDirectoryError(error);
        setStatus(localizedError);
        emit cacheDirChangeRejected(path, localizedError);
        return;
    }
    if (m_cacheDir == normalizedPath)
        return;

    if (!ComponentCacheService::instance()->setCacheDir(normalizedPath, /*migrateExistingCache=*/true, &error)) {
        // 保存迁移失败的可读原因，供状态和拒绝信号共同使用。
        const QString reason = translateCacheDirectoryError(error.isEmpty() ? QStringLiteral("迁移失败") : error);
        setStatus(QStringLiteral("缓存目录未切换：%1").arg(reason));
        emit cacheDirChangeRejected(path, reason);
        return;
    }

    m_cacheDir = normalizedPath;
    m_configService->setCacheDir(normalizedPath);
    emit cacheDirChanged();
}

/**
 * @brief 设置磁盘缓存上限并限制在允许范围内。
 * @param maxSizeMB 磁盘缓存上限（MB）。
 */
void ExportSettingsViewModel::setDiskCacheLimitMB(int maxSizeMB) {
    // 将磁盘缓存上限限制在配置服务允许的范围内。
    const int normalizedSize = qBound(1, maxSizeMB, ConfigService::MAX_DISK_CACHE_LIMIT_MB);
    if (m_diskCacheLimitMB != normalizedSize) {
        m_diskCacheLimitMB = normalizedSize;
        m_configService->setDiskCacheLimitMB(normalizedSize);
        ComponentCacheService::instance()->setDiskCacheLimit(normalizedSize);
        emit diskCacheLimitMBChanged();
    }
}

/**
 * @brief 构建导出选项、预加载组件数据并启动导出。
 * @param componentIds 待导出的元器件编号列表。
 */
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

/**
 * @brief 将界面设置转换为导出服务使用的选项对象。
 */
void ExportSettingsViewModel::buildExportOptions() {
    // 根据当前界面设置构造导出服务选项。
    const ExportOptions options = ExportOptionsBuilder::build(*this);

    m_exportService->setOptions(options);
    m_exportService->setOutputPath(options.outputPath);
}

/**
 * @brief 请求导出服务取消当前导出并更新界面状态。
 */
void ExportSettingsViewModel::cancelExport() {
    qDebug() << "Cancelling export";

    if (!m_exportService) {
        return;
    }

    m_exportService->cancelExport();
    setIsExporting(false);
    setStatus("Export cancelled");
}

/**
 * @brief 创建并打开当前输出目录。
 */
bool ExportSettingsViewModel::openOutputFolder() {
    if (m_outputPath.isEmpty()) {
        return false;
    }

    // 保存即将打开的输出目录路径。
    QString pathToOpen = m_outputPath;
    if (!QDir(pathToOpen).exists()) {
        QDir().mkpath(pathToOpen);
    }

    // 使用文件工具打开输出目录。
    FileUtils utils;
    return utils.openFolder(pathToOpen);
}

/**
 * @brief 将预加载进度转换为界面状态文本。
 * @param progress 导出总体进度。
 */
void ExportSettingsViewModel::handlePreloadProgressChanged(const PreloadProgress& progress) {
    Q_UNUSED(progress);
    setStatus(QString("Preloading... %1/%2").arg(progress.completedCount).arg(progress.totalCount));
}

/**
 * @brief 处理预加载完成信号并进入实际导出阶段。
 * @param successCount 成功项目数量。
 * @param failedCount 失败项目数量。
 */
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

/**
 * @brief 处理整体导出进度变化并更新状态文本。
 * @param progress 导出总体进度。
 */
void ExportSettingsViewModel::handleProgressChanged(const ExportOverallProgress& progress) {
    Q_UNUSED(progress);
    setStatus("Exporting...");
}

/**
 * @brief 记录单个导出类型完成后的统计信息。
 * @param typeName 导出类型名称。
 * @param successCount 成功数量。
 * @param failedCount 失败数量。
 * @param skippedCount 跳过数量。
 */
void ExportSettingsViewModel::handleTypeCompleted(const QString& typeName,
                                                  int successCount,
                                                  int failedCount,
                                                  int skippedCount) {
    Q_UNUSED(skippedCount);
    qDebug() << "Type completed:" << typeName << "success=" << successCount << "failed=" << failedCount;
}

/**
 * @brief 处理全部导出完成信号并更新成功或失败状态。
 * @param successCount 成功项目数量。
 * @param failedCount 失败项目数量。
 */
void ExportSettingsViewModel::handleCompleted(int successCount, int failedCount) {
    qDebug() << "Export completed: success=" << successCount << "failed=" << failedCount;
    setIsExporting(false);

    if (failedCount > 0) {
        setStatus(QString("Export completed with errors: %1 failed").arg(failedCount));
    } else {
        setStatus("Export completed successfully");
    }
}

/**
 * @brief 处理导出取消信号并恢复空闲状态。
 */
void ExportSettingsViewModel::handleCancelled() {
    qDebug() << "Export cancelled";
    setIsExporting(false);
    setStatus("Export cancelled");
}

/**
 * @brief 处理导出失败信号并向界面展示错误信息。
 * @param error 错误信息。
 */
void ExportSettingsViewModel::handleFailed(const QString& error) {
    qWarning() << "Export failed:" << error;
    setIsExporting(false);
    setStatus(QString("Export failed: %1").arg(error));
}

/**
 * @brief 更新导出中状态并通知 QML 绑定。
 * @param exporting 是否正在导出。
 */
void ExportSettingsViewModel::setIsExporting(bool exporting) {
    if (m_isExporting != exporting) {
        m_isExporting = exporting;
        emit isExportingChanged();
    }
}

/**
 * @brief 更新状态文本并通知 QML 绑定。
 * @param status 状态文本。
 */
void ExportSettingsViewModel::setStatus(const QString& status) {
    if (m_status != status) {
        m_status = status;
        emit statusChanged();
    }
}

/**
 * @brief 从配置服务加载设置并仅对发生变化的属性发出通知。
 */
void ExportSettingsViewModel::loadFromConfig() {
    // 保存旧值以便比较，仅在值变化时发射信号（避免不必要的 QML 绑定刷新）
    const QString oldOutputPath = m_outputPath;
    // 保存加载配置前的库名称，用于判断是否需要通知界面。
    const QString oldLibName = m_libName;
    // 保存加载配置前的符号导出开关。
    const bool oldExportSymbol = m_exportSymbol;
    // 保存加载配置前的封装导出开关。
    const bool oldExportFootprint = m_exportFootprint;
    // 保存加载配置前的 3D 模型导出开关。
    const bool oldExportModel3D = m_exportModel3D;
    // 保存加载配置前的 3D 模型格式选项。
    const int oldExportModel3DFormat = m_exportModel3DFormat;
    // 保存加载配置前的 3D 模型路径模式。
    const int oldExportModel3DPathMode = m_exportModel3DPathMode;
    // 保存加载配置前的预览图导出开关。
    const bool oldExportPreviewImages = m_exportPreviewImages;
    // 保存加载配置前的数据手册导出开关。
    const bool oldExportDatasheet = m_exportDatasheet;
    // 保存加载配置前的覆盖已有文件开关。
    const bool oldOverwriteExistingFiles = m_overwriteExistingFiles;
    // 保存加载配置前的弱网络模式开关。
    const bool oldWeakNetworkSupport = m_weakNetworkSupport;
    // 保存加载配置前的导出模式。
    const int oldExportMode = m_exportMode;
    // 保存加载配置前的调试模式开关。
    const bool oldDebugMode = m_debugMode;
    // 保存加载配置前的缓存目录路径。
    const QString oldCacheDir = m_cacheDir;
    // 保存加载配置前的磁盘缓存上限。
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
    m_exportMode = qBound(0, m_configService->getExportMode(), 2);
    // 读取配置文件中保存的缓存目录路径。
    const QString configuredCacheDir = m_configService->getCacheDir();
    // 保存安全校验返回的规范化缓存目录路径。
    QString normalizedCacheDir;
    if (CacheSafety::validateSelection(configuredCacheDir, &normalizedCacheDir, nullptr))
        m_cacheDir = normalizedCacheDir;
    else
        m_cacheDir = ComponentCacheService::instance()->cacheDir();
    m_diskCacheLimitMB = m_configService->getDiskCacheLimitMB();

    // 判断调试模式是否由环境变量控制。
    bool envDebugMode = qEnvironmentVariableIsSet("EASYKICONVERTER_DEBUG_MODE");
    if (envDebugMode) {
        // 读取环境变量提供的调试模式文本值。
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

/**
 * @brief 请求配置服务保存当前设置。
 */
void ExportSettingsViewModel::saveConfig() {
    m_configService->saveConfig();
}

/**
 * @brief 恢复界面设置的默认值并同步配置服务。
 */
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
    // 读取配置文件中保存的缓存目录路径。
    const QString configuredCacheDir = m_configService->getCacheDir();
    // 保存安全校验返回的规范化缓存目录路径。
    QString normalizedCacheDir;
    // 保存缓存目录校验失败的具体原因。
    QString cacheDirError;
    if (CacheSafety::validateSelection(configuredCacheDir, &normalizedCacheDir, &cacheDirError)) {
        m_cacheDir = normalizedCacheDir;
    } else {
        // 恢复配置时保留当前已验证的缓存目录，避免无效配置触发迁移或清理。
        m_cacheDir = ComponentCacheService::instance()->cacheDir();
        setStatus(cacheDirError);
    }
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
    if (!ComponentCacheService::instance()->setCacheDir(m_cacheDir, /*migrateExistingCache=*/false)) {
        // 当前目录无法安全恢复时，不覆盖已生效的缓存目录配置。
        m_cacheDir = ComponentCacheService::instance()->cacheDir();
        m_configService->setCacheDir(m_cacheDir);
    }
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
