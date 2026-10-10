#include "OLECompoundSerializer.h"

#include "OLECompoundWriter.h"

namespace EasyKiConverter {

/**
 * @brief 保存 OLE 写入器的只读状态引用。
 */
OLECompoundSerializer::OLECompoundSerializer(const OLECompoundWriter& owner) : m_owner(owner) {}

/**
 * @brief 将目录条目按 CFB V3 的固定字段顺序编码。
 * @param entry 待编码的目录条目。
 * @param buffer 接收编码结果的缓冲区。
 */
void OLECompoundSerializer::serializeDirectoryEntry(const OLECompoundWriter::DirectoryEntry& entry,
                                                    QByteArray& buffer) const {
    buffer.resize(OLECompoundWriter::DIR_ENTRY_SIZE);
    buffer.fill(0);
    int offset = 0;  // 当前写入位置。

    // 名称（64 字节，UTF-16LE）。
    for (int i = 0; i < 32; ++i) {
        buffer[offset++] = static_cast<char>(entry.name[i] & 0xFF);
        buffer[offset++] = static_cast<char>((entry.name[i] >> 8) & 0xFF);
    }

    // 名称大小（2 字节，包含结尾空字符）。
    buffer[offset++] = static_cast<char>(entry.nameSize & 0xFF);
    buffer[offset++] = static_cast<char>((entry.nameSize >> 8) & 0xFF);

    // 对象类型和红黑树颜色标志。
    buffer[offset++] = static_cast<char>(entry.objectType);
    buffer[offset++] = static_cast<char>(entry.colorFlag);

    // 左子节点、右子节点和子节点索引。
    const uint32_t childLinks[] = {entry.leftChild, entry.rightChild, entry.child};  // 三个目录树链接索引。
    for (uint32_t link : childLinks) {
        buffer[offset++] = static_cast<char>(link & 0xFF);
        buffer[offset++] = static_cast<char>((link >> 8) & 0xFF);
        buffer[offset++] = static_cast<char>((link >> 16) & 0xFF);
        buffer[offset++] = static_cast<char>((link >> 24) & 0xFF);
    }

    // CLSID（16 字节）。
    for (uint8_t value : entry.clsid) {
        buffer[offset++] = static_cast<char>(value);
    }

    // StateBits（4 字节）。
    for (int i = 0; i < 4; ++i) {
        buffer[offset++] = static_cast<char>((entry.stateBits >> (i * 8)) & 0xFF);
    }

    // 创建时间和修改时间（各 8 字节）。
    for (int i = 0; i < 8; ++i) {
        buffer[offset++] = static_cast<char>((entry.creationTime >> (i * 8)) & 0xFF);
    }
    for (int i = 0; i < 8; ++i) {
        buffer[offset++] = static_cast<char>((entry.modifiedTime >> (i * 8)) & 0xFF);
    }

    // 起始扇区（4 字节）。
    buffer[offset++] = static_cast<char>(entry.startSector & 0xFF);
    buffer[offset++] = static_cast<char>((entry.startSector >> 8) & 0xFF);
    buffer[offset++] = static_cast<char>((entry.startSector >> 16) & 0xFF);
    buffer[offset++] = static_cast<char>((entry.startSector >> 24) & 0xFF);

    // 流大小（8 字节）。
    for (int i = 0; i < 8; ++i) {
        buffer[offset++] = static_cast<char>((entry.streamSize >> (i * 8)) & 0xFF);
    }
}

/**
 * @brief 将 OLE 版本、扇区布局和 FAT 索引编码到固定文件头。
 * @param header 接收文件头编码结果的缓冲区。
 */
void OLECompoundSerializer::serializeFileHeader(QByteArray& header) const {
    header.resize(OLECompoundWriter::SECTOR_SIZE);
    header.fill(0);
    int offset = 0;  // 当前文件头写入位置。

    // Compound File Binary Format 魔数（8 字节）。
    const char signature[] = {static_cast<char>(0xD0),
                              static_cast<char>(0xCF),
                              static_cast<char>(0x11),
                              static_cast<char>(0xE0),
                              static_cast<char>(0xA1),
                              static_cast<char>(0xB1),
                              static_cast<char>(0x1A),
                              static_cast<char>(0xE1)};
    for (char value : signature) {
        header[offset++] = value;
    }

    // CLSID（16 字节）保留为零。
    offset += 16;

    // V3 版本、小端字节序、512 字节扇区和 64 字节 mini sector。
    header[offset++] = 0x3E;
    header[offset++] = 0x00;
    header[offset++] = 0x03;
    header[offset++] = 0x00;
    header[offset++] = static_cast<char>(0xFE);
    header[offset++] = static_cast<char>(0xFF);
    header[offset++] = 0x09;
    header[offset++] = 0x00;
    header[offset++] = 0x06;
    header[offset++] = 0x00;

    // 保留字段和 V3 不使用的目录扇区数。
    offset += 6;
    offset += 4;

    const uint32_t fatSectorCount =
        static_cast<uint32_t>(m_owner.m_fat.size()) / (OLECompoundWriter::SECTOR_SIZE / 4);  // FAT 扇区数量。
    writeUInt32(header, offset, fatSectorCount);

    // FAT 和 DIFAT 位于文件头之后，目录紧随其后。
    const uint32_t difatSectorCount = static_cast<uint32_t>(m_owner.m_difatSectors.size());  // DIFAT 扇区数量。
    writeUInt32(header, offset, fatSectorCount + difatSectorCount);

    // Transaction signature 保留为零。
    offset += 4;

    // 小流阈值为 4096 字节。
    writeUInt32(header, offset, 4096);

    uint32_t firstMiniFatSector = OLECompoundWriter::ENDOFCHAIN;  // 第一个 MiniFAT 扇区索引。
    if (!m_owner.m_miniFat.isEmpty()) {
        const uint32_t directorySectors =
            (static_cast<uint32_t>(m_owner.m_directory.size()) * OLECompoundWriter::DIR_ENTRY_SIZE +
             OLECompoundWriter::SECTOR_SIZE - 1) /
            OLECompoundWriter::SECTOR_SIZE;  // 目录占用的扇区数量。
        firstMiniFatSector = fatSectorCount + difatSectorCount + directorySectors;
    }
    writeUInt32(header, offset, firstMiniFatSector);

    const uint32_t miniFatSectorCount =
        static_cast<uint32_t>((m_owner.m_miniFat.size() * 4 + OLECompoundWriter::SECTOR_SIZE - 1) /
                              OLECompoundWriter::SECTOR_SIZE);  // MiniFAT 占用的扇区数量。
    writeUInt32(header, offset, miniFatSectorCount);

    const uint32_t firstDifatSector =
        difatSectorCount == 0 ? OLECompoundWriter::ENDOFCHAIN : fatSectorCount;  // 第一个 DIFAT 扇区索引。
    writeUInt32(header, offset, firstDifatSector);
    writeUInt32(header, offset, difatSectorCount);

    // 头部最多直接保存 109 个 FAT 扇区索引。
    for (uint32_t i = 0; i < 109; ++i) {
        writeUInt32(header, offset, i < fatSectorCount ? i : OLECompoundWriter::FREESECT);
    }
}

/**
 * @brief 按小端序写入一个无符号 32 位字段并推进偏移。
 * @param buffer 接收字段的缓冲区。
 * @param offset 当前写入位置，并在写入后推进。
 * @param value 待写入的字段值。
 */
void OLECompoundSerializer::writeUInt32(QByteArray& buffer, int& offset, uint32_t value) {
    buffer[offset++] = static_cast<char>(value & 0xFF);
    buffer[offset++] = static_cast<char>((value >> 8) & 0xFF);
    buffer[offset++] = static_cast<char>((value >> 16) & 0xFF);
    buffer[offset++] = static_cast<char>((value >> 24) & 0xFF);
}

}  // namespace EasyKiConverter
