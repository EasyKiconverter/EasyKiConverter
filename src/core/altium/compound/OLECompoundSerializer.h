#pragma once

#include "OLECompoundWriter.h"

#include <QByteArray>

namespace EasyKiConverter {

/**
 * @brief 编码 OLE 复合文档的文件级固定结构。
 *
 * 该协作者只负责目录条目和文件头的字节序列化，不参与存储树构建、流分配或文件落盘。
 */
class OLECompoundSerializer final {
public:
    /**
     * @brief 保存所属 OLE 写入器的只读状态引用。
     * @param owner 被序列化的 OLE 写入器。
     */
    explicit OLECompoundSerializer(const OLECompoundWriter& owner);

    /**
     * @brief 将一个目录条目编码为固定 128 字节记录。
     * @param entry 待编码的目录条目。
     * @param buffer 接收序列化字节的缓冲区。
     */
    void serializeDirectoryEntry(const OLECompoundWriter::DirectoryEntry& entry, QByteArray& buffer) const;

    /**
     * @brief 将当前 OLE 文档状态编码为 512 字节文件头。
     * @param header 接收文件头字节的缓冲区。
     */
    void serializeFileHeader(QByteArray& header) const;

private:
    /**
     * @brief 按小端序写入无符号 32 位字段。
     * @param buffer 接收字段的字节缓冲区。
     * @param offset 当前写入偏移，并在写入后递增。
     * @param value 待写入的无符号 32 位值。
     */
    static void writeUInt32(QByteArray& buffer, int& offset, uint32_t value);

    /**
     * @brief 被编码的 OLE 写入器。
     */
    const OLECompoundWriter& m_owner;
};

}  // namespace EasyKiConverter
