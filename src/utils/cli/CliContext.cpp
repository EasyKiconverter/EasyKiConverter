#include "CliContext.h"

#include "services/ComponentService.h"
#include "services/export/ParallelExportService.h"
#include "utils/PathSecurity.h"

#include <QDebug>

namespace EasyKiConverter {

static int model3DFormatFromString(const QString& format) {
    if (format == QStringLiteral("step")) {
        return ExportOptions::MODEL_3D_FORMAT_STEP;
    }
    if (format == QStringLiteral("both")) {
        return ExportOptions::MODEL_3D_FORMAT_BOTH;
    }
    return ExportOptions::MODEL_3D_FORMAT_WRL;
}

CliContext::CliContext(const CommandLineParser& parser) : m_parser(parser) {
    // 创建服务实例
    m_componentService = new ComponentService();
    m_exportService = new ParallelExportService();
    m_exportService->setComponentService(m_componentService);
}

CliContext::~CliContext() {
    if (m_exportService) {
        m_exportService->cancelExport();
        delete m_exportService;
    }
    if (m_componentService) {
        delete m_componentService;
    }
}

// 将命令行参数转换为导出服务使用的完整选项。
ExportOptions CliContext::createExportOptions() const {
    ExportOptions options;

    // 设置输出路径
    options.outputPath = m_parser.outputDir();
    // 清洗 libName，防止路径穿越（如 ../foo）
    options.libName = PathSecurity::sanitizeFilename(m_parser.libName());

    // 设置导出类型（CLI 默认：符号库、封装库；3D 模型通过参数启用）
    options.exportSymbol = m_parser.exportSymbol();
    options.exportFootprint = m_parser.exportFootprint();
    options.exportModel3D = m_parser.export3DModel();
    options.exportModel3DFormat = model3DFormatFromString(m_parser.model3DFormat());
    options.exportPreviewImages = m_parser.exportPreview();
    options.exportDatasheet = m_parser.exportDatasheet();

    // 设置默认值
    options.debugMode = m_parser.isDebugMode();

    // 弱网模式
    options.weakNetworkSupport = m_parser.weakNetworkSupport();

    // 更新模式
    options.updateMode = m_parser.updateMode();

    // 3D 模型路径模式
    const QString pathMode = m_parser.model3DPathMode();
    options.exportModel3DPathMode = (pathMode == QStringLiteral("absolute")) ? ExportOptions::MODEL_3D_PATH_ABSOLUTE
                                                                             : ExportOptions::MODEL_3D_PATH_RELATIVE;

    // 覆盖模式（默认 true，--no-overwrite 禁用）
    options.overwriteExistingFiles = m_parser.overwriteExistingFiles();

    // 库描述
    options.symbolLibraryDescription = m_parser.symbolDescription();
    options.footprintLibraryDescription = m_parser.footprintDescription();

    // 目标 EDA 格式
    const QString format = m_parser.targetFormat();
    if (format == QStringLiteral("altium")) {
        options.targetFormat = TargetEdaFormat::Altium;
    } else if (format == QStringLiteral("xpedition")) {
        options.targetFormat = TargetEdaFormat::Xpedition;
    } else if (format == QStringLiteral("allegro")) {
        options.targetFormat = TargetEdaFormat::Allegro;
    } else if (format == QStringLiteral("pads")) {
        options.targetFormat = TargetEdaFormat::Pads;
    } else if (format == QStringLiteral("eagle")) {
        options.targetFormat = TargetEdaFormat::Eagle;
    } else if (format == QStringLiteral("pcad")) {
        options.targetFormat = TargetEdaFormat::Pcad;
    } else {
        options.targetFormat = TargetEdaFormat::KiCad;
    }
    if (options.targetFormat == TargetEdaFormat::Altium &&
        (options.exportModel3DFormat & ExportOptions::MODEL_3D_FORMAT_WRL)) {
        // Altium PcbLib 仅可靠嵌入 STEP；CLI 即使收到 wrl/both 也自动收敛到 STEP。
        options.exportModel3DFormat = ExportOptions::MODEL_3D_FORMAT_STEP;
    }
    if (options.targetFormat == TargetEdaFormat::Xpedition && options.exportModel3D) {
        qWarning() << "Xpedition 目标当前不支持 3D 模型关联，已忽略 --3d-model";
        options.exportModel3D = false;
    }
    if (options.targetFormat == TargetEdaFormat::Allegro) {
        options.exportSymbol = false;
        if (options.exportModel3D)
            options.exportModel3DFormat = ExportOptions::MODEL_3D_FORMAT_STEP;
    }
    if (options.targetFormat == TargetEdaFormat::Pcad) {
        options.exportSymbol = false;
    }

    return options;
}

}  // namespace EasyKiConverter
