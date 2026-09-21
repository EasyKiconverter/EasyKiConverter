#pragma once

/**
 * @file ExporterEagleFootprint.h
 * @brief Eagle XML 组合库导出器。
 */

#include "core/interfaces/IFootprintExporter.h"

namespace EasyKiConverter {

/**
 * @brief 将统一组件 IR 写入 Eagle XML library 文件。
 * @details 在同一个 .lbr 中输出 package、symbol、deviceset 和 pin-to-pad 关联；三维文件由独立阶段输出。
 */
class ExporterEagleFootprint final : public IFootprintExporter {
public:
    /** @brief 获取 Eagle library 文件扩展名。 */
    QString libraryFileExtension() const override;

    /** @brief Eagle 封装库输出为单个 XML 文件。 */
    bool isDirectoryOutput() const override;

    /** @brief 导出包含单个 package 的 Eagle XML 文件。 */
    bool exportFootprint(const IR::FootprintComponentIR& footprint,
                         const QString& filePath,
                         const QString& model3DPath = QString()) override;

    /** @brief 导出包含多个 package 的 Eagle XML library 文件。 */
    bool exportFootprintLibrary(const QList<IR::FootprintComponentIR>& footprints,
                                const QString& libName,
                                const QString& filePath,
                                bool preferWrl = true,
                                bool exportStep = false,
                                const QString& libraryDescription = QString(),
                                const QString& libraryKeywords = QString(),
                                bool useAbsolutePaths = false,
                                const QString& model3DBaseDir = QString()) override;

    /** @brief 写入仅包含 Symbol 定义的 Eagle XML 库。 */
    bool exportSymbolLibrary(const QList<IR::SymbolComponentIR>& symbols,
                             const QString& libName,
                             const QString& filePath) override;

    /** @brief 写入 Eagle XML 的完整 Symbol、Package 和 DeviceSet 关联库。 */
    bool exportComponentLibrary(const QList<IR::ComponentIR>& components,
                                const QString& libName,
                                const QString& filePath,
                                bool exportModel3D = false,
                                const QString& model3DBaseDir = QString()) override;

    /** @brief 获取最近一次导出的诊断信息。 */
    QStringList diagnostics() const override;

private:
    QStringList m_diagnostics;
};

}  // namespace EasyKiConverter
