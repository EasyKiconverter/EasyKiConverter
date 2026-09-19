#pragma once

/**
 * @file XpeditionHkpAdapter.h
 * @brief Xpedition HKP 模型到统一封装和组件 IR 的适配器。
 */

#include "core/ir/ComponentIR.h"
#include "core/parser/XpeditionHkpModel.h"
#include "core/parser/XpeditionHkpReader.h"
#include "core/parser/XpeditionSymbolModel.h"

#include <QMap>

namespace EasyKiConverter {

/** @brief Xpedition HKP 转换后的封装和顶层组件结果。 */
struct XpeditionHkpConversionResult {
    QList<IR::FootprintComponentIR> footprints;
    QList<IR::ComponentIR> components;
};

/**
 * @brief 将 Xpedition HKP 格式专用模型适配为统一 IR。
 * @details 该适配器只处理 Cell、Padstack 和 PDB 关联；符号模型由调用方按符号名提供，避免把不同文件的读取顺序写死在适配器中。
 */
class XpeditionHkpAdapter {
public:
    /**
     * @brief 将 HKP 模型转换为封装和组件 IR。
     * @param model 已解析的 HKP 格式模型。
     * @param symbols 按 Xpedition 符号名称索引的统一符号 IR。
     * @param diagnostics 可选转换诊断接收器。
     * @return 封装定义和有效的顶层组件聚合结果。
     */
    static XpeditionHkpConversionResult toIR(const Parser::XpeditionHkpModel& model,
                                             const QMap<QString, IR::SymbolComponentIR>& symbols,
                                             Parser::ParseDiagnostics* diagnostics = nullptr);

    /**
     * @brief 合并一个 HKP 文档和多个符号文档后转换为统一 IR。
     * @param document 已解析的 HKP 文档。
     * @param symbolDocuments 已解析的 Xpedition 符号文档列表。
     * @param diagnostics 可选转换诊断接收器。
     * @return 跨文件关联后的封装和顶层组件结果。
     */
    static XpeditionHkpConversionResult toIR(const Parser::XpeditionHkpDocument& document,
                                             const QList<Parser::XpeditionSymbolDocument>& symbolDocuments,
                                             Parser::ParseDiagnostics* diagnostics = nullptr);

    /**
     * @brief 将一个 Xpedition Cell 转换为封装 IR。
     * @param model 用于解析 Padstack 和 Pad 引用的 HKP 模型。
     * @param cell Cell 格式专用模型。
     * @param diagnostics 可选转换诊断接收器。
     * @return 统一封装 IR。
     */
    static IR::FootprintComponentIR toFootprint(const Parser::XpeditionHkpModel& model,
                                                const Parser::XpeditionCellDefinition& cell,
                                                Parser::ParseDiagnostics* diagnostics = nullptr);
};

}  // namespace EasyKiConverter
