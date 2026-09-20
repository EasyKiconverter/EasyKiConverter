#include "EncodingDetector.h"

namespace EasyKiConverter::Parser {
namespace {

/** @brief 严格验证 UTF-8 多字节序列，拒绝过长和截断序列。 */
bool isValidUtf8(const QByteArray& data) {
    int remaining = 0;
    for (unsigned char byte : data) {
        if (remaining == 0) {
            if (byte <= 0x7F)
                continue;
            if (byte >= 0xC2 && byte <= 0xDF)
                remaining = 1;
            else if (byte >= 0xE0 && byte <= 0xEF)
                remaining = 2;
            else if (byte >= 0xF0 && byte <= 0xF4)
                remaining = 3;
            else
                return false;
        } else {
            if (byte < 0x80 || byte > 0xBF)
                return false;
            --remaining;
        }
    }
    return remaining == 0;
}

/** @brief 按 BOM 已确认的字节序解码 UTF-16 文本。 */
QString decodeUtf16(const QByteArray& data, bool littleEndian) {
    const int byteCount = data.size() - 2;
    if (byteCount < 0 || byteCount % 2 != 0)
        return {};
    QString result;
    result.reserve(byteCount / 2);
    for (int index = 2; index < data.size(); index += 2) {
        const quint16 first = static_cast<unsigned char>(data.at(index));
        const quint16 second = static_cast<unsigned char>(data.at(index + 1));
        const quint16 value = littleEndian ? first | (second << 8) : (first << 8) | second;
        result.append(QChar(value));
    }
    return result;
}

}  // namespace

/** @brief 根据 BOM 和 UTF-8 合法性判断文本编码。 */
EncodingDetectionResult EncodingDetector::detect(const QByteArray& data) {
    if (data.startsWith(QByteArray::fromHex("EFBBBF")))
        return {TextEncoding::Utf8, true, 100};
    if (data.startsWith(QByteArray::fromHex("FFFE")))
        return {TextEncoding::Utf16LittleEndian, data.size() >= 2 && (data.size() - 2) % 2 == 0, 100};
    if (data.startsWith(QByteArray::fromHex("FEFF")))
        return {TextEncoding::Utf16BigEndian, data.size() >= 2 && (data.size() - 2) % 2 == 0, 100};
    if (isValidUtf8(data))
        return {TextEncoding::Utf8, true, 90};
    if (!data.isEmpty())
        return {TextEncoding::Latin1, true, 30};
    return {TextEncoding::Utf8, true, 90};
}

/** @brief 按检测结果解码文本并记录回退或错误诊断。 */
QString EncodingDetector::decode(const QByteArray& data, ParseDiagnostics* diagnostics, const QString& filePath) {
    if (diagnostics)
        diagnostics->setFilePath(filePath);
    const EncodingDetectionResult result = detect(data);
    if (!result.valid) {
        if (diagnostics)
            diagnostics->add(ParseSeverity::Error, ParseScope::File, QStringLiteral("文本编码标记不完整"));
        return {};
    }
    // 根据已确认的编码分支解码，未知编码不会静默转换。
    switch (result.encoding) {
        case TextEncoding::Utf8: {
            const int offset = data.startsWith(QByteArray::fromHex("EFBBBF")) ? 3 : 0;
            return QString::fromUtf8(data.constData() + offset, data.size() - offset);
        }
        case TextEncoding::Utf16LittleEndian:
            return decodeUtf16(data, true);
        case TextEncoding::Utf16BigEndian:
            return decodeUtf16(data, false);
        case TextEncoding::Latin1:
            if (diagnostics)
                diagnostics->add(ParseSeverity::Warning,
                                 ParseScope::File,
                                 QStringLiteral("无法确认文本编码，已按 Latin-1 回退解码"));
            return QString::fromLatin1(data);
        case TextEncoding::Unknown:
            break;
    }
    if (diagnostics)
        diagnostics->add(ParseSeverity::Error, ParseScope::File, QStringLiteral("不支持的文本编码"));
    return {};
}

}  // namespace EasyKiConverter::Parser
