#ifndef FILEAPPENDER_H
#define FILEAPPENDER_H

#include "IAppender.h"
#include "LogLevel.h"

#include <QAtomicInt>
#include <QFile>
#include <QMutex>
#include <QQueue>
#include <QThread>
#include <QWaitCondition>

namespace EasyKiConverter {

/**
 * @brief 文件日志输出器
 * 
 * 支持以下特性：
 * - 按大小滚动（默认 10MB）
 * - 保留历史文件数量限制
 * - 异步写入（后台线程）
 * - 缓冲刷新
 * - 路径安全验证
 * - 有界队列防止内存溢出
 */
class FileAppender : public IAppender {
public:
    /**
     * @brief 构造函数
     * @param filePath 日志文件路径
     * @param maxSize 最大文件大小（字节），超过此大小会滚动
     * @param maxFiles 最大保留文件数（包含当前文件）
     * @param async 是否启用异步写入
     */
    explicit FileAppender(const QString& filePath,
                          qint64 maxSize = 10 * 1024 * 1024,  // 10MB
                          int maxFiles = 5,
                          bool async = true);

    /**
     * @brief 析构函数
     */
    ~FileAppender() override;

    /**
     * @brief 输出日志到文件
     * @param record 待写入的日志记录。
     * @param formatted 已格式化的日志文本。
     */
    void append(const LogRecord& record, const QString& formatted) override;

    /**
     * @brief 刷新缓冲区到文件
     */
    void flush() override;

    /**
     * @brief 关闭文件和后台线程
     */
    void close() override;

    /**
     * @brief 设置最大文件大小
     * @param maxSize 最大文件大小（字节）。
     */
    void setMaxSize(qint64 maxSize) {
        m_maxSize = maxSize;
    }

    /**
     * @brief 获取最大文件大小
     */
    qint64 maxSize() const {
        return m_maxSize;
    }

    /**
     * @brief 设置最大文件数
     * @param maxFiles 最大保留文件数。
     */
    void setMaxFiles(int maxFiles) {
        m_maxFiles = maxFiles;
    }

    /**
     * @brief 获取最大文件数
     */
    int maxFiles() const {
        return m_maxFiles;
    }

    /**
     * @brief 获取当前日志文件路径
     */
    QString filePath() const {
        return m_filePath;
    }

    /**
     * @brief 是否启用异步写入
     */
    bool isAsync() const {
        return m_async;
    }

    /**
     * @brief 设置异步队列最大大小
     * @param maxSize 最大消息数（0 表示无限制，默认 10000）
     */
    void setMaxQueueSize(int maxSize) {
        m_maxQueueSize = maxSize;
    }

    /**
     * @brief 获取异步队列最大大小
     */
    int maxQueueSize() const {
        return m_maxQueueSize;
    }

    /**
     * @brief 检查文件路径是否有效
     */
    bool isValidPath() const {
        return m_pathValid;
    }

    /**
     * @brief 获取最后一次错误信息
     */
    QString lastError() const {
        return m_lastError;
    }

private:
    QString m_filePath;  ///< 当前日志文件路径。
    QString m_canonicalPath;  ///< 解析后的规范化日志路径。
    QString m_baseDir;  ///< 日志文件所在基础目录。
    qint64 m_maxSize;  ///< 单个日志文件的最大字节数。
    int m_maxFiles;  ///< 保留的历史日志文件数量上限。
    bool m_async;  ///< 是否使用异步写入线程。
    int m_maxQueueSize = 10000;  ///< 异步日志队列大小上限。
    bool m_pathValid = false;  ///< 当前日志路径是否通过安全校验。
    QString m_lastError;  ///< 最近一次路径或文件操作错误。

    QFile m_file;  ///< 当前打开的日志文件。
    mutable QRecursiveMutex m_fileMutex;  ///< 支持嵌套调用的文件递归锁。

    // 异步写入相关
    QThread* m_writerThread = nullptr;  ///< 异步日志写入线程。
    QQueue<QString> m_writeQueue;  ///< 待写入的日志消息队列。
    mutable QMutex m_queueMutex;  ///< 保护日志消息队列的互斥量。
    QWaitCondition m_queueCondition;  ///< 唤醒异步写入线程的条件变量。
    QAtomicInt m_running;  ///< 异步写入线程运行状态。
    QAtomicInt m_flushRequested;  ///< 异步刷新请求标志。
    QAtomicInt m_queueOverflow;  ///< 队列溢出标志。

    /**
     * @brief 验证和规范化文件路径
     * @param filePath 待校验的日志文件路径。
     * @return 路径安全且规范化成功时返回 true。
     */
    bool validateAndNormalizePath(const QString& filePath);

    /**
     * @brief 检查路径是否在允许的目录范围内
     * @param path 待检查的日志文件路径。
     * @return 路径位于允许目录内时返回 true。
     */
    bool isPathInAllowedDir(const QString& path) const;

    /**
     * @brief 打开日志文件
     */
    bool openFile();

    /**
     * @brief 滚动日志文件
     */
    void rollOver();

    /**
     * @brief 清理旧日志文件
     */
    void cleanupOldFiles();

    /**
     * @brief 生成滚动文件名
     * @param index 历史日志滚动序号。
     * @return 生成的历史日志文件路径。
     */
    QString generateRolledFileName(int index) const;

    /**
     * @brief 后台写入线程函数
     */
    void writerThreadFunc();

    /**
     * @brief 直接写入（同步模式或关闭时使用）
     * @param formatted 已格式化的日志文本。
     */
    void writeDirect(const QString& formatted);

    /**
     * @brief 检查是否需要滚动
     */
    bool needsRollover() const;
};

}  // namespace EasyKiConverter

#endif  // FILEAPPENDER_H
