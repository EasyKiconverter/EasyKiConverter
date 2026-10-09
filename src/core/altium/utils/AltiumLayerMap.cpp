#include "AltiumLayerMap.h"

namespace EasyKiConverter {

/**
 * @brief 将 EasyEDA 层编号转换为 Altium V7 层标识。
 * @param layer EasyEDA 层编号。
 * @return Altium V7 层标识。
 */
uint32_t AltiumLayerMap::toV7LayerId(uint8_t layer) {
    if (layer == 32)
        return 0x0100FFFF;  // Bottom
    if (layer >= 1 && layer <= 31)
        return 0x01000000 + layer;  // Top/Mid
    if (layer >= 39 && layer <= 54)
        return 0x01010000 + (layer - 38);  // Internal Planes
    if (layer >= 57 && layer <= 72)
        return 0x01020000 + (layer - 56);  // Mechanical
    if (layer == 33)
        return 0x01030006;  // Top Overlay
    if (layer == 34)
        return 0x01030007;  // Bottom Overlay
    if (layer == 35)
        return 0x01030008;  // Top Paste
    if (layer == 36)
        return 0x01030009;  // Bottom Paste
    if (layer == 37)
        return 0x0103000A;  // Top Solder
    if (layer == 38)
        return 0x0103000B;  // Bottom Solder
    if (layer == 55)
        return 0x0103000C;  // Drill Guide
    if (layer == 56)
        return 0x0103000D;  // Keepout
    if (layer == 73)
        return 0x0103000E;  // Drill Drawing
    if (layer == 74)
        return 0x0103000F;  // Multi Layer
    return 0x01000000 + layer;  // 默认
}

/**
 * @brief 将 EasyEDA 层编号转换为 Altium 层名称。
 * @param layer EasyEDA 层编号。
 * @return Altium 层名称。
 */
QString AltiumLayerMap::toLayerName(uint8_t layer) {
    // 优先处理 Altium 的固定层名称，再计算内部层和机械层名称。
    switch (layer) {
        case 1:
            return "TOP";
        case 32:
            return "BOTTOM";
        case 33:
            return "TOPOVERLAY";
        case 34:
            return "BOTTOMOVERLAY";
        case 35:
            return "TOPPASTE";
        case 36:
            return "BOTTOMPASTE";
        case 37:
            return "TOPSOLDER";
        case 38:
            return "BOTTOMSOLDER";
        case 56:
            return "KEEPOUT";
        case 74:
            return "MULTILAYER";
        default:
            if (layer >= 2 && layer <= 31)
                return QString("MIDLAYER%1").arg(layer - 1);
            if (layer >= 39 && layer <= 54)
                return QString("INTERNALPLANE%1").arg(layer - 38);
            if (layer >= 57 && layer <= 72)
                return QString("MECHANICAL%1").arg(layer - 56);
            return QString("LAYER%1").arg(layer);
    }
}

/**
 * @brief 将 EasyEDA 焊盘形状转换为 Altium 焊盘形状枚举。
 * @param easyedaShape EasyEDA 焊盘形状名称。
 * @return Altium 焊盘形状枚举值。
 */
uint8_t AltiumLayerMap::toAltiumPadShape(const QString& easyedaShape) {
    QString upper = easyedaShape.toUpper();  // 统一形状名称大小写以便匹配。
    if (upper == "ELLIPSE" || upper == "CIRCLE" || upper == "ROUND")
        return 1;  // Round
    if (upper == "RECT" || upper == "RECTANGLE")
        return 2;  // Rectangular
    if (upper == "OCTAGON")
        return 3;  // Octagonal
    if (upper == "POLYGON" || upper == "ROUNDEDRECT" || upper == "RoundedRectangle")
        return 9;  // RoundedRectangle
    return 1;  // 默认 Round
}

/**
 * @brief 将 EasyEDA 引脚类型转换为 Altium 电气类型。
 * @param easyedaPinType EasyEDA 引脚类型编号。
 * @return Altium 电气类型枚举值。
 */
uint8_t AltiumLayerMap::toAltiumElectricalType(int easyedaPinType) {
    // 将 EasyEDA 类型编号映射为 Altium 的电气类型枚举。
    switch (easyedaPinType) {
        case 1:
            return 0;  // Input → Input
        case 2:
            return 2;  // Output → Output
        case 3:
            return 1;  // Bidirectional → IO
        case 4:
            return 7;  // Power → Power
        default:
            return 4;  // Unspecified → Passive
    }
}

/**
 * @brief 将 EasyEDA 引脚角度量化为 Altium 方向索引。
 * @param easyedaRotation EasyEDA 引脚旋转角度，单位为度。
 * @return Altium 方向索引。
 */
uint8_t AltiumLayerMap::toAltiumPinOrientation(int easyedaRotation) {
    // EasyEDA 角度（度）→ Altium 方向索引
    int normalized = ((easyedaRotation % 360) + 360) % 360;  // 归一化到 0 至 359 度。
    if (normalized < 45 || normalized >= 315)
        return 0;  // Right
    if (normalized >= 45 && normalized < 135)
        return 1;  // Up (90°)
    if (normalized >= 135 && normalized < 225)
        return 2;  // Left (180°)
    return 3;  // Down (270°)
}

}  // namespace EasyKiConverter
