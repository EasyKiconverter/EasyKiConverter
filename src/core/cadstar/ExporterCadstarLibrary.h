#pragma once

/**
 * @file ExporterCadstarLibrary.h
 * @brief CADSTAR ASCII 符号、封装和器件关联库导出器。
 */

#include "core/interfaces/IFootprintExporter.h"

namespace EasyKiConverter {

/**
 * @brief 将统一 IR 写入可回读的 CADSTAR ASCII 库。
 * @details 该导出器生成文本交换库，不声称生成 CADSTAR 私有数据库。
 *          符号、封装和 Part 在同一个 .lib 中输出，3D 模型由独立阶段输出。
 */
class ExporterCadstarLibrary final : public IFootprintExporter {
public:
    /** @brief 返回 CADSTAR ASCII 库扩展名。 */
    QString libraryFileExtension() const override;

    /** @brief CADSTAR ASCII 库使用单文件输出。 */
    bool isDirectoryOutput() const override;

    /** @brief 导出单个封装为 CADSTAR Package。 */
    bool exportFootprint(const IR::FootprintComponentIR& footprint,
                         const QString& filePath,
                         const QString& model3DPath = QString()) override;

    /** @brief 导出多个封装为 CADSTAR ASCII 库。 */
    bool exportFootprintLibrary(const QList<IR::FootprintComponentIR>& footprints,
                                const QString& libName,
                                const QString& filePath,
                                bool preferWrl = true,
                                bool exportStep = false,
                                const QString& libraryDescription = QString(),
                                const QString& libraryKeywords = QString(),
                                bool useAbsolutePaths = false,
                                const QString& model3DBaseDir = QString()) override;

    /** @brief 导出包含符号、封装和 Part 关联的完整 CADSTAR ASCII 库。 */
    bool exportComponentLibrary(const QList<IR::ComponentIR>& components,
                                const QString& libName,
                                const QString& filePath,
                                bool exportModel3D = false,
                                const QString& model3DBaseDir = QString()) override;

    /** @brief 返回最近一次导出的诊断。 */
    QStringList diagnostics() const override;

private:
    QStringList m_diagnostics;
};

}  // namespace EasyKiConverter
