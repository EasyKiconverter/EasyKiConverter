#pragma once

#include "models/AltiumSchComponent.h"

namespace EasyKiConverter {

class AltiumSchLibWriter;

/**
 * @brief 校验 SchLib 图元记录的 OWNERPARTID 范围。
 * @details OWNERPARTID 约束横跨引脚、图元、文本和参数记录，独立封装后可避免主写入器同时承担格式校验与记录编码。
 */
class AltiumSchOwnershipValidator final {
public:
    /**
     * @brief 校验组件中全部可拥有记录的部件归属。
     * @param writer 所属的 SchLib 主写入器。
     * @param component 待校验的原理图元件。
     * @return 所有记录归属均有效时返回 true。
     */
    static bool validate(AltiumSchLibWriter& writer, const AltiumSchComponent& component);
};

}  // namespace EasyKiConverter
