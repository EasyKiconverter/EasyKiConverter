#include "AltiumSymbolAnnotationConverter.h"

#include "utils/AltiumCoord.h"
#include "utils/AltiumSymbolConversionUtils.h"

#include <QtGlobal>

namespace EasyKiConverter {

namespace {

using AltiumSymbolConversionUtils::toAltiumColor;
using AltiumSymbolConversionUtils::toAltiumLineStyle;
using AltiumSymbolConversionUtils::toAltiumOrientation;
using AltiumSymbolConversionUtils::toAltiumOwnerPartId;

}  // namespace

/**
 * @brief 将符号文本的通用属性映射到 Altium 文本记录。
 * @param text 统一 IR 中的符号文本。
 * @return 转换后的 Altium 文本记录。
 */
AltiumSchText AltiumSymbolAnnotationConverter::convertText(const IR::SymbolTextIR& text) {
    AltiumSchText altiumText;  // 转换后的 Altium 文本记录。
    altiumText.locationX = AltiumCoord::mmToRaw(text.position.x());
    altiumText.locationY = AltiumCoord::mmToRaw(text.position.y());
    altiumText.text = text.text;
    altiumText.fontId = 0;
    altiumText.fontName = text.fontFamily;
    altiumText.fontSizeMm = text.fontSizeMm;
    altiumText.bold = text.bold;
    altiumText.italic = text.italic;
    altiumText.anchor = text.anchor.trimmed().isEmpty() ? QStringLiteral("middle") : text.anchor.trimmed();
    altiumText.color = toAltiumColor(text.color);
    altiumText.isHidden = !text.visible;
    altiumText.orientation = toAltiumOrientation(text.rotation);
    altiumText.ownerPartId = toAltiumOwnerPartId(text.partIndex);
    return altiumText;
}

/**
 * @brief 将符号文本框的边界、样式和文本属性映射到 Altium 记录。
 * @param frame 统一 IR 中的符号文本框。
 * @return 转换后的 Altium 文本框记录。
 */
AltiumSchTextFrame AltiumSymbolAnnotationConverter::convertTextFrame(const IR::SymbolTextFrameIR& frame) {
    AltiumSchTextFrame altiumFrame;  // 转换后的 Altium 文本框记录。
    altiumFrame.locationX = AltiumCoord::mmToRaw(frame.x0);
    altiumFrame.locationY = AltiumCoord::mmToRaw(frame.y0);
    altiumFrame.cornerX = AltiumCoord::mmToRaw(frame.x1);
    altiumFrame.cornerY = AltiumCoord::mmToRaw(frame.y1);
    altiumFrame.lineWidth = AltiumCoord::lineWidthMmToIndex(frame.strokeWidth);
    altiumFrame.lineStyle = toAltiumLineStyle(frame.strokeStyle);
    altiumFrame.color = toAltiumColor(frame.strokeColor);
    altiumFrame.areaColor = frame.isFilled ? toAltiumColor(frame.fillColor) : 0;
    altiumFrame.textColor = toAltiumColor(frame.textColor);
    altiumFrame.fontName = frame.fontFamily;
    altiumFrame.fontSizeMm = frame.fontSizeMm;
    altiumFrame.bold = frame.bold;
    altiumFrame.italic = frame.italic;
    altiumFrame.fontId = qMax(0, frame.fontId);
    altiumFrame.orientation = ((frame.orientation % 4) + 4) % 4;
    altiumFrame.alignment = qMax(0, frame.alignment);
    altiumFrame.textMargin = AltiumCoord::mmToRaw(qMax(0.0, frame.textMargin));
    altiumFrame.text = frame.text;
    altiumFrame.isSolid = frame.isFilled;
    altiumFrame.showBorder = frame.showBorder;
    altiumFrame.wordWrap = frame.wordWrap;
    altiumFrame.clipToRect = frame.clipToRect;
    altiumFrame.transparent = frame.transparent;
    altiumFrame.ownerPartId = toAltiumOwnerPartId(frame.partIndex);
    return altiumFrame;
}

/**
 * @brief 将符号图片的边界、样式和嵌入数据映射到 Altium 记录。
 * @param image 统一 IR 中的符号图片。
 * @return 转换后的 Altium 图片记录。
 */
AltiumSchImage AltiumSymbolAnnotationConverter::convertImage(const IR::SymbolImageIR& image) {
    AltiumSchImage altiumImage;  // 转换后的 Altium 图片记录。
    altiumImage.locationX = AltiumCoord::mmToRaw(image.x0);
    altiumImage.locationY = AltiumCoord::mmToRaw(image.y0);
    altiumImage.cornerX = AltiumCoord::mmToRaw(image.x1);
    altiumImage.cornerY = AltiumCoord::mmToRaw(image.y1);
    altiumImage.rotation = image.rotation;
    altiumImage.lineWidth = AltiumCoord::lineWidthMmToIndex(image.strokeWidth);
    altiumImage.lineStyle = toAltiumLineStyle(image.strokeStyle);
    altiumImage.color = toAltiumColor(image.strokeColor);
    altiumImage.areaColor = image.isFilled ? toAltiumColor(image.fillColor) : 0;
    altiumImage.fileName = image.fileName;
    altiumImage.data = image.data;
    altiumImage.isSolid = image.isFilled;
    altiumImage.transparent = image.transparent;
    altiumImage.showBorder = image.showBorder;
    altiumImage.keepAspect = image.keepAspect;
    altiumImage.embedImage = !image.data.isEmpty();
    altiumImage.ownerPartId = toAltiumOwnerPartId(image.partIndex);
    return altiumImage;
}

}  // namespace EasyKiConverter
