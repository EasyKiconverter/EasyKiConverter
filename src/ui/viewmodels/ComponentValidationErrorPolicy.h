#ifndef COMPONENTVALIDATIONERRORPOLICY_H
#define COMPONENTVALIDATIONERRORPOLICY_H

#include <QString>

namespace EasyKiConverter {

/**
 * @brief 集中定义元件验证错误的分类规则
 * @details 该策略只解析错误文本，不修改列表项状态，也不负责显示翻译。
 */
class ComponentValidationErrorPolicy final {
public:
    /**
     * @brief 预览图错误的显示分类。
     */
    enum class PreviewErrorKind { Timeout, NotFound, Forbidden, Other };

    /**
     * @brief 判断错误是否属于 CAD 数据获取失败。
     * @param error 错误信息。
     */
    static bool isCadDataFailure(const QString& error);

    /**
     * @brief 判断错误是否表示元件不存在或明确禁止重试。
     * @param error 错误信息。
     */
    static bool isNonRetryable(const QString& error);

    /**
     * @brief 判断错误是否表示资源不存在。
     * @param error 错误信息。
     */
    static bool isNotFound(const QString& error);

    /**
     * @brief 将预览图错误归类为界面可显示的错误类型。
     * @param error 错误信息。
     */
    static PreviewErrorKind classifyPreviewError(const QString& error);
};

}  // namespace EasyKiConverter

#endif  // COMPONENTVALIDATIONERRORPOLICY_H
