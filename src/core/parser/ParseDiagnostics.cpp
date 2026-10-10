#include "ParseDiagnostics.h"

#include <QLocale>

#include <cmath>

namespace EasyKiConverter::Parser {

/**
 * @brief 添加一条解析诊断。
 * @param severity 诊断严重级别。
 * @param scope 诊断作用域。
 * @param message 诊断信息。
 * @param entity 相关实体名称。
 * @param line 相关行号。
 * @param column 相关列号。
 */
void ParseDiagnostics::add(ParseSeverity severity,
                           ParseScope scope,
                           const QString& message,
                           const QString& entity,
                           int line,
                           int column) {
    m_items.append({severity, scope, m_filePath, entity, message, line, column});
}

/**
 * @brief 设置后续诊断共享的源文件位置。
 * @param filePath 源文件路径。
 */
void ParseDiagnostics::setFilePath(const QString& filePath) {
    m_filePath = filePath;
}

/**
 * @brief 以解析顺序返回诊断列表。
 * @return 当前诊断列表。
 */
const QList<ParseDiagnostic>& ParseDiagnostics::items() const {
    return m_items;
}

/**
 * @brief 合并另一组解析诊断。
 * @param other 要追加的诊断集合。
 */
void ParseDiagnostics::append(const ParseDiagnostics& other) {
    m_items.append(other.items());
}

/**
 * @brief 判断本次解析是否存在错误。
 * @return 存在错误时返回 true。
 */
bool ParseDiagnostics::hasErrors() const {
    for (const ParseDiagnostic& item : m_items) {
        if (item.severity == ParseSeverity::Error)
            return true;
    }
    return false;
}

/**
 * @brief 判断是否没有产生任何诊断。
 * @return 没有诊断时返回 true。
 */
bool ParseDiagnostics::isEmpty() const {
    return m_items.isEmpty();
}

/**
 * @brief 清理上一轮解析的诊断。
 */
void ParseDiagnostics::clear() {
    m_items.clear();
}

/**
 * @brief 严格解析有限双精度数值。
 * @param text 待解析文本。
 * @param diagnostics 可选的诊断接收器。
 * @param field 字段名称。
 * @param line 源文件行号。
 * @return 解析出的数值，失败时返回 0。
 */
double StrictNumberParser::parseDouble(const QString& text,
                                       ParseDiagnostics* diagnostics,
                                       const QString& field,
                                       int line) {
    bool ok = false;  // Qt 数值转换是否成功。
    const double value = QLocale::c().toDouble(text.trimmed(), &ok);  // 按 C locale 解析的数值。
    if (!ok || !std::isfinite(value)) {
        if (diagnostics)
            diagnostics->add(
                ParseSeverity::Error, ParseScope::Field, QStringLiteral("字段不是有限数字：%1").arg(text), field, line);
        return 0.0;
    }
    return value;
}

/**
 * @brief 严格解析整数，避免非法字段被静默转换为零。
 * @param text 待解析文本。
 * @param diagnostics 可选的诊断接收器。
 * @param field 字段名称。
 * @param line 源文件行号。
 * @return 解析出的整数，失败时返回 0。
 */
qint64 StrictNumberParser::parseInteger(const QString& text,
                                        ParseDiagnostics* diagnostics,
                                        const QString& field,
                                        int line) {
    bool ok = false;  // Qt 整数转换是否成功。
    const qint64 value = text.trimmed().toLongLong(&ok);  // 解析出的整数值。
    if (!ok) {
        if (diagnostics)
            diagnostics->add(
                ParseSeverity::Error, ParseScope::Field, QStringLiteral("字段不是合法整数：%1").arg(text), field, line);
        return 0;
    }
    return value;
}

}  // namespace EasyKiConverter::Parser
