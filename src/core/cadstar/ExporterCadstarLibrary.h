#pragma once

/**
 * @file ExporterCadstarLibrary.h
 * @brief CADSTAR ASCII 符号、封装和器件关联库导出器。
 */

#include "core/interfaces/IFootprintExporter.h"
#include "core/interfaces/ISymbolExporter.h"

namespace EasyKiConverter {

/**
 * @brief 将统一 IR 写入可回读的 CADSTAR ASCII 库。
 * @details 该导出器生成文本交换库，不声称生成 CADSTAR 私有数据库。
 *          符号、封装和 Part 在同一个 .lib 中输出，3D 模型由独立阶段输出。
 */
class ExporterCadstarLibrary final : public IFootprintExporter, public ISymbolExporter {
public:
    /**
     * @brief 返回 CADSTAR ASCII 库扩展名。
     * @return 目标库文件扩展名。
     */
    QString libraryFileExtension() const override;

    /**
     * @brief CADSTAR ASCII 库使用单文件输出。
     * @return 始终返回 false。
     */
    bool isDirectoryOutput() const override;

    /**
     * @brief 导出单个封装为 CADSTAR Package。
     * @param footprint 待导出的封装 IR。
     * @param filePath 输出文件路径。
     * @param model3DPath 可选 3D 模型路径。
     * @return 导出成功时返回 true。
     */
    bool exportFootprint(const IR::FootprintComponentIR& footprint,
                         const QString& filePath,
                         const QString& model3DPath = QString()) override;

    /**
     * @brief 导出多个封装为 CADSTAR ASCII 库。
     * @param footprints 待导出的封装列表。
     * @param libName 库名称。
     * @param filePath 输出文件路径。
     * @param preferWrl 是否优先使用 WRL 模型。
     * @param exportStep 是否导出 STEP 模型。
     * @param libraryDescription 库描述。
     * @param libraryKeywords 库关键词。
     * @param useAbsolutePaths 是否使用绝对模型路径。
     * @param model3DBaseDir 3D 模型基础目录。
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

    /**
     * @brief 写入仅包含 Component 符号定义的 CADSTAR ASCII 库。
     * @param symbols 待导出的符号列表。
     * @param libName 库名称。
     * @param filePath 输出文件路径。
     * @return 导出成功时返回 true。
     */
    bool exportSymbolLibrary(const QList<IR::SymbolComponentIR>& symbols,
                             const QString& libName,
                             const QString& filePath) override;

    /**
     * @brief 通过通用符号接口导出 CADSTAR ASCII 符号库。
     * @param symbols 待导出的符号列表。
     * @param libName 库名称。
     * @param filePath 输出文件路径。
     * @param appendMode 是否追加已有内容。
     * @param updateMode 是否更新已有内容。
     * @param libraryDescription 库描述。
     * @return 导出成功时返回 true。
     */
    bool exportSymbolLibrary(const QList<IR::SymbolComponentIR>& symbols,
                             const QString& libName,
                             const QString& filePath,
                             bool appendMode,
                             bool updateMode,
                             const QString& libraryDescription = QString()) override;

    /**
     * @brief 通过通用符号接口导出单个 CADSTAR 符号。
     * @param symbol 待导出的符号 IR。
     * @param filePath 输出文件路径。
     * @return 导出成功时返回 true。
     */
    bool exportSymbol(const IR::SymbolComponentIR& symbol, const QString& filePath) override;

    /**
     * @brief 导出包含符号、封装和 Part 关联的完整 CADSTAR ASCII 库。
     * @param components 待导出的器件列表。
     * @param libName 库名称。
     * @param filePath 输出文件路径。
     * @param exportModel3D 是否导出 3D 模型。
     * @param model3DBaseDir 3D 模型基础目录。
     * @return 导出成功时返回 true。
     */
    bool exportComponentLibrary(const QList<IR::ComponentIR>& components,
                                const QString& libName,
                                const QString& filePath,
                                bool exportModel3D = false,
                                const QString& model3DBaseDir = QString()) override;

    /**
     * @brief 返回最近一次导出的诊断。
     * @return 诊断信息列表。
     */
    QStringList diagnostics() const override;

private:
    QStringList m_diagnostics;  ///< 最近一次导出的诊断信息。
};

}  // namespace EasyKiConverter
