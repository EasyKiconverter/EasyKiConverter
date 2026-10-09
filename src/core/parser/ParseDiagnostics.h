#pragma once

/**
 * @file ParseDiagnostics.h
 * @brief 格式解析过程中的统一诊断模型。
 */

#include <QList>
#include <QString>

namespace EasyKiConverter::Parser {

/**
 * @brief 解析诊断严重级别。
 */
enum class ParseSeverity { Info, Warning, Error, Skipped };

/**
 * @brief 诊断所关联的对象范围。
 */
enum class ParseScope { File, Component, Symbol, Footprint, Field };

/**
 * @brief 一条格式解析诊断。
 * @details 诊断同时保留文件、行列和实体信息，避免非法输入被静默归零。
 */
struct ParseDiagnostic {
    ParseSeverity severity = ParseSeverity::Info;  ///< 诊断严重级别。
    ParseScope scope = ParseScope::File;  ///< 诊断关联的对象范围。
    QString filePath;  ///< 诊断来源文件路径。
    QString entity;  ///< 诊断关联的实体标识。
    QString message;  ///< 面向用户或日志的诊断文本。
    int line = 0;  ///< 源文件行号。
    int column = 0;  ///< 源文件列号。
};

/**
 * @brief 收集解析过程中的信息、警告、错误和跳过记录。
 */
class ParseDiagnostics {
public:
    /**
     * @brief 添加一条解析诊断。
     * @param severity 诊断严重级别。
     * @param scope 诊断关联范围。
     * @param message 诊断文本。
     * @param entity 可选的实体标识。
     * @param line 可选的源文件行号。
     * @param column 可选的源文件列号。
     */
    void add(ParseSeverity severity,
             ParseScope scope,
             const QString& message,
             const QString& entity = QString(),
             int line = 0,
             int column = 0);

    /**
     * @brief 设置当前诊断关联的源文件路径。
     * @param filePath 源文件路径。
     */
    void setFilePath(const QString& filePath);

    /**
     * @brief 返回全部诊断，顺序与解析发生顺序一致。
     */
    const QList<ParseDiagnostic>& items() const;

    /**
     * @brief 追加另一阶段的诊断并保留其原始文件和位置。
     * @param other 待追加的诊断集合。
     */
    void append(const ParseDiagnostics& other);

    /**
     * @brief 判断是否存在错误诊断。
     */
    bool hasErrors() const;

    /**
     * @brief 判断是否为空。
     */
    bool isEmpty() const;

    /**
     * @brief 清空当前诊断。
     */
    void clear();

private:
    QString m_filePath;  ///< 当前诊断集合关联的源文件路径。
    QList<ParseDiagnostic> m_items;  ///< 按产生顺序保存的诊断条目。
};

/**
 * @brief 对数字字段执行严格转换，失败时写入诊断。
 */
class StrictNumberParser {
public:
    /**
     * @brief 解析有限的双精度数值。
     * @param text 原始文本。
     * @param diagnostics 诊断接收器。
     * @param field 字段名称。
     * @param line 源文件行号。
     * @return 成功时返回数值，否则返回 0 并记录错误。
     */
    static double parseDouble(const QString& text, ParseDiagnostics* diagnostics, const QString& field, int line = 0);

    /**
     * @brief 解析整数数值。
     * @param text 原始文本。
     * @param diagnostics 诊断接收器。
     * @param field 字段名称。
     * @param line 源文件行号。
     * @return 成功时返回整数，否则返回 0 并记录错误。
     */
    static qint64 parseInteger(const QString& text, ParseDiagnostics* diagnostics, const QString& field, int line = 0);
};

}  // namespace EasyKiConverter::Parser
