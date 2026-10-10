#pragma once

#include "core/ir/SymbolIR.h"
#include "models/AltiumSchComponent.h"

namespace EasyKiConverter {

/**
 * @brief 将符号的封装和模型关联转换为 Altium 实现记录。
 * @details 转换器不保存状态，仅负责候选封装、显式模型和来源元数据模型的映射。
 */
class AltiumSymbolImplementationConverter final {
public:
    /**
     * @brief 根据符号 IR 构造去重后的封装和模型实现列表。
     * @param symbol 符号 IR 数据。
     * @return 封装和三维模型实现列表。
     */
    static QList<AltiumSchComponent::Implementation> convert(const IR::SymbolComponentIR& symbol);
};

}  // namespace EasyKiConverter
