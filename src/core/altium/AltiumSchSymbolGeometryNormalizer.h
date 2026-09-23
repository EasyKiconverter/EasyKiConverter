#pragma once

#include "models/AltiumSchComponent.h"

namespace EasyKiConverter {

/**
 * @brief 归一化 Altium SchLib 符号的几何坐标。
 * @details 统一符号原点、引脚连接网格、主体边界锚点，并将普通可见文本和参数字段排布到图形下方。
 */
class AltiumSchSymbolGeometryNormalizer final {
public:
    /**
     * @brief 将符号图元坐标归一化到 Altium 可用的原点和吸附网格。
     * @param component 待修改的 SchLib 符号组件。
     */
    static void normalize(AltiumSchComponent& component);
};

}  // namespace EasyKiConverter
