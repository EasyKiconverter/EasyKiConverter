#pragma once

/**
 * @file ExporterOrcadSymbol.h
 * @brief OrCAD Capture XML 符号库导出器。
 */

#include "core/interfaces/ISymbolExporter.h"

namespace EasyKiConverter {

/**
 * @brief 将统一符号 IR 写入 OrCAD Capture XML 符号库。
 * @details 输出的是公开 XML 交换格式，不是 Capture 私有二进制 OLB；用户需要在
 *          Capture 中按对应版本执行 XML 导入或转换。封装名称写入 Package 属性，
 *          不在符号 XML 中伪造 PCB 封装几何或三维关联。
 */
class ExporterOrcadSymbol final : public ISymbolExporter {
public:
    /** @brief 返回 OrCAD Capture XML 符号库扩展名。 */
    QString libraryFileExtension() const override;

    /** @brief 导出一个 OrCAD Capture XML 符号文件。 */
    bool exportSymbol(const IR::SymbolComponentIR& symbol, const QString& filePath) override;

    /**
     * @brief 导出多个符号组成的 OrCAD Capture XML 符号库。
     * @param symbols 待导出的符号列表。
     * @param libName XML 库显示名称。
     * @param filePath 输出 XML 路径。
     * @param appendMode 是否追加到已有库；当前不支持。
     * @param updateMode 是否更新已有库；当前不支持。
     * @param libraryDescription 库描述，写入 XML 根定义名称。
     * @return 写入成功返回 true，否则返回 false。
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
