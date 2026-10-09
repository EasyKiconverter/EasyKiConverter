#ifndef LOGLEVEL_H
#define LOGLEVEL_H

#include <QString>

namespace EasyKiConverter {

/**
 * @brief 日志级别枚举
 * 
 * 级别从低到高：Trace < Debug < Info < Warn < Error < Fatal < Off
 */
enum class LogLevel {
    Trace = 0,  ///< 详细追踪信息（仅开发调试）
    Debug = 1,  ///< 调试信息
    Info = 2,  ///< 常规运行信息
    Warn = 3,  ///< 警告信息
    Error = 4,  ///< 错误信息
    Fatal = 5,  ///< 致命错误
    Off = 6  ///< 关闭日志
};

/**
 * @brief 将日志级别转换为字符串
 * @param level 日志级别。
 */
inline QString logLevelToString(LogLevel level) {
    switch (level) {
        case LogLevel::Trace:
            return QStringLiteral("TRACE");
        case LogLevel::Debug:
            return QStringLiteral("DEBUG");
        case LogLevel::Info:
            return QStringLiteral("INFO");
        case LogLevel::Warn:
            return QStringLiteral("WARN");
        case LogLevel::Error:
            return QStringLiteral("ERROR");
        case LogLevel::Fatal:
            return QStringLiteral("FATAL");
        case LogLevel::Off:
            return QStringLiteral("OFF");
        default:
            return QStringLiteral("UNKNOWN");
    }
}

/**
 * @brief 从字符串解析日志级别
 * @param str 待解析的字符串。
 */
inline LogLevel logLevelFromString(const QString& str) {
    QString upper = str.toUpper().trimmed();  ///< 规范化后的大写日志级别文本。
    if (upper == "TRACE")
        return LogLevel::Trace;
    if (upper == "DEBUG")
        return LogLevel::Debug;
    if (upper == "INFO")
        return LogLevel::Info;
    if (upper == "WARN" || upper == "WARNING")
        return LogLevel::Warn;
    if (upper == "ERROR")
        return LogLevel::Error;
    if (upper == "FATAL")
        return LogLevel::Fatal;
    if (upper == "OFF")
        return LogLevel::Off;
    return LogLevel::Info;  // 默认
}

}  // namespace EasyKiConverter

#endif  // LOGLEVEL_H
