#pragma once

#include "core/interfaces/ISymbolExporter.h"

namespace EasyKiConverter {

/**
 * @brief PADS ASCII Schematic Decal 符号导出器。
 * @details 输出 PADS Parts Library 的 Schematic Decal（.c）和 Part Type（.p）文件。
 */
class ExporterPadsSymbol final : public ISymbolExporter {
public:
    /** @brief 返回 PADS 符号库文件扩展名。 */
    QString libraryFileExtension() const override;

    /** @brief 导出单个 PADS Schematic Decal。 */
    bool exportSymbol(const IR::SymbolComponentIR& symbol, const QString& filePath) override;

    /** @brief 将多个符号导出到一个 PADS Schematic Decal 文件。 */
    bool exportSymbolLibrary(const QList<IR::SymbolComponentIR>& symbols,
                             const QString& libName,
                             const QString& filePath,
                             bool appendMode = true,
                             bool updateMode = false,
                             const QString& libraryDescription = QString()) override;

    /** @brief 返回最近一次导出的诊断信息。 */
    QStringList diagnostics() const override;

    /** @brief 返回 PADS Part Type 等伴随库文件。 */
    CompanionFiles companionFiles() const override;

private:
    QStringList m_diagnostics;
    CompanionFiles m_companionFiles;
};

}  // namespace EasyKiConverter
