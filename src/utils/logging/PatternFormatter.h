#ifndef PATTERNFORMATTER_H
#define PATTERNFORMATTER_H

#include "IFormatter.h"

#include <QRegularExpression>
#include <QString>

namespace EasyKiConverter {

/**
 * @brief 模式格式化器
 * 
 * 支持占位符的日志格式化器：
 * - %{timestamp}  - 时间戳 (2024-01-15 10:30:45.123)
 * - %{time}       - 时间 (10:30:45.123)
 * - %{date}       - 日期 (2024-01-15)
 * - %{level}      - 日志级别 (DEBUG/WARN/ERROR)
 * - %{module}     - 模块名 (Network/Export)
 * - %{thread}     - 线程 ID
 * - %{threadname} - 线程名称
 * - %{file}       - 文件名（含路径）
 * - %{shortfile}  - 简化文件名（不含路径）
 * - %{line}       - 行号
 * - %{function}   - 函数名
 * - %{message}    - 日志消息
 * - %{newline}    - 换行符
 * - %%            - 百分号
 * 
 * 默认模式: "[%{timestamp}] [%{level}] [%{module}] [%{thread}] %{message} (%{shortfile}:%{line})"
 */
class PatternFormatter : public IFormatter {
public:
    /**
     * @brief 构造函数
     * @param pattern 格式化模式
     */
    explicit PatternFormatter(const QString& pattern = defaultPattern());

    /**
     * @brief 格式化日志记录
     * @param record 日志记录。
     */
    QString format(const LogRecord& record) override;

    /**
     * @brief 克隆格式化器
     */
    QSharedPointer<IFormatter> clone() const override;

    /**
     * @brief 设置格式化模式
     * @param pattern 格式化模式。
     */
    void setPattern(const QString& pattern);

    /**
     * @brief 获取当前格式化模式
     */
    QString pattern() const {
        return m_pattern;
    }

    /**
     * @brief 获取默认模式
     */
    static QString defaultPattern() {
        return QStringLiteral("[%{timestamp}] [%{level}] [%{module}] [%{thread}] %{message} (%{shortfile}:%{line})");
    }

    /**
     * @brief 获取简单模式（不含文件位置）
     */
    static QString simplePattern() {
        return QStringLiteral("[%{timestamp}] [%{level}] [%{module}] %{message}");
    }

    /**
     * @brief 获取详细模式（包含函数名）
     */
    static QString verbosePattern() {
        return QStringLiteral(
            "[%{timestamp}] [%{level}] [%{module}] [%{thread}] %{function}(): %{message} (%{shortfile}:%{line})");
    }

private:
    QString m_pattern;  ///< 当前日志格式模式。

    // 预编译的占位符正则
    static const QRegularExpression s_placeholderRegex;

    /**
     * @brief 替换单个占位符
     * @param placeholder 占位符名称。
     * @param record 日志记录。
     */
    QString replacePlaceholder(const QString& placeholder, const LogRecord& record) const;
};

}  // namespace EasyKiConverter

#endif  // PATTERNFORMATTER_H
