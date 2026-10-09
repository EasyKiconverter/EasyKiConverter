#pragma once

/**
 * @file SymbolDataConverter.h
 * @brief SymbolData -> SymbolComponentIR 转换桥接（薄包装层）
 *
 * 此文件保留向后兼容性。实际逻辑已拆分到：
 * - GeometryNormalizer.h：坐标解析和几何归一化
 * - IrBuilder.h：IR 构建和类型映射
 *
 * 新代码应直接使用 IrBuilder.h。
 */

#include "IrBuilder.h"

// 向后兼容：将旧函数名引入 IR 命名空间
// FootprintDataConverter.h 等依赖这些符号
namespace EasyKiConverter {
namespace IR {

/**
 * @deprecated 使用 GeometryNormalizer::parseFlatPointString
 */
using GeometryNormalizer::parseFlatPointString;

/**
 * @deprecated 使用 GeometryNormalizer::parseCommaSeparatedPoints
 */
using GeometryNormalizer::parseCommaSeparatedPoints;

/**
 * @deprecated 使用 GeometryNormalizer::parseSimpleSvgPath
 */
using GeometryNormalizer::parseSimpleSvgPath;

}  // namespace IR
}  // namespace EasyKiConverter
