#include "XpeditionZipWriter.h"

#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>

namespace EasyKiConverter {

namespace {

/**
 * @brief 计算 ZIP 条目的 CRC-32 校验值。
 * @param data 条目原始内容。
 * @return ZIP 规范要求的 CRC-32 值。
 */
quint32 crc32(const QByteArray& data) {
    quint32 crc = 0xFFFFFFFFu;  // CRC-32 累积值。
    for (const auto byte : data) {
        crc ^= static_cast<quint8>(byte);
        for (int bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (0xEDB88320u & static_cast<quint32>(-(crc & 1u)));
    }
    return ~crc;
}

/**
 * @brief 以小端序追加一个 16 位整数。
 * @param output 待追加的字节数组。
 * @param value 待写入的整数。
 */
void appendU16(QByteArray& output, quint16 value) {
    output.append(static_cast<char>(value & 0xFF));
    output.append(static_cast<char>((value >> 8) & 0xFF));
}

/**
 * @brief 以小端序追加一个 32 位整数。
 * @param output 待追加的字节数组。
 * @param value 待写入的整数。
 */
void appendU32(QByteArray& output, quint32 value) {
    appendU16(output, static_cast<quint16>(value & 0xFFFF));
    appendU16(output, static_cast<quint16>((value >> 16) & 0xFFFF));
}

/**
 * @brief 校验 ZIP 条目名称是否为安全的相对路径。
 * @param name 待校验的条目名称。
 * @return 不包含绝对路径和目录穿越片段时返回 true。
 */
bool isSafeEntryName(const QString& name) {
    if (name.isEmpty() || name.startsWith('/') || name.contains('\\'))
        return false;
    const QStringList parts = name.split('/', Qt::KeepEmptyParts);  // 相对路径片段。
    for (const QString& part : parts) {
        if (part.isEmpty() || part == "." || part == "..")
            return false;
    }
    return true;
}

}  // namespace

/**
 * @brief 添加 ZIP 条目并校验路径安全性与名称唯一性。
 * @param name ZIP 内的相对条目名称。
 * @param data 条目内容。
 * @return 条目可安全加入时返回 true。
 */
bool XpeditionZipWriter::addFile(const QString& name, const QByteArray& data) {
    // 先拒绝不安全路径，再拒绝重复名称，避免生成不可预测的库结构。
    if (!isSafeEntryName(name))
        return false;
    for (const Entry& entry : m_entries) {
        if (entry.name == name)
            return false;
    }
    m_entries.append({name, data});
    return true;
}

/**
 * @brief 按 ZIP 规范写入本地文件头、数据、中央目录和结束记录。
 * @param filePath 输出 ZIP 文件路径。
 * @return 文件成功写入时返回 true。
 */
bool XpeditionZipWriter::write(const QString& filePath) const {
    // 采用无压缩 ZIP，直接写入本地文件头、中央目录和结束记录。
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;

    QByteArray body;  // ZIP 本地文件头、数据和目录内容。

    struct CentralEntry {
        QByteArray name;  ///< UTF-8 条目名称。
        quint32 crc = 0;  ///< 条目 CRC-32 校验值。
        quint32 size = 0;  ///< 条目数据长度。
        quint32 offset = 0;  ///< 本地文件头在归档中的偏移。
    };

    QVector<CentralEntry> central;  // 待写入的中央目录条目。

    for (const Entry& entry : m_entries) {
        const QByteArray name = entry.name.toUtf8();  // UTF-8 条目名称。
        const quint32 offset = static_cast<quint32>(body.size());  // 当前本地头偏移。
        const quint32 checksum = crc32(entry.data);  // 当前条目校验值。
        appendU32(body, 0x04034B50u);
        appendU16(body, 20);
        appendU16(body, 0);
        appendU16(body, 0);
        appendU16(body, 0);
        appendU16(body, 0);
        appendU32(body, checksum);
        appendU32(body, static_cast<quint32>(entry.data.size()));
        appendU32(body, static_cast<quint32>(entry.data.size()));
        appendU16(body, static_cast<quint16>(name.size()));
        appendU16(body, 0);
        body.append(name);
        body.append(entry.data);
        central.append({name, checksum, static_cast<quint32>(entry.data.size()), offset});
    }

    const quint32 centralOffset = static_cast<quint32>(body.size());  // 中央目录起始偏移。
    for (const CentralEntry& entry : central) {
        appendU32(body, 0x02014B50u);
        appendU16(body, 20);
        appendU16(body, 20);
        appendU16(body, 0);
        appendU16(body, 0);
        appendU16(body, 0);
        appendU16(body, 0);
        appendU32(body, entry.crc);
        appendU32(body, entry.size);
        appendU32(body, entry.size);
        appendU16(body, static_cast<quint16>(entry.name.size()));
        appendU16(body, 0);
        appendU16(body, 0);
        appendU16(body, 0);
        appendU16(body, 0);
        appendU32(body, 0);
        appendU32(body, entry.offset);
        body.append(entry.name);
    }

    const quint32 centralSize = static_cast<quint32>(body.size()) - centralOffset;  // 中央目录长度。
    appendU32(body, 0x06054B50u);
    appendU16(body, 0);
    appendU16(body, 0);
    appendU16(body, static_cast<quint16>(central.size()));
    appendU16(body, static_cast<quint16>(central.size()));
    appendU32(body, centralSize);
    appendU32(body, centralOffset);
    appendU16(body, 0);

    return file.write(body) == body.size();
}

}  // namespace EasyKiConverter
