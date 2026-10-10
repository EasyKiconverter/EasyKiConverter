#ifndef LOGGER_H
#define LOGGER_H

#include "IAppender.h"
#include "IFormatter.h"
#include "LogLevel.h"
#include "LogModule.h"
#include "LogRecord.h"

#include <QAtomicInt>
#include <QHash>
#include <QList>
#include <QMutex>
#include <QObject>
#include <QReadWriteLock>
#include <QSharedPointer>
#include <QThreadStorage>

namespace EasyKiConverter {

/**
 * @brief 日志管理器
 * 
 * 单例模式的日志管理器，负责：
 * - 管理全局日志级别和模块级别
 * - 分发日志到各个 Appender
 * - 提供线程安全的日志接口
 * 
 * 性能优化：
 * - 使用原子变量存储全局日志级别，避免锁竞争
 * - 使用读写锁保护模块级别映射
 * - shouldLog() 快速路径无需获取锁
 */
class Logger : public QObject {
    Q_OBJECT

public:
    /**
     * @brief 获取单例实例
     */
    static Logger* instance();

    /**
     * @brief 设置全局日志级别
     * @param level 日志级别。
     */
    void setGlobalLevel(LogLevel level);

    /**
     * @brief 获取全局日志级别
     */
    LogLevel globalLevel() const {
        return static_cast<LogLevel>(m_globalLevel.loadRelaxed());
    }

    /**
     * @brief 设置特定模块的日志级别
     * @param module 日志模块。
     * @param level 日志级别。
     */
    void setModuleLevel(LogModule module, LogLevel level);

    /**
     * @brief 获取特定模块的日志级别
     * @param module 日志模块。
     */
    LogLevel moduleLevel(LogModule module) const;

    /**
     * @brief 清除模块级别设置（恢复使用全局级别）
     * @param module 日志模块。
     */
    void clearModuleLevel(LogModule module);

    /**
     * @brief 清除所有模块级别设置
     */
    void clearAllModuleLevels();

    /**
     * @brief 添加 Appender
     * @param appender 日志输出器。
     */
    void addAppender(QSharedPointer<IAppender> appender);

    /**
     * @brief 移除 Appender
     * @param appender 日志输出器。
     */
    void removeAppender(QSharedPointer<IAppender> appender);

    /**
     * @brief 清除所有 Appender
     */
    void clearAppenders();

    /**
     * @brief 获取所有 Appender
     */
    QList<QSharedPointer<IAppender>> appenders() const;

    /**
     * @brief 核心日志方法
     * @param level 日志级别
     * @param module 日志模块
     * @param message 日志消息
     * @param file 源文件名（可选）
     * @param function 函数名（可选）
     * @param line 行号（可选）
     */
    void log(LogLevel level,
             LogModule module,
             const QString& message,
             const char* file = nullptr,
             const char* function = nullptr,
             int line = 0);

    /**
     * @brief 检查是否应该记录日志（用于条件求值优化）
     * 
     * 性能优化：快速路径使用原子变量，无需获取锁
     * @param level 日志级别。
     * @param module 日志模块。
     */
    bool shouldLog(LogLevel level, LogModule module) const;

    /**
     * @brief 刷新所有 Appender
     */
    void flush();

    /**
     * @brief 关闭所有 Appender
     */
    void close();

    /**
     * @brief 设置线程名称（用于日志输出）
     * @param name 名称。
     */
    void setThreadName(const QString& name);

    /**
     * @brief 获取当前线程名称
     */
    QString threadName() const;

    /**
     * @brief 清除当前线程的名称设置
     */
    void clearThreadName();

signals:
    /**
     * @brief 日志记录信号（可用于 UI 集成）
     * @param record 日志记录。
     */
    void logRecord(const LogRecord& record);

private:
    Logger();
    ~Logger();

    // 禁止拷贝
    Logger(const Logger&) = delete;
    // 禁止赋值，保证全局日志器保持唯一实例。
    Logger& operator=(const Logger&) = delete;

    // 使用原子变量存储全局日志级别，实现无锁快速检查
    mutable QAtomicInt m_globalLevel{static_cast<int>(LogLevel::Info)};

    // 模块级别使用读写锁保护
    QHash<LogModule, LogLevel> m_moduleLevels;  ///< 各模块的日志级别映射。
    mutable QReadWriteLock m_moduleLevelsLock;  ///< 保护模块日志级别映射的读写锁。

    // Appender 列表使用互斥锁保护
    QList<QSharedPointer<IAppender>> m_appenders;  ///< 已注册的日志输出器列表。
    mutable QMutex m_appendersMutex;  ///< 保护日志输出器列表的互斥量。

    // 线程本地存储的线程名称
    static QThreadStorage<QString> s_threadNames;
};

/**
 * @brief 日志工具类
 */
class LogUtils {
public:
    /**
     * @brief 格式化字符串（支持 {} 占位符）
     * 
     * 示例：
     * format("Hello {}", "World") -> "Hello World"
     * format("Count: {}, Value: {}", 42, 3.14) -> "Count: 42, Value: 3.14"
     * @param pattern 格式化模式。
     * @param args 待处理的 args。
     */
    template <typename... Args>
    static QString format(const QString& pattern, const Args&... args) {
        QString result = pattern;  ///< 逐个替换占位符后的格式化文本。
        formatHelper(result, args...);
        return result;
    }

    /**
     * @brief 格式化字符串（单参数版本）
     * @param pattern 格式化模式。
     */
    static QString format(const QString& pattern) {
        return pattern;
    }

private:
    /**
     * @brief 递归替换格式字符串中的参数占位符。
     * @param result 待替换的格式化文本。
     * @param first 当前待转换的第一个参数。
     * @param rest 尚未处理的其余参数。
     */
    template <typename T, typename... Args>
    static void formatHelper(QString& result, const T& first, const Args&... rest) {
        replaceFirstPlaceholder(result, toString(first));
        formatHelper(result, rest...);
    }

    /**
     * @brief 处理格式化参数递归的终止分支。
     * @param result 已完成替换的格式化文本。
     */
    static void formatHelper(QString& /*result*/) {
        // 递归终止
    }

    /**
     * @brief 将单个格式化参数转换为文本。
     * @param value 待转换的格式化参数。
     * @return 参数对应的日志文本。
     */
    template <typename T>
    static QString toString(const T& value) {
        // 根据参数类型选择无损的日志文本转换方式。
        if constexpr (std::is_same_v<T, QString>) {
            return value;
        } else if constexpr (std::is_same_v<T, const char*> || std::is_same_v<T, char*>) {
            return QString::fromUtf8(value);
        } else if constexpr (std::is_arithmetic_v<T>) {
            return QString::number(value);
        } else {
            return QString(value);
        }
    }

    static void replaceFirstPlaceholder(QString& result, const QString& replacement);
};

/**
 * @brief 性能追踪辅助类
 * 
 * 用于自动记录代码块的执行时间
 */
class ScopedTracer {
public:
    ScopedTracer(LogModule module, const QString& operation);
    ~ScopedTracer();

private:
    LogModule m_module;  ///< 追踪所属的日志模块。
    QString m_operation;  ///< 追踪的操作名称。
    qint64 m_startTime;  ///< 追踪开始时的时间戳。
};

/**
 * @brief 时间日志记录器
 */
class TimeLogger {
public:
    TimeLogger(LogModule module, const QString& operation);
    ~TimeLogger();

    void checkpoint(const QString& name);
    void finish();

private:
    LogModule m_module;  ///< 时间日志所属的日志模块。
    QString m_operation;  ///< 时间日志对应的操作名称。
    qint64 m_startTime;  ///< 时间日志开始时的时间戳。
    bool m_finished;  ///< 是否已经记录结束日志。
};

}  // namespace EasyKiConverter

#endif  // LOGGER_H
