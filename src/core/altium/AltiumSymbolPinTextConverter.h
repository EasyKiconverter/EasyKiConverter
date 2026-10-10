#pragma once

#include "core/ir/SymbolIR.h"
#include "models/AltiumSchComponent.h"

namespace EasyKiConverter {

/**
 * @brief 将符号引脚名称和编号转换为 Altium 文本记录。
 * @details 转换器接收主导出器的诊断上下文，保留无效参数跳过和对齐锚点回退行为。
 */
class AltiumSymbolPinTextConverter final {
public:
    /**
     * @brief 转换一个引脚上声明的名称和编号文本。
     * @param pin 待转换的符号引脚。
     * @param symbolName 符号名称，用于诊断。
     * @param diagnostics 接收转换诊断的列表。
     * @return Altium 文本记录列表。
     */
    static QList<AltiumSchText> convert(const IR::SymbolPinIR& pin,
                                        const QString& symbolName,
                                        QStringList* diagnostics);
};

}  // namespace EasyKiConverter
