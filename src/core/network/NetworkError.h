#ifndef NETWORKERROR_H
#define NETWORKERROR_H

#include <QDateTime>
#include <QMap>
#include <QString>

namespace EasyKiConverter {

/**
 * @brief 统一的网络错误表示
 */
class NetworkError {
public:
    enum class Severity {
        Info,  ///< 一般信息。
        Warning,  ///< 可恢复警告。
        Error,  ///< 当前操作失败。
        Critical  ///< 需要立即停止或人工处理。
    };

    enum class Category {
        Network,  ///< 网络请求或连接问题。
        Parsing,  ///< 响应解析问题。
        Validation,  ///< 数据校验问题。
        System  ///< 本地系统或文件问题。
    };

    NetworkError() = default;

    NetworkError(const QString& componentId,
                 const QString& message,
                 Severity severity,
                 Category category,
                 int statusCode = 0)
        : componentId(componentId)
        , message(message)
        , severity(severity)
        , category(category)
        , statusCode(statusCode)
        , timestamp(QDateTime::currentDateTime()) {}

    QString componentId;  ///< 相关元器件编号。
    QString message;  ///< 面向用户或日志的错误信息。
    Severity severity = Severity::Warning;  ///< 错误严重级别。
    Category category = Category::Network;  ///< 错误分类。
    int statusCode = 0;  ///< HTTP 或本地状态码。
    QDateTime timestamp;  ///< 错误产生时间。
    QMap<QString, QString> details;  ///< 附加诊断键值。

    /**
     * @brief 将网络错误格式化为可读文本。
     * @return 格式化后的错误文本。
     */
    QString toString() const {
        return QString("[%1] %2 (%3): %4 (status=%5)")
            .arg(static_cast<int>(severity))
            .arg(categoryToString(category))
            .arg(componentId)
            .arg(message)
            .arg(statusCode);
    }

    /**
     * @brief 将严重级别转换为文本。
     * @param s 严重级别。
     * @return 严重级别文本。
     */
    static QString severityToString(Severity s) {
        switch (s) {
            case Severity::Info:
                return "Info";
            case Severity::Warning:
                return "Warning";
            case Severity::Error:
                return "Error";
            case Severity::Critical:
                return "Critical";
        }
        return "Unknown";
    }

    /**
     * @brief 将错误分类转换为文本。
     * @param c 错误分类。
     * @return 分类文本。
     */
    static QString categoryToString(Category c) {
        switch (c) {
            case Category::Network:
                return "Network";
            case Category::Parsing:
                return "Parsing";
            case Category::Validation:
                return "Validation";
            case Category::System:
                return "System";
        }
        return "Unknown";
    }
};

}  // namespace EasyKiConverter

#endif  // NETWORKERROR_H
