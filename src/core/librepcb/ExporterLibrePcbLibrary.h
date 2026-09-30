#pragma once

/**
 * @file ExporterLibrePcbLibrary.h
 * @brief LibrePCB 2 原生 .lplib 库导出器。
 */

#include "core/interfaces/IFootprintExporter.h"
#include "core/interfaces/ISymbolExporter.h"

namespace EasyKiConverter {

/**
 * @brief 将统一 IR 写入 LibrePCB 2.1.x 原生库目录。
 *
 * LibrePCB 使用目录型库而非单一库文件。本导出器负责同时生成
 * Symbol、Package、Component 和 Device 元素，以及它们之间的 UUID 引用。
 * 目标格式特有的转换只存在于本类，不扩展通用 IR。
 */
class ExporterLibrePcbLibrary final : public IFootprintExporter, public ISymbolExporter {
public:
    /** @brief 返回 LibrePCB 库目录后缀。 */
    QString libraryFileExtension() const override;

    /** @brief LibrePCB 库使用目录输出。 */
    bool isDirectoryOutput() const override;

    /** @brief 导出单个封装库元素。 */
    bool exportFootprint(const IR::FootprintComponentIR& footprint,
                         const QString& filePath,
                         const QString& model3DPath = QString()) override;

    /** @brief 导出多个封装为 LibrePCB 原生库。 */
    bool exportFootprintLibrary(const QList<IR::FootprintComponentIR>& footprints,
                                const QString& libName,
                                const QString& filePath,
                                bool preferWrl = true,
                                bool exportStep = false,
                                const QString& libraryDescription = QString(),
                                const QString& libraryKeywords = QString(),
                                bool useAbsolutePaths = false,
                                const QString& model3DBaseDir = QString()) override;

    /** @brief 导出仅包含符号元素的 LibrePCB 原生库。 */
    bool exportSymbolLibrary(const QList<IR::SymbolComponentIR>& symbols,
                             const QString& libName,
                             const QString& filePath) override;

    /** @brief 通过统一符号接口导出 LibrePCB 符号库。 */
    bool exportSymbolLibrary(const QList<IR::SymbolComponentIR>& symbols,
                             const QString& libName,
                             const QString& filePath,
                             bool appendMode,
                             bool updateMode,
                             const QString& libraryDescription = QString()) override;

    /** @brief 导出单个符号元素。 */
    bool exportSymbol(const IR::SymbolComponentIR& symbol, const QString& filePath) override;

    /** @brief 导出包含符号、封装、器件关联和三维引用的完整库。 */
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
