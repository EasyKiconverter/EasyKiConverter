#include "ConsoleAppender.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QQueue>
#include <QTextStream>
#include <QThread>
#include <QWaitCondition>

#include <cstdio>

#ifdef Q_OS_WIN
#    include <windows.h>
#else
#    include <unistd.h>
#endif

namespace EasyKiConverter {

// ANSI 转义序列定义
const QString ConsoleAppender::s_colorReset = QStringLiteral("\033[0m");  ///< 重置终端颜色的 ANSI 转义序列。
const QString ConsoleAppender::s_colorGray = QStringLiteral("\033[90m");  ///< Trace 日志的灰色 ANSI 转义序列。
const QString ConsoleAppender::s_colorCyan = QStringLiteral("\033[36m");  ///< Debug 日志的青色 ANSI 转义序列。
const QString ConsoleAppender::s_colorWhite = QStringLiteral("\033[37m");  ///< Info 日志的白色 ANSI 转义序列。
const QString ConsoleAppender::s_colorYellow = QStringLiteral("\033[33m");  ///< Warn 日志的黄色 ANSI 转义序列。
const QString ConsoleAppender::s_colorRed = QStringLiteral("\033[31m");  ///< Error 日志的红色 ANSI 转义序列。
const QString ConsoleAppender::s_bgRed = QStringLiteral("\033[41m");  ///< Fatal 日志的红色背景 ANSI 转义序列。

/**
 * @brief 创建控制台输出器并按需启动异步写入线程。
 */
ConsoleAppender::ConsoleAppender(bool useColors, bool async)
    // 按输出设备能力初始化颜色和异步写入状态。
    : m_useColors(useColors && supportsColors())
    // 默认异步模式（更好的性能），使用 async 参数切换为同步模式
    , m_async(async)
    , m_running(0)
    , m_flushRequested(0)
    , m_queueOverflow(0) {
    // 设置默认颜色
    m_levelColors[LogLevel::Trace] = s_colorGray;
    m_levelColors[LogLevel::Debug] = s_colorCyan;
    m_levelColors[LogLevel::Info] = s_colorWhite;
    m_levelColors[LogLevel::Warn] = s_colorYellow;
    m_levelColors[LogLevel::Error] = s_colorRed;
    m_levelColors[LogLevel::Fatal] = s_colorWhite;

    // Fatal 级别使用红色背景
    m_levelBackgrounds[LogLevel::Fatal] = s_bgRed;

    // 启动异步写入线程
    if (m_async) {
        m_running.storeRelaxed(1);
        m_writerThread = QThread::create([this]() { writerThreadFunc(); });
        m_writerThread->start();
    }
}

/**
 * @brief 关闭异步写入线程并刷新控制台输出。
 */
ConsoleAppender::~ConsoleAppender() {
    close();
}

/**
 * @brief 将格式化日志追加到同步输出或异步队列。
 * @param record 日志记录。
 * @param formatted 已格式化的日志文本。
 */
void ConsoleAppender::append(const LogRecord& record, const QString& formatted) {
    QString output = formatted;  ///< 待写入控制台的文本。

    if (m_useColors) {
        output = colorize(formatted, record.level);
    }

    output += "\n";
    QByteArray utf8 = output.toUtf8();  ///< UTF-8 编码后的日志数据。
    bool isError = (record.level >= LogLevel::Error);  ///< 是否应写入标准错误流。

    if (m_async) {
        // 异步写入：加入队列（带大小限制）
        QMutexLocker locker(&m_queueMutex);

        // 检查队列大小
        if (m_maxQueueSize > 0 && m_writeQueue.size() >= m_maxQueueSize) {
            // 队列已满，丢弃最旧的消息
            m_queueOverflow.storeRelaxed(1);
            m_writeQueue.dequeue();
        }

        // 使用特殊前缀标记错误输出
        QByteArray data =
            isError ? QByteArray("\x01") + utf8 : QByteArray("\x00") + utf8;  ///< 带输出流标记的队列数据。
        m_writeQueue.enqueue(data);
        m_queueCondition.wakeOne();
    } else {
        // 同步写入
        QMutexLocker locker(&m_mutex);
        writeDirect(utf8, isError);
    }
}

/**
 * @brief 刷新异步队列和标准输出流。
 */
void ConsoleAppender::flush() {
    if (m_async) {
        // 请求刷新
        m_flushRequested.storeRelaxed(1);
        m_queueCondition.wakeAll();

        // 等待队列清空（带超时保护）
        int timeout = 3000;  ///< 等待异步队列清空的最大时间（毫秒）。
        int waited = 0;  ///< 已等待的时间（毫秒）。
        while (waited < timeout) {
            QMutexLocker locker(&m_queueMutex);
            if (m_writeQueue.isEmpty()) {
                break;
            }
            locker.unlock();
            QThread::msleep(10);
            waited += 10;
        }
    }

    QMutexLocker locker(&m_mutex);
    fflush(stdout);
    fflush(stderr);
}

/**
 * @brief 停止异步写入线程并完成剩余输出。
 */
void ConsoleAppender::close() {
    // 停止异步线程
    if (m_async && m_writerThread) {
        m_running.storeRelaxed(0);
        m_queueCondition.wakeAll();

        // 等待线程结束（带超时保护）
        if (!m_writerThread->wait(2000)) {
            m_writerThread->terminate();
            m_writerThread->wait();
        }
        delete m_writerThread;
        m_writerThread = nullptr;
    }

    flush();
}

/**
 * @brief 设置指定日志级别的前景颜色。
 * @param level 日志级别。
 * @param colorCode 前景颜色 ANSI 代码。
 */
void ConsoleAppender::setLevelColor(LogLevel level, const QString& colorCode) {
    m_levelColors[level] = colorCode;
}

/**
 * @brief 设置指定日志级别的背景颜色。
 * @param level 日志级别。
 * @param bgColorCode 背景颜色 ANSI 代码。
 */
void ConsoleAppender::setLevelBackground(LogLevel level, const QString& bgColorCode) {
    m_levelBackgrounds[level] = bgColorCode;
}

/**
 * @brief 根据实际输出设备判断是否可以安全输出 ANSI 颜色。
 */
bool ConsoleAppender::supportsColors() {
#ifdef Q_OS_WIN
    // Windows 10+ 支持 ANSI 颜色
    static bool checked = false;  ///< 是否已经完成 Windows 控制台能力检测。
    static bool supported = false;  ///< Windows 控制台是否支持 ANSI 颜色。

    if (!checked) {
        checked = true;

        // 启用 Windows 控制台的 ANSI 转义序列处理
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hOut != INVALID_HANDLE_VALUE) {
            DWORD dwMode = 0;  ///< Windows 控制台当前输出模式。
            if (GetConsoleMode(hOut, &dwMode)) {
                dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
                if (SetConsoleMode(hOut, dwMode)) {
                    supported = true;
                }
            }
        }
    }

    return supported;
#else
    // Unix/Linux/macOS 根据实际输出设备判断 ANSI 颜色支持。
    // 只有输出连接到真实终端时才启用颜色，管道、文件和测试捕获输出保持纯文本。
    const bool stdoutIsTerminal = isatty(STDOUT_FILENO) == 1;  ///< 标准输出是否连接到终端。
    const bool stderrIsTerminal = isatty(STDERR_FILENO) == 1;  ///< 标准错误是否连接到终端。
    if (!stdoutIsTerminal && !stderrIsTerminal) {
        return false;
    }

    // TERM=dumb 明确表示终端不支持 ANSI 控制序列，不应强行覆盖用户环境。
    return qEnvironmentVariable("TERM") != QStringLiteral("dumb");
#endif
}

/**
 * @brief 为日志文本添加级别对应的颜色序列。
 * @param text 文本内容。
 * @param level 日志级别。
 */
QString ConsoleAppender::colorize(const QString& text, LogLevel level) const {
    QString result;  ///< 添加颜色控制序列后的日志文本。

    if (m_levelBackgrounds.contains(level)) {
        result += m_levelBackgrounds[level];
    }

    if (m_levelColors.contains(level)) {
        result += m_levelColors[level];
    }

    result += text;
    result += s_colorReset;

    return result;
}

/**
 * @brief 消费异步队列并写入对应输出流。
 */
void ConsoleAppender::writerThreadFunc() {
    // 设置线程的 stdout 为无缓冲模式
    setvbuf(stdout, nullptr, _IONBF, 0);
    setvbuf(stderr, nullptr, _IONBF, 0);

    while (m_running.loadRelaxed()) {
        QByteArray data;  ///< 当前待写入的队列数据。
        bool overflowed = false;  ///< 本轮处理前是否发生过队列溢出。

        {
            QMutexLocker locker(&m_queueMutex);

            // 等待新消息或刷新请求
            while (m_writeQueue.isEmpty() && m_running.loadRelaxed() && !m_flushRequested.loadRelaxed()) {
                m_queueCondition.wait(&m_queueMutex, 100);
            }

            if (!m_writeQueue.isEmpty()) {
                data = m_writeQueue.dequeue();
            }

            // 检查是否有溢出发生过
            if (m_queueOverflow.loadRelaxed()) {
                overflowed = true;
                m_queueOverflow.storeRelaxed(0);
            }
        }

        // 如果发生过溢出，添加警告消息
        if (overflowed) {
            writeDirect("[WARNING] Log queue overflow, some messages were dropped\n", false);
        }

        if (!data.isEmpty()) {
            // 检查第一个字节判断输出流
            bool isError = (data.at(0) == '\x01');  ///< 队列数据是否标记为标准错误输出。
            writeDirect(data.mid(1), isError);
        }

        // 处理刷新请求
        if (m_flushRequested.loadRelaxed()) {
            QMutexLocker locker(&m_mutex);
            fflush(stdout);
            fflush(stderr);
            m_flushRequested.storeRelaxed(0);
        }
    }

    // 写入剩余消息
    while (true) {
        QByteArray data;  ///< 线程退出前待写入的剩余队列数据。
        {
            QMutexLocker locker(&m_queueMutex);
            if (m_writeQueue.isEmpty()) {
                break;
            }
            data = m_writeQueue.dequeue();
        }

        if (!data.isEmpty()) {
            bool isError = (data.at(0) == '\x01');  ///< 剩余数据是否写入标准错误流。
            writeDirect(data.mid(1), isError);
        }
    }
}

/**
 * @brief 将日志字节写入标准输出或标准错误流。
 * @param data 数据缓冲区。
 * @param isError 是否为错误输出。
 */
void ConsoleAppender::writeDirect(const QByteArray& data, bool isError) {
    // 使用 fdopen 重新打开 stdout，确保在子线程中正确输出
    FILE* out = isError ? stderr : stdout;  ///< 实际写入的 C 标准输出流。

    // 写入数据
    size_t written = fwrite(data.constData(), 1, data.size(), out);  ///< 实际写入的字节数。
    (void)written;  // 忽略返回值

    // 立即刷新
    fflush(out);
}

}  // namespace EasyKiConverter
