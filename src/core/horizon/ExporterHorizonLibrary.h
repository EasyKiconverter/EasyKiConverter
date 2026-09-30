#pragma once

#include "core/interfaces/IFootprintExporter.h"
#include "core/interfaces/ISymbolExporter.h"

namespace EasyKiConverter {

/** @brief 将通用 IR 写入 Horizon Pool source files 的导出器。 */
class ExporterHorizonLibrary final : public IFootprintExporter, public ISymbolExporter {
public:
    /** @brief 返回 Horizon Pool 目录输出后缀。 */
    QString libraryFileExtension() const override;
    /** @brief Horizon Pool 使用目录型输出。 */
    bool isDirectoryOutput() const override;

    /** @brief 导出单个封装及其 Padstack source files。 */
    bool exportFootprint(const IR::FootprintComponentIR& footprint,
                         const QString& filePath,
                         const QString& model3DPath = QString()) override;

    /** @brief 将多个封装导出到 Horizon Pool。 */
    bool exportFootprintLibrary(const QList<IR::FootprintComponentIR>& footprints,
                                const QString& libName,
                                const QString& filePath,
                                bool preferWrl = true,
                                bool exportStep = false,
                                const QString& libraryDescription = QString(),
                                const QString& libraryKeywords = QString(),
                                bool useAbsolutePaths = false,
                                const QString& model3DBaseDir = QString()) override;

    /** @brief 导出单个符号到 Horizon Pool。 */
    bool exportSymbol(const IR::SymbolComponentIR& symbol, const QString& filePath) override;
    /** @brief 将多个符号、Unit 和 Entity 导出到 Horizon Pool。 */
    bool exportSymbolLibrary(const QList<IR::SymbolComponentIR>& symbols,
                             const QString& libName,
                             const QString& filePath,
                             bool appendMode = true,
                             bool updateMode = false,
                             const QString& libraryDescription = QString()) override;

    /** @brief 导出包含符号、封装、Part 和 pad_map 的完整组件库。 */
    bool exportComponentLibrary(const QList<IR::ComponentIR>& components,
                                const QString& libName,
                                const QString& filePath,
                                bool exportModel3D = false,
                                const QString& model3DBaseDir = QString()) override;

    /** @brief 返回最近一次导出的诊断信息。 */
    QStringList diagnostics() const override;

private:
    QStringList m_diagnostics;
};

}  // namespace EasyKiConverter
