#include "ComponentValidationErrorPolicy.h"

namespace EasyKiConverter {

/**
 * @brief 判断错误是否会使元件验证失败
 * @param error 错误信息。
 */
bool ComponentValidationErrorPolicy::isCadDataFailure(const QString& error) {
    return error.contains(QStringLiteral("CAD data")) || error.contains(QStringLiteral("Symbol data")) ||
           error.contains(QStringLiteral("Footprint data")) || error.contains(QStringLiteral("Empty CAD")) ||
           error.contains(QStringLiteral("parse.*EasyEDA")) ||
           error.contains(QStringLiteral("No result"), Qt::CaseInsensitive) ||
           error.contains(QStringLiteral("API returned success=false"), Qt::CaseInsensitive) ||
           error.contains(QStringLiteral("API response missing result field"), Qt::CaseInsensitive) ||
           error.contains(QStringLiteral("403")) || error.contains(QStringLiteral("404")) ||
           error.contains(QStringLiteral("timeout"), Qt::CaseInsensitive) ||
           error.contains(QStringLiteral("access denied"), Qt::CaseInsensitive) ||
           error.contains(QStringLiteral("forbidden"), Qt::CaseInsensitive) || isNotFound(error) ||
           error.contains(QStringLiteral("connection closed"), Qt::CaseInsensitive) ||
           error.contains(QStringLiteral("operation canceled"), Qt::CaseInsensitive) ||
           error.contains(QStringLiteral("Request cancelled"), Qt::CaseInsensitive) ||
           error.contains(QStringLiteral("network error"), Qt::CaseInsensitive) ||
           error.contains(QStringLiteral("fetch error"), Qt::CaseInsensitive);
}

/**
 * @brief 判断验证失败是否不应自动重试
 * @param error 错误信息。
 */
bool ComponentValidationErrorPolicy::isNonRetryable(const QString& error) {
    return error.contains(QStringLiteral("HTTP 404"), Qt::CaseInsensitive) ||
           error.contains(QStringLiteral("404 Not Found"), Qt::CaseInsensitive) ||
           error.contains(QStringLiteral("component not found"), Qt::CaseInsensitive) ||
           error.contains(QStringLiteral("元器件不存在"), Qt::CaseInsensitive) ||
           error.contains(QStringLiteral("[NO_RETRY]"), Qt::CaseInsensitive) ||
           error.contains(QStringLiteral("No result"), Qt::CaseInsensitive);
}

/**
 * @brief 判断错误是否属于资源不存在
 * @param error 错误信息。
 */
bool ComponentValidationErrorPolicy::isNotFound(const QString& error) {
    return error.contains(QStringLiteral("No result"), Qt::CaseInsensitive) ||
           error.contains(QStringLiteral("404"), Qt::CaseInsensitive) ||
           error.contains(QStringLiteral("not found"), Qt::CaseInsensitive);
}

/**
 * @brief 将预览图错误映射为稳定的显示分类
 * @param error 错误信息。
 */
ComponentValidationErrorPolicy::PreviewErrorKind ComponentValidationErrorPolicy::classifyPreviewError(
    const QString& error) {
    if (error.contains(QStringLiteral("Request timeout"), Qt::CaseInsensitive) ||
        error.contains(QStringLiteral("timeout"), Qt::CaseInsensitive)) {
        return PreviewErrorKind::Timeout;
    }
    if (isNotFound(error)) {
        return PreviewErrorKind::NotFound;
    }
    if (error.contains(QStringLiteral("403"))) {
        return PreviewErrorKind::Forbidden;
    }
    return PreviewErrorKind::Other;
}

}  // namespace EasyKiConverter
