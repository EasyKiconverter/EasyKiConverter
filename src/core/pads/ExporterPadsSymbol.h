#pragma once

#include "core/interfaces/ISymbolExporter.h"

namespace EasyKiConverter {

/**
 * @brief PADS ASCII Schematic Decal 符号导出器。
 * @details 输出 PADS Parts Library 的 Schematic Decal（.c）和 Part Type（.p）文件。
 */
class ExporterPadsSymbol final : public ISymbolExporter {
public:
    /**
     * @brief 返回 PADS 符号库文件扩展名。
     * @return PADS 符号库文件扩展名。
     */
    QString libraryFileExtension() const override;

    /**
     * @brief 导出单个 PADS Schematic Decal。
     * @param symbol 待导出的符号 IR。
     * @param filePath 输出文件路径。
     * @return 导出成功时返回 true。
     */
    bool exportSymbol(const IR::SymbolComponentIR& symbol, const QString& filePath) override;

    /**
     * @brief 将多个符号导出到一个 PADS Schematic Decal 文件。
     * @param symbols 待导出的符号列表。
     * @param libName 库名称。
     * @param filePath 输出文件路径。
     * @param appendMode 是否追加；当前格式不支持。
     * @param updateMode 是否更新；当前格式不支持。
     * @param libraryDescription 库描述。
     * @return 导出成功时返回 true。
     */
    bool exportSymbolLibrary(const QList<IR::SymbolComponentIR>& symbols,
                             const QString& libName,
                             const QString& filePath,
                             bool appendMode = true,
                             bool updateMode = false,
                             const QString& libraryDescription = QString()) override;

    /**
     * @brief 返回最近一次导出的诊断信息。
     * @return 诊断信息列表。
     */
    QStringList diagnostics() const override;

    /**
     * @brief 返回 PADS Part Type 等伴随库文件。
     * @return 伴随文件映射。
     */
    CompanionFiles companionFiles() const override;

private:
    QStringList m_diagnostics;  ///< 最近一次导出的诊断信息。
    CompanionFiles m_companionFiles;  ///< 最近一次导出的伴随文件。
};

}  // namespace EasyKiConverter
