#pragma once

#include "core/interfaces/ISymbolExporter.h"

namespace EasyKiConverter {

/**
 * @brief Xpedition 符号库导出器
 * @details 将统一符号 IR 写入 Xpedition ASCII 符号文件并打包为 ZIP。
 */
class ExporterXpeditionSymbol : public ISymbolExporter {
public:
    /** @brief 返回符号库压缩包扩展名。 */
    QString libraryFileExtension() const override;
    /** @brief 返回符号导出阶段收集的诊断信息。 */
    QStringList diagnostics() const override;
    /** @brief 导出单个符号到 Xpedition 符号库。 */
    bool exportSymbol(const IR::SymbolComponentIR& symbol, const QString& filePath) override;
    /**
     * @brief 将多个符号及其多单元部件写入一个符号库压缩包。
     * @param symbols 待导出的符号列表。
     * @param libName 保留的库名称参数。
     * @param filePath 目标压缩包路径。
     * @param appendMode 保留接口兼容性。
     * @param updateMode 保留接口兼容性。
     * @param libraryDescription 保留的库描述参数。
     * @return 导出成功时返回 true。
     */
    bool exportSymbolLibrary(const QList<IR::SymbolComponentIR>& symbols,
                             const QString& libName,
                             const QString& filePath,
                             bool appendMode = true,
                             bool updateMode = false,
                             const QString& libraryDescription = QString()) override;

private:
    /**
     * @brief 生成符号部件对应的 ZIP 条目名称。
     * @param outputName 已经完成清理和去重的符号名称。
     * @param partIndex 符号部件索引，从零开始。
     * @return ZIP 内的相对条目名称。
     */
    QString symbolFileName(const QString& outputName, int partIndex) const;
    /**
     * @brief 生成一个符号部件的 Xpedition ASCII 内容。
     * @param symbol 原始符号 IR。
     * @param partIndex 符号部件索引，从零开始。
     * @param outputName 写入目标语法的安全符号名称。
     * @return UTF-8 编码的符号部件内容。
     */
    QByteArray symbolFile(const IR::SymbolComponentIR& symbol, int partIndex, const QString& outputName) const;
    QStringList m_diagnostics;
};

}  // namespace EasyKiConverter
