#include "ArchiveInspector.h"

#include <QSet>

namespace EasyKiConverter::Parser {
namespace {

constexpr quint32 kEndOfCentralDirectory = 0x06054B50;  ///< ZIP 中央目录结束记录签名。
constexpr quint32 kCentralDirectoryEntry = 0x02014B50;  ///< ZIP 中央目录条目签名。
constexpr quint32 kLocalFileEntry = 0x04034B50;  ///< ZIP 本地文件记录签名。
constexpr quint16 kDeflate = 8;  ///< ZIP Deflate 压缩方法编号。
constexpr quint16 kEncryptedFlag = 0x0001;  ///< ZIP 加密标志位。
constexpr qsizetype kEndOfCentralDirectoryMinSize = 22;  ///< ZIP 中央目录结束记录的最小大小（字节）。
constexpr qsizetype kMaxEndOfCentralDirectorySearch = 65557;  ///< ZIP 尾部中央目录搜索上限（字节）。

/**
 * @brief 按 ZIP 小端字节序读取 16 位字段。
 * @param data ZIP 数据缓冲区。
 * @param offset 字段在缓冲区中的偏移。
 * @return 读取到的 16 位无符号值。
 */
quint16 read16(const QByteArray& data, qsizetype offset) {
    return static_cast<quint16>(static_cast<unsigned char>(data.at(offset))) |
           static_cast<quint16>(static_cast<unsigned char>(data.at(offset + 1))) << 8;
}

/**
 * @brief 按 ZIP 小端字节序读取 32 位字段。
 * @param data ZIP 数据缓冲区。
 * @param offset 字段在缓冲区中的偏移。
 * @return 读取到的 32 位无符号值。
 */
quint32 read32(const QByteArray& data, qsizetype offset) {
    return static_cast<quint32>(static_cast<unsigned char>(data.at(offset))) |
           static_cast<quint32>(static_cast<unsigned char>(data.at(offset + 1))) << 8 |
           static_cast<quint32>(static_cast<unsigned char>(data.at(offset + 2))) << 16 |
           static_cast<quint32>(static_cast<unsigned char>(data.at(offset + 3))) << 24;
}

/**
 * @brief 检查归档字段是否完全落在输入缓冲区内。
 * @param data 输入缓冲区。
 * @param offset 字段起始偏移。
 * @param size 字段长度。
 * @return 字段完全位于缓冲区内时返回 true。
 */
bool hasBytes(const QByteArray& data, qsizetype offset, qsizetype size) {
    return offset >= 0 && size >= 0 && offset <= data.size() - size;
}

/**
 * @brief 在 ZIP 尾部安全范围内查找中央目录结束标记。
 * @param data ZIP 数据缓冲区。
 * @return 标记偏移，未找到时返回 -1。
 */
qsizetype findEndOfCentralDirectory(const QByteArray& data) {
    if (data.size() < kEndOfCentralDirectoryMinSize)
        return -1;
    const qsizetype begin = qMax<qsizetype>(0, data.size() - kMaxEndOfCentralDirectorySearch);  ///< 中央目录搜索起点。
    for (qsizetype offset = data.size() - kEndOfCentralDirectoryMinSize; offset >= begin; --offset) {
        if (read32(data, offset) == kEndOfCentralDirectory)
            return offset;
    }
    return -1;
}

/**
 * @brief 判断归档条目是否包含绝对路径或路径穿越片段。
 * @param path 归档条目路径。
 * @return 路径不安全时返回 true。
 */
bool isUnsafePath(const QString& path) {
    QString normalized = path;  ///< 统一分隔符后的归档路径。
    normalized.replace(QChar('\\'), QChar('/'));
    if (normalized.isEmpty() || normalized.contains(QChar('\0')) || normalized.startsWith(QChar('/')))
        return true;
    if (normalized.size() >= 2 && normalized.at(1) == QChar(':'))
        return true;
    const QStringList parts = normalized.split(QChar('/'), Qt::KeepEmptyParts);  ///< 路径分量列表。
    for (const QString& part : parts) {
        if (part == QStringLiteral(".."))
            return true;
    }
    return false;
}

/**
 * @brief 根据 Unix 外部属性识别符号链接条目。
 * @param externalAttributes ZIP 条目的 Unix 外部属性。
 * @return 条目为符号链接时返回 true。
 */
bool isUnixSymlink(quint32 externalAttributes) {
    return ((externalAttributes >> 16) & 0xF000U) == 0xA000U;
}

}  // namespace

/**
 * @brief 读取 ZIP 中央目录并执行路径、大小和压缩方式校验。
 * @param data ZIP 数据缓冲区。
 * @param limits 归档安全限制。
 * @param filePath 待检查文件的显示路径。
 * @return 归档检查结果。
 */
ArchiveInspectionResult ArchiveInspector::inspect(const QByteArray& data,
                                                  const ArchiveLimits& limits,
                                                  const QString& filePath) {
    ArchiveInspectionResult result;  ///< 归档检查结果。
    result.diagnostics.setFilePath(filePath);
    const qsizetype eocd = findEndOfCentralDirectory(data);  ///< 中央目录结束记录偏移。
    if (eocd < 0 || !hasBytes(data, eocd, kEndOfCentralDirectoryMinSize)) {
        result.diagnostics.add(ParseSeverity::Error, ParseScope::File, QStringLiteral("ZIP 缺少中央目录结束标记"));
        return result;
    }
    const quint16 disk = read16(data, eocd + 4);  ///< 当前磁盘编号。
    const quint16 centralDirectoryDisk = read16(data, eocd + 6);  ///< 中央目录所在磁盘编号。
    const quint16 entryCount = read16(data, eocd + 10);  ///< 中央目录条目数量。
    const quint32 centralDirectorySize = read32(data, eocd + 12);  ///< 中央目录字节数。
    const quint32 centralDirectoryOffset = read32(data, eocd + 16);  ///< 中央目录起始偏移。
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
    QSet<QString> paths;  ///< 已发现的归档路径集合。
    qsizetype cursor = static_cast<qsizetype>(centralDirectoryOffset);  ///< 当前中央目录读取偏移。
    const qsizetype centralDirectoryEnd =
        cursor + static_cast<qsizetype>(centralDirectorySize);  ///< 中央目录结束偏移。
    for (quint16 index = 0; index < entryCount; ++index) {
        if (cursor > centralDirectoryEnd || centralDirectoryEnd - cursor < 46 ||
            read32(data, cursor) != kCentralDirectoryEntry) {
            result.diagnostics.add(ParseSeverity::Error, ParseScope::File, QStringLiteral("ZIP 中央目录条目损坏"));
            return result;
        }
        const quint16 flags = read16(data, cursor + 8);  ///< 条目通用标志位。
        const quint16 method = read16(data, cursor + 10);  ///< 条目压缩方法。
        const quint32 compressedSize = read32(data, cursor + 20);  ///< 压缩数据大小。
        const quint32 uncompressedSize = read32(data, cursor + 24);  ///< 解压后数据大小。
        const quint16 nameLength = read16(data, cursor + 28);  ///< 条目名称长度。
        const quint16 extraLength = read16(data, cursor + 30);  ///< 扩展字段长度。
        const quint16 commentLength = read16(data, cursor + 32);  ///< 条目注释长度。
        const quint32 externalAttributes = read32(data, cursor + 38);  ///< 条目外部属性。
        const quint32 localHeaderOffset = read32(data, cursor + 42);  ///< 本地文件头偏移。
        const qsizetype recordSize = 46 + nameLength + extraLength + commentLength;  ///< 中央目录记录长度。
        if (recordSize > centralDirectoryEnd - cursor) {
            result.diagnostics.add(ParseSeverity::Error, ParseScope::File, QStringLiteral("ZIP 条目长度超出中央目录"));
            return result;
        }
        const QString path = QString::fromUtf8(data.constData() + cursor + 46, nameLength);  ///< 中央目录中的条目路径。
        if (localHeaderOffset > static_cast<quint32>(data.size()) ||
            !hasBytes(data, static_cast<qsizetype>(localHeaderOffset), 30) ||
            read32(data, static_cast<qsizetype>(localHeaderOffset)) != kLocalFileEntry) {
            result.diagnostics.add(
                ParseSeverity::Error, ParseScope::File, QStringLiteral("ZIP 条目本地文件头无效"), path);
            return result;
        }
        const qsizetype localOffset = static_cast<qsizetype>(localHeaderOffset);  ///< 本地文件头读取偏移。
        const quint16 localNameLength = read16(data, localOffset + 26);  ///< 本地文件头名称长度。
        const quint16 localExtraLength = read16(data, localOffset + 28);  ///< 本地文件头扩展字段长度。
        const qsizetype localRecordSize = 30 + localNameLength + localExtraLength;  ///< 本地文件记录长度。
        if (!hasBytes(data, localOffset, localRecordSize)) {
            result.diagnostics.add(
                ParseSeverity::Error, ParseScope::File, QStringLiteral("ZIP 条目本地文件头范围无效"), path);
            return result;
        }
        const QString localPath =
            QString::fromUtf8(data.constData() + localOffset + 30, localNameLength);  ///< 本地文件头中的路径。
        if (localPath != path) {
            result.diagnostics.add(
                ParseSeverity::Error, ParseScope::File, QStringLiteral("ZIP 条目名称与本地文件头不一致"), path);
            return result;
        }
        const qsizetype dataOffset = localOffset + localRecordSize;  ///< 条目压缩数据起始偏移。
        if (!hasBytes(data, dataOffset, compressedSize)) {
            result.diagnostics.add(
                ParseSeverity::Error, ParseScope::File, QStringLiteral("ZIP 条目压缩数据范围无效"), path);
            return result;
        }
        const bool directory = path.endsWith(QChar('/'));  ///< 条目是否表示目录。
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
