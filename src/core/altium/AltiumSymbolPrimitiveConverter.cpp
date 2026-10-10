#include "AltiumSymbolPrimitiveConverter.h"

#include "utils/AltiumCoord.h"
#include "utils/AltiumSymbolConversionUtils.h"

#include <QtGlobal>

namespace EasyKiConverter {

namespace {

using AltiumSymbolConversionUtils::finiteNonNegative;
using AltiumSymbolConversionUtils::toAltiumColor;
using AltiumSymbolConversionUtils::toAltiumLineStyle;
using AltiumSymbolConversionUtils::toAltiumOwnerPartId;

}  // namespace

/**
 * @brief 将矩形边界和填充样式映射到 Altium 矩形记录。
 * @param rect 统一 IR 中的矩形图元。
 * @return 转换后的 Altium 矩形记录。
 */
AltiumSchRectangle AltiumSymbolPrimitiveConverter::convertRectangle(const IR::SymbolRectangleIR& rect) {
    AltiumSchRectangle altiumRect;  // 转换后的 Altium 矩形记录。
    altiumRect.locationX = AltiumCoord::mmToRaw(rect.x0);
    altiumRect.locationY = AltiumCoord::mmToRaw(rect.y0);
    altiumRect.cornerX = AltiumCoord::mmToRaw(rect.x1);
    altiumRect.cornerY = AltiumCoord::mmToRaw(rect.y1);
    altiumRect.lineWidth = AltiumCoord::lineWidthMmToIndex(rect.strokeWidth);
    altiumRect.lineStyle = toAltiumLineStyle(rect.strokeStyle);
    altiumRect.color = toAltiumColor(rect.strokeColor);
    altiumRect.areaColor = rect.isFilled ? toAltiumColor(rect.fillColor) : 0xFFFFFF;
    altiumRect.isSolid = rect.isFilled;
    altiumRect.ownerPartId = toAltiumOwnerPartId(rect.partIndex);
    return altiumRect;
}

/**
 * @brief 将圆角半径、边界和填充样式映射到 Altium 圆角矩形记录。
 * @param rect 统一 IR 中的圆角矩形图元。
 * @return 转换后的 Altium 圆角矩形记录。
 */
AltiumSchRoundRectangle AltiumSymbolPrimitiveConverter::convertRoundRectangle(const IR::SymbolRectangleIR& rect) {
    AltiumSchRoundRectangle altiumRect;  // 转换后的 Altium 圆角矩形记录。
    altiumRect.locationX = AltiumCoord::mmToRaw(rect.x0);
    altiumRect.locationY = AltiumCoord::mmToRaw(rect.y0);
    altiumRect.cornerX = AltiumCoord::mmToRaw(rect.x1);
    altiumRect.cornerY = AltiumCoord::mmToRaw(rect.y1);
    altiumRect.cornerXRadius = AltiumCoord::mmToRaw(finiteNonNegative(rect.cornerRadiusX));
    altiumRect.cornerYRadius = AltiumCoord::mmToRaw(finiteNonNegative(rect.cornerRadiusY));
    altiumRect.lineWidth = AltiumCoord::lineWidthMmToIndex(rect.strokeWidth);
    altiumRect.lineStyle = toAltiumLineStyle(rect.strokeStyle);
    altiumRect.color = toAltiumColor(rect.strokeColor);
    altiumRect.areaColor = rect.isFilled ? toAltiumColor(rect.fillColor) : 0xFFFFFF;
    altiumRect.isSolid = rect.isFilled;
    altiumRect.ownerPartId = toAltiumOwnerPartId(rect.partIndex);
    return altiumRect;
}

/**
 * @brief 将圆心、半径和填充样式映射到 Altium 椭圆记录。
 * @param circle 统一 IR 中的圆形图元。
 * @return 转换后的 Altium 椭圆记录。
 */
AltiumSchEllipse AltiumSymbolPrimitiveConverter::convertCircle(const IR::SymbolCircleIR& circle) {
    AltiumSchEllipse altiumEllipse;  // 转换后的 Altium 椭圆记录。
    altiumEllipse.centerX = AltiumCoord::mmToRaw(circle.center.x());
    altiumEllipse.centerY = AltiumCoord::mmToRaw(circle.center.y());
    const double radius = finiteNonNegative(circle.radius);  // 经过有限值和非负约束的半径。
    altiumEllipse.radiusX = AltiumCoord::mmToRaw(radius);
    altiumEllipse.radiusY = AltiumCoord::mmToRaw(radius);
    altiumEllipse.lineWidth = AltiumCoord::lineWidthMmToIndex(circle.strokeWidth);
    altiumEllipse.lineStyle = toAltiumLineStyle(circle.strokeStyle);
    altiumEllipse.color = toAltiumColor(circle.strokeColor);
    altiumEllipse.areaColor = circle.isFilled ? toAltiumColor(circle.fillColor) : 0xFFFFFF;
    altiumEllipse.isSolid = circle.isFilled;
    altiumEllipse.ownerPartId = toAltiumOwnerPartId(circle.partIndex);
    return altiumEllipse;
}

/**
 * @brief 将 IEEE 图元编号、方向和颜色映射到 Altium 记录。
 * @param ieee 统一 IR 中的 IEEE 图元。
 * @return 转换后的 Altium IEEE 记录。
 */
AltiumSchIeee AltiumSymbolPrimitiveConverter::convertIeee(const IR::SymbolIeeeIR& ieee) {
    AltiumSchIeee altiumIeee;  // 转换后的 Altium IEEE 记录。
    altiumIeee.symbol = qBound(0, ieee.symbol, 34);
    altiumIeee.locationX = AltiumCoord::mmToRaw(ieee.position.x());
    altiumIeee.locationY = AltiumCoord::mmToRaw(ieee.position.y());
    altiumIeee.scaleFactor = qMax(1, ieee.scaleFactor);
    altiumIeee.orientation = ((ieee.orientation % 4) + 4) % 4;
    altiumIeee.mirrored = ieee.mirrored;
    altiumIeee.color = toAltiumColor(ieee.color);
    altiumIeee.ownerPartId = toAltiumOwnerPartId(ieee.partIndex);
    return altiumIeee;
}

}  // namespace EasyKiConverter
