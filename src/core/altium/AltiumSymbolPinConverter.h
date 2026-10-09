#ifndef ALTIUMSYMBOLPINCONVERTER_H
#define ALTIUMSYMBOLPINCONVERTER_H

#include "core/ir/SymbolIR.h"
#include "models/AltiumSchComponent.h"

namespace EasyKiConverter {

/**
 * @brief 将 IR 符号引脚转换为 Altium SchLib 引脚记录。
 *
 * 该类集中处理电气类型、显示标志和 IEEE 装饰映射，保持主符号转换器只负责记录编排。
 */
class AltiumSymbolPinConverter final {
public:
    /**
     * @brief 转换一个 IR 引脚。
     * @param pin 待转换的符号引脚。
     * @return Altium 引脚记录。
     */
    static AltiumSchPin convert(const IR::SymbolPinIR& pin);
};

}  // namespace EasyKiConverter

#endif  // ALTIUMSYMBOLPINCONVERTER_H
