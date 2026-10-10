#pragma once

#include "core/ir/SymbolIR.h"
#include "models/AltiumSchComponent.h"

namespace EasyKiConverter {

/**
 * @brief 将符号基础图元转换为 Altium SchLib 记录。
 * @details 转换器不保存状态，仅负责基础几何、样式、颜色和部件归属映射。
 */
class AltiumSymbolPrimitiveConverter final {
public:
    /**
     * @brief 转换矩形。
     * @param rect 待转换矩形。
     * @return Altium 矩形记录。
     */
    static AltiumSchRectangle convertRectangle(const IR::SymbolRectangleIR& rect);

    /**
     * @brief 转换圆角矩形。
     * @param rect 待转换圆角矩形。
     * @return Altium 圆角矩形记录。
     */
    static AltiumSchRoundRectangle convertRoundRectangle(const IR::SymbolRectangleIR& rect);

    /**
     * @brief 转换圆形。
     * @param circle 待转换圆形。
     * @return Altium 椭圆记录。
     */
    static AltiumSchEllipse convertCircle(const IR::SymbolCircleIR& circle);

    /**
     * @brief 转换 IEEE 图元。
     * @param ieee 待转换 IEEE 图元。
     * @return Altium IEEE 记录。
     */
    static AltiumSchIeee convertIeee(const IR::SymbolIeeeIR& ieee);
};

}  // namespace EasyKiConverter
