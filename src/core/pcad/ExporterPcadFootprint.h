#pragma once

/**
 * @file ExporterPcadFootprint.h
 * @brief P-CAD ASCII PCB 库导出器。
 */

#include "core/interfaces/IFootprintExporter.h"

namespace EasyKiConverter {

/**
 * @brief 将统一封装 IR 写入 P-CAD ASCII PCB Library（`.lia`）。
 * @details 符号和器件关联由 ExporterPcadSymbol 配套写入，三维模型由独立模型阶段写入。
 */
class ExporterPcadFootprint final : public IFootprintExporter {
public:
    /** @brief 获取 P-CAD ASCII PCB Library 文件扩展名。 */
    QString libraryFileExtension() const override;

    /** @brief P-CAD ASCII PCB Library 使用单文件输出。 */
    bool isDirectoryOutput() const override;

    /**
     * @brief 导出包含单个 Pattern 的 P-CAD ASCII 库文件。
     * @param footprint 待导出的封装 IR。
     * @param filePath 输出库文件路径。
     * @param model3DPath 可选三维模型路径。
     * @return 导出成功时返回 true。
     */
    bool exportFootprint(const IR::FootprintComponentIR& footprint,
                         const QString& filePath,
                         const QString& model3DPath = QString()) override;

    /**
     * @brief 导出多个 Pattern 及其去重后的 Pad Style。
     * @param footprints 待导出的封装列表。
     * @param libName 库名称。
     * @param filePath 输出库文件路径。
     * @param preferWrl 是否优先使用 WRL 模型。
     * @param exportStep 是否导出 STEP 模型。
     * @param libraryDescription 库描述。
     * @param libraryKeywords 库关键字。
     * @param useAbsolutePaths 是否使用绝对模型路径。
     * @param model3DBaseDir 三维模型基础目录。
     * @return 导出成功时返回 true。
     */
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
    QStringList m_diagnostics;  ///< 最近一次导出的诊断信息。
};

}  // namespace EasyKiConverter
