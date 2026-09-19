#include "ParseDiagnostics.h"

#include <QLocale>

#include <cmath>

namespace EasyKiConverter::Parser {

void ParseDiagnostics::add(ParseSeverity severity,
                           ParseScope scope,
                           const QString& message,
                           const QString& entity,
                           int line,
                           int column) {
    m_items.append({severity, scope, m_filePath, entity, message, line, column});
}

// 设置后续诊断共享的源文件位置。
void ParseDiagnostics::setFilePath(const QString& filePath) {
    m_filePath = filePath;
}

// 以解析顺序返回诊断，供读取器和测试检查。
const QList<ParseDiagnostic>& ParseDiagnostics::items() const {
    return m_items;
}

// 合并不同文件或阶段的诊断，保留每条记录原有的文件路径。
void ParseDiagnostics::append(const ParseDiagnostics& other) {
    m_items.append(other.items());
}

// 判断本次解析是否存在阻止可靠转换的错误。
bool ParseDiagnostics::hasErrors() const {
    for (const ParseDiagnostic& item : m_items) {
        if (item.severity == ParseSeverity::Error)
            return true;
    }
    return false;
}

// 判断是否没有产生任何诊断。
bool ParseDiagnostics::isEmpty() const {
    return m_items.isEmpty();
}

// 清理上一轮解析的诊断，准备复用接收器。
void ParseDiagnostics::clear() {
    m_items.clear();
}

double StrictNumberParser::parseDouble(const QString& text,
                                       ParseDiagnostics* diagnostics,
                                       const QString& field,
                                       int line) {
    bool ok = false;
    const double value = QLocale::c().toDouble(text.trimmed(), &ok);
    if (!ok || !std::isfinite(value)) {
        if (diagnostics)
            diagnostics->add(
                ParseSeverity::Error, ParseScope::Field, QStringLiteral("字段不是有限数字：%1").arg(text), field, line);
        return 0.0;
    }
    return value;
}

// 严格解析整数，避免非法字段被静默转换为零。
qint64 StrictNumberParser::parseInteger(const QString& text,
                                        ParseDiagnostics* diagnostics,
                                        const QString& field,
                                        int line) {
    bool ok = false;
    const qint64 value = text.trimmed().toLongLong(&ok);
    if (!ok) {
        if (diagnostics)
            diagnostics->add(
                ParseSeverity::Error, ParseScope::Field, QStringLiteral("字段不是合法整数：%1").arg(text), field, line);
        return 0;
    }
    return value;
}

}  // namespace EasyKiConverter::Parser
