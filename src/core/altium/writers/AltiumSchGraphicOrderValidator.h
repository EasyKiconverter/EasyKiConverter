#pragma once

#include "models/AltiumSchComponent.h"

namespace EasyKiConverter {

/**
 * @brief 校验 SchLib 来源图元顺序是否完整且可安全使用。
 * @details 校验图元引用、引脚映射、路径分段和重复记录，避免来源顺序导致图元静默丢失。
 */
class AltiumSchGraphicOrderValidator final {
public:
    /**
     * @brief 校验组件的来源图元顺序。
     * @param component 待校验的 SchLib 组件。
     * @return 来源顺序完整且所有引用有效时返回 true。
     */
    static bool validate(const AltiumSchComponent& component);
};

}  // namespace EasyKiConverter
