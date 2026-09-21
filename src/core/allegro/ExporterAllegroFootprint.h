#pragma once

#include "core/interfaces/IFootprintExporter.h"
#include "core/interfaces/ISymbolExporter.h"

namespace EasyKiConverter {

/**
 * @brief 生成 Allegro PCB Footprint Import Package。
 * @details 不直接写入 Cadence 私有 .dra/.psm 数据库；输出由用户安装的 Allegro 工具继续生成原生库。
 */
class ExporterAllegroFootprint final : public IFootprintExporter, public ISymbolExporter {
public:
    /** @brief 获取 Import Package 目录后缀。 */
    QString libraryFileExtension() const override;

    /** @brief Allegro Import Package 使用目录输出。 */
    bool isDirectoryOutput() const override;

    /** @brief 导出一个封装到 Import Package 目录。 */
    bool exportFootprint(const IR::FootprintComponentIR& footprint,
                         const QString& filePath,
                         const QString& model3DPath = QString()) override;

    /** @brief 导出多个封装及其规范化依赖文件。 */
    bool exportFootprintLibrary(const QList<IR::FootprintComponentIR>& footprints,
                                const QString& libName,
                                const QString& filePath,
                                bool preferWrl = true,
                                bool exportStep = false,
                                const QString& libraryDescription = QString(),
                                const QString& libraryKeywords = QString(),
                                bool useAbsolutePaths = false,
                                const QString& model3DBaseDir = QString()) override;

    /** @brief 导出仅包含符号规范化数据的 Allegro Import Package。 */
    bool exportSymbolLibrary(const QList<IR::SymbolComponentIR>& symbols,
                             const QString& libName,
                             const QString& filePath) override;

    /** @brief 通过通用符号接口导出 Allegro 语义 Import Package。 */
    bool exportSymbolLibrary(const QList<IR::SymbolComponentIR>& symbols,
                             const QString& libName,
                             const QString& filePath,
                             bool appendMode,
                             bool updateMode,
                             const QString& libraryDescription = QString()) override;

    /** @brief 导出单个 Allegro 符号规范化数据。 */
    bool exportSymbol(const IR::SymbolComponentIR& symbol, const QString& filePath) override;

    /** @brief 导出同时包含符号、封装和关联关系的 Allegro Import Package。 */
    bool exportComponentLibrary(const QList<IR::ComponentIR>& components,
                                const QString& libName,
                                const QString& filePath,
                                bool exportModel3D = false,
                                const QString& model3DBaseDir = QString()) override;

    /** @brief 获取最近一次导出的诊断。 */
    QStringList diagnostics() const override;

private:
    QStringList m_diagnostics;
};

}  // namespace EasyKiConverter
