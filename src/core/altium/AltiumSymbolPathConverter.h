#pragma once

#include "core/ir/SymbolIR.h"
#include "models/AltiumSchComponent.h"

namespace EasyKiConverter {

/**
 * @brief 协调符号路径及其分段到 Altium 图元记录的转换。
 * @details 负责路径段校验、曲线类型映射和来源索引维护，不负责符号整体归一化。
 */
class AltiumSymbolPathConverter final {
public:
    /**
     * @brief 将符号中的所有路径追加到目标组件并记录诊断。
     * @param symbol 待转换的符号 IR。
     * @param component 接收路径记录的 Altium 组件。
     * @param diagnostics 接收转换诊断的列表。
     */
    static void append(const IR::SymbolComponentIR& symbol, AltiumSchComponent& component, QStringList& diagnostics);
};

}  // namespace EasyKiConverter
