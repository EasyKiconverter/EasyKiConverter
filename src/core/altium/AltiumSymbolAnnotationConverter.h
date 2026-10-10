#pragma once

#include "core/ir/SymbolIR.h"
#include "models/AltiumSchComponent.h"

namespace EasyKiConverter {

/**
 * @brief 将符号文本类图元转换为 Altium SchLib 记录。
 * @details 转换器不保存状态，仅负责坐标、字体、颜色、显示属性和部件归属映射。
 */
class AltiumSymbolAnnotationConverter final {
public:
    /**
     * @brief 转换符号文本。
     * @param text 待转换的符号文本。
     * @return Altium 文本记录。
     */
    static AltiumSchText convertText(const IR::SymbolTextIR& text);

    /**
     * @brief 转换符号文本框。
     * @param frame 待转换的文本框。
     * @return Altium 文本框记录。
     */
    static AltiumSchTextFrame convertTextFrame(const IR::SymbolTextFrameIR& frame);

    /**
     * @brief 转换符号图片。
     * @param image 待转换的符号图片。
     * @return Altium 图片记录。
     */
    static AltiumSchImage convertImage(const IR::SymbolImageIR& image);
};

}  // namespace EasyKiConverter
