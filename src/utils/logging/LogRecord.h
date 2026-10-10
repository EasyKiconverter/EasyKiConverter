#ifndef LOGRECORD_H
#define LOGRECORD_H

#include "LogLevel.h"
#include "LogModule.h"

#include <QDateTime>
#include <QString>
#include <QtGlobal>

namespace EasyKiConverter {

/**
 * @brief 日志记录结构
 * 
 * 包含单条日志的所有上下文信息
 */
struct LogRecord {
    LogLevel level = LogLevel::Info;  ///< 日志级别
    LogModule module = LogModule::Core;  ///< 日志模块
    QString message;  ///< 日志消息
    QString fileName;  ///< 源文件名
    QString functionName;  ///< 函数名
    int line = 0;  ///< 行号
    Qt::HANDLE threadId = nullptr;  ///< 线程 ID
    qint64 timestamp = 0;  ///< 毫秒级时间戳
    QString threadName;  ///< 可选：线程名称

    /**
     * @brief 获取格式化的时间戳字符串
     */
    QString formattedTimestamp() const {
        return QDateTime::fromMSecsSinceEpoch(timestamp).toString("yyyy-MM-dd HH:mm:ss.zzz");
    }

    /**
     * @brief 获取简化的文件名（不含路径）
     */
    QString shortFileName() const {
        int lastSlash = fileName.lastIndexOf('/');  ///< Unix 路径分隔符位置。
        int lastBackslash = fileName.lastIndexOf('\\');  ///< Windows 路径分隔符位置。
        int pos = qMax(lastSlash, lastBackslash);  ///< 最后一个路径分隔符位置。
        return pos >= 0 ? fileName.mid(pos + 1) : fileName;
    }
};

}  // namespace EasyKiConverter

#endif  // LOGRECORD_H
