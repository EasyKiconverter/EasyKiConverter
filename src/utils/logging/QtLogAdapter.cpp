#include "QtLogAdapter.h"

#include <QByteArray>
#include <QCoreApplication>

namespace EasyKiConverter {

QtMessageHandler QtLogAdapter::s_originalHandler = nullptr;  ///< Qt 安装前保存的原始消息处理器。
LogModule QtLogAdapter::s_defaultModule = LogModule::Core;  ///< Qt 消息默认映射的日志模块。
bool QtLogAdapter::s_preserveOriginal = false;  ///< 是否继续调用原始 Qt 消息处理器。
bool QtLogAdapter::s_installed = false;  ///< 是否已经安装自定义 Qt 消息处理器。
thread_local bool QtLogAdapter::s_inHandler = false;  ///< 当前线程是否正在处理 Qt 消息。

/**
 * @brief 安装 EasyKiConverter 的 Qt 消息处理器。
 */
void QtLogAdapter::install() {
    if (s_installed) {
        return;
    }

    // 防止上次 install/uninstall 周期中异常退出留下的脏状态
    s_inHandler = false;
    s_originalHandler = qInstallMessageHandler(qtMessageHandler);
    s_installed = true;
}

/**
 * @brief 恢复安装前的 Qt 消息处理器。
 */
void QtLogAdapter::uninstall() {
    if (!s_installed) {
        return;
    }

    qInstallMessageHandler(s_originalHandler);
    s_originalHandler = nullptr;
    s_installed = false;
    s_inHandler = false;
}

/**
 * @brief 设置 Qt 消息默认映射的日志模块。
 * @param module 日志模块。
 */
void QtLogAdapter::setDefaultModule(LogModule module) {
    s_defaultModule = module;
}

/**
 * @brief 获取 Qt 消息默认映射的日志模块。
 */
LogModule QtLogAdapter::defaultModule() {
    return s_defaultModule;
}

/**
 * @brief 设置是否保留调用原始 Qt 消息处理器。
 * @param preserve 是否保留原始内容。
 */
void QtLogAdapter::setPreserveOriginal(bool preserve) {
    s_preserveOriginal = preserve;
}

/**
 * @brief 获取是否保留调用原始 Qt 消息处理器。
 */
bool QtLogAdapter::preserveOriginal() {
    return s_preserveOriginal;
}

/**
 * @brief 将 Qt 消息转换为项目日志并按需转发原始消息。
 * @param type 对象或数据类型。
 * @param context Qt 日志上下文。
 * @param msg 消息文本。
 */
void QtLogAdapter::qtMessageHandler(QtMsgType type, const QMessageLogContext& context, const QString& msg) {
    // === 递归调用检测 ===
    // 检查是否已经在处理日志（防止递归栈溢出）
    if (s_inHandler) {
        // 递归调用检测到，使用原始处理器或直接输出到 stderr
        if (s_originalHandler) {
            s_originalHandler(type, context, msg);
        } else {
            fprintf(stderr, "[RECURSIVE] %s\n", msg.toUtf8().constData());
        }
        return;
    }

    s_inHandler = true;

    struct RecursionGuardReset {
        ~RecursionGuardReset() {
            s_inHandler = false;
        }
    } guardReset;

    // 将 Qt 日志类型映射到我们的日志级别
    LogLevel level;  ///< 当前 Qt 消息对应的项目日志级别。
    // 按 Qt 消息类型选择对应的项目级别。
    switch (type) {
        case QtDebugMsg:
            level = LogLevel::Debug;
            break;
        case QtInfoMsg:
            level = LogLevel::Info;
            break;
        case QtWarningMsg:
            level = LogLevel::Warn;
            break;
        case QtCriticalMsg:
            level = LogLevel::Error;
            break;
        case QtFatalMsg:
            level = LogLevel::Fatal;
            break;
        default:
            level = LogLevel::Info;
            break;
    }

    // 安全地获取 Logger 实例并检查是否应该记录
    Logger* logger = Logger::instance();  ///< 当前进程的日志器单例。
    if (!logger) {
        return;
    }

    if (!logger->shouldLog(level, s_defaultModule)) {
        return;
    }

    // 构建日志消息（安全地处理字符串）
    QString message;  ///< 组合分类信息后的日志消息。

    // 避免格式化字符串攻击：不对用户消息进行额外格式化
    // 仅在消息中添加分类信息
    if (context.category && strlen(context.category) > 0) {
        QByteArray category = context.category;  ///< Qt 消息分类名称。
        if (category != "default") {
            message = QString("[%1] %2").arg(QString::fromUtf8(category), msg);
        } else {
            message = msg;
        }
    } else {
        message = msg;
    }

    // 记录日志（使用安全的文件名和函数名）
    const char* safeFile = context.file ? context.file : "";  ///< 安全的源文件名指针。
    const char* safeFunction = context.function ? context.function : "";  ///< 安全的函数名指针。
    int safeLine = context.line >= 0 ? context.line : 0;  ///< 非负的源代码行号。

    logger->log(level, s_defaultModule, message, safeFile, safeFunction, safeLine);

    // 如果需要保留原始行为
    if (s_preserveOriginal && s_originalHandler) {
        s_inHandler = false;
        s_originalHandler(type, context, msg);
        s_inHandler = true;
    }

    // Qt 的 qFatal 默认会终止程序，这里保持这个行为
    if (type == QtFatalMsg) {
        logger->flush();
        // 不调用 abort()，让程序正常退出
        // 递归保护会在函数退出时自动清除
    }
}

}  // namespace EasyKiConverter
