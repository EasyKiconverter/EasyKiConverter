#pragma once

/**
 * @file ExporterPadsFootprint.h
 * @brief PADS Parts Library ASCII PCB Decal 导出器。
 */

#include "core/interfaces/IFootprintExporter.h"

namespace EasyKiConverter {

/**
 * @brief 将统一封装 IR 写入 PADS PCB Decal ASCII 文件。
 * @details 每个封装输出为独立的 `.d` 文件；不生成 PADS 原生二进制库或原理图库。
 */
class ExporterPadsFootprint final : public IFootprintExporter {
public:
    /**
     * @brief 获取 PADS 封装库目录后缀。
     * @return PADS 封装库目录后缀。
     */
    QString libraryFileExtension() const override;

    /**
     * @brief 判断 PADS 封装库是否使用目录保存多个 `.d` Decal 文件。
     * @return PADS 输出始终为目录时返回 true。
     */
    bool isDirectoryOutput() const override;

    /**
     * @brief 导出单个 PADS PCB Decal。
     * @param footprint 待导出的封装。
     * @param filePath 输出文件路径。
     * @param model3DPath 未使用的三维模型路径参数。
     * @return 导出成功时返回 true。
     */
    bool exportFootprint(const IR::FootprintComponentIR& footprint,
                         const QString& filePath,
                         const QString& model3DPath = QString()) override;

    /**
     * @brief 导出多个 PADS PCB Decal 文件。
     * @param footprints 待导出的封装列表。
     * @param libName 目标库名称。
     * @param filePath 输出目录路径。
     * @param preferWrl 是否优先使用 WRL 模型。
     * @param exportStep 是否导出 STEP 模型。
     * @param libraryDescription 库描述文本。
     * @param libraryKeywords 库关键词文本。
     * @param useAbsolutePaths 是否使用绝对模型路径。
     * @param model3DBaseDir 三维模型基础目录。
     * @return 全部封装导出成功时返回 true。
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

    /**
     * @brief 获取最近一次导出的诊断信息。
     * @return 最近一次导出的诊断信息列表。
     */
    QStringList diagnostics() const override;

private:
    QStringList m_diagnostics;  ///< 最近一次导出的诊断信息。
};

}  // namespace EasyKiConverter
