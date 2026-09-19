#pragma once

/**
 * @file PcadAdapter.h
 * @brief P-CAD 格式专用模型到统一 IR 的适配器。
 */

#include "core/ir/FootprintIR.h"
#include "core/parser/PcadModel.h"

namespace EasyKiConverter {

/** @brief P-CAD Pattern 和板级放置的统一转换结果。 */
struct PcadConversionResult {
    QList<IR::FootprintComponentIR> footprints;
    QList<IR::FootprintPlacementIR> placements;
    QList<Parser::PcadGraphic> boardGraphics;
};

/**
 * @brief 将 P-CAD Pad Style、Pattern 和图形转换为现有 Footprint IR。
 * @details 板级器件放置保留在转换结果中，不伪装成封装定义写入 IR。
 */
class PcadAdapter {
public:
    /**
     * @brief 转换整个 P-CAD PCB 模型并校验引用。
     * @param board P-CAD 格式专用模型。
     * @param diagnostics 可选转换诊断接收器。
     * @return 封装 IR、板级放置和未归属图形。
     */
    static PcadConversionResult toIR(const Parser::PcadBoard& board, Parser::ParseDiagnostics* diagnostics = nullptr);

    /**
     * @brief 将一个 P-CAD Pattern 转换为封装 IR。
     * @param board 用于解析 Pad Style 和层语义的 P-CAD 模型。
     * @param pattern P-CAD Pattern。
     * @param diagnostics 可选转换诊断接收器。
     * @return 统一封装 IR。
     */
    static IR::FootprintComponentIR toFootprint(const Parser::PcadBoard& board,
                                                const Parser::PcadPattern& pattern,
                                                Parser::ParseDiagnostics* diagnostics = nullptr);
};

}  // namespace EasyKiConverter
