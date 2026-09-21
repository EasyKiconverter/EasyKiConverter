#pragma once

/**
 * @file ExporterPcadFootprint.h
 * @brief P-CAD ASCII PCB 封装库导出器。
 */

#include "core/interfaces/IFootprintExporter.h"

namespace EasyKiConverter {

/**
 * @brief 将统一封装 IR 写入 P-CAD ASCII PCB Library（`.lia`）。
 * @details 符号库由 ExporterPcadSymbol 单独写入，三维模型由独立模型阶段写入。
 */
class ExporterPcadFootprint final : public IFootprintExporter {
public:
    /** @brief 获取 P-CAD ASCII PCB Library 文件扩展名。 */
    QString libraryFileExtension() const override;

    /** @brief P-CAD ASCII PCB Library 使用单文件输出。 */
    bool isDirectoryOutput() const override;

    /** @brief 导出包含单个 Pattern 的 P-CAD ASCII 库文件。 */
    bool exportFootprint(const IR::FootprintComponentIR& footprint,
                         const QString& filePath,
                         const QString& model3DPath = QString()) override;

    /** @brief 导出多个 Pattern 及其去重后的 Pad Style。 */
    bool exportFootprintLibrary(const QList<IR::FootprintComponentIR>& footprints,
                                const QString& libName,
                                const QString& filePath,
                                bool preferWrl = true,
                                bool exportStep = false,
                                const QString& libraryDescription = QString(),
                                const QString& libraryKeywords = QString(),
                                bool useAbsolutePaths = false,
                                const QString& model3DBaseDir = QString()) override;

    /** @brief 获取最近一次导出的诊断信息。 */
    QStringList diagnostics() const override;

private:
    QStringList m_diagnostics;
};

}  // namespace EasyKiConverter
