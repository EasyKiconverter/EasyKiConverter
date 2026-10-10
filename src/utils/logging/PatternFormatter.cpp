#include "PatternFormatter.h"

#include <QDateTime>
#include <QRegularExpression>

namespace EasyKiConverter {

/**
 * @brief 日志占位符匹配表达式。
 */
const QRegularExpression PatternFormatter::s_placeholderRegex(QStringLiteral("%\\{([a-zA-Z]+)\\}"));

/**
 * @brief 使用指定日志格式创建模式格式化器。
 */
PatternFormatter::PatternFormatter(const QString& pattern) : m_pattern(pattern) {}

/**
 * @brief 按当前模式替换日志记录中的占位符。
 * @param record 日志记录。
 */
QString PatternFormatter::format(const LogRecord& record) {
    QString result = m_pattern;  ///< 待替换占位符的格式化结果。

    QRegularExpressionMatchIterator it = s_placeholderRegex.globalMatch(m_pattern);  ///< 占位符匹配迭代器。
    QStringList processed;  ///< 已处理的完整占位符集合。

    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();  ///< 当前占位符匹配结果。
        QString placeholder = match.captured(1).toLower();  ///< 当前占位符名称。

        // 避免重复处理相同的占位符
        QString fullMatch = match.captured(0);  ///< 当前占位符的完整文本。
        if (processed.contains(fullMatch)) {
            continue;
        }
        processed.append(fullMatch);

        QString replacement = replacePlaceholder(placeholder, record);  ///< 当前占位符的替换文本。
        result.replace(fullMatch, replacement);
    }

    // 处理 %% 转义
    result.replace(QStringLiteral("%%"), QStringLiteral("%"));

    return result;
}

/**
 * @brief 创建当前格式化器的独立副本。
 */
QSharedPointer<IFormatter> PatternFormatter::clone() const {
    return QSharedPointer<PatternFormatter>::create(m_pattern);
}

/**
 * @brief 更新日志格式模式。
 * @param pattern 格式化模式。
 */
void PatternFormatter::setPattern(const QString& pattern) {
    m_pattern = pattern;
}

/**
 * @brief 将单个日志占位符转换为对应的记录字段文本。
 * @param placeholder 占位符名称。
 * @param record 日志记录。
 */
QString PatternFormatter::replacePlaceholder(const QString& placeholder, const LogRecord& record) const {
    if (placeholder == "timestamp") {
        return record.formattedTimestamp();
    }
    if (placeholder == "time") {
        return QDateTime::fromMSecsSinceEpoch(record.timestamp).toString("HH:mm:ss.zzz");
    }
    if (placeholder == "date") {
        return QDateTime::fromMSecsSinceEpoch(record.timestamp).toString("yyyy-MM-dd");
    }
    if (placeholder == "level") {
        return logLevelToString(record.level);
    }
    if (placeholder == "module") {
        return logModuleToString(record.module);
    }
    if (placeholder == "thread") {
        return QString::number(reinterpret_cast<quintptr>(record.threadId), 16).toUpper().rightJustified(5, '0');
    }
    if (placeholder == "threadname") {
        return record.threadName.isEmpty()
                   ? QString::number(reinterpret_cast<quintptr>(record.threadId), 16).toUpper().rightJustified(5, '0')
                   : record.threadName;
    }
    if (placeholder == "file") {
        return record.fileName;
    }
    if (placeholder == "shortfile") {
        return record.shortFileName();
    }
    if (placeholder == "line") {
        return QString::number(record.line);
    }
    if (placeholder == "function") {
        return record.functionName;
    }
    if (placeholder == "message") {
        return record.message;
    }
    if (placeholder == "newline") {
        return QStringLiteral("\n");
    }

    // 未知占位符，保持原样
    return QStringLiteral("%{") + placeholder + QStringLiteral("}");
}

}  // namespace EasyKiConverter
