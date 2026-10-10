#ifndef ALTIUMSYMBOLCONVERSIONUTILS_H
#define ALTIUMSYMBOLCONVERSIONUTILS_H

#include "core/ir/SymbolIR.h"

#include <QColor>
#include <QtGlobal>

#include <cmath>
#include <cstdint>

namespace EasyKiConverter::AltiumSymbolConversionUtils {

/**
 * @brief 将 IR 部件索引转换为 Altium 的部件 ID。
 * @param partIndex IR 部件索引。
 * @return Altium 部件 ID。
 */
inline int toAltiumOwnerPartId(int partIndex) {
    if (partIndex == -1)
        return -1;
    if (partIndex < -1)
        return partIndex;
    if (partIndex >= 32767)
        return 32768;
    return qMax(1, partIndex + 1);
}

/**
 * @brief 将任意角度归一化为 Altium 的四向文字方向。
 * @param rotation 原始旋转角度。
 * @return Altium 四向文字方向编号。
 */
inline int toAltiumOrientation(double rotation) {
    return ((qRound(rotation / 90.0) % 4) + 4) % 4;
}

/**
 * @brief 将非法或负数几何尺寸归一化为可写入的非负有限值。
 * @param value 原始几何尺寸。
 * @return 非负有限几何尺寸。
 */
inline double finiteNonNegative(double value) {
    return std::isfinite(value) ? qMax(0.0, value) : 0.0;
}

/**
 * @brief 将 IR 线型映射为 Altium SchLib 的线型编号。
 * @param style IR 线型。
 * @return Altium SchLib 线型编号。
 */
inline int toAltiumLineStyle(IR::StrokeStyle style) {
    // 采用固定编号与 SchLib 记录格式对应，未知类型回退为实线。
    switch (style) {
        case IR::StrokeStyle::Dashed:
            return 1;
        case IR::StrokeStyle::Dotted:
            return 2;
        case IR::StrokeStyle::Solid:
        default:
            return 0;
    }
}

/**
 * @brief 将 Qt 颜色转换为 Altium 使用的 0x00BBGGRR 编码。
 * @details 黑色和未指定颜色使用 Altium 符号默认深蓝色，避免写入器省略颜色参数。
 * @param color Qt 颜色值。
 * @return Altium 颜色编码。
 */
inline uint32_t toAltiumColor(const QColor& color) {
    constexpr uint32_t kDefaultSchematicColor = 0x68380B;  ///< 原理图默认颜色。
    if (!color.isValid() || color.alpha() == 0 || color == QColor(Qt::black))
        return kDefaultSchematicColor;
    return (static_cast<uint32_t>(color.blue()) << 16) | (static_cast<uint32_t>(color.green()) << 8) |
           static_cast<uint32_t>(color.red());
}

/**
 * @brief 将原始坐标量化到 Altium SchLib 的最小转换网格。
 * @param value 原始坐标值。
 * @return 量化后的坐标值。
 */
inline int quantizeSchematicOffset(int value) {
    constexpr int kSchematicUnitRaw = 1000;  ///< 原理图坐标的基础单位比例。
    if (value >= 0)
        return ((value + kSchematicUnitRaw / 2) / kSchematicUnitRaw) * kSchematicUnitRaw;
    return ((value - kSchematicUnitRaw / 2) / kSchematicUnitRaw) * kSchematicUnitRaw;
}

}  // namespace EasyKiConverter::AltiumSymbolConversionUtils

#endif  // ALTIUMSYMBOLCONVERSIONUTILS_H
