#pragma once

#include "core/ir/SymbolIR.h"
#include "models/AltiumSchComponent.h"

namespace EasyKiConverter {

/**
 * @brief 将已通过校验的符号参数转换为 Altium 参数记录。
 * @details 转换器不负责参数有效性诊断，仅负责字段、坐标、颜色和部件归属映射。
 */
class AltiumSymbolParameterConverter final {
public:
    /**
     * @brief 转换单个符号参数。
     * @param parameter 待转换的符号参数。
     * @return Altium 参数记录。
     */
    static AltiumSchParameter convert(const IR::SymbolParameterIR& parameter);
};

}  // namespace EasyKiConverter
