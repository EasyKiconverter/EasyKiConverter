#include "OLECompoundReader.h"

#include <QFile>
#include <QSet>

#include <algorithm>
#include <cstring>
#include <functional>
#include <limits>

namespace EasyKiConverter {

namespace {

constexpr quint32 kEndOfChain = 0xFFFFFFFEU;  ///< OLE FAT 链结束标记。
constexpr quint32 kFreeSector = 0xFFFFFFFFU;  ///< OLE 空闲扇区标记。
constexpr quint32 kFatSector = 0xFFFFFFFDU;  ///< OLE FAT 扇区标记。
constexpr quint32 kDifSector = 0xFFFFFFFCU;  ///< OLE DIFAT 扇区标记。
constexpr quint32 kNoStream = 0xFFFFFFFFU;  ///< OLE 无子流标记。
constexpr quint32 kSectorSize = 512;  ///< OLE 普通扇区大小（字节）。
constexpr quint32 kMiniSectorSize = 64;  ///< OLE Mini 扇区大小（字节）。
constexpr quint32 kMiniStreamCutoff = 4096;  ///< OLE 使用 Mini Stream 的大小阈值（字节）。
constexpr int kDirectoryEntrySize = 128;  ///< OLE 目录条目大小（字节）。
constexpr int kMaxDirectoryDepth = 4096;  ///< OLE 目录树递归深度上限。

/**
 * @brief 从 OLE 缓冲区按小端序读取无符号十六位字段。
 * @param data OLE 原始缓冲区。
 * @param offset 字段起始偏移。
 * @return 读取到的无符号十六位数值。
 */
quint16 readU16(const QByteArray& data, qsizetype offset) {
    return static_cast<quint16>(static_cast<unsigned char>(data.at(offset))) |
           (static_cast<quint16>(static_cast<unsigned char>(data.at(offset + 1))) << 8);
}

/**
 * @brief 从 OLE 缓冲区按小端序读取无符号三十二位字段。
 * @param data OLE 原始缓冲区。
 * @param offset 字段起始偏移。
 * @return 读取到的无符号三十二位数值。
 */
quint32 readU32(const QByteArray& data, qsizetype offset) {
    return static_cast<quint32>(static_cast<unsigned char>(data.at(offset))) |
           (static_cast<quint32>(static_cast<unsigned char>(data.at(offset + 1))) << 8) |
           (static_cast<quint32>(static_cast<unsigned char>(data.at(offset + 2))) << 16) |
           (static_cast<quint32>(static_cast<unsigned char>(data.at(offset + 3))) << 24);
}

/**
 * @brief 从 OLE 缓冲区按小端序读取无符号六十四位字段。
 * @param data OLE 原始缓冲区。
 * @param offset 字段起始偏移。
 * @return 读取到的无符号六十四位数值。
 */
quint64 readU64(const QByteArray& data, qsizetype offset) {
    quint64 value = 0;  ///< 值。
    for (int i = 0; i < 8; ++i)
        value |= static_cast<quint64>(static_cast<unsigned char>(data.at(offset + i))) << (i * 8);
    return value;
}

struct DirectoryEntry {
    quint8 type = 0;  ///< 类型。
    quint32 left = kNoStream;  ///< 左子节点索引。
    quint32 right = kNoStream;  ///< 右子节点索引。
    quint32 child = kNoStream;  ///< 子节点索引。
    quint32 startSector = kEndOfChain;  ///< 起始扇区。
    quint64 streamSize = 0;  ///< 流大小。
    QString name;  ///< 名称。
};

}  // namespace

OLECompoundReader::OLECompoundReader() = default;

OLECompoundReader::~OLECompoundReader() = default;

/**
 * @brief 清除上一次打开操作产生的状态和流索引。
 */
void OLECompoundReader::clear() {
    m_opened = false;
    m_errorMessage.clear();
    m_streams.clear();
}

/**
 * @brief 记录解析失败并将部分构建结果回滚到关闭状态。
 * @param message 解析失败原因。
 * @return 始终返回 false，便于调用方直接返回失败。
 */
bool OLECompoundReader::fail(const QString& message) {
    clear();
    m_errorMessage = message;
    return false;
}

/**
 * @brief 打开并校验 OLE 容器，然后建立可按路径读取的流索引。
 * @param filePath OLE 文件路径。
 * @return 文件有效且流索引建立成功时返回 true。
 */
bool OLECompoundReader::open(const QString& filePath) {
    clear();

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
        return fail(QStringLiteral("无法打开 OLE 文件: %1").arg(file.errorString()));
    const QByteArray fileData = file.readAll();  ///< 文件数据。
    if (fileData.size() < static_cast<qsizetype>(kSectorSize * 2))
        return fail(QStringLiteral("OLE 文件大小不足"));

    static constexpr unsigned char signature[] = {0xD0, 0xCF, 0x11, 0xE0, 0xA1, 0xB1, 0x1A, 0xE1};
    if (memcmp(fileData.constData(), signature, sizeof(signature)) != 0)
        return fail(QStringLiteral("不是有效的 OLE 复合文档"));
    if (readU16(fileData, 26) != 3 || readU16(fileData, 28) != 0xFFFE || readU16(fileData, 30) != 9 ||
        readU16(fileData, 32) != 6)
        return fail(QStringLiteral("仅支持 OLE V3、512 字节扇区和 64 字节迷你扇区"));

    const quint32 sectorCount = static_cast<quint32>((fileData.size() - kSectorSize) / kSectorSize);  ///< 扇区数量。
    if (sectorCount == 0 || static_cast<qsizetype>(sectorCount + 1) * kSectorSize != fileData.size())
        return fail(QStringLiteral("OLE 文件包含不完整的扇区"));

    auto sector = [&](quint32 index, QByteArray* result) {
        if (index >= sectorCount)
            return false;
        *result = fileData.mid(static_cast<qsizetype>(index + 1) * kSectorSize, kSectorSize);
        return result->size() == static_cast<int>(kSectorSize);
    };

    const quint32 firstDirectorySector = readU32(fileData, 48);  ///< 首个目录扇区。
    const quint32 firstMiniFatSector = readU32(fileData, 60);  ///< 首个 Mini-FAT 扇区。
    const quint32 miniFatSectorCount = readU32(fileData, 64);  ///< Mini-FAT 扇区数量。
    const quint32 firstDifatSector = readU32(fileData, 68);  ///< 首个 DIFAT 扇区。
    const quint32 difatSectorCount = readU32(fileData, 72);  ///< DIFAT 扇区数量。
    const quint32 fatSectorCount = readU32(fileData, 44);  ///< FAT 扇区数量。

    QVector<quint32> fatSectors;  ///< FAT 扇区列表。
    for (int i = 0; i < 109; ++i) {
        const quint32 value = readU32(fileData, 76 + i * 4);  ///< 值。
        if (value != kFreeSector)
            fatSectors.append(value);
    }
    quint32 difatSector = firstDifatSector;  ///< DIFAT 扇区。
    QSet<quint32> visitedDifat;  ///< FAT 表。
    for (quint32 i = 0; i < difatSectorCount && difatSector != kEndOfChain; ++i) {
        if (difatSector >= sectorCount || visitedDifat.contains(difatSector))
            return fail(QStringLiteral("DIFAT 链无效"));
        visitedDifat.insert(difatSector);
        QByteArray data;  ///< 数据。
        if (!sector(difatSector, &data))
            return fail(QStringLiteral("无法读取 DIFAT 扇区"));
        for (int j = 0; j < 127; ++j) {
            const quint32 value = readU32(data, j * 4);  ///< 值。
            if (value != kFreeSector)
                fatSectors.append(value);
        }
        difatSector = readU32(data, 508);
    }
    if (fatSectors.size() < static_cast<int>(fatSectorCount))
        return fail(QStringLiteral("FAT 扇区数量不足"));

    QVector<quint32> fat(static_cast<int>(sectorCount), kFreeSector);
    for (quint32 i = 0; i < fatSectorCount; ++i) {
        if (fatSectors.at(static_cast<int>(i)) >= sectorCount)
            return fail(QStringLiteral("FAT 扇区索引越界"));
        QByteArray data;  ///< 数据。
        sector(fatSectors.at(static_cast<int>(i)), &data);
        for (int j = 0; j < 128; ++j) {
            const quint32 index = i * 128U + static_cast<quint32>(j);  ///< 索引。
            if (index < sectorCount)
                fat[static_cast<int>(index)] = readU32(data, j * 4);
        }
    }

    auto readChain =
        [&](quint32 start, quint64 byteCount, const QVector<quint32>& table, quint32 unitSize, QByteArray* result) {
            result->clear();
            if (byteCount == 0)
                return true;
            if (byteCount > static_cast<quint64>(std::numeric_limits<int>::max()))
                return false;
            quint32 current = start;  ///< 当前值。
            QSet<quint32> visited;  ///< 访问集合。
            while (result->size() < static_cast<qsizetype>(byteCount)) {
                if (current >= static_cast<quint32>(table.size()) || current == kEndOfChain || current == kFreeSector ||
                    visited.contains(current))
                    return false;
                visited.insert(current);
                if (unitSize == kSectorSize) {
                    QByteArray data;  ///< 数据。
                    if (!sector(current, &data))
                        return false;
                    result->append(data);
                } else {
                    const qsizetype offset = static_cast<qsizetype>(current) * unitSize;  ///< 偏移量。
                    if (offset < 0 || offset + unitSize > result->capacity())
                        return false;
                }
                current = table.at(static_cast<int>(current));
                if (result->size() > static_cast<qsizetype>(byteCount) + unitSize)
                    break;
            }
            if (result->size() < static_cast<qsizetype>(byteCount))
                return false;
            *result = result->left(static_cast<int>(byteCount));
            return true;
        };

    QVector<quint32> miniFat;  ///< Mini-FAT 表。
    if (miniFatSectorCount > 0) {
        QByteArray miniFatData;  ///< Mini-FAT 原始数据。
        if (!readChain(firstMiniFatSector,
                       static_cast<quint64>(miniFatSectorCount) * kSectorSize,
                       fat,
                       kSectorSize,
                       &miniFatData))
            return fail(QStringLiteral("无法读取 Mini-FAT"));
        miniFat.reserve(miniFatData.size() / 4);
        for (qsizetype offset = 0; offset + 4 <= miniFatData.size(); offset += 4)
            miniFat.append(readU32(miniFatData, offset));
    }

    QByteArray directoryData;  ///< 目录数据。
    QSet<quint32> directorySectors;  ///< 目录扇区集合。
    quint32 currentDirectorySector = firstDirectorySector;  ///< 当前目录扇区。
    while (currentDirectorySector != kEndOfChain) {
        if (currentDirectorySector >= sectorCount || directorySectors.contains(currentDirectorySector))
            return fail(QStringLiteral("目录扇区链无效"));
        directorySectors.insert(currentDirectorySector);
        QByteArray data;  ///< 数据。
        sector(currentDirectorySector, &data);
        directoryData.append(data);
        currentDirectorySector = fat.at(static_cast<int>(currentDirectorySector));
    }
    if (directoryData.isEmpty() || directoryData.size() % kDirectoryEntrySize != 0)
        return fail(QStringLiteral("目录数据无效"));

    QVector<DirectoryEntry> entries;  ///< 目录条目列表。
    entries.reserve(directoryData.size() / kDirectoryEntrySize);
    for (qsizetype offset = 0; offset < directoryData.size(); offset += kDirectoryEntrySize) {
        DirectoryEntry entry;  ///< 当前目录条目。
        const quint16 nameSize = readU16(directoryData, offset + 64);  ///< 名称字段字节数。
        if (nameSize > 64 || nameSize % 2 != 0 || (nameSize != 0 && nameSize < 2))
            return fail(QStringLiteral("目录名称长度无效"));
        const int nameChars = nameSize == 0 ? 0 : nameSize / 2 - 1;  ///< 名称字符数。
        for (int i = 0; i < nameChars; ++i)
            entry.name.append(QChar(readU16(directoryData, offset + i * 2)));
        entry.type = static_cast<quint8>(directoryData.at(offset + 66));
        if (entry.type != 0 && entry.type != 1 && entry.type != 2 && entry.type != 5)
            return fail(QStringLiteral("目录对象类型无效"));
        entry.left = readU32(directoryData, offset + 68);
        entry.right = readU32(directoryData, offset + 72);
        entry.child = readU32(directoryData, offset + 76);
        entry.startSector = readU32(directoryData, offset + 116);
        entry.streamSize = readU64(directoryData, offset + 120);
        entries.append(entry);
    }
    if (entries.isEmpty() || entries.at(0).type != 5)
        return fail(QStringLiteral("OLE 根目录条目无效"));

    QByteArray miniStreamData;  ///< Mini-Stream 数据。
    if (entries.at(0).streamSize > 0 &&
        !readChain(entries.at(0).startSector, entries.at(0).streamSize, fat, kSectorSize, &miniStreamData))
        return fail(QStringLiteral("无法读取 Mini-Stream"));

    auto readMiniStream = [&](const DirectoryEntry& entry, QByteArray* result) {
        result->clear();
        if (entry.streamSize == 0)
            return true;
        if (entry.streamSize > static_cast<quint64>(std::numeric_limits<int>::max()) || miniFat.isEmpty())
            return false;
        quint32 current = entry.startSector;  ///< 当前值。
        QSet<quint32> visited;  ///< 访问集合。
        while (result->size() < static_cast<qsizetype>(entry.streamSize)) {
            if (current >= static_cast<quint32>(miniFat.size()) || visited.contains(current))
                return false;
            visited.insert(current);
            const qsizetype offset = static_cast<qsizetype>(current) * kMiniSectorSize;  ///< 偏移量。
            if (offset + kMiniSectorSize > miniStreamData.size())
                return false;
            result->append(miniStreamData.constData() + offset, kMiniSectorSize);
            current = miniFat.at(static_cast<int>(current));
            if (result->size() > static_cast<qsizetype>(entry.streamSize) + kMiniSectorSize)
                break;
        }
        *result = result->left(static_cast<int>(entry.streamSize));
        return result->size() == static_cast<int>(entry.streamSize);
    };

    QSet<quint32> visitedEntries;  ///< 已访问目录条目集合。
    std::function<bool(quint32, const QString&, int)> visitSiblings;
    visitSiblings = [&](quint32 index, const QString& parentPath, int depth) {
        if (index == kNoStream)
            return true;
        if (depth > kMaxDirectoryDepth || index >= static_cast<quint32>(entries.size()) ||
            visitedEntries.contains(index))
            return false;
        const DirectoryEntry& entry = entries.at(static_cast<int>(index));  ///< 当前目录条目。
        if (!visitSiblings(entry.left, parentPath, depth + 1))
            return false;
        if (entry.type != 1 && entry.type != 2)
            return false;
        const QString path = parentPath.isEmpty() ? entry.name : parentPath + "/" + entry.name;  ///< 路径。
        if (entry.type == 1) {
            if (!visitSiblings(entry.child, path, depth + 1))
                return false;
        } else {
            QByteArray streamData;  ///< 流数据。
            const bool regular = entry.streamSize >= kMiniStreamCutoff;  ///< 是否为普通流。
            // 校验结果。
            const bool valid = regular ? readChain(entry.startSector, entry.streamSize, fat, kSectorSize, &streamData)
                                       : readMiniStream(entry, &streamData);
            if (!valid || m_streams.contains(path))
                return false;
            m_streams.insert(path, streamData);
        }
        visitedEntries.insert(index);
        return visitSiblings(entry.right, parentPath, depth + 1);
    };
    if (!visitSiblings(entries.at(0).child, QString(), 0))
        return fail(QStringLiteral("OLE 目录树无效"));

    m_opened = true;
    return true;
}

/**
 * @brief 判断当前容器是否记录了解析错误。
 * @return 存在解析错误时返回 true。
 */
bool OLECompoundReader::hasError() const {
    return !m_errorMessage.isEmpty();
}

/**
 * @brief 返回当前容器的解析错误信息。
 * @return 最近一次解析错误信息；没有错误时为空。
 */
QString OLECompoundReader::errorString() const {
    return m_errorMessage;
}

/**
 * @brief 返回排序后的流路径，便于调用方稳定遍历容器内容。
 * @return 按大小写敏感顺序排列的流路径列表。
 */
QStringList OLECompoundReader::streamPaths() const {
    QStringList paths = m_streams.keys();  ///< 流路径列表。
    paths.sort(Qt::CaseSensitive);
    return paths;
}

/**
 * @brief 判断指定路径的流是否已经建立索引。
 * @param streamPath 相对于根条目的流路径。
 * @return 流已建立索引时返回 true。
 */
bool OLECompoundReader::containsStream(const QString& streamPath) const {
    return m_streams.contains(streamPath);
}

/**
 * @brief 复制指定路径的流内容，并拒绝不存在或无效的输出参数。
 * @param streamPath 相对于根条目的流路径。
 * @param data 接收流内容的输出缓冲区。
 * @return 流存在且输出缓冲区有效时返回 true。
 */
bool OLECompoundReader::readStream(const QString& streamPath, QByteArray* data) const {
    if (data == nullptr)
        return false;
    const auto iterator = m_streams.constFind(streamPath);  ///< 迭代器。
    if (iterator == m_streams.cend())
        return false;
    *data = iterator.value();
    return true;
}

}  // namespace EasyKiConverter
