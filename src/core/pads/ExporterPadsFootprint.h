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
    /** @brief 获取 PADS 封装库目录后缀。 */
    QString libraryFileExtension() const override;

    /** @brief PADS 封装库使用目录保存多个 `.d` Decal 文件。 */
    bool isDirectoryOutput() const override;

    /** @brief 导出单个 PADS PCB Decal。 */
    bool exportFootprint(const IR::FootprintComponentIR& footprint,
                         const QString& filePath,
                         const QString& model3DPath = QString()) override;

    /** @brief 导出多个 PADS PCB Decal 文件。 */
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
