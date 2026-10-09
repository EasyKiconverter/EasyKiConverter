#pragma once

#include <QString>

#include <cmath>
#include <cstdint>
#include <limits>

namespace EasyKiConverter {

/**
 * @brief Altium 坐标转换工具
 * @details 处理 EasyKiConverter 内部坐标 → Altium 各种坐标格式的转换。
 *
 * 坐标系统对照：
 * - EasyKiConverter: 1 mil = 10,000 原始单位 (raw)
 * - Altium DXP 单位: 1 DXP = 10 mil = 100,000 raw
 * - Altium PCB 原始单位: 1 mil = 10,000 raw（与 EasyKiConverter 相同）
 * - Schematic Units: 1 mil = 10 单位 (raw / 1000)
 */
namespace AltiumCoord {

/**
 * @brief 原始单位 → DXP 整数部分（SchLib 参数记录使用）
 * @param raw Altium 原始坐标值
 * @details SchLib 二进制记录使用 10 mil DXP 网格；采用对称四舍五入，
 *          避免直接截断造成相邻引脚的量化误差。
 */
constexpr int16_t toDxpInt(int raw) {
    // 对称四舍五入，保持 0.4 mm 等非整数 mil 间距的相对比例。
    const int64_t widened = raw;  // 扩展为 64 位后的原始坐标。
    const int64_t rounded = widened >= 0 ? (widened + 50000) / 100000 : (widened - 50000) / 100000;  // 四舍五入结果。
    return static_cast<int16_t>(rounded);
}

/**
 * @brief 原始单位 → DXP 小数部分（SchLib 参数记录使用）
 * @param raw Altium 原始坐标值
 */
constexpr int32_t toDxpFrac(int raw) {
    return raw % 100000;
}

/**
 * @brief 原始单位 → Schematic Units（多边形/折线顶点使用）
 * @param raw Altium 原始坐标值
 */
constexpr int32_t toSchematicUnits(int raw) {
    return raw / 1000;
}

/**
 * @brief 原始单位 → mil 值
 * @param raw Altium 原始坐标值
 */
constexpr double toMils(int raw) {
    return raw / 10000.0;
}

/**
 * @brief 毫米 → Altium mil 字符串（如 "100mil"）
 * @param mm 毫米值
 */
inline QString mmToMilString(double mm) {
    const double mils = std::isfinite(mm) ? mm / 0.0254 : 0.0;  // 转换后的 mil 值。
    return QString("%1mil").arg(mils, 0, 'f', 6);
}

/**
 * @brief 原始单位 → mil 字符串
 * @param raw Altium 原始坐标值
 */
inline QString rawToMilString(int raw) {
    return QString("%1mil").arg(raw / 10000.0, 0, 'f', 6);
}

/**
 * @brief 线宽索引转换
 * @param rawWidth 原始线宽值
 * @details EasyEDA 使用实际线宽，Altium SchLib 使用索引 (0-3)
 *   0 = Small (~1mil), 1 = Medium (~2mil), 2 = Large (~4mil), 3 = Largest (~6mil)
 */
constexpr int lineWidthToIndex(int rawWidth) {
    double mils = rawWidth / 10000.0;  // 转换后的 mil 线宽。
    if (mils >= 5.0)
        return 3;
    if (mils >= 3.0)
        return 2;
    if (mils >= 1.5)
        return 1;
    return 0;
}

/**
 * @brief 毫米 → 线宽索引
 * @param mm 线宽（mm）
 * @return Altium SchLib 线宽索引 (0-3)
 */
inline int lineWidthMmToIndex(double mm) {
    double mils = mm / 0.0254;  // 转换后的 mil 线宽。
    if (!std::isfinite(mils))
        return 0;
    if (mils >= 5.0)
        return 3;
    if (mils >= 3.0)
        return 2;
    if (mils >= 1.5)
        return 1;
    return 0;
}

/**
 * @brief IR 坐标单位（mm）→ Altium 原始单位
 * @param value 待限制范围的原始单位浮点值
 * @details 原始单位: 1 mil = 10,000 raw
 *          换算: mm / 0.0254 * 10000
 */
inline int32_t clampToInt32(double value) {
    if (!std::isfinite(value))
        return 0;

    constexpr double maxValue = static_cast<double>(std::numeric_limits<int32_t>::max());  ///< int32 最大值。
    constexpr double minValue = static_cast<double>(std::numeric_limits<int32_t>::min());  ///< int32 最小值。
    if (value >= maxValue)
        return std::numeric_limits<int32_t>::max();
    if (value <= minValue)
        return std::numeric_limits<int32_t>::min();
    return static_cast<int32_t>(value);
}

inline int32_t mmToRaw(double mm) {
    return clampToInt32(mm / 0.0254 * 10000.0);
}

/**
 * @brief IR 坐标单位（mm）→ Altium Schematic Units
 * @param mm 毫米坐标值
 * @details Schematic Units: 1 mil = 10 units (raw / 1000)
 *          换算: mm / 0.0254 * 10
 */
inline int32_t mmToSchematicUnits(double mm) {
    return clampToInt32(mm / 0.0254 * 10.0);
}

}  // namespace AltiumCoord
}  // namespace EasyKiConverter
