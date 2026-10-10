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
    /**
     * @brief 获取 Import Package 目录后缀。
     * @return Import Package 目录后缀。
     */
    QString libraryFileExtension() const override;

    /**
     * @brief 判断 Allegro Import Package 是否使用目录输出。
     * @return 始终返回 true。
     */
    bool isDirectoryOutput() const override;

    /**
     * @brief 导出一个封装到 Import Package 目录。
     * @param footprint 待导出的封装 IR。
     * @param filePath Import Package 输出目录。
     * @param model3DPath 可选的三维模型路径。
     * @return 导出成功返回 true，否则返回 false。
     */
    bool exportFootprint(const IR::FootprintComponentIR& footprint,
                         const QString& filePath,
                         const QString& model3DPath = QString()) override;

    /**
     * @brief 导出多个封装及其规范化依赖文件。
     * @param footprints 待导出的封装 IR 列表。
     * @param libName 输出库名称。
     * @param filePath Import Package 输出目录。
     * @param preferWrl 是否优先使用 WRL 模型（Allegro Import Package 当前不使用）。
     * @param exportStep 是否导出 STEP 模型。
     * @param libraryDescription 库描述。
     * @param libraryKeywords 库关键词。
     * @param useAbsolutePaths 是否使用绝对模型路径（Import Package 当前不使用）。
     * @param model3DBaseDir 三维模型基础目录。
     * @return 导出成功返回 true，否则返回 false。
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
     * @brief 导出仅包含符号规范化数据的 Allegro Import Package。
     * @param symbols 待导出的符号 IR 列表。
     * @param libName 输出库名称。
     * @param filePath Import Package 输出目录。
     * @return 导出成功返回 true，否则返回 false。
     */
    bool exportSymbolLibrary(const QList<IR::SymbolComponentIR>& symbols,
                             const QString& libName,
                             const QString& filePath) override;

    /**
     * @brief 通过通用符号接口导出 Allegro 语义 Import Package。
     * @param symbols 待导出的符号 IR 列表。
     * @param libName 输出库名称。
     * @param filePath Import Package 输出目录。
     * @param appendMode 是否追加到已有库。
     * @param updateMode 是否更新已有库。
     * @param libraryDescription 库描述。
     * @return 导出成功返回 true，否则返回 false。
     */
    bool exportSymbolLibrary(const QList<IR::SymbolComponentIR>& symbols,
                             const QString& libName,
                             const QString& filePath,
                             bool appendMode,
                             bool updateMode,
                             const QString& libraryDescription = QString()) override;

    /**
     * @brief 导出单个 Allegro 符号规范化数据。
     * @param symbol 待导出的符号。
     * @param filePath Import Package 输出目录。
     * @return 导出成功返回 true，否则返回 false。
     */
    bool exportSymbol(const IR::SymbolComponentIR& symbol, const QString& filePath) override;

    /**
     * @brief 导出同时包含符号、封装和关联关系的 Allegro Import Package。
     * @param components 待导出的完整组件 IR 列表。
     * @param libName 输出库名称。
     * @param filePath Import Package 输出目录。
     * @param exportModel3D 是否导出三维模型。
     * @param model3DBaseDir 三维模型基础目录。
     * @return 导出成功返回 true，否则返回 false。
     */
    bool exportComponentLibrary(const QList<IR::ComponentIR>& components,
                                const QString& libName,
                                const QString& filePath,
                                bool exportModel3D = false,
                                const QString& model3DBaseDir = QString()) override;

    /**
     * @brief 获取最近一次导出的诊断。
     * @return 最近一次导出的诊断信息列表。
     */
    QStringList diagnostics() const override;

private:
    QStringList m_diagnostics;  ///< 最近一次导出的诊断信息列表。
};

}  // namespace EasyKiConverter
