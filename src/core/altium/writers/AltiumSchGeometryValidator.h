#pragma once

#include "models/AltiumSchComponent.h"

namespace EasyKiConverter {

class AltiumSchLibWriter;

/**
 * @brief 校验 SchLib 图元的几何参数。
 * @details 校验坐标、尺寸、方向、字符串和控制点数量，拒绝无法安全编码或读取的图元记录。
 */
class AltiumSchGeometryValidator final {
public:
    /**
     * @brief 校验组件中全部图元的几何约束。
     * @param writer 所属的 SchLib 主写入器。
     * @param component 待校验的原理图元件。
     * @return 所有图元均符合约束时返回 true。
     */
    static bool validate(AltiumSchLibWriter& writer, const AltiumSchComponent& component);
};

}  // namespace EasyKiConverter
