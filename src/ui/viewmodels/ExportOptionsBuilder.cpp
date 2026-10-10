#include "ExportOptionsBuilder.h"

#include "ExportSettingsViewModel.h"

#include <QDir>
#include <QStandardPaths>

namespace EasyKiConverter {

/**
 * @brief 解析输出目录，确保空路径和相对路径都落到用户文档目录下。
 * @param configuredPath 用户配置的输出路径。
 * @param libName 导出库名称。
 * @return 规范化后的输出目录路径。
 */
static QString resolveOutputPath(const QString& configuredPath, const QString& libName) {
    // 保存待解析的输出路径。
    QString outputPath = configuredPath;
    // 获取用户文档目录作为相对路径的默认根目录。
    const QString documentsPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    QDir exportRoot(documentsPath);
    if (!exportRoot.exists(QStringLiteral("EasyKiConverter")))
        exportRoot.mkdir(QStringLiteral("EasyKiConverter"));
    exportRoot.cd(QStringLiteral("EasyKiConverter"));

    if (outputPath.isEmpty())
        return exportRoot.absoluteFilePath(libName);

    QDir outputDir(outputPath);
    if (outputDir.isAbsolute())
        return outputDir.cleanPath(outputPath);
    return exportRoot.absoluteFilePath(outputPath);
}

/**
 * @brief 将目标格式转换为日志可读名称，保持目标格式来源只有一个。
 * @param format 目标 EDA 格式枚举值。
 * @return 目标格式的日志名称。
 */
static const char* targetFormatName(TargetEdaFormat format) {
    if (format == TargetEdaFormat::Altium)
        return "Altium";
    if (format == TargetEdaFormat::Xpedition)
        return "Xpedition";
    if (format == TargetEdaFormat::Allegro)
        return "Allegro";
    if (format == TargetEdaFormat::Pads)
        return "PADS";
    if (format == TargetEdaFormat::Eagle)
        return "Eagle";
    if (format == TargetEdaFormat::Cadstar)
        return "CADSTAR";
    if (format == TargetEdaFormat::Pcad)
        return "P-CAD";
    if (format == TargetEdaFormat::Orcad)
        return "OrCAD Capture";
    if (format == TargetEdaFormat::LibrePcb)
        return "LibrePCB";
    if (format == TargetEdaFormat::Horizon)
        return "Horizon EDA";
    return "KiCad";
}

/**
 * @brief 组装所有导出选项并输出一次完整的诊断快照。
 * @param viewModel 导出设置视图模型。
 * @return 已组装的导出选项。
 */
ExportOptions ExportOptionsBuilder::build(const ExportSettingsViewModel& viewModel) {
    // 保存组装后的导出选项。
    ExportOptions options;
    options.outputPath = resolveOutputPath(viewModel.m_outputPath, viewModel.m_libName);
    if (options.outputPath.isEmpty()) {
        // 获取桌面目录作为输出路径的最终回退位置。
        const QString desktopPath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
        options.outputPath = QDir(desktopPath).absoluteFilePath(viewModel.m_libName);
    }

    options.libName = viewModel.m_libName;
    options.exportSymbol = viewModel.m_exportSymbol;
    options.exportFootprint = viewModel.m_exportFootprint;
    options.exportModel3D = viewModel.m_exportModel3D;
    options.exportModel3DFormat = viewModel.m_exportModel3DFormat;
    options.exportModel3DPathMode = viewModel.m_exportModel3DPathMode;
    options.exportPreviewImages = viewModel.m_exportPreviewImages;
    options.exportDatasheet = viewModel.m_exportDatasheet;
    options.weakNetworkSupport = viewModel.m_weakNetworkSupport;
    options.debugMode = viewModel.m_debugMode;
    options.exportSymbolDescription = viewModel.m_exportSymbolDescription;
    options.exportFootprintDescription = viewModel.m_exportFootprintDescription;
    options.symbolLibraryDescription = viewModel.m_symbolLibraryDescription;
    options.footprintLibraryDescription = viewModel.m_footprintLibraryDescription;
    options.footprintLibraryKeywords = viewModel.m_footprintLibraryKeywords;
    options.targetFormat = viewModel.m_targetModel
                               ? static_cast<TargetEdaFormat>(viewModel.m_targetModel->currentTargetFormat())
                               : TargetEdaFormat::KiCad;
    // Xpedition ZIP 只能完整替换，界面的“覆盖”选项不能被误传成更新模式。
    // 标记各类目标格式是否需要特殊的完整替换策略。
    const bool xpeditionTarget = options.targetFormat == TargetEdaFormat::Xpedition;
    // 标记是否选择 Allegro 目标格式。
    const bool allegroTarget = options.targetFormat == TargetEdaFormat::Allegro;
    // 标记是否选择 PADS 目标格式。
    const bool padsTarget = options.targetFormat == TargetEdaFormat::Pads;
    // 标记是否选择 Eagle 目标格式。
    const bool eagleTarget = options.targetFormat == TargetEdaFormat::Eagle;
    // 标记是否选择 P-CAD 目标格式。
    const bool pcadTarget = options.targetFormat == TargetEdaFormat::Pcad;
    // 标记是否选择 OrCAD 目标格式。
    const bool orcadTarget = options.targetFormat == TargetEdaFormat::Orcad;
    // 标记是否选择 LibrePCB 目标格式。
    const bool librePcbTarget = options.targetFormat == TargetEdaFormat::LibrePcb;
    // 汇总所有仅支持完整替换的目标格式。
    const bool requiresFullReplacement = xpeditionTarget || allegroTarget || padsTarget || eagleTarget || pcadTarget ||
                                         orcadTarget || librePcbTarget ||
                                         options.targetFormat == TargetEdaFormat::Cadstar;
    if (allegroTarget)
        options.exportSymbol = false;
    if (orcadTarget)
        options.exportFootprint = false;
    options.overwriteExistingFiles = viewModel.m_overwriteExistingFiles || viewModel.m_exportMode == 2;
    options.updateMode = viewModel.m_exportMode == 1 && !requiresFullReplacement;

    qInfo() << "Export options:" << "OutputPath:" << options.outputPath << "LibName:" << options.libName
            << "TargetFormat:" << targetFormatName(options.targetFormat) << "Symbol:" << options.exportSymbol
            << "Footprint:" << options.exportFootprint << "3D Model:" << options.exportModel3D
            << "3D Model Format:" << options.exportModel3DFormat << "(1=WRL, 2=STEP, 3=Both)"
            << "3D Model Path Mode:" << options.exportModel3DPathMode << "(0=Relative, 1=Absolute)"
            << "Preview Images:" << options.exportPreviewImages << "Datasheet:" << options.exportDatasheet
            << "Client Weak Network Adaptation:" << options.weakNetworkSupport << "Update Mode:" << options.updateMode
            << "Debug Mode:" << options.debugMode << "Symbol Description:" << options.exportSymbolDescription
            << "Footprint Description:" << options.exportFootprintDescription
            << "Symbol Library Description:" << options.symbolLibraryDescription
            << "Footprint Library Description:" << options.footprintLibraryDescription;
    return options;
}

}  // namespace EasyKiConverter
