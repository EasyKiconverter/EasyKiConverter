#ifndef IAPPENDER_H
#define IAPPENDER_H

#include "IFormatter.h"
#include "LogRecord.h"

#include <QSharedPointer>
#include <QString>

namespace EasyKiConverter {

/**
 * @brief 日志输出器接口
 * 
 * 负责将格式化后的日志输出到特定目标（控制台、文件等）
 */
class IAppender {
public:
    virtual ~IAppender() = default;

    /**
     * @brief 输出日志记录
     * @param record 原始日志记录
     * @param formatted 格式化后的字符串
     */
    virtual void append(const LogRecord& record, const QString& formatted) = 0;

    /**
     * @brief 刷新缓冲区
     */
    virtual void flush() {}

    /**
     * @brief 关闭输出器
     */
    virtual void close() {}

    /**
     * @brief 设置格式化器
     * @param formatter 日志格式化器。
     */
    void setFormatter(QSharedPointer<IFormatter> formatter) {
        m_formatter = formatter;
    }

    /**
     * @brief 获取格式化器
     */
    QSharedPointer<IFormatter> formatter() const {
        return m_formatter;
    }

    /**
     * @brief 格式化日志记录（使用设置的格式化器）
     * @param record 日志记录。
     */
    QString formatRecord(const LogRecord& record) const {
        if (m_formatter) {
            return m_formatter->format(record);
        }
        return record.message;
    }

protected:
    QSharedPointer<IFormatter> m_formatter;  ///< 当前日志格式化器。
};

}  // namespace EasyKiConverter

#endif  // IAPPENDER_H
