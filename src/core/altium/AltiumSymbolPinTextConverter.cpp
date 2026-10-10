#include "AltiumSymbolPinTextConverter.h"

#include "utils/AltiumCoord.h"
#include "utils/AltiumSymbolConversionUtils.h"

#include <cmath>

namespace EasyKiConverter {

namespace {

/**
 * @brief 向可选诊断列表追加一条引脚文本诊断。
 * @param diagnostics 可选的诊断列表。
 * @param message 待追加的诊断信息。
 */
void appendDiagnostic(QStringList* diagnostics, const QString& message) {
    if (diagnostics != nullptr)
        diagnostics->append(message);
}

/**
 * @brief 规范化引脚文本锚点，并在非法值时记录回退诊断。
 * @param anchor 输入的文本锚点。
 * @param symbolName 符号名称。
 * @param context 文本字段上下文。
 * @param diagnostics 可选的诊断列表。
 * @return Altium 支持的文本锚点。
 */
QString normalizeAnchor(const QString& anchor,
                        const QString& symbolName,
                        const QString& context,
                        QStringList* diagnostics) {
    const QString normalized = anchor.trimmed().toLower();  // 去除空白并统一大小写后的锚点。
    if (normalized.isEmpty())
        return QStringLiteral("middle");
    if (!QStringList{QStringLiteral("start"), QStringLiteral("middle"), QStringLiteral("end")}.contains(normalized)) {
        appendDiagnostic(
            diagnostics,
            QStringLiteral("符号 %1 %2 的对齐锚点无效: %3，已回退为 middle").arg(symbolName).arg(context).arg(anchor));
        return QStringLiteral("middle");
    }
    return normalized;
}

/**
 * @brief 将已通过位置校验的引脚文本字段映射到 Altium 记录。
 * @param value 文本内容。
 * @param position 文本位置。
 * @param fontSizeMm 字体大小，单位为毫米。
 * @param rotation 文本旋转角度。
 * @param anchor 文本锚点。
 * @param symbolName 符号名称。
 * @param context 文本字段上下文。
 * @param pin 所属的统一 IR 引脚。
 * @param diagnostics 可选的诊断列表。
 * @return 转换后的 Altium 文本记录。
 */
AltiumSchText makePinText(const QString& value,
                          const QPointF& position,
                          double fontSizeMm,
                          double rotation,
                          const QString& anchor,
                          const QString& symbolName,
                          const QString& context,
                          const IR::SymbolPinIR& pin,
                          QStringList* diagnostics) {
    AltiumSchText text;  // 转换后的 Altium 引脚文本记录。
    text.locationX = AltiumCoord::mmToRaw(position.x());
    text.locationY = AltiumCoord::mmToRaw(position.y());
    text.text = value;
    text.fontSizeMm = fontSizeMm;
    text.anchor = normalizeAnchor(anchor, symbolName, context, diagnostics);
    text.isDisplayed = true;
    text.orientation = AltiumSymbolConversionUtils::toAltiumOrientation(rotation);
    text.ownerPartId = pin.commonToAllParts ? -1 : AltiumSymbolConversionUtils::toAltiumOwnerPartId(pin.partIndex);
    text.isPinLabel = true;
    text.sourcePartIndex = pin.commonToAllParts ? -1 : pin.partIndex;
    return text;
}

}  // namespace

/**
 * @brief 转换引脚名称和编号文本，并将编号统一输出为水平文本。
 * @param pin 统一 IR 中的引脚。
 * @param symbolName 符号名称。
 * @param diagnostics 可选的诊断列表。
 * @return 转换后的引脚文本记录列表。
 */
QList<AltiumSchText> AltiumSymbolPinTextConverter::convert(const IR::SymbolPinIR& pin,
                                                           const QString& symbolName,
                                                           QStringList* diagnostics) {
    QList<AltiumSchText> texts;  // 当前引脚生成的名称和编号文本。
    if (pin.hasNamePosition && !pin.name.isEmpty()) {
        if (!std::isfinite(pin.nameFontSizeMm) || pin.nameFontSizeMm < 0.0 || !std::isfinite(pin.nameRotation) ||
            !std::isfinite(pin.namePosition.x()) || !std::isfinite(pin.namePosition.y())) {
            appendDiagnostic(
                diagnostics,
                QStringLiteral("符号 %1 引脚 %2 名称文本参数无效，已跳过").arg(symbolName).arg(pin.designator));
        } else {
            texts.append(makePinText(pin.name,
                                     pin.namePosition,
                                     pin.nameFontSizeMm,
                                     pin.nameRotation,
                                     pin.nameAnchor,
                                     symbolName,
                                     QStringLiteral("引脚 %1 名称文本").arg(pin.designator),
                                     pin,
                                     diagnostics));
        }
    }
    const bool showDesignator = pin.display.showDesignator || pin.showDesignator;  // 是否需要输出引脚编号。
    if (showDesignator && !pin.designator.isEmpty()) {
        // 没有可靠的源编号坐标时使用 IR 提供的引脚位置，避免回退到 Altium 随引脚旋转的内置编号。
        const QPointF numberPosition =
            pin.hasNumberPosition ? pin.numberPosition : pin.position;  // 编号文本的源位置或安全回退位置。
        if (!std::isfinite(pin.numberFontSizeMm) || pin.numberFontSizeMm < 0.0 || !std::isfinite(numberPosition.x()) ||
            !std::isfinite(numberPosition.y())) {
            appendDiagnostic(
                diagnostics,
                QStringLiteral("符号 %1 引脚 %2 编号文本参数无效，已跳过").arg(symbolName).arg(pin.designator));
        } else {
            // 将编号作为独立水平文本记录，避免依赖 Altium 引脚内置显示方向。
            AltiumSchText numberText = makePinText(pin.designator,
                                                   numberPosition,
                                                   pin.numberFontSizeMm,
                                                   0.0,
                                                   pin.numberAnchor,
                                                   symbolName,
                                                   QStringLiteral("引脚 %1 编号文本").arg(pin.designator),
                                                   pin,
                                                   diagnostics);
            numberText.isFallbackPinNumber = !pin.hasNumberPosition;
            texts.append(numberText);
        }
    }
    return texts;
}

}  // namespace EasyKiConverter
