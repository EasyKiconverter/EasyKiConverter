#pragma once

/**
 * @file XpeditionSymbolAdapter.h
 * @brief Xpedition 符号格式模型到统一 IR 的适配器。
 */

#include "core/ir/SymbolIR.h"
#include "core/parser/XpeditionSymbolModel.h"

namespace EasyKiConverter {

/**
 * @brief 将 Xpedition V54 符号模型转换为统一符号 IR。
 */
class XpeditionSymbolAdapter {
public:
    /**
     * @brief 执行格式专用模型到 IR 的转换。
     * @param document 已解析的 Xpedition 符号文档。
     * @param diagnostics 可选的转换诊断接收器。
     * @return 统一符号表示；无法表达的字段会生成诊断。
     */
    static IR::SymbolComponentIR toIR(const Parser::XpeditionSymbolDocument& document,
                                      Parser::ParseDiagnostics* diagnostics = nullptr);
};

}  // namespace EasyKiConverter
