#include "AltiumSymbolPinTextConverter.h"

#include "utils/AltiumCoord.h"
#include "utils/AltiumSymbolConversionUtils.h"

#include <cmath>

namespace EasyKiConverter {

namespace {

/** @brief 向可选诊断列表追加一条引脚文本诊断。 */
void appendDiagnostic(QStringList* diagnostics, const QString& message) {
    if (diagnostics != nullptr)
        diagnostics->append(message);
}

/** @brief 规范化引脚文本锚点，并在非法值时记录回退诊断。 */
QString normalizeAnchor(const QString& anchor,
                        const QString& symbolName,
                        const QString& context,
                        QStringList* diagnostics) {
    const QString normalized = anchor.trimmed().toLower();
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

/** @brief 将已通过位置校验的引脚文本字段映射到 Altium 记录。 */
AltiumSchText makePinText(const QString& value,
                          const QPointF& position,
                          double fontSizeMm,
                          double rotation,
                          const QString& anchor,
                          const QString& symbolName,
                          const QString& context,
                          const IR::SymbolPinIR& pin,
                          QStringList* diagnostics) {
    AltiumSchText text;
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

/** @brief 转换引脚名称和编号文本，并将编号统一输出为水平文本。 */
QList<AltiumSchText> AltiumSymbolPinTextConverter::convert(const IR::SymbolPinIR& pin,
                                                           const QString& symbolName,
                                                           QStringList* diagnostics) {
    QList<AltiumSchText> texts;
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
    const bool showDesignator = pin.display.showDesignator || pin.showDesignator;
    if (showDesignator && !pin.designator.isEmpty()) {
        // 没有可靠的源编号坐标时使用 IR 提供的引脚位置，避免回退到 Altium 随引脚旋转的内置编号。
        const QPointF numberPosition = pin.hasNumberPosition ? pin.numberPosition : pin.position;
        if (!std::isfinite(pin.numberFontSizeMm) || pin.numberFontSizeMm < 0.0 || !std::isfinite(numberPosition.x()) ||
            !std::isfinite(numberPosition.y())) {
            appendDiagnostic(
                diagnostics,
                QStringLiteral("符号 %1 引脚 %2 编号文本参数无效，已跳过").arg(symbolName).arg(pin.designator));
        } else {
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
