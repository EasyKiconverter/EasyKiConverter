#pragma once

/**
 * @file ExporterPcadSymbol.h
 * @brief P-CAD ASCII 原理图符号库导出器。
 */

#include "core/interfaces/ISymbolExporter.h"

namespace EasyKiConverter {

/**
 * @brief 将统一符号 IR 写入 P-CAD ASCII 原理图库。
 * @details 符号库使用独立文件名，避免与 PCB `.lia` 封装库发生覆盖冲突。
 *          封装和三维模型仍由各自导出阶段生成，并通过器件关联字段连接。
 */
class ExporterPcadSymbol final : public ISymbolExporter {
public:
    /** @brief 返回 P-CAD 原理图库文件扩展名。 */
    QString libraryFileExtension() const override;

    /** @brief 导出单个 P-CAD 原理图符号。 */
    bool exportSymbol(const IR::SymbolComponentIR& symbol, const QString& filePath) override;

    /**
     * @brief 导出包含符号定义和器件关联的 P-CAD ASCII 原理图库。
     * @param symbols 待导出的符号列表。
     * @param libName 库名称。
     * @param filePath 输出文件路径。
     * @param appendMode 是否追加到已有库；当前实现不支持。
     * @param updateMode 是否更新已有库；当前实现不支持。
     * @param libraryDescription 库描述文本。
     * @return 成功返回 true，结构或 I/O 错误返回 false。
     */
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
