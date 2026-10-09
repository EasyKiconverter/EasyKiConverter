#ifndef ALTIUMSCHLIBRARYHEADERWRITER_H
#define ALTIUMSCHLIBRARYHEADERWRITER_H

#include "models/AltiumSchComponent.h"

#include <QList>
#include <QStringList>

namespace EasyKiConverter {

class AltiumSchLibWriter;
class OLECompoundWriter;

/**
 * @brief 写入 Altium SchLib 的文件级头部流。
 *
 * 该类负责 FileHeader 和可选 SectionKeys 流的参数编码，组件记录和图元
 * 数据仍由 AltiumSchLibWriter 负责，以保持文件级和组件级职责分离。
 */
class AltiumSchLibraryHeaderWriter final {
public:
    /**
     * @brief 创建绑定到 SchLib 主写入器的文件头写入器。
     * @param owner 所属的 SchLib 主写入器。
     */
    explicit AltiumSchLibraryHeaderWriter(AltiumSchLibWriter& owner);

    /**
     * @brief 写入包含字体表、组件列表和记录权重的 FileHeader 流。
     * @param ole SchLib 根 OLE 写入器。
     * @param components 待写入文件头的组件列表。
     */
    void writeFileHeader(OLECompoundWriter& ole, const QList<AltiumSchComponent>& components);

    /**
     * @brief 写入组件名称与存储键不一致时需要的 SectionKeys 流。
     * @param ole SchLib 根 OLE 写入器。
     * @param components 待写入的组件列表。
     * @param sectionKeys 组件对应的 OLE 存储键列表。
     */
    void writeSectionKeys(OLECompoundWriter& ole,
                          const QList<AltiumSchComponent>& components,
                          const QStringList& sectionKeys);

private:
    /** @brief 保存协作者所属的 SchLib 主写入器。 */
    AltiumSchLibWriter& m_owner;
};

}  // namespace EasyKiConverter

#endif  // ALTIUMSCHLIBRARYHEADERWRITER_H
