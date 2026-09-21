#pragma once

#include "core/interfaces/ISymbolExporter.h"

namespace EasyKiConverter {

/**
 * @brief PADS ASCII Schematic Decal 符号导出器。
 * @details 输出 PADS Parts Library 的符号图形文件（.c），不伪造 Part Type 器件关联文件。
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

private:
    QStringList m_diagnostics;
};

}  // namespace EasyKiConverter
