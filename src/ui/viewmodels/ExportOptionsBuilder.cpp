#include "ExportOptionsBuilder.h"

#include "ExportSettingsViewModel.h"

#include <QDir>
#include <QStandardPaths>

namespace EasyKiConverter {

// 解析输出目录，确保空路径和相对路径都落到用户文档目录下。
static QString resolveOutputPath(const QString& configuredPath, const QString& libName) {
    QString outputPath = configuredPath;
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

// 将目标格式转换为日志可读名称，保持目标格式来源只有一个。
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
    return "KiCad";
}

// 组装所有导出选项并输出一次完整的诊断快照。
ExportOptions ExportOptionsBuilder::build(const ExportSettingsViewModel& viewModel) {
    ExportOptions options;
    options.outputPath = resolveOutputPath(viewModel.m_outputPath, viewModel.m_libName);
    if (options.outputPath.isEmpty()) {
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
                               ? static_cast<TargetEdaFormat>(viewModel.m_targetModel->currentIndex())
                               : TargetEdaFormat::KiCad;
    // Xpedition ZIP 只能完整替换，界面的“覆盖”选项不能被误传成更新模式。
    const bool xpeditionTarget = options.targetFormat == TargetEdaFormat::Xpedition;
    const bool allegroTarget = options.targetFormat == TargetEdaFormat::Allegro;
    const bool padsTarget = options.targetFormat == TargetEdaFormat::Pads;
    const bool eagleTarget = options.targetFormat == TargetEdaFormat::Eagle;
    if (allegroTarget || eagleTarget)
        options.exportSymbol = false;
    if (padsTarget) {
        options.exportSymbol = false;
        options.exportModel3D = false;
    }
    if (eagleTarget) {
        options.exportSymbol = false;
        options.exportModel3D = false;
    }
    options.overwriteExistingFiles =
        viewModel.m_overwriteExistingFiles || (xpeditionTarget && viewModel.m_exportMode == 1);
    options.updateMode =
        viewModel.m_exportMode == 1 && !xpeditionTarget && !allegroTarget && !padsTarget && !eagleTarget;

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
