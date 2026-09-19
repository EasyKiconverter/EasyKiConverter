#pragma once

/**
 * @file CadstarAdapter.h
 * @brief Cadstar 格式专用模型到统一 IR 的适配器。
 */

#include "core/ir/ComponentIR.h"
#include "core/ir/FootprintIR.h"
#include "core/ir/SymbolIR.h"
#include "core/parser/CadstarModel.h"

namespace EasyKiConverter {

/** @brief Cadstar Part 关联的统一转换结果。 */
struct CadstarConversionResult {
    QList<IR::FootprintComponentIR> footprints;
    QList<IR::SymbolComponentIR> symbols;
    QList<IR::ComponentIR> components;
};

/**
 * @brief 将 Cadstar Pad、Package、Component 和 Part 转换为统一 IR。
 */
class CadstarAdapter {
public:
    /**
     * @brief 转换整个 Cadstar 库并校验器件关联。
     * @param library Cadstar 解析模型。
     * @param diagnostics 可选转换诊断接收器。
     * @return 封装、符号和有效的顶层组件结果。
     */
    static CadstarConversionResult toIR(const Parser::CadstarLibrary& library,
                                        Parser::ParseDiagnostics* diagnostics = nullptr);

    /**
     * @brief 将一个 Cadstar Package 转换为封装 IR。
     * @param library 用于解析焊盘引用和单位的 Cadstar 库。
     * @param packageModel Cadstar 封装模型。
     * @param diagnostics 可选转换诊断接收器。
     * @return 统一封装 IR。
     */
    static IR::FootprintComponentIR toFootprint(const Parser::CadstarLibrary& library,
                                                const Parser::CadstarPackage& packageModel,
                                                Parser::ParseDiagnostics* diagnostics = nullptr);

    /**
     * @brief 将一个 Cadstar Component 转换为符号 IR。
     * @param library 用于读取单位的 Cadstar 库。
     * @param component Cadstar 符号模型。
     * @param diagnostics 可选转换诊断接收器。
     * @return 统一符号 IR。
     */
    static IR::SymbolComponentIR toSymbol(const Parser::CadstarLibrary& library,
                                          const Parser::CadstarComponent& component,
                                          Parser::ParseDiagnostics* diagnostics = nullptr);
};

}  // namespace EasyKiConverter
