#ifndef ALTIUMSCHIMAGESTORAGEWRITER_H
#define ALTIUMSCHIMAGESTORAGEWRITER_H

#include "models/AltiumSchComponent.h"

#include <QList>

namespace EasyKiConverter {

class AltiumSchLibWriter;
class OLECompoundWriter;

/**
 * @brief 准备并写入 Altium SchLib 的嵌入图片 Storage 流。
 *
 * 该类负责嵌入文件名校验、重复名称消解和根 Storage 流写入；图片几何
 * 记录仍由 AltiumSchImageRecordWriter 编码。
 */
class AltiumSchImageStorageWriter final {
public:
    /**
     * @brief 创建绑定到 SchLib 主写入器的图片 Storage 写入器。
     * @param owner 所属的 SchLib 主写入器。
     */
    explicit AltiumSchImageStorageWriter(AltiumSchLibWriter& owner);

    /**
     * @brief 为嵌入图片准备稳定且唯一的 Storage 文件名。
     * @param components 待处理的原理图组件列表。
     */
    void prepareNames(const QList<AltiumSchComponent>& components);

    /**
     * @brief 将已准备好的嵌入图片编码并写入根 Storage 流。
     * @param ole SchLib 根 OLE 写入器。
     * @param components 待写入嵌入图片的组件列表。
     */
    void write(OLECompoundWriter& ole, const QList<AltiumSchComponent>& components);

private:
    /**
     * @brief 保存协作者所属的 SchLib 主写入器。
     */
    AltiumSchLibWriter& m_owner;
};

}  // namespace EasyKiConverter

#endif  // ALTIUMSCHIMAGESTORAGEWRITER_H
