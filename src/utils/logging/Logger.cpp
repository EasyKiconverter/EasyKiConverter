#include "Logger.h"

#include "PatternFormatter.h"

#include <QDateTime>
#include <QMutexLocker>
#include <QReadLocker>
#include <QThread>
#include <QWriteLocker>

namespace EasyKiConverter {

QThreadStorage<QString> Logger::s_threadNames;  ///< 线程本地日志名称存储。

/**
 * @brief 获取全局 Logger 单例。
 */
Logger* Logger::instance() {
    static Logger instance;  ///< 进程级 Logger 单例对象。
    return &instance;
}

/**
 * @brief 构造日志器并初始化默认状态。
 */
Logger::Logger() {}

/**
 * @brief 关闭日志器及其输出器。
 */
Logger::~Logger() {
    close();
}

/**
 * @brief 设置全局日志级别。
 * @param level 日志级别。
 */
void Logger::setGlobalLevel(LogLevel level) {
    m_globalLevel.storeRelaxed(static_cast<int>(level));
}

/**
 * @brief 设置指定模块的日志级别。
 * @param module 日志模块。
 * @param level 日志级别。
 */
void Logger::setModuleLevel(LogModule module, LogLevel level) {
    QWriteLocker locker(&m_moduleLevelsLock);  ///< 保护模块级别映射的写锁。
    m_moduleLevels[module] = level;
}

/**
 * @brief 获取模块级别，未设置时回退到全局级别。
 * @param module 日志模块。
 */
LogLevel Logger::moduleLevel(LogModule module) const {
    QReadLocker locker(&m_moduleLevelsLock);  ///< 保护模块级别映射的读锁。
    return m_moduleLevels.value(module, static_cast<LogLevel>(m_globalLevel.loadRelaxed()));
}

/**
 * @brief 清除指定模块的日志级别覆盖。
 * @param module 日志模块。
 */
void Logger::clearModuleLevel(LogModule module) {
    QWriteLocker locker(&m_moduleLevelsLock);  ///< 保护模块级别映射的写锁。
    m_moduleLevels.remove(module);
}

/**
 * @brief 清除全部模块日志级别覆盖。
 */
void Logger::clearAllModuleLevels() {
    QWriteLocker locker(&m_moduleLevelsLock);  ///< 保护模块级别映射的写锁。
    m_moduleLevels.clear();
}

/**
 * @brief 注册一个日志输出器。
 * @param appender 日志输出器。
 */
void Logger::addAppender(QSharedPointer<IAppender> appender) {
    QMutexLocker locker(&m_appendersMutex);  ///< 保护输出器列表的互斥锁。
    if (!m_appenders.contains(appender)) {
        m_appenders.append(appender);
    }
}

/**
 * @brief 移除一个日志输出器。
 * @param appender 日志输出器。
 */
void Logger::removeAppender(QSharedPointer<IAppender> appender) {
    QMutexLocker locker(&m_appendersMutex);  ///< 保护输出器列表的互斥锁。
    m_appenders.removeOne(appender);
}

/**
 * @brief 移除全部日志输出器。
 */
void Logger::clearAppenders() {
    QMutexLocker locker(&m_appendersMutex);  ///< 保护输出器列表的互斥锁。
    m_appenders.clear();
}

/**
 * @brief 获取当前已注册的日志输出器快照。
 */
QList<QSharedPointer<IAppender>> Logger::appenders() const {
    QMutexLocker locker(&m_appendersMutex);  ///< 保护输出器列表的互斥锁。
    return m_appenders;
}

/**
 * @brief 构造日志记录并分发给所有输出器。
 * @param level 日志级别。
 * @param module 日志模块。
 * @param message 日志消息。
 * @param file 源文件名。
 * @param function 函数名。
 * @param line 源代码行号。
 */
void Logger::log(LogLevel level,
                 LogModule module,
                 const QString& message,
                 const char* file,
                 const char* function,
                 int line) {
    // 快速检查是否应该记录（无锁快速路径）
    if (!shouldLog(level, module)) {
        return;
    }

    // 构建日志记录
    LogRecord record;  ///< 当前待分发的日志记录。
    record.level = level;
    record.module = module;
    record.message = message;
    record.timestamp = QDateTime::currentMSecsSinceEpoch();
    record.threadId = QThread::currentThreadId();

    if (file) {
        record.fileName = QString::fromUtf8(file);
    }
    if (function) {
        record.functionName = QString::fromUtf8(function);
    }
    record.line = line;

    // 获取线程名称
    if (s_threadNames.hasLocalData()) {
        record.threadName = s_threadNames.localData();
    }

    // 发射信号
    emit logRecord(record);

    // 分发到各 Appender
    QMutexLocker locker(&m_appendersMutex);  ///< 保护输出器列表的互斥锁。
    for (auto& appender : m_appenders) {
        QString formatted;  ///< 当前输出器使用的格式化文本。
        if (appender->formatter()) {
            formatted = appender->formatter()->format(record);
        } else {
            formatted = record.message;
        }
        appender->append(record, formatted);
    }
}

/**
 * @brief 判断指定日志是否应按级别输出。
 * @param level 日志级别。
 * @param module 日志模块。
 */
bool Logger::shouldLog(LogLevel level, LogModule module) const {
    // 快速路径：先检查全局级别（使用原子变量，无锁）
    int globalLvl = m_globalLevel.loadRelaxed();  ///< 当前全局日志级别数值。
    if (static_cast<int>(level) < globalLvl) {
        // 如果全局级别已经过滤掉了，直接返回
        // 但还需要检查模块级别（可能模块级别更低）

        // 检查模块级别（需要读锁）
        QReadLocker locker(&m_moduleLevelsLock);  ///< 保护模块级别映射的读锁。
        if (m_moduleLevels.isEmpty()) {
            return false;  // 没有模块级别设置，全局过滤生效
        }

        auto it = m_moduleLevels.find(module);  ///< 当前模块级别迭代器。
        if (it == m_moduleLevels.end()) {
            return false;  // 该模块没有特定设置，全局过滤生效
        }

        // 模块有特定设置，使用模块级别
        return static_cast<int>(level) >= static_cast<int>(it.value());
    }

    // 全局级别检查通过
    // 但还需要检查模块是否有更严格的级别设置
    QReadLocker locker(&m_moduleLevelsLock);  ///< 保护模块级别映射的读锁。
    auto it = m_moduleLevels.find(module);  ///< 当前模块级别迭代器。
    if (it != m_moduleLevels.end()) {
        return static_cast<int>(level) >= static_cast<int>(it.value());
    }

    return true;  // 模块没有特定设置，使用全局结果
}

/**
 * @brief 刷新全部日志输出器。
 */
void Logger::flush() {
    QMutexLocker locker(&m_appendersMutex);  ///< 保护输出器列表的互斥锁。
    for (auto& appender : m_appenders) {
        appender->flush();
    }
}

/**
 * @brief 关闭全部日志输出器。
 */
void Logger::close() {
    QMutexLocker locker(&m_appendersMutex);  ///< 保护输出器列表的互斥锁。
    for (auto& appender : m_appenders) {
        appender->close();
    }
}

/**
 * @brief 设置当前线程的日志名称。
 * @param name 名称。
 */
void Logger::setThreadName(const QString& name) {
    s_threadNames.setLocalData(name);
}

/**
 * @brief 获取当前线程的日志名称。
 */
QString Logger::threadName() const {
    if (s_threadNames.hasLocalData()) {
        return s_threadNames.localData();
    }
    return QString();
}

/**
 * @brief 清除当前线程的日志名称。
 */
void Logger::clearThreadName() {
    if (s_threadNames.hasLocalData()) {
        s_threadNames.setLocalData(QString());
    }
}

// LogUtils 实现
/**
 * @brief 替换格式字符串中的第一个占位符。
 * @param result 输出结果。
 * @param replacement 替换文本。
 */
void LogUtils::replaceFirstPlaceholder(QString& result, const QString& replacement) {
    // 安全检查：避免对包含格式化字符的输入进行处理
    // 仅替换第一个 {} 占位符，不处理其他格式化字符
    int pos = result.indexOf(QStringLiteral("{}"));  ///< 第一个占位符的位置。
    if (pos >= 0) {
        // 对替换内容进行安全处理，转义可能的格式化字符
        QString safeReplacement = replacement;  ///< 不再执行二次格式化的替换文本。
        // 不对内容进行额外的格式化处理，直接替换
        result.replace(pos, 2, safeReplacement);
    }
}

// ScopedTracer 实现
/**
 * @brief 记录追踪操作的开始日志。
 */
ScopedTracer::ScopedTracer(LogModule module, const QString& operation)
    // 初始化追踪上下文和开始时间。
    : m_module(module), m_operation(operation), m_startTime(QDateTime::currentMSecsSinceEpoch()) {
    Logger::instance()->log(
        LogLevel::Trace, m_module, LogUtils::format("[BEGIN] {}", m_operation), nullptr, nullptr, 0);
}

/**
 * @brief 记录追踪操作的结束日志及耗时。
 */
ScopedTracer::~ScopedTracer() {
    qint64 elapsed = QDateTime::currentMSecsSinceEpoch() - m_startTime;  ///< 本次追踪操作耗时（毫秒）。
    Logger::instance()->log(
        LogLevel::Trace, m_module, LogUtils::format("[END] {} ({} ms)", m_operation, elapsed), nullptr, nullptr, 0);
}

// TimeLogger 实现
/**
 * @brief 创建时间日志记录器并记录开始时间。
 */
TimeLogger::TimeLogger(LogModule module, const QString& operation)
    // 初始化时间日志上下文和完成状态。
    : m_module(module), m_operation(operation), m_startTime(QDateTime::currentMSecsSinceEpoch()), m_finished(false) {}

TimeLogger::~TimeLogger() {
    if (!m_finished) {
        finish();
    }
}

/**
 * @brief 在时间日志中记录一个检查点。
 * @param name 名称。
 */
void TimeLogger::checkpoint(const QString& name) {
    qint64 elapsed = QDateTime::currentMSecsSinceEpoch() - m_startTime;  ///< 距开始时间的耗时（毫秒）。
    Logger::instance()->log(LogLevel::Debug,
                            m_module,
                            LogUtils::format("[{}] {} - {} ms", m_operation, name, elapsed),
                            nullptr,
                            nullptr,
                            0);
}

/**
 * @brief 记录完成日志并防止重复结束。
 */
void TimeLogger::finish() {
    if (!m_finished) {
        qint64 elapsed = QDateTime::currentMSecsSinceEpoch() - m_startTime;  ///< 距开始时间的总耗时（毫秒）。
        Logger::instance()->log(LogLevel::Info,
                                m_module,
                                LogUtils::format("[FINISH] {} ({} ms)", m_operation, elapsed),
                                nullptr,
                                nullptr,
                                0);
        m_finished = true;
    }
}

}  // namespace EasyKiConverter
