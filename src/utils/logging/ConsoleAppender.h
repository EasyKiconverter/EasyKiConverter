#ifndef CONSOLEAPPENDER_H
#define CONSOLEAPPENDER_H

#include "IAppender.h"
#include "LogLevel.h"

#include <QAtomicInt>
#include <QHash>
#include <QMutex>
#include <QQueue>
#include <QString>
#include <QThread>
#include <QWaitCondition>

namespace EasyKiConverter {

/**
 * @brief 控制台日志输出器
 * 
 * 支持彩色输出的控制台 Appender，根据日志级别自动着色：
 * - Trace: 灰色
 * - Debug: 青色
 * - Info:  白色（默认）
 * - Warn:  黄色
 * - Error: 红色
 * - Fatal: 红底白字
 * 
 * 支持异步输出模式，减少对主线程的性能影响。
 */
class ConsoleAppender : public IAppender {
public:
    /**
     * @brief 构造函数
     * @param useColors 是否启用彩色输出
     * @param async 是否启用异步输出（默认 true，更好的性能）
     */
    explicit ConsoleAppender(bool useColors = true, bool async = true);

    /**
     * @brief 析构函数
     */
    ~ConsoleAppender() override;

    /**
     * @brief 输出日志到控制台
     * @param record 日志记录。
     * @param formatted 已格式化的日志文本。
     */
    void append(const LogRecord& record, const QString& formatted) override;

    /**
     * @brief 刷新输出流
     */
    void flush() override;

    /**
     * @brief 关闭输出器
     */
    void close() override;

    /**
     * @brief 启用/禁用彩色输出
     * @param useColors 待处理的 useColors。
     */
    void setUseColors(bool useColors) {
        m_useColors = useColors;
    }

    /**
     * @brief 是否启用彩色输出
     */
    bool useColors() const {
        return m_useColors;
    }

    /**
     * @brief 设置特定级别的颜色
     * @param level 日志级别。
     * @param colorCode 前景颜色 ANSI 代码。
     */
    void setLevelColor(LogLevel level, const QString& colorCode);

    /**
     * @brief 设置特定级别的背景色（用于 Fatal）
     * @param level 日志级别。
     * @param bgColorCode 背景颜色 ANSI 代码。
     */
    void setLevelBackground(LogLevel level, const QString& bgColorCode);

    /**
     * @brief 设置异步队列最大大小
     * @param size 数据大小。
     */
    void setMaxQueueSize(int size) {
        m_maxQueueSize = size;
    }

private:
    bool m_useColors;  ///< 是否为终端输出添加 ANSI 颜色。
    bool m_async;  ///< 是否使用异步写入线程。
    QMutex m_mutex;  ///< 保护同步控制台写入的互斥量。

    // 异步写入相关
    QThread* m_writerThread = nullptr;  ///< 异步日志写入线程。
    QMutex m_queueMutex;  ///< 保护异步写入队列的互斥量。
    QQueue<QByteArray> m_writeQueue;  ///< 待写入的带流标记日志队列。
    QWaitCondition m_queueCondition;  ///< 唤醒异步写入线程的条件变量。
    QAtomicInt m_running;  ///< 异步写入线程运行状态。
    QAtomicInt m_flushRequested;  ///< 异步刷新请求标志。
    QAtomicInt m_queueOverflow;  ///< 队列溢出标志。
    int m_maxQueueSize = 1000;  // 最大队列大小

    // ANSI 颜色代码
    QHash<LogLevel, QString> m_levelColors;  ///< 日志级别到前景颜色的映射。
    QHash<LogLevel, QString> m_levelBackgrounds;  ///< 日志级别到背景颜色的映射。

    // ANSI 转义序列
    static const QString s_colorReset;  ///< 重置终端颜色的 ANSI 序列。
    static const QString s_colorGray;  ///< Trace 日志的 ANSI 前景色。
    static const QString s_colorCyan;  ///< Debug 日志的 ANSI 前景色。
    static const QString s_colorWhite;  ///< Info 日志的 ANSI 前景色。
    static const QString s_colorYellow;  ///< Warn 日志的 ANSI 前景色。
    static const QString s_colorRed;  ///< Error 日志的 ANSI 前景色。
    static const QString s_bgRed;  ///< Fatal 日志的 ANSI 背景色。

    /**
     * @brief 检测终端是否支持颜色
     */
    static bool supportsColors();

    /**
     * @brief 获取带颜色的字符串
     * @param text 文本内容。
     * @param level 日志级别。
     */
    QString colorize(const QString& text, LogLevel level) const;

    /**
     * @brief 异步写入线程函数
     */
    void writerThreadFunc();

    /**
     * @brief 直接写入控制台
     * @param data 数据缓冲区。
     * @param isError 是否为错误输出。
     */
    void writeDirect(const QByteArray& data, bool isError);
};

}  // namespace EasyKiConverter

#endif  // CONSOLEAPPENDER_H
