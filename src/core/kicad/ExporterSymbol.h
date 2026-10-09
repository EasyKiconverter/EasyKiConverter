#ifndef EXPORTERSYMBOL_H
#define EXPORTERSYMBOL_H

#include "SymbolGraphicsGenerator.h"
#include "core/interfaces/ISymbolExporter.h"

#include <QJsonObject>
#include <QString>
#include <QTextStream>

namespace EasyKiConverter {

/**
 * @brief KiCad 符号导出器类
 *
 * 将 IR 符号数据导出为 KiCad 符号库格式。
 */
class ExporterSymbol : public ISymbolExporter {
public:
    ExporterSymbol();

    /**
     * @brief 析构函数
     */
    ~ExporterSymbol();

    /**
     * @brief 获取符号库文件扩展名
     * @return ".kicad_sym"
     */
    QString libraryFileExtension() const override {
        return QStringLiteral(".kicad_sym");
    }

    /**
     * @brief 导出单个 KiCad 符号。
     *
     * @param symbol 符号 IR 数据
     * @param filePath 输出文件路径
     * @return bool 是否成功
     */
    bool exportSymbol(const IR::SymbolComponentIR& symbol, const QString& filePath) override;

    /**
     * @brief 导出多个 KiCad 符号到符号库。
     *
     * @param symbols 符号 IR 列表
     * @param libName 库名称
     * @param filePath 输出文件路径
     * @param appendMode 是否使用追加模式（默认true）
     * @param updateMode 是否使用更新模式（默认 false）。如果为 true，则替换已存在的符号
     * @param libraryDescription 库描述。
     * @return bool 是否成功
     */
    bool exportSymbolLibrary(const QList<IR::SymbolComponentIR>& symbols,
                             const QString& libName,
                             const QString& filePath,
                             bool appendMode = true,
                             bool updateMode = false,
                             const QString& libraryDescription = QString()) override;

private:
    /**
     * @brief 生成 KiCad 符号库头部。
     *
     * @param libName 库名称
     * @return QString 头部文本
     */
    QString generateHeader(const QString& libName) const;

    /**
     * @brief 生成 KiCad 符号内容
     *
     * @param symbol 符号 IR 数据
     * @param libName 库名称（用于 Footprint 前缀）
     * @return QString 符号内容
     */
    QString generateSymbolContent(const IR::SymbolComponentIR& symbol, const QString& libName) const;

    /**
     * @brief 生成属于指定 part 的图形元素
     *
     * @param symbol 符号 IR 数据
     * @param partIdx 部件索引
     * @return QString 图形元素文本
     */
    QString generatePartDrawings(const IR::SymbolComponentIR& symbol, int partIdx) const;

private:
    mutable SymbolGraphicsGenerator m_graphicsGenerator;  // 图形元素生成器
    QString m_detectedVersion;  // 检测到的文件版本
};

}  // namespace EasyKiConverter

#endif  // EXPORTERSYMBOL_H
