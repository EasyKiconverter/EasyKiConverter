#pragma once

#include "models/AltiumCommon.h"
#include "models/AltiumSchComponent.h"

#include <QList>
#include <QString>

namespace EasyKiConverter {

/**
 * @brief 管理 SchLib 文件头使用的字体表。
 * @details 负责字体去重、字体 ID 分配，以及在写入 FileHeader 前预注册图元使用的字体。
 */
class AltiumSchFontRegistry {
public:
    /**
     * @brief 清空当前字体表。
     */
    void clear();

    /**
     * @brief 获取或添加字体并返回 Altium 使用的 1-based ID。
     * @param fontName 字体名称
     * @param fontSize 字体大小（磅）
     * @param bold 是否加粗
     * @param italic 是否倾斜
     * @param underline 是否带下划线
     * @return 字体表中的 1-based ID
     */
    int getOrAdd(const QString& fontName, int fontSize, bool bold = false, bool italic = false, bool underline = false);

    /**
     * @brief 预注册组件中所有文本图元使用的字体。
     * @param components 待读取字体信息的组件列表
     */
    void registerTextFonts(const QList<AltiumSchComponent>& components);

    /**
     * @brief 获取当前字体表。
     * @return 字体表的只读引用
     */
    const QList<AltiumModels::FontEntry>& entries() const;

    /**
     * @brief 获取当前字体数量。
     * @return 字体数量
     */
    int size() const;

private:
    /**
     * @brief 保存已注册字体，列表下标加一即为 Altium 字体 ID。
     */
    QList<AltiumModels::FontEntry> m_entries;
};

}  // namespace EasyKiConverter
