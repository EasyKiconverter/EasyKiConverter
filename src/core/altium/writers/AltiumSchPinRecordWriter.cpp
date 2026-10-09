#include "AltiumSchPinRecordWriter.h"

#include "AltiumSchLibWriter.h"
#include "utils/AltiumConstants.h"
#include "utils/AltiumCoord.h"

namespace EasyKiConverter {

/**
 * @brief 创建 SchLib 引脚记录写入器。
 * @param owner 所属的 SchLib 主写入器。
 */
AltiumSchPinRecordWriter::AltiumSchPinRecordWriter(AltiumSchLibWriter& owner) : m_owner(owner) {}

/**
 * @brief 编码引脚二进制记录并递增共享内容序号。
 * @param writer 目标二进制写入器。
 * @param pin 待写入的引脚模型。
 */
void AltiumSchPinRecordWriter::write(AltiumBinaryWriter& writer, const AltiumSchPin& pin) {
    writer.beginBlock(AltiumConstants::SCH_BLOCK_FLAG_BINARY_PIN);

    writer.writeInt32(2);  // Record type = 2
    writer.writeUInt8(0);  // Unknown
    writer.writeInt16(static_cast<int16_t>(m_owner.normalizeOwnerPartId(pin.ownerPartId, QStringLiteral("引脚"))));
    // OwnerPartId 为 -1 表示公共 Part Zero；当前符号只有一个显示模式。
    writer.writeUInt8(1);  // OwnerPartDisplayMode

    // Symbol edges / IEEE 装饰必须位于描述字符串之前。
    writer.writeUInt8(pin.symbolInnerEdge);
    writer.writeUInt8(pin.symbolOuterEdge);
    writer.writeUInt8(pin.symbolInside);
    writer.writeUInt8(pin.symbolOutside);

    // Description 使用空 Pascal 短字符串。
    writer.writePascalShortString("");

    // FormalType=1 才会让 Altium 将记录作为可连接的 Pin 对象处理。
    writer.writeUInt8(1);  // FormalType: normal pin
    writer.writeUInt8(static_cast<uint8_t>(pin.electricalType));  // ElectricalType

    // PinConglomerate：低两位为方向，其余位表示隐藏、显示名称和显示编号。
    uint8_t conglomerate = static_cast<uint8_t>(pin.orientation);
    if (pin.isHidden)
        conglomerate |= 0x04;
    if (pin.showName)
        conglomerate |= 0x08;
    if (pin.showDesignator)
        conglomerate |= 0x10;
    writer.writeUInt8(conglomerate);

    writer.writeInt16(AltiumCoord::toDxpInt(pin.length));
    writer.writeInt16(AltiumCoord::toDxpInt(pin.locationX));
    writer.writeInt16(AltiumCoord::toDxpInt(pin.locationY));
    writer.writeUInt32(pin.color);

    writer.writePascalShortString(pin.name);
    writer.writePascalShortString(pin.designator);
    writer.writePascalShortString("");  // SwapIdGroup
    writer.writePascalShortString("");  // PartAndSequence
    writer.writePascalShortString("");  // DefaultValue

    writer.endBlock();
    // 二进制引脚没有文本形式的 IndexInSheet，但仍占用共享内容记录序号。
    ++m_owner.m_nextIndexInSheet;
}

}  // namespace EasyKiConverter
