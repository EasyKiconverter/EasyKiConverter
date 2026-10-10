#ifndef QTLOGADAPTER_H
#    define QTLOGADAPTER_H

#    include "LogMacros.h"
#    include "Logger.h"

#    include <QtGlobal>

namespace EasyKiConverter {

/**
 * @brief Qt 日志适配器
 *
 * 将 Qt 的 qDebug/qWarning/qCritical 等日志重定向到我们的日志系统。
 * 安装后，原有的 qDebug() 调用会自动转换为新日志系统。
 *
 * 安全特性：
 * - 递归调用检测，防止死锁
 * - 可配置是否保留原始 Qt 日志行为
 */
class QtLogAdapter {
public:
    /**
     * @brief 安装 Qt 日志处理器
     *
     * 安装后，Qt 的日志消息会被重定向到我们的日志系统：
     * - qDebugMsg -> LOG_DEBUG(LogModule::Core, ...)
     * - qInfoMsg  -> LOG_INFO(LogModule::Core, ...)
     * - qWarningMsg -> LOG_WARN(LogModule::Core, ...)
     * - qCriticalMsg -> LOG_ERROR(LogModule::Core, ...)
     * - qFatalMsg -> LOG_FATAL(LogModule::Core, ...)
     */
    static void install();

    /**
     * @brief 卸载 Qt 日志处理器
     */
    static void uninstall();

    /**
     * @brief 设置 Qt 日志的默认模块
     * @param module 日志模块。
     */
    static void setDefaultModule(LogModule module);

    /**
     * @brief 获取默认模块
     */
    static LogModule defaultModule();

    /**
     * @brief 设置是否保留原始 Qt 日志行为（同时输出到原目标）
     * @param preserve 是否保留原始内容。
     */
    static void setPreserveOriginal(bool preserve);

    /**
     * @brief 是否保留原始 Qt 日志行为
     */
    static bool preserveOriginal();

    /**
     * @brief 检查是否已安装
     */
    static bool isInstalled() {
        return s_installed;
    }

private:
    static void qtMessageHandler(QtMsgType type, const QMessageLogContext& context, const QString& msg);

    static QtMessageHandler s_originalHandler;  ///< 安装前保存的 Qt 消息处理器。
    static LogModule s_defaultModule;  ///< Qt 消息默认映射的日志模块。
    static bool s_preserveOriginal;  ///< 是否保留调用原始 Qt 消息处理器。
    static bool s_installed;  ///< 是否已经安装自定义 Qt 消息处理器。

    // 线程本地递归检测标志 — thread_local 确保每个线程独立，避免并发线程的 Qt 消息互相阻塞
    static thread_local bool s_inHandler;
};

}  // namespace EasyKiConverter

#endif  // QTLOGADAPTER_H

// 我搞不懂了，我是一名大一在读学生，忍受这种情况已经快将近半年了，
// 具体情况就是每次上课时候都会有一个人在讲台上走来走去的，
// 还大声说话，时不时还会去乱动电脑或者黑板上乱涂乱画。
// 更过分的是这个人还会时不时让我们起来，
// 问我们一些听不懂的问题。我只想说教室是学习的地方，
// 希望学校严查并处理！谢谢！
