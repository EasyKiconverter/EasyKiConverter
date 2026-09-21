#pragma once

#include "core/ir/SymbolIR.h"

#include <QByteArray>
#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>

namespace EasyKiConverter {

/**
 * @brief 符号导出器通用接口
 * @details 定义将 IR 符号数据导出为目标 EDA 符号库格式的通用接口。
 *          KiCad、Altium 等不同目标格式各自实现此接口。
 */
class ISymbolExporter {
public:
    /**
     * @brief 符号库导出的伴随文件内容。
     * @details 目标格式需要多个相互关联的库文件时，主库文件之外的文件通过此结构参与同一事务提交。
     */
    using CompanionFiles = QMap<QString, QByteArray>;

    virtual ~ISymbolExporter() = default;

    /**
     * @brief 获取符号库文件扩展名
     * @return 文件扩展名（如 ".kicad_sym"、".SchLib"），包含点号
     */
    virtual QString libraryFileExtension() const = 0;

    /**
     * @brief 获取最近一次导出的非致命诊断
     * @return 目标格式写入器产生的诊断列表
     */
    virtual QStringList diagnostics() const {
        return {};
    }

    /**
     * @brief 获取最近一次导出的伴随文件。
     * @return 文件名到完整文件内容的映射，文件名必须是相对库目录的安全文件名。
     */
    virtual CompanionFiles companionFiles() const {
        return {};
    }

    /**
     * @brief 导出单个符号到文件
     * @param symbol 符号 IR 数据
     * @param filePath 输出文件路径
     * @return 是否成功
     */
    virtual bool exportSymbol(const IR::SymbolComponentIR& symbol, const QString& filePath) = 0;

    /**
     * @brief 导出符号库（多个符号合并到一个库文件）
     * @param symbols 符号 IR 列表
     * @param libName 库名称
     * @param filePath 输出文件路径
     * @param appendMode 是否追加模式
     * @param updateMode 是否更新模式（替换已存在的符号）
     * @param libraryDescription 库描述文本
     * @return 是否成功
     */
    virtual bool exportSymbolLibrary(const QList<IR::SymbolComponentIR>& symbols,
                                     const QString& libName,
                                     const QString& filePath,
                                     bool appendMode = true,
                                     bool updateMode = false,
                                     const QString& libraryDescription = QString()) = 0;
};

}  // namespace EasyKiConverter
