#pragma once

#include "core/ir/IRTypes.h"

#include <QString>

#include <cstdint>

namespace EasyKiConverter {

/**
 * @brief EasyEDA 层 ID → Altium 层编号映射
 *
 * EasyEDA 和 Altium 共享 Protel/Altium 层编号体系，大部分层 1:1 映射。
 *
 * 层编号对照：
 *   1 = Top Layer, 2-31 = Inner Layers, 32 = Bottom Layer
 *   33 = Top Overlay, 34 = Bottom Overlay
 *   35 = Top Paste, 36 = Bottom Paste
 *   37 = Top Solder Mask, 38 = Bottom Solder Mask
 *   39-54 = Internal Planes 1-16
 *   55 = Drill Guide, 56 = Keepout
 *   57-72 = Mechanical 1-16
 *   73 = Drill Drawing, 74 = Multi Layer
 */
namespace AltiumLayerMap {

/**
 * @brief 将 EasyEDA 层 ID 转换为 Altium 层字节。
 * @param easyedaLayerId EasyEDA 层 ID。
 * @return 限制在字节范围内的 Altium 层编号。
 */
constexpr uint8_t toAltiumLayer(int easyedaLayerId) {
    return static_cast<uint8_t>(qBound(0, easyedaLayerId, 255));
}

/**
 * @brief Altium 层字节 → V7 Layer ID
 * @details V7 Layer ID 用于 PcbLib 中的 V7_LAYER 参数。
 * @param layer Altium 层字节。
 * @return 对应的 V7 层 ID。
 */
uint32_t toV7LayerId(uint8_t layer);

/**
 * @brief Altium 层字节 → 层名称字符串
 * @details 用于 Region/ComponentBody 的 V7_LAYER 参数。
 * @param layer Altium 层字节。
 * @return 对应的层名称。
 */
QString toLayerName(uint8_t layer);

/**
 * @brief EasyEDA 焊盘形状 → Altium 焊盘形状
 * @details
 *   EasyEDA: "ELLIPSE" → Altium: Round (1)
 *   EasyEDA: "RECT" → Altium: Rectangular (2)
 *   EasyEDA: "POLYGON" → Altium: RoundedRectangle (9)
 * @param easyedaShape EasyEDA 焊盘形状名称。
 * @return Altium 焊盘形状编号。
 */
uint8_t toAltiumPadShape(const QString& easyedaShape);

/**
 * @brief EasyEDA 引脚类型 → Altium 电气类型
 * @details
 *   EasyEDA PinType: Unspecified=0, Input=1, Output=2, Bidirectional=3, Power=4
 *   Altium: Input=0, IO=1, Output=2, OpenCollector=3, Passive=4, HiZ=5, Power=7
 * @param easyedaPinType EasyEDA 引脚类型编号。
 * @return Altium 电气类型编号。
 */
uint8_t toAltiumElectricalType(int easyedaPinType);

/**
 * @brief EasyEDA 引脚方向（角度）→ Altium 引脚方向
 * @details
 *   EasyEDA: 0° → Right(0), 90° → Up(1), 180° → Left(2), 270° → Down(3)
 * @param easyedaRotation EasyEDA 引脚旋转角度。
 * @return Altium 引脚方向编号。
 */
uint8_t toAltiumPinOrientation(int easyedaRotation);

/**
 * @brief IR PadShape 枚举 → Altium 焊盘形状字节
 * @param shape IR 焊盘形状枚举
 * @return Altium 焊盘形状字节 (1=Round, 2=Rect, 9=RoundedRectangle)
 */
inline uint8_t toAltiumPadShape(IR::PadShape shape) {
    // 将 IR 焊盘形状收敛到 Altium 支持的圆形、矩形和圆角矩形编号。
    switch (shape) {
        case IR::PadShape::Ellipse:
        case IR::PadShape::Oval:
            return 1;  // Round
        case IR::PadShape::Rect:
            return 2;  // Rectangular
        case IR::PadShape::Polygon:
        case IR::PadShape::RoundRect:
        case IR::PadShape::Trapezoid:
            return 9;  // RoundedRectangle
        default:
            return 1;  // 默认 Round
    }
}

/**
 * @brief IR PinElectricalType 枚举 → Altium 电气类型字节
 * @param type IR 引脚电气类型枚举
 * @return Altium 引脚电气类型字节
 */
inline uint8_t toAltiumElectricalType(IR::PinElectricalType type) {
    // 将 IR 电气类型转换为 Altium 引脚电气类型编号。
    switch (type) {
        case IR::PinElectricalType::Input:
            return 0;  // Input
        case IR::PinElectricalType::Output:
            return 2;  // Output
        case IR::PinElectricalType::Bidirectional:
            return 1;  // IO
        case IR::PinElectricalType::Power:
            return 7;  // Power
        case IR::PinElectricalType::OpenCollector:
            return 3;  // OpenCollector
        case IR::PinElectricalType::OpenEmitter:
            return 6;  // OpenEmitter
        case IR::PinElectricalType::Passive:
            return 4;  // Passive
        default:
            return 4;  // Unspecified → Passive
    }
}

/**
 * @brief IR PinDirection 枚举 → Altium 引脚方向字节
 * @param dir IR 引脚方向枚举
 * @return Altium 引脚方向字节 (0=Right, 1=Up, 2=Left, 3=Down)
 */
inline uint8_t toAltiumPinOrientation(IR::PinDirection dir) {
    // 将 IR 引脚方向转换为 Altium 的四向方向编号。
    switch (dir) {
        case IR::PinDirection::Right:
            return 0;
        case IR::PinDirection::Up:
            return 1;
        case IR::PinDirection::Left:
            return 2;
        case IR::PinDirection::Down:
            return 3;
        default:
            return 0;
    }
}

/**
 * @brief IR LayerType 枚举 → Altium Protel 层编号
 * @param layer IR 通用层类型
 * @return Altium 兼容的层编号，无法映射时返回 0
 *
 * @note EasyEDA 和 Altium 共享 Protel 层编号体系，大部分可直接映射。
 *       此函数替代原 EasyedaLayerMap::fromLayerTypeToAltium()。
 */
inline int fromLayerTypeToAltium(IR::LayerType layer) {
    // 将统一层类型映射为 Altium 的 Protel 层编号。
    switch (layer) {
        case IR::LayerType::TopCopper:
            return 1;  // Top Layer
        case IR::LayerType::BottomCopper:
            return 32;  // Bottom Layer
        case IR::LayerType::TopSilk:
            return 33;  // Top Overlay
        case IR::LayerType::BottomSilk:
            return 34;  // Bottom Overlay
        case IR::LayerType::TopPaste:
            return 35;  // Top Paste
        case IR::LayerType::BottomPaste:
            return 36;  // Bottom Paste
        case IR::LayerType::TopMask:
            return 37;  // Top Solder Mask
        case IR::LayerType::BottomMask:
            return 38;  // Bottom Solder Mask
        case IR::LayerType::MultiLayer:
            return 74;  // Multi Layer
        case IR::LayerType::EdgeCuts:
            return 57;  // Mechanical 1 (library body outline)
        case IR::LayerType::KeepOut:
            return 56;  // Keepout
        case IR::LayerType::TopAssembly:
            return 57;  // Mechanical 1
        case IR::LayerType::BottomAssembly:
            return 58;  // Mechanical 2
        case IR::LayerType::UserDefined:
            return 57;  // Mechanical 1，承载 EasyEDA Dwgs.User/Cmts.User 等用户层图元
        default:
            return 0;
    }
}

}  // namespace AltiumLayerMap
}  // namespace EasyKiConverter
