#pragma once

/**
 * @file ComponentDataConverter.h
 * @brief ComponentData -> ComponentIR 转换桥接
 *
 * 聚合 SymbolData/FootprintData/Model3DData 到 IR 的转换。
 */

#include "ComponentIR.h"
#include "FootprintDataConverter.h"
#include "Model3DDataConverter.h"
#include "SymbolDataConverter.h"
#include "models/ComponentData.h"

namespace EasyKiConverter {
namespace IR {

/**
 * @brief ComponentData -> ComponentIR 转换
 * @param data 旧模型的组件数据
 * @return IR 的完整组件数据
 */
inline ComponentIR toComponentIR(const ComponentData& data) {
    ComponentIR ir;  // 待填充的完整组件 IR。

    // 通用元数据
    ir.name = data.name();
    ir.description = data.symbolData() ? data.symbolData()->info().description : QString();
    ir.prefix = data.prefix();
    ir.package = data.package();
    ir.manufacturer = data.manufacturer();
    ir.manufacturerPart = data.manufacturerPart();
    ir.datasheet = data.datasheet();

    // 转换符号
    if (data.symbolData()) {
        ir.symbol = toSymbolIR(*data.symbolData());
    }

    // 转换封装
    if (data.footprintData()) {
        ir.footprint = toFootprintIR(*data.footprintData());
    }

    // 转换 3D 模型（独立的，不依赖 FootprintData 内嵌的）
    if (data.model3DData()) {
        ir.model3D = toModel3DIR(*data.model3DData());
    }

    return ir;
}

/**
 * @brief 批量转换 ComponentData 到 ComponentIR
 * @param dataMap 组件数据映射表（componentId -> ComponentData）
 * @return 组件 IR 映射表
 */
inline QMap<QString, ComponentIR> toComponentIRMap(const QMap<QString, QSharedPointer<ComponentData>>& dataMap) {
    QMap<QString, ComponentIR> irMap;  // 按组件编号索引的转换结果。
    for (auto it = dataMap.constBegin(); it != dataMap.constEnd(); ++it) {
        if (it.value()) {
            irMap.insert(it.key(), toComponentIR(*it.value()));
        }
    }
    return irMap;
}

/**
 * @brief 批量提取符号 IR 列表
 * @param dataMap 组件数据映射表
 * @return 符号 IR 列表（用于库级导出）
 */
inline QList<SymbolComponentIR> extractSymbolIRList(const QMap<QString, QSharedPointer<ComponentData>>& dataMap) {
    QList<SymbolComponentIR> list;  // 可用于库级导出的符号 IR 列表。
    for (auto it = dataMap.constBegin(); it != dataMap.constEnd(); ++it) {
        if (it.value() && it.value()->symbolData()) {
            list.append(toSymbolIR(*it.value()->symbolData()));
        }
    }
    return list;
}

/**
 * @brief 批量提取封装 IR 列表
 * @param dataMap 组件数据映射表
 * @return 封装 IR 列表（用于库级导出）
 */
inline QList<FootprintComponentIR> extractFootprintIRList(const QMap<QString, QSharedPointer<ComponentData>>& dataMap) {
    QList<FootprintComponentIR> list;  // 可用于库级导出的封装 IR 列表。
    for (auto it = dataMap.constBegin(); it != dataMap.constEnd(); ++it) {
        if (it.value() && it.value()->footprintData()) {
            list.append(toFootprintIR(*it.value()->footprintData()));
        }
    }
    return list;
}

}  // namespace IR
}  // namespace EasyKiConverter
