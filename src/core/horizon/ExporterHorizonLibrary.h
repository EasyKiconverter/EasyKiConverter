#pragma once

#include "core/interfaces/IFootprintExporter.h"
#include "core/interfaces/ISymbolExporter.h"

namespace EasyKiConverter {

/**
 * @brief 将通用 IR 写入 Horizon Pool source files 的导出器。
 */
class ExporterHorizonLibrary final : public IFootprintExporter, public ISymbolExporter {
public:
    /**
     * @brief 返回 Horizon Pool 目录输出后缀。
     * @return Horizon Pool 目录输出后缀。
     */
    QString libraryFileExtension() const override;
    /**
     * @brief 判断 Horizon Pool 是否使用目录型输出。
     * @return 始终返回 true。
     */
    bool isDirectoryOutput() const override;

    /**
     * @brief 导出单个封装及其 Padstack source files。
     * @param footprint 封装 IR 数据。
     * @param filePath Horizon Pool 根目录。
     * @param model3DPath 兼容通用接口的参数；Horizon 始终嵌入 IR 中的模型数据并忽略此路径。
     */
    bool exportFootprint(const IR::FootprintComponentIR& footprint,
                         const QString& filePath,
                         const QString& model3DPath = QString()) override;

    /**
     * @brief 将多个封装导出到 Horizon Pool。
     * @param footprints 封装 IR 列表。
     * @param libName Pool 名称。
     * @param filePath Horizon Pool 根目录。
     * @param preferWrl 兼容通用接口的参数；Horizon 按 IR 中实际可用模型格式写入。
     * @param exportStep 是否写入 IR 中的 STEP 数据。
     * @param libraryDescription 兼容通用接口的库描述参数，Horizon 当前不单独序列化。
     * @param libraryKeywords 兼容通用接口的关键词参数，Horizon 当前不单独序列化。
     * @param useAbsolutePaths 兼容通用接口的路径模式参数；Horizon 使用 Pool 内部相对文件名。
     * @param model3DBaseDir 兼容通用接口的参数；模型始终写入 Pool 的 `3d_models/`。
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
     * @brief 导出单个符号到 Horizon Pool。
     * @param symbol 待导出的符号。
     * @param filePath Horizon Pool 根目录。
     * @return 导出成功返回 true，否则返回 false。
     */
    bool exportSymbol(const IR::SymbolComponentIR& symbol, const QString& filePath) override;
    /**
     * @brief 将多个符号、Unit 和 Entity 导出到 Horizon Pool。
     * @param symbols 待导出的符号 IR 列表。
     * @param libName Pool 名称。
     * @param filePath Horizon Pool 根目录。
     * @param appendMode 是否追加到已有 Pool。
     * @param updateMode 是否更新已有 Pool。
     * @param libraryDescription 库描述。
     * @return 导出成功返回 true，否则返回 false。
     */
    bool exportSymbolLibrary(const QList<IR::SymbolComponentIR>& symbols,
                             const QString& libName,
                             const QString& filePath,
                             bool appendMode = true,
                             bool updateMode = false,
                             const QString& libraryDescription = QString()) override;

    /**
     * @brief 导出包含符号、封装、Part 和 pad_map 的完整组件库。
     * @param components 完整组件 IR 列表。
     * @param libName Pool 名称。
     * @param filePath Horizon Pool 根目录。
     * @param exportModel3D 是否嵌入组件和封装中的三维模型。
     * @param model3DBaseDir 兼容通用接口的参数；Horizon 始终使用 Pool 内部 `3d_models/`，不读取此路径。
     */
    bool exportComponentLibrary(const QList<IR::ComponentIR>& components,
                                const QString& libName,
                                const QString& filePath,
                                bool exportModel3D = false,
                                const QString& model3DBaseDir = QString()) override;

    /**
     * @brief 返回最近一次导出的诊断信息。
     * @return 最近一次导出的诊断信息列表。
     */
    QStringList diagnostics() const override;

private:
    QStringList m_diagnostics;  ///< 最近一次导出的诊断信息列表。
};

}  // namespace EasyKiConverter
