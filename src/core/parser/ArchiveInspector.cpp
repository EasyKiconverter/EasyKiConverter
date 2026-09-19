#include "ArchiveInspector.h"

#include <QSet>

namespace EasyKiConverter::Parser {
namespace {

constexpr quint32 kEndOfCentralDirectory = 0x06054B50;
constexpr quint32 kCentralDirectoryEntry = 0x02014B50;
constexpr quint16 kDeflate = 8;
constexpr quint16 kEncryptedFlag = 0x0001;
constexpr qsizetype kEndOfCentralDirectoryMinSize = 22;
constexpr qsizetype kMaxEndOfCentralDirectorySearch = 65557;

/** @brief 按 ZIP 小端字节序读取 16 位字段。 */
quint16 read16(const QByteArray& data, qsizetype offset) {
    return static_cast<quint16>(static_cast<unsigned char>(data.at(offset))) |
           static_cast<quint16>(static_cast<unsigned char>(data.at(offset + 1))) << 8;
}

/** @brief 按 ZIP 小端字节序读取 32 位字段。 */
quint32 read32(const QByteArray& data, qsizetype offset) {
    return static_cast<quint32>(static_cast<unsigned char>(data.at(offset))) |
           static_cast<quint32>(static_cast<unsigned char>(data.at(offset + 1))) << 8 |
           static_cast<quint32>(static_cast<unsigned char>(data.at(offset + 2))) << 16 |
           static_cast<quint32>(static_cast<unsigned char>(data.at(offset + 3))) << 24;
}

/** @brief 检查归档字段是否完全落在输入缓冲区内。 */
bool hasBytes(const QByteArray& data, qsizetype offset, qsizetype size) {
    return offset >= 0 && size >= 0 && offset <= data.size() - size;
}

/** @brief 在 ZIP 尾部安全范围内查找中央目录结束标记。 */
qsizetype findEndOfCentralDirectory(const QByteArray& data) {
    if (data.size() < kEndOfCentralDirectoryMinSize)
        return -1;
    const qsizetype begin = qMax<qsizetype>(0, data.size() - kMaxEndOfCentralDirectorySearch);
    for (qsizetype offset = data.size() - kEndOfCentralDirectoryMinSize; offset >= begin; --offset) {
        if (read32(data, offset) == kEndOfCentralDirectory)
            return offset;
    }
    return -1;
}

/** @brief 判断归档条目是否包含绝对路径或路径穿越片段。 */
bool isUnsafePath(const QString& path) {
    QString normalized = path;
    normalized.replace(QChar('\\'), QChar('/'));
    if (normalized.isEmpty() || normalized.contains(QChar('\0')) || normalized.startsWith(QChar('/')))
        return true;
    if (normalized.size() >= 2 && normalized.at(1) == QChar(':'))
        return true;
    const QStringList parts = normalized.split(QChar('/'), Qt::KeepEmptyParts);
    for (const QString& part : parts) {
        if (part == QStringLiteral(".."))
            return true;
    }
    return false;
}

/** @brief 根据 Unix 外部属性识别符号链接条目。 */
bool isUnixSymlink(quint32 externalAttributes) {
    return ((externalAttributes >> 16) & 0xF000U) == 0xA000U;
}

}  // namespace

/** @brief 读取 ZIP 中央目录并执行路径、大小和压缩方式校验。 */
ArchiveInspectionResult ArchiveInspector::inspect(const QByteArray& data,
                                                  const ArchiveLimits& limits,
                                                  const QString& filePath) {
    ArchiveInspectionResult result;
    result.diagnostics.setFilePath(filePath);
    const qsizetype eocd = findEndOfCentralDirectory(data);
    if (eocd < 0 || !hasBytes(data, eocd, kEndOfCentralDirectoryMinSize)) {
        result.diagnostics.add(ParseSeverity::Error, ParseScope::File, QStringLiteral("ZIP 缺少中央目录结束标记"));
        return result;
    }
    const quint16 disk = read16(data, eocd + 4);
    const quint16 centralDirectoryDisk = read16(data, eocd + 6);
    const quint16 entryCount = read16(data, eocd + 10);
    const quint32 centralDirectorySize = read32(data, eocd + 12);
    const quint32 centralDirectoryOffset = read32(data, eocd + 16);
    if (disk != 0 || centralDirectoryDisk != 0 || entryCount == 0xFFFF || centralDirectorySize == 0xFFFFFFFF ||
        centralDirectoryOffset == 0xFFFFFFFF) {
        result.diagnostics.add(ParseSeverity::Error, ParseScope::File, QStringLiteral("不支持多磁盘 ZIP 归档"));
        return result;
    }
    if (entryCount > limits.maxEntries) {
        result.diagnostics.add(ParseSeverity::Error, ParseScope::File, QStringLiteral("ZIP 条目数量超过安全限制"));
        return result;
    }
    if (centralDirectoryOffset > static_cast<quint32>(data.size()) ||
        centralDirectorySize > static_cast<quint32>(data.size() - centralDirectoryOffset) ||
        !hasBytes(data, centralDirectoryOffset, centralDirectorySize)) {
        result.diagnostics.add(ParseSeverity::Error, ParseScope::File, QStringLiteral("ZIP 中央目录范围无效"));
        return result;
    }
    QSet<QString> paths;
    qsizetype cursor = static_cast<qsizetype>(centralDirectoryOffset);
    const qsizetype centralDirectoryEnd = cursor + static_cast<qsizetype>(centralDirectorySize);
    for (quint16 index = 0; index < entryCount; ++index) {
        if (cursor > centralDirectoryEnd || centralDirectoryEnd - cursor < 46 ||
            read32(data, cursor) != kCentralDirectoryEntry) {
            result.diagnostics.add(ParseSeverity::Error, ParseScope::File, QStringLiteral("ZIP 中央目录条目损坏"));
            return result;
        }
        const quint16 flags = read16(data, cursor + 8);
        const quint16 method = read16(data, cursor + 10);
        const quint32 compressedSize = read32(data, cursor + 20);
        const quint32 uncompressedSize = read32(data, cursor + 24);
        const quint16 nameLength = read16(data, cursor + 28);
        const quint16 extraLength = read16(data, cursor + 30);
        const quint16 commentLength = read16(data, cursor + 32);
        const quint32 externalAttributes = read32(data, cursor + 38);
        const qsizetype recordSize = 46 + nameLength + extraLength + commentLength;
        if (recordSize > centralDirectoryEnd - cursor) {
            result.diagnostics.add(ParseSeverity::Error, ParseScope::File, QStringLiteral("ZIP 条目长度超出中央目录"));
            return result;
        }
        const QString path = QString::fromUtf8(data.constData() + cursor + 46, nameLength);
        const bool directory = path.endsWith(QChar('/'));
        if (isUnsafePath(path) || isUnixSymlink(externalAttributes)) {
            result.diagnostics.add(ParseSeverity::Error, ParseScope::File, QStringLiteral("ZIP 条目路径不安全"), path);
            return result;
        }
        if (paths.contains(path)) {
            result.diagnostics.add(ParseSeverity::Error, ParseScope::File, QStringLiteral("ZIP 条目路径重复"), path);
            return result;
        }
        paths.insert(path);
        if ((flags & kEncryptedFlag) != 0 || (method != 0 && method != kDeflate)) {
            result.diagnostics.add(
                ParseSeverity::Error, ParseScope::File, QStringLiteral("ZIP 条目压缩或加密方式不受支持"), path);
            return result;
        }
        if (static_cast<qint64>(uncompressedSize) > limits.maxEntryUncompressedBytes) {
            result.diagnostics.add(
                ParseSeverity::Error, ParseScope::File, QStringLiteral("ZIP 单个条目展开后超过大小限制"), path);
            return result;
        }
        if (result.totalUncompressedBytes > limits.maxTotalUncompressedBytes - uncompressedSize) {
            result.diagnostics.add(
                ParseSeverity::Error, ParseScope::File, QStringLiteral("ZIP 总展开大小超过限制"), path);
            return result;
        }
        if (compressedSize == 0 && uncompressedSize > 0) {
            result.diagnostics.add(
                ParseSeverity::Error, ParseScope::File, QStringLiteral("ZIP 条目压缩大小无效"), path);
            return result;
        }
        if (compressedSize > 0 &&
            static_cast<double>(uncompressedSize) / static_cast<double>(compressedSize) > limits.maxCompressionRatio) {
            result.diagnostics.add(
                ParseSeverity::Error, ParseScope::File, QStringLiteral("ZIP 压缩比超过安全限制"), path);
            return result;
        }
        result.totalUncompressedBytes += uncompressedSize;
        result.entries.append({path, compressedSize, uncompressedSize, method, directory});
        cursor += recordSize;
    }
    result.safe = true;
    return result;
}

}  // namespace EasyKiConverter::Parser
