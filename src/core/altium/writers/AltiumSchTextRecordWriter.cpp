#include "AltiumSchTextRecordWriter.h"

#include "AltiumSchLibWriter.h"

#include <QDebug>

#include <cmath>

namespace EasyKiConverter {

/**
 * @brief 创建 SchLib 文本记录写入器。
 * @param owner 所属的 SchLib 主写入器。
 */
AltiumSchTextRecordWriter::AltiumSchTextRecordWriter(AltiumSchLibWriter& owner) : m_owner(owner) {}

/**
 * @brief 写入文本记录并处理字体、可见性和对齐属性
 * @param writer 目标二进制写入器。
 * @param text 待写入的文本模型。
 */
void AltiumSchTextRecordWriter::writeText(AltiumBinaryWriter& writer, const AltiumSchText& text) {
    QMap<QString, QString> params;  // 当前文本的 RECORD=4 参数。
    params["RECORD"] = "4";
    m_owner.addOwnerParams(params, text.ownerPartId);
    m_owner.addCoordParam(params, "Location.X", text.locationX);
    m_owner.addCoordParam(params, "Location.Y", text.locationY);

    if (text.text.trimmed().isEmpty()) {
        const QString diagnostic =
            QStringLiteral("Altium SchLib 文本内容为空，仍保留记录以维持记录计数");  // 空文本诊断。
        m_owner.m_diagnostics.append(diagnostic);
        qWarning() << "AltiumSchLibWriter:" << diagnostic;
    }

    const bool hasValidFontSize = std::isfinite(text.fontSizeMm) && text.fontSizeMm > 0.0;  // 字体大小是否有效。
    if (text.fontSizeMm != 0.0 && !hasValidFontSize) {
        const QString diagnostic =
            QStringLiteral("Altium SchLib 文本字体大小无效，已回退为默认字体大小");  // 字体大小诊断。
        m_owner.m_diagnostics.append(diagnostic);
        qWarning() << "AltiumSchLibWriter:" << diagnostic;
    }
    if (text.fontName.isEmpty() && !hasValidFontSize &&
        (text.fontId < 1 || text.fontId > m_owner.m_fontRegistry.size())) {
        const QString diagnostic =
            QStringLiteral("Altium SchLib 文本字体 ID 无效，已回退为默认字体");  // 字体 ID 诊断。
        m_owner.m_diagnostics.append(diagnostic);
        qWarning() << "AltiumSchLibWriter:" << diagnostic;
    }

    if (text.orientation != 0)
        params["Orientation"] = QString::number(text.orientation);
    m_owner.addColorParam(params, "Color", text.color);
    int fontId = text.fontId;  // 当前文本使用的字体 ID。
    if (!text.fontName.isEmpty() || hasValidFontSize || fontId <= 0) {
        constexpr double MILLIMETERS_PER_POINT = 25.4 / 72.0;  // 点制字体到毫米的换算系数。
        const QString fontName =
            text.fontName.isEmpty() ? QStringLiteral("Times New Roman") : text.fontName;  // 文本字体名称。
        const int fontSize =
            hasValidFontSize ? qMax(1, qRound(text.fontSizeMm / MILLIMETERS_PER_POINT)) : 10;  // 文本字体磅值。
        fontId = m_owner.m_fontRegistry.getOrAdd(fontName, fontSize, text.bold, text.italic);
    } else if (fontId < 1 || fontId > m_owner.m_fontRegistry.size()) {
        fontId = 1;
    }
    params["FontID"] = QString::number(fontId);
    params["Text"] = text.text;
    if (text.isHidden || !text.isDisplayed)
        params["IsHidden"] = "T";
    const QString normalizedAnchor = text.anchor.trimmed().toLower();  // 规范化后的文本对齐锚点。
    if (!normalizedAnchor.isEmpty()) {
        if (!QStringList{QStringLiteral("start"), QStringLiteral("middle"), QStringLiteral("end")}.contains(
                normalizedAnchor)) {
            const QString diagnostic =  // 对齐锚点无效时的诊断文本。
                QStringLiteral("Altium SchLib 文本对齐锚点无效: %1，已回退为 middle").arg(text.anchor);
            m_owner.m_diagnostics.append(diagnostic);
            qWarning() << "AltiumSchLibWriter:" << diagnostic;
            params["TextAnchor"] = QStringLiteral("middle");
        } else {
            params["TextAnchor"] = normalizedAnchor;
        }
    }
    if (hasValidFontSize)
        params["FontSize"] = QString::number(text.fontSizeMm, 'f', 4);

    m_owner.addUniqueID(params);
    writer.writeCStringParameterBlockUtf8(params);
}

/**
 * @brief 写入文本框记录并处理字体、边框和换行属性
 * @param writer 目标二进制写入器。
 * @param frame 待写入的文本框模型。
 */
void AltiumSchTextRecordWriter::writeTextFrame(AltiumBinaryWriter& writer, const AltiumSchTextFrame& frame) {
    QMap<QString, QString> params;  // 当前文本框的 RECORD=28 参数。
    params["RECORD"] = "28";
    m_owner.addOwnerParams(params, frame.ownerPartId);
    m_owner.addCoordParam(params, "Location.X", frame.locationX);
    m_owner.addCoordParam(params, "Location.Y", frame.locationY);
    m_owner.addCoordParam(params, "Corner.X", frame.cornerX);
    m_owner.addCoordParam(params, "Corner.Y", frame.cornerY);
    if (frame.lineWidth != 0)
        params["LineWidth"] = QString::number(frame.lineWidth);
    if (frame.lineStyle != 0)
        params["LineStyle"] = QString::number(frame.lineStyle);
    m_owner.addColorParam(params, "Color", frame.color);
    params["AreaColor"] = QString::number(frame.areaColor);
    m_owner.addColorParam(params, "TextColor", frame.textColor);
    const bool hasValidFontSize = std::isfinite(frame.fontSizeMm) && frame.fontSizeMm > 0.0;  // 字体大小是否有效。
    if (frame.fontSizeMm != 0.0 && !hasValidFontSize) {
        const QString diagnostic =
            QStringLiteral("Altium SchLib 文本框字体大小无效，已回退为默认字体大小");  // 字体大小诊断。
        m_owner.m_diagnostics.append(diagnostic);
        qWarning() << "AltiumSchLibWriter:" << diagnostic;
    }
    int fontId = frame.fontId;  // 当前文本框使用的字体 ID。
    if (!frame.fontName.isEmpty() || hasValidFontSize || fontId <= 0) {
        constexpr double MILLIMETERS_PER_POINT = 25.4 / 72.0;  // 点制字体到毫米的换算系数。
        const QString fontName =
            frame.fontName.isEmpty() ? QStringLiteral("Times New Roman") : frame.fontName;  // 文本框字体名称。
        const int fontSize =
            hasValidFontSize ? qMax(1, qRound(frame.fontSizeMm / MILLIMETERS_PER_POINT)) : 10;  // 文本框字体磅值。
        fontId = m_owner.m_fontRegistry.getOrAdd(fontName, fontSize, frame.bold, frame.italic);
    }
    if (frame.fontName.isEmpty() && !hasValidFontSize && (fontId < 1 || fontId > m_owner.m_fontRegistry.size())) {
        const QString diagnostic =
            QStringLiteral("Altium SchLib 文本框字体 ID 无效，已回退为默认字体");  // 字体 ID 诊断。
        m_owner.m_diagnostics.append(diagnostic);
        qWarning() << "AltiumSchLibWriter:" << diagnostic;
    }
    params["FontID"] = QString::number(fontId >= 1 && fontId <= m_owner.m_fontRegistry.size() ? fontId : 1);
    if (hasValidFontSize)
        params["FontSize"] = QString::number(frame.fontSizeMm, 'f', 4);
    if (frame.text.trimmed().isEmpty()) {
        const QString diagnostic =
            QStringLiteral("Altium SchLib 文本框内容为空，仍保留记录以维持记录计数");  // 空文本框诊断。
        m_owner.m_diagnostics.append(diagnostic);
        qWarning() << "AltiumSchLibWriter:" << diagnostic;
    }
    if (frame.isSolid)
        params["IsSolid"] = "T";
    if (frame.showBorder)
        params["ShowBorder"] = "T";
    if (frame.orientation != 0)
        params["Orientation"] = QString::number(frame.orientation);
    if (frame.alignment != 0)
        params["Alignment"] = QString::number(frame.alignment);
    if (frame.wordWrap)
        params["WordWrap"] = "T";
    if (frame.clipToRect)
        params["ClipToRect"] = "T";
    params["Text"] = frame.text;
    m_owner.addCoordParam(params, "TextMargin", frame.textMargin);
    if (frame.transparent)
        params["Transparent"] = "T";
    m_owner.addUniqueID(params);
    writer.writeCStringParameterBlockUtf8(params);
}

}  // namespace EasyKiConverter
