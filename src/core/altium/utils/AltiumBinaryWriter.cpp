#include "AltiumBinaryWriter.h"

#include <cstring>

namespace EasyKiConverter {

/**
 * @brief 构造函数
 */
AltiumBinaryWriter::AltiumBinaryWriter(QByteArray& buffer) : m_buffer(buffer) {}

/**
 * @brief 以单字节形式写入有符号八位整数
 * @param value 待写入的整数值
 */
void AltiumBinaryWriter::writeInt8(int8_t value) {
    m_buffer.append(static_cast<char>(value));
}

/**
 * @brief 以单字节形式写入无符号八位整数
 * @param value 待写入的整数值
 */
void AltiumBinaryWriter::writeUInt8(uint8_t value) {
    m_buffer.append(static_cast<char>(value));
}

/**
 * @brief 按小端序写入有符号十六位整数
 * @param value 待写入的整数值
 */
void AltiumBinaryWriter::writeInt16(int16_t value) {
    m_buffer.append(static_cast<char>(value & 0xFF));
    m_buffer.append(static_cast<char>((value >> 8) & 0xFF));
}

/**
 * @brief 按小端序写入无符号十六位整数
 * @param value 待写入的整数值
 */
void AltiumBinaryWriter::writeUInt16(uint16_t value) {
    m_buffer.append(static_cast<char>(value & 0xFF));
    m_buffer.append(static_cast<char>((value >> 8) & 0xFF));
}

/**
 * @brief 按小端序写入有符号三十二位整数
 * @param value 待写入的整数值
 */
void AltiumBinaryWriter::writeInt32(int32_t value) {
    m_buffer.append(static_cast<char>(value & 0xFF));
    m_buffer.append(static_cast<char>((value >> 8) & 0xFF));
    m_buffer.append(static_cast<char>((value >> 16) & 0xFF));
    m_buffer.append(static_cast<char>((value >> 24) & 0xFF));
}

/**
 * @brief 按小端序写入无符号三十二位整数
 * @param value 待写入的整数值
 */
void AltiumBinaryWriter::writeUInt32(uint32_t value) {
    m_buffer.append(static_cast<char>(value & 0xFF));
    m_buffer.append(static_cast<char>((value >> 8) & 0xFF));
    m_buffer.append(static_cast<char>((value >> 16) & 0xFF));
    m_buffer.append(static_cast<char>((value >> 24) & 0xFF));
}

/**
 * @brief 按 IEEE 754 位模式写入单精度浮点数
 * @param value 待写入的浮点数
 */
void AltiumBinaryWriter::writeFloat(float value) {
    uint32_t bits = 0;  // 浮点数的原始位模式。
    std::memcpy(&bits, &value, sizeof(float));
    writeUInt32(bits);
}

/**
 * @brief 按 IEEE 754 位模式写入双精度浮点数
 * @param value 待写入的浮点数
 */
void AltiumBinaryWriter::writeDouble(double value) {
    uint64_t bits = 0;  // 浮点数的原始位模式。
    std::memcpy(&bits, &value, sizeof(double));
    for (int i = 0; i < 8; ++i) {
        m_buffer.append(static_cast<char>((bits >> (i * 8)) & 0xFF));
    }
}

/**
 * @brief 将原始字节追加到当前输出缓冲区
 * @param data 待追加的字节数据
 */
void AltiumBinaryWriter::writeBytes(const QByteArray& data) {
    m_buffer.append(data);
}

/**
 * @brief 开始一个大小前缀块
 * @param flags 块标志位
 * @details 写入 4 字节占位符 (0)，记录当前位置
 */
void AltiumBinaryWriter::beginBlock(uint8_t flags) {
    m_blockPositions.append(m_buffer.size());  // 保存当前块头位置。
    // 写入占位符
    m_buffer.append(static_cast<char>(flags));
    m_buffer.append('\0');
    m_buffer.append('\0');
    m_buffer.append('\0');
}

/**
 * @brief 结束大小前缀块，回填块大小
 */
void AltiumBinaryWriter::endBlock() {
    if (m_blockPositions.isEmpty()) {
        return;
    }

    int blockStart = m_blockPositions.takeLast();  // 当前块头起始偏移。
    int payloadSize = m_buffer.size() - blockStart - 4;  // 块载荷字节数。
    uint8_t flags = static_cast<uint8_t>(m_buffer[blockStart]);  // 块标志位。

    // 回填大小：(flags << 24) | payload_size
    uint32_t sizeHeader = (static_cast<uint32_t>(flags) << 24) | (static_cast<uint32_t>(payloadSize) & 0x00FFFFFF);
    m_buffer[blockStart] = static_cast<char>(sizeHeader & 0xFF);
    m_buffer[blockStart + 1] = static_cast<char>((sizeHeader >> 8) & 0xFF);
    m_buffer[blockStart + 2] = static_cast<char>((sizeHeader >> 16) & 0xFF);
    m_buffer[blockStart + 3] = static_cast<char>((sizeHeader >> 24) & 0xFF);
}

/**
 * @brief 写入 Pascal 短字符串
 * @param str 待写入的字符串
 */
void AltiumBinaryWriter::writePascalShortString(const QString& str) {
    QByteArray encoded = str.toLatin1();  // Latin-1 编码后的字符串。
    uint8_t len = static_cast<uint8_t>(qMin(encoded.size(), 255));  // 截断后的长度。
    writeUInt8(len);
    m_buffer.append(encoded.left(len));
}

/**
 * @brief 写入字符串块
 * @param str 待写入的字符串
 */
void AltiumBinaryWriter::writeStringBlock(const QString& str) {
    QByteArray encoded = str.toLatin1();  // Latin-1 编码后的字符串。
    uint8_t strLen = static_cast<uint8_t>(qMin(encoded.size(), 255));  // 字符串长度。
    uint32_t blockSize = 1 + strLen;  // 字符串块总长度。
    writeUInt32(blockSize);
    writeUInt8(strLen);
    m_buffer.append(encoded.left(strLen));
}

/**
 * @brief 写入带长度块和终止字节的 Pascal 字符串
 * @param str 待写入的字符串
 */
void AltiumBinaryWriter::writePascalString(const QString& str) {
    beginBlock();
    writePascalShortString(str);
    writeUInt8(0);
    endBlock();
}

/**
 * @brief 写入 C 字符串参数块
 * @param params 参数键值表
 */
void AltiumBinaryWriter::writeCStringParameterBlock(const QMap<QString, QString>& params) {
    QString paramStr;  // 拼接后的参数字符串。
    QList<QString> orderedKeys;  // 按协议优先级排列的参数键。
    for (const QString& preferred : {QStringLiteral("HEADER"),
                                     QStringLiteral("RECORD"),
                                     QStringLiteral("PATTERN"),
                                     QStringLiteral("ISNOTACCESIBLE"),
                                     QStringLiteral("IndexInSheet"),
                                     QStringLiteral("OWNERPARTID"),
                                     QStringLiteral("OwnerPartId")}) {
        if (params.contains(preferred))
            orderedKeys.append(preferred);
    }
    for (auto it = params.constBegin(); it != params.constEnd(); ++it) {
        if (!orderedKeys.contains(it.key()))
            orderedKeys.append(it.key());
    }
    for (const QString& key : orderedKeys) {
        auto it = params.constFind(key);  // 当前参数迭代器。
        if (it == params.constEnd())
            continue;
        QString value = it.value();  // 当前字段对应的文本值。
        value.replace('|', ' ');
        value.replace(QChar::Null, ' ');
        paramStr += "|" + it.key() + "=" + value;
    }
    paramStr += "|";
    writeCStringParameterBlockRaw(paramStr);
}

/**
 * @brief 写入 C 字符串参数块（自动处理非 ASCII 字符）
 * @param params 参数键值表
 */
void AltiumBinaryWriter::writeCStringParameterBlockUtf8(const QMap<QString, QString>& params) {
    QByteArray encoded;  // 已编码的参数字节。
    // Altium 的参数块虽然语义上是键值集合，但记录识别器会读取首个
    // 参数来判断记录类型。QMap 的字典序会把 ALLPINCOUNT/AREACOLOR
    // 放在 RECORD/HEADER 前面，从而使合法数据被当作未知记录。保持少量
    // 协议首字段的稳定顺序，其余字段仍使用 QMap 的确定性顺序。
    QList<QString> orderedKeys;
    for (const QString& preferred : {QStringLiteral("HEADER"),
                                     QStringLiteral("RECORD"),
                                     QStringLiteral("PATTERN"),
                                     QStringLiteral("ISNOTACCESIBLE"),
                                     QStringLiteral("IndexInSheet"),
                                     QStringLiteral("OWNERPARTID"),
                                     QStringLiteral("OwnerPartId")}) {
        if (params.contains(preferred))
            orderedKeys.append(preferred);
    }
    for (auto it = params.constBegin(); it != params.constEnd(); ++it) {
        if (!orderedKeys.contains(it.key()))
            orderedKeys.append(it.key());
    }

    for (const QString& keyName : orderedKeys) {
        auto it = params.constFind(keyName);  // 当前参数迭代器。
        if (it == params.constEnd())
            continue;
        const QByteArray ansiKey = it.key().toLatin1();  // ANSI 参数键。
        const QByteArray utf8Key = it.key().toUtf8();  // UTF-8 参数键。
        QString value = it.value();  // 当前字段对应的文本值。
        value.replace('|', ' ');
        value.replace(QChar::Null, ' ');
        const QByteArray ansiValue = value.toLatin1();  // 当前字段转换后的 ANSI 字节序列。
        encoded += '|';
        encoded += ansiKey;
        encoded += '=';
        encoded += ansiValue;

        // %UTF8% 参数的键和值都必须使用原始 UTF-8 字节，而不是再次 Latin-1 转码。
        if (QString::fromLatin1(ansiValue) != value) {
            encoded += "|%UTF8%";
            encoded += utf8Key;
            encoded += '=';
            encoded += value.toUtf8();
        }
    }
    encoded += '|';
    writeCStringParameterBlockRaw(encoded);
}

/**
 * @brief 写入原始 C 字符串参数块
 * @param paramString 参数字符串
 */
void AltiumBinaryWriter::writeCStringParameterBlockRaw(const QString& paramString) {
    writeCStringParameterBlockRaw(paramString.toLatin1());
}

/**
 * @brief 写入已经编码好的 C 字符串参数块，并追加 NUL 终止字节
 * @param parameterBytes 已编码的参数字节
 */
void AltiumBinaryWriter::writeCStringParameterBlockRaw(const QByteArray& parameterBytes) {
    QByteArray encoded = parameterBytes;  // 带 NUL 终止符的参数字节。
    encoded.append('\0');  // null 终止符
    uint32_t size = static_cast<uint32_t>(encoded.size());  // 参数块字节数。
    writeUInt32(size);
    m_buffer.append(encoded);
}

/**
 * @brief 写入坐标点
 * @param x X 坐标
 * @param y Y 坐标
 */
void AltiumBinaryWriter::writeCoordPoint(int32_t x, int32_t y) {
    writeInt32(x);
    writeInt32(y);
}

/**
 * @brief 写入坐标值
 * @param value 坐标值
 */
void AltiumBinaryWriter::writeCoord(int32_t value) {
    writeInt32(value);
}

/**
 * @brief 获取当前写入位置
 */
int AltiumBinaryWriter::position() const {
    return m_buffer.size();
}

}  // namespace EasyKiConverter
