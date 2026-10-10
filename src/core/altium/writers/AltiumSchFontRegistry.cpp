#include "AltiumSchFontRegistry.h"

#include <QtGlobal>

#include <cmath>

namespace EasyKiConverter {

/**
 * @brief 清空字体注册表，准备开始一次新的 SchLib 写入。
 */
void AltiumSchFontRegistry::clear() {
    m_entries.clear();
}

/**
 * @brief 按字体属性去重并返回符合 Altium 约定的 1-based 字体 ID。
 * @param fontName 字体名称。
 * @param fontSize 字体大小，单位为磅。
 * @param bold 是否加粗。
 * @param italic 是否倾斜。
 * @param underline 是否带下划线。
 * @return 字体表中的 1-based 字体 ID。
 */
int AltiumSchFontRegistry::getOrAdd(const QString& fontName, int fontSize, bool bold, bool italic, bool underline) {
    for (int i = 0; i < m_entries.size(); ++i) {
        const AltiumModels::FontEntry& entry = m_entries[i];  // 当前已注册字体条目。
        if (entry.name.compare(fontName, Qt::CaseInsensitive) == 0 && entry.size == fontSize && entry.bold == bold &&
            entry.italic == italic && entry.underline == underline) {
            return i + 1;
        }
    }

    AltiumModels::FontEntry entry;  // 待加入字体表的新条目。
    entry.name = fontName;
    entry.size = fontSize;
    entry.bold = bold;
    entry.italic = italic;
    entry.underline = underline;
    m_entries.append(entry);
    return m_entries.size();
}

/**
 * @brief 扫描所有文本类图元，确保 FileHeader 写入前字体表已经完整。
 * @param components 待读取字体信息的组件列表。
 */
void AltiumSchFontRegistry::registerTextFonts(const QList<AltiumSchComponent>& components) {
    constexpr double MILLIMETERS_PER_POINT = 25.4 / 72.0;  // 点制字体到毫米的换算系数。
    for (const AltiumSchComponent& component : components) {  // 遍历待注册字体的组件。
        for (const AltiumSchText& text : component.texts) {
            const bool hasValidFontSize =
                std::isfinite(text.fontSizeMm) && text.fontSizeMm > 0.0;  // 字体大小是否有效。
            if (text.fontName.isEmpty() && text.fontId > 0 && !hasValidFontSize)
                continue;
            const QString fontName =
                text.fontName.isEmpty() ? QStringLiteral("Times New Roman") : text.fontName;  // 文本字体名称。
            const int fontSize =
                hasValidFontSize ? qMax(1, qRound(text.fontSizeMm / MILLIMETERS_PER_POINT)) : 10;  // 文本字体磅值。
            getOrAdd(fontName, fontSize, text.bold, text.italic);
        }

        for (const AltiumSchTextFrame& frame : component.textFrames) {
            const bool hasValidFontSize =
                std::isfinite(frame.fontSizeMm) && frame.fontSizeMm > 0.0;  // 字体大小是否有效。
            if (frame.fontName.isEmpty() && frame.fontId > 0 && !hasValidFontSize)
                continue;
            const QString fontName =
                frame.fontName.isEmpty() ? QStringLiteral("Times New Roman") : frame.fontName;  // 文本框字体名称。
            const int fontSize =
                hasValidFontSize ? qMax(1, qRound(frame.fontSizeMm / MILLIMETERS_PER_POINT)) : 10;  // 文本框字体磅值。
            getOrAdd(fontName, fontSize, frame.bold, frame.italic);
        }

        for (const AltiumSchParameter& parameter : component.parameters) {
            if (!std::isfinite(parameter.fontSizeMm) || parameter.fontSizeMm <= 0.0)
                continue;
            const int fontSize = qMax(1, qRound(parameter.fontSizeMm / MILLIMETERS_PER_POINT));  // 参数字体磅值。
            getOrAdd(QStringLiteral("Times New Roman"), fontSize);
        }
    }
}

/**
 * @brief 返回字体表，供 FileHeader 序列化使用。
 * @return 字体表的只读引用。
 */
const QList<AltiumModels::FontEntry>& AltiumSchFontRegistry::entries() const {
    return m_entries;
}

/**
 * @brief 返回当前已注册字体数量，用于校验图元中的字体 ID。
 * @return 当前字体数量。
 */
int AltiumSchFontRegistry::size() const {
    return m_entries.size();
}

}  // namespace EasyKiConverter
