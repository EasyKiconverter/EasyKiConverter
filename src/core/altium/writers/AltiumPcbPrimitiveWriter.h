#ifndef ALTIUMPCBPRIMITIVEWRITER_H
#define ALTIUMPCBPRIMITIVEWRITER_H

#include "models/AltiumPcbComponent.h"
#include "utils/AltiumBinaryWriter.h"

namespace EasyKiConverter {

class AltiumPcbLibWriter;

/**
 * @brief 写入 PcbLib 封装中的各类二进制图元记录。
 *
 * 该类集中处理焊盘、走线、弧线、文本、填充、区域和组件实体的记录格式，
 * 文件级存储和封装级容器仍由 AltiumPcbLibWriter 负责。
 */
class AltiumPcbPrimitiveWriter final {
public:
    /**
     * @brief 创建绑定到 PcbLib 主写入器的图元写入器。
     * @param owner 所属的 PcbLib 主写入器。
     */
    explicit AltiumPcbPrimitiveWriter(AltiumPcbLibWriter& owner);

    /**
     * @brief 写入焊盘记录。
     * @param writer 目标二进制写入器。
     * @param pad 焊盘模型。
     */
    void writePad(AltiumBinaryWriter& writer, const AltiumPcbPad& pad);

    /**
     * @brief 写入走线记录。
     * @param writer 目标二进制写入器。
     * @param track 走线模型。
     * @param componentIndex 所属元件索引。
     */
    void writeTrack(AltiumBinaryWriter& writer, const AltiumPcbTrack& track, int componentIndex);

    /**
     * @brief 写入弧线记录。
     * @param writer 目标二进制写入器。
     * @param arc 弧线模型。
     */
    void writeArc(AltiumBinaryWriter& writer, const AltiumPcbArc& arc);

    /**
     * @brief 写入文本记录。
     * @param writer 目标二进制写入器。
     * @param text 文本模型。
     */
    void writeText(AltiumBinaryWriter& writer, const AltiumPcbText& text);

    /**
     * @brief 写入填充记录。
     * @param writer 目标二进制写入器。
     * @param fill 填充模型。
     */
    void writeFill(AltiumBinaryWriter& writer, const AltiumPcbFill& fill);

    /**
     * @brief 写入区域记录。
     * @param writer 目标二进制写入器。
     * @param region 区域模型。
     */
    void writeRegion(AltiumBinaryWriter& writer, const AltiumPcbRegion& region);

    /**
     * @brief 写入组件实体记录。
     * @param writer 目标二进制写入器。
     * @param body 组件实体模型。
     */
    void writeComponentBody(AltiumBinaryWriter& writer, const AltiumPcbComponentBody& body);

private:
    /**
     * @brief 写入焊盘记录末尾的多层扩展块。
     * @param writer 目标二进制写入器。
     * @param pad 焊盘模型。
     */
    void writePadExtendedBlock(AltiumBinaryWriter& writer, const AltiumPcbPad& pad);

    /**
     * @brief 保存主写入器引用，不负责其生命周期。
     */
    AltiumPcbLibWriter& m_owner;
};

}  // namespace EasyKiConverter

#endif  // ALTIUMPCBPRIMITIVEWRITER_H
