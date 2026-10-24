/**
 * @file test_altium_ole.cpp
 * @brief Altium OLE 复合文档写入器的单元测试
 * @details 验证 OLECompoundWriter 的 CFB 格式正确性，包括：
 * - 迷你流和常规流的读写往返
 * - DIFAT 扩展（大文件 > 5.6MB）
 * - 无效/重名条目的拒绝
 * - SchLib/PcbLib 完整文件写入
 * - 焊盘扩展块、UID 图元信息、元件体
 * - UTF-8 参数编码
 * - 导出器的多部件和折线保持
 */

#include "core/altium/AltiumSchSymbolGeometryNormalizer.h"
#include "core/altium/AltiumSymbolPinConverter.h"
#include "core/altium/AltiumSymbolPinTextConverter.h"
#include "core/altium/ExporterAltiumFootprint.h"
#include "core/altium/ExporterAltiumSymbol.h"
#include "core/altium/compound/OLECompoundReader.h"
#include "core/altium/compound/OLECompoundWriter.h"
#include "core/altium/readers/AltiumPcbLibReader.h"
#include "core/altium/readers/AltiumSchLibReader.h"
#include "core/altium/utils/AltiumBinaryReader.h"
#include "core/altium/utils/AltiumBinaryWriter.h"
#include "core/altium/utils/AltiumConstants.h"
#include "core/altium/utils/AltiumCoord.h"
#include "core/altium/utils/AltiumLayerMap.h"
#include "core/altium/utils/AltiumWriterUtils.h"
#include "core/altium/writers/AltiumPcbLibWriter.h"
#include "core/altium/writers/AltiumSchLibWriter.h"

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QMap>
#include <QProcess>
#include <QSet>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

#include <algorithm>
#include <cstdint>
#include <limits>

using namespace EasyKiConverter;

namespace {

/**
 * @brief CFB 扇区大小（字节）
 */
constexpr qint64 CFB_SECTOR_SIZE = 512;
/**
 * @brief CFB 迷你扇区大小（字节）
 */
constexpr qint64 CFB_MINI_SECTOR_SIZE = 64;
/**
 * @brief CFB FAT 链终结标记
 */
constexpr quint32 CFB_ENDOFCHAIN = 0xFFFFFFFE;
/**
 * @brief CFB 空闲扇区标记
 */
constexpr quint32 CFB_FREESECT = 0xFFFFFFFF;
/**
 * @brief CFB FAT 扇区标记
 */
constexpr quint32 CFB_FATSECT = 0xFFFFFFFD;
/**
 * @brief CFB DIFAT 扇区标记
 */
constexpr quint32 CFB_DIFSECT = 0xFFFFFFFC;

/**
 * @brief 从字节数组中读取小端序 32 位无符号整数
 * @param data 数据缓冲区
 * @param offset 起始偏移
 * @return 读取的 32 位值
 */
quint32 readU32(const QByteArray& data, qint64 offset) {
    return static_cast<quint32>(static_cast<unsigned char>(data.at(offset))) |
           (static_cast<quint32>(static_cast<unsigned char>(data.at(offset + 1))) << 8) |
           (static_cast<quint32>(static_cast<unsigned char>(data.at(offset + 2))) << 16) |
           (static_cast<quint32>(static_cast<unsigned char>(data.at(offset + 3))) << 24);
}

/**
 * @brief 从字节数组中读取小端序 16 位无符号整数
 * @param data 数据缓冲区
 * @param offset 起始偏移
 * @return 读取的 16 位值
 */
quint16 readU16(const QByteArray& data, qint64 offset) {
    return static_cast<quint16>(static_cast<unsigned char>(data.at(offset))) |
           (static_cast<quint16>(static_cast<unsigned char>(data.at(offset + 1))) << 8);
}

/**
 * @brief 统计 SchLib Data 流中的记录块数量
 * @param data Data 流内容
 * @return 记录数量；块结构损坏时返回 -1
 */
int countSchLibRecords(const QByteArray& data) {
    // count：测试使用的数量或索引值。
    int count = 0;
    for (int offset = 0; offset + 4 <= data.size();) {
        // payloadSize：测试使用的数量或索引值。
        const int payloadSize = static_cast<int>(readU32(data, offset) & 0x00FFFFFFU);
        // nextOffset：下一个测试偏移量。
        const int nextOffset = offset + 4 + payloadSize;
        if (payloadSize <= 0 || nextOffset > data.size())
            return -1;
        ++count;
        offset = nextOffset;
    }
    return count;
}

/**
 * @brief 从字节数组中读取小端序 64 位无符号整数
 * @param data 数据缓冲区
 * @param offset 起始偏移
 * @return 读取的 64 位值
 */
quint64 readU64(const QByteArray& data, qint64 offset) {
    // value：测试使用的配置值或状态。
    quint64 value = 0;
    for (int i = 0; i < 8; ++i) {
        value |= static_cast<quint64>(static_cast<unsigned char>(data.at(offset + i))) << (i * 8);
    }
    return value;
}

/**
 * @brief CFB 目录条目结构（测试用简化版）
 */
struct CfbDirectoryEntry {
    // name：测试使用的元器件标识或名称。
    QString name;
    // type：测试使用的配置值或状态。
    quint8 type = 0;
    // color：测试用颜色值。
    quint8 color = 1;
    // leftChild：左子节点索引。
    quint32 leftChild = CFB_FREESECT;
    // rightChild：右子节点索引。
    quint32 rightChild = CFB_FREESECT;
    // child：子节点索引。
    quint32 child = CFB_FREESECT;
    // startSector：起始扇区索引。
    quint32 startSector = CFB_ENDOFCHAIN;
    // streamSize：测试使用的数量或索引值。
    quint64 streamSize = 0;
};

/**
 * @brief 读取 CFB 文件的指定扇区
 * @param fileData 完整文件数据
 * @param sector 扇区编号
 * @return 扇区数据（512 字节）
 */
QByteArray readCfbSector(const QByteArray& fileData, quint32 sector) {
    // offset：测试用文件偏移量。
    const qint64 offset = static_cast<qint64>(sector + 1) * CFB_SECTOR_SIZE;
    return fileData.mid(offset, CFB_SECTOR_SIZE);
}

/**
 * @brief 读取常规流数据（大于 4096 字节的流）
 * @param fileData 完整文件数据
 * @param fat FAT 表
 * @param startSector 起始扇区
 * @param size 流大小
 * @return 流数据
 */
QByteArray readRegularStream(const QByteArray& fileData,
                             const QVector<quint32>& fat,
                             quint32 startSector,
                             quint64 size) {
    // result：测试得到的实际结果数据。
    QByteArray result;
    // sector：当前测试用扇区索引。
    quint32 sector = startSector;
    while (result.size() < static_cast<int>(size) && sector != CFB_ENDOFCHAIN && sector != CFB_FREESECT) {
        result.append(readCfbSector(fileData, sector));
        sector = fat.value(static_cast<int>(sector));
    }
    return result.left(static_cast<int>(size));
}

/**
 * @brief 读取迷你流数据（小于 4096 字节的流）
 * @param fileData 完整文件数据（未使用，保留接口一致性）
 * @param fat FAT 表（未使用）
 * @param miniFat Mini-FAT 表
 * @param miniStreamData Mini-Stream 数据
 * @param startMiniSector 起始迷你扇区
 * @param size 流大小
 * @return 流数据
 */
QByteArray readMiniStream(const QByteArray& fileData,
                          const QVector<quint32>& fat,
                          const QVector<quint32>& miniFat,
                          const QByteArray& miniStreamData,
                          quint32 startMiniSector,
                          quint64 size) {
    Q_UNUSED(fileData);
    Q_UNUSED(fat);
    // result：测试得到的实际结果数据。
    QByteArray result;
    // sector：当前测试用扇区索引。
    quint32 sector = startMiniSector;
    while (result.size() < static_cast<int>(size) && sector != CFB_ENDOFCHAIN && sector != CFB_FREESECT) {
        result.append(miniStreamData.mid(static_cast<qint64>(sector) * CFB_MINI_SECTOR_SIZE, CFB_MINI_SECTOR_SIZE));
        sector = miniFat.value(static_cast<int>(sector));
    }
    return result.left(static_cast<int>(size));
}

/**
 * @brief CFB 名称比较函数（先按长度，再按字母序）
 * @param left 左侧名称
 * @param right 右侧名称
 * @return left 是否排在 right 前面
 */
bool cfbNameLess(const QString& left, const QString& right) {
    if (left.size() != right.size()) {
        return left.size() < right.size();
    }
    return left.compare(right, Qt::CaseInsensitive) < 0;
}

/**
 * @brief 从 CFB 文件中读取指定路径的流数据
 * @param filePath CFB 文件路径
 * @param streamPath 流路径（如 "FileHeader" 或 "Library/Header"）
 * @param output 输出数据
 * @param directoryOutput 可选输出完整目录条目列表
 * @return 是否成功读取
 */
bool readCfbStream(const QString& filePath,
                   const QString& streamPath,
                   QByteArray& output,
                   QVector<CfbDirectoryEntry>* directoryOutput = nullptr) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    // data：测试使用的文本或数据内容。
    const QByteArray data = file.readAll();

    if (data.size() < CFB_SECTOR_SIZE || data.left(8) != QByteArray::fromHex("d0cf11e0a1b11ae1")) {
        return false;
    }

    // numFatSectors：FAT 扇区数量。
    const quint32 numFatSectors = readU32(data, 44);
    // firstDirSector：测试使用的文件系统路径。
    const quint32 firstDirSector = readU32(data, 48);
    // firstMiniFatSector：首个 Mini-FAT 扇区。
    const quint32 firstMiniFatSector = readU32(data, 60);
    // numMiniFatSectors：Mini-FAT 扇区数量。
    const quint32 numMiniFatSectors = readU32(data, 64);
    // firstDifatSector：首个 DIFAT 扇区。
    const quint32 firstDifatSector = readU32(data, 68);
    // numDifatSectors：DIFAT 扇区数量。
    const quint32 numDifatSectors = readU32(data, 72);

    // fatSectorList：FAT 扇区列表。
    QVector<quint32> fatSectorList;
    for (int i = 0; i < 109 && static_cast<int>(fatSectorList.size()) < static_cast<int>(numFatSectors); ++i) {
        // sector：当前测试用扇区索引。
        const quint32 sector = readU32(data, 76 + i * 4);
        if (sector == CFB_ENDOFCHAIN || sector == CFB_FREESECT) {
            break;
        }
        fatSectorList.append(sector);
    }
    // difatSector：当前 DIFAT 扇区。
    quint32 difatSector = firstDifatSector;
    for (quint32 i = 0; i < numDifatSectors && difatSector != CFB_ENDOFCHAIN; ++i) {
        // sectorData：测试使用的文本或数据内容。
        const QByteArray sectorData = readCfbSector(data, difatSector);
        for (int j = 0; j < 127 && fatSectorList.size() < static_cast<int>(numFatSectors); ++j) {
            // fatSector：当前 FAT 扇区。
            const quint32 fatSector = readU32(sectorData, j * 4);
            if (fatSector != CFB_FREESECT) {
                fatSectorList.append(fatSector);
            }
        }
        difatSector = readU32(sectorData, 127 * 4);
    }
    if (fatSectorList.size() != static_cast<int>(numFatSectors)) {
        return false;
    }

    // fat：FAT 表。
    QVector<quint32> fat;
    for (quint32 fatSector : fatSectorList) {
        // sectorData：测试使用的文本或数据内容。
        const QByteArray sectorData = readCfbSector(data, fatSector);
        for (int i = 0; i < CFB_SECTOR_SIZE / 4; ++i) {
            fat.append(readU32(sectorData, i * 4));
        }
    }

    // entries：测试用条目集合。
    QVector<CfbDirectoryEntry> entries;
    // dirSector：测试使用的文件系统路径。
    quint32 dirSector = firstDirSector;
    while (dirSector != CFB_ENDOFCHAIN && dirSector != CFB_FREESECT) {
        // sectorData：测试使用的文本或数据内容。
        const QByteArray sectorData = readCfbSector(data, dirSector);
        for (int i = 0; i < CFB_SECTOR_SIZE / 128; ++i) {
            // offset：测试用文件偏移量。
            const qint64 offset = i * 128;
            // entry：测试使用的缓存或元器件对象。
            CfbDirectoryEntry entry;
            // nameSize：测试使用的元器件标识或名称。
            const quint16 nameSize = static_cast<quint16>(readU32(sectorData, offset + 64) & 0xFFFF);
            entry.name = QString::fromUtf16(reinterpret_cast<const char16_t*>(sectorData.constData() + offset),
                                            (nameSize - 2) / 2);
            entry.type = static_cast<quint8>(sectorData.at(offset + 66));
            entry.color = static_cast<quint8>(sectorData.at(offset + 67));
            entry.leftChild = readU32(sectorData, offset + 68);
            entry.rightChild = readU32(sectorData, offset + 72);
            entry.child = readU32(sectorData, offset + 76);
            entry.startSector = readU32(sectorData, offset + 116);
            entry.streamSize = readU64(sectorData, offset + 120);
            entries.append(entry);
        }
        dirSector = fat.value(static_cast<int>(dirSector));
    }

    if (entries.isEmpty() || entries.at(0).type != 5) {
        return false;
    }
    if (directoryOutput) {
        *directoryOutput = entries;
    }

    // miniStream：Mini-Stream 数据。
    const QByteArray miniStream = readRegularStream(data, fat, entries.at(0).startSector, entries.at(0).streamSize);
    // miniFat：Mini-FAT 表。
    QVector<quint32> miniFat;
    // miniFatSector：当前 Mini-FAT 扇区。
    quint32 miniFatSector = firstMiniFatSector;
    for (quint32 i = 0; i < numMiniFatSectors; ++i) {
        if (miniFatSector == CFB_ENDOFCHAIN || miniFatSector == CFB_FREESECT) {
            return false;
        }
        // sectorData：测试使用的文本或数据内容。
        const QByteArray sectorData = readCfbSector(data, miniFatSector);
        for (int j = 0; j < CFB_SECTOR_SIZE / 4; ++j) {
            miniFat.append(readU32(sectorData, j * 4));
        }
        miniFatSector = fat.value(static_cast<int>(miniFatSector), CFB_FREESECT);
    }
    if (numMiniFatSectors > 0 && miniFatSector != CFB_ENDOFCHAIN) {
        return false;
    }

    // pathParts：测试使用的文件系统路径。
    const QStringList pathParts = streamPath.split('/');
    // entryIndex：测试使用的数量或索引值。
    int entryIndex = 0;
    for (const QString& part : pathParts) {
        // node：当前目录树节点。
        int node = entries.at(entryIndex).child;
        // found：检测到的格式记录。
        int found = -1;
        while (node != CFB_FREESECT && node != CFB_ENDOFCHAIN) {
            if (entries.at(node).name.compare(part, Qt::CaseInsensitive) == 0) {
                found = node;
                break;
            }
            node = cfbNameLess(part, entries.at(node).name) ? entries.at(node).leftChild : entries.at(node).rightChild;
        }
        if (found < 0) {
            return false;
        }
        entryIndex = found;
    }

    // entry：测试使用的缓存或元器件对象。
    const CfbDirectoryEntry& entry = entries.at(entryIndex);
    if (entry.type != 2) {
        return false;
    }
    output = entry.streamSize < 4096
                 ? readMiniStream(data, fat, miniFat, miniStream, entry.startSector, entry.streamSize)
                 : readRegularStream(data, fat, entry.startSector, entry.streamSize);
    return output.size() == static_cast<int>(entry.streamSize);
}

/**
 * @brief 解析 SchLib /Storage 中的嵌入图片条目
 * @param storage Storage 流数据
 * @param compressedImages 输出图片名到原始 zlib 数据的映射
 * @return 条目结构和长度均有效时返回 true
 */
bool parseAltiumImageStorage(const QByteArray& storage, QMap<QString, QByteArray>& compressedImages) {
    compressedImages.clear();
    if (storage.size() < 4)
        return false;

    // headerSize：测试使用的数量或索引值。
    const int headerSize = static_cast<int>(readU32(storage, 0) & 0x00FFFFFFU);
    // offset：测试用文件偏移量。
    int offset = 4 + headerSize;
    if (offset > storage.size())
        return false;

    while (offset < storage.size()) {
        if (offset + 4 > storage.size())
            return false;
        // blockHeader：块头数据。
        const quint32 blockHeader = readU32(storage, offset);
        if ((blockHeader >> 24) != 1)
            return false;
        // entrySize：测试使用的数量或索引值。
        const int entrySize = static_cast<int>(blockHeader & 0x00FFFFFFU);
        // entryOffset：测试使用的缓存或元器件对象。
        const int entryOffset = offset + 4;
        // entryEnd：测试使用的缓存或元器件对象。
        const int entryEnd = entryOffset + entrySize;
        if (entrySize < 6 || entryEnd > storage.size())
            return false;
        if (static_cast<quint8>(storage.at(entryOffset)) != 0xD0)
            return false;

        // nameLength：测试使用的元器件标识或名称。
        const int nameLength = static_cast<quint8>(storage.at(entryOffset + 1));
        // compressedLengthOffset：文件记录的字节偏移量。
        const int compressedLengthOffset = entryOffset + 2 + nameLength;
        if (compressedLengthOffset + 4 > entryEnd)
            return false;
        // name：测试使用的元器件标识或名称。
        const QString name = QString::fromLocal8Bit(storage.constData() + entryOffset + 2, nameLength);
        if (name.isEmpty() || compressedImages.contains(name))
            return false;

        // compressedLength：压缩数据长度。
        const int compressedLength = static_cast<int>(readU32(storage, compressedLengthOffset));
        // compressedOffset：文件记录的字节偏移量。
        const int compressedOffset = compressedLengthOffset + 4;
        if (compressedLength != entryEnd - compressedOffset)
            return false;
        compressedImages.insert(name, storage.mid(compressedOffset, compressedLength));
        offset = entryEnd;
    }
    return offset == storage.size();
}

/**
 * @brief 为去除 Qt 长度前缀的 zlib 数据恢复 qUncompress 输入
 * @param compressed 原始 zlib 数据
 * @param uncompressedSize 解压后的字节数
 * @return 带 Qt 长度前缀的压缩数据
 */
QByteArray restoreQtCompressionHeader(const QByteArray& compressed, int uncompressedSize) {
    // withHeader：带文件头的测试数据。
    QByteArray withHeader;
    withHeader.reserve(compressed.size() + 4);
    withHeader.append(static_cast<char>((uncompressedSize >> 24) & 0xFF));
    withHeader.append(static_cast<char>((uncompressedSize >> 16) & 0xFF));
    withHeader.append(static_cast<char>((uncompressedSize >> 8) & 0xFF));
    withHeader.append(static_cast<char>(uncompressedSize & 0xFF));
    withHeader.append(compressed);
    return withHeader;
}

/**
 * @brief 递归验证 CFB 目录红黑树的平衡性和搜索序
 * @param entries 目录条目列表
 * @param node 当前节点索引
 * @param lowerBound 名称下界（BST 约束）
 * @param upperBound 名称上界（BST 约束）
 * @param visited 已访问节点集合
 * @param valid 验证结果标志（输出）
 * @return 当前子树的黑高
 */
int validateRedBlackSubtree(const QVector<CfbDirectoryEntry>& entries,
                            quint32 node,
                            const QString* lowerBound,
                            const QString* upperBound,
                            QSet<quint32>& visited,
                            bool& valid) {
    if (node == CFB_FREESECT) {
        return 1;
    }
    if (node >= static_cast<quint32>(entries.size()) || visited.contains(node)) {
        valid = false;
        return 0;
    }
    visited.insert(node);
    // entry：测试使用的缓存或元器件对象。
    const CfbDirectoryEntry& entry = entries.at(static_cast<int>(node));
    if ((lowerBound && !cfbNameLess(*lowerBound, entry.name)) ||
        (upperBound && !cfbNameLess(entry.name, *upperBound))) {
        valid = false;
    }
    if (entry.color == 0) {
        if ((entry.leftChild != CFB_FREESECT && entries.at(static_cast<int>(entry.leftChild)).color == 0) ||
            (entry.rightChild != CFB_FREESECT && entries.at(static_cast<int>(entry.rightChild)).color == 0)) {
            valid = false;
        }
    }
    // leftHeight：几何计算中的坐标或尺寸值。
    const int leftHeight = validateRedBlackSubtree(entries, entry.leftChild, lowerBound, &entry.name, visited, valid);
    // rightHeight：几何计算中的坐标或尺寸值。
    const int rightHeight = validateRedBlackSubtree(entries, entry.rightChild, &entry.name, upperBound, visited, valid);
    if (leftHeight != rightHeight) {
        valid = false;
    }
    return leftHeight + (entry.color == 1 ? 1 : 0);
}

}  // namespace

/**
 * @brief Altium OLE 复合文档写入器测试类
 * @details 验证 OLECompoundWriter、AltiumSchLibWriter、AltiumPcbLibWriter 的格式正确性，
 * 以及 ExporterAltiumSymbol 和 ExporterAltiumFootprint 的转换正确性。
 */
class TestAltiumOle : public QObject {
    Q_OBJECT

private slots:

    /**
     * @brief 验证极限原始坐标量化时不会发生整数溢出。
     */
    void coordinateConversionHandlesIntegerExtremes() {
        QCOMPARE(AltiumCoord::toDxpInt(std::numeric_limits<int>::max()), int16_t(21475));
        QCOMPARE(AltiumCoord::toDxpInt(std::numeric_limits<int>::min()), int16_t(-21475));
        QCOMPARE(AltiumCoord::mmToRaw(1.0e12), std::numeric_limits<int32_t>::max());
        QCOMPARE(AltiumCoord::mmToRaw(-1.0e12), std::numeric_limits<int32_t>::min());
        QCOMPARE(AltiumCoord::mmToRaw(std::numeric_limits<double>::quiet_NaN()), int32_t(0));
        QCOMPARE(AltiumCoord::mmToSchematicUnits(1.0e12), std::numeric_limits<int32_t>::max());
        QCOMPARE(AltiumCoord::mmToSchematicUnits(-1.0e12), std::numeric_limits<int32_t>::min());
        QCOMPARE(AltiumCoord::mmToSchematicUnits(std::numeric_limits<double>::infinity()), int32_t(0));
        QCOMPARE(AltiumCoord::mmToMilString(std::numeric_limits<double>::quiet_NaN()), QStringLiteral("0.000000mil"));
        QCOMPARE(AltiumCoord::mmToMilString(std::numeric_limits<double>::infinity()), QStringLiteral("0.000000mil"));
        QCOMPARE(AltiumCoord::lineWidthMmToIndex(std::numeric_limits<double>::quiet_NaN()), 0);
        QCOMPARE(AltiumCoord::lineWidthMmToIndex(std::numeric_limits<double>::infinity()), 0);
    }

    /**
     * @brief 验证 EasyEDA 用户自定义层可映射到有效的 Altium 机械层。
     */
    void userDefinedLayerMapsToMechanicalLayer() {
        QCOMPARE(AltiumLayerMap::fromLayerTypeToAltium(IR::LayerType::UserDefined), 57);
    }

    /**
     * @brief 验证 Altium 符号引脚编号始终通过独立水平文本显示。
     * @details 同时覆盖存在和缺失源编号坐标的引脚，确保不会回退到随引脚方向旋转的内置编号。
     */
    void pinDesignatorsAlwaysUseHorizontalText() {
        // positionedPin：测试用引脚数据。
        IR::SymbolPinIR positionedPin;
        positionedPin.name = QStringLiteral("IN");
        positionedPin.designator = QStringLiteral("1");
        positionedPin.position = QPointF(2.0, 3.0);
        positionedPin.numberPosition = QPointF(1.0, 2.0);
        positionedPin.numberRotation = 90.0;
        positionedPin.hasNumberPosition = true;
        positionedPin.direction = IR::PinDirection::Up;

        const QList<AltiumSchText> positionedTexts =
            AltiumSymbolPinTextConverter::convert(positionedPin, QStringLiteral("POSITIONED"), nullptr);
        QCOMPARE(positionedTexts.size(), 1);
        QCOMPARE(positionedTexts.first().text, QStringLiteral("1"));
        QCOMPARE(positionedTexts.first().orientation, 0);
        QCOMPARE(positionedTexts.first().locationX, AltiumCoord::mmToRaw(1.0));
        QCOMPARE(positionedTexts.first().locationY, AltiumCoord::mmToRaw(2.0));
        QVERIFY(!AltiumSymbolPinConverter::convert(positionedPin).showDesignator);

        // fallbackPin：测试用引脚数据。
        IR::SymbolPinIR fallbackPin = positionedPin;
        fallbackPin.designator = QStringLiteral("2");
        fallbackPin.position = QPointF(-4.0, 5.0);
        fallbackPin.numberPosition = {};
        fallbackPin.numberRotation = 270.0;
        fallbackPin.hasNumberPosition = false;
        fallbackPin.direction = IR::PinDirection::Down;

        const QList<AltiumSchText> fallbackTexts =
            AltiumSymbolPinTextConverter::convert(fallbackPin, QStringLiteral("FALLBACK"), nullptr);
        QCOMPARE(fallbackTexts.size(), 1);
        QCOMPARE(fallbackTexts.first().text, QStringLiteral("2"));
        QCOMPARE(fallbackTexts.first().orientation, 0);
        QCOMPARE(fallbackTexts.first().locationX, AltiumCoord::mmToRaw(-4.0));
        QCOMPARE(fallbackTexts.first().locationY, AltiumCoord::mmToRaw(5.0));
        QVERIFY(!AltiumSymbolPinConverter::convert(fallbackPin).showDesignator);

        // exportedSymbol：测试用符号数据。
        IR::SymbolComponentIR exportedSymbol;
        exportedSymbol.name = QStringLiteral("HORIZONTAL_PIN_NUMBERS");
        exportedSymbol.pins = {positionedPin, fallbackPin};
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        // libraryPath：测试使用的文件系统路径。
        const QString libraryPath = tempDir.filePath(QStringLiteral("horizontal-pin-numbers.SchLib"));
        // exporter：负责生成测试输出的导出器实例。
        ExporterAltiumSymbol exporter;
        QVERIFY(exporter.exportSymbolLibrary(
            {exportedSymbol}, QStringLiteral("horizontal-pin-numbers"), libraryPath, false, false));
        // symbolData：测试使用的文本或数据内容。
        QByteArray symbolData;
        QVERIFY(readCfbStream(libraryPath, QStringLiteral("HORIZONTAL_PIN_NUMBERS/Data"), symbolData));
        for (const QByteArray& designator : {QByteArrayLiteral("1"), QByteArrayLiteral("2")}) {
            // textOffset：测试使用的文本或数据内容。
            const int textOffset = symbolData.indexOf("Text=" + designator);
            QVERIFY(textOffset > 0);
            // textRecordOffset：测试使用的文本或数据内容。
            const int textRecordOffset = symbolData.lastIndexOf("|RECORD=4|", textOffset);
            QVERIFY(textRecordOffset >= 0);
            // nextRecordOffset：下一个记录偏移量。
            const int nextRecordOffset = symbolData.indexOf("|RECORD=", textOffset);
            QVERIFY(nextRecordOffset > textOffset);
            // textRecord：测试使用的文本或数据内容。
            const QByteArray textRecord = symbolData.mid(textRecordOffset, nextRecordOffset - textRecordOffset);
            QVERIFY(!textRecord.contains("Orientation=1"));
            QVERIFY(!textRecord.contains("Orientation=2"));
            QVERIFY(!textRecord.contains("Orientation=3"));
        }
        QVERIFY(!symbolData.contains("ShowDesignator=T"));
    }

    /**
     * @brief 验证普通符号文本和可见参数统一排布在图形主体下方。
     * @details 引脚标签属于引脚局部文本，不应被移动到符号底部或改变其原始位置。
     */
    void visibleSymbolFieldsArePlacedBelowGraphics() {
        // component：测试使用的缓存或元器件对象。
        AltiumSchComponent component;
        // body：测试用主体数据。
        AltiumSchRectangle body;
        body.locationX = -1000000;
        body.locationY = -500000;
        body.cornerX = 1000000;
        body.cornerY = 500000;
        component.rectangles.append(body);

        // text：测试使用的文本或数据内容。
        AltiumSchText text;
        text.locationX = 9000000;
        text.locationY = 1000000;
        text.orientation = 3;
        component.texts.append(text);

        // pinLabel：测试用引脚数据。
        AltiumSchText pinLabel;
        pinLabel.locationX = 500000;
        pinLabel.locationY = 500000;
        pinLabel.isPinLabel = true;
        component.texts.append(pinLabel);

        // parameter：测试用参数。
        AltiumSchParameter parameter;
        parameter.locationX = 8000000;
        parameter.locationY = 1000000;
        parameter.isHidden = false;
        parameter.orientation = 2;
        component.parameters.append(parameter);

        AltiumSchSymbolGeometryNormalizer::normalize(component);

        // movedText：测试使用的文本或数据内容。
        const AltiumSchText& movedText = component.texts.first();
        // preservedPinLabel：测试用引脚数据。
        const AltiumSchText& preservedPinLabel = component.texts.at(1);
        // movedParameter：迁移后的参数。
        const AltiumSchParameter& movedParameter = component.parameters.first();
        // normalizedBody：规范化后的符号主体。
        const AltiumSchRectangle& normalizedBody = component.rectangles.first();
        // graphicCenterX：图形主体中心横坐标。
        const int graphicCenterX = (normalizedBody.locationX + normalizedBody.cornerX) / 2;
        // graphicMinY：图形主体最小纵坐标。
        const int graphicMinY = qMin(normalizedBody.locationY, normalizedBody.cornerY);
        QVERIFY(movedText.locationX == graphicCenterX);
        QVERIFY(movedText.locationY < graphicMinY);
        QCOMPARE(movedText.locationX, movedParameter.locationX);
        QVERIFY(movedParameter.locationY < movedText.locationY);
        QCOMPARE(movedText.orientation, 0);
        QCOMPARE(movedParameter.orientation, 0);
        QVERIFY(preservedPinLabel.locationX != movedText.locationX ||
                preservedPinLabel.locationY != movedText.locationY);
    }

    /**
     * @brief 验证来源元数据默认隐藏，器件值位于符号主体下方。
     * @details 保留 Datasheet、供应商和料号字段供参数编辑器使用，但避免长 URL 和机器字段污染符号画面。
     */
    void sourceMetadataFieldsUseReadableVisibility() {
        // tempDir：测试生成文件所在的临时目录。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // component：包含来源元数据和主体轮廓的测试元件。
        AltiumSchComponent component;
        component.name = QStringLiteral("LM5164DDAR");
        component.designatorPrefix = QStringLiteral("U");
        component.sourceMetadata.insert(QStringLiteral("datasheet"),
                                        QStringLiteral("https://example.com/LM5164DDAR_C477928.html"));
        component.sourceMetadata.insert(QStringLiteral("manufacturerPart"), QStringLiteral("LM5164DDAR"));
        component.sourceMetadata.insert(QStringLiteral("lcscId"), QStringLiteral("C477928"));
        // body：测试用主体矩形。
        AltiumSchRectangle body;
        body.locationX = -1000000;
        body.locationY = -1000000;
        body.cornerX = 1000000;
        body.cornerY = 1000000;
        component.rectangles.append(body);

        // outputPath：测试生成的 SchLib 文件路径。
        const QString outputPath = QDir(tempDir.path()).filePath(QStringLiteral("metadata-visibility.SchLib"));
        // writer：负责写入测试 SchLib 的写入器。
        AltiumSchLibWriter writer;
        QVERIFY(writer.write({component}, outputPath, QStringLiteral("metadata-visibility")));

        // reader：负责读取生成文件的记录读取器。
        AltiumSchLibReader reader;
        QVERIFY2(reader.open(outputPath), qPrintable(reader.errorString()));
        // records：读取到的元件记录。
        QVector<AltiumSchLibReader::Record> records;
        QVERIFY(reader.readComponentRecords(QStringLiteral("LM5164DDAR"), &records));
        // foundComment：是否找到可见的器件值字段。
        bool foundComment = false;
        // foundHiddenDatasheet：是否找到隐藏的数据手册字段。
        bool foundHiddenDatasheet = false;
        // foundHiddenManufacturerPart：是否找到隐藏的制造商料号字段。
        bool foundHiddenManufacturerPart = false;
        for (const AltiumSchLibReader::Record& record : records) {
            if (record.recordType != 41)
                continue;
            const QString name = record.parameters.value(QStringLiteral("NAME"));  // 当前参数的名称。
            if (name == QStringLiteral("Comment")) {
                foundComment = true;
                QVERIFY(!record.parameters.contains(QStringLiteral("IsHidden")));
                QVERIFY(record.parameters.contains(QStringLiteral("LOCATION.X")));
                QVERIFY(record.parameters.contains(QStringLiteral("LOCATION.Y")));
            } else if (name == QStringLiteral("Datasheet")) {
                foundHiddenDatasheet = record.parameters.value(QStringLiteral("IsHidden")) == QStringLiteral("T");
            } else if (name == QStringLiteral("Manufacturer Part Number")) {
                foundHiddenManufacturerPart =
                    record.parameters.value(QStringLiteral("IsHidden")) == QStringLiteral("T");
            }
        }
        QVERIFY(foundComment);
        QVERIFY(foundHiddenDatasheet);
        QVERIFY(foundHiddenManufacturerPart);
    }

    /**
     * @brief 验证缺少源编号坐标时编号文本跟随投影后的引脚主体端。
     * @details 覆盖水平和垂直引脚，确保主体边界投影及连接点量化不会造成编号错位。
     */
    void fallbackPinNumbersFollowNormalizedPins() {
        // component：测试使用的缓存或元器件对象。
        AltiumSchComponent component;
        // body：测试用主体数据。
        AltiumSchRectangle body;
        body.locationX = -1000000;
        body.locationY = -1000000;
        body.cornerX = 1000000;
        body.cornerY = 1000000;
        component.rectangles.append(body);

        // horizontalPin：测试用引脚数据。
        AltiumSchPin horizontalPin;
        horizontalPin.locationX = 4500000;
        horizontalPin.locationY = 250000;
        horizontalPin.length = 350000;
        horizontalPin.orientation = AltiumModels::PinOrientation::Right;
        component.pins.append(horizontalPin);
        // horizontalNumber：水平引脚编号文本。
        AltiumSchText horizontalNumber;
        horizontalNumber.locationX = 4500000;
        horizontalNumber.locationY = 250000;
        horizontalNumber.isPinLabel = true;
        horizontalNumber.isPinNumber = true;
        horizontalNumber.isFallbackPinNumber = true;
        horizontalNumber.sourcePinIndex = 0;
        component.texts.append(horizontalNumber);

        // verticalPin：测试用引脚数据。
        AltiumSchPin verticalPin;
        verticalPin.locationX = -250000;
        verticalPin.locationY = 4500000;
        verticalPin.length = 350000;
        verticalPin.orientation = AltiumModels::PinOrientation::Up;
        component.pins.append(verticalPin);
        // verticalNumber：垂直引脚编号文本。
        AltiumSchText verticalNumber = horizontalNumber;
        verticalNumber.locationX = -250000;
        verticalNumber.locationY = 4500000;
        verticalNumber.sourcePinIndex = 1;
        verticalNumber.isPinNumber = true;
        component.texts.append(verticalNumber);

        AltiumSchSymbolGeometryNormalizer::normalize(component);

        QCOMPARE(component.texts.at(0).locationX, component.pins.at(0).locationX + 350000 + 200000);
        QCOMPARE(component.texts.at(0).locationY, component.pins.at(0).locationY);
        QCOMPARE(component.texts.at(1).locationX, component.pins.at(1).locationX);
        QCOMPARE(component.texts.at(1).locationY, component.pins.at(1).locationY + 350000 + 200000);
    }

    /**
     * @brief 验证长引脚名称不会侵入符号主体中心。
     * @details 通过扩展主体宽度并限制引脚文本字号，覆盖 EN/UVLO、PGOOD 等常见长名称布局。
     */
    void longPinLabelsStayInsideExpandedBody() {
        // component：包含左右引脚和主体轮廓的测试符号。
        AltiumSchComponent component;
        // body：初始较窄的主体矩形，归一化时应按长名称扩展。
        AltiumSchRectangle body;
        body.locationX = -1000000;
        body.locationY = -1000000;
        body.cornerX = 1000000;
        body.cornerY = 1000000;
        component.rectangles.append(body);

        // leftPin：左侧测试引脚。
        AltiumSchPin leftPin;
        leftPin.locationX = -2000000;
        leftPin.locationY = 0;
        leftPin.length = 350000;
        leftPin.orientation = AltiumModels::PinOrientation::Left;
        component.pins.append(leftPin);
        // rightPin：右侧测试引脚。
        AltiumSchPin rightPin = leftPin;
        rightPin.locationX = 2000000;
        rightPin.orientation = AltiumModels::PinOrientation::Right;
        component.pins.append(rightPin);

        // leftLabel：左侧长引脚名称。
        AltiumSchText leftLabel;
        leftLabel.text = QStringLiteral("EN/UVLO");
        leftLabel.fontSizeMm = 2.0;
        leftLabel.isPinLabel = true;
        leftLabel.sourcePinIndex = 0;
        component.texts.append(leftLabel);
        // rightLabel：右侧长引脚名称。
        AltiumSchText rightLabel = leftLabel;
        rightLabel.text = QStringLiteral("PGOOD");
        rightLabel.sourcePinIndex = 1;
        component.texts.append(rightLabel);

        AltiumSchSymbolGeometryNormalizer::normalize(component);

        const AltiumSchRectangle& normalizedBody = component.rectangles.first();  // 扩展后的主体矩形。
        const int bodyWidth = qAbs(normalizedBody.cornerX - normalizedBody.locationX);  // 主体宽度。
        QVERIFY(bodyWidth > 2000000);
        QCOMPARE(component.texts.at(0).fontSizeMm, 1.27);
        QCOMPARE(component.texts.at(1).fontSizeMm, 1.27);
        QVERIFY(component.texts.at(0).locationX > qMin(normalizedBody.locationX, normalizedBody.cornerX));
        QVERIFY(component.texts.at(1).locationX < qMax(normalizedBody.locationX, normalizedBody.cornerX));
    }

    /**
     * @brief 验证多部件符号按部件独立计算主体、引脚和字段布局。
     * @details 一个部件的主体宽度、引脚间距和字段位置不得推动另一个部件，公共部件仍保持独立归属。
     */
    void multipartGeometryUsesIndependentPartBounds() {
        // component：包含两个几何尺寸不同部件的测试符号。
        AltiumSchComponent component;
        component.partCount = 2;

        // firstBody：第一个部件的窄主体。
        AltiumSchRectangle firstBody;
        firstBody.locationX = -1000000;
        firstBody.locationY = -1000000;
        firstBody.cornerX = 1000000;
        firstBody.cornerY = 1000000;
        firstBody.ownerPartId = 1;
        component.rectangles.append(firstBody);

        // secondBody：第二个部件的宽主体，故意使用不同的源位置。
        AltiumSchRectangle secondBody;
        secondBody.locationX = -5000000;
        secondBody.locationY = 2000000;
        secondBody.cornerX = 5000000;
        secondBody.cornerY = 4000000;
        secondBody.ownerPartId = 2;
        component.rectangles.append(secondBody);

        // firstPin：第一个部件的右侧引脚。
        AltiumSchPin firstPin;
        firstPin.ownerPartId = 1;
        firstPin.locationX = 3000000;
        firstPin.locationY = 0;
        firstPin.length = 500000;
        firstPin.orientation = AltiumModels::PinOrientation::Right;
        component.pins.append(firstPin);

        // secondPin：第二个部件的右侧引脚。
        AltiumSchPin secondPin = firstPin;
        secondPin.ownerPartId = 2;
        secondPin.locationY = 3000000;
        component.pins.append(secondPin);

        // firstLabel：第一个部件的引脚名称。
        AltiumSchText firstLabel;
        firstLabel.text = QStringLiteral("P1");
        firstLabel.isPinLabel = true;
        firstLabel.sourcePinIndex = 0;
        firstLabel.ownerPartId = 1;
        component.texts.append(firstLabel);

        // secondLabel：第二个部件的引脚名称。
        AltiumSchText secondLabel = firstLabel;
        secondLabel.text = QStringLiteral("P2");
        secondLabel.sourcePinIndex = 1;
        secondLabel.ownerPartId = 2;
        component.texts.append(secondLabel);

        // firstField：第一个部件的可见参数字段。
        AltiumSchParameter firstField;
        firstField.name = QStringLiteral("Comment");
        firstField.value = QStringLiteral("PART1");
        firstField.ownerPartId = 1;
        firstField.isHidden = false;
        component.parameters.append(firstField);

        // secondField：第二个部件的可见参数字段。
        AltiumSchParameter secondField = firstField;
        secondField.value = QStringLiteral("PART2");
        secondField.ownerPartId = 2;
        component.parameters.append(secondField);

        AltiumSchSymbolGeometryNormalizer::normalize(component);

        // normalizedFirstBody：归一化后的第一个部件主体。
        const AltiumSchRectangle& normalizedFirstBody = component.rectangles.at(0);
        // normalizedSecondBody：归一化后的第二个部件主体。
        const AltiumSchRectangle& normalizedSecondBody = component.rectangles.at(1);
        QCOMPARE(qAbs(normalizedFirstBody.cornerX - normalizedFirstBody.locationX), 2000000);
        QCOMPARE(qAbs(normalizedSecondBody.cornerX - normalizedSecondBody.locationX), 10000000);
        QCOMPARE(component.pins.at(0).locationX, qMax(normalizedFirstBody.locationX, normalizedFirstBody.cornerX));
        QCOMPARE(component.pins.at(1).locationX, qMax(normalizedSecondBody.locationX, normalizedSecondBody.cornerX));
        QCOMPARE(component.texts.at(0).locationY, component.pins.at(0).locationY);
        QCOMPARE(component.texts.at(1).locationY, component.pins.at(1).locationY);
        QVERIFY(component.parameters.at(0).locationY <
                qMin(normalizedFirstBody.locationY, normalizedFirstBody.cornerY));
        QVERIFY(component.parameters.at(1).locationY <
                qMin(normalizedSecondBody.locationY, normalizedSecondBody.cornerY));
    }

    /**
     * @brief 验证高引脚数多部件符号不会跨部件混合引脚布局。
     * @details 使用接近 C114407 的 134/131 引脚规模，检查引脚数量、归属、编号、方向和主体连接边界均保持独立。
     */
    void highPinCountMultipartGeometryKeepsPartOwnership() {
        // component：包含两个高引脚数部件的合成测试符号。
        AltiumSchComponent component;
        component.partCount = 2;

        // firstBody：第一个部件的主体边界。
        AltiumSchRectangle firstBody;
        firstBody.locationX = -5000000;
        firstBody.locationY = -70000000;
        firstBody.cornerX = 5000000;
        firstBody.cornerY = 70000000;
        firstBody.ownerPartId = 1;
        component.rectangles.append(firstBody);

        // secondBody：第二个部件的主体边界，使用不同宽度和垂直位置。
        AltiumSchRectangle secondBody;
        secondBody.locationX = -8000000;
        secondBody.locationY = 300000000;
        secondBody.cornerX = 8000000;
        secondBody.cornerY = 440000000;
        secondBody.ownerPartId = 2;
        component.rectangles.append(secondBody);

        // firstPartPinCount：第一个部件的高引脚数规模。
        constexpr int firstPartPinCount = 134;
        // secondPartPinCount：第二个部件的高引脚数规模。
        constexpr int secondPartPinCount = 131;
        // firstPartPinIndex：第一个部件的引脚循环索引。
        for (int firstPartPinIndex = 0; firstPartPinIndex < firstPartPinCount; ++firstPartPinIndex) {
            // firstPartPin：当前第一个部件的右侧引脚。
            AltiumSchPin firstPartPin;
            firstPartPin.name = QStringLiteral("A%1").arg(firstPartPinIndex);
            firstPartPin.designator = QString::number(firstPartPinIndex + 1);
            firstPartPin.locationX = 15000000;
            firstPartPin.locationY = -66000000 + firstPartPinIndex * 1000000;
            firstPartPin.length = 500000;
            firstPartPin.orientation = AltiumModels::PinOrientation::Right;
            firstPartPin.ownerPartId = 1;
            component.pins.append(firstPartPin);
        }

        // secondPartPinIndex：第二个部件的引脚循环索引。
        for (int secondPartPinIndex = 0; secondPartPinIndex < secondPartPinCount; ++secondPartPinIndex) {
            // secondPartPin：当前第二个部件的右侧引脚。
            AltiumSchPin secondPartPin;
            secondPartPin.name = QStringLiteral("B%1").arg(secondPartPinIndex);
            secondPartPin.designator = QString::number(200 + secondPartPinIndex);
            secondPartPin.locationX = 17900000;
            secondPartPin.locationY = 304000000 + secondPartPinIndex * 1000000;
            secondPartPin.length = 500000;
            secondPartPin.orientation = AltiumModels::PinOrientation::Right;
            secondPartPin.ownerPartId = 2;
            component.pins.append(secondPartPin);
        }

        AltiumSchSymbolGeometryNormalizer::normalize(component);

        // firstPartDesignators：收集第一个部件的编号，检查导出过程中没有丢失或重复。
        QSet<QString> firstPartDesignators;
        // secondPartDesignators：收集第二个部件的编号，检查导出过程中没有丢失或重复。
        QSet<QString> secondPartDesignators;
        // normalizedFirstBody：归一化后的第一个部件主体。
        const AltiumSchRectangle& normalizedFirstBody = component.rectangles.at(0);
        // normalizedSecondBody：归一化后的第二个部件主体。
        const AltiumSchRectangle& normalizedSecondBody = component.rectangles.at(1);
        // firstBodyMinY：第一个部件归一化主体的最小 Y 坐标。
        const int firstBodyMinY = qMin(normalizedFirstBody.locationY, normalizedFirstBody.cornerY);
        // firstBodyMaxY：第一个部件归一化主体的最大 Y 坐标。
        const int firstBodyMaxY = qMax(normalizedFirstBody.locationY, normalizedFirstBody.cornerY);
        // secondBodyMinY：第二个部件归一化主体的最小 Y 坐标。
        const int secondBodyMinY = qMin(normalizedSecondBody.locationY, normalizedSecondBody.cornerY);
        // secondBodyMaxY：第二个部件归一化主体的最大 Y 坐标。
        const int secondBodyMaxY = qMax(normalizedSecondBody.locationY, normalizedSecondBody.cornerY);
        for (const AltiumSchPin& pin : component.pins) {
            if (pin.ownerPartId == 1) {
                firstPartDesignators.insert(pin.designator);
                QCOMPARE(pin.orientation, AltiumModels::PinOrientation::Right);
                QCOMPARE(pin.locationX, qMax(normalizedFirstBody.locationX, normalizedFirstBody.cornerX));
                QVERIFY(pin.locationY >= firstBodyMinY);
                QVERIFY(pin.locationY <= firstBodyMaxY);
            } else if (pin.ownerPartId == 2) {
                secondPartDesignators.insert(pin.designator);
                QCOMPARE(pin.orientation, AltiumModels::PinOrientation::Right);
                QCOMPARE(pin.locationX, qMax(normalizedSecondBody.locationX, normalizedSecondBody.cornerX));
                QVERIFY(pin.locationY >= secondBodyMinY);
                QVERIFY(pin.locationY <= secondBodyMaxY);
            } else {
                QFAIL("高引脚数测试发现未知部件归属");
            }
        }
        QCOMPARE(firstPartDesignators.size(), firstPartPinCount);
        QCOMPARE(secondPartDesignators.size(), secondPartPinCount);
        QCOMPARE(firstPartDesignators.size() + secondPartDesignators.size(), component.pins.size());
    }

    /**
     * @brief 验证高引脚数部件会收紧过大的主体垂直空白。
     * @details 主体边界应围绕本部件的水平引脚范围调整，不能保留源数据带来的无意义大段空白。
     */
    void multipartBodyHeightFollowsIndependentPinRanges() {
        // component：包含两个垂直空白明显不同的部件。
        AltiumSchComponent component;
        component.partCount = 2;

        // firstBody：第一个部件的过高主体。
        AltiumSchRectangle firstBody;
        firstBody.locationX = -2000000;
        firstBody.locationY = -100000000;
        firstBody.cornerX = 2000000;
        firstBody.cornerY = -50000000;
        firstBody.ownerPartId = 1;
        component.rectangles.append(firstBody);

        // secondBody：第二个部件的过高主体。
        AltiumSchRectangle secondBody = firstBody;
        secondBody.locationY = 300000000;
        secondBody.cornerY = 350000000;
        secondBody.ownerPartId = 2;
        component.rectangles.append(secondBody);

        // firstPinIndex：第一个部件引脚的循环索引。
        for (int firstPinIndex = 0; firstPinIndex < 3; ++firstPinIndex) {
            // firstPin：第一个部件的右侧引脚。
            AltiumSchPin firstPin;
            firstPin.ownerPartId = 1;
            firstPin.locationX = 3000000;
            firstPin.locationY = -1000000 + firstPinIndex * 1000000;
            firstPin.length = 500000;
            firstPin.orientation = AltiumModels::PinOrientation::Right;
            component.pins.append(firstPin);
        }

        // secondPinIndex：第二个部件引脚的循环索引。
        for (int secondPinIndex = 0; secondPinIndex < 3; ++secondPinIndex) {
            // secondPin：第二个部件的右侧引脚。
            AltiumSchPin secondPin;
            secondPin.ownerPartId = 2;
            secondPin.locationX = 3000000;
            secondPin.locationY = 399000000 + secondPinIndex * 1000000;
            secondPin.length = 500000;
            secondPin.orientation = AltiumModels::PinOrientation::Right;
            component.pins.append(secondPin);
        }

        AltiumSchSymbolGeometryNormalizer::normalize(component);

        // normalizedFirstBody：归一化后的第一个部件主体。
        const AltiumSchRectangle& normalizedFirstBody = component.rectangles.at(0);
        // normalizedSecondBody：归一化后的第二个部件主体。
        const AltiumSchRectangle& normalizedSecondBody = component.rectangles.at(1);
        // firstHeight：第一个部件的归一化主体高度。
        const int firstHeight = qAbs(normalizedFirstBody.cornerY - normalizedFirstBody.locationY);
        // secondHeight：第二个部件的归一化主体高度。
        const int secondHeight = qAbs(normalizedSecondBody.cornerY - normalizedSecondBody.locationY);
        QCOMPARE(firstHeight, 4000000);
        QCOMPARE(secondHeight, 4000000);
        QVERIFY(component.pins.at(0).locationY >= qMin(normalizedFirstBody.locationY, normalizedFirstBody.cornerY));
        QVERIFY(component.pins.at(0).locationY <= qMax(normalizedFirstBody.locationY, normalizedFirstBody.cornerY));
        QVERIFY(component.pins.at(3).locationY >= qMin(normalizedSecondBody.locationY, normalizedSecondBody.cornerY));
        QVERIFY(component.pins.at(3).locationY <= qMax(normalizedSecondBody.locationY, normalizedSecondBody.cornerY));
        // firstBodyCenterY：第一个部件主体的垂直中心。
        const int firstBodyCenterY = (normalizedFirstBody.locationY + normalizedFirstBody.cornerY) / 2;
        // secondBodyCenterY：第二个部件主体的垂直中心。
        const int secondBodyCenterY = (normalizedSecondBody.locationY + normalizedSecondBody.cornerY) / 2;
        // firstPinCenterY：第一个部件两端引脚的垂直中心。
        const int firstPinCenterY = (component.pins.at(0).locationY + component.pins.at(2).locationY) / 2;
        // secondPinCenterY：第二个部件两端引脚的垂直中心。
        const int secondPinCenterY = (component.pins.at(3).locationY + component.pins.at(5).locationY) / 2;
        QVERIFY(qAbs(firstPinCenterY - firstBodyCenterY) <= 100000);
        QVERIFY(qAbs(secondPinCenterY - secondBodyCenterY) <= 100000);
        // firstPinEdgeDelta：第一个部件引脚主体端与主体右边界的吸附误差。
        const int firstPinEdgeDelta =
            qAbs(component.pins.at(0).locationX - qMax(normalizedFirstBody.locationX, normalizedFirstBody.cornerX));
        // secondPinEdgeDelta：第二个部件引脚主体端与主体右边界的吸附误差。
        const int secondPinEdgeDelta =
            qAbs(component.pins.at(3).locationX - qMax(normalizedSecondBody.locationX, normalizedSecondBody.cornerX));
        QVERIFY(firstPinEdgeDelta <= 100000);
        QVERIFY(secondPinEdgeDelta <= 100000);
    }

    /**
     * @brief 验证包含迷你流和常规流的 CFB 文件可被正确解析
     * @details 写入多种大小的流，验证红黑树平衡性，以及嵌套存储路径的读取
     */
    void writesReadableCfbWithMiniAndRegularStreams() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // writer：负责写入目标格式的 writer 实例。
        OLECompoundWriter writer;
        QVERIFY(writer.create());
        QVERIFY(writer.writeStream("FileHeader", QByteArrayLiteral("header payload")));
        QVERIFY(writer.addStorage("Library"));
        QVERIFY(writer.writeStream("Library", "Header", QByteArrayLiteral("hdr")));
        QVERIFY(writer.addStorage("Component"));

        // regularData：测试使用的文本或数据内容。
        QByteArray regularData;
        regularData.reserve(5000);
        for (int i = 0; i < 5000; ++i) {
            regularData.append(static_cast<char>((i * 31 + 7) & 0xFF));
        }
        QVERIFY(writer.writeStream("Component", "Data", regularData));
        QVERIFY(writer.writeStream("Small", QByteArrayLiteral("small stream")));
        QVERIFY(writer.writeStream("Empty", QByteArray()));
        for (int i = 0; i < 140; ++i) {
            QVERIFY(
                writer.writeStream(QStringLiteral("Sibling%1").arg(i, 2, 10, QLatin1Char('0')), QByteArray(1, 'x')));
        }

        // filePath：测试使用的文件系统路径。
        const QString filePath = QDir(tempDir.path()).filePath(QStringLiteral("test.cfb"));
        QVERIFY(writer.saveToFile(filePath));

        // gsf：解析得到的 GSF 实现记录。
        const QString gsf = QStandardPaths::findExecutable(QStringLiteral("gsf"));
        if (!gsf.isEmpty()) {
            // validator：测试使用的元器件标识或名称。
            QProcess validator;
            validator.start(gsf, {QStringLiteral("list"), filePath});
            QVERIFY(validator.waitForFinished(10000));
            QCOMPARE(validator.exitCode(), 0);
            // diagnostics：导出过程中收集的诊断信息。
            const QByteArray diagnostics = validator.readAllStandardError() + validator.readAllStandardOutput();
            QVERIFY2(!diagnostics.contains("invalid OLE"), diagnostics.constData());
        }

        // headerPayload：头部载荷数据。
        QByteArray headerPayload;
        // directory：测试使用的文件系统路径。
        QVector<CfbDirectoryEntry> directory;
        QVERIFY(readCfbStream(filePath, QStringLiteral("FileHeader"), headerPayload, &directory));
        QCOMPARE(headerPayload, QByteArrayLiteral("header payload"));

        QVERIFY(directory.at(static_cast<int>(directory.at(0).child)).color == 1);
        // visited：测试使用的数值结果。
        QSet<quint32> visited;
        // validTree：测试使用的元器件标识或名称。
        bool validTree = true;
        validateRedBlackSubtree(directory, directory.at(0).child, nullptr, nullptr, visited, validTree);
        QVERIFY(validTree);

        const auto emptyEntry = std::find_if(directory.cbegin(), directory.cend(), [](const CfbDirectoryEntry& entry) {
            return entry.name == QStringLiteral("Empty");
        });
        QVERIFY(emptyEntry != directory.cend());
        QCOMPARE(emptyEntry->streamSize, quint64(0));
        QCOMPARE(emptyEntry->startSector, CFB_ENDOFCHAIN);

        // libraryHeader：库头数据。
        QByteArray libraryHeader;
        QVERIFY(readCfbStream(filePath, QStringLiteral("Library/Header"), libraryHeader));
        QCOMPARE(libraryHeader, QByteArrayLiteral("hdr"));

        // regularPayload：普通载荷数据。
        QByteArray regularPayload;
        QVERIFY(readCfbStream(filePath, QStringLiteral("Component/Data"), regularPayload));
        QCOMPARE(regularPayload, regularData);

        // smallPayload：小载荷数据。
        QByteArray smallPayload;
        QVERIFY(readCfbStream(filePath, QStringLiteral("Small"), smallPayload));
        QCOMPARE(smallPayload, QByteArrayLiteral("small stream"));
    }

    /**
     * @brief 验证大文件（>109 扇区）正确生成 DIFAT 扩展扇区
     * @details 写入 8MB 数据，验证 DIFAT 链和往返一致性
     */
    void writesDifatForLargeCompoundFiles() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        QByteArray payload(8 * 1024 * 1024, 0);
        for (int i = 0; i < payload.size(); ++i) {
            payload[i] = static_cast<char>((i * 17 + 3) & 0xFF);
        }
        // writer：负责写入目标格式的 writer 实例。
        OLECompoundWriter writer;
        QVERIFY(writer.create());
        QVERIFY(writer.writeStream(QStringLiteral("LargeModel"), payload));
        // filePath：测试使用的文件系统路径。
        const QString filePath = QDir(tempDir.path()).filePath(QStringLiteral("large.cfb"));
        QVERIFY(writer.saveToFile(filePath));

        QFile file(filePath);
        QVERIFY(file.open(QIODevice::ReadOnly));
        // header：测试用文件头。
        const QByteArray header = file.read(512);
        QVERIFY(readU32(header, 44) > 109);
        QVERIFY(readU32(header, 72) > 0);
        QCOMPARE(readU32(readCfbSector(header + file.readAll(), readU32(header, 68)), 127 * 4), CFB_ENDOFCHAIN);

        // roundTrip：回读后的等价数据。
        QByteArray roundTrip;
        QVERIFY(readCfbStream(filePath, QStringLiteral("LargeModel"), roundTrip));
        QCOMPARE(roundTrip, payload);

        // reader：负责回读生成内容的 reader 实例。
        OLECompoundReader reader;
        QVERIFY(reader.open(filePath));
        QVERIFY(reader.containsStream(QStringLiteral("LargeModel")));
        QVERIFY(reader.readStream(QStringLiteral("LargeModel"), &roundTrip));
        QCOMPARE(roundTrip, payload);
    }

    /**
     * @brief 验证无效路径和重名条目被正确拒绝
     */
    void rejectsInvalidOrDuplicateCfbEntries() {
        // writer：负责写入目标格式的 writer 实例。
        OLECompoundWriter writer;
        QVERIFY(writer.create());
        QVERIFY(!writer.addStorage(QStringLiteral("Missing"), QStringLiteral("Child")));
        QVERIFY(writer.hasError());
        QCOMPARE(writer.errorString(), QStringLiteral("无法创建存储: Missing/Child"));
        QVERIFY(!writer.saveToFile(QDir::temp().filePath(QStringLiteral("invalid-cfb.cfb"))));

        QVERIFY(writer.create());
        QVERIFY(!writer.hasError());
        QVERIFY(!writer.addStorage(QStringLiteral("Bad/Name")));
        QVERIFY(writer.addStorage(QStringLiteral("Models")));
        QVERIFY(!writer.addStorage(QStringLiteral("models")));
        QVERIFY(writer.writeStream(QStringLiteral("Header"), QByteArrayLiteral("one")));
        QVERIFY(!writer.writeStream(QStringLiteral("header"), QByteArrayLiteral("two")));
        QVERIFY(!writer.writeStream(QStringLiteral("Missing"), QStringLiteral("Data"), QByteArray()));
    }

    /**
     * @brief 验证 SchLib 写入器拒绝无效入口参数并清理旧诊断
     */
    void rejectsInvalidSchLibWriterInputs() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // writer：负责写入目标格式的 writer 实例。
        AltiumSchLibWriter writer;
        // outputPath：测试使用的文件系统路径。
        const QString outputPath = QDir(tempDir.path()).filePath(QStringLiteral("invalid-input.SchLib"));
        QVERIFY(!writer.write({}, outputPath));
        QVERIFY(writer.diagnostics().join('\n').contains(QStringLiteral("输入组件为空")));

        // unnamed：测试使用的元器件标识或名称。
        AltiumSchComponent unnamed;
        QVERIFY(!writer.write({unnamed}, outputPath));
        QVERIFY(writer.diagnostics().join('\n').contains(QStringLiteral("组件名称为空")));
        QVERIFY(!writer.diagnostics().join('\n').contains(QStringLiteral("输入组件为空")));

        // oversizedName：测试使用的元器件标识或名称。
        AltiumSchComponent oversizedName;
        oversizedName.name = QString(256, QChar('A'));
        QVERIFY(!writer.write({oversizedName}, outputPath));
        QVERIFY(writer.diagnostics().join('\n').contains(QStringLiteral("名称超过 255 字节")));

        // nonLatinName：测试使用的元器件标识或名称。
        AltiumSchComponent nonLatinName;
        nonLatinName.name = QStringLiteral("中文元件");
        QVERIFY(!writer.write({nonLatinName}, outputPath));
        QVERIFY(writer.diagnostics().join('\n').contains(QStringLiteral("名称包含无法编码的字符")));

        // oversizedPinText：测试使用的文本或数据内容。
        AltiumSchComponent oversizedPinText;
        oversizedPinText.name = QStringLiteral("OVERSIZED_PIN_TEXT");
        // oversizedPin：测试使用的数量或索引值。
        AltiumSchPin oversizedPin;
        oversizedPin.name = QString(256, QChar('N'));
        oversizedPinText.pins.append(oversizedPin);
        QVERIFY(!writer.write({oversizedPinText}, outputPath));
        QVERIFY(writer.diagnostics().join('\n').contains(QStringLiteral("引脚 0 名称超过 255 字节")));

        // nonLatinPinText：测试使用的文本或数据内容。
        AltiumSchComponent nonLatinPinText;
        nonLatinPinText.name = QStringLiteral("NON_LATIN_PIN");
        // nonLatinPin：测试用引脚数据。
        AltiumSchPin nonLatinPin;
        nonLatinPin.name = QStringLiteral("中文引脚");
        nonLatinPinText.pins.append(nonLatinPin);
        QVERIFY(!writer.write({nonLatinPinText}, outputPath));
        QVERIFY(writer.diagnostics().join('\n').contains(QStringLiteral("引脚 0 名称包含无法编码的字符")));

        // invalidPinType：测试使用的元器件标识或名称。
        AltiumSchComponent invalidPinType;
        invalidPinType.name = QStringLiteral("INVALID_PIN_TYPE");
        // invalidPin：测试使用的元器件标识或名称。
        AltiumSchPin invalidPin;
        invalidPin.orientation = static_cast<AltiumModels::PinOrientation>(4);
        invalidPinType.pins.append(invalidPin);
        QVERIFY(!writer.write({invalidPinType}, outputPath));
        QVERIFY(writer.diagnostics().join('\n').contains(QStringLiteral("引脚 0 方向无效")));

        // invalidImageBounds：测试使用的元器件标识或名称。
        AltiumSchComponent invalidImageBounds;
        invalidImageBounds.name = QStringLiteral("INVALID_IMAGE_BOUNDS");
        // invalidImage：测试使用的元器件标识或名称。
        AltiumSchImage invalidImage;
        invalidImage.fileName = QStringLiteral("image.png");
        invalidImage.locationX = 100;
        invalidImage.cornerX = 100;
        invalidImage.cornerY = 100;
        invalidImageBounds.images.append(invalidImage);
        QVERIFY(!writer.write({invalidImageBounds}, outputPath));
        QVERIFY(writer.diagnostics().join('\n').contains(QStringLiteral("图片边界尺寸无效")));

        // named：测试使用的元器件标识或名称。
        AltiumSchComponent named;
        named.name = QStringLiteral("VALID");
        QVERIFY(!writer.write({named}, QString()));
        QVERIFY(writer.diagnostics().join('\n').contains(QStringLiteral("输出路径为空")));
        QVERIFY(!writer.diagnostics().join('\n').contains(QStringLiteral("组件名称为空")));

        // duplicateName：测试使用的元器件标识或名称。
        AltiumSchComponent duplicateName;
        duplicateName.name = QStringLiteral("valid");
        QVERIFY(!writer.write({named, duplicateName}, outputPath));
        QVERIFY(writer.diagnostics().join('\n').contains(QStringLiteral("组件名称重复（不区分大小写）")));

        // invalidPartCount：测试使用的元器件标识或名称。
        AltiumSchComponent invalidPartCount;
        invalidPartCount.name = QStringLiteral("INVALID_PART_COUNT");
        invalidPartCount.partCount = 0;
        // invalidFontSizeText：测试使用的元器件标识或名称。
        AltiumSchText invalidFontSizeText;
        invalidFontSizeText.text = QStringLiteral("invalid font size");
        invalidFontSizeText.fontName = QStringLiteral("Arial");
        invalidFontSizeText.fontSizeMm = std::numeric_limits<double>::infinity();
        invalidFontSizeText.anchor = QStringLiteral("unsupported");
        invalidPartCount.texts.append(invalidFontSizeText);
        // emptyText：测试使用的文本或数据内容。
        AltiumSchText emptyText;
        invalidPartCount.texts.append(emptyText);
        // invalidOwnerPin：测试使用的元器件标识或名称。
        AltiumSchPin invalidOwnerPin;
        invalidOwnerPin.name = QStringLiteral("A");
        invalidOwnerPin.designator = QStringLiteral("1");
        invalidOwnerPin.ownerPartId = 0;
        invalidPartCount.pins.append(invalidOwnerPin);
        // invalidOwnerRectangle：测试使用的元器件标识或名称。
        AltiumSchRectangle invalidOwnerRectangle;
        invalidOwnerRectangle.cornerX = 100000;
        invalidOwnerRectangle.cornerY = 100000;
        invalidOwnerRectangle.ownerPartId = 0;
        invalidPartCount.rectangles.append(invalidOwnerRectangle);
        // invalidArc：测试使用的元器件标识或名称。
        AltiumSchArc invalidArc;
        invalidArc.radius = 100000;
        invalidArc.startAngle = std::numeric_limits<double>::quiet_NaN();
        invalidArc.endAngle = std::numeric_limits<double>::infinity();
        invalidPartCount.arcs.append(invalidArc);
        // invalidPie：测试使用的元器件标识或名称。
        AltiumSchPie invalidPie;
        invalidPie.radius = 100000;
        invalidPie.startAngle = std::numeric_limits<double>::infinity();
        invalidPartCount.pies.append(invalidPie);
        // invalidEllipticalArc：测试使用的元器件标识或名称。
        AltiumSchEllipticalArc invalidEllipticalArc;
        invalidEllipticalArc.radiusX = 100000;
        invalidEllipticalArc.radiusY = 50000;
        invalidEllipticalArc.endAngle = std::numeric_limits<double>::quiet_NaN();
        invalidPartCount.ellipticalArcs.append(invalidEllipticalArc);
        // emptyTextFrame：测试使用的文本或数据内容。
        AltiumSchTextFrame emptyTextFrame;
        emptyTextFrame.cornerX = 100000;
        emptyTextFrame.cornerY = 100000;
        invalidPartCount.textFrames.append(emptyTextFrame);
        // invalidFontSizeTextFrame：测试使用的元器件标识或名称。
        AltiumSchTextFrame invalidFontSizeTextFrame;
        invalidFontSizeTextFrame.text = QStringLiteral("invalid frame font size");
        invalidFontSizeTextFrame.cornerX = 100000;
        invalidFontSizeTextFrame.cornerY = 100000;
        invalidFontSizeTextFrame.fontName = QStringLiteral("Arial");
        invalidFontSizeTextFrame.fontSizeMm = std::numeric_limits<double>::infinity();
        invalidPartCount.textFrames.append(invalidFontSizeTextFrame);
        QVERIFY(writer.write({invalidPartCount}, outputPath));
        QVERIFY(writer.diagnostics().join('\n').contains(QStringLiteral("partCount 无效")));
        QVERIFY(writer.diagnostics().join('\n').contains(QStringLiteral("引脚 OWNERPARTID=0 无效")));
        QVERIFY(writer.diagnostics().join('\n').contains(QStringLiteral("图元 OWNERPARTID=0 无效")));
        QVERIFY(writer.diagnostics().join('\n').contains(QStringLiteral("文本字体大小无效")));
        QVERIFY(writer.diagnostics().join('\n').contains(QStringLiteral("文本框字体大小无效")));
        QVERIFY(writer.diagnostics().join('\n').contains(QStringLiteral("文本对齐锚点无效")));
        QVERIFY(writer.diagnostics().join('\n').contains(QStringLiteral("文本内容为空")));
        QVERIFY(writer.diagnostics().join('\n').contains(QStringLiteral("文本框内容为空")));
        QVERIFY(writer.diagnostics().join('\n').contains(QStringLiteral("圆弧起始角度无效")));
        QVERIFY(writer.diagnostics().join('\n').contains(QStringLiteral("圆弧结束角度无效")));
        QVERIFY(writer.diagnostics().join('\n').contains(QStringLiteral("扇形起始角度无效")));
        QVERIFY(writer.diagnostics().join('\n').contains(QStringLiteral("椭圆弧结束角度无效")));

        // invalidTextMargin：测试使用的元器件标识或名称。
        AltiumSchComponent invalidTextMargin;
        invalidTextMargin.name = QStringLiteral("INVALID_TEXT_MARGIN");
        // negativeTextMargin：测试使用的文本或数据内容。
        AltiumSchTextFrame negativeTextMargin;
        negativeTextMargin.cornerX = 100000;
        negativeTextMargin.cornerY = 100000;
        negativeTextMargin.textMargin = -1;
        invalidTextMargin.textFrames.append(negativeTextMargin);
        QVERIFY(!writer.write({invalidTextMargin}, outputPath));
        QVERIFY(writer.diagnostics().join('\n').contains(QStringLiteral("文本框文本边距无效")));

        // invalidTextFrameBounds：测试使用的元器件标识或名称。
        AltiumSchComponent invalidTextFrameBounds;
        invalidTextFrameBounds.name = QStringLiteral("INVALID_TEXT_FRAME_BOUNDS");
        // zeroTextFrame：测试使用的文本或数据内容。
        AltiumSchTextFrame zeroTextFrame;
        zeroTextFrame.cornerX = 100000;
        invalidTextFrameBounds.textFrames.append(zeroTextFrame);
        QVERIFY(!writer.write({invalidTextFrameBounds}, outputPath));
        QVERIFY(writer.diagnostics().join('\n').contains(QStringLiteral("文本框边界尺寸无效")));

        // invalidRectangleBounds：测试使用的元器件标识或名称。
        AltiumSchComponent invalidRectangleBounds;
        invalidRectangleBounds.name = QStringLiteral("INVALID_RECTANGLE_BOUNDS");
        // zeroRectangle：测试用矩形图元。
        AltiumSchRectangle zeroRectangle;
        zeroRectangle.cornerX = 100000;
        invalidRectangleBounds.rectangles.append(zeroRectangle);
        QVERIFY(!writer.write({invalidRectangleBounds}, outputPath));
        QVERIFY(writer.diagnostics().join('\n').contains(QStringLiteral("矩形图元边界尺寸无效")));

        // invalidRoundRectangleBounds：测试使用的元器件标识或名称。
        AltiumSchComponent invalidRoundRectangleBounds;
        invalidRoundRectangleBounds.name = QStringLiteral("INVALID_ROUND_RECTANGLE_BOUNDS");
        // zeroRoundRectangle：测试用矩形图元。
        AltiumSchRoundRectangle zeroRoundRectangle;
        zeroRoundRectangle.cornerY = 100000;
        invalidRoundRectangleBounds.roundRectangles.append(zeroRoundRectangle);
        QVERIFY(!writer.write({invalidRoundRectangleBounds}, outputPath));
        QVERIFY(writer.diagnostics().join('\n').contains(QStringLiteral("圆角矩形图元边界尺寸无效")));

        // invalidLineGeometry：测试使用的元器件标识或名称。
        AltiumSchComponent invalidLineGeometry;
        invalidLineGeometry.name = QStringLiteral("INVALID_LINE_GEOMETRY");
        invalidLineGeometry.lines.append(AltiumSchLine{});
        QVERIFY(!writer.write({invalidLineGeometry}, outputPath));
        QVERIFY(writer.diagnostics().join('\n').contains(QStringLiteral("线段图元长度无效")));

        // invalidStroke：测试使用的元器件标识或名称。
        AltiumSchComponent invalidStroke;
        invalidStroke.name = QStringLiteral("INVALID_STROKE_WIDTH");
        // invalidStrokeRectangle：测试使用的元器件标识或名称。
        AltiumSchRectangle invalidStrokeRectangle;
        invalidStrokeRectangle.cornerX = 100000;
        invalidStrokeRectangle.cornerY = 100000;
        invalidStrokeRectangle.lineWidth = 4;
        invalidStroke.rectangles.append(invalidStrokeRectangle);
        QVERIFY(!writer.write({invalidStroke}, outputPath));
        QVERIFY(writer.diagnostics().join('\n').contains(QStringLiteral("矩形图元线宽索引无效: 4")));

        invalidStroke.name = QStringLiteral("INVALID_STROKE_STYLE");
        invalidStrokeRectangle.lineWidth = 0;
        invalidStrokeRectangle.lineStyle = 3;
        invalidStroke.rectangles = {invalidStrokeRectangle};
        QVERIFY(!writer.write({invalidStroke}, outputPath));
        QVERIFY(writer.diagnostics().join('\n').contains(QStringLiteral("矩形图元线型无效: 3")));

        // invalidOrientation：测试使用的元器件标识或名称。
        AltiumSchComponent invalidOrientation;
        invalidOrientation.name = QStringLiteral("INVALID_ORIENTATION");
        // invalidOrientationText：测试使用的元器件标识或名称。
        AltiumSchText invalidOrientationText;
        invalidOrientationText.text = QStringLiteral("invalid orientation");
        invalidOrientationText.orientation = 4;
        invalidOrientation.texts.append(invalidOrientationText);
        QVERIFY(!writer.write({invalidOrientation}, outputPath));
        QVERIFY(writer.diagnostics().join('\n').contains(QStringLiteral("文本图元方向无效: 4")));

        // invalidIeee：测试使用的元器件标识或名称。
        AltiumSchComponent invalidIeee;
        invalidIeee.name = QStringLiteral("INVALID_IEEE");
        // invalidIeeeGraphic：测试使用的元器件标识或名称。
        AltiumSchIeee invalidIeeeGraphic;
        invalidIeeeGraphic.symbol = 35;
        invalidIeee.ieeeSymbols.append(invalidIeeeGraphic);
        QVERIFY(!writer.write({invalidIeee}, outputPath));
        QVERIFY(writer.diagnostics().join('\n').contains(QStringLiteral("IEEE 图元符号编号无效: 35")));

        invalidIeeeGraphic.symbol = 0;
        invalidIeeeGraphic.scaleFactor = 0;
        invalidIeee.ieeeSymbols = {invalidIeeeGraphic};
        QVERIFY(!writer.write({invalidIeee}, outputPath));
        QVERIFY(writer.diagnostics().join('\n').contains(QStringLiteral("IEEE 图元缩放因子无效: 0")));

        invalidIeeeGraphic.scaleFactor = 1;
        invalidIeeeGraphic.lineWidth = 4;
        invalidIeee.ieeeSymbols = {invalidIeeeGraphic};
        QVERIFY(!writer.write({invalidIeee}, outputPath));
        QVERIFY(writer.diagnostics().join('\n').contains(QStringLiteral("IEEE 图元线宽索引无效: 4")));

        // oversizedPartCount：测试使用的元器件标识或名称。
        AltiumSchComponent oversizedPartCount;
        oversizedPartCount.name = QStringLiteral("OVERSIZED_PART_COUNT");
        oversizedPartCount.partCount = std::numeric_limits<int>::max();
        QVERIFY(!writer.write({oversizedPartCount}, outputPath));
        QVERIFY(writer.diagnostics().join('\n').contains(QStringLiteral("partCount 超出支持范围")));

        // header：测试用文件头。
        QByteArray header;
        QVERIFY(readCfbStream(outputPath, QStringLiteral("FileHeader"), header));
        QVERIFY(header.contains("PARTCOUNT0=2"));

        // emptyImageReader：负责回读生成内容的 reader 实例。
        AltiumSchLibReader emptyImageReader;
        QVERIFY2(emptyImageReader.open(outputPath), qPrintable(emptyImageReader.errorString()));
        // emptyImageEntries：测试解析得到的条目集合。
        QVector<AltiumSchLibReader::ImageStorageEntry> emptyImageEntries;
        QVERIFY2(emptyImageReader.readImageStorage(&emptyImageEntries), qPrintable(emptyImageReader.errorString()));
        QVERIFY(emptyImageEntries.isEmpty());

        // invalidOwnerRecords：测试使用的元器件标识或名称。
        QVector<AltiumSchLibReader::Record> invalidOwnerRecords;
        QVERIFY2(emptyImageReader.readComponentRecords(QStringLiteral("INVALID_PART_COUNT"), &invalidOwnerRecords),
                 qPrintable(emptyImageReader.errorString()));
        // invalidWeightOffset：测试使用的元器件标识或名称。
        const int invalidWeightOffset = header.indexOf("WEIGHT=");
        // invalidWeightEnd：测试使用的元器件标识或名称。
        const int invalidWeightEnd = header.indexOf('|', invalidWeightOffset);
        QVERIFY(invalidWeightOffset >= 0);
        QVERIFY(invalidWeightEnd > invalidWeightOffset);
        // invalidWeightOk：测试使用的元器件标识或名称。
        bool invalidWeightOk = false;
        const int invalidHeaderWeight =
            QString::fromLatin1(header.mid(invalidWeightOffset + 7, invalidWeightEnd - invalidWeightOffset - 7))
                .toInt(&invalidWeightOk);
        QVERIFY(invalidWeightOk);
        QCOMPARE(invalidOwnerRecords.size(), invalidHeaderWeight);
        for (const auto& record : invalidOwnerRecords) {
            if (record.recordType == 2 || record.recordType == 9)
                QCOMPARE(record.ownerPartId, 1);
            if (record.recordType == 4 && record.parameters.value(QStringLiteral("Text")) == "invalid font size")
                QCOMPARE(record.parameters.value(QStringLiteral("TextAnchor")), QStringLiteral("middle"));
        }

        // invalidGeometry：测试使用的元器件标识或名称。
        AltiumSchComponent invalidGeometry;
        invalidGeometry.name = QStringLiteral("INVALID_GEOMETRY");
        // invalidRadiusArc：测试使用的元器件标识或名称。
        AltiumSchArc invalidRadiusArc;
        invalidGeometry.arcs.append(invalidRadiusArc);
        QVERIFY(!writer.write({invalidGeometry}, outputPath));
        QVERIFY(writer.diagnostics().join('\n').contains(QStringLiteral("圆弧半径无效")));

        // invalidImageGeometry：测试使用的元器件标识或名称。
        AltiumSchComponent invalidImageGeometry;
        invalidImageGeometry.name = QStringLiteral("INVALID_IMAGE_GEOMETRY");
        // invalidRotationImage：测试使用的元器件标识或名称。
        AltiumSchImage invalidRotationImage;
        invalidRotationImage.rotation = std::numeric_limits<double>::quiet_NaN();
        invalidImageGeometry.images.append(invalidRotationImage);
        QVERIFY(!writer.write({invalidImageGeometry}, outputPath));
        QVERIFY(writer.diagnostics().join('\n').contains(QStringLiteral("图片旋转角度无效")));

        // invalidPolygon：测试使用的元器件标识或名称。
        AltiumSchComponent invalidPolygon;
        invalidPolygon.name = QStringLiteral("INVALID_POLYGON");
        // invalidPolygonData：测试使用的元器件标识或名称。
        AltiumSchPolygon invalidPolygonData;
        invalidPolygonData.vertices = {QPointF(0, 0), QPointF(100000, 0)};
        invalidPolygon.polygons.append(invalidPolygonData);
        QVERIFY(!writer.write({invalidPolygon}, outputPath));
        QVERIFY(writer.diagnostics().join('\n').contains(QStringLiteral("多边形顶点数量不足")));

        // invalidPartOwner：测试使用的元器件标识或名称。
        AltiumSchComponent invalidPartOwner;
        invalidPartOwner.name = QStringLiteral("INVALID_PART_OWNER");
        invalidPartOwner.partCount = 2;
        // outOfRangeRectangle：测试用矩形图元。
        AltiumSchRectangle outOfRangeRectangle;
        outOfRangeRectangle.cornerX = 100000;
        outOfRangeRectangle.cornerY = 100000;
        outOfRangeRectangle.ownerPartId = 3;
        invalidPartOwner.rectangles.append(outOfRangeRectangle);
        QVERIFY(!writer.write({invalidPartOwner}, outputPath));
        QVERIFY(writer.diagnostics().join('\n').contains(QStringLiteral("矩形图元 OWNERPARTID=3 超出部件范围")));

        // invalidCommonOwner：测试使用的元器件标识或名称。
        AltiumSchComponent invalidCommonOwner;
        invalidCommonOwner.name = QStringLiteral("INVALID_COMMON_OWNER");
        // invalidCommonRectangle：测试使用的元器件标识或名称。
        AltiumSchRectangle invalidCommonRectangle;
        invalidCommonRectangle.cornerX = 100000;
        invalidCommonRectangle.cornerY = 100000;
        invalidCommonRectangle.ownerPartId = -2;
        invalidCommonOwner.rectangles.append(invalidCommonRectangle);
        QVERIFY(!writer.write({invalidCommonOwner}, outputPath));
        QVERIFY(writer.diagnostics().join('\n').contains(QStringLiteral("矩形图元 OWNERPARTID=-2 超出部件范围")));

        // invalidImplementationParameter：测试使用的元器件标识或名称。
        AltiumSchComponent invalidImplementationParameter;
        invalidImplementationParameter.name = QStringLiteral("INVALID_IMPLEMENTATION_PARAMETER");
        // invalidImplementation：测试使用的元器件标识或名称。
        AltiumSchComponent::Implementation invalidImplementation;
        invalidImplementation.modelName = QStringLiteral("MODEL");
        invalidImplementation.parameters.insert(QStringLiteral("BAD|KEY"), QStringLiteral("value"));
        invalidImplementationParameter.implementations.append(invalidImplementation);
        QVERIFY(!writer.write({invalidImplementationParameter}, outputPath));
        QVERIFY(writer.diagnostics().join('\n').contains(QStringLiteral("实现参数键无效")));

        // invalidSourceMetadata：测试使用的元器件标识或名称。
        AltiumSchComponent invalidSourceMetadata;
        invalidSourceMetadata.name = QStringLiteral("INVALID_SOURCE_METADATA");
        invalidSourceMetadata.sourceMetadata.insert(QStringLiteral("BAD|KEY"), QStringLiteral("value"));
        QVERIFY(!writer.write({invalidSourceMetadata}, outputPath));
        QVERIFY(writer.diagnostics().join('\n').contains(QStringLiteral("源元数据参数名无效")));

        // invalidParameterName：测试使用的元器件标识或名称。
        AltiumSchComponent invalidParameterName;
        invalidParameterName.name = QStringLiteral("INVALID_PARAMETER_NAME");
        // invalidParameter：测试使用的元器件标识或名称。
        AltiumSchParameter invalidParameter;
        invalidParameter.name = QStringLiteral("BAD|NAME");
        invalidParameter.value = QStringLiteral("value");
        invalidParameterName.parameters.append(invalidParameter);
        QVERIFY(!writer.write({invalidParameterName}, outputPath));
        QVERIFY(writer.diagnostics().join('\n').contains(QStringLiteral("参数参数名无效")));

        // invalidParameterValue：测试使用的元器件标识或名称。
        AltiumSchComponent invalidParameterValue;
        invalidParameterValue.name = QStringLiteral("INVALID_PARAMETER_VALUE");
        // invalidTextValue：测试使用的元器件标识或名称。
        AltiumSchText invalidTextValue;
        invalidTextValue.text = QStringLiteral("BAD|TEXT");
        invalidParameterValue.texts.append(invalidTextValue);
        QVERIFY(!writer.write({invalidParameterValue}, outputPath));
        QVERIFY(writer.diagnostics().join('\n').contains(QStringLiteral("文本内容包含参数分隔符或 NUL")));

        // invalidPcb：测试使用的元器件标识或名称。
        AltiumPcbComponent invalidPcb;
        invalidPcb.name = QStringLiteral("INVALID_PCB_FLOATS");
        // invalidPad：测试使用的元器件标识或名称。
        AltiumPcbPad invalidPad;
        invalidPad.sizeTopX = invalidPad.sizeTopY = 1000;
        invalidPad.sizeMidX = invalidPad.sizeMidY = 1000;
        invalidPad.sizeBotX = invalidPad.sizeBotY = 1000;
        invalidPad.holeSize = 1000;
        invalidPad.rotation = std::numeric_limits<double>::quiet_NaN();
        invalidPad.holeRotation = std::numeric_limits<double>::infinity();
        invalidPcb.pads.append(invalidPad);
        // invalidPcbArc：测试使用的元器件标识或名称。
        AltiumPcbArc invalidPcbArc;
        invalidPcbArc.radius = 1000;
        invalidPcbArc.width = 1000;
        invalidPcbArc.startAngle = std::numeric_limits<double>::infinity();
        invalidPcbArc.endAngle = std::numeric_limits<double>::quiet_NaN();
        invalidPcb.arcs.append(invalidPcbArc);
        // invalidPcbText：测试使用的元器件标识或名称。
        AltiumPcbText invalidPcbText;
        invalidPcbText.rotation = std::numeric_limits<double>::quiet_NaN();
        invalidPcb.texts.append(invalidPcbText);
        // invalidPcbFill：测试使用的元器件标识或名称。
        AltiumPcbFill invalidPcbFill;
        invalidPcbFill.rotation = std::numeric_limits<double>::infinity();
        invalidPcb.fills.append(invalidPcbFill);
        // invalidPcbRegion：测试使用的元器件标识或名称。
        AltiumPcbRegion invalidPcbRegion;
        invalidPcbRegion.vertices.append(QPointF(std::numeric_limits<double>::quiet_NaN(), 0.0));
        invalidPcbRegion.vertices.append(QPointF(1000.0, 0.0));
        invalidPcbRegion.vertices.append(QPointF(0.0, 1000.0));
        invalidPcb.regions.append(invalidPcbRegion);
        // invalidBody：测试使用的元器件标识或名称。
        AltiumPcbComponentBody invalidBody;
        invalidBody.bodyOpacity3d = std::numeric_limits<double>::infinity();
        invalidBody.model2dRotation = std::numeric_limits<double>::quiet_NaN();
        invalidBody.outline.append(QPointF(0.0, std::numeric_limits<double>::quiet_NaN()));
        invalidPcb.bodies.append(invalidBody);
        // invalidModel：测试使用的元器件标识或名称。
        AltiumPcbComponent::Model3D invalidModel;
        invalidModel.name = QStringLiteral("invalid.step");
        invalidModel.stepData = QByteArrayLiteral("ISO-10303-21;");
        invalidModel.rotX = std::numeric_limits<double>::infinity();
        invalidModel.rotY = std::numeric_limits<double>::quiet_NaN();
        invalidModel.rotZ = std::numeric_limits<double>::infinity();
        invalidModel.dz = std::numeric_limits<double>::quiet_NaN();
        invalidPcb.models.append(invalidModel);
        invalidPcb.bodies[0].modelId = invalidModel.id.isEmpty() ? invalidModel.name : invalidModel.id;
        // pcbWriter：负责写入目标格式的 writer 实例。
        AltiumPcbLibWriter pcbWriter;
        // pcbOutputPath：测试使用的文件系统路径。
        const QString pcbOutputPath = QDir(tempDir.path()).filePath(QStringLiteral("invalid-floats.PcbLib"));
        QVERIFY(pcbWriter.write({invalidPcb}, pcbOutputPath));
        QVERIFY(pcbWriter.diagnostics().join('\n').contains(QStringLiteral("焊盘旋转角度无效")));
        QVERIFY(pcbWriter.diagnostics().join('\n').contains(QStringLiteral("焊盘孔旋转角度无效")));
        QVERIFY(pcbWriter.diagnostics().join('\n').contains(QStringLiteral("PCB 弧线起始角度无效")));
        QVERIFY(pcbWriter.diagnostics().join('\n').contains(QStringLiteral("PCB 弧线结束角度无效")));
        QVERIFY(pcbWriter.diagnostics().join('\n').contains(QStringLiteral("3D 元件体不透明度无效")));
        QVERIFY(pcbWriter.diagnostics().join('\n').contains(QStringLiteral("3D 元件体二维旋转无效")));
        QVERIFY(pcbWriter.diagnostics().join('\n').contains(QStringLiteral("3D 模型 X 旋转无效")));
        QVERIFY(pcbWriter.diagnostics().join('\n').contains(QStringLiteral("3D 模型 Y 旋转无效")));
        QVERIFY(pcbWriter.diagnostics().join('\n').contains(QStringLiteral("3D 模型 Z 旋转无效")));
        QVERIFY(pcbWriter.diagnostics().join('\n').contains(QStringLiteral("3D 模型 Z 偏移无效")));

        // invalidFootprint：测试使用的元器件标识或名称。
        IR::FootprintComponentIR invalidFootprint;
        invalidFootprint.name = QStringLiteral("INVALID_FOOTPRINT_FLOATS");
        // invalidFootprintPad：测试使用的元器件标识或名称。
        IR::FootprintPadIR invalidFootprintPad;
        invalidFootprintPad.number = QStringLiteral("1");
        invalidFootprintPad.size = QSizeF(1.0, 1.0);
        invalidFootprintPad.rotation = std::numeric_limits<double>::quiet_NaN();
        invalidFootprint.pads.append(invalidFootprintPad);
        // footprintExporter：负责生成测试输出的导出器实例。
        ExporterAltiumFootprint footprintExporter;
        // footprintOutputPath：测试使用的文件系统路径。
        const QString footprintOutputPath = QDir(tempDir.path()).filePath(QStringLiteral("invalid-footprint.PcbLib"));
        QVERIFY(footprintExporter.exportFootprint(invalidFootprint, footprintOutputPath));
        QVERIFY(footprintExporter.diagnostics().join('\n').contains(QStringLiteral("焊盘旋转角度无效")));

        // invalidInputWriter：测试使用的元器件标识或名称。
        AltiumPcbLibWriter invalidInputWriter;
        // unnamedPcb：测试使用的元器件标识或名称。
        AltiumPcbComponent unnamedPcb;
        QVERIFY(!invalidInputWriter.write({unnamedPcb}, pcbOutputPath));
        QVERIFY(invalidInputWriter.diagnostics().join('\n').contains(QStringLiteral("封装名称为空")));
        // invalidHeightPcb：测试使用的元器件标识或名称。
        AltiumPcbComponent invalidHeightPcb;
        invalidHeightPcb.name = QStringLiteral("INVALID_HEIGHT");
        invalidHeightPcb.height = -1.0;
        QVERIFY(!invalidInputWriter.write({invalidHeightPcb}, pcbOutputPath));
        QVERIFY(invalidInputWriter.diagnostics().join('\n').contains(QStringLiteral("高度必须为非负有限值")));
        QVERIFY(!invalidInputWriter.write({unnamedPcb}, pcbOutputPath + QStringLiteral("|invalid")));
        QVERIFY(
            invalidInputWriter.diagnostics().join('\n').contains(QStringLiteral("输出路径包含参数分隔符、换行或 NUL")));
        // oversizedPcbName：测试使用的元器件标识或名称。
        AltiumPcbComponent oversizedPcbName;
        oversizedPcbName.name = QString(256, QChar('A'));
        QVERIFY(!invalidInputWriter.write({oversizedPcbName}, pcbOutputPath));
        QVERIFY(invalidInputWriter.diagnostics().join('\n').contains(QStringLiteral("名称超过 255 字节")));
        // nonLatinPcbName：测试使用的元器件标识或名称。
        AltiumPcbComponent nonLatinPcbName;
        nonLatinPcbName.name = QStringLiteral("中文封装");
        QVERIFY(!invalidInputWriter.write({nonLatinPcbName}, pcbOutputPath));
        QVERIFY(invalidInputWriter.diagnostics().join('\n').contains(QStringLiteral("名称包含无法编码的字符")));
        // invalidPadPcb：测试使用的元器件标识或名称。
        AltiumPcbComponent invalidPadPcb;
        invalidPadPcb.name = QStringLiteral("INVALID_PAD_SIZE");
        invalidPadPcb.pads.append(AltiumPcbPad());
        QVERIFY(!invalidInputWriter.write({invalidPadPcb}, pcbOutputPath));
        QVERIFY(invalidInputWriter.diagnostics().join('\n').contains(QStringLiteral("非正焊盘尺寸")));
        // oversizedPadDesignator：测试使用的数量或索引值。
        AltiumPcbComponent oversizedPadDesignator;
        oversizedPadDesignator.name = QStringLiteral("OVERSIZED_PAD_DESIGNATOR");
        // oversizedPad：测试使用的数量或索引值。
        AltiumPcbPad oversizedPad;
        oversizedPad.designator = QString(256, QChar('A'));
        oversizedPadDesignator.pads.append(oversizedPad);
        QVERIFY(!invalidInputWriter.write({oversizedPadDesignator}, pcbOutputPath));
        QVERIFY(invalidInputWriter.diagnostics().join('\n').contains(QStringLiteral("焊盘编号超过 255 字节")));
        // nonLatinPadDesignator：测试用焊盘数据。
        AltiumPcbComponent nonLatinPadDesignator;
        nonLatinPadDesignator.name = QStringLiteral("NON_LATIN_PAD");
        // nonLatinPad：测试用焊盘数据。
        AltiumPcbPad nonLatinPad;
        nonLatinPad.designator = QStringLiteral("中文焊盘");
        nonLatinPadDesignator.pads.append(nonLatinPad);
        QVERIFY(!invalidInputWriter.write({nonLatinPadDesignator}, pcbOutputPath));
        QVERIFY(invalidInputWriter.diagnostics().join('\n').contains(QStringLiteral("焊盘编号包含无法编码的字符")));
        // oversizedPcbText：测试使用的文本或数据内容。
        AltiumPcbComponent oversizedPcbText;
        oversizedPcbText.name = QStringLiteral("OVERSIZED_PCB_TEXT");
        // oversizedText：测试使用的文本或数据内容。
        AltiumPcbText oversizedText;
        oversizedText.text = QString(256, QChar('A'));
        oversizedPcbText.texts.append(oversizedText);
        QVERIFY(!invalidInputWriter.write({oversizedPcbText}, pcbOutputPath));
        QVERIFY(invalidInputWriter.diagnostics().join('\n').contains(QStringLiteral("文本内容超过 255 字节")));
        // nonLatinPcbText：测试使用的文本或数据内容。
        AltiumPcbComponent nonLatinPcbText;
        nonLatinPcbText.name = QStringLiteral("NON_LATIN_TEXT");
        // nonLatinText：测试使用的文本或数据内容。
        AltiumPcbText nonLatinText;
        nonLatinText.text = QStringLiteral("中文文本");
        nonLatinPcbText.texts.append(nonLatinText);
        QVERIFY(!invalidInputWriter.write({nonLatinPcbText}, pcbOutputPath));
        QVERIFY(invalidInputWriter.diagnostics().join('\n').contains(QStringLiteral("文本内容包含无法编码的字符")));
        // nonLatinModelComponent：测试使用的配置值或状态。
        AltiumPcbComponent nonLatinModelComponent;
        nonLatinModelComponent.name = QStringLiteral("NON_LATIN_MODEL");
        // nonLatinModel：测试使用的配置值或状态。
        AltiumPcbComponent::Model3D nonLatinModel;
        nonLatinModel.name = QStringLiteral("中文模型.step");
        nonLatinModel.stepData = QByteArrayLiteral("ISO-10303-21;");
        nonLatinModelComponent.models.append(nonLatinModel);
        QVERIFY(!invalidInputWriter.write({nonLatinModelComponent}, pcbOutputPath));
        QVERIFY(invalidInputWriter.diagnostics().join('\n').contains(QStringLiteral("3D 模型名称包含无法编码的字符")));
        // invalidLayerPcb：测试使用的元器件标识或名称。
        AltiumPcbComponent invalidLayerPcb;
        invalidLayerPcb.name = QStringLiteral("INVALID_LAYER");
        // invalidLayerTrack：测试使用的元器件标识或名称。
        AltiumPcbTrack invalidLayerTrack;
        invalidLayerTrack.layer = 0;
        invalidLayerPcb.tracks.append(invalidLayerTrack);
        QVERIFY(!invalidInputWriter.write({invalidLayerPcb}, pcbOutputPath));
        QVERIFY(invalidInputWriter.diagnostics().join('\n').contains(QStringLiteral("无效走线层号")));

        // invalidPadShapePcb：测试使用的元器件标识或名称。
        AltiumPcbComponent invalidPadShapePcb;
        invalidPadShapePcb.name = QStringLiteral("INVALID_PAD_SHAPE");
        // invalidPadShape：测试使用的元器件标识或名称。
        AltiumPcbPad invalidPadShape;
        invalidPadShape.sizeTopX = invalidPadShape.sizeTopY = 1000;
        invalidPadShape.sizeMidX = invalidPadShape.sizeMidY = 1000;
        invalidPadShape.sizeBotX = invalidPadShape.sizeBotY = 1000;
        invalidPadShape.holeSize = 1000;
        invalidPadShape.shapeTop = 4;
        invalidPadShapePcb.pads.append(invalidPadShape);
        QVERIFY(!invalidInputWriter.write({invalidPadShapePcb}, pcbOutputPath));
        QVERIFY(invalidInputWriter.diagnostics().join('\n').contains(QStringLiteral("无效焊盘形状")));

        // invalidTrackWidthPcb：测试使用的元器件标识或名称。
        AltiumPcbComponent invalidTrackWidthPcb;
        invalidTrackWidthPcb.name = QStringLiteral("INVALID_TRACK_WIDTH");
        // invalidTrackWidth：测试使用的元器件标识或名称。
        AltiumPcbTrack invalidTrackWidth;
        invalidTrackWidth.width = 0;
        invalidTrackWidthPcb.tracks.append(invalidTrackWidth);
        QVERIFY(!invalidInputWriter.write({invalidTrackWidthPcb}, pcbOutputPath));
        QVERIFY(invalidInputWriter.diagnostics().join('\n').contains(QStringLiteral("非正走线宽度")));

        // invalidHolePcb：测试使用的元器件标识或名称。
        AltiumPcbComponent invalidHolePcb;
        invalidHolePcb.name = QStringLiteral("INVALID_SLOT");
        // invalidSlot：测试使用的元器件标识或名称。
        AltiumPcbPad invalidSlot;
        invalidSlot.sizeTopX = invalidSlot.sizeTopY = 1000;
        invalidSlot.sizeMidX = invalidSlot.sizeMidY = 1000;
        invalidSlot.sizeBotX = invalidSlot.sizeBotY = 1000;
        invalidSlot.holeSize = 1000;
        invalidSlot.holeType = 2;
        invalidHolePcb.pads.append(invalidSlot);
        QVERIFY(!invalidInputWriter.write({invalidHolePcb}, pcbOutputPath));
        QVERIFY(invalidInputWriter.diagnostics().join('\n').contains(QStringLiteral("槽孔长度非正")));

        // duplicatePcbName：测试使用的元器件标识或名称。
        AltiumPcbComponent duplicatePcbName;
        duplicatePcbName.name = QStringLiteral("duplicate-pcb");
        // duplicatePcbNameCase：测试使用的元器件标识或名称。
        AltiumPcbComponent duplicatePcbNameCase;
        duplicatePcbNameCase.name = QStringLiteral("DUPLICATE-PCB");
        QVERIFY(!invalidInputWriter.write({duplicatePcbName, duplicatePcbNameCase}, pcbOutputPath));
        QVERIFY(invalidInputWriter.diagnostics().join('\n').contains(QStringLiteral("封装名称重复（不区分大小写）")));

        // invalidExtendedInfo：测试使用的元器件标识或名称。
        AltiumPcbComponent invalidExtendedInfo;
        invalidExtendedInfo.name = QStringLiteral("INVALID_EXTENDED_INFO");
        // extendedInfoPad：测试用焊盘数据。
        AltiumPcbPad extendedInfoPad;
        extendedInfoPad.sizeTopX = extendedInfoPad.sizeTopY = 1000;
        extendedInfoPad.sizeMidX = extendedInfoPad.sizeMidY = 1000;
        extendedInfoPad.sizeBotX = extendedInfoPad.sizeBotY = 1000;
        extendedInfoPad.holeSize = 1000;
        invalidExtendedInfo.pads.append(extendedInfoPad);
        // invalidExtendedEntry：测试使用的元器件标识或名称。
        AltiumPcbExtendedPrimitiveInfo invalidExtendedEntry;
        invalidExtendedEntry.primitiveIndex = 1;
        invalidExtendedEntry.objectName = QStringLiteral("Pad");
        invalidExtendedInfo.extendedPrimitives.append(invalidExtendedEntry);
        QVERIFY(!invalidInputWriter.write({invalidExtendedInfo}, pcbOutputPath));
        QVERIFY(invalidInputWriter.diagnostics().join('\n').contains(QStringLiteral("扩展图元索引无效")));

        // invalidModelData：测试使用的元器件标识或名称。
        AltiumPcbComponent invalidModelData;
        invalidModelData.name = QStringLiteral("INVALID_MODEL_DATA");
        // emptyModel：测试使用的配置值或状态。
        AltiumPcbComponent::Model3D emptyModel;
        emptyModel.name = QStringLiteral("empty.step");
        invalidModelData.models.append(emptyModel);
        QVERIFY(!invalidInputWriter.write({invalidModelData}, pcbOutputPath));
        QVERIFY(invalidInputWriter.diagnostics().join('\n').contains(QStringLiteral("3D 模型数据为空")));

        // duplicateModelIdComponent：测试使用的元器件标识或名称。
        AltiumPcbComponent duplicateModelIdComponent;
        duplicateModelIdComponent.name = QStringLiteral("DUPLICATE_MODEL_ID");
        // firstModel：测试使用的配置值或状态。
        AltiumPcbComponent::Model3D firstModel;
        firstModel.name = QStringLiteral("first.step");
        firstModel.id = QStringLiteral("model-id");
        firstModel.stepData = QByteArrayLiteral("ISO-10303-21;");
        // secondModel：测试使用的配置值或状态。
        AltiumPcbComponent::Model3D secondModel = firstModel;
        secondModel.name = QStringLiteral("second.step");
        secondModel.id = QStringLiteral("MODEL-ID");
        duplicateModelIdComponent.models = {firstModel, secondModel};
        QVERIFY(!invalidInputWriter.write({duplicateModelIdComponent}, pcbOutputPath));
        QVERIFY(invalidInputWriter.diagnostics().join('\n').contains(QStringLiteral("3D 模型 ID 重复")));

        // invalidModelIdComponent：测试使用的元器件标识或名称。
        AltiumPcbComponent invalidModelIdComponent;
        invalidModelIdComponent.name = QStringLiteral("INVALID_MODEL_ID");
        // invalidModelIdModel：测试使用的元器件标识或名称。
        AltiumPcbComponent::Model3D invalidModelIdModel = firstModel;
        invalidModelIdModel.id = QStringLiteral("   ");
        invalidModelIdComponent.models = {invalidModelIdModel};
        QVERIFY(!invalidInputWriter.write({invalidModelIdComponent}, pcbOutputPath));
        QVERIFY(invalidInputWriter.diagnostics().join('\n').contains(QStringLiteral("3D 模型 ID 规范化后为空")));

        invalidModelIdModel.id = QStringLiteral("bad|model-id");
        invalidModelIdComponent.models = {invalidModelIdModel};
        QVERIFY(!invalidInputWriter.write({invalidModelIdComponent}, pcbOutputPath));
        QVERIFY(invalidInputWriter.diagnostics().join('\n').contains(
            QStringLiteral("模型元数据包含参数分隔符、换行或 NUL")));

        // invalidBodyComponent：测试使用的元器件标识或名称。
        AltiumPcbComponent invalidBodyComponent;
        invalidBodyComponent.name = QStringLiteral("INVALID_BODY");
        // invalidBodyData：测试使用的元器件标识或名称。
        AltiumPcbComponentBody invalidBodyData;
        invalidBodyData.layerName = QStringLiteral("MECHANICAL17");
        invalidBodyComponent.bodies.append(invalidBodyData);
        QVERIFY(!invalidInputWriter.write({invalidBodyComponent}, pcbOutputPath));
        QVERIFY(invalidInputWriter.diagnostics().join('\n').contains(QStringLiteral("无效 3D 元件体层名")));

        invalidBodyComponent.bodies.clear();
        invalidBodyData.layerName = QStringLiteral("MECHANICAL1");
        invalidBodyData.kind = 3;
        invalidBodyComponent.bodies.append(invalidBodyData);
        QVERIFY(!invalidInputWriter.write({invalidBodyComponent}, pcbOutputPath));
        QVERIFY(invalidInputWriter.diagnostics().join('\n').contains(QStringLiteral("无效 3D 元件体类型")));

        invalidBodyData.kind = 0;
        invalidBodyData.bodyOpacity3d = 2.0;
        invalidBodyComponent.bodies = {invalidBodyData};
        QVERIFY(!invalidInputWriter.write({invalidBodyComponent}, pcbOutputPath));
        QVERIFY(invalidInputWriter.diagnostics().join('\n').contains(QStringLiteral("无效 3D 元件体透明度")));

        // invalidBodyAssociation：测试使用的元器件标识或名称。
        AltiumPcbComponent invalidBodyAssociation;
        invalidBodyAssociation.name = QStringLiteral("INVALID_BODY_ASSOCIATION");
        invalidBodyAssociation.models = {firstModel};
        // unassociatedBody：几何计算中的坐标或尺寸值。
        AltiumPcbComponentBody unassociatedBody;
        unassociatedBody.modelId = QStringLiteral("unknown-model");
        invalidBodyAssociation.bodies = {unassociatedBody};
        QVERIFY(!invalidInputWriter.write({invalidBodyAssociation}, pcbOutputPath));
        QVERIFY(invalidInputWriter.diagnostics().join('\n').contains(QStringLiteral("未关联有效模型")));

        // invalidCStringComponent：测试使用的元器件标识或名称。
        AltiumPcbComponent invalidCStringComponent;
        invalidCStringComponent.name = QStringLiteral("INVALID_CSTRING");
        // invalidCStringRegion：测试使用的元器件标识或名称。
        AltiumPcbRegion invalidCStringRegion;
        invalidCStringRegion.name = QStringLiteral("BAD|REGION");
        invalidCStringRegion.vertices = {QPointF(0.0, 0.0), QPointF(1000.0, 0.0), QPointF(0.0, 1000.0)};
        invalidCStringComponent.regions.append(invalidCStringRegion);
        QVERIFY(!invalidInputWriter.write({invalidCStringComponent}, pcbOutputPath));
        QVERIFY(invalidInputWriter.diagnostics().join('\n').contains(QStringLiteral("区域名称包含参数分隔符或 NUL")));

        invalidCStringComponent.regions.clear();
        // invalidCStringBody：测试使用的元器件标识或名称。
        AltiumPcbComponentBody invalidCStringBody;
        invalidCStringBody.name = QStringLiteral("BAD\0BODY");
        invalidCStringComponent.bodies.append(invalidCStringBody);
        QVERIFY(!invalidInputWriter.write({invalidCStringComponent}, pcbOutputPath));
        QVERIFY(
            invalidInputWriter.diagnostics().join('\n').contains(QStringLiteral("3D 元件体名称包含参数分隔符或 NUL")));

        invalidCStringComponent.bodies.clear();
        // extensionRegion：扩展区域图元。
        AltiumPcbRegion extensionRegion;
        extensionRegion.vertices = {QPointF(0.0, 0.0), QPointF(1000.0, 0.0), QPointF(0.0, 1000.0)};
        invalidCStringComponent.regions.append(extensionRegion);
        // invalidCStringExtension：测试使用的元器件标识或名称。
        AltiumPcbExtendedPrimitiveInfo invalidCStringExtension;
        invalidCStringExtension.primitiveIndex = 0;
        invalidCStringExtension.objectName = QStringLiteral("Pad");
        invalidCStringExtension.params.insert(QStringLiteral("BAD|KEY"), QStringLiteral("value"));
        invalidCStringComponent.extendedPrimitives.append(invalidCStringExtension);
        QVERIFY(!invalidInputWriter.write({invalidCStringComponent}, pcbOutputPath));
        QVERIFY(invalidInputWriter.diagnostics().join('\n').contains(QStringLiteral("扩展参数键包含参数分隔符或 NUL")));

        invalidCStringComponent.extendedPrimitives.clear();
        invalidCStringComponent.description = QStringLiteral("中文描述");
        QVERIFY(!invalidInputWriter.write({invalidCStringComponent}, pcbOutputPath));
        QVERIFY(invalidInputWriter.diagnostics().join('\n').contains(QStringLiteral("封装描述包含无法编码的字符")));

        invalidCStringComponent.description = QStringLiteral("line\nbreak");
        QVERIFY(!invalidInputWriter.write({invalidCStringComponent}, pcbOutputPath));
        QVERIFY(invalidInputWriter.diagnostics().join('\n').contains(QStringLiteral("封装描述包含换行字符")));
    }

    /**
     * @brief 验证嵌入图片的 Storage 文件名冲突和无效数据诊断
     * @details 确认重复文件名会被稳定改名，非法文件名和空数据不会进入 Storage，且有效压缩数据可回读。
     */
    void reusingSchLibWriterResetsContentIndexes() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // component：测试使用的缓存或元器件对象。
        AltiumSchComponent component;
        component.name = QStringLiteral("REUSED_WRITER");
        // rectangle：测试用矩形图元。
        AltiumSchRectangle rectangle;
        rectangle.cornerX = 100000;
        rectangle.cornerY = 100000;
        component.rectangles.append(rectangle);

        // writer：负责写入目标格式的 writer 实例。
        AltiumSchLibWriter writer;
        // firstPath：测试使用的文件系统路径。
        const QString firstPath = QDir(tempDir.path()).filePath(QStringLiteral("first.SchLib"));
        // secondPath：测试使用的文件系统路径。
        const QString secondPath = QDir(tempDir.path()).filePath(QStringLiteral("second.SchLib"));
        QVERIFY(writer.write({component}, firstPath));
        QVERIFY(writer.write({component}, secondPath));

        // reader：负责回读生成内容的 reader 实例。
        AltiumSchLibReader reader;
        QVERIFY2(reader.open(secondPath), qPrintable(reader.errorString()));
        // records：回读得到的记录集合。
        QVector<AltiumSchLibReader::Record> records;
        QVERIFY2(reader.readComponentRecords(0, &records), qPrintable(reader.errorString()));
        // foundRectangle：检测到的矩形图元。
        bool foundRectangle = false;
        for (const auto& record : records) {
            if (record.recordType == 14) {
                QCOMPARE(record.indexInSheet, -1);
                foundRectangle = true;
                break;
            }
        }
        QVERIFY(foundRectangle);
    }

    // 验证嵌入图片的存储名称和二进制内容均符合复合文档要求。
    /**
     * @brief 验证 validatesEmbeddedImageStorageNamesAndPayloads 测试场景。
     */
    void validatesEmbeddedImageStorageNamesAndPayloads() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // imageData：测试使用的文本或数据内容。
        const QByteArray imageData = QByteArrayLiteral("embedded-image-payload");
        const auto makeImage = [&](const QString& fileName, const QByteArray& data) {
            // image：测试用图像数据。
            AltiumSchImage image;
            image.fileName = fileName;
            image.data = data;
            image.embedImage = true;
            image.cornerX = 100000;
            image.cornerY = 100000;
            return image;
        };

        // component：测试使用的缓存或元器件对象。
        AltiumSchComponent component;
        component.name = QStringLiteral("IMAGE_STORAGE_DIAGNOSTICS");
        component.images.append(makeImage(QStringLiteral("same.png"), imageData));
        component.images.append(makeImage(QStringLiteral("SAME.PNG"), imageData));
        component.images.append(makeImage(QStringLiteral("invalid?.png"), imageData));
        component.images.append(makeImage(QStringLiteral("empty.png"), {}));
        component.images.append(makeImage(QStringLiteral("   "), imageData));
        component.images.append(makeImage(QStringLiteral("CON.png"), imageData));
        component.images.append(makeImage(QStringLiteral("trailing.png "), imageData));
        component.images.append(makeImage(QStringLiteral("zero%1width.png").arg(QChar(0x200B)), imageData));
        component.images.append(makeImage(QString(QChar(0xD800)) + QStringLiteral(".png"), imageData));

        // writer：负责写入目标格式的 writer 实例。
        AltiumSchLibWriter writer;
        // outputPath：测试使用的文件系统路径。
        const QString outputPath = QDir(tempDir.path()).filePath(QStringLiteral("image-diagnostics.SchLib"));
        QVERIFY(writer.write({component}, outputPath));
        // diagnostics：导出过程中收集的诊断信息。
        const QString diagnostics = writer.diagnostics().join('\n');
        QVERIFY(diagnostics.contains(QStringLiteral("嵌入文件名 SAME.PNG 重复")));
        QVERIFY(diagnostics.contains(QStringLiteral("嵌入文件名无效: invalid?.png")));
        QVERIFY(diagnostics.contains(QStringLiteral("嵌入文件名无效:    ，已跳过 Storage")));
        QVERIFY(diagnostics.contains(QStringLiteral("嵌入文件名无效: CON.png")));
        QVERIFY(diagnostics.contains(QStringLiteral("嵌入文件名无效: trailing.png ")));
        QVERIFY(diagnostics.contains(QStringLiteral("嵌入文件名无效: zero")));
        QVERIFY(diagnostics.contains(QStringLiteral("嵌入文件名无效:")));
        QVERIFY(diagnostics.contains(QStringLiteral("嵌入数据为空")));

        // reader：负责回读生成内容的 reader 实例。
        AltiumSchLibReader reader;
        QVERIFY2(reader.open(outputPath), qPrintable(reader.errorString()));
        // entries：测试用条目集合。
        QVector<AltiumSchLibReader::ImageStorageEntry> entries;
        QVERIFY2(reader.readImageStorage(&entries), qPrintable(reader.errorString()));
        QCOMPARE(entries.size(), 2);
        QCOMPARE(entries.at(0).name, QStringLiteral("same.png"));
        QCOMPARE(entries.at(1).name, QStringLiteral("SAME_2.PNG"));
        QByteArray expectedSize(4, '\0');
        expectedSize[3] = static_cast<char>(imageData.size());
        QCOMPARE(qUncompress(expectedSize + entries.at(0).compressedData), imageData);
        QCOMPARE(qUncompress(expectedSize + entries.at(1).compressedData), imageData);
    }

    /**
     * @brief 验证 SchLib 和 PcbLib 完整写入和流结构
     * @details 写入包含引脚、矩形、路径的符号和包含焊盘、走线的封装，
     * 验证 FileHeader 参数、Data 流内容、Library 元数据和图元记录布局
     */
    void writesSchLibAndPcbLibStorages() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // symbol：测试用符号数据。
        AltiumSchComponent symbol;
        symbol.name = QStringLiteral("C2040");
        symbol.partCount = 1;
        symbol.sourceMetadata.insert(QStringLiteral("manufacturer"), QStringLiteral("Example Corp"));
        symbol.sourceMetadata.insert(QStringLiteral("lcscId"), QStringLiteral("C2040"));
        symbol.aliases = {QStringLiteral("C2040_ALIAS")};
        // parameter：测试用参数。
        AltiumSchParameter parameter;
        parameter.name = QStringLiteral("Temperature Coefficient");
        parameter.value = QStringLiteral("±50ppm/K");
        parameter.readOnly = true;
        symbol.parameters.append(parameter);

        // pin：测试用引脚数据。
        AltiumSchPin pin;
        pin.name = QStringLiteral("A");
        pin.designator = QStringLiteral("1");
        pin.locationX = 100000;
        pin.locationY = 200000;
        pin.length = 100000;
        pin.electricalType = AltiumModels::PinElectricalType::Passive;
        pin.orientation = AltiumModels::PinOrientation::Right;
        symbol.pins.append(pin);

        // rect：测试用矩形图元。
        AltiumSchRectangle rect;
        rect.locationX = 100000;
        rect.locationY = 100000;
        rect.cornerX = 500000;
        rect.cornerY = 500000;
        rect.lineStyle = 1;
        symbol.rectangles.append(rect);

        // roundRect：圆角矩形图元。
        AltiumSchRoundRectangle roundRect;
        roundRect.locationX = 600000;
        roundRect.locationY = 100000;
        roundRect.cornerX = 900000;
        roundRect.cornerY = 500000;
        roundRect.cornerXRadius = 50000;
        roundRect.cornerYRadius = 75000;
        symbol.roundRectangles.append(roundRect);

        // path：测试使用的文件系统路径。
        AltiumSchPath path;
        path.vertices = {QPointF(0, 0), QPointF(100000, 100000), QPointF(200000, 0)};
        path.ownerPartId = 1;
        symbol.paths.append(path);
        // ieee：测试用 IEEE 浮点值。
        AltiumSchIeee ieee;
        ieee.symbol = 18;  // Pi
        ieee.locationX = 300000;
        ieee.locationY = 300000;
        ieee.mirrored = true;
        symbol.ieeeSymbols.append(ieee);
        // pie：测试用扇形数据。
        AltiumSchPie pie;
        pie.centerX = 1000000;
        pie.centerY = 300000;
        pie.radius = 100000;
        pie.startAngle = 30.0;
        pie.endAngle = 150.0;
        symbol.pies.append(pie);
        // ellipticalArc：测试用圆弧图元。
        AltiumSchEllipticalArc ellipticalArc;
        ellipticalArc.centerX = 1200000;
        ellipticalArc.centerY = 300000;
        ellipticalArc.radiusX = 150000;
        ellipticalArc.radiusY = 80000;
        ellipticalArc.startAngle = 45.0;
        ellipticalArc.endAngle = 270.0;
        symbol.ellipticalArcs.append(ellipticalArc);
        // textFrame：测试使用的文本或数据内容。
        AltiumSchTextFrame textFrame;
        textFrame.locationX = 1300000;
        textFrame.locationY = 100000;
        textFrame.cornerX = 1600000;
        textFrame.cornerY = 500000;
        textFrame.text = QStringLiteral("说明\n第二行");
        textFrame.fontName = QStringLiteral("Courier New");
        textFrame.fontSizeMm = 25.4 / 72.0 * 11.0;
        textFrame.bold = true;
        textFrame.italic = true;
        textFrame.orientation = 1;
        textFrame.alignment = 2;
        textFrame.textMargin = 25000;
        textFrame.isSolid = true;
        textFrame.showBorder = true;
        textFrame.wordWrap = true;
        textFrame.clipToRect = true;
        textFrame.transparent = true;
        symbol.textFrames.append(textFrame);
        // styledText：测试使用的文本或数据内容。
        AltiumSchText styledText;
        styledText.locationX = 1500000;
        styledText.locationY = 700000;
        styledText.text = QStringLiteral("标签 α");
        styledText.fontId = 0;
        styledText.fontName = QStringLiteral("Arial");
        styledText.fontSizeMm = 25.4 / 72.0 * 8.0;
        styledText.bold = true;
        styledText.italic = true;
        styledText.anchor = QStringLiteral("start");
        symbol.texts.append(styledText);
        // fallbackFontText：测试使用的文本或数据内容。
        AltiumSchText fallbackFontText;
        fallbackFontText.locationX = 1600000;
        fallbackFontText.locationY = 700000;
        fallbackFontText.text = QStringLiteral("Fallback font");
        fallbackFontText.fontId = 99;
        symbol.texts.append(fallbackFontText);
        // sizeOnlyText：测试使用的文本或数据内容。
        AltiumSchText sizeOnlyText;
        sizeOnlyText.locationX = 1650000;
        sizeOnlyText.locationY = 700000;
        sizeOnlyText.text = QStringLiteral("Size-only font");
        sizeOnlyText.fontId = 1;
        sizeOnlyText.fontSizeMm = 25.4 / 72.0 * 12.0;
        symbol.texts.append(sizeOnlyText);
        // image：测试用图像数据。
        AltiumSchImage image;
        image.locationX = 1700000;
        image.locationY = 100000;
        image.cornerX = 1900000;
        image.cornerY = 300000;
        image.fileName = QStringLiteral("logo.png");
        image.data = QByteArrayLiteral("fake-png-data");
        image.embedImage = true;
        image.keepAspect = true;
        symbol.images.append(image);

        // impl：实现记录。
        AltiumSchComponent::Implementation impl;
        impl.modelName = QStringLiteral("模型-高精度");
        impl.modelType = QStringLiteral("PCBLIB");
        impl.parameters.insert(QStringLiteral("热模型"), QStringLiteral("高精度"));
        impl.pinMappings.insert(QStringLiteral("1"), QStringLiteral("A1"));
        symbol.implementations.append(impl);
        // simulationImpl：仿真实现记录。
        AltiumSchComponent::Implementation simulationImpl;
        simulationImpl.modelName = QStringLiteral("C2040_SPICE");
        simulationImpl.modelType = QStringLiteral("SIM");
        symbol.implementations.append(simulationImpl);

        // schPath：测试使用的文件系统路径。
        const QString schPath = QDir(tempDir.path()).filePath(QStringLiteral("easyeda_convertlib.SchLib"));
        // schWriter：负责写入目标格式的 writer 实例。
        AltiumSchLibWriter schWriter;
        QVERIFY(schWriter.write({symbol}, schPath, QStringLiteral("easyeda_convertlib")));

        // invalidBezierComponent：测试使用的元器件标识或名称。
        AltiumSchComponent invalidBezierComponent;
        invalidBezierComponent.name = QStringLiteral("INVALID_DIRECT_BEZIER");
        // invalidBezier：测试使用的元器件标识或名称。
        AltiumSchBezier invalidBezier;
        invalidBezier.controlPoints = {QPointF(0, 0), QPointF(100000, 0), QPointF(200000, 0)};
        invalidBezierComponent.beziers.append(invalidBezier);
        // invalidBezierWriter：测试使用的元器件标识或名称。
        AltiumSchLibWriter invalidBezierWriter;
        const QString invalidBezierOutput =
            QDir(tempDir.path()).filePath(QStringLiteral("invalid-direct-bezier.SchLib"));
        QVERIFY(!invalidBezierWriter.write({invalidBezierComponent}, invalidBezierOutput));
        QVERIFY(invalidBezierWriter.diagnostics().join('\n').contains(QStringLiteral("Bézier 控制点数量无效")));

        // schHeader：原理图库头数据。
        QByteArray schHeader;
        QVERIFY(readCfbStream(schPath, QStringLiteral("FileHeader"), schHeader));
        QVERIFY(schHeader.contains("COMPCOUNT=1"));
        QVERIFY(schHeader.contains("WEIGHT=28"));
        QVERIFY(schHeader.contains("LIBREF0=C2040"));
        QVERIFY(schHeader.contains("PARTCOUNT0=2"));
        QVERIFY(schHeader.mid(4).startsWith("|HEADER="));
        QVERIFY(schHeader.contains("FontName2=Arial"));
        QVERIFY(schHeader.contains("Size2=8"));
        QVERIFY(schHeader.contains("Bold2=T"));
        QVERIFY(schHeader.contains("Italic2=T"));
        QVERIFY(schHeader.contains("FontName3=Times New Roman"));
        QVERIFY(schHeader.contains("Size3=12"));
        QVERIFY(schHeader.contains("FontName4=Courier New"));
        QVERIFY(schHeader.contains("Size4=11"));
        QVERIFY(schHeader.contains("Bold4=T"));
        QVERIFY(schHeader.contains("Italic4=T"));

        // schData：测试使用的文本或数据内容。
        QByteArray schData;
        QVERIFY(readCfbStream(schPath, QStringLiteral("C2040/Data"), schData));
        // weightOffset：权重字段偏移量。
        const int weightOffset = schHeader.indexOf("WEIGHT=");
        // weightEnd：权重字段结束偏移量。
        const int weightEnd = schHeader.indexOf('|', weightOffset);
        QVERIFY(weightOffset >= 0);
        QVERIFY(weightEnd > weightOffset);
        // weightOk：测试用权重校验结果。
        bool weightOk = false;
        const int headerWeight =
            QString::fromLatin1(schHeader.mid(weightOffset + 7, weightEnd - weightOffset - 7)).toInt(&weightOk);
        QVERIFY(weightOk);
        QCOMPARE(countSchLibRecords(schData), headerWeight);
        QVERIFY(schData.mid(4).startsWith("|RECORD=1|"));
        QVERIFY(schData.contains("LibReference=C2040"));
        QVERIFY(schData.contains("RECORD=14"));
        QVERIFY(!schData.contains("|IndexInSheet=0|"));
        QVERIFY(!schData.contains("OWNERPARTDISPLAYMODE="));
        QVERIFY(schData.contains("LineStyleExt=1"));
        QVERIFY(schData.contains("RECORD=10"));
        QVERIFY(schData.contains("CornerXRadius=1"));
        QVERIFY(schData.contains("RECORD=45"));
        QVERIFY(schData.contains("DESIMP0=A1"));
        QVERIFY(schData.contains("MODELTYPE=SIM"));
        QVERIFY(schData.contains("DATAFILECOUNT=0"));
        QVERIFY(schData.contains("%UTF8%MODELNAME="));
        QVERIFY(schData.contains(QStringLiteral("模型-高精度").toUtf8()));
        QVERIFY(schData.contains(QStringLiteral("%UTF8%热模型=").toUtf8()));
        QVERIFY(schData.contains(QStringLiteral("高精度").toUtf8()));
        QVERIFY(schData.contains("RECORD=6"));
        QVERIFY(schData.contains("RECORD=3"));
        QVERIFY(schData.contains("Symbol=18"));
        QVERIFY(schData.contains("RECORD=9"));
        QVERIFY(schData.contains("StartAngle=30.000"));
        QVERIFY(schData.contains("RECORD=11"));
        QVERIFY(schData.contains("SecondaryRadius=1"));
        QVERIFY(schData.contains("RECORD=28"));
        QVERIFY(schData.contains(QStringLiteral("第二行").toUtf8()));
        QVERIFY(schData.contains("FontID=4"));
        QVERIFY(schData.contains("FontSize=3.8806"));
        QVERIFY(schData.contains("Orientation=1"));
        QVERIFY(schData.contains("Alignment=2"));
        QVERIFY(schData.contains("TextMargin=0"));
        QVERIFY(schData.contains("TextMargin_Frac=25000"));
        QVERIFY(schData.contains("IsSolid=T"));
        QVERIFY(schData.contains("ShowBorder=T"));
        QVERIFY(schData.contains("WordWrap=T"));
        QVERIFY(schData.contains("ClipToRect=T"));
        QVERIFY(schData.contains("Transparent=T"));
        // fallbackTextOffset：测试使用的文本或数据内容。
        const int fallbackTextOffset = schData.indexOf("Text=Fallback font");
        // fallbackRecordOffset：回退记录偏移量。
        const int fallbackRecordOffset = schData.lastIndexOf("|RECORD=4|", fallbackTextOffset);
        // nextFallbackRecordOffset：下一条回退记录偏移量。
        const int nextFallbackRecordOffset = schData.indexOf("|RECORD=", fallbackTextOffset + 1);
        QVERIFY(fallbackTextOffset > fallbackRecordOffset);
        QVERIFY(nextFallbackRecordOffset > fallbackRecordOffset);
        const QByteArray fallbackRecord =
            schData.mid(fallbackRecordOffset, nextFallbackRecordOffset - fallbackRecordOffset);
        QVERIFY(fallbackRecord.contains("FontID=1"));
        // sizeOnlyTextOffset：测试使用的文本或数据内容。
        const int sizeOnlyTextOffset = schData.indexOf("Text=Size-only font");
        // sizeOnlyRecordOffset：测试使用的数量或索引值。
        const int sizeOnlyRecordOffset = schData.lastIndexOf("|RECORD=4|", sizeOnlyTextOffset);
        // nextSizeOnlyRecordOffset：测试使用的数量或索引值。
        const int nextSizeOnlyRecordOffset = schData.indexOf("|RECORD=", sizeOnlyTextOffset + 1);
        QVERIFY(sizeOnlyTextOffset > sizeOnlyRecordOffset);
        QVERIFY(nextSizeOnlyRecordOffset > sizeOnlyRecordOffset);
        const QByteArray sizeOnlyRecord =
            schData.mid(sizeOnlyRecordOffset, nextSizeOnlyRecordOffset - sizeOnlyRecordOffset);
        QVERIFY(sizeOnlyRecord.contains("FontID=3"));
        QVERIFY(schData.contains("FontSize=2.8222"));
        QVERIFY(schData.contains("TextAnchor=start"));
        QVERIFY(schData.contains(QStringLiteral("标签 α").toUtf8()));
        QVERIFY(schData.contains("RECORD=30"));
        QVERIFY(schData.contains("EmbedImage=T"));
        QVERIFY(schData.contains("FileName=logo.png"));
        // imageStorage：图片存储数据。
        QByteArray imageStorage;
        QVERIFY(readCfbStream(schPath, QStringLiteral("Storage"), imageStorage));
        QVERIFY(imageStorage.contains("logo.png"));
        // imageNameOffset：测试使用的元器件标识或名称。
        const int imageNameOffset = imageStorage.indexOf("logo.png");
        QVERIFY(imageNameOffset >= 2);
        QCOMPARE(static_cast<uint8_t>(imageStorage.at(imageNameOffset - 2)), static_cast<uint8_t>(0xD0));
        QCOMPARE(static_cast<uint8_t>(imageStorage.at(imageNameOffset - 1)), static_cast<uint8_t>(8));
        // imageLengthOffset：图片长度字段偏移量。
        const int imageLengthOffset = imageNameOffset + 8;
        QVERIFY(imageLengthOffset + 4 <= imageStorage.size());
        // compressedLength：压缩数据长度。
        const quint32 compressedLength = readU32(imageStorage, imageLengthOffset);
        // imageDataOffset：测试使用的文本或数据内容。
        const int imageDataOffset = imageLengthOffset + 4;
        QVERIFY(compressedLength > 0);
        QVERIFY(imageDataOffset + static_cast<int>(compressedLength) <= imageStorage.size());
        QByteArray expectedSize(4, '\0');
        expectedSize[3] = static_cast<char>(image.data.size());
        const QByteArray restoredImage =
            qUncompress(expectedSize + imageStorage.mid(imageDataOffset, compressedLength));
        QCOMPARE(restoredImage, image.data);
        // imageReader：负责回读生成内容的 reader 实例。
        AltiumSchLibReader imageReader;
        QVERIFY2(imageReader.open(schPath), qPrintable(imageReader.errorString()));
        // imageEntries：测试用图像条目集合。
        QVector<AltiumSchLibReader::ImageStorageEntry> imageEntries;
        QVERIFY2(imageReader.readImageStorage(&imageEntries), qPrintable(imageReader.errorString()));
        QCOMPARE(imageEntries.size(), 1);
        QCOMPARE(imageEntries.first().flags, quint8(1));
        QCOMPARE(imageEntries.first().name, QStringLiteral("logo.png"));
        QCOMPARE(qUncompress(expectedSize + imageEntries.first().compressedData), image.data);
        // implementationRecords：测试解析得到的条目集合。
        QVector<AltiumSchLibReader::Record> implementationRecords;
        QVERIFY2(imageReader.readComponentRecords(QStringLiteral("C2040"), &implementationRecords),
                 qPrintable(imageReader.errorString()));
        // foundUnicodeImplementation：检测到的 Unicode 实现。
        bool foundUnicodeImplementation = false;
        // foundUnicodeImplementationParameters：检测到的 Unicode 实现参数。
        bool foundUnicodeImplementationParameters = false;
        // foundUnicodeText：测试使用的文本或数据内容。
        bool foundUnicodeText = false;
        for (const AltiumSchLibReader::Record& record : implementationRecords) {
            if (record.recordType == 45 &&
                record.parameters.value(QStringLiteral("MODELNAME")) == QStringLiteral("模型-高精度")) {
                foundUnicodeImplementation = true;
            }
            if (record.recordType == 48 &&
                record.parameters.value(QStringLiteral("热模型")) == QStringLiteral("高精度")) {
                foundUnicodeImplementationParameters = true;
            }
            if (record.recordType == 4 && record.parameters.value(QStringLiteral("Text")) == QStringLiteral("标签 α"))
                foundUnicodeText = true;
        }
        QVERIFY(foundUnicodeImplementation);
        QVERIFY(foundUnicodeImplementationParameters);
        QVERIFY(foundUnicodeText);
        QVERIFY(schData.contains("Mirror=T"));
        QVERIFY(schData.contains("OWNERPARTID=1"));
        QVERIFY(schData.contains("PartCount=2"));
        QVERIFY(schData.contains("RECORD=34"));
        QVERIFY(schData.contains("NAME=Designator"));
        QVERIFY(schData.contains("RECORD=41"));
        QVERIFY(schData.contains("NAME=Comment"));
        QVERIFY(schData.contains("TEXT=C2040"));
        QVERIFY(schData.contains("NAME=Manufacturer"));
        QVERIFY(schData.contains("TEXT=Example Corp"));
        QVERIFY(schData.contains("NAME=LCSC Part"));
        QVERIFY(schData.contains("NAME=Aliases"));
        QVERIFY(schData.contains("Aliases=C2040_ALIAS"));
        QVERIFY(schData.contains("C2040_ALIAS"));
        QVERIFY(schData.contains("NAME=Temperature Coefficient"));
        QVERIFY(schData.contains("IsHidden=T"));
        QVERIFY(schData.contains("READONLYSTATE=1"));

        // footprint：测试用封装数据。
        AltiumPcbComponent footprint;
        footprint.name = QStringLiteral("LQFN-56_L7.0-W7.0-P0.4-EP");
        footprint.description = QStringLiteral("LQFN-56");
        footprint.height = 0.7;

        // pad：测试用焊盘数据。
        AltiumPcbPad pad;
        pad.designator = QStringLiteral("1");
        pad.locationX = 10000;
        pad.locationY = 20000;
        pad.sizeTopX = 20000;
        pad.sizeTopY = 20000;
        pad.sizeMidX = 20000;
        pad.sizeMidY = 20000;
        pad.sizeBotX = 20000;
        pad.sizeBotY = 20000;
        pad.isSMD = true;
        pad.isPlated = false;
        pad.layer = 1;
        pad.shapeTop = 1;
        pad.shapeMid = 1;
        pad.shapeBot = 1;
        footprint.pads.append(pad);

        // track：测试用线段图元。
        AltiumPcbTrack track;
        track.startX = 0;
        track.startY = 0;
        track.endX = 10000;
        track.endY = 10000;
        track.width = 5000;
        track.layer = 33;
        footprint.tracks.append(track);

        // arc：测试用圆弧图元。
        AltiumPcbArc arc;
        arc.centerX = 30000;
        arc.centerY = 40000;
        arc.radius = 5000;
        arc.startAngle = 15.0;
        arc.endAngle = 225.0;
        arc.width = 800;
        arc.layer = 1;
        footprint.arcs.append(arc);

        // pcbPath：测试使用的文件系统路径。
        const QString pcbPath = QDir(tempDir.path()).filePath(QStringLiteral("easyeda_convertlib.PcbLib"));
        // pcbWriter：负责写入目标格式的 writer 实例。
        AltiumPcbLibWriter pcbWriter;
        QVERIFY(pcbWriter.write({footprint}, pcbPath, QStringLiteral("easyeda_convertlib")));

        // pcbHeader：PCB 文件头。
        QByteArray pcbHeader;
        QVERIFY(readCfbStream(pcbPath, QStringLiteral("FileHeader"), pcbHeader));
        QCOMPARE(pcbHeader.size(), 32);
        QCOMPARE(readU32(pcbHeader, 0), quint32(27));
        QCOMPARE(static_cast<quint8>(pcbHeader.at(4)), quint8(27));
        QCOMPARE(pcbHeader.mid(5), QByteArrayLiteral("PCB 6.0 Binary Library File"));

        // libraryData：测试使用的文本或数据内容。
        QByteArray libraryData;
        QVERIFY(readCfbStream(pcbPath, QStringLiteral("Library/Data"), libraryData));
        QVERIFY(libraryData.contains("LQFN-56_L7.0-W7.0-P0.4-EP"));
        QVERIFY((readU32(libraryData, 0) & 0x00FFFFFFU) > 10000);
        QVERIFY(libraryData.contains("KIND=Protel_Advanced_PCB"));
        QVERIFY(libraryData.contains("LAYER82NAME=Via Holes"));

        // footprintParameters：测试用封装数据。
        QByteArray footprintParameters;
        QVERIFY(readCfbStream(pcbPath, QStringLiteral("LQFN-56_L7.0-W7.0-P0.4-EP/Parameters"), footprintParameters));
        QVERIFY(footprintParameters.mid(4).startsWith("|PATTERN="));

        // footprintData：测试使用的文本或数据内容。
        QByteArray footprintData;
        QVERIFY(readCfbStream(pcbPath, QStringLiteral("LQFN-56_L7.0-W7.0-P0.4-EP/Data"), footprintData));
        QVERIFY(footprintData.contains("LQFN-56_L7.0-W7.0-P0.4-EP"));
        // primitiveOffset：图元记录偏移量。
        int primitiveOffset = 4 + static_cast<int>(readU32(footprintData, 0));
        QCOMPARE(static_cast<quint8>(footprintData.at(primitiveOffset++)), quint8(2));
        for (int i = 0; i < 4; ++i) {
            primitiveOffset += 4 + static_cast<int>(readU32(footprintData, primitiveOffset) & 0x00FFFFFFU);
        }
        primitiveOffset += 4 + static_cast<int>(readU32(footprintData, primitiveOffset) & 0x00FFFFFFU);
        primitiveOffset += 4 + static_cast<int>(readU32(footprintData, primitiveOffset) & 0x00FFFFFFU);
        QCOMPARE(static_cast<quint8>(footprintData.at(primitiveOffset++)), quint8(4));
        QCOMPARE(readU32(footprintData, primitiveOffset) & 0x00FFFFFFU, quint32(36));

        // schReader：负责回读生成内容的 reader 实例。
        OLECompoundReader schReader;
        QVERIFY2(schReader.open(schPath), qPrintable(schReader.errorString()));
        QVERIFY(schReader.containsStream(QStringLiteral("C2040/Data")));
        // schReaderData：测试使用的文本或数据内容。
        QByteArray schReaderData;
        QVERIFY(schReader.readStream(QStringLiteral("C2040/Data"), &schReaderData));
        QVERIFY(schReaderData.contains("RECORD=41"));

        // pcbReader：负责回读生成内容的 reader 实例。
        OLECompoundReader pcbReader;
        QVERIFY2(pcbReader.open(pcbPath), qPrintable(pcbReader.errorString()));
        QVERIFY(pcbReader.containsStream(QStringLiteral("Library/Data")));
        // pcbReaderData：测试使用的文本或数据内容。
        QByteArray pcbReaderData;
        QVERIFY(pcbReader.readStream(QStringLiteral("Library/Data"), &pcbReaderData));
        QVERIFY(!pcbReaderData.isEmpty());

        // pcbLibraryReader：负责回读生成内容的 reader 实例。
        AltiumPcbLibReader pcbLibraryReader;
        QVERIFY2(pcbLibraryReader.open(pcbPath), qPrintable(pcbLibraryReader.errorString()));
        // pcbComponents：测试使用的缓存或元器件对象。
        const auto pcbComponents = pcbLibraryReader.components();
        QCOMPARE(pcbComponents.size(), 1);
        QCOMPARE(pcbComponents.first().name, QStringLiteral("LQFN-56_L7.0-W7.0-P0.4-EP"));
        QCOMPARE(pcbComponents.first().sectionKey, pcbComponents.first().name);
        QCOMPARE(pcbLibraryReader.fileVersion(), QStringLiteral("PCB 6.0 Binary Library File"));
        QVERIFY(pcbLibraryReader.libraryMetadata().contains("KIND=Protel_Advanced_PCB"));
        // footprintReaderData：测试使用的文本或数据内容。
        QByteArray footprintReaderData;
        QVERIFY(pcbLibraryReader.readFootprintStream(0, QStringLiteral("Data"), &footprintReaderData));
        QVERIFY(footprintReaderData.contains("LQFN-56_L7.0-W7.0-P0.4-EP"));
        // footprintObjects：测试构造的 JSON 数据对象。
        QVector<AltiumPcbLibReader::PrimitiveRecord> footprintObjects;
        QVERIFY(pcbLibraryReader.readFootprintObjects(0, &footprintObjects));
        QCOMPARE(footprintObjects.size(), 3);
        QCOMPARE(footprintObjects.at(0).objectId, quint8(AltiumConstants::PCB_OBJECT_PAD));
        QCOMPARE(footprintObjects.at(0).layer, quint8(1));
        QVERIFY(footprintObjects.at(0).primitiveFlags != 0);
        QVERIFY(footprintObjects.at(0).hasPadFields);
        QCOMPARE(footprintObjects.at(0).designator, QStringLiteral("1"));
        QCOMPARE(footprintObjects.at(0).pad.locationX, qint32(10000));
        QCOMPARE(footprintObjects.at(0).pad.sizeTopX, qint32(20000));
        QCOMPARE(footprintObjects.at(0).pad.holeType, quint8(0));
        QCOMPARE(footprintObjects.at(0).blocks.size(), 6);
        QCOMPARE(footprintObjects.at(1).objectId, quint8(AltiumConstants::PCB_OBJECT_TRACK));
        QCOMPARE(footprintObjects.at(1).layer, quint8(33));
        QCOMPARE(footprintObjects.at(1).primitiveFlags, quint16(0x0C));
        QVERIFY(footprintObjects.at(1).hasTrackFields);
        QCOMPARE(footprintObjects.at(1).track.endX, qint32(10000));
        QCOMPARE(footprintObjects.at(1).blocks.size(), 1);
        QCOMPARE(footprintObjects.at(2).objectId, quint8(AltiumConstants::PCB_OBJECT_ARC));
        QVERIFY(footprintObjects.at(2).hasArcFields);
        QCOMPARE(footprintObjects.at(2).arc.radius, qint32(5000));
        QCOMPARE(footprintObjects.at(2).arc.endAngle, 225.0);
        // reconstructedFootprintObjects：测试构造的 JSON 数据对象。
        QByteArray reconstructedFootprintObjects;
        for (const auto& object : footprintObjects)
            reconstructedFootprintObjects.append(object.encoded);
        // footprintNameSize：测试使用的元器件标识或名称。
        const int footprintNameSize = static_cast<int>(readU32(footprintReaderData, 0));
        QCOMPARE(reconstructedFootprintObjects, footprintReaderData.mid(4 + footprintNameSize));
        // footprintHeaderReaderData：测试使用的文本或数据内容。
        QByteArray footprintHeaderReaderData;
        QVERIFY(pcbLibraryReader.readFootprintStream(0, QStringLiteral("Header"), &footprintHeaderReaderData));
        AltiumBinaryReader footprintHeaderReader(footprintHeaderReaderData);
        // primitiveCount：测试使用的数量或索引值。
        uint32_t primitiveCount = 0;
        QVERIFY(footprintHeaderReader.readUInt32(&primitiveCount));
        QVERIFY(primitiveCount > 0);

        // schLibraryReader：负责回读生成内容的 reader 实例。
        AltiumSchLibReader schLibraryReader;
        QVERIFY2(schLibraryReader.open(schPath), qPrintable(schLibraryReader.errorString()));
        // schComponents：测试使用的缓存或元器件对象。
        const auto schComponents = schLibraryReader.components();
        QCOMPARE(schComponents.size(), 1);
        QCOMPARE(schComponents.first().name, QStringLiteral("C2040"));
        QCOMPARE(schComponents.first().sectionKey, QStringLiteral("C2040"));
        QCOMPARE(schComponents.first().partCount, 1);
        QCOMPARE(schLibraryReader.headerParameters().value(QStringLiteral("COMPCOUNT")), QStringLiteral("1"));
        // fonts：解析得到的字体集合。
        const auto fonts = schLibraryReader.fonts();
        QVERIFY(fonts.size() >= 2);
        QCOMPARE(fonts.at(0).name, QStringLiteral("Times New Roman"));
        QCOMPARE(fonts.at(1).name, QStringLiteral("Arial"));
        QCOMPARE(fonts.at(1).size, 8);
        QVERIFY(fonts.at(1).bold);
        QVERIFY(fonts.at(1).italic);
        // componentData：测试使用的文本或数据内容。
        QByteArray componentData;
        QVERIFY(schLibraryReader.readComponentData(0, &componentData));
        QVERIFY(componentData.contains("LibReference=C2040"));
        // records：回读得到的记录集合。
        QVector<AltiumSchLibReader::Record> records;
        QVERIFY(schLibraryReader.readComponentRecords(0, &records));
        QCOMPARE(records.size(), headerWeight);
        // reconstructedRecords：测试解析得到的条目集合。
        QByteArray reconstructedRecords;
        for (const auto& record : records)
            reconstructedRecords.append(record.encoded);
        QCOMPARE(reconstructedRecords, componentData);
        QVERIFY(records.first().payload.startsWith("|RECORD=1|"));
        QVERIFY(records.first().hasParameters);
        QCOMPARE(records.first().parameters.value(QStringLiteral("RECORD")), QStringLiteral("1"));
        QVERIFY(records.first().parameters.contains(QStringLiteral("LibReference")));
        QCOMPARE(records.first().recordType, 1);
        QCOMPARE(records.first().ownerPartId, -1);
        QCOMPARE(records.first().indexInSheet, -1);
        // sawBinaryPin：测试用引脚数据。
        bool sawBinaryPin = false;
        for (const auto& record : records) {
            if (!record.hasParameters && record.payload.size() >= 4 &&
                static_cast<quint32>(static_cast<unsigned char>(record.payload.at(0))) == 2) {
                sawBinaryPin = true;
                QCOMPARE(record.recordType, 2);
                QCOMPARE(record.ownerPartId, 1);
                break;
            }
        }
        QVERIFY(sawBinaryPin);

        // previousIndexInSheet：测试使用的数量或索引值。
        int previousIndexInSheet = -1;
        for (const auto& record : records) {
            if (record.indexInSheet >= 0) {
                QVERIFY(record.indexInSheet > previousIndexInSheet);
                previousIndexInSheet = record.indexInSheet;
            }
            if (record.ownerPartId >= 0)
                QVERIFY(record.ownerPartId <= schComponents.first().partCount);
        }
    }

    /**
     * @brief 验证焊盘扩展块写入完整的 596 字节布局
     * @details 包含槽孔焊盘、圆角矩形形状、WideStrings 广字符串编码
     */
    void pcbLibPadWritesCompleteExtendedBlock() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // footprint：测试用封装数据。
        AltiumPcbComponent footprint;
        footprint.name = QStringLiteral("TEST_PAD");

        // pad：测试用焊盘数据。
        AltiumPcbPad pad;
        pad.designator = QStringLiteral("1");
        pad.locationX = 0;
        pad.locationY = 0;
        pad.sizeTopX = 433070;
        pad.sizeTopY = 787400;
        pad.sizeMidX = 433070;
        pad.sizeMidY = 787400;
        pad.sizeBotX = 433070;
        pad.sizeBotY = 787400;
        pad.holeSize = 236220;
        pad.shapeTop = 9;  // RoundedRectangle
        pad.shapeMid = 9;
        pad.shapeBot = 9;
        pad.rotation = 90.0;
        pad.isPlated = true;
        pad.layer = 74;  // Multi
        pad.isSMD = false;
        pad.holeType = 2;  // Slot
        pad.holeSlotLengthRaw = 590550;
        pad.holeRotation = 90.0;
        pad.cornerRadiusPercentage = 50;
        footprint.pads.append(pad);

        // text：测试使用的文本或数据内容。
        AltiumPcbText text;
        text.text = QStringLiteral("REF**");
        text.layer = 33;
        footprint.texts.append(text);

        // pcbPath：测试使用的文件系统路径。
        const QString pcbPath = QDir(tempDir.path()).filePath(QStringLiteral("test_pad.PcbLib"));
        // pcbWriter：负责写入目标格式的 writer 实例。
        AltiumPcbLibWriter pcbWriter;
        QVERIFY(pcbWriter.write({footprint}, pcbPath));

        // footprintData：测试使用的文本或数据内容。
        QByteArray footprintData;
        QVERIFY(readCfbStream(pcbPath, QStringLiteral("TEST_PAD/Data"), footprintData));
        // cursor：当前读取游标。
        int cursor = 4 + static_cast<int>(readU32(footprintData, 0));
        QCOMPARE(static_cast<quint8>(footprintData.at(cursor++)), quint8(2));
        for (int i = 0; i < 4; ++i) {
            cursor += 4 + static_cast<int>(readU32(footprintData, cursor) & 0x00FFFFFFU);
        }
        QCOMPARE(readU32(footprintData, cursor) & 0x00FFFFFFU, quint32(114));
        cursor += 4 + 114;
        QCOMPARE(readU32(footprintData, cursor) & 0x00FFFFFFU, quint32(596));
        cursor += 4 + 596;
        QCOMPARE(static_cast<quint8>(footprintData.at(cursor++)), quint8(5));
        QCOMPARE(readU32(footprintData, cursor) & 0x00FFFFFFU, quint32(252));

        // pcbReader：负责回读生成内容的 reader 实例。
        AltiumPcbLibReader pcbReader;
        QVERIFY2(pcbReader.open(pcbPath), qPrintable(pcbReader.errorString()));
        // objects：测试构造的 JSON 数据对象。
        QVector<AltiumPcbLibReader::PrimitiveRecord> objects;
        QVERIFY(pcbReader.readFootprintObjects(QStringLiteral("TEST_PAD"), &objects));
        QCOMPARE(objects.size(), 2);
        QCOMPARE(objects.at(0).objectId, quint8(AltiumConstants::PCB_OBJECT_PAD));
        QCOMPARE(objects.at(0).layer, quint8(74));
        QCOMPARE(objects.at(0).primitiveFlags, quint16(0x0C));
        QVERIFY(objects.at(0).hasPadFields);
        QCOMPARE(objects.at(0).designator, QStringLiteral("1"));
        QCOMPARE(objects.at(0).pad.holeType, quint8(2));
        QCOMPARE(objects.at(0).pad.holeSlotLengthRaw, qint32(590550));
        QCOMPARE(objects.at(0).pad.cornerRadiusPercentage, quint8(50));
        QCOMPARE(objects.at(0).blocks.size(), 6);
        QCOMPARE(objects.at(1).objectId, quint8(AltiumConstants::PCB_OBJECT_TEXT));
        QCOMPARE(objects.at(1).layer, quint8(33));
        QCOMPARE(objects.at(1).primitiveFlags, quint16(0x08));
        QVERIFY(objects.at(1).hasTextFields);
        QCOMPARE(objects.at(1).textFields.locationX, qint32(0));
        QCOMPARE(objects.at(1).text, QStringLiteral("REF**"));
        QCOMPARE(objects.at(1).blocks.size(), 2);
        QVERIFY(objects.at(1).blocks.at(1).payload.startsWith(char(5)));

        // wideStrings：测试使用的元器件标识或名称。
        QByteArray wideStrings;
        QVERIFY(readCfbStream(pcbPath, QStringLiteral("TEST_PAD/WideStrings"), wideStrings));
        QVERIFY(wideStrings.contains("ENCODEDTEXT0=82,69,70,42,42"));
    }

    /**
     * @brief 验证 PcbLib 损坏图元不会被结构化读取器误解析
     * @details 覆盖公共头部截断和 Pad 字符串子块长度错误，并确认原始 Data 流仍可回退读取。
     */
    void rejectsMalformedPcbLibPrimitiveObjects() {
        const auto writeMalformedLibrary = [](const QString& filePath, const QByteArray& footprintData) {
            // writer：负责写入目标格式的 writer 实例。
            OLECompoundWriter writer;
            if (!writer.create() || !writer.writeStream(QStringLiteral("FileHeader"), [&] {
                    // data：测试使用的文本或数据内容。
                    QByteArray data;
                    AltiumBinaryWriter binaryWriter(data);
                    binaryWriter.writeInt32(27);
                    binaryWriter.writePascalShortString(QStringLiteral("PCB 6.0 Binary Library File"));
                    return data;
                }()))
                return false;

            if (!writer.addStorage(QStringLiteral("Library")))
                return false;
            // libraryData：测试使用的文本或数据内容。
            QByteArray libraryData;
            AltiumBinaryWriter libraryWriter(libraryData);
            libraryWriter.beginBlock();
            libraryWriter.writeBytes(QByteArrayLiteral("KIND=Protel_Advanced_PCB"));
            libraryWriter.endBlock();
            libraryWriter.writeUInt32(1);
            libraryWriter.writeStringBlock(QStringLiteral("BROKEN"));
            if (!writer.writeStream(QStringLiteral("Library"), QStringLiteral("Data"), libraryData))
                return false;
            if (!writer.addStorage(QStringLiteral("BROKEN")) ||
                !writer.writeStream(QStringLiteral("BROKEN"), QStringLiteral("Data"), footprintData) ||
                !writer.writeStream(QStringLiteral("BROKEN"), QStringLiteral("WideStrings"), [&] {
                    // data：测试使用的文本或数据内容。
                    QByteArray data;
                    AltiumBinaryWriter binaryWriter(data);
                    binaryWriter.writeCStringParameterBlock({});
                    return data;
                }()))
                return false;
            return writer.saveToFile(filePath);
        };

        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // truncatedHeaderData：测试使用的文本或数据内容。
        QByteArray truncatedHeaderData;
        AltiumBinaryWriter truncatedHeaderWriter(truncatedHeaderData);
        truncatedHeaderWriter.writeStringBlock(QStringLiteral("BROKEN"));
        truncatedHeaderWriter.writeUInt8(AltiumConstants::PCB_OBJECT_TRACK);
        truncatedHeaderWriter.beginBlock();
        truncatedHeaderWriter.writeBytes(QByteArray(12, '\0'));
        truncatedHeaderWriter.endBlock();
        // truncatedHeaderPath：测试使用的文件系统路径。
        const QString truncatedHeaderPath = QDir(tempDir.path()).filePath(QStringLiteral("truncated-header.PcbLib"));
        QVERIFY(writeMalformedLibrary(truncatedHeaderPath, truncatedHeaderData));

        // truncatedHeaderReader：负责回读生成内容的 reader 实例。
        AltiumPcbLibReader truncatedHeaderReader;
        QVERIFY2(truncatedHeaderReader.open(truncatedHeaderPath), qPrintable(truncatedHeaderReader.errorString()));
        // objects：测试构造的 JSON 数据对象。
        QVector<AltiumPcbLibReader::PrimitiveRecord> objects;
        QVERIFY(!truncatedHeaderReader.readFootprintObjects(QStringLiteral("BROKEN"), &objects));
        QVERIFY(objects.isEmpty());
        QVERIFY(truncatedHeaderReader.hasError());
        QVERIFY(truncatedHeaderReader.errorString().contains(QStringLiteral("公共头部")));
        // rawData：测试使用的文本或数据内容。
        QByteArray rawData;
        QVERIFY(truncatedHeaderReader.readFootprintStream(QStringLiteral("BROKEN"), QStringLiteral("Data"), &rawData));
        QCOMPARE(rawData, truncatedHeaderData);

        // malformedStringData：测试使用的文本或数据内容。
        QByteArray malformedStringData;
        AltiumBinaryWriter malformedStringWriter(malformedStringData);
        malformedStringWriter.writeStringBlock(QStringLiteral("BROKEN"));
        malformedStringWriter.writeUInt8(AltiumConstants::PCB_OBJECT_PAD);
        malformedStringWriter.beginBlock();
        malformedStringWriter.writeBytes(QByteArrayLiteral("bad"));
        malformedStringWriter.endBlock();
        // malformedStringPath：测试使用的文件系统路径。
        const QString malformedStringPath = QDir(tempDir.path()).filePath(QStringLiteral("malformed-string.PcbLib"));
        QVERIFY(writeMalformedLibrary(malformedStringPath, malformedStringData));

        // malformedStringReader：负责回读生成内容的 reader 实例。
        AltiumPcbLibReader malformedStringReader;
        QVERIFY2(malformedStringReader.open(malformedStringPath), qPrintable(malformedStringReader.errorString()));
        QVERIFY(!malformedStringReader.readFootprintObjects(QStringLiteral("BROKEN"), &objects));
        QVERIFY(objects.isEmpty());
        QVERIFY(malformedStringReader.hasError());
        QVERIFY(malformedStringReader.errorString().contains(QStringLiteral("字符串子块")));
        QVERIFY(malformedStringReader.readFootprintStream(0, QStringLiteral("Data"), &rawData));
        QCOMPARE(rawData, malformedStringData);

        // invalidArcData：测试使用的元器件标识或名称。
        QByteArray invalidArcData;
        AltiumBinaryWriter invalidArcWriter(invalidArcData);
        invalidArcWriter.writeStringBlock(QStringLiteral("BROKEN"));
        invalidArcWriter.writeUInt8(AltiumConstants::PCB_OBJECT_ARC);
        invalidArcWriter.beginBlock();
        invalidArcWriter.writeUInt8(1);
        invalidArcWriter.writeUInt16(0);
        invalidArcWriter.writeBytes(QByteArray(10, '\0'));
        invalidArcWriter.writeInt32(0);
        invalidArcWriter.writeInt32(0);
        invalidArcWriter.writeInt32(0);
        invalidArcWriter.writeDouble(std::numeric_limits<double>::quiet_NaN());
        invalidArcWriter.writeDouble(360.0);
        invalidArcWriter.writeInt32(0);
        invalidArcWriter.endBlock();
        // invalidArcPath：测试使用的文件系统路径。
        const QString invalidArcPath = QDir(tempDir.path()).filePath(QStringLiteral("invalid-arc-fields.PcbLib"));
        QVERIFY(writeMalformedLibrary(invalidArcPath, invalidArcData));

        // invalidArcReader：测试使用的元器件标识或名称。
        AltiumPcbLibReader invalidArcReader;
        QVERIFY2(invalidArcReader.open(invalidArcPath), qPrintable(invalidArcReader.errorString()));
        QVERIFY(!invalidArcReader.readFootprintObjects(QStringLiteral("BROKEN"), &objects));
        QVERIFY(invalidArcReader.errorString().contains(QStringLiteral("弧线半径必须为正")));

        // invalidRegionData：测试使用的元器件标识或名称。
        QByteArray invalidRegionData;
        AltiumBinaryWriter invalidRegionWriter(invalidRegionData);
        invalidRegionWriter.writeStringBlock(QStringLiteral("BROKEN"));
        invalidRegionWriter.writeUInt8(AltiumConstants::PCB_OBJECT_REGION);
        invalidRegionWriter.beginBlock();
        invalidRegionWriter.writeUInt8(1);
        invalidRegionWriter.writeUInt16(0);
        invalidRegionWriter.writeBytes(QByteArray(10, '\0'));
        invalidRegionWriter.writeUInt32(0);
        invalidRegionWriter.writeUInt8(0);
        invalidRegionWriter.writeCStringParameterBlock({});
        invalidRegionWriter.writeUInt32(2);
        invalidRegionWriter.writeDouble(0.0);
        invalidRegionWriter.writeDouble(0.0);
        invalidRegionWriter.writeDouble(1.0);
        invalidRegionWriter.writeDouble(1.0);
        invalidRegionWriter.endBlock();
        // invalidRegionPath：测试使用的文件系统路径。
        const QString invalidRegionPath = QDir(tempDir.path()).filePath(QStringLiteral("invalid-region-fields.PcbLib"));
        QVERIFY(writeMalformedLibrary(invalidRegionPath, invalidRegionData));

        // invalidRegionReader：测试使用的元器件标识或名称。
        AltiumPcbLibReader invalidRegionReader;
        QVERIFY2(invalidRegionReader.open(invalidRegionPath), qPrintable(invalidRegionReader.errorString()));
        QVERIFY(!invalidRegionReader.readFootprintObjects(QStringLiteral("BROKEN"), &objects));
        QVERIFY(invalidRegionReader.errorString().contains(QStringLiteral("区域至少需要三个顶点")));

        // invalidBodyOpacityData：测试使用的元器件标识或名称。
        QByteArray invalidBodyOpacityData;
        AltiumBinaryWriter invalidBodyOpacityWriter(invalidBodyOpacityData);
        invalidBodyOpacityWriter.writeStringBlock(QStringLiteral("BROKEN"));
        invalidBodyOpacityWriter.writeUInt8(AltiumConstants::PCB_OBJECT_COMPONENT_BODY);
        invalidBodyOpacityWriter.beginBlock();
        invalidBodyOpacityWriter.writeUInt8(57);
        invalidBodyOpacityWriter.writeUInt16(0);
        invalidBodyOpacityWriter.writeBytes(QByteArray(10, '\0'));
        invalidBodyOpacityWriter.writeUInt32(0);
        invalidBodyOpacityWriter.writeUInt8(0);
        invalidBodyOpacityWriter.writeCStringParameterBlock({{QStringLiteral("BODYOPACITY3D"), QStringLiteral("NaN")}});
        invalidBodyOpacityWriter.writeUInt32(3);
        for (const QPointF& vertex : {QPointF(0.0, 0.0), QPointF(1.0, 0.0), QPointF(0.0, 1.0)}) {
            invalidBodyOpacityWriter.writeDouble(vertex.x());
            invalidBodyOpacityWriter.writeDouble(vertex.y());
        }
        invalidBodyOpacityWriter.endBlock();
        const QString invalidBodyOpacityPath =
            QDir(tempDir.path()).filePath(QStringLiteral("invalid-body-opacity.PcbLib"));
        QVERIFY(writeMalformedLibrary(invalidBodyOpacityPath, invalidBodyOpacityData));

        // invalidBodyOpacityReader：测试使用的元器件标识或名称。
        AltiumPcbLibReader invalidBodyOpacityReader;
        QVERIFY2(invalidBodyOpacityReader.open(invalidBodyOpacityPath),
                 qPrintable(invalidBodyOpacityReader.errorString()));
        QVERIFY(!invalidBodyOpacityReader.readFootprintObjects(QStringLiteral("BROKEN"), &objects));
        QVERIFY(invalidBodyOpacityReader.errorString().contains(QStringLiteral("透明度必须在 0..1 范围内")));

        // invalidLayerData：测试使用的元器件标识或名称。
        QByteArray invalidLayerData;
        AltiumBinaryWriter invalidLayerWriter(invalidLayerData);
        invalidLayerWriter.writeStringBlock(QStringLiteral("BROKEN"));
        invalidLayerWriter.writeUInt8(AltiumConstants::PCB_OBJECT_TRACK);
        invalidLayerWriter.beginBlock();
        invalidLayerWriter.writeUInt8(0);
        invalidLayerWriter.writeUInt16(0);
        invalidLayerWriter.writeBytes(QByteArray(10, '\0'));
        for (int i = 0; i < 4; ++i)
            invalidLayerWriter.writeInt32(0);
        invalidLayerWriter.writeInt32(100);
        invalidLayerWriter.writeUInt16(0xFFFF);
        invalidLayerWriter.writeUInt8(0);
        invalidLayerWriter.endBlock();
        // invalidLayerPath：测试使用的文件系统路径。
        const QString invalidLayerPath = QDir(tempDir.path()).filePath(QStringLiteral("invalid-layer.PcbLib"));
        QVERIFY(writeMalformedLibrary(invalidLayerPath, invalidLayerData));

        // invalidLayerReader：测试使用的元器件标识或名称。
        AltiumPcbLibReader invalidLayerReader;
        QVERIFY2(invalidLayerReader.open(invalidLayerPath), qPrintable(invalidLayerReader.errorString()));
        QVERIFY(!invalidLayerReader.readFootprintObjects(QStringLiteral("BROKEN"), &objects));
        QVERIFY(invalidLayerReader.errorString().contains(QStringLiteral("图元层号必须在 1..74")));

        // invalidTrackWidthData：测试使用的元器件标识或名称。
        QByteArray invalidTrackWidthData;
        AltiumBinaryWriter invalidTrackWidthWriter(invalidTrackWidthData);
        invalidTrackWidthWriter.writeStringBlock(QStringLiteral("BROKEN"));
        invalidTrackWidthWriter.writeUInt8(AltiumConstants::PCB_OBJECT_TRACK);
        invalidTrackWidthWriter.beginBlock();
        invalidTrackWidthWriter.writeUInt8(1);
        invalidTrackWidthWriter.writeUInt16(0);
        invalidTrackWidthWriter.writeBytes(QByteArray(10, '\0'));
        for (int i = 0; i < 4; ++i)
            invalidTrackWidthWriter.writeInt32(0);
        invalidTrackWidthWriter.writeInt32(0);
        invalidTrackWidthWriter.writeUInt16(0);
        invalidTrackWidthWriter.writeUInt8(0);
        invalidTrackWidthWriter.endBlock();
        const QString invalidTrackWidthPath =
            QDir(tempDir.path()).filePath(QStringLiteral("invalid-track-width.PcbLib"));
        QVERIFY(writeMalformedLibrary(invalidTrackWidthPath, invalidTrackWidthData));

        // invalidTrackWidthReader：测试使用的元器件标识或名称。
        AltiumPcbLibReader invalidTrackWidthReader;
        QVERIFY2(invalidTrackWidthReader.open(invalidTrackWidthPath),
                 qPrintable(invalidTrackWidthReader.errorString()));
        QVERIFY(!invalidTrackWidthReader.readFootprintObjects(QStringLiteral("BROKEN"), &objects));
        QVERIFY(invalidTrackWidthReader.errorString().contains(QStringLiteral("走线宽度必须为正")));

        // invalidArcWidthData：测试使用的元器件标识或名称。
        QByteArray invalidArcWidthData;
        AltiumBinaryWriter invalidArcWidthWriter(invalidArcWidthData);
        invalidArcWidthWriter.writeStringBlock(QStringLiteral("BROKEN"));
        invalidArcWidthWriter.writeUInt8(AltiumConstants::PCB_OBJECT_ARC);
        invalidArcWidthWriter.beginBlock();
        invalidArcWidthWriter.writeUInt8(1);
        invalidArcWidthWriter.writeUInt16(0);
        invalidArcWidthWriter.writeBytes(QByteArray(10, '\0'));
        invalidArcWidthWriter.writeInt32(0);
        invalidArcWidthWriter.writeInt32(0);
        invalidArcWidthWriter.writeInt32(100);
        invalidArcWidthWriter.writeDouble(0.0);
        invalidArcWidthWriter.writeDouble(360.0);
        invalidArcWidthWriter.writeInt32(0);
        invalidArcWidthWriter.endBlock();
        // invalidArcWidthPath：测试使用的文件系统路径。
        const QString invalidArcWidthPath = QDir(tempDir.path()).filePath(QStringLiteral("invalid-arc-width.PcbLib"));
        QVERIFY(writeMalformedLibrary(invalidArcWidthPath, invalidArcWidthData));

        // invalidArcWidthReader：测试使用的元器件标识或名称。
        AltiumPcbLibReader invalidArcWidthReader;
        QVERIFY2(invalidArcWidthReader.open(invalidArcWidthPath), qPrintable(invalidArcWidthReader.errorString()));
        QVERIFY(!invalidArcWidthReader.readFootprintObjects(QStringLiteral("BROKEN"), &objects));
        QVERIFY(invalidArcWidthReader.errorString().contains(QStringLiteral("弧线宽度必须为正")));

        // invalidTextSizeData：测试使用的元器件标识或名称。
        QByteArray invalidTextSizeData;
        AltiumBinaryWriter invalidTextSizeWriter(invalidTextSizeData);
        invalidTextSizeWriter.writeStringBlock(QStringLiteral("BROKEN"));
        invalidTextSizeWriter.writeUInt8(AltiumConstants::PCB_OBJECT_TEXT);
        invalidTextSizeWriter.beginBlock();
        invalidTextSizeWriter.writeUInt8(1);
        invalidTextSizeWriter.writeUInt16(0);
        invalidTextSizeWriter.writeBytes(QByteArray(10, '\0'));
        invalidTextSizeWriter.writeInt32(0);
        invalidTextSizeWriter.writeInt32(0);
        invalidTextSizeWriter.writeInt32(0);
        invalidTextSizeWriter.writeInt16(0);
        invalidTextSizeWriter.writeDouble(0.0);
        invalidTextSizeWriter.writeUInt8(0);
        invalidTextSizeWriter.writeInt32(0);
        invalidTextSizeWriter.writeUInt8(0);
        invalidTextSizeWriter.writeUInt8(0);
        invalidTextSizeWriter.writeUInt8(0);
        invalidTextSizeWriter.writeUInt8(0);
        invalidTextSizeWriter.writeBytes(QByteArray(71, '\0'));
        invalidTextSizeWriter.writeUInt32(0);
        invalidTextSizeWriter.writeBytes(QByteArray(41, '\0'));
        invalidTextSizeWriter.writeUInt8(0);
        invalidTextSizeWriter.writeBytes(QByteArray(65, '\0'));
        invalidTextSizeWriter.writeUInt32(0);
        invalidTextSizeWriter.endBlock();
        // textPayload：测试使用的文本或数据内容。
        QByteArray textPayload;
        AltiumBinaryWriter textPayloadWriter(textPayload);
        textPayloadWriter.writeUInt8(1);
        textPayloadWriter.writeBytes(QByteArrayLiteral("T"));
        invalidTextSizeWriter.beginBlock();
        invalidTextSizeWriter.writeBytes(textPayload);
        invalidTextSizeWriter.endBlock();
        // invalidTextSizePath：测试使用的文件系统路径。
        const QString invalidTextSizePath = QDir(tempDir.path()).filePath(QStringLiteral("invalid-text-size.PcbLib"));
        QVERIFY(writeMalformedLibrary(invalidTextSizePath, invalidTextSizeData));

        // invalidTextSizeReader：测试使用的元器件标识或名称。
        AltiumPcbLibReader invalidTextSizeReader;
        QVERIFY2(invalidTextSizeReader.open(invalidTextSizePath), qPrintable(invalidTextSizeReader.errorString()));
        QVERIFY(!invalidTextSizeReader.readFootprintObjects(QStringLiteral("BROKEN"), &objects));
        QVERIFY(invalidTextSizeReader.errorString().contains(QStringLiteral("文本尺寸必须有效")));

        // invalidTextIndexData：测试使用的元器件标识或名称。
        QByteArray invalidTextIndexData = invalidTextSizeData;
        // wideStringIndexOffset：测试使用的元器件标识或名称。
        const int wideStringIndexOffset = 4 + 7 + 1 + 4 + 13 + 12 + 2 + 8 + 1 + 4 + 4 + 71;
        QVERIFY(wideStringIndexOffset + 4 <= invalidTextIndexData.size());
        // textHeightOffset：测试使用的文本或数据内容。
        const int textHeightOffset = 4 + 7 + 1 + 4 + 13 + 8;
        invalidTextIndexData[textHeightOffset] = static_cast<char>(1);
        invalidTextIndexData[textHeightOffset + 1] = '\0';
        invalidTextIndexData[textHeightOffset + 2] = '\0';
        invalidTextIndexData[textHeightOffset + 3] = '\0';
        invalidTextIndexData[wideStringIndexOffset] = static_cast<char>(7);
        invalidTextIndexData[wideStringIndexOffset + 1] = '\0';
        invalidTextIndexData[wideStringIndexOffset + 2] = '\0';
        invalidTextIndexData[wideStringIndexOffset + 3] = '\0';
        // invalidTextIndexPath：测试使用的文件系统路径。
        const QString invalidTextIndexPath = QDir(tempDir.path()).filePath(QStringLiteral("invalid-text-index.PcbLib"));
        QVERIFY(writeMalformedLibrary(invalidTextIndexPath, invalidTextIndexData));

        // invalidTextIndexReader：测试使用的元器件标识或名称。
        AltiumPcbLibReader invalidTextIndexReader;
        QVERIFY2(invalidTextIndexReader.open(invalidTextIndexPath), qPrintable(invalidTextIndexReader.errorString()));
        QVERIFY(!invalidTextIndexReader.readFootprintObjects(QStringLiteral("BROKEN"), &objects));
        QVERIFY(invalidTextIndexReader.errorString().contains(QStringLiteral("不存在的 WideStrings 索引")));

        // validPadFootprint：测试使用的元器件标识或名称。
        IR::FootprintComponentIR validPadFootprint;
        validPadFootprint.name = QStringLiteral("VALID_PAD");
        // validPad：测试使用的元器件标识或名称。
        IR::FootprintPadIR validPad;
        validPad.number = QStringLiteral("1");
        validPad.position = QPointF(0.0, 0.0);
        validPad.size = QSizeF(1.0, 1.0);
        validPadFootprint.pads.append(validPad);
        // validPadPath：测试使用的文件系统路径。
        const QString validPadPath = QDir(tempDir.path()).filePath(QStringLiteral("valid-pad.PcbLib"));
        // validPadExporter：测试使用的元器件标识或名称。
        ExporterAltiumFootprint validPadExporter;
        QVERIFY(
            validPadExporter.exportFootprintLibrary({validPadFootprint}, QStringLiteral("valid-pad"), validPadPath));

        // validPadReader：测试使用的元器件标识或名称。
        AltiumPcbLibReader validPadReader;
        QVERIFY2(validPadReader.open(validPadPath), qPrintable(validPadReader.errorString()));
        // validPadData：测试使用的元器件标识或名称。
        QByteArray validPadData;
        QVERIFY(validPadReader.readFootprintStream(QStringLiteral("VALID_PAD"), QStringLiteral("Data"), &validPadData));
        AltiumBinaryReader validPadBinaryReader(validPadData);
        // validPadName：测试使用的元器件标识或名称。
        QString validPadName;
        QVERIFY(validPadBinaryReader.readStringBlock(&validPadName));
        // padObjectId：测试使用的元器件标识或名称。
        uint8_t padObjectId = 0;
        QVERIFY(validPadBinaryReader.readUInt8(&padObjectId));
        // invalidPadData：测试使用的元器件标识或名称。
        QByteArray invalidPadData;
        AltiumBinaryWriter invalidPadWriter(invalidPadData);
        invalidPadWriter.writeStringBlock(validPadName);
        invalidPadWriter.writeUInt8(padObjectId);
        for (int blockIndex = 0; blockIndex < 6; ++blockIndex) {
            // payload：测试用载荷数据。
            QByteArray payload;
            // flags：测试用标志位集合。
            uint8_t flags = 0;
            QVERIFY(validPadBinaryReader.readBlock(&payload, &flags));
            if (blockIndex == 4) {
                // 公共头部 13 字节、坐标 8 字节、三层尺寸 24 字节、孔径 4 字节后是形状字段。
                QVERIFY(payload.size() > 49);
                payload[49] = '\0';
            }
            invalidPadWriter.beginBlock(flags);
            invalidPadWriter.writeBytes(payload);
            invalidPadWriter.endBlock();
        }
        QVERIFY(validPadBinaryReader.remaining() == 0);
        // invalidPadPath：测试使用的文件系统路径。
        const QString invalidPadPath = QDir(tempDir.path()).filePath(QStringLiteral("invalid-pad-shape.PcbLib"));
        QVERIFY(writeMalformedLibrary(invalidPadPath, invalidPadData));

        // invalidPadReader：测试使用的元器件标识或名称。
        AltiumPcbLibReader invalidPadReader;
        QVERIFY2(invalidPadReader.open(invalidPadPath), qPrintable(invalidPadReader.errorString()));
        QVERIFY(!invalidPadReader.readFootprintObjects(QStringLiteral("BROKEN"), &objects));
        QVERIFY(invalidPadReader.errorString().contains(QStringLiteral("焊盘形状无效")));

        AltiumBinaryReader invalidThroughHoleBinaryReader(validPadData);
        // invalidThroughHolePadName：测试使用的元器件标识或名称。
        QString invalidThroughHolePadName;
        QVERIFY(invalidThroughHoleBinaryReader.readStringBlock(&invalidThroughHolePadName));
        // throughHolePadObjectId：测试使用的元器件标识或名称。
        uint8_t throughHolePadObjectId = 0;
        QVERIFY(invalidThroughHoleBinaryReader.readUInt8(&throughHolePadObjectId));
        // invalidThroughHolePadData：测试使用的元器件标识或名称。
        QByteArray invalidThroughHolePadData;
        AltiumBinaryWriter invalidThroughHolePadWriter(invalidThroughHolePadData);
        invalidThroughHolePadWriter.writeStringBlock(invalidThroughHolePadName);
        invalidThroughHolePadWriter.writeUInt8(throughHolePadObjectId);
        for (int blockIndex = 0; blockIndex < 6; ++blockIndex) {
            // payload：测试用载荷数据。
            QByteArray payload;
            // flags：测试用标志位集合。
            uint8_t flags = 0;
            QVERIFY(invalidThroughHoleBinaryReader.readBlock(&payload, &flags));
            if (blockIndex == 4) {
                // 公共头部首字节是层号；孔径位于公共头部和位置、三层尺寸之后。
                QVERIFY(payload.size() > 48);
                payload[0] = static_cast<char>(AltiumConstants::PCB_LAYER_MULTI);
                payload[45] = '\0';
                payload[46] = '\0';
                payload[47] = '\0';
                payload[48] = '\0';
            }
            invalidThroughHolePadWriter.beginBlock(flags);
            invalidThroughHolePadWriter.writeBytes(payload);
            invalidThroughHolePadWriter.endBlock();
        }
        QVERIFY(invalidThroughHoleBinaryReader.remaining() == 0);
        const QString invalidThroughHolePath =
            QDir(tempDir.path()).filePath(QStringLiteral("invalid-through-hole.PcbLib"));
        QVERIFY(writeMalformedLibrary(invalidThroughHolePath, invalidThroughHolePadData));

        // invalidThroughHoleReader：测试使用的元器件标识或名称。
        AltiumPcbLibReader invalidThroughHoleReader;
        QVERIFY2(invalidThroughHoleReader.open(invalidThroughHolePath),
                 qPrintable(invalidThroughHoleReader.errorString()));
        QVERIFY(!invalidThroughHoleReader.readFootprintObjects(QStringLiteral("BROKEN"), &objects));
        QVERIFY(invalidThroughHoleReader.errorString().contains(QStringLiteral("通孔焊盘孔径必须为正")));
    }

    /**
     * @brief 验证 SchLib 损坏记录提供诊断并保留原始流回退路径
     */
    void rejectsMalformedSchLibRecords() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // writer：负责写入目标格式的 writer 实例。
        OLECompoundWriter writer;
        QVERIFY(writer.create());
        // headerData：测试使用的文本或数据内容。
        QByteArray headerData;
        AltiumBinaryWriter headerWriter(headerData);
        headerWriter.writeCStringParameterBlock({{QStringLiteral("COMPCOUNT"), QStringLiteral("1")}});
        headerWriter.writeInt32(1);
        headerWriter.writeStringBlock(QStringLiteral("BROKEN"));
        QVERIFY(writer.writeStream(QStringLiteral("FileHeader"), headerData));
        QVERIFY(writer.addStorage(QStringLiteral("BROKEN")));
        const QByteArray malformedData(2, '\0');
        QVERIFY(writer.writeStream(QStringLiteral("BROKEN"), QStringLiteral("Data"), malformedData));
        // malformedStorage：损坏的图片存储数据。
        QByteArray malformedStorage;
        AltiumBinaryWriter storageWriter(malformedStorage);
        // validCompressedData：测试使用的元器件标识或名称。
        const QByteArray validCompressedData = qCompress(QByteArrayLiteral("valid-image-data"), 9).mid(4);
        storageWriter.writeCStringParameterBlock({{QStringLiteral("HEADER"), QStringLiteral("Icon storage")},
                                                  {QStringLiteral("Weight"), QStringLiteral("2")}});
        for (int i = 0; i < 2; ++i) {
            storageWriter.beginBlock(1);
            storageWriter.writeUInt8(0xD0);
            storageWriter.writeUInt8(5);
            storageWriter.writeBytes(QByteArrayLiteral("x.png"));
            storageWriter.writeUInt32(static_cast<quint32>(validCompressedData.size()));
            storageWriter.writeBytes(validCompressedData);
            storageWriter.endBlock();
        }
        QVERIFY(writer.writeStream(QStringLiteral("Storage"), malformedStorage));
        // path：测试使用的文件系统路径。
        const QString path = QDir(tempDir.path()).filePath(QStringLiteral("malformed-record.SchLib"));
        QVERIFY(writer.saveToFile(path));

        // reader：负责回读生成内容的 reader 实例。
        AltiumSchLibReader reader;
        QVERIFY2(reader.open(path), qPrintable(reader.errorString()));
        // records：回读得到的记录集合。
        QVector<AltiumSchLibReader::Record> records;
        QVERIFY(!reader.readComponentRecords(QStringLiteral("BROKEN"), &records));
        QVERIFY(records.isEmpty());
        QVERIFY(reader.hasError());
        QVERIFY(reader.errorString().contains(QStringLiteral("记录失败")));
        // imageEntries：测试用图像条目集合。
        QVector<AltiumSchLibReader::ImageStorageEntry> imageEntries;
        QVERIFY(!reader.readImageStorage(&imageEntries));
        QVERIFY(imageEntries.isEmpty());
        QVERIFY(reader.errorString().contains(QStringLiteral("重复或非法文件名")));
        // rawData：测试使用的文本或数据内容。
        QByteArray rawData;
        QVERIFY(reader.readComponentData(QStringLiteral("BROKEN"), &rawData));
        QCOMPARE(rawData, malformedData);
    }

    /**
     * @brief 验证 SchLib 图片 Storage 的损坏压缩数据会被拒绝
     */
    void rejectsCorruptSchLibImageCompression() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // writer：负责写入目标格式的 writer 实例。
        OLECompoundWriter writer;
        QVERIFY(writer.create());
        // headerData：测试使用的文本或数据内容。
        QByteArray headerData;
        AltiumBinaryWriter headerWriter(headerData);
        headerWriter.writeCStringParameterBlock({{QStringLiteral("COMPCOUNT"), QStringLiteral("1")}});
        headerWriter.writeInt32(1);
        headerWriter.writeStringBlock(QStringLiteral("BROKEN"));
        QVERIFY(writer.writeStream(QStringLiteral("FileHeader"), headerData));
        QVERIFY(writer.addStorage(QStringLiteral("BROKEN")));
        QVERIFY(writer.writeStream(QStringLiteral("BROKEN"), QStringLiteral("Data"), QByteArray()));

        // storageData：测试使用的文本或数据内容。
        QByteArray storageData;
        AltiumBinaryWriter storageWriter(storageData);
        storageWriter.writeCStringParameterBlock({{QStringLiteral("HEADER"), QStringLiteral("Icon storage")},
                                                  {QStringLiteral("Weight"), QStringLiteral("1")}});
        storageWriter.beginBlock(1);
        storageWriter.writeUInt8(0xD0);
        storageWriter.writeUInt8(5);
        storageWriter.writeBytes(QByteArrayLiteral("x.png"));
        storageWriter.writeUInt32(1);
        storageWriter.writeUInt8(0x78);
        storageWriter.endBlock();
        QVERIFY(writer.writeStream(QStringLiteral("Storage"), storageData));

        // path：测试使用的文件系统路径。
        const QString path = QDir(tempDir.path()).filePath(QStringLiteral("corrupt-image.SchLib"));
        QVERIFY(writer.saveToFile(path));

        // reader：负责回读生成内容的 reader 实例。
        AltiumSchLibReader reader;
        QVERIFY2(reader.open(path), qPrintable(reader.errorString()));
        // entries：测试用条目集合。
        QVector<AltiumSchLibReader::ImageStorageEntry> entries;
        QVERIFY(!reader.readImageStorage(&entries));
        QVERIFY(entries.isEmpty());
        QVERIFY(reader.errorString().contains(QStringLiteral("压缩数据无效")));
    }

    /**
     * @brief 验证 SchLib 图片 Storage 的压缩长度字段必须与实际载荷一致。
     */
    void rejectsMismatchedSchLibImageCompressionLength() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // writer：负责写入目标格式的 writer 实例。
        OLECompoundWriter writer;
        QVERIFY(writer.create());
        // headerData：测试使用的文本或数据内容。
        QByteArray headerData;
        AltiumBinaryWriter headerWriter(headerData);
        headerWriter.writeCStringParameterBlock({{QStringLiteral("COMPCOUNT"), QStringLiteral("1")}});
        headerWriter.writeInt32(1);
        headerWriter.writeStringBlock(QStringLiteral("BROKEN_LENGTH"));
        QVERIFY(writer.writeStream(QStringLiteral("FileHeader"), headerData));
        QVERIFY(writer.addStorage(QStringLiteral("BROKEN_LENGTH")));
        QVERIFY(writer.writeStream(QStringLiteral("BROKEN_LENGTH"), QStringLiteral("Data"), QByteArray()));

        // storageData：测试使用的文本或数据内容。
        QByteArray storageData;
        AltiumBinaryWriter storageWriter(storageData);
        storageWriter.writeCStringParameterBlock({{QStringLiteral("HEADER"), QStringLiteral("Icon storage")},
                                                  {QStringLiteral("Weight"), QStringLiteral("1")}});
        storageWriter.beginBlock(1);
        storageWriter.writeUInt8(0xD0);
        storageWriter.writeUInt8(5);
        storageWriter.writeBytes(QByteArrayLiteral("x.png"));
        storageWriter.writeUInt32(2);
        storageWriter.writeUInt8(0x78);
        storageWriter.endBlock();
        QVERIFY(writer.writeStream(QStringLiteral("Storage"), storageData));

        // path：测试使用的文件系统路径。
        const QString path = QDir(tempDir.path()).filePath(QStringLiteral("mismatched-image-length.SchLib"));
        QVERIFY(writer.saveToFile(path));

        // reader：负责回读生成内容的 reader 实例。
        AltiumSchLibReader reader;
        QVERIFY2(reader.open(path), qPrintable(reader.errorString()));
        // entries：测试用条目集合。
        QVector<AltiumSchLibReader::ImageStorageEntry> entries;
        QVERIFY(!reader.readImageStorage(&entries));
        QVERIFY(entries.isEmpty());
        QVERIFY(reader.errorString().contains(QStringLiteral("条目内容无效")));
    }

    /**
     * @brief 验证 SchLib 图片 Storage 的空解压数据会被拒绝。
     */
    void rejectsEmptySchLibImageCompression() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // writer：负责写入目标格式的 writer 实例。
        OLECompoundWriter writer;
        QVERIFY(writer.create());
        // headerData：测试使用的文本或数据内容。
        QByteArray headerData;
        AltiumBinaryWriter headerWriter(headerData);
        headerWriter.writeCStringParameterBlock({{QStringLiteral("COMPCOUNT"), QStringLiteral("1")}});
        headerWriter.writeInt32(1);
        headerWriter.writeStringBlock(QStringLiteral("EMPTY_IMAGE"));
        QVERIFY(writer.writeStream(QStringLiteral("FileHeader"), headerData));
        QVERIFY(writer.addStorage(QStringLiteral("EMPTY_IMAGE")));
        QVERIFY(writer.writeStream(QStringLiteral("EMPTY_IMAGE"), QStringLiteral("Data"), QByteArray()));

        // 合法的 zlib 空流：可以通过 inflate 完整性检查，但解压后没有任何图像数据。
        const QByteArray compressed = QByteArray::fromHex("789c030000000001");
        // storageData：测试使用的文本或数据内容。
        QByteArray storageData;
        AltiumBinaryWriter storageWriter(storageData);
        storageWriter.writeCStringParameterBlock({{QStringLiteral("HEADER"), QStringLiteral("Icon storage")},
                                                  {QStringLiteral("Weight"), QStringLiteral("1")}});
        storageWriter.beginBlock(1);
        storageWriter.writeUInt8(0xD0);
        storageWriter.writeUInt8(9);
        storageWriter.writeBytes(QByteArrayLiteral("empty.png"));
        storageWriter.writeUInt32(static_cast<quint32>(compressed.size()));
        storageWriter.writeBytes(compressed);
        storageWriter.endBlock();
        QVERIFY(writer.writeStream(QStringLiteral("Storage"), storageData));

        // path：测试使用的文件系统路径。
        const QString path = QDir(tempDir.path()).filePath(QStringLiteral("empty-image.SchLib"));
        QVERIFY(writer.saveToFile(path));

        // reader：负责回读生成内容的 reader 实例。
        AltiumSchLibReader reader;
        QVERIFY2(reader.open(path), qPrintable(reader.errorString()));
        // entries：测试用条目集合。
        QVector<AltiumSchLibReader::ImageStorageEntry> entries;
        QVERIFY(!reader.readImageStorage(&entries));
        QVERIFY(entries.isEmpty());
        QVERIFY(reader.errorString().contains(QStringLiteral("压缩数据无效")));
    }

    /**
     * @brief 验证 SchLib 参数数值字段损坏时不会静默回退
     */
    void rejectsMalformedSchLibRecordMetadata() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // writer：负责写入目标格式的 writer 实例。
        OLECompoundWriter writer;
        QVERIFY(writer.create());
        // headerData：测试使用的文本或数据内容。
        QByteArray headerData;
        AltiumBinaryWriter headerWriter(headerData);
        headerWriter.writeCStringParameterBlock(
            {{QStringLiteral("COMPCOUNT"), QStringLiteral("1")}, {QStringLiteral("PARTCOUNT0"), QStringLiteral("2")}});
        headerWriter.writeInt32(1);
        headerWriter.writeStringBlock(QStringLiteral("BROKEN_METADATA"));
        QVERIFY(writer.writeStream(QStringLiteral("FileHeader"), headerData));
        QVERIFY(writer.addStorage(QStringLiteral("BROKEN_METADATA")));
        // componentData：测试使用的文本或数据内容。
        QByteArray componentData;
        AltiumBinaryWriter componentWriter(componentData);
        componentWriter.writeCStringParameterBlock({{QStringLiteral("RECORD"), QStringLiteral("1")},
                                                    {QStringLiteral("FONTID"), QStringLiteral("not-a-number")}});
        QVERIFY(writer.writeStream(QStringLiteral("BROKEN_METADATA"), QStringLiteral("Data"), componentData));
        // path：测试使用的文件系统路径。
        const QString path = QDir(tempDir.path()).filePath(QStringLiteral("malformed-record-metadata.SchLib"));
        QVERIFY(writer.saveToFile(path));

        // reader：负责回读生成内容的 reader 实例。
        AltiumSchLibReader reader;
        QVERIFY2(reader.open(path), qPrintable(reader.errorString()));
        // records：回读得到的记录集合。
        QVector<AltiumSchLibReader::Record> records;
        QVERIFY(!reader.readComponentRecords(QStringLiteral("BROKEN_METADATA"), &records));
        QVERIFY(records.isEmpty());
        QVERIFY(reader.errorString().contains(QStringLiteral("FONTID 无效")));
        // rawData：测试使用的文本或数据内容。
        QByteArray rawData;
        QVERIFY(reader.readComponentData(QStringLiteral("BROKEN_METADATA"), &rawData));
        QCOMPARE(rawData, componentData);
    }

    /**
     * @brief 验证 SchLib 记录不会接受小于 -1 的 OWNERPARTID
     */
    void rejectsInvalidSchLibOwnerPartId() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // writer：负责写入目标格式的 writer 实例。
        OLECompoundWriter writer;
        QVERIFY(writer.create());
        // headerData：测试使用的文本或数据内容。
        QByteArray headerData;
        AltiumBinaryWriter headerWriter(headerData);
        headerWriter.writeCStringParameterBlock(
            {{QStringLiteral("COMPCOUNT"), QStringLiteral("1")}, {QStringLiteral("PARTCOUNT0"), QStringLiteral("2")}});
        headerWriter.writeInt32(1);
        headerWriter.writeStringBlock(QStringLiteral("INVALID_OWNER"));
        QVERIFY(writer.writeStream(QStringLiteral("FileHeader"), headerData));
        QVERIFY(writer.addStorage(QStringLiteral("INVALID_OWNER")));

        // componentData：测试使用的文本或数据内容。
        QByteArray componentData;
        AltiumBinaryWriter componentWriter(componentData);
        componentWriter.writeCStringParameterBlock(
            {{QStringLiteral("RECORD"), QStringLiteral("14")}, {QStringLiteral("OWNERPARTID"), QStringLiteral("-2")}});
        QVERIFY(writer.writeStream(QStringLiteral("INVALID_OWNER"), QStringLiteral("Data"), componentData));
        // path：测试使用的文件系统路径。
        const QString path = QDir(tempDir.path()).filePath(QStringLiteral("invalid-owner.SchLib"));
        QVERIFY(writer.saveToFile(path));

        // reader：负责回读生成内容的 reader 实例。
        AltiumSchLibReader reader;
        QVERIFY2(reader.open(path), qPrintable(reader.errorString()));
        // records：回读得到的记录集合。
        QVector<AltiumSchLibReader::Record> records;
        QVERIFY(!reader.readComponentRecords(QStringLiteral("INVALID_OWNER"), &records));
        QVERIFY(records.isEmpty());
        QVERIFY(reader.errorString().contains(QStringLiteral("OWNERPARTID 小于 -1")));
    }

    /**
     * @brief 验证 SchLib 组件名称和 SectionKeys 映射不会重复
     */
    void rejectsDuplicateSchLibComponentsAndSectionKeys() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const auto createLibrary = [&](const QString& fileName,
                                       const QStringList& names,
                                       const QStringList& sectionKeys,
                                       const QStringList& sectionKeyRefs) {
            // writer：负责写入目标格式的 writer 实例。
            OLECompoundWriter writer;
            if (!writer.create())
                return false;

            // headerData：测试使用的文本或数据内容。
            QByteArray headerData;
            AltiumBinaryWriter headerWriter(headerData);
            headerWriter.writeCStringParameterBlock({{QStringLiteral("COMPCOUNT"), QString::number(names.size())}});
            headerWriter.writeInt32(names.size());
            for (const QString& name : names)
                headerWriter.writeStringBlock(name);
            if (!writer.writeStream(QStringLiteral("FileHeader"), headerData))
                return false;

            if (!sectionKeys.isEmpty()) {
                // sectionKeyData：测试使用的文本或数据内容。
                QByteArray sectionKeyData;
                AltiumBinaryWriter sectionKeyWriter(sectionKeyData);
                // parameters：测试用参数集合。
                QMap<QString, QString> parameters;
                parameters[QStringLiteral("KeyCount")] = QString::number(sectionKeys.size());
                for (int i = 0; i < sectionKeys.size(); ++i) {
                    parameters[QStringLiteral("LibRef%1").arg(i)] =
                        sectionKeyRefs.isEmpty() ? names.at(i) : sectionKeyRefs.at(i);
                    parameters[QStringLiteral("SectionKey%1").arg(i)] = sectionKeys.at(i);
                }
                sectionKeyWriter.writeCStringParameterBlock(parameters);
                if (!writer.writeStream(QStringLiteral("SectionKeys"), sectionKeyData))
                    return false;
            }
            return writer.saveToFile(QDir(tempDir.path()).filePath(fileName));
        };

        QVERIFY(createLibrary(
            QStringLiteral("duplicate-components.SchLib"), {QStringLiteral("DUP"), QStringLiteral("dup")}, {}, {}));
        // duplicateComponentReader：测试使用的缓存或元器件对象。
        AltiumSchLibReader duplicateComponentReader;
        QVERIFY(!duplicateComponentReader.open(
            QDir(tempDir.path()).filePath(QStringLiteral("duplicate-components.SchLib"))));
        QVERIFY(duplicateComponentReader.errorString().contains(QStringLiteral("组件名称重复")));

        QVERIFY(createLibrary(QStringLiteral("duplicate-section-keys.SchLib"),
                              {QStringLiteral("A"), QStringLiteral("B")},
                              {QStringLiteral("SHARED"), QStringLiteral("shared")},
                              {}));
        // duplicateSectionKeyReader：负责回读生成内容的 reader 实例。
        AltiumSchLibReader duplicateSectionKeyReader;
        QVERIFY(!duplicateSectionKeyReader.open(
            QDir(tempDir.path()).filePath(QStringLiteral("duplicate-section-keys.SchLib"))));
        QVERIFY(duplicateSectionKeyReader.errorString().contains(QStringLiteral("SectionKeys 映射重复")));

        QVERIFY(createLibrary(QStringLiteral("case-mismatched-section-key.SchLib"),
                              {QStringLiteral("A"), QStringLiteral("B")},
                              {QStringLiteral("KEY_A"), QStringLiteral("KEY_B")},
                              {QStringLiteral("a"), QStringLiteral("B")}));
        // caseMismatchedReader：负责回读生成内容的 reader 实例。
        AltiumSchLibReader caseMismatchedReader;
        QVERIFY(!caseMismatchedReader.open(
            QDir(tempDir.path()).filePath(QStringLiteral("case-mismatched-section-key.SchLib"))));
        QVERIFY(caseMismatchedReader.errorString().contains(QStringLiteral("SectionKeys 包含未知组件")));
    }

    /**
     * @brief 验证 SchLib 同一字段的大小写别名不能携带冲突值
     */
    void rejectsConflictingSchLibParameterAliases() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // writer：负责写入目标格式的 writer 实例。
        OLECompoundWriter writer;
        QVERIFY(writer.create());
        // headerData：测试使用的文本或数据内容。
        QByteArray headerData;
        AltiumBinaryWriter headerWriter(headerData);
        headerWriter.writeCStringParameterBlock(
            {{QStringLiteral("COMPCOUNT"), QStringLiteral("1")}, {QStringLiteral("PARTCOUNT0"), QStringLiteral("2")}});
        headerWriter.writeInt32(1);
        headerWriter.writeStringBlock(QStringLiteral("CONFLICTING_ALIASES"));
        QVERIFY(writer.writeStream(QStringLiteral("FileHeader"), headerData));
        QVERIFY(writer.addStorage(QStringLiteral("CONFLICTING_ALIASES")));

        // componentData：测试使用的文本或数据内容。
        QByteArray componentData;
        AltiumBinaryWriter componentWriter(componentData);
        componentWriter.writeCStringParameterBlock({{QStringLiteral("RECORD"), QStringLiteral("14")},
                                                    {QStringLiteral("OWNERPARTID"), QStringLiteral("1")},
                                                    {QStringLiteral("OwnerPartId"), QStringLiteral("2")}});
        QVERIFY(writer.writeStream(QStringLiteral("CONFLICTING_ALIASES"), QStringLiteral("Data"), componentData));
        // path：测试使用的文件系统路径。
        const QString path = QDir(tempDir.path()).filePath(QStringLiteral("conflicting-aliases.SchLib"));
        QVERIFY(writer.saveToFile(path));

        // reader：负责回读生成内容的 reader 实例。
        AltiumSchLibReader reader;
        QVERIFY2(reader.open(path), qPrintable(reader.errorString()));
        // records：回读得到的记录集合。
        QVector<AltiumSchLibReader::Record> records;
        QVERIFY(!reader.readComponentRecords(QStringLiteral("CONFLICTING_ALIASES"), &records));
        QVERIFY(records.isEmpty());
        QVERIFY(reader.errorString().contains(QStringLiteral("数值字段无效")));
    }

    /**
     * @brief 验证 SchLib 内容记录索引不能重复或倒序。
     */
    void rejectsNonMonotonicSchLibRecordIndexes() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // writer：负责写入目标格式的 writer 实例。
        OLECompoundWriter writer;
        QVERIFY(writer.create());
        // headerData：测试使用的文本或数据内容。
        QByteArray headerData;
        AltiumBinaryWriter headerWriter(headerData);
        headerWriter.writeCStringParameterBlock({{QStringLiteral("COMPCOUNT"), QStringLiteral("1")},
                                                 {QStringLiteral("PARTCOUNT0"), QStringLiteral("2")},
                                                 {QStringLiteral("FontIdCount"), QStringLiteral("1")},
                                                 {QStringLiteral("FontName1"), QStringLiteral("Times New Roman")},
                                                 {QStringLiteral("Size1"), QStringLiteral("10")}});
        headerWriter.writeInt32(1);
        headerWriter.writeStringBlock(QStringLiteral("NON_MONOTONIC"));
        QVERIFY(writer.writeStream(QStringLiteral("FileHeader"), headerData));
        QVERIFY(writer.addStorage(QStringLiteral("NON_MONOTONIC")));

        // componentData：测试使用的文本或数据内容。
        QByteArray componentData;
        AltiumBinaryWriter componentWriter(componentData);
        componentWriter.writeCStringParameterBlock({{QStringLiteral("RECORD"), QStringLiteral("13")},
                                                    {QStringLiteral("OWNERPARTID"), QStringLiteral("1")},
                                                    {QStringLiteral("IndexInSheet"), QStringLiteral("1")}});
        componentWriter.writeCStringParameterBlock({{QStringLiteral("RECORD"), QStringLiteral("13")},
                                                    {QStringLiteral("OWNERPARTID"), QStringLiteral("1")},
                                                    {QStringLiteral("IndexInSheet"), QStringLiteral("1")}});
        QVERIFY(writer.writeStream(QStringLiteral("NON_MONOTONIC"), QStringLiteral("Data"), componentData));
        // path：测试使用的文件系统路径。
        const QString path = QDir(tempDir.path()).filePath(QStringLiteral("non-monotonic.SchLib"));
        QVERIFY(writer.saveToFile(path));

        // reader：负责回读生成内容的 reader 实例。
        AltiumSchLibReader reader;
        QVERIFY2(reader.open(path), qPrintable(reader.errorString()));
        // records：回读得到的记录集合。
        QVector<AltiumSchLibReader::Record> records;
        QVERIFY(!reader.readComponentRecords(QStringLiteral("NON_MONOTONIC"), &records));
        QVERIFY(records.isEmpty());
        QVERIFY(reader.errorString().contains(QStringLiteral("IndexInSheet 非递增")));
    }

    /**
     * @brief 验证二进制引脚也参与 SchLib 内容序号连续性校验。
     */
    void rejectsSchLibRecordIndexGapAcrossBinaryPin() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // writer：负责写入目标格式的 writer 实例。
        OLECompoundWriter writer;
        QVERIFY(writer.create());
        // headerData：测试使用的文本或数据内容。
        QByteArray headerData;
        AltiumBinaryWriter headerWriter(headerData);
        headerWriter.writeCStringParameterBlock(
            {{QStringLiteral("COMPCOUNT"), QStringLiteral("1")}, {QStringLiteral("PARTCOUNT0"), QStringLiteral("2")}});
        headerWriter.writeInt32(1);
        headerWriter.writeStringBlock(QStringLiteral("INDEX_GAP"));
        QVERIFY(writer.writeStream(QStringLiteral("FileHeader"), headerData));
        QVERIFY(writer.addStorage(QStringLiteral("INDEX_GAP")));

        // componentData：测试使用的文本或数据内容。
        QByteArray componentData;
        AltiumBinaryWriter componentWriter(componentData);
        componentWriter.writeCStringParameterBlock({{QStringLiteral("RECORD"), QStringLiteral("1")},
                                                    {QStringLiteral("LibReference"), QStringLiteral("INDEX_GAP")}});
        componentWriter.beginBlock(AltiumConstants::SCH_BLOCK_FLAG_BINARY_PIN);
        componentWriter.writeInt32(2);
        componentWriter.writeUInt8(0);
        componentWriter.writeInt16(1);
        componentWriter.writeUInt8(1);
        componentWriter.endBlock();
        componentWriter.writeCStringParameterBlock({{QStringLiteral("RECORD"), QStringLiteral("13")},
                                                    {QStringLiteral("OWNERPARTID"), QStringLiteral("1")},
                                                    {QStringLiteral("IndexInSheet"), QStringLiteral("2")}});
        QVERIFY(writer.writeStream(QStringLiteral("INDEX_GAP"), QStringLiteral("Data"), componentData));
        // path：测试使用的文件系统路径。
        const QString path = QDir(tempDir.path()).filePath(QStringLiteral("index-gap.SchLib"));
        QVERIFY(writer.saveToFile(path));

        // reader：负责回读生成内容的 reader 实例。
        AltiumSchLibReader reader;
        QVERIFY2(reader.open(path), qPrintable(reader.errorString()));
        // records：回读得到的记录集合。
        QVector<AltiumSchLibReader::Record> records;
        QVERIFY(!reader.readComponentRecords(QStringLiteral("INDEX_GAP"), &records));
        QVERIFY(records.isEmpty());
        QVERIFY(reader.errorString().contains(QStringLiteral("IndexInSheet 不连续")));
    }

    /**
     * @brief 验证 SchLib 已知几何记录的半径和点列字段不会静默接受无效值。
     */
    void rejectsInvalidSchLibGeometryRecords() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // writer：负责写入目标格式的 writer 实例。
        OLECompoundWriter writer;
        QVERIFY(writer.create());
        // headerData：测试使用的文本或数据内容。
        QByteArray headerData;
        AltiumBinaryWriter headerWriter(headerData);
        headerWriter.writeCStringParameterBlock(
            {{QStringLiteral("COMPCOUNT"), QStringLiteral("1")}, {QStringLiteral("PARTCOUNT0"), QStringLiteral("2")}});
        headerWriter.writeInt32(1);
        headerWriter.writeStringBlock(QStringLiteral("INVALID_GEOMETRY_RECORD"));
        QVERIFY(writer.writeStream(QStringLiteral("FileHeader"), headerData));
        QVERIFY(writer.addStorage(QStringLiteral("INVALID_GEOMETRY_RECORD")));

        // componentData：测试使用的文本或数据内容。
        QByteArray componentData;
        AltiumBinaryWriter componentWriter(componentData);
        componentWriter.writeCStringParameterBlock({{QStringLiteral("RECORD"), QStringLiteral("8")},
                                                    {QStringLiteral("Radius"), QStringLiteral("0")},
                                                    {QStringLiteral("SecondaryRadius"), QStringLiteral("1")}});
        QVERIFY(writer.writeStream(QStringLiteral("INVALID_GEOMETRY_RECORD"), QStringLiteral("Data"), componentData));
        // path：测试使用的文件系统路径。
        const QString path = QDir(tempDir.path()).filePath(QStringLiteral("invalid-geometry-record.SchLib"));
        QVERIFY(writer.saveToFile(path));

        // reader：负责回读生成内容的 reader 实例。
        AltiumSchLibReader reader;
        QVERIFY2(reader.open(path), qPrintable(reader.errorString()));
        // records：回读得到的记录集合。
        QVector<AltiumSchLibReader::Record> records;
        QVERIFY(!reader.readComponentRecords(QStringLiteral("INVALID_GEOMETRY_RECORD"), &records));
        QVERIFY(records.isEmpty());
        QVERIFY(reader.errorString().contains(QStringLiteral("Radius 无效")));
    }

    /**
     * @brief 验证 SchLib 圆弧和图片记录的半径、边界字段不会静默接受无效值。
     */
    void rejectsInvalidSchLibArcAndImageRecords() {
        const auto verifyRejectedRecord =
            [](const QMap<QString, QString>& parameters, const QString& expectedMessage, const QString& componentName) {
                // tempDir：测试使用的文件系统路径。
                QTemporaryDir tempDir;
                QVERIFY(tempDir.isValid());

                // writer：负责写入目标格式的 writer 实例。
                OLECompoundWriter writer;
                QVERIFY(writer.create());
                // headerData：测试使用的文本或数据内容。
                QByteArray headerData;
                AltiumBinaryWriter headerWriter(headerData);
                headerWriter.writeCStringParameterBlock({{QStringLiteral("COMPCOUNT"), QStringLiteral("1")}});
                headerWriter.writeInt32(1);
                headerWriter.writeStringBlock(componentName);
                QVERIFY(writer.writeStream(QStringLiteral("FileHeader"), headerData));
                QVERIFY(writer.addStorage(componentName));

                // componentData：测试使用的文本或数据内容。
                QByteArray componentData;
                AltiumBinaryWriter componentWriter(componentData);
                componentWriter.writeCStringParameterBlock(parameters);
                QVERIFY(writer.writeStream(componentName, QStringLiteral("Data"), componentData));
                // path：测试使用的文件系统路径。
                const QString path = QDir(tempDir.path()).filePath(componentName + QStringLiteral(".SchLib"));
                QVERIFY(writer.saveToFile(path));

                // reader：负责回读生成内容的 reader 实例。
                AltiumSchLibReader reader;
                QVERIFY2(reader.open(path), qPrintable(reader.errorString()));
                // records：回读得到的记录集合。
                QVector<AltiumSchLibReader::Record> records;
                QVERIFY(!reader.readComponentRecords(componentName, &records));
                QVERIFY(records.isEmpty());
                QVERIFY(reader.errorString().contains(expectedMessage));
            };

        verifyRejectedRecord(
            {{QStringLiteral("RECORD"), QStringLiteral("12")}, {QStringLiteral("Radius"), QStringLiteral("0")}},
            QStringLiteral("Radius 无效"),
            QStringLiteral("INVALID_ARC_RECORD"));
        verifyRejectedRecord({{QStringLiteral("RECORD"), QStringLiteral("30")},
                              {QStringLiteral("Location.X"), QStringLiteral("1")},
                              {QStringLiteral("Location.Y"), QStringLiteral("2")},
                              {QStringLiteral("Corner.X"), QStringLiteral("1")},
                              {QStringLiteral("Corner.Y"), QStringLiteral("3")}},
                             QStringLiteral("图片边界尺寸无效"),
                             QStringLiteral("INVALID_IMAGE_RECORD"));
        verifyRejectedRecord({{QStringLiteral("RECORD"), QStringLiteral("30")},
                              {QStringLiteral("Location.X"), QStringLiteral("1")},
                              {QStringLiteral("Location.Y"), QStringLiteral("2")},
                              {QStringLiteral("Corner.X"), QStringLiteral("3")},
                              {QStringLiteral("Corner.Y"), QStringLiteral("4")},
                              {QStringLiteral("Rotation"), QStringLiteral("nan")}},
                             QStringLiteral("图片旋转角度无效"),
                             QStringLiteral("INVALID_IMAGE_ROTATION_RECORD"));
        verifyRejectedRecord({{QStringLiteral("RECORD"), QStringLiteral("12")},
                              {QStringLiteral("Radius"), QStringLiteral("1")},
                              {QStringLiteral("StartAngle"), QStringLiteral("nan")}},
                             QStringLiteral("圆弧角度无效"),
                             QStringLiteral("INVALID_ARC_ANGLE_RECORD"));
    }

    /**
     * @brief 验证 SchLib IEEE 记录的编号、缩放、线宽和方向字段不会越界。
     */
    void rejectsInvalidSchLibIeeeRecords() {
        const auto verifyRejectedRecord =
            [](const QMap<QString, QString>& parameters, const QString& expectedMessage, const QString& componentName) {
                // tempDir：测试使用的文件系统路径。
                QTemporaryDir tempDir;
                QVERIFY(tempDir.isValid());

                // writer：负责写入目标格式的 writer 实例。
                OLECompoundWriter writer;
                QVERIFY(writer.create());
                // headerData：测试使用的文本或数据内容。
                QByteArray headerData;
                AltiumBinaryWriter headerWriter(headerData);
                headerWriter.writeCStringParameterBlock({{QStringLiteral("COMPCOUNT"), QStringLiteral("1")}});
                headerWriter.writeInt32(1);
                headerWriter.writeStringBlock(componentName);
                QVERIFY(writer.writeStream(QStringLiteral("FileHeader"), headerData));
                QVERIFY(writer.addStorage(componentName));

                // componentData：测试使用的文本或数据内容。
                QByteArray componentData;
                AltiumBinaryWriter componentWriter(componentData);
                componentWriter.writeCStringParameterBlock(parameters);
                QVERIFY(writer.writeStream(componentName, QStringLiteral("Data"), componentData));
                // path：测试使用的文件系统路径。
                const QString path = QDir(tempDir.path()).filePath(componentName + QStringLiteral(".SchLib"));
                QVERIFY(writer.saveToFile(path));

                // reader：负责回读生成内容的 reader 实例。
                AltiumSchLibReader reader;
                QVERIFY2(reader.open(path), qPrintable(reader.errorString()));
                // records：回读得到的记录集合。
                QVector<AltiumSchLibReader::Record> records;
                QVERIFY(!reader.readComponentRecords(componentName, &records));
                QVERIFY(records.isEmpty());
                QVERIFY(reader.errorString().contains(expectedMessage));
            };

        verifyRejectedRecord(
            {{QStringLiteral("RECORD"), QStringLiteral("3")}, {QStringLiteral("Symbol"), QStringLiteral("35")}},
            QStringLiteral("IEEE 符号编号无效"),
            QStringLiteral("INVALID_IEEE_SYMBOL_RECORD"));
        verifyRejectedRecord(
            {{QStringLiteral("RECORD"), QStringLiteral("3")}, {QStringLiteral("ScaleFactor"), QStringLiteral("0")}},
            QStringLiteral("IEEE 缩放因子无效"),
            QStringLiteral("INVALID_IEEE_SCALE_RECORD"));
        verifyRejectedRecord(
            {{QStringLiteral("RECORD"), QStringLiteral("3")}, {QStringLiteral("LineWidth"), QStringLiteral("4")}},
            QStringLiteral("IEEE 线宽索引无效"),
            QStringLiteral("INVALID_IEEE_WIDTH_RECORD"));
        verifyRejectedRecord(
            {{QStringLiteral("RECORD"), QStringLiteral("3")}, {QStringLiteral("Orientation"), QStringLiteral("4")}},
            QStringLiteral("IEEE 方向无效"),
            QStringLiteral("INVALID_IEEE_ORIENTATION_RECORD"));
    }

    /**
     * @brief 验证 SchLib 矩形类记录的退化边界和负文本边距会被读取器拒绝。
     */
    void rejectsDegenerateSchLibBounds() {
        const auto verifyRejectedRecord =
            [](const QMap<QString, QString>& parameters, const QString& expectedMessage, const QString& componentName) {
                // tempDir：测试使用的文件系统路径。
                QTemporaryDir tempDir;
                QVERIFY(tempDir.isValid());

                // writer：负责写入目标格式的 writer 实例。
                OLECompoundWriter writer;
                QVERIFY(writer.create());
                // headerData：测试使用的文本或数据内容。
                QByteArray headerData;
                AltiumBinaryWriter headerWriter(headerData);
                headerWriter.writeCStringParameterBlock({{QStringLiteral("COMPCOUNT"), QStringLiteral("1")}});
                headerWriter.writeInt32(1);
                headerWriter.writeStringBlock(componentName);
                QVERIFY(writer.writeStream(QStringLiteral("FileHeader"), headerData));
                QVERIFY(writer.addStorage(componentName));

                // componentData：测试使用的文本或数据内容。
                QByteArray componentData;
                AltiumBinaryWriter componentWriter(componentData);
                componentWriter.writeCStringParameterBlock(parameters);
                QVERIFY(writer.writeStream(componentName, QStringLiteral("Data"), componentData));
                // path：测试使用的文件系统路径。
                const QString path = QDir(tempDir.path()).filePath(componentName + QStringLiteral(".SchLib"));
                QVERIFY(writer.saveToFile(path));

                // reader：负责回读生成内容的 reader 实例。
                AltiumSchLibReader reader;
                QVERIFY2(reader.open(path), qPrintable(reader.errorString()));
                // records：回读得到的记录集合。
                QVector<AltiumSchLibReader::Record> records;
                QVERIFY(!reader.readComponentRecords(componentName, &records));
                QVERIFY(records.isEmpty());
                QVERIFY(reader.errorString().contains(expectedMessage));
            };

        const QMap<QString, QString> degenerateBounds = {{QStringLiteral("Location.X"), QStringLiteral("1")},
                                                         {QStringLiteral("Location.Y"), QStringLiteral("2")},
                                                         {QStringLiteral("Corner.X"), QStringLiteral("1")},
                                                         {QStringLiteral("Corner.Y"), QStringLiteral("3")}};
        for (const auto& testCase : {
                 std::pair{QStringLiteral("10"), QStringLiteral("圆角矩形边界尺寸无效")},
                 std::pair{QStringLiteral("13"), QStringLiteral("线段边界尺寸无效")},
                 std::pair{QStringLiteral("14"), QStringLiteral("矩形边界尺寸无效")},
                 std::pair{QStringLiteral("28"), QStringLiteral("文本框边界尺寸无效")},
             }) {
            // parameters：测试用参数集合。
            auto parameters = degenerateBounds;
            parameters.insert(QStringLiteral("RECORD"), testCase.first);
            verifyRejectedRecord(parameters, testCase.second, QStringLiteral("DEGENERATE_") + testCase.first);
        }

        // negativeMargin：测试用集合数据。
        auto negativeMargin = QMap<QString, QString>{{QStringLiteral("RECORD"), QStringLiteral("28")},
                                                     {QStringLiteral("TextMargin"), QStringLiteral("-1")}};
        verifyRejectedRecord(negativeMargin, QStringLiteral("TextMargin 无效"), QStringLiteral("NEGATIVE_MARGIN"));

        auto negativeFractionalMargin =
            QMap<QString, QString>{{QStringLiteral("RECORD"), QStringLiteral("28")},
                                   {QStringLiteral("TextMargin_Frac"), QStringLiteral("-1")}};
        verifyRejectedRecord(
            negativeFractionalMargin, QStringLiteral("TextMargin 无效"), QStringLiteral("NEGATIVE_FRACTIONAL_MARGIN"));
    }

    /**
     * @brief 验证 PcbLib 封装写入 UniqueIdPrimitiveInformation 流
     * @details 验证 Header 中的图元计数和 Data 中的 PRIMITIVEOBJECTID 条目
     */
    void pcbLibWritesUniqueIdPrimitiveInformation() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // footprint：测试用封装数据。
        AltiumPcbComponent footprint;
        footprint.name = QStringLiteral("TEST_UID");

        // pad：测试用焊盘数据。
        AltiumPcbPad pad;
        pad.designator = QStringLiteral("1");
        pad.sizeTopX = 20000;
        pad.sizeTopY = 20000;
        pad.sizeMidX = 20000;
        pad.sizeMidY = 20000;
        pad.sizeBotX = 20000;
        pad.sizeBotY = 20000;
        pad.isSMD = true;
        pad.layer = 1;
        footprint.pads.append(pad);

        // track：测试用线段图元。
        AltiumPcbTrack track;
        track.startX = 0;
        track.startY = 0;
        track.endX = 10000;
        track.endY = 0;
        track.width = 1000;
        track.layer = 33;
        footprint.tracks.append(track);

        // pcbPath：测试使用的文件系统路径。
        const QString pcbPath = QDir(tempDir.path()).filePath(QStringLiteral("test_uid.PcbLib"));
        // pcbWriter：负责写入目标格式的 writer 实例。
        AltiumPcbLibWriter pcbWriter;
        QVERIFY(pcbWriter.write({footprint}, pcbPath));

        // 验证 UniqueIdPrimitiveInformation 流存在
        QByteArray uidHeader;
        QVERIFY(readCfbStream(pcbPath, QStringLiteral("TEST_UID/UniqueIdPrimitiveInformation/Header"), uidHeader));
        QCOMPARE(readU32(uidHeader, 0), quint32(2));  // 1 pad + 1 track = 2 图元

        // uidData：测试使用的元器件标识或名称。
        QByteArray uidData;
        QVERIFY(readCfbStream(pcbPath, QStringLiteral("TEST_UID/UniqueIdPrimitiveInformation/Data"), uidData));
        // 验证包含 PRIMITIVEOBJECTID
        QVERIFY(uidData.contains("PRIMITIVEOBJECTID=Pad"));
        QVERIFY(uidData.contains("PRIMITIVEOBJECTID=Track"));
    }

    /**
     * @brief 验证 PcbLib 封装正确写入 3D 元件体（Object ID = 12）
     */
    void pcbLibWritesComponentBody() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // footprint：测试用封装数据。
        AltiumPcbComponent footprint;
        footprint.name = QStringLiteral("TEST_BODY");
        footprint.height = 1.0;

        // 需要至少一个图元来计算包围盒
        AltiumPcbPad pad;
        pad.designator = QStringLiteral("1");
        pad.locationX = -50000;
        pad.locationY = -50000;
        pad.sizeTopX = 100000;
        pad.sizeTopY = 100000;
        pad.sizeMidX = 100000;
        pad.sizeMidY = 100000;
        pad.sizeBotX = 100000;
        pad.sizeBotY = 100000;
        pad.isSMD = true;
        pad.layer = 1;
        footprint.pads.append(pad);

        // 添加一个 3D 模型
        AltiumPcbComponent::Model3D model;
        model.name = QStringLiteral("test.step");
        model.id = QStringLiteral("model-id");
        model.stepData = QByteArrayLiteral("ISO-10303-21;");
        footprint.models.append(model);

        // body：测试用主体数据。
        AltiumPcbComponentBody body;
        body.layerName = QStringLiteral(" mechanical1 ");
        body.name = QStringLiteral("__BODY__");
        body.modelId = model.id;
        body.modelName = model.name;
        body.overallHeightRaw = 10000;  // 1mil
        body.outline = {QPointF(-50000, -50000), QPointF(50000, -50000), QPointF(50000, 50000), QPointF(-50000, 50000)};
        footprint.bodies.append(body);

        // fill：填充属性。
        AltiumPcbFill fill;
        fill.corner1X = -20000;
        fill.corner1Y = -10000;
        fill.corner2X = 20000;
        fill.corner2Y = 10000;
        fill.rotation = 15.0;
        fill.layer = 33;
        footprint.fills.append(fill);

        // region：测试用区域图元。
        AltiumPcbRegion region;
        region.layer = 33;
        region.kind = 2;
        region.name = QStringLiteral("COURTYARD");
        region.vertices = {
            QPointF(-30000, -30000), QPointF(30000, -30000), QPointF(30000, 30000), QPointF(-30000, 30000)};
        footprint.regions.append(region);

        // pcbPath：测试使用的文件系统路径。
        const QString pcbPath = QDir(tempDir.path()).filePath(QStringLiteral("test_body.PcbLib"));
        // pcbWriter：负责写入目标格式的 writer 实例。
        AltiumPcbLibWriter pcbWriter;
        QVERIFY(pcbWriter.write({footprint}, pcbPath));

        // modelData：测试使用的文本或数据内容。
        QByteArray modelData;
        QVERIFY(readCfbStream(pcbPath, QStringLiteral("Library/Models/Data"), modelData));
        QVERIFY(modelData.contains("ID=model-id"));
        QVERIFY(modelData.contains("NAME=test.step"));
        QVERIFY(!modelData.contains("ID=model|id"));

        // 验证封装 Data 流包含 ComponentBody (Object ID = 12)
        QByteArray footprintData;
        QVERIFY(readCfbStream(pcbPath, QStringLiteral("TEST_BODY/Data"), footprintData));
        // Object ID 12 应该出现在数据中
        bool foundBody = false;
        for (int i = 0; i < footprintData.size(); ++i) {
            if (static_cast<unsigned char>(footprintData.at(i)) == 12) {
                foundBody = true;
                break;
            }
        }
        QVERIFY(foundBody);

        // reader：负责回读生成内容的 reader 实例。
        AltiumPcbLibReader reader;
        QVERIFY2(reader.open(pcbPath), qPrintable(reader.errorString()));
        // objects：测试构造的 JSON 数据对象。
        QVector<AltiumPcbLibReader::PrimitiveRecord> objects;
        QVERIFY2(reader.readFootprintObjects(QStringLiteral("TEST_BODY"), &objects), qPrintable(reader.errorString()));

        // foundFill：测试发现的填充属性。
        bool foundFill = false;
        // foundRegion：检测到的区域图元。
        bool foundRegion = false;
        // foundStructuredBody：检测到的格式记录。
        bool foundStructuredBody = false;
        for (const auto& object : objects) {
            if (object.objectId == AltiumConstants::PCB_OBJECT_FILL) {
                foundFill = true;
                QVERIFY(object.hasFillFields);
                QCOMPARE(object.fill.corner1X, fill.corner1X);
                QCOMPARE(object.fill.corner2Y, fill.corner2Y);
                QCOMPARE(object.fill.rotation, fill.rotation);
            } else if (object.objectId == AltiumConstants::PCB_OBJECT_REGION) {
                foundRegion = true;
                QVERIFY(object.hasRegionFields);
                QCOMPARE(object.region.parameters.value(QStringLiteral("NAME")), region.name);
                QCOMPARE(object.region.vertices.size(), region.vertices.size());
                QCOMPARE(object.region.vertices.first(), region.vertices.first());
            } else if (object.objectId == AltiumConstants::PCB_OBJECT_COMPONENT_BODY) {
                foundStructuredBody = true;
                QVERIFY(object.hasComponentBodyFields);
                QCOMPARE(object.componentBody.parameters.value(QStringLiteral("V7_LAYER")),
                         QStringLiteral("MECHANICAL1"));
                QCOMPARE(object.componentBody.parameters.value(QStringLiteral("MODEL.NAME")), body.modelName);
                QCOMPARE(object.componentBody.outline.size(), body.outline.size());
                QCOMPARE(object.componentBody.outline.last(), body.outline.last());
            }
        }
        QVERIFY(foundFill);
        QVERIFY(foundRegion);
        QVERIFY(foundStructuredBody);
    }

    /**
     * @brief 验证 SchLib 写入包含 UTF-8 中文参数的元件
     * @details 验证 %UTF8% 参数编码和中文文本正确序列化
     */
    void schLibWritesUtf8Parameters() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // symbol：测试用符号数据。
        AltiumSchComponent symbol;
        symbol.name = QStringLiteral("TEST_UTF8");
        symbol.description = QStringLiteral("测试中文描述");
        symbol.partCount = 1;
        symbol.sourceMetadata.insert(QStringLiteral("manufacturer"), QStringLiteral("测试厂商"));

        // pin：测试用引脚数据。
        AltiumSchPin pin;
        pin.name = QStringLiteral("A");
        pin.designator = QStringLiteral("1");
        pin.locationX = 100000;
        pin.locationY = 200000;
        pin.length = 100000;
        pin.electricalType = AltiumModels::PinElectricalType::Passive;
        pin.orientation = AltiumModels::PinOrientation::Right;
        symbol.pins.append(pin);

        // text：测试使用的文本或数据内容。
        AltiumSchText text;
        text.text = QStringLiteral("中文文本");
        text.locationX = 300000;
        text.locationY = 300000;
        text.fontId = 1;
        text.color = 0;
        symbol.texts.append(text);
        // arialText：测试使用的文本或数据内容。
        AltiumSchText arialText;
        arialText.text = QStringLiteral("Arial 文本");
        arialText.fontName = QStringLiteral("Arial");
        arialText.fontSizeMm = 25.4 / 72.0 * 10.0;
        symbol.texts.append(arialText);
        // lowercaseArialText：测试使用的文本或数据内容。
        AltiumSchText lowercaseArialText = arialText;
        lowercaseArialText.text = QStringLiteral("arial 文本");
        lowercaseArialText.fontName = QStringLiteral("arial");
        symbol.texts.append(lowercaseArialText);
        // invalidFontParameter：测试使用的元器件标识或名称。
        AltiumSchParameter invalidFontParameter;
        invalidFontParameter.name = QStringLiteral("InvalidFont");
        invalidFontParameter.value = QStringLiteral("回退字体");
        invalidFontParameter.fontId = 99;
        symbol.parameters.append(invalidFontParameter);

        // schPath：测试使用的文件系统路径。
        const QString schPath = QDir(tempDir.path()).filePath(QStringLiteral("test_utf8.SchLib"));
        // schWriter：负责写入目标格式的 writer 实例。
        AltiumSchLibWriter schWriter;
        QVERIFY(schWriter.write({symbol}, schPath));

        // schData：测试使用的文本或数据内容。
        QByteArray schData;
        QVERIFY(readCfbStream(schPath, QStringLiteral("TEST_UTF8/Data"), schData));

        // 验证包含 %UTF8% 参数
        QVERIFY(schData.contains("%UTF8%ComponentDescription=") || schData.contains("ComponentDescription="));
        QVERIFY(schData.contains("%UTF8%Text=") || schData.contains("Text="));
        QVERIFY(schData.contains(QStringLiteral("测试中文描述").toUtf8()));
        QVERIFY(schData.contains(QStringLiteral("测试厂商").toUtf8()));
        QVERIFY(schData.contains(QStringLiteral("中文文本").toUtf8()));

        // reader：负责回读生成内容的 reader 实例。
        AltiumSchLibReader reader;
        QVERIFY2(reader.open(schPath), qPrintable(reader.errorString()));
        // records：回读得到的记录集合。
        QVector<AltiumSchLibReader::Record> records;
        QVERIFY2(reader.readComponentRecords(QStringLiteral("TEST_UTF8"), &records), qPrintable(reader.errorString()));
        // sawFontReference：是否读取到字体引用。
        bool sawFontReference = false;
        // sawParameterFontReference：是否读取到参数字体引用。
        bool sawParameterFontReference = false;
        for (const auto& record : records) {
            if (record.parameters.value(QStringLiteral("RECORD")) == QStringLiteral("4")) {
                sawFontReference = true;
                if (record.parameters.value(QStringLiteral("Text")) == QStringLiteral("中文文本"))
                    QCOMPARE(record.fontId, 1);
                else
                    QCOMPARE(record.fontId, 2);
            }
            if (record.parameters.value(QStringLiteral("NAME")) == QStringLiteral("InvalidFont")) {
                sawParameterFontReference = true;
                QCOMPARE(record.fontId, 1);
            }
        }
        QVERIFY(sawFontReference);
        QVERIFY(sawParameterFontReference);
        QCOMPARE(reader.fonts().size(), 2);

        // rotatedTextSymbol：测试使用的文本或数据内容。
        IR::SymbolComponentIR rotatedTextSymbol;
        rotatedTextSymbol.name = QStringLiteral("ROTATED_TEXT");
        // rotatedText：测试使用的文本或数据内容。
        IR::SymbolTextIR rotatedText;
        rotatedText.text = QStringLiteral("方向");
        rotatedText.rotation = -90.0;
        rotatedText.fontSizeMm = 2.5;
        rotatedTextSymbol.texts.append(rotatedText);
        // rotatedParameter：旋转后的参数。
        IR::SymbolParameterIR rotatedParameter;
        rotatedParameter.name = QStringLiteral("角度参数");
        rotatedParameter.value = QStringLiteral("值");
        rotatedParameter.rotation = -90.0;
        rotatedParameter.fontSizeMm = 2.5;
        rotatedTextSymbol.parameters.append(rotatedParameter);
        // firstPartParameter：测试使用的元器件标识或名称。
        IR::SymbolParameterIR firstPartParameter;
        firstPartParameter.name = QStringLiteral("第一部件参数");
        firstPartParameter.value = QStringLiteral("P1");
        firstPartParameter.partIndex = 0;
        rotatedTextSymbol.parameters.append(firstPartParameter);
        // sharedFirstPartParameter：测试使用的元器件标识或名称。
        IR::SymbolParameterIR sharedFirstPartParameter;
        sharedFirstPartParameter.name = QStringLiteral("Shared");
        sharedFirstPartParameter.value = QStringLiteral("P1");
        sharedFirstPartParameter.partIndex = 0;
        rotatedTextSymbol.parameters.append(sharedFirstPartParameter);
        // sharedSecondPartParameter：测试使用的元器件标识或名称。
        IR::SymbolParameterIR sharedSecondPartParameter = sharedFirstPartParameter;
        sharedSecondPartParameter.value = QStringLiteral("P2");
        sharedSecondPartParameter.partIndex = 1;
        rotatedTextSymbol.parameters.append(sharedSecondPartParameter);
        // commonParameter：测试用公共参数。
        IR::SymbolParameterIR commonParameter;
        commonParameter.name = QStringLiteral("公共参数");
        commonParameter.value = QStringLiteral("COMMON");
        commonParameter.partIndex = -1;
        rotatedTextSymbol.parameters.append(commonParameter);
        rotatedTextSymbol.partCount = 2;
        // rotatedTextPath：测试使用的文件系统路径。
        const QString rotatedTextPath = QDir(tempDir.path()).filePath(QStringLiteral("rotated-text.SchLib"));
        // rotatedTextExporter：测试使用的文本或数据内容。
        ExporterAltiumSymbol rotatedTextExporter;
        QVERIFY(rotatedTextExporter.exportSymbolLibrary(
            {rotatedTextSymbol}, QStringLiteral("rotated-text"), rotatedTextPath, false, false));
        // rotatedTextData：测试使用的文本或数据内容。
        QByteArray rotatedTextData;
        QVERIFY(readCfbStream(rotatedTextPath, QStringLiteral("ROTATED_TEXT/Data"), rotatedTextData));
        QVERIFY(rotatedTextData.contains("Orientation=3"));
        QVERIFY(rotatedTextData.contains("NAME=第一部件参数"));
        QVERIFY(rotatedTextData.contains("NAME=公共参数"));
        QCOMPARE(rotatedTextData.count("NAME=Shared"), 2);
        // rotatedParameterOffset：旋转参数偏移量。
        const int rotatedParameterOffset = rotatedTextData.indexOf("NAME=角度参数");
        // rotatedParameterRecordOffset：旋转参数记录偏移量。
        const int rotatedParameterRecordOffset = rotatedTextData.lastIndexOf("|RECORD=41|", rotatedParameterOffset);
        QVERIFY(rotatedParameterOffset > rotatedParameterRecordOffset);
        QVERIFY(rotatedTextData.mid(rotatedParameterRecordOffset, rotatedParameterOffset - rotatedParameterRecordOffset)
                    .contains("FONTID=2"));
        QVERIFY(rotatedTextData.contains("OWNERPARTID=1"));
        QVERIFY(rotatedTextData.contains("OWNERPARTID=-1"));
    }

    /**
     * @brief 验证 makeUniqueSectionKeys 生成的键满足 CFB 大小写唯一性
     */
    void createsUniqueCfbSectionKeys() {
        const QStringList keys =
            AltiumWriterUtils::makeUniqueSectionKeys({QStringLiteral("Same/Name"),
                                                      QStringLiteral("same:name"),
                                                      QString(40, QLatin1Char('A')),
                                                      QString(39, QLatin1Char('A')) + QStringLiteral("B")});
        QCOMPARE(keys.size(), 4);
        // folded：折叠后的参数记录。
        QSet<QString> folded;
        for (const QString& key : keys) {
            QVERIFY(!key.isEmpty());
            QVERIFY(key.size() <= 31);
            QVERIFY(!folded.contains(key.toCaseFolded()));
            folded.insert(key.toCaseFolded());
        }
    }

    /**
     * @brief 验证 Altium 不会静默覆盖已有库
     */
    void rejectsUnsupportedAltiumLibraryMerge() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // outputPath：测试使用的文件系统路径。
        const QString outputPath = QDir(tempDir.path()).filePath(QStringLiteral("existing.SchLib"));
        QFile existingFile(outputPath);
        QVERIFY(existingFile.open(QIODevice::WriteOnly));
        // originalData：测试使用的文本或数据内容。
        const QByteArray originalData = QByteArrayLiteral("existing-library");
        QCOMPARE(existingFile.write(originalData), originalData.size());
        existingFile.close();

        // symbol：测试用符号数据。
        IR::SymbolComponentIR symbol;
        symbol.name = QStringLiteral("NEW_SYMBOL");
        // exporter：负责生成测试输出的导出器实例。
        ExporterAltiumSymbol exporter;
        QVERIFY(!exporter.exportSymbolLibrary({symbol}, QStringLiteral("existing"), outputPath, true, false));
        QVERIFY(!exporter.diagnostics().isEmpty());
        QVERIFY(exporter.diagnostics().first().contains(QStringLiteral("暂不支持")));

        QVERIFY(existingFile.open(QIODevice::ReadOnly));
        QCOMPARE(existingFile.readAll(), originalData);
        existingFile.close();

        QVERIFY(!exporter.exportSymbolLibrary({symbol}, QStringLiteral("existing"), outputPath, false, true));
        QVERIFY(!exporter.diagnostics().isEmpty());
    }

    /**
     * @brief 验证导出器保持多部件结构和折线逐段展开逻辑
     * @details 符号多部件的 OWNERPARTID 正确映射，封装折线按段写入 Track，
     * 安装孔转为非电镀 MultiLayer pad，板框保留层和线宽
     */
    void exportersPreserveMultipartAndPolylineStructure() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // symbol：测试用符号数据。
        IR::SymbolComponentIR symbol;
        symbol.name = QStringLiteral("MULTIPART");
        symbol.designatorPrefix = QStringLiteral("U");
        symbol.partCount = 2;
        // pin：测试用引脚数据。
        IR::SymbolPinIR pin;
        pin.name = QStringLiteral("B");
        pin.designator = QStringLiteral("2");
        pin.length = 2.54;
        pin.partIndex = 1;
        pin.style.decoration = IR::PinDecoration::AnalogInput;
        symbol.pins.append(pin);
        // commonPin：测试用公共引脚。
        IR::SymbolPinIR commonPin;
        commonPin.name = QStringLiteral("VCC");
        commonPin.designator = QStringLiteral("3");
        commonPin.length = 2.54;
        commonPin.commonToAllParts = true;
        symbol.pins.append(commonPin);
        // secondPartPin：测试使用的元器件标识或名称。
        IR::SymbolPinIR secondPartPin;
        secondPartPin.name = QStringLiteral("A");
        secondPartPin.designator = QStringLiteral("1");
        secondPartPin.length = 2.54;
        secondPartPin.partIndex = 0;
        symbol.pins.append(secondPartPin);
        // path：测试使用的文件系统路径。
        IR::SymbolPathIR path;
        path.points = {QPointF(0, 0), QPointF(1, 1), QPointF(2, 0)};
        path.partIndex = 1;
        symbol.paths.append(path);
        // bezier：测试用贝塞尔曲线。
        IR::SymbolBezierIR bezier;
        bezier.controlPoints = {QPointF(-1, 0), QPointF(-0.5, 2), QPointF(0.5, -2), QPointF(1, 0)};
        bezier.partIndex = 1;
        symbol.beziers.append(bezier);
        // roundedRectangle：测试用矩形图元。
        IR::SymbolRectangleIR roundedRectangle;
        roundedRectangle.x0 = -2.0;
        roundedRectangle.y0 = -1.0;
        roundedRectangle.x1 = 2.0;
        roundedRectangle.y1 = 1.0;
        roundedRectangle.cornerRadiusX = 0.25;
        roundedRectangle.cornerRadiusY = 0.2;
        roundedRectangle.partIndex = 1;
        symbol.rectangles.append(roundedRectangle);
        // commonRectangle：公共矩形记录。
        IR::SymbolRectangleIR commonRectangle;
        commonRectangle.x0 = -3.0;
        commonRectangle.y0 = -2.0;
        commonRectangle.x1 = -2.0;
        commonRectangle.y1 = -1.0;
        commonRectangle.partIndex = -1;
        symbol.rectangles.append(commonRectangle);
        // secondPartRectangle：测试使用的元器件标识或名称。
        IR::SymbolRectangleIR secondPartRectangle;
        secondPartRectangle.x0 = 4.0;
        secondPartRectangle.y0 = 3.0;
        secondPartRectangle.x1 = 5.0;
        secondPartRectangle.y1 = 4.0;
        secondPartRectangle.partIndex = 0;
        symbol.rectangles.append(secondPartRectangle);
        // firstPartPolygon：测试使用的元器件标识或名称。
        IR::SymbolPolygonIR firstPartPolygon;
        firstPartPolygon.points = {QPointF(10.0, 10.0), QPointF(11.0, 10.0), QPointF(10.0, 11.0)};
        firstPartPolygon.partIndex = 1;
        symbol.polygons.append(firstPartPolygon);
        // secondPartPolygon：测试使用的元器件标识或名称。
        IR::SymbolPolygonIR secondPartPolygon;
        secondPartPolygon.points = {QPointF(20.0, 20.0), QPointF(21.0, 20.0), QPointF(20.0, 21.0)};
        secondPartPolygon.partIndex = 0;
        symbol.polygons.append(secondPartPolygon);
        // commonText：测试使用的文本或数据内容。
        IR::SymbolTextIR commonText;
        commonText.text = QStringLiteral("COMMON_TEXT");
        commonText.partIndex = -1;
        symbol.texts.append(commonText);
        // partText：测试使用的元器件标识或名称。
        IR::SymbolTextIR partText;
        partText.text = QStringLiteral("PART_TEXT");
        partText.partIndex = 1;
        symbol.texts.append(partText);
        // partParameter：测试使用的元器件标识或名称。
        IR::SymbolParameterIR partParameter;
        partParameter.name = QStringLiteral("PartOnly");
        partParameter.value = QStringLiteral("Part 1 value");
        partParameter.partIndex = 1;
        symbol.parameters.append(partParameter);
        // image：测试用图像数据。
        IR::SymbolImageIR image;
        image.x0 = -1.0;
        image.y0 = -0.5;
        image.x1 = 1.0;
        image.y1 = 0.5;
        image.fileName = QStringLiteral("multipart.png");
        image.data = QByteArrayLiteral("multipart-image");
        symbol.images.append(image);
        // duplicateImage：重复图片记录。
        IR::SymbolImageIR duplicateImage = image;
        duplicateImage.data = QByteArrayLiteral("multipart-image-duplicate");
        duplicateImage.fileName = QStringLiteral("C:\\assets\\multipart.png");
        symbol.images.append(duplicateImage);
        symbol.graphicOrder = {
            {QStringLiteral("P"), 0, 1},
            {QStringLiteral("R"), 0, 1},
            {QStringLiteral("PT"), 0, 1},
            {QStringLiteral("P"), 0, 0},
            {QStringLiteral("P"), 1, 0},
            {QStringLiteral("R"), 0, -1},
            {QStringLiteral("R"), 0, 0},
            {QStringLiteral("PG"), 0, 1},
            {QStringLiteral("PG"), 0, 0},
            {QStringLiteral("T"), 0, -1},
            {QStringLiteral("T"), 0, 1},
        };

        // schPath：测试使用的文件系统路径。
        const QString schPath = QDir(tempDir.path()).filePath(QStringLiteral("multipart.SchLib"));
        // symbolExporter：负责生成测试输出的导出器实例。
        ExporterAltiumSymbol symbolExporter;
        QVERIFY(symbolExporter.exportSymbolLibrary({symbol}, QStringLiteral("multipart"), schPath, false, false));
        // symbolData：测试使用的文本或数据内容。
        QByteArray symbolData;
        QVERIFY(readCfbStream(schPath, QStringLiteral("MULTIPART/Data"), symbolData));
        // pinOffset：测试用引脚数据。
        const int pinOffset = 4 + static_cast<int>(readU32(symbolData, 0) & 0x00FFFFFFU);
        QCOMPARE(readU32(symbolData, pinOffset + 4), quint32(2));
        QCOMPARE(readU16(symbolData, pinOffset + 9), quint16(2));
        QCOMPARE(static_cast<uint8_t>(symbolData.at(pinOffset + 15)), static_cast<uint8_t>(5));
        // orderedPinOwners：测试用引脚数据。
        QList<quint16> orderedPinOwners;
        for (int offset = pinOffset; offset + 13 <= symbolData.size();) {
            // payloadSize：测试使用的数量或索引值。
            const int payloadSize = static_cast<int>(readU32(symbolData, offset) & 0x00FFFFFFU);
            // nextOffset：下一个测试偏移量。
            const int nextOffset = offset + 4 + payloadSize;
            if (payloadSize < 9 || nextOffset > symbolData.size())
                break;
            if (readU32(symbolData, offset + 4) == 2)
                orderedPinOwners.append(readU16(symbolData, offset + 9));
            offset = nextOffset;
        }
        QCOMPARE(orderedPinOwners, QList<quint16>({quint16(2), quint16(0xFFFF), quint16(1)}));
        QVERIFY(symbolData.contains("OWNERPARTID=2"));
        QVERIFY(symbolData.contains("OWNERPARTID=-1"));
        QVERIFY(symbolData.contains("IndexInSheet=1"));
        QVERIFY(symbolData.contains("IndexInSheet=2"));
        QVERIFY(symbolData.contains("RECORD=5"));
        QVERIFY(symbolData.contains("LocationCount=4"));
        QVERIFY(symbolData.contains("RECORD=10"));
        // firstPolygonRecord：第一条多边形记录。
        const int firstPolygonRecord = symbolData.indexOf("RECORD=7");
        // secondPolygonRecord：第二条多边形记录。
        const int secondPolygonRecord = symbolData.indexOf("RECORD=7", firstPolygonRecord + 1);
        QVERIFY(firstPolygonRecord > 0);
        QVERIFY(secondPolygonRecord > firstPolygonRecord);
        QVERIFY(symbolData.mid(firstPolygonRecord, secondPolygonRecord - firstPolygonRecord).contains("OWNERPARTID=2"));
        QVERIFY(symbolData.mid(secondPolygonRecord).contains("OWNERPARTID=1"));
        QVERIFY(symbolData.contains("RECORD=30"));
        // roundedRectangleRecord：圆角矩形记录。
        const int roundedRectangleRecord = symbolData.indexOf("RECORD=10");
        // pathRecord：测试使用的文件系统路径。
        const int pathRecord = symbolData.indexOf("RECORD=6");
        // rectangleRecord：矩形记录。
        const int rectangleRecord = symbolData.indexOf("RECORD=14");
        QVERIFY(roundedRectangleRecord > pinOffset);
        QVERIFY(pathRecord > roundedRectangleRecord);
        QVERIFY(rectangleRecord > pathRecord);
        QVERIFY(symbolData.contains("EmbedImage=T"));
        QVERIFY(symbolData.contains("FileName=multipart.png"));
        QVERIFY(symbolData.contains("FileName=multipart_2.png"));
        // commonTextOffset：测试使用的文本或数据内容。
        const int commonTextOffset = symbolData.indexOf("Text=COMMON_TEXT");
        // commonTextRecordOffset：测试使用的文本或数据内容。
        const int commonTextRecordOffset = symbolData.lastIndexOf("|RECORD=4|", commonTextOffset);
        QVERIFY(commonTextOffset > commonTextRecordOffset);
        QVERIFY(symbolData.mid(commonTextRecordOffset, commonTextOffset - commonTextRecordOffset)
                    .contains("OWNERPARTID=-1"));
        // partTextOffset：测试使用的元器件标识或名称。
        const int partTextOffset = symbolData.indexOf("Text=PART_TEXT");
        // partTextRecordOffset：测试使用的元器件标识或名称。
        const int partTextRecordOffset = symbolData.lastIndexOf("|RECORD=4|", partTextOffset);
        QVERIFY(partTextOffset > partTextRecordOffset);
        QVERIFY(symbolData.mid(partTextRecordOffset, partTextOffset - partTextRecordOffset).contains("OWNERPARTID=2"));
        // partParameterOffset：测试使用的元器件标识或名称。
        const int partParameterOffset = symbolData.indexOf("NAME=PartOnly");
        // partParameterRecordOffset：测试使用的元器件标识或名称。
        const int partParameterRecordOffset = symbolData.lastIndexOf("|RECORD=41|", partParameterOffset);
        QVERIFY(partParameterOffset > partParameterRecordOffset);
        // nextParameterRecordOffset：下一条参数记录偏移量。
        const int nextParameterRecordOffset = symbolData.indexOf("|RECORD=", partParameterRecordOffset + 1);
        QVERIFY(nextParameterRecordOffset > partParameterRecordOffset);
        const QByteArray partParameterRecord =
            symbolData.mid(partParameterRecordOffset, nextParameterRecordOffset - partParameterRecordOffset);
        QVERIFY(partParameterRecord.contains("OWNERPARTID=2"));
        QVERIFY(!partParameterRecord.contains("OWNERPARTDISPLAYMODE="));

        // multipartReader：测试使用的元器件标识或名称。
        AltiumSchLibReader multipartReader;
        QVERIFY2(multipartReader.open(schPath), qPrintable(multipartReader.errorString()));
        QCOMPARE(multipartReader.components().size(), 1);
        QCOMPARE(multipartReader.components().first().partCount, 2);
        // multipartRecords：测试使用的元器件标识或名称。
        QVector<AltiumSchLibReader::Record> multipartRecords;
        QVERIFY2(multipartReader.readComponentRecords(0, &multipartRecords), qPrintable(multipartReader.errorString()));
        // sawCommonPin：测试用引脚数据。
        bool sawCommonPin = false;
        // sawCommonRectangle：是否读取到公共矩形。
        bool sawCommonRectangle = false;
        // sawCommonText：测试使用的文本或数据内容。
        bool sawCommonText = false;
        // sawPartPin：测试使用的元器件标识或名称。
        bool sawPartPin = false;
        for (const auto& record : multipartRecords) {
            if (record.recordType == 2) {
                QCOMPARE(record.ownerPartDisplayMode, 0);
                sawCommonPin |= record.ownerPartId == -1;
                sawPartPin |= record.ownerPartId == 1 || record.ownerPartId == 2;
            } else if (record.hasParameters && (record.recordType == 14 || record.recordType == 4) &&
                       record.ownerPartId == -1) {
                QCOMPARE(record.ownerPartDisplayMode, -1);
                sawCommonRectangle |= record.recordType == 14;
                sawCommonText |= record.recordType == 4;
            }
        }
        QVERIFY(sawCommonPin);
        QVERIFY(sawCommonRectangle);
        QVERIFY(sawCommonText);
        QVERIFY(sawPartPin);

        // multipartStorage：测试使用的元器件标识或名称。
        QByteArray multipartStorage;
        QVERIFY(readCfbStream(schPath, QStringLiteral("Storage"), multipartStorage));
        QVERIFY(multipartStorage.contains("multipart.png"));
        QVERIFY(multipartStorage.contains("multipart_2.png"));
        // compressedImages：压缩图片映射。
        QMap<QString, QByteArray> compressedImages;
        QVERIFY(parseAltiumImageStorage(multipartStorage, compressedImages));
        QCOMPARE(compressedImages.keys(),
                 QStringList({QStringLiteral("multipart.png"), QStringLiteral("multipart_2.png")}));
        QCOMPARE(qUncompress(restoreQtCompressionHeader(compressedImages.value(QStringLiteral("multipart.png")),
                                                        QByteArrayLiteral("multipart-image").size())),
                 QByteArrayLiteral("multipart-image"));
        QCOMPARE(qUncompress(restoreQtCompressionHeader(compressedImages.value(QStringLiteral("multipart_2.png")),
                                                        QByteArrayLiteral("multipart-image-duplicate").size())),
                 QByteArrayLiteral("multipart-image-duplicate"));

        // diagnosticSymbol：测试用符号数据。
        AltiumSchComponent diagnosticSymbol;
        diagnosticSymbol.name = QStringLiteral("IMAGE_DIAGNOSTICS");
        // emptyImage：空图片数据。
        AltiumSchImage emptyImage;
        emptyImage.embedImage = true;
        emptyImage.fileName = QStringLiteral("empty.png");
        emptyImage.cornerX = 100000;
        emptyImage.cornerY = 100000;
        diagnosticSymbol.images.append(emptyImage);
        // invalidNameImage：测试使用的元器件标识或名称。
        AltiumSchImage invalidNameImage;
        invalidNameImage.embedImage = true;
        invalidNameImage.fileName = QStringLiteral("bad|name.png");
        invalidNameImage.data = QByteArrayLiteral("image");
        invalidNameImage.cornerX = 100000;
        invalidNameImage.cornerY = 100000;
        diagnosticSymbol.images.append(invalidNameImage);
        diagnosticSymbol.graphicOrder = {{QStringLiteral("T"), 0, 0}};
        // fallbackPolygon：回退多边形记录。
        AltiumSchPolygon fallbackPolygon;
        fallbackPolygon.vertices = {QPointF(100000, 100000), QPointF(200000, 100000), QPointF(100000, 200000)};
        diagnosticSymbol.polygons.append(fallbackPolygon);
        // diagnosticWriter：负责写入目标格式的 writer 实例。
        AltiumSchLibWriter diagnosticWriter;
        // diagnosticPath：测试使用的文件系统路径。
        const QString diagnosticPath = QDir(tempDir.path()).filePath(QStringLiteral("image-diagnostics.SchLib"));
        QVERIFY(diagnosticWriter.write({diagnosticSymbol}, diagnosticPath, QStringLiteral("IMAGE_DIAGNOSTICS")));
        // fallbackData：测试使用的文本或数据内容。
        QByteArray fallbackData;
        QVERIFY(readCfbStream(diagnosticPath, QStringLiteral("IMAGE_DIAGNOSTICS/Data"), fallbackData));
        QVERIFY(fallbackData.contains("RECORD=7"));
        QVERIFY(fallbackData.contains("FileName=empty.png"));
        QVERIFY(!fallbackData.contains("FileName=empty.png|EmbedImage=T"));
        QVERIFY(diagnosticWriter.diagnostics().contains(
            QStringLiteral("组件 IMAGE_DIAGNOSTICS 图片 0 的嵌入数据为空，已跳过 Storage")));
        QVERIFY(diagnosticWriter.diagnostics().contains(
            QStringLiteral("组件 IMAGE_DIAGNOSTICS 图片 1 的嵌入文件名无效: bad|name.png，已跳过 Storage")));
        // missingExternalNameImage：测试使用的元器件标识或名称。
        AltiumSchImage missingExternalNameImage;
        missingExternalNameImage.cornerX = 100000;
        missingExternalNameImage.cornerY = 100000;
        diagnosticSymbol.images.append(missingExternalNameImage);
        // invalidWindowsNameImage：测试使用的元器件标识或名称。
        AltiumSchImage invalidWindowsNameImage;
        invalidWindowsNameImage.embedImage = true;
        invalidWindowsNameImage.fileName = QStringLiteral("bad:name.png");
        invalidWindowsNameImage.data = QByteArrayLiteral("image");
        invalidWindowsNameImage.cornerX = 100000;
        invalidWindowsNameImage.cornerY = 100000;
        diagnosticSymbol.images.append(invalidWindowsNameImage);
        // oversizedNameImage：测试使用的元器件标识或名称。
        AltiumSchImage oversizedNameImage;
        oversizedNameImage.embedImage = true;
        oversizedNameImage.fileName = QString(256, QLatin1Char('a')) + QStringLiteral(".png");
        oversizedNameImage.data = QByteArrayLiteral("image");
        oversizedNameImage.cornerX = 100000;
        oversizedNameImage.cornerY = 100000;
        diagnosticSymbol.images.append(oversizedNameImage);
        // oversizedDiagnosticPath：测试使用的文件系统路径。
        const QString oversizedDiagnosticPath = QDir(tempDir.path()).filePath(QStringLiteral("oversized-image.SchLib"));
        QVERIFY(
            diagnosticWriter.write({diagnosticSymbol}, oversizedDiagnosticPath, QStringLiteral("IMAGE_DIAGNOSTICS")));
        QVERIFY(diagnosticWriter.diagnostics().join('\n').contains(QStringLiteral("图片 3 的嵌入文件名无效")));
        QVERIFY(diagnosticWriter.diagnostics().join('\n').contains(QStringLiteral("图片 4 的嵌入文件名超过 255 字节")));
        QVERIFY(diagnosticWriter.diagnostics().join('\n').contains(
            QStringLiteral("组件 IMAGE_DIAGNOSTICS 图片 2 的外部文件名为空，已跳过文件引用")));
        // diagnosticData：测试使用的文本或数据内容。
        QByteArray diagnosticData;
        QVERIFY(readCfbStream(oversizedDiagnosticPath, QStringLiteral("IMAGE_DIAGNOSTICS/Data"), diagnosticData));
        QVERIFY(!diagnosticData.contains("FileName=bad|name.png"));
        QVERIFY(!diagnosticData.contains("FileName=aaaaaaaa"));

        // invalidBezierSymbol：测试使用的元器件标识或名称。
        IR::SymbolComponentIR invalidBezierSymbol;
        invalidBezierSymbol.name = QStringLiteral("INVALID_BEZIER");
        // invalidBezier：测试使用的元器件标识或名称。
        IR::SymbolBezierIR invalidBezier;
        invalidBezier.controlPoints = {QPointF(0.0, 0.0), QPointF(1.0, 1.0)};
        invalidBezierSymbol.beziers.append(invalidBezier);
        // invalidBezierExporter：测试使用的元器件标识或名称。
        ExporterAltiumSymbol invalidBezierExporter;
        // invalidBezierPath：测试使用的文件系统路径。
        const QString invalidBezierPath = QDir(tempDir.path()).filePath(QStringLiteral("invalid-bezier.SchLib"));
        QVERIFY(invalidBezierExporter.exportSymbol(invalidBezierSymbol, invalidBezierPath));
        QVERIFY(invalidBezierExporter.diagnostics().contains(
            QStringLiteral("符号 INVALID_BEZIER Bézier 图元 0 的控制点参数无效（数量为 2），已跳过")));

        // invalidWriterInputSymbol：测试使用的元器件标识或名称。
        IR::SymbolComponentIR invalidWriterInputSymbol;
        invalidWriterInputSymbol.name = QStringLiteral("INVALID|WRITER_INPUT");
        // invalidWriterInputExporter：测试使用的元器件标识或名称。
        ExporterAltiumSymbol invalidWriterInputExporter;
        const QString invalidWriterInputPath =
            QDir(tempDir.path()).filePath(QStringLiteral("invalid-writer-input.SchLib"));
        QVERIFY(!invalidWriterInputExporter.exportSymbol(invalidWriterInputSymbol, invalidWriterInputPath));
        QVERIFY(invalidWriterInputExporter.diagnostics().contains(
            QStringLiteral("Altium SchLib 组件 INVALID|WRITER_INPUT 的组件名称包含参数分隔符或 NUL，已拒绝写入")));

        // invalidWriterLibraryExporter：测试使用的元器件标识或名称。
        ExporterAltiumSymbol invalidWriterLibraryExporter;
        const QString invalidWriterLibraryPath =
            QDir(tempDir.path()).filePath(QStringLiteral("invalid-writer-library.SchLib"));
        QVERIFY(!invalidWriterLibraryExporter.exportSymbolLibrary(
            {invalidWriterInputSymbol}, QStringLiteral("invalid-writer-library"), invalidWriterLibraryPath, false));
        QVERIFY(invalidWriterLibraryExporter.diagnostics().contains(
            QStringLiteral("Altium SchLib 组件 INVALID|WRITER_INPUT 的组件名称包含参数分隔符或 NUL，已拒绝写入")));

        // invalidPrimitivePointsSymbol：测试使用的元器件标识或名称。
        IR::SymbolComponentIR invalidPrimitivePointsSymbol;
        invalidPrimitivePointsSymbol.name = QStringLiteral("INVALID_PRIMITIVE_POINTS");
        // invalidPolyline：测试使用的元器件标识或名称。
        IR::SymbolPolylineIR invalidPolyline;
        invalidPolyline.points = {QPointF(0.0, 0.0)};
        invalidPrimitivePointsSymbol.polylines.append(invalidPolyline);
        // invalidFiniteBezier：测试使用的元器件标识或名称。
        IR::SymbolBezierIR invalidFiniteBezier;
        invalidFiniteBezier.controlPoints = {QPointF(0.0, 0.0),
                                             QPointF(1.0, 0.0),
                                             QPointF(std::numeric_limits<double>::infinity(), 1.0),
                                             QPointF(2.0, 0.0)};
        invalidPrimitivePointsSymbol.beziers.append(invalidFiniteBezier);
        // invalidPrimitivePointsExporter：测试使用的元器件标识或名称。
        ExporterAltiumSymbol invalidPrimitivePointsExporter;
        const QString invalidPrimitivePointsFile =
            QDir(tempDir.path()).filePath(QStringLiteral("invalid-primitive-points.SchLib"));
        QVERIFY(invalidPrimitivePointsExporter.exportSymbol(invalidPrimitivePointsSymbol, invalidPrimitivePointsFile));
        QVERIFY(invalidPrimitivePointsExporter.diagnostics().contains(
            QStringLiteral("符号 INVALID_PRIMITIVE_POINTS 折线图元 0 的点列无效，已跳过")));
        QVERIFY(invalidPrimitivePointsExporter.diagnostics().contains(
            QStringLiteral("符号 INVALID_PRIMITIVE_POINTS Bézier 图元 0 的控制点参数无效（数量为 4），已跳过")));

        // invalidGeometrySymbol：测试使用的元器件标识或名称。
        IR::SymbolComponentIR invalidGeometrySymbol;
        invalidGeometrySymbol.name = QStringLiteral("INVALID_GEOMETRY");
        // invalidCircle：测试使用的元器件标识或名称。
        IR::SymbolCircleIR invalidCircle;
        invalidCircle.radius = -1.0;
        invalidGeometrySymbol.circles.append(invalidCircle);
        // invalidStrokeCircle：测试使用的元器件标识或名称。
        IR::SymbolCircleIR invalidStrokeCircle;
        invalidStrokeCircle.strokeWidth = std::numeric_limits<double>::quiet_NaN();
        invalidGeometrySymbol.circles.append(invalidStrokeCircle);
        // degenerateArc：退化圆弧。
        IR::SymbolArcIR degenerateArc;
        degenerateArc.startPoint = QPointF(0.0, 0.0);
        degenerateArc.midPoint = QPointF(1.0, 1.0);
        degenerateArc.endPoint = QPointF(2.0, 2.0);
        invalidGeometrySymbol.arcs.append(degenerateArc);
        // invalidGeometryExporter：测试使用的元器件标识或名称。
        ExporterAltiumSymbol invalidGeometryExporter;
        // invalidGeometryPath：测试使用的文件系统路径。
        const QString invalidGeometryPath = QDir(tempDir.path()).filePath(QStringLiteral("invalid-geometry.SchLib"));
        QVERIFY(invalidGeometryExporter.exportSymbol(invalidGeometrySymbol, invalidGeometryPath));
        QVERIFY(invalidGeometryExporter.diagnostics().contains(
            QStringLiteral("符号 INVALID_GEOMETRY 圆图元 0 的半径无效，已跳过")));
        QVERIFY(invalidGeometryExporter.diagnostics().contains(
            QStringLiteral("符号 INVALID_GEOMETRY 圆图元 1 的中心或线宽无效，已跳过")));
        QVERIFY(invalidGeometryExporter.diagnostics().contains(
            QStringLiteral("符号 INVALID_GEOMETRY 圆弧图元 0 三点退化，已使用安全回退圆心")));

        // invalidArcGeometrySymbol：测试使用的元器件标识或名称。
        IR::SymbolComponentIR invalidArcGeometrySymbol;
        invalidArcGeometrySymbol.name = QStringLiteral("INVALID_ARC_GEOMETRY");
        // invalidPie：测试使用的元器件标识或名称。
        IR::SymbolPieIR invalidPie;
        invalidPie.radius = -1.0;
        invalidPie.startAngle = std::numeric_limits<double>::quiet_NaN();
        invalidArcGeometrySymbol.pies.append(invalidPie);
        // invalidEllipticalArc：测试使用的元器件标识或名称。
        IR::SymbolEllipticalArcIR invalidEllipticalArc;
        invalidEllipticalArc.radiusX = 0.0;
        invalidEllipticalArc.radiusY = 1.0;
        invalidArcGeometrySymbol.ellipticalArcs.append(invalidEllipticalArc);
        // invalidArcGeometryExporter：测试使用的元器件标识或名称。
        ExporterAltiumSymbol invalidArcGeometryExporter;
        const QString invalidArcGeometryPath =
            QDir(tempDir.path()).filePath(QStringLiteral("invalid-arc-geometry.SchLib"));
        QVERIFY(invalidArcGeometryExporter.exportSymbol(invalidArcGeometrySymbol, invalidArcGeometryPath));
        QVERIFY(invalidArcGeometryExporter.diagnostics().contains(
            QStringLiteral("符号 INVALID_ARC_GEOMETRY 扇形图元 0 的几何参数无效，已跳过")));
        QVERIFY(invalidArcGeometryExporter.diagnostics().contains(
            QStringLiteral("符号 INVALID_ARC_GEOMETRY 椭圆弧图元 0 的几何参数无效，已跳过")));

        // invalidTextSymbol：测试使用的元器件标识或名称。
        IR::SymbolComponentIR invalidTextSymbol;
        invalidTextSymbol.name = QStringLiteral("INVALID_TEXT_DATA");
        // invalidText：测试使用的元器件标识或名称。
        IR::SymbolTextIR invalidText;
        invalidText.text = QStringLiteral("label");
        invalidText.position = QPointF(std::numeric_limits<double>::quiet_NaN(), 0.0);
        invalidTextSymbol.texts.append(invalidText);
        // invalidTextFrame：测试使用的元器件标识或名称。
        IR::SymbolTextFrameIR invalidTextFrame;
        invalidTextFrame.textMargin = -1.0;
        invalidTextSymbol.textFrames.append(invalidTextFrame);
        // invalidParameter：测试使用的元器件标识或名称。
        IR::SymbolParameterIR invalidParameter;
        invalidParameter.name = QStringLiteral("Custom");
        invalidParameter.position = QPointF(0.0, std::numeric_limits<double>::infinity());
        invalidTextSymbol.parameters.append(invalidParameter);
        // invalidTextExporter：测试使用的元器件标识或名称。
        ExporterAltiumSymbol invalidTextExporter;
        // invalidTextPath：测试使用的文件系统路径。
        const QString invalidTextPath = QDir(tempDir.path()).filePath(QStringLiteral("invalid-text-data.SchLib"));
        QVERIFY(invalidTextExporter.exportSymbol(invalidTextSymbol, invalidTextPath));
        QVERIFY(invalidTextExporter.diagnostics().contains(
            QStringLiteral("符号 INVALID_TEXT_DATA 文本图元 0 的内容或几何参数无效，已跳过")));
        QVERIFY(invalidTextExporter.diagnostics().contains(
            QStringLiteral("符号 INVALID_TEXT_DATA 文本框图元 0 的几何参数无效，已跳过")));
        QVERIFY(invalidTextExporter.diagnostics().contains(
            QStringLiteral("符号 INVALID_TEXT_DATA 参数 Custom 的位置、旋转角度或名称无效，已跳过")));

        // invalidTextAnchorSymbol：测试使用的元器件标识或名称。
        IR::SymbolComponentIR invalidTextAnchorSymbol;
        invalidTextAnchorSymbol.name = QStringLiteral("INVALID_TEXT_ANCHOR");
        // invalidTextAnchor：测试使用的元器件标识或名称。
        IR::SymbolTextIR invalidTextAnchor;
        invalidTextAnchor.text = QStringLiteral("Anchor");
        invalidTextAnchor.anchor = QStringLiteral("unsupported");
        invalidTextAnchorSymbol.texts.append(invalidTextAnchor);
        // invalidPinAnchor：测试使用的元器件标识或名称。
        IR::SymbolPinIR invalidPinAnchor;
        invalidPinAnchor.name = QStringLiteral("Pin");
        invalidPinAnchor.designator = QStringLiteral("7");
        invalidPinAnchor.position = QPointF(0.0, 0.0);
        invalidPinAnchor.length = 2.54;
        invalidPinAnchor.hasNamePosition = true;
        invalidPinAnchor.namePosition = QPointF(0.0, 0.0);
        invalidPinAnchor.nameAnchor = QStringLiteral("unsupported");
        invalidTextAnchorSymbol.pins.append(invalidPinAnchor);
        // invalidTextAnchorExporter：测试使用的元器件标识或名称。
        ExporterAltiumSymbol invalidTextAnchorExporter;
        const QString invalidTextAnchorPath =
            QDir(tempDir.path()).filePath(QStringLiteral("invalid-text-anchor.SchLib"));
        QVERIFY(invalidTextAnchorExporter.exportSymbol(invalidTextAnchorSymbol, invalidTextAnchorPath));
        QVERIFY2(invalidTextAnchorExporter.diagnostics().join('\n').contains(
                     QStringLiteral("符号 INVALID_TEXT_ANCHOR 文本图元 0 的对齐锚点无效")),
                 qPrintable(invalidTextAnchorExporter.diagnostics().join('\n')));
        QVERIFY(invalidTextAnchorExporter.diagnostics().join('\n').contains(
            QStringLiteral("符号 INVALID_TEXT_ANCHOR 引脚 7 名称文本 的对齐锚点无效")));

        // invalidParameterFont：测试使用的元器件标识或名称。
        IR::SymbolParameterIR invalidParameterFont;
        invalidParameterFont.name = QStringLiteral("FontInvalid");
        invalidParameterFont.fontSizeMm = std::numeric_limits<double>::quiet_NaN();
        invalidTextSymbol.parameters.append(invalidParameterFont);
        // invalidParameterFontExporter：测试使用的元器件标识或名称。
        ExporterAltiumSymbol invalidParameterFontExporter;
        const QString invalidParameterFontPath =
            QDir(tempDir.path()).filePath(QStringLiteral("invalid-parameter-font.SchLib"));
        QVERIFY(invalidParameterFontExporter.exportSymbol(invalidTextSymbol, invalidParameterFontPath));
        QVERIFY(invalidParameterFontExporter.diagnostics().contains(
            QStringLiteral("符号 INVALID_TEXT_DATA 参数 FontInvalid 的字体大小无效，已跳过")));

        // invalidCommonPartIndexSymbol：测试使用的元器件标识或名称。
        IR::SymbolComponentIR invalidCommonPartIndexSymbol;
        invalidCommonPartIndexSymbol.name = QStringLiteral("INVALID_COMMON_PART_INDEX");
        // invalidCommonPartIndexRectangle：测试使用的元器件标识或名称。
        IR::SymbolRectangleIR invalidCommonPartIndexRectangle;
        invalidCommonPartIndexRectangle.partIndex = -2;
        invalidCommonPartIndexRectangle.x0 = 0.0;
        invalidCommonPartIndexRectangle.y0 = 0.0;
        invalidCommonPartIndexRectangle.x1 = 1.0;
        invalidCommonPartIndexRectangle.y1 = 1.0;
        invalidCommonPartIndexSymbol.rectangles.append(invalidCommonPartIndexRectangle);
        // invalidCommonPartIndexExporter：测试使用的元器件标识或名称。
        ExporterAltiumSymbol invalidCommonPartIndexExporter;
        const QString invalidCommonPartIndexPath =
            QDir(tempDir.path()).filePath(QStringLiteral("invalid-common-part-index.SchLib"));
        QVERIFY(!invalidCommonPartIndexExporter.exportSymbol(invalidCommonPartIndexSymbol, invalidCommonPartIndexPath));
        QVERIFY(invalidCommonPartIndexExporter.diagnostics().contains(QStringLiteral(
            "Altium SchLib 组件 INVALID_COMMON_PART_INDEX 的矩形图元 OWNERPARTID=-2 超出部件范围，已拒绝写入")));

        // invalidBoundsSymbol：测试使用的元器件标识或名称。
        IR::SymbolComponentIR invalidBoundsSymbol;
        invalidBoundsSymbol.name = QStringLiteral("INVALID_BOUNDS_DATA");
        // invalidRectangle：测试使用的元器件标识或名称。
        IR::SymbolRectangleIR invalidRectangle;
        invalidRectangle.x1 = std::numeric_limits<double>::quiet_NaN();
        invalidBoundsSymbol.rectangles.append(invalidRectangle);
        // zeroTextFrame：测试使用的文本或数据内容。
        IR::SymbolTextFrameIR zeroTextFrame;
        zeroTextFrame.x1 = 0.0;
        zeroTextFrame.y1 = 0.0;
        invalidBoundsSymbol.textFrames.append(zeroTextFrame);
        // invalidBoundsExporter：测试使用的元器件标识或名称。
        ExporterAltiumSymbol invalidBoundsExporter;
        // invalidBoundsPath：测试使用的文件系统路径。
        const QString invalidBoundsPath = QDir(tempDir.path()).filePath(QStringLiteral("invalid-bounds-data.SchLib"));
        QVERIFY(invalidBoundsExporter.exportSymbol(invalidBoundsSymbol, invalidBoundsPath));
        QVERIFY(invalidBoundsExporter.diagnostics().contains(
            QStringLiteral("符号 INVALID_BOUNDS_DATA 矩形图元 0 的边界无效，已跳过")));
        QVERIFY(invalidBoundsExporter.diagnostics().contains(
            QStringLiteral("符号 INVALID_BOUNDS_DATA 文本框图元 0 的几何参数无效，已跳过")));

        // invalidImageSymbol：测试使用的元器件标识或名称。
        IR::SymbolComponentIR invalidImageSymbol;
        invalidImageSymbol.name = QStringLiteral("INVALID_IMAGE_DATA");
        // invalidImage：测试使用的元器件标识或名称。
        IR::SymbolImageIR invalidImage;
        invalidImage.x1 = std::numeric_limits<double>::quiet_NaN();
        invalidImage.fileName = QStringLiteral("image.png");
        invalidImageSymbol.images.append(invalidImage);
        // invalidImageExporter：测试使用的元器件标识或名称。
        ExporterAltiumSymbol invalidImageExporter;
        // invalidImagePath：测试使用的文件系统路径。
        const QString invalidImagePath = QDir(tempDir.path()).filePath(QStringLiteral("invalid-image-data.SchLib"));
        QVERIFY(invalidImageExporter.exportSymbol(invalidImageSymbol, invalidImagePath));
        QVERIFY(invalidImageExporter.diagnostics().contains(
            QStringLiteral("符号 INVALID_IMAGE_DATA 图片图元 0 的边界、线宽或资源无效，已跳过")));

        // malformedDataUrlImageSymbol：测试使用的文本或数据内容。
        IR::SymbolComponentIR malformedDataUrlImageSymbol;
        malformedDataUrlImageSymbol.name = QStringLiteral("MALFORMED_DATA_URL_IMAGE");
        // malformedDataUrlImage：测试使用的文本或数据内容。
        IR::SymbolImageIR malformedDataUrlImage;
        malformedDataUrlImage.x1 = 1.0;
        malformedDataUrlImage.y1 = 1.0;
        malformedDataUrlImage.fileName = QStringLiteral("data:image/png;base64,not-decoded");
        malformedDataUrlImageSymbol.images.append(malformedDataUrlImage);
        // malformedDataUrlImageExporter：测试使用的文本或数据内容。
        ExporterAltiumSymbol malformedDataUrlImageExporter;
        const QString malformedDataUrlImagePath =
            QDir(tempDir.path()).filePath(QStringLiteral("malformed-data-url-image.SchLib"));
        QVERIFY(malformedDataUrlImageExporter.exportSymbol(malformedDataUrlImageSymbol, malformedDataUrlImagePath));
        QVERIFY(malformedDataUrlImageExporter.diagnostics().contains(
            QStringLiteral("符号 MALFORMED_DATA_URL_IMAGE 图片图元 0 包含未解析的 data URL，已跳过")));
        // malformedDataUrlImageData：测试使用的文本或数据内容。
        QByteArray malformedDataUrlImageData;
        QVERIFY(readCfbStream(
            malformedDataUrlImagePath, QStringLiteral("MALFORMED_DATA_URL_IMAGE/Data"), malformedDataUrlImageData));
        QVERIFY(!malformedDataUrlImageData.contains("RECORD=30"));

        // invalidArcSymbol：测试使用的元器件标识或名称。
        IR::SymbolComponentIR invalidArcSymbol;
        invalidArcSymbol.name = QStringLiteral("INVALID_ARC_DATA");
        // invalidArc：测试使用的元器件标识或名称。
        IR::SymbolArcIR invalidArc;
        invalidArc.startPoint = QPointF(std::numeric_limits<double>::quiet_NaN(), 0.0);
        invalidArcSymbol.arcs.append(invalidArc);
        // invalidIeee：测试使用的元器件标识或名称。
        IR::SymbolIeeeIR invalidIeee;
        invalidIeee.position = QPointF(0.0, std::numeric_limits<double>::infinity());
        invalidArcSymbol.ieeeSymbols.append(invalidIeee);
        // invalidArcExporter：测试使用的元器件标识或名称。
        ExporterAltiumSymbol invalidArcExporter;
        // invalidArcPath：测试使用的文件系统路径。
        const QString invalidArcPath = QDir(tempDir.path()).filePath(QStringLiteral("invalid-arc-data.SchLib"));
        QVERIFY(invalidArcExporter.exportSymbol(invalidArcSymbol, invalidArcPath));
        QVERIFY(invalidArcExporter.diagnostics().contains(
            QStringLiteral("符号 INVALID_ARC_DATA 圆弧图元 0 的点列或线宽无效，已跳过")));
        QVERIFY(invalidArcExporter.diagnostics().contains(
            QStringLiteral("符号 INVALID_ARC_DATA IEEE 图形 0 的位置无效，已跳过")));

        // zeroRadiusArcSymbol：测试用符号数据。
        IR::SymbolComponentIR zeroRadiusArcSymbol;
        zeroRadiusArcSymbol.name = QStringLiteral("ZERO_RADIUS_ARC");
        // zeroRadiusArc：零半径圆弧。
        IR::SymbolArcIR zeroRadiusArc;
        zeroRadiusArc.startPoint = QPointF(0.0, 0.0);
        zeroRadiusArc.midPoint = QPointF(0.0, 0.0);
        zeroRadiusArc.endPoint = QPointF(0.0, 0.0);
        zeroRadiusArc.strokeWidth = 0.1;
        zeroRadiusArcSymbol.arcs.append(zeroRadiusArc);
        // zeroRadiusArcExporter：负责生成测试输出的导出器实例。
        ExporterAltiumSymbol zeroRadiusArcExporter;
        // zeroRadiusArcPath：测试使用的文件系统路径。
        const QString zeroRadiusArcPath = QDir(tempDir.path()).filePath(QStringLiteral("zero-radius-arc.SchLib"));
        QVERIFY(zeroRadiusArcExporter.exportSymbol(zeroRadiusArcSymbol, zeroRadiusArcPath));
        QVERIFY(zeroRadiusArcExporter.diagnostics().contains(
            QStringLiteral("符号 ZERO_RADIUS_ARC 圆弧图元 0 半径量化后无效，已跳过")));

        // invalidPathSymbol：测试使用的文件系统路径。
        IR::SymbolComponentIR invalidPathSymbol;
        invalidPathSymbol.name = QStringLiteral("INVALID_PATH_SEGMENT");
        // invalidPath：测试使用的文件系统路径。
        IR::SymbolPathIR invalidPath;
        // invalidEllipseArc：测试使用的元器件标识或名称。
        IR::SymbolPathSegmentIR invalidEllipseArc;
        invalidEllipseArc.type = IR::SymbolPathSegmentIR::Type::EllipticalArc;
        invalidEllipseArc.arcCenter = QPointF(0.0, 0.0);
        invalidEllipseArc.radiusX = 0.0;
        invalidEllipseArc.radiusY = 1.0;
        invalidEllipseArc.arcStartAngle = 0.0;
        invalidEllipseArc.arcEndAngle = 90.0;
        invalidPath.segments.append(invalidEllipseArc);
        invalidPathSymbol.paths.append(invalidPath);
        // invalidPathExporter：测试使用的文件系统路径。
        ExporterAltiumSymbol invalidPathExporter;
        // invalidPathFile：测试使用的文件系统路径。
        const QString invalidPathFile = QDir(tempDir.path()).filePath(QStringLiteral("invalid-path-segment.SchLib"));
        QVERIFY(invalidPathExporter.exportSymbol(invalidPathSymbol, invalidPathFile));
        QVERIFY(invalidPathExporter.diagnostics().contains(
            QStringLiteral("符号 INVALID_PATH_SEGMENT 路径图元 0 的段 0 参数无效，已跳过")));

        // invalidPathPointsSymbol：测试使用的文件系统路径。
        IR::SymbolComponentIR invalidPathPointsSymbol;
        invalidPathPointsSymbol.name = QStringLiteral("INVALID_PATH_POINTS");
        // invalidFilledPath：测试使用的文件系统路径。
        IR::SymbolPathIR invalidFilledPath;
        invalidFilledPath.isFilled = true;
        invalidFilledPath.points = {QPointF(0.0, 0.0), QPointF(std::numeric_limits<double>::quiet_NaN(), 1.0)};
        invalidPathPointsSymbol.paths.append(invalidFilledPath);
        // invalidPathPointsExporter：测试使用的文件系统路径。
        ExporterAltiumSymbol invalidPathPointsExporter;
        const QString invalidPathPointsFile =
            QDir(tempDir.path()).filePath(QStringLiteral("invalid-path-points.SchLib"));
        QVERIFY(invalidPathPointsExporter.exportSymbol(invalidPathPointsSymbol, invalidPathPointsFile));
        QVERIFY(invalidPathPointsExporter.diagnostics().contains(
            QStringLiteral("符号 INVALID_PATH_POINTS 路径图元 0 的点列无效，已跳过")));

        // nativeArcSymbol：测试用符号数据。
        IR::SymbolComponentIR nativeArcSymbol;
        nativeArcSymbol.name = QStringLiteral("NATIVE_ARC");
        // nativeArcPath：测试使用的文件系统路径。
        IR::SymbolPathIR nativeArcPath;
        nativeArcPath.strokeWidth = 0.1;
        // nativeArcSegment：原生圆弧段。
        IR::SymbolPathSegmentIR nativeArcSegment;
        nativeArcSegment.type = IR::SymbolPathSegmentIR::Type::CircularArc;
        nativeArcSegment.start = QPointF(0.0, 0.0);
        nativeArcSegment.arcMid = QPointF(1.0, 1.0);
        nativeArcSegment.end = QPointF(2.0, 0.0);
        nativeArcPath.segments.append(nativeArcSegment);
        nativeArcSymbol.paths.append(nativeArcPath);
        // nativeArcPathFile：测试使用的文件系统路径。
        const QString nativeArcPathFile = QDir(tempDir.path()).filePath(QStringLiteral("native-arc.SchLib"));
        // nativeArcExporter：负责生成测试输出的导出器实例。
        ExporterAltiumSymbol nativeArcExporter;
        QVERIFY(nativeArcExporter.exportSymbolLibrary(
            {nativeArcSymbol}, QStringLiteral("native-arc"), nativeArcPathFile, false, false));
        // nativeArcData：测试使用的文本或数据内容。
        QByteArray nativeArcData;
        QVERIFY(readCfbStream(nativeArcPathFile, QStringLiteral("NATIVE_ARC/Data"), nativeArcData));
        QVERIFY(nativeArcData.contains("RECORD=12"));

        // zeroRadiusNativeArcSymbol：测试用符号数据。
        IR::SymbolComponentIR zeroRadiusNativeArcSymbol;
        zeroRadiusNativeArcSymbol.name = QStringLiteral("ZERO_RADIUS_NATIVE_ARC");
        // zeroRadiusNativeArcPath：测试使用的文件系统路径。
        IR::SymbolPathIR zeroRadiusNativeArcPath;
        zeroRadiusNativeArcPath.strokeWidth = 0.1;
        // zeroRadiusNativeArcSegment：零半径原生圆弧段。
        IR::SymbolPathSegmentIR zeroRadiusNativeArcSegment;
        zeroRadiusNativeArcSegment.type = IR::SymbolPathSegmentIR::Type::CircularArc;
        zeroRadiusNativeArcSegment.start = QPointF(0.0, 0.0);
        zeroRadiusNativeArcSegment.arcMid = QPointF(0.0, 0.0);
        zeroRadiusNativeArcSegment.end = QPointF(0.0, 0.0);
        zeroRadiusNativeArcPath.segments.append(zeroRadiusNativeArcSegment);
        zeroRadiusNativeArcSymbol.paths.append(zeroRadiusNativeArcPath);
        const QString zeroRadiusNativeArcFile =
            QDir(tempDir.path()).filePath(QStringLiteral("zero-radius-native-arc.SchLib"));
        // zeroRadiusNativeArcExporter：负责生成测试输出的导出器实例。
        ExporterAltiumSymbol zeroRadiusNativeArcExporter;
        QVERIFY(zeroRadiusNativeArcExporter.exportSymbol(zeroRadiusNativeArcSymbol, zeroRadiusNativeArcFile));
        QVERIFY(zeroRadiusNativeArcExporter.diagnostics().contains(
            QStringLiteral("符号 ZERO_RADIUS_NATIVE_ARC 路径图元 0 的段 0 圆弧半径量化后无效，已跳过")));

        // quantizedRadiusSymbol：测试用符号数据。
        IR::SymbolComponentIR quantizedRadiusSymbol;
        quantizedRadiusSymbol.name = QStringLiteral("QUANTIZED_RADIUS_GRAPHICS");
        // tinyCircle：微小圆形。
        IR::SymbolCircleIR tinyCircle;
        tinyCircle.center = QPointF(0.0, 0.0);
        tinyCircle.radius = 0.000001;
        tinyCircle.strokeWidth = 0.1;
        quantizedRadiusSymbol.circles.append(tinyCircle);
        // tinyEllipse：微小椭圆。
        IR::SymbolEllipseIR tinyEllipse;
        tinyEllipse.center = QPointF(1.0, 0.0);
        tinyEllipse.radiusX = 0.000001;
        tinyEllipse.radiusY = 1.0;
        tinyEllipse.strokeWidth = 0.1;
        quantizedRadiusSymbol.ellipses.append(tinyEllipse);
        // tinyPie：微小扇形。
        IR::SymbolPieIR tinyPie;
        tinyPie.center = QPointF(2.0, 0.0);
        tinyPie.radius = 0.000001;
        tinyPie.strokeWidth = 0.1;
        quantizedRadiusSymbol.pies.append(tinyPie);
        // tinyEllipticalArc：微小椭圆弧。
        IR::SymbolEllipticalArcIR tinyEllipticalArc;
        tinyEllipticalArc.center = QPointF(3.0, 0.0);
        tinyEllipticalArc.radiusX = 0.000001;
        tinyEllipticalArc.radiusY = 1.0;
        tinyEllipticalArc.strokeWidth = 0.1;
        quantizedRadiusSymbol.ellipticalArcs.append(tinyEllipticalArc);
        // tinyEllipticalArcPath：测试使用的文件系统路径。
        IR::SymbolPathIR tinyEllipticalArcPath;
        tinyEllipticalArcPath.strokeWidth = 0.1;
        // tinyEllipticalArcSegment：微小椭圆弧段。
        IR::SymbolPathSegmentIR tinyEllipticalArcSegment;
        tinyEllipticalArcSegment.type = IR::SymbolPathSegmentIR::Type::EllipticalArc;
        tinyEllipticalArcSegment.arcCenter = QPointF(4.0, 0.0);
        tinyEllipticalArcSegment.radiusX = 0.000001;
        tinyEllipticalArcSegment.radiusY = 1.0;
        tinyEllipticalArcPath.segments.append(tinyEllipticalArcSegment);
        quantizedRadiusSymbol.paths.append(tinyEllipticalArcPath);
        // quantizedRadiusExporter：负责生成测试输出的导出器实例。
        ExporterAltiumSymbol quantizedRadiusExporter;
        const QString quantizedRadiusFile =
            QDir(tempDir.path()).filePath(QStringLiteral("quantized-radius-graphics.SchLib"));
        QVERIFY(quantizedRadiusExporter.exportSymbol(quantizedRadiusSymbol, quantizedRadiusFile));
        // quantizedRadiusDiagnostics：测试期间收集的错误或诊断信息。
        const QStringList quantizedRadiusDiagnostics = quantizedRadiusExporter.diagnostics();
        QVERIFY(quantizedRadiusDiagnostics.contains(
            QStringLiteral("符号 QUANTIZED_RADIUS_GRAPHICS 圆图元 0 半径量化后无效，已跳过")));
        QVERIFY(quantizedRadiusDiagnostics.contains(
            QStringLiteral("符号 QUANTIZED_RADIUS_GRAPHICS 椭圆图元 0 半径量化后无效，已跳过")));
        QVERIFY(quantizedRadiusDiagnostics.contains(
            QStringLiteral("符号 QUANTIZED_RADIUS_GRAPHICS 扇形图元 0 半径量化后无效，已跳过")));
        QVERIFY(quantizedRadiusDiagnostics.contains(
            QStringLiteral("符号 QUANTIZED_RADIUS_GRAPHICS 椭圆弧图元 0 半径量化后无效，已跳过")));
        QVERIFY(quantizedRadiusDiagnostics.contains(
            QStringLiteral("符号 QUANTIZED_RADIUS_GRAPHICS 路径图元 0 的段 0 椭圆弧半径量化后无效，已跳过")));

        // nativeQuadraticSymbol：测试用符号数据。
        IR::SymbolComponentIR nativeQuadraticSymbol;
        nativeQuadraticSymbol.name = QStringLiteral("NATIVE_QUADRATIC");
        // nativeQuadraticPath：测试使用的文件系统路径。
        IR::SymbolPathIR nativeQuadraticPath;
        nativeQuadraticPath.strokeWidth = 0.1;
        // nativeQuadraticSegment：原生二次曲线段。
        IR::SymbolPathSegmentIR nativeQuadraticSegment;
        nativeQuadraticSegment.type = IR::SymbolPathSegmentIR::Type::QuadraticBezier;
        nativeQuadraticSegment.start = QPointF(0.0, 0.0);
        nativeQuadraticSegment.control1 = QPointF(1.0, 2.0);
        nativeQuadraticSegment.end = QPointF(2.0, 0.0);
        nativeQuadraticPath.segments.append(nativeQuadraticSegment);
        // nativeCubicSegment：原生三次曲线段。
        IR::SymbolPathSegmentIR nativeCubicSegment;
        nativeCubicSegment.type = IR::SymbolPathSegmentIR::Type::CubicBezier;
        nativeCubicSegment.start = QPointF(2.0, 0.0);
        nativeCubicSegment.control1 = QPointF(3.0, 1.0);
        nativeCubicSegment.control2 = QPointF(4.0, 1.0);
        nativeCubicSegment.end = QPointF(5.0, 0.0);
        nativeQuadraticPath.segments.append(nativeCubicSegment);
        // nativeCircularArcSegment：原生圆弧段。
        IR::SymbolPathSegmentIR nativeCircularArcSegment;
        nativeCircularArcSegment.type = IR::SymbolPathSegmentIR::Type::CircularArc;
        nativeCircularArcSegment.start = QPointF(5.0, 0.0);
        nativeCircularArcSegment.arcMid = QPointF(6.0, 1.0);
        nativeCircularArcSegment.end = QPointF(7.0, 0.0);
        nativeQuadraticPath.segments.append(nativeCircularArcSegment);
        nativeQuadraticSymbol.paths.append(nativeQuadraticPath);
        const QString nativeQuadraticPathFile =
            QDir(tempDir.path()).filePath(QStringLiteral("native-quadratic.SchLib"));
        // nativeQuadraticExporter：负责生成测试输出的导出器实例。
        ExporterAltiumSymbol nativeQuadraticExporter;
        QVERIFY(nativeQuadraticExporter.exportSymbolLibrary(
            {nativeQuadraticSymbol}, QStringLiteral("native-quadratic"), nativeQuadraticPathFile, false, false));
        // nativeQuadraticData：测试使用的文本或数据内容。
        QByteArray nativeQuadraticData;
        QVERIFY(readCfbStream(nativeQuadraticPathFile, QStringLiteral("NATIVE_QUADRATIC/Data"), nativeQuadraticData));
        QCOMPARE(nativeQuadraticData.count("RECORD=5"), 2);
        QVERIFY(nativeQuadraticData.contains("RECORD=12"));
        QVERIFY(nativeQuadraticData.contains("LocationCount=4"));

        // nativeEllipticalArcSymbol：测试用符号数据。
        IR::SymbolComponentIR nativeEllipticalArcSymbol;
        nativeEllipticalArcSymbol.name = QStringLiteral("NATIVE_ELLIPTICAL_ARC");
        // nativeEllipticalArcPath：测试使用的文件系统路径。
        IR::SymbolPathIR nativeEllipticalArcPath;
        nativeEllipticalArcPath.strokeWidth = 0.1;
        // nativeEllipticalArcSegment：原生椭圆弧段。
        IR::SymbolPathSegmentIR nativeEllipticalArcSegment;
        nativeEllipticalArcSegment.type = IR::SymbolPathSegmentIR::Type::EllipticalArc;
        nativeEllipticalArcSegment.arcCenter = QPointF(0.0, 0.0);
        nativeEllipticalArcSegment.radiusX = 2.0;
        nativeEllipticalArcSegment.radiusY = 1.0;
        nativeEllipticalArcSegment.arcStartAngle = 0.0;
        nativeEllipticalArcSegment.arcEndAngle = 90.0;
        nativeEllipticalArcPath.segments.append(nativeEllipticalArcSegment);
        nativeEllipticalArcSymbol.paths.append(nativeEllipticalArcPath);
        const QString nativeEllipticalArcFile =
            QDir(tempDir.path()).filePath(QStringLiteral("native-elliptical-arc.SchLib"));
        // nativeEllipticalArcExporter：负责生成测试输出的导出器实例。
        ExporterAltiumSymbol nativeEllipticalArcExporter;
        QVERIFY(nativeEllipticalArcExporter.exportSymbolLibrary({nativeEllipticalArcSymbol},
                                                                QStringLiteral("native-elliptical-arc"),
                                                                nativeEllipticalArcFile,
                                                                false,
                                                                false));
        // nativeEllipticalArcData：测试使用的文本或数据内容。
        QByteArray nativeEllipticalArcData;
        QVERIFY(readCfbStream(
            nativeEllipticalArcFile, QStringLiteral("NATIVE_ELLIPTICAL_ARC/Data"), nativeEllipticalArcData));
        QVERIFY(nativeEllipticalArcData.contains("RECORD=11"));
        QVERIFY(nativeEllipticalArcData.contains("SecondaryRadius="));

        // footprint：测试用封装数据。
        IR::FootprintComponentIR footprint;
        footprint.name = QStringLiteral("SEGMENTS");
        // polyline：测试用折线。
        IR::FootprintTrackIR polyline;
        polyline.points = {QPointF(0, 0), QPointF(1, 0), QPointF(1, 1)};
        polyline.width = 0.2;
        polyline.layer = IR::LayerType::TopSilk;
        footprint.tracks.append(polyline);
        // hole：测试用孔径数据。
        IR::FootprintHoleIR hole;
        hole.center = QPointF(0.5, 0.5);
        hole.radius = 0.25;
        footprint.holes.append(hole);
        // outline：测试用轮廓。
        IR::FootprintOutlineIR outline;
        outline.points = {QPointF(-1, -1), QPointF(2, -1)};
        outline.strokeWidth = 0.1;
        footprint.outlines.append(outline);

        // pcbPath：测试使用的文件系统路径。
        const QString pcbPath = QDir(tempDir.path()).filePath(QStringLiteral("segments.PcbLib"));
        // footprintExporter：负责生成测试输出的导出器实例。
        ExporterAltiumFootprint footprintExporter;
        QVERIFY(footprintExporter.exportFootprintLibrary({footprint}, QStringLiteral("segments"), pcbPath));
        // footprintHeader：测试用封装数据。
        QByteArray footprintHeader;
        QVERIFY(readCfbStream(pcbPath, QStringLiteral("SEGMENTS/Header"), footprintHeader));
        QCOMPARE(readU32(footprintHeader, 0), quint32(4));  // 2 track segments + 1 hole pad + 1 outline segment
    }

    /**
     * @brief 验证 Altium 3D 元件体保留 STEP 原点补偿。
     */
    void altiumFootprintPreservesStepOffsetInComponentBody() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // footprint：测试用封装数据。
        IR::FootprintComponentIR footprint;
        footprint.name = QStringLiteral("STEP_OFFSET");
        // pad：测试用焊盘数据。
        IR::FootprintPadIR pad;
        pad.number = QStringLiteral("1");
        pad.position = QPointF(0.0, 0.0);
        pad.size = QSizeF(1.0, 1.0);
        footprint.pads.append(pad);

        // model：测试使用的配置值或状态。
        IR::Model3DIR model;
        model.setName(QStringLiteral("offset.step"));
        model.setStepData(QByteArrayLiteral("ISO-10303-21;"));
        model.setTranslation(IR::Model3DVec3(1.0, 2.0, 3.0));
        model.setStepOffsetMm(IR::Model3DVec3(4.0, 5.0, 6.0));
        footprint.models3d.append(model);
        // secondModel：测试使用的配置值或状态。
        IR::Model3DIR secondModel;
        secondModel.setName(QStringLiteral("second.step"));
        secondModel.setStepData(QByteArrayLiteral("ISO-10303-21;SECOND"));
        secondModel.setTranslation(IR::Model3DVec3(10.0, 11.0, 12.0));
        secondModel.setStepOffsetMm(IR::Model3DVec3(1.0, 2.0, 3.0));
        footprint.models3d.append(secondModel);

        // pcbPath：测试使用的文件系统路径。
        const QString pcbPath = QDir(tempDir.path()).filePath(QStringLiteral("step-offset.PcbLib"));
        // exporter：负责生成测试输出的导出器实例。
        ExporterAltiumFootprint exporter;
        QVERIFY(exporter.exportFootprintLibrary({footprint}, QStringLiteral("step-offset"), pcbPath));

        // reader：负责回读生成内容的 reader 实例。
        AltiumPcbLibReader reader;
        QVERIFY2(reader.open(pcbPath), qPrintable(reader.errorString()));
        // objects：测试构造的 JSON 数据对象。
        QVector<AltiumPcbLibReader::PrimitiveRecord> objects;
        QVERIFY2(reader.readFootprintObjects(QStringLiteral("STEP_OFFSET"), &objects),
                 qPrintable(reader.errorString()));

        // bodyIds：测试使用的元器件标识或名称。
        QSet<QString> bodyIds;
        for (const auto& object : objects) {
            if (object.objectId != AltiumConstants::PCB_OBJECT_COMPONENT_BODY)
                continue;

            QVERIFY(object.hasComponentBodyFields);
            // params：测试用参数集合。
            const auto& params = object.componentBody.parameters;
            const auto expectedMil = [](double millimeters) {
                return QString::number(AltiumCoord::mmToRaw(millimeters) / 10000.0, 'f', 4) + QStringLiteral("mil");
            };
            // modelName：测试使用的元器件标识或名称。
            const QString modelName = params.value(QStringLiteral("MODEL.NAME"));
            if (modelName == QStringLiteral("offset.step")) {
                QCOMPARE(params.value(QStringLiteral("MODEL.2D.X")), expectedMil(5.0));
                QCOMPARE(params.value(QStringLiteral("MODEL.2D.Y")), expectedMil(7.0));
                QCOMPARE(params.value(QStringLiteral("MODEL.3D.DZ")), expectedMil(9.0));
            } else if (modelName == QStringLiteral("second.step")) {
                QCOMPARE(params.value(QStringLiteral("MODEL.2D.X")), expectedMil(11.0));
                QCOMPARE(params.value(QStringLiteral("MODEL.2D.Y")), expectedMil(13.0));
                QCOMPARE(params.value(QStringLiteral("MODEL.3D.DZ")), expectedMil(15.0));
            } else {
                QFAIL("unexpected 3D model name");
            }
            bodyIds.insert(params.value(QStringLiteral("MODELID")));
        }
        QCOMPARE(bodyIds.size(), 2);
    }

    /**
     * @brief 不完整来源顺序回退到默认写出，避免丢失图元。
     */
    void incompleteGraphicOrderFallsBackWithoutDroppingGraphics() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // symbol：测试用符号数据。
        AltiumSchComponent symbol;
        symbol.name = QStringLiteral("INCOMPLETE_ORDER");
        // first：第一个测试结果。
        AltiumSchRectangle first;
        first.sourceGraphicIndex = 0;
        first.sourcePartIndex = 0;
        first.locationX = 100000;
        first.cornerX = 200000;
        first.cornerY = 100000;
        symbol.rectangles.append(first);
        // second：第二个测试结果。
        AltiumSchRectangle second;
        second.sourceGraphicIndex = 1;
        second.sourcePartIndex = 0;
        second.locationX = 300000;
        second.cornerX = 400000;
        second.cornerY = 100000;
        symbol.rectangles.append(second);
        symbol.graphicOrder = {{QStringLiteral("R"), 0, 0}};

        // writer：负责写入目标格式的 writer 实例。
        AltiumSchLibWriter writer;
        // path：测试使用的文件系统路径。
        const QString path = QDir(tempDir.path()).filePath(QStringLiteral("incomplete-order.SchLib"));
        QVERIFY(writer.write({symbol}, path, QStringLiteral("incomplete-order")));
        QVERIFY(writer.diagnostics().contains(
            QStringLiteral("符号 INCOMPLETE_ORDER 的 graphicOrder 不完整或包含无效引用，已回退到默认图元顺序")));

        // data：测试使用的文本或数据内容。
        QByteArray data;
        QVERIFY(readCfbStream(path, QStringLiteral("INCOMPLETE_ORDER/Data"), data));
        QCOMPARE(data.count(QByteArrayLiteral("RECORD=14")), 2);
        QVERIFY(data.contains("Location.X=1"));
        QVERIFY(data.contains("Location.X=3"));
    }

    /**
     * @brief 验证重复来源路径段索引会回退并保留全部路径记录。
     */
    void duplicatePathSegmentIndexFallsBackWithoutDroppingGraphics() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // symbol：测试用符号数据。
        AltiumSchComponent symbol;
        symbol.name = QStringLiteral("DUPLICATE_PATH_SEGMENT");
        for (int i = 0; i < 2; ++i) {
            // path：测试使用的文件系统路径。
            AltiumSchPath path;
            path.vertices = {QPointF(i * 100000, 0), QPointF(i * 100000 + 50000, 50000)};
            path.sourceGraphicType = QStringLiteral("PT");
            path.sourceGraphicIndex = 0;
            path.sourceSegmentIndex = 0;
            symbol.paths.append(path);
        }
        symbol.graphicOrder = {{QStringLiteral("PT"), 0, 0}};

        // writer：负责写入目标格式的 writer 实例。
        AltiumSchLibWriter writer;
        // path：测试使用的文件系统路径。
        const QString path = QDir(tempDir.path()).filePath(QStringLiteral("duplicate-path-segment.SchLib"));
        QVERIFY(writer.write({symbol}, path, QStringLiteral("duplicate-path-segment")));
        QVERIFY(writer.diagnostics().contains(
            QStringLiteral("符号 DUPLICATE_PATH_SEGMENT 的 graphicOrder 不完整或包含无效引用，已回退到默认图元顺序")));

        // data：测试使用的文本或数据内容。
        QByteArray data;
        QVERIFY(readCfbStream(path, QStringLiteral("DUPLICATE_PATH_SEGMENT/Data"), data));
        QCOMPARE(data.count(QByteArrayLiteral("RECORD=6")), 2);
    }

    /**
     * @brief 验证有序图元模式下未参与来源顺序的图片只写出一次。
     */
    void orderedGraphicsDoNotDuplicateUnindexedImages() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // symbol：测试用符号数据。
        AltiumSchComponent symbol;
        symbol.name = QStringLiteral("ORDERED_IMAGE");
        // rectangle：测试用矩形图元。
        AltiumSchRectangle rectangle;
        rectangle.sourceGraphicIndex = 0;
        rectangle.sourcePartIndex = 0;
        rectangle.cornerX = 100000;
        rectangle.cornerY = 100000;
        symbol.rectangles.append(rectangle);
        // image：测试用图像数据。
        AltiumSchImage image;
        image.fileName = QStringLiteral("unindexed.png");
        image.cornerX = 100000;
        image.cornerY = 100000;
        image.sourceGraphicIndex = -1;
        image.sourcePartIndex = 0;
        symbol.images.append(image);
        symbol.graphicOrder = {{QStringLiteral("R"), 0, 0}};

        // writer：负责写入目标格式的 writer 实例。
        AltiumSchLibWriter writer;
        // path：测试使用的文件系统路径。
        const QString path = QDir(tempDir.path()).filePath(QStringLiteral("ordered-image.SchLib"));
        QVERIFY(writer.write({symbol}, path, QStringLiteral("ordered-image")));

        // data：测试使用的文本或数据内容。
        QByteArray data;
        QVERIFY(readCfbStream(path, QStringLiteral("ORDERED_IMAGE/Data"), data));
        QCOMPARE(data.count(QByteArrayLiteral("RECORD=14")), 1);
        QCOMPARE(data.count(QByteArrayLiteral("RECORD=30")), 1);
    }

    /**
     * @brief 验证普通矩形与圆角矩形索引冲突时回退并保留全部图元。
     */
    void conflictingRectangleOrderFallsBackWithoutDroppingGraphics() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // symbol：测试用符号数据。
        AltiumSchComponent symbol;
        symbol.name = QStringLiteral("CONFLICTING_RECTANGLE_ORDER");
        // rectangle：测试用矩形图元。
        AltiumSchRectangle rectangle;
        rectangle.sourceGraphicIndex = 0;
        rectangle.sourcePartIndex = 0;
        rectangle.cornerX = 100000;
        rectangle.cornerY = 100000;
        symbol.rectangles.append(rectangle);
        // roundRectangle：测试用圆角矩形。
        AltiumSchRoundRectangle roundRectangle;
        roundRectangle.sourceGraphicIndex = 0;
        roundRectangle.sourcePartIndex = 0;
        roundRectangle.cornerX = 200000;
        roundRectangle.cornerY = 200000;
        symbol.roundRectangles.append(roundRectangle);
        symbol.graphicOrder = {{QStringLiteral("R"), 0, 0}};

        // writer：负责写入目标格式的 writer 实例。
        AltiumSchLibWriter writer;
        // path：测试使用的文件系统路径。
        const QString path = QDir(tempDir.path()).filePath(QStringLiteral("conflicting-rectangle-order.SchLib"));
        QVERIFY(writer.write({symbol}, path, QStringLiteral("conflicting-rectangle-order")));
        QVERIFY(writer.diagnostics().contains(QStringLiteral(
            "符号 CONFLICTING_RECTANGLE_ORDER 的 graphicOrder 不完整或包含无效引用，已回退到默认图元顺序")));

        // data：测试使用的文本或数据内容。
        QByteArray data;
        QVERIFY(readCfbStream(path, QStringLiteral("CONFLICTING_RECTANGLE_ORDER/Data"), data));
        QCOMPARE(data.count(QByteArrayLiteral("RECORD=14")), 1);
        QCOMPARE(data.count(QByteArrayLiteral("RECORD=10")), 1);
    }

    /**
     * @brief 验证同类图元来源索引冲突时不会静默丢失记录。
     */
    void conflictingPolygonOrderFallsBackWithoutDroppingGraphics() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // symbol：测试用符号数据。
        AltiumSchComponent symbol;
        symbol.name = QStringLiteral("CONFLICTING_POLYGON_ORDER");
        for (const int offset : {0, 100000}) {
            // polygon：测试用多边形图元。
            AltiumSchPolygon polygon;
            polygon.sourceGraphicIndex = 0;
            polygon.sourcePartIndex = 0;
            polygon.vertices = {QPointF(offset, 0), QPointF(offset + 10000, 0), QPointF(offset, 10000)};
            symbol.polygons.append(polygon);
        }
        symbol.graphicOrder = {{QStringLiteral("PG"), 0, 0}};

        // writer：负责写入目标格式的 writer 实例。
        AltiumSchLibWriter writer;
        // path：测试使用的文件系统路径。
        const QString path = QDir(tempDir.path()).filePath(QStringLiteral("conflicting-polygon-order.SchLib"));
        QVERIFY(writer.write({symbol}, path, QStringLiteral("conflicting-polygon-order")));
        QVERIFY(writer.diagnostics().contains(QStringLiteral(
            "符号 CONFLICTING_POLYGON_ORDER 的 graphicOrder 不完整或包含无效引用，已回退到默认图元顺序")));

        // data：测试使用的文本或数据内容。
        QByteArray data;
        QVERIFY(readCfbStream(path, QStringLiteral("CONFLICTING_POLYGON_ORDER/Data"), data));
        QCOMPARE(data.count(QByteArrayLiteral("RECORD=7")), 2);
    }

    /**
     * @brief 路径分段编号出现缺口时仍按实际索引写出全部图元。
     */
    void gappedPathSegmentsPreserveGraphics() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // symbol：测试用符号数据。
        AltiumSchComponent symbol;
        symbol.name = QStringLiteral("GAPPED_PATH");
        for (const int segmentIndex : {0, 2}) {
            // path：测试使用的文件系统路径。
            AltiumSchPath path;
            path.sourceGraphicType = QStringLiteral("PT");
            path.sourceGraphicIndex = 0;
            path.sourceSegmentIndex = segmentIndex;
            path.sourcePartIndex = 0;
            path.vertices = {QPointF(segmentIndex * 1000, 0), QPointF(segmentIndex * 1000 + 1000, 0)};
            symbol.paths.append(path);
        }
        symbol.graphicOrder = {{QStringLiteral("PT"), 0, 0}};

        // writer：负责写入目标格式的 writer 实例。
        AltiumSchLibWriter writer;
        // path：测试使用的文件系统路径。
        const QString path = QDir(tempDir.path()).filePath(QStringLiteral("gapped-path.SchLib"));
        QVERIFY(writer.write({symbol}, path, QStringLiteral("gapped-path")));
        QVERIFY(!writer.diagnostics().contains(
            QStringLiteral("符号 GAPPED_PATH 的 graphicOrder 不完整或包含无效引用，已回退到默认图元顺序")));

        // data：测试使用的文本或数据内容。
        QByteArray data;
        QVERIFY(readCfbStream(path, QStringLiteral("GAPPED_PATH/Data"), data));
        QCOMPARE(data.count(QByteArrayLiteral("RECORD=6")), 2);
    }

    /**
     * @brief 来源图元索引缺失时回退，避免有序写出丢失弧线。
     */
    void unresolvedOrderedGraphicFallsBackWithoutDroppingGraphics() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // symbol：测试用符号数据。
        AltiumSchComponent symbol;
        symbol.name = QStringLiteral("UNRESOLVED_ORDER");

        // rectangle：测试用矩形图元。
        AltiumSchRectangle rectangle;
        rectangle.sourceGraphicIndex = 0;
        rectangle.sourcePartIndex = 0;
        rectangle.cornerX = 100000;
        rectangle.cornerY = 100000;
        symbol.rectangles.append(rectangle);

        // arc：测试用圆弧图元。
        AltiumSchArc arc;
        arc.radius = 50000;
        arc.sourceGraphicType = QStringLiteral("A");
        arc.sourceGraphicIndex = -1;
        arc.sourcePartIndex = 0;
        symbol.arcs.append(arc);
        symbol.graphicOrder = {{QStringLiteral("R"), 0, 0}};

        // writer：负责写入目标格式的 writer 实例。
        AltiumSchLibWriter writer;
        // path：测试使用的文件系统路径。
        const QString path = QDir(tempDir.path()).filePath(QStringLiteral("unresolved-order.SchLib"));
        QVERIFY(writer.write({symbol}, path, QStringLiteral("unresolved-order")));
        QVERIFY(writer.diagnostics().contains(
            QStringLiteral("符号 UNRESOLVED_ORDER 的 graphicOrder 不完整或包含无效引用，已回退到默认图元顺序")));

        // data：测试使用的文本或数据内容。
        QByteArray data;
        QVERIFY(readCfbStream(path, QStringLiteral("UNRESOLVED_ORDER/Data"), data));
        QCOMPARE(data.count(QByteArrayLiteral("RECORD=14")), 1);
        QCOMPARE(data.count(QByteArrayLiteral("RECORD=12")), 1);
    }

    /**
     * @brief 验证 Pie 和 IEEE 图元遵循无 UniqueID 的兼容记录格式。
     */
    void pieAndIeeeRecordsOmitUniqueIds() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // symbol：测试用符号数据。
        AltiumSchComponent symbol;
        symbol.name = QStringLiteral("UNIQUE_ID_GRAPHICS");
        // pie：测试用扇形数据。
        AltiumSchPie pie;
        pie.centerX = 100000;
        pie.radius = 200000;
        symbol.pies.append(pie);
        // ieee：测试用 IEEE 浮点值。
        AltiumSchIeee ieee;
        ieee.locationX = 300000;
        ieee.locationY = 400000;
        symbol.ieeeSymbols.append(ieee);

        // writer：负责写入目标格式的 writer 实例。
        AltiumSchLibWriter writer;
        // path：测试使用的文件系统路径。
        const QString path = QDir(tempDir.path()).filePath(QStringLiteral("unique-id-graphics.SchLib"));
        QVERIFY(writer.write({symbol}, path, QStringLiteral("unique-id-graphics")));

        // data：测试使用的文本或数据内容。
        QByteArray data;
        QVERIFY(readCfbStream(path, QStringLiteral("UNIQUE_ID_GRAPHICS/Data"), data));
        // pieOffset：扇形记录偏移量。
        const int pieOffset = data.indexOf("RECORD=9");
        // ieeeOffset：IEEE 字段偏移量。
        const int ieeeOffset = data.indexOf("RECORD=3");
        QVERIFY(pieOffset >= 0);
        QVERIFY(ieeeOffset >= 0);
        QVERIFY(!data.mid(pieOffset, ieeeOffset - pieOffset).contains("UniqueID="));
        // nextRecordOffset：下一个记录偏移量。
        const int nextRecordOffset = data.indexOf("|RECORD=", ieeeOffset + 1);
        QVERIFY(nextRecordOffset > ieeeOffset);
        QVERIFY(!data.mid(ieeeOffset, nextRecordOffset - ieeeOffset).contains("UniqueID="));
    }

    /**
     * @brief 验证二进制引脚会占用共享内容索引，且首条用户参数不会写出索引 0。
     */
    void contentIndexIsSharedAcrossPinsGraphicsAndParameters() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // symbol：测试用符号数据。
        AltiumSchComponent symbol;
        symbol.name = QStringLiteral("CONTENT_INDEX");
        symbol.designatorPrefix = QStringLiteral("U");

        // pin：测试用引脚数据。
        AltiumSchPin pin;
        pin.name = QStringLiteral("IN");
        pin.designator = QStringLiteral("1");
        symbol.pins.append(pin);

        // rectangle：测试用矩形图元。
        AltiumSchRectangle rectangle;
        rectangle.locationX = -100000;
        rectangle.locationY = -100000;
        rectangle.cornerX = 100000;
        rectangle.cornerY = 100000;
        symbol.rectangles.append(rectangle);

        // parameter：测试用参数。
        AltiumSchParameter parameter;
        parameter.name = QStringLiteral("Custom");
        parameter.value = QStringLiteral("value");
        parameter.ownerPartId = 1;
        symbol.parameters.append(parameter);
        // commonParameter：测试用公共参数。
        AltiumSchParameter commonParameter;
        commonParameter.name = QStringLiteral("Common");
        commonParameter.value = QStringLiteral("shared");
        commonParameter.ownerPartId = -1;
        symbol.parameters.append(commonParameter);
        // secondPartParameter：测试使用的元器件标识或名称。
        AltiumSchParameter secondPartParameter;
        secondPartParameter.name = QStringLiteral("PartSpecific");
        secondPartParameter.value = QStringLiteral("part");
        secondPartParameter.ownerPartId = 1;
        symbol.parameters.append(secondPartParameter);

        // writer：负责写入目标格式的 writer 实例。
        AltiumSchLibWriter writer;
        // path：测试使用的文件系统路径。
        const QString path = QDir(tempDir.path()).filePath(QStringLiteral("content-index.SchLib"));
        QVERIFY(writer.write({symbol}, path, QStringLiteral("content-index")));

        // data：测试使用的文本或数据内容。
        QByteArray data;
        QVERIFY(readCfbStream(path, QStringLiteral("CONTENT_INDEX/Data"), data));
        QVERIFY(!data.contains("IndexInSheet=0"));
        QVERIFY(data.contains("RECORD=14"));
        QVERIFY(data.contains("IndexInSheet=1"));
        QVERIFY(!data.contains("OWNERPARTDISPLAYMODE="));
        QVERIFY(data.contains("OWNERPARTID=1"));
        QVERIFY(data.contains("RECORD=41"));
        QVERIFY(data.contains("IndexInSheet=2"));
        // customParameterOffset：自定义参数偏移量。
        const int customParameterOffset = data.indexOf("NAME=Custom");
        // parameterOffset：参数偏移量。
        const int parameterOffset = data.lastIndexOf("|RECORD=41|", customParameterOffset);
        // nextRecordOffset：下一个记录偏移量。
        const int nextRecordOffset = data.indexOf("|RECORD=", parameterOffset + 1);
        QVERIFY(parameterOffset >= 0);
        QVERIFY(customParameterOffset > parameterOffset);
        QVERIFY(nextRecordOffset > parameterOffset);
        // parameterRecord：参数记录。
        const QByteArray parameterRecord = data.mid(parameterOffset, nextRecordOffset - parameterOffset);
        QVERIFY(parameterRecord.contains("OWNERPARTID=1"));
        QVERIFY(!parameterRecord.contains("ISNOTACCESIBLE"));

        // commonParameterOffset：公共参数偏移量。
        const int commonParameterOffset = data.indexOf("NAME=Common");
        // commonRecordOffset：公共记录偏移量。
        const int commonRecordOffset = data.lastIndexOf("|RECORD=41|", commonParameterOffset);
        // nextCommonRecordOffset：下一条公共记录偏移量。
        const int nextCommonRecordOffset = data.indexOf("|RECORD=", commonParameterOffset + 1);
        QVERIFY(commonRecordOffset >= 0);
        QVERIFY(commonParameterOffset > commonRecordOffset);
        QVERIFY(nextCommonRecordOffset > commonRecordOffset);
        // commonRecord：公共记录。
        const QByteArray commonRecord = data.mid(commonRecordOffset, nextCommonRecordOffset - commonRecordOffset);
        QVERIFY(commonRecord.contains("OWNERPARTID=-1"));
        QVERIFY(!commonRecord.contains("IndexInSheet="));

        // partSpecificOffset：测试使用的元器件标识或名称。
        const int partSpecificOffset = data.indexOf("NAME=PartSpecific");
        // partSpecificRecordOffset：测试使用的元器件标识或名称。
        const int partSpecificRecordOffset = data.lastIndexOf("|RECORD=41|", partSpecificOffset);
        // nextPartSpecificRecordOffset：测试使用的元器件标识或名称。
        const int nextPartSpecificRecordOffset = data.indexOf("|RECORD=", partSpecificOffset + 1);
        QVERIFY(partSpecificRecordOffset >= 0);
        QVERIFY(partSpecificOffset > partSpecificRecordOffset);
        QVERIFY(nextPartSpecificRecordOffset > partSpecificRecordOffset);
        const QByteArray partSpecificRecord =
            data.mid(partSpecificRecordOffset, nextPartSpecificRecordOffset - partSpecificRecordOffset);
        QVERIFY(partSpecificRecord.contains("OWNERPARTID=1"));
        QVERIFY(partSpecificRecord.contains("IndexInSheet=3"));

        // reader：负责回读生成内容的 reader 实例。
        AltiumSchLibReader reader;
        QVERIFY2(reader.open(path), qPrintable(reader.errorString()));
        // records：回读得到的记录集合。
        QVector<AltiumSchLibReader::Record> records;
        QVERIFY2(reader.readComponentRecords(QStringLiteral("CONTENT_INDEX"), &records),
                 qPrintable(reader.errorString()));
        // foundGraphic：检测到的图形记录。
        bool foundGraphic = false;
        // foundParameter：检测到的参数记录。
        bool foundParameter = false;
        // foundCommonParameter：检测到的公共参数记录。
        bool foundCommonParameter = false;
        // foundPartSpecificParameter：测试使用的元器件标识或名称。
        bool foundPartSpecificParameter = false;
        for (const AltiumSchLibReader::Record& record : records) {
            if (!record.hasParameters)
                continue;
            if (record.recordType == 14) {
                QCOMPARE(record.ownerPartId, 1);
                QCOMPARE(record.ownerPartDisplayMode, -1);
                foundGraphic = true;
            } else if (record.recordType == 41 &&
                       record.parameters.value(QStringLiteral("NAME")) == QStringLiteral("Custom")) {
                QCOMPARE(record.ownerPartId, 1);
                QCOMPARE(record.ownerPartDisplayMode, -1);
                foundParameter = true;
            } else if (record.recordType == 41 &&
                       record.parameters.value(QStringLiteral("NAME")) == QStringLiteral("Common")) {
                QCOMPARE(record.ownerPartId, -1);
                QCOMPARE(record.indexInSheet, -1);
                foundCommonParameter = true;
            } else if (record.recordType == 41 &&
                       record.parameters.value(QStringLiteral("NAME")) == QStringLiteral("PartSpecific")) {
                QCOMPARE(record.ownerPartId, 1);
                QCOMPARE(record.indexInSheet, 3);
                foundPartSpecificParameter = true;
            }
        }
        QVERIFY(foundGraphic);
        QVERIFY(foundParameter);
        QVERIFY(foundCommonParameter);
        QVERIFY(foundPartSpecificParameter);
    }

    /**
     * @brief 验证 C477928 类单显示模式符号使用 Altium 默认显示模式。
     * @details 使用从 C477928 导出结果提取的九引脚和矩形轮廓结构，避免测试访问网络。
     */
    void issue227SingleDisplayModeUsesDefault() {
        // tempDir：测试输出所在的临时目录。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // component：C477928 类单部件符号模型。
        AltiumSchComponent component;
        component.name = QStringLiteral("LM5164DDAR");
        component.designatorPrefix = QStringLiteral("U");
        // pinNames：用于覆盖 C477928 结构的九个引脚名称。
        const QStringList pinNames = {QStringLiteral("GND"),
                                      QStringLiteral("VIN"),
                                      QStringLiteral("EN/UVLO"),
                                      QStringLiteral("RON"),
                                      QStringLiteral("FB"),
                                      QStringLiteral("PGOOD"),
                                      QStringLiteral("BST"),
                                      QStringLiteral("SW"),
                                      QStringLiteral("EP")};
        for (int index = 0; index < pinNames.size(); ++index) {  // index：当前引脚在结构样本中的序号。
            // pin：当前符号引脚模型。
            AltiumSchPin pin;
            pin.name = pinNames.at(index);
            pin.designator = QString::number(index + 1);
            pin.ownerPartId = 1;
            pin.locationX = -450000 + index * 100000;
            pin.locationY = 290000;
            pin.length = 100000;
            component.pins.append(pin);
        }
        // outline：表示器件主体的矩形轮廓。
        AltiumSchRectangle outline;
        outline.locationX = -450000;
        outline.locationY = 400000;
        outline.cornerX = 450000;
        outline.cornerY = -200000;
        outline.ownerPartId = 1;
        component.rectangles.append(outline);

        // outputPath：测试生成的 SchLib 文件路径。
        const QString outputPath = QDir(tempDir.path()).filePath(QStringLiteral("C477928.SchLib"));
        // writer：负责生成 SchLib 的写入器。
        AltiumSchLibWriter writer;
        QVERIFY(writer.write({component}, outputPath, QStringLiteral("C477928")));

        // data：生成的符号数据流。
        QByteArray data;
        QVERIFY(readCfbStream(outputPath, QStringLiteral("LM5164DDAR/Data"), data));
        QVERIFY(data.contains("DisplayModeCount=1"));
        QVERIFY(!data.contains("OWNERPARTDISPLAYMODE="));

        // reader：负责验证生成文件的记录级结构。
        AltiumSchLibReader reader;
        QVERIFY2(reader.open(outputPath), qPrintable(reader.errorString()));
        // records：读取到的 SchLib 记录集合。
        QVector<AltiumSchLibReader::Record> records;
        QVERIFY2(reader.readComponentRecords(QStringLiteral("LM5164DDAR"), &records), qPrintable(reader.errorString()));
        // pinCount：读取到的引脚记录数量。
        int pinCount = 0;
        // foundOutline：是否读取到主体矩形记录。
        bool foundOutline = false;
        for (const AltiumSchLibReader::Record& record : records) {  // record：当前读取到的 SchLib 记录。
            if (record.recordType == 2) {
                QCOMPARE(record.ownerPartId, 1);
                QCOMPARE(record.ownerPartDisplayMode, 0);
                ++pinCount;
            } else if (record.recordType == 14) {
                QCOMPARE(record.ownerPartId, 1);
                QCOMPARE(record.ownerPartDisplayMode, -1);
                foundOutline = true;
            }
        }
        QCOMPARE(pinCount, pinNames.size());
        QVERIFY(foundOutline);
    }

    /**
     * @brief 验证生产 OLE 读取器能够读取目录、迷你流和普通流。
     */
    void compoundReaderReadsMiniAndRegularStreams() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // writer：负责写入目标格式的 writer 实例。
        OLECompoundWriter writer;
        QVERIFY(writer.create());
        QVERIFY(writer.addStorage(QStringLiteral("Nested")));
        QVERIFY(writer.addStorage(QStringLiteral("Nested"), QStringLiteral("Deep")));
        const QByteArray miniData("mini-stream-data");
        const QByteArray deepData("nested-stream-data");
        const QByteArray regularData(5000, '\x5A');
        QVERIFY(writer.writeStream(QStringLiteral("Tiny"), miniData));
        QVERIFY(writer.writeStream(QStringLiteral("Nested/Deep"), QStringLiteral("Data"), deepData));
        QVERIFY(writer.writeStream(QStringLiteral("Regular"), regularData));

        // path：测试使用的文件系统路径。
        const QString path = QDir(tempDir.path()).filePath(QStringLiteral("reader-roundtrip.cfb"));
        QVERIFY(writer.saveToFile(path));

        // reader：负责回读生成内容的 reader 实例。
        OLECompoundReader reader;
        QVERIFY(reader.open(path));
        QCOMPARE(reader.streamPaths(),
                 QStringList({QStringLiteral("Nested/Deep/Data"), QStringLiteral("Regular"), QStringLiteral("Tiny")}));
        QVERIFY(reader.containsStream(QStringLiteral("Nested/Deep/Data")));

        // data：测试使用的文本或数据内容。
        QByteArray data;
        QVERIFY(reader.readStream(QStringLiteral("Tiny"), &data));
        QCOMPARE(data, miniData);
        QVERIFY(reader.readStream(QStringLiteral("Nested/Deep/Data"), &data));
        QCOMPARE(data, deepData);
        QVERIFY(reader.readStream(QStringLiteral("Regular"), &data));
        QCOMPARE(data, regularData);
        QVERIFY(!reader.readStream(QStringLiteral("Missing"), &data));
    }

    /**
     * @brief 无效输入必须被读取器拒绝并返回诊断信息。
     */
    void compoundReaderRejectsInvalidDocument() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        // path：测试使用的文件系统路径。
        const QString path = QDir(tempDir.path()).filePath(QStringLiteral("invalid.cfb"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QVERIFY(file.write("not-an-ole-document") > 0);
        file.close();

        // reader：负责回读生成内容的 reader 实例。
        OLECompoundReader reader;
        QVERIFY(!reader.open(path));
        QVERIFY(reader.hasError());
        QVERIFY(!reader.errorString().isEmpty());
        QVERIFY(reader.streamPaths().isEmpty());
    }

    /**
     * @brief 验证 Altium 二进制读写器的基础块和 Unicode 参数往返。
     */
    void binaryReaderRoundTripsWriterFormats() {
        // buffer：测试用缓冲区。
        QByteArray buffer;
        AltiumBinaryWriter writer(buffer);
        writer.writeInt16(-1234);
        writer.writeUInt32(0xA1B2C3D4U);
        writer.writeFloat(1.25F);
        writer.writeDouble(-3.5);
        writer.beginBlock(7);
        writer.writeBytes(QByteArrayLiteral("payload"));
        writer.endBlock();
        writer.writePascalShortString(QStringLiteral("短文本"));
        writer.writeStringBlock(QStringLiteral("string-block"));
        writer.writePascalString(QStringLiteral("pascal-block"));
        writer.writeCStringParameterBlockUtf8({{QStringLiteral("ASCII"), QStringLiteral("value")},
                                               {QStringLiteral("DESCRIPTION"), QStringLiteral("参数值")}});

        AltiumBinaryReader reader(buffer);
        // signedValue：测试使用的配置值或状态。
        int16_t signedValue = 0;
        // unsignedValue：测试使用的配置值或状态。
        uint32_t unsignedValue = 0;
        // floatValue：测试使用的配置值或状态。
        float floatValue = 0.0F;
        // doubleValue：测试使用的配置值或状态。
        double doubleValue = 0.0;
        QVERIFY(reader.readInt16(&signedValue));
        QVERIFY(reader.readUInt32(&unsignedValue));
        QVERIFY(reader.readFloat(&floatValue));
        QVERIFY(reader.readDouble(&doubleValue));
        QCOMPARE(signedValue, int16_t(-1234));
        QCOMPARE(unsignedValue, uint32_t(0xA1B2C3D4U));
        QCOMPARE(floatValue, 1.25F);
        QCOMPARE(doubleValue, -3.5);

        // payload：测试用载荷数据。
        QByteArray payload;
        // flags：测试用标志位集合。
        uint8_t flags = 0;
        QVERIFY(reader.readBlock(&payload, &flags));
        QCOMPARE(flags, uint8_t(7));
        QCOMPARE(payload, QByteArrayLiteral("payload"));

        // value：测试使用的配置值或状态。
        QString value;
        QVERIFY(reader.readPascalShortString(&value));
        QCOMPARE(value, QStringLiteral("???"));  // Writer 的 Pascal 短字符串使用 Latin-1。
        QVERIFY(reader.readStringBlock(&value));
        QCOMPARE(value, QStringLiteral("string-block"));
        QVERIFY(reader.readPascalString(&value));
        QCOMPARE(value, QStringLiteral("pascal-block"));

        // params：测试用参数集合。
        QMap<QString, QString> params;
        QVERIFY(reader.readCStringParameterBlock(&params));
        QCOMPARE(params.value(QStringLiteral("ASCII")), QStringLiteral("value"));
        QCOMPARE(params.value(QStringLiteral("DESCRIPTION")), QStringLiteral("参数值"));
        QCOMPARE(reader.remaining(), 0);
        QVERIFY(!reader.hasError());

        // duplicateData：测试使用的文本或数据内容。
        QByteArray duplicateData;
        AltiumBinaryWriter duplicateWriter(duplicateData);
        duplicateWriter.writeCStringParameterBlockRaw(QByteArrayLiteral("|DUPLICATE=first|DUPLICATE=second|"));
        AltiumBinaryReader duplicateReader(duplicateData);
        // duplicateParameters：重复参数集合。
        QMap<QString, QString> duplicateParameters;
        QVERIFY(!duplicateReader.readCStringParameterBlock(&duplicateParameters));
        QVERIFY(duplicateReader.errorString().contains(QStringLiteral("重复键")));
    }

    /**
     * @brief 验证 SchLib SectionKeys 能恢复截断和冲突后的存储名称。
     */
    void schLibReaderResolvesSectionKeys() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // first：第一个测试结果。
        AltiumSchComponent first;
        first.name = QStringLiteral("A/B");
        // second：第二个测试结果。
        AltiumSchComponent second;
        second.name = QStringLiteral("A:B");
        // path：测试使用的文件系统路径。
        const QString path = QDir(tempDir.path()).filePath(QStringLiteral("section-keys.SchLib"));
        // writer：负责写入目标格式的 writer 实例。
        AltiumSchLibWriter writer;
        QVERIFY(writer.write({first, second}, path, QStringLiteral("section-keys")));

        // reader：负责回读生成内容的 reader 实例。
        AltiumSchLibReader reader;
        QVERIFY2(reader.open(path), qPrintable(reader.errorString()));
        // components：测试使用的缓存或元器件对象。
        const auto components = reader.components();
        QCOMPARE(components.size(), 2);
        QCOMPARE(components.at(0).name, QStringLiteral("A/B"));
        QCOMPARE(components.at(1).name, QStringLiteral("A:B"));
        QVERIFY(components.at(0).sectionKey != components.at(1).sectionKey);
        // data：测试使用的文本或数据内容。
        QByteArray data;
        QVERIFY(reader.readComponentData(QStringLiteral("A/B"), &data));
        QVERIFY(data.contains("LibReference=A/B"));
        QVERIFY(reader.readComponentData(QStringLiteral("A:B"), &data));
        QVERIFY(data.contains("LibReference=A:B"));

        // totalRecordCount：测试使用的数量或索引值。
        int totalRecordCount = 0;
        for (int componentIndex = 0; componentIndex < components.size(); ++componentIndex) {
            // records：回读得到的记录集合。
            QVector<AltiumSchLibReader::Record> records;
            QVERIFY2(reader.readComponentRecords(componentIndex, &records), qPrintable(reader.errorString()));
            QVERIFY(!records.isEmpty());
            QCOMPARE(records.first().recordType, 1);
            totalRecordCount += records.size();
        }
        // weightOk：测试用权重校验结果。
        bool weightOk = false;
        // declaredWeight：声明的测试权重。
        const int declaredWeight = reader.headerParameters().value(QStringLiteral("WEIGHT")).toInt(&weightOk);
        QVERIFY(weightOk);
        QCOMPARE(declaredWeight, totalRecordCount);
    }

    /**
     * @brief 验证 PcbLib SectionKeys 能恢复冲突后的封装存储名称。
     */
    void pcbLibReaderResolvesSectionKeys() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // first：第一个测试结果。
        AltiumPcbComponent first;
        first.name = QStringLiteral("A/B");
        // second：第二个测试结果。
        AltiumPcbComponent second;
        second.name = QStringLiteral("A:B");
        // path：测试使用的文件系统路径。
        const QString path = QDir(tempDir.path()).filePath(QStringLiteral("section-keys.PcbLib"));
        // writer：负责写入目标格式的 writer 实例。
        AltiumPcbLibWriter writer;
        QVERIFY(writer.write({first, second}, path, QStringLiteral("section-keys")));

        // reader：负责回读生成内容的 reader 实例。
        AltiumPcbLibReader reader;
        QVERIFY2(reader.open(path), qPrintable(reader.errorString()));
        // components：测试使用的缓存或元器件对象。
        const auto components = reader.components();
        QCOMPARE(components.size(), 2);
        QCOMPARE(components.at(0).name, QStringLiteral("A/B"));
        QCOMPARE(components.at(1).name, QStringLiteral("A:B"));
        QVERIFY(components.at(0).sectionKey != components.at(1).sectionKey);
        // data：测试使用的文本或数据内容。
        QByteArray data;
        QVERIFY(reader.readFootprintStream(QStringLiteral("A/B"), QStringLiteral("Data"), &data));
        QVERIFY(data.contains("A/B"));
        QVERIFY(reader.readFootprintStream(QStringLiteral("A:B"), QStringLiteral("Data"), &data));
        QVERIFY(data.contains("A:B"));
    }

    /**
     * @brief 验证 PcbLib 重复封装名称和 SectionKeys 映射会被拒绝
     */
    void rejectsDuplicatePcbLibComponentsAndSectionKeys() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const auto createLibrary = [&](const QString& fileName,
                                       const QStringList& names,
                                       const QStringList& sectionKeys,
                                       const QStringList& sectionKeyRefs) {
            // writer：负责写入目标格式的 writer 实例。
            OLECompoundWriter writer;
            if (!writer.create())
                return false;

            // fileHeader：测试使用的文件系统路径。
            QByteArray fileHeader;
            AltiumBinaryWriter fileHeaderWriter(fileHeader);
            // version：格式版本号。
            const QByteArray version = QByteArrayLiteral("PCB 6.0 Binary Library File");
            fileHeaderWriter.writeInt32(version.size());
            fileHeaderWriter.writePascalShortString(QString::fromLatin1(version));
            if (!writer.writeStream(QStringLiteral("FileHeader"), fileHeader))
                return false;

            // libraryData：测试使用的文本或数据内容。
            QByteArray libraryData;
            AltiumBinaryWriter libraryWriter(libraryData);
            libraryWriter.beginBlock();
            libraryWriter.writeBytes(QByteArrayLiteral("metadata"));
            libraryWriter.endBlock();
            libraryWriter.writeUInt32(static_cast<quint32>(names.size()));
            for (const QString& name : names)
                libraryWriter.writeStringBlock(name);
            if (!writer.addStorage(QStringLiteral("Library")) ||
                !writer.writeStream(QStringLiteral("Library"), QStringLiteral("Data"), libraryData))
                return false;

            if (!sectionKeys.isEmpty()) {
                // sectionKeyData：测试使用的文本或数据内容。
                QByteArray sectionKeyData;
                AltiumBinaryWriter sectionKeyWriter(sectionKeyData);
                sectionKeyWriter.writeUInt32(static_cast<quint32>(sectionKeys.size()));
                for (int i = 0; i < sectionKeys.size(); ++i) {
                    sectionKeyWriter.writePascalString(sectionKeyRefs.isEmpty() ? names.at(i) : sectionKeyRefs.at(i));
                    sectionKeyWriter.writeStringBlock(sectionKeys.at(i));
                }
                if (!writer.writeStream(QStringLiteral("SectionKeys"), sectionKeyData))
                    return false;
            }
            return writer.saveToFile(QDir(tempDir.path()).filePath(fileName));
        };

        QVERIFY(createLibrary(
            QStringLiteral("duplicate-pcb-components.PcbLib"), {QStringLiteral("DUP"), QStringLiteral("dup")}, {}, {}));
        // duplicateComponentReader：测试使用的缓存或元器件对象。
        AltiumPcbLibReader duplicateComponentReader;
        QVERIFY(!duplicateComponentReader.open(
            QDir(tempDir.path()).filePath(QStringLiteral("duplicate-pcb-components.PcbLib"))));
        QVERIFY(duplicateComponentReader.errorString().contains(QStringLiteral("封装名称重复")));

        QVERIFY(createLibrary(QStringLiteral("duplicate-pcb-section-keys.PcbLib"),
                              {QStringLiteral("A"), QStringLiteral("B")},
                              {QStringLiteral("SHARED"), QStringLiteral("shared")},
                              {}));
        // duplicateSectionKeyReader：负责回读生成内容的 reader 实例。
        AltiumPcbLibReader duplicateSectionKeyReader;
        QVERIFY(!duplicateSectionKeyReader.open(
            QDir(tempDir.path()).filePath(QStringLiteral("duplicate-pcb-section-keys.PcbLib"))));
        QVERIFY(duplicateSectionKeyReader.errorString().contains(QStringLiteral("SectionKeys 映射重复")));

        QVERIFY(createLibrary(QStringLiteral("case-mismatched-pcb-section-key.PcbLib"),
                              {QStringLiteral("A"), QStringLiteral("B")},
                              {QStringLiteral("KEY_A"), QStringLiteral("KEY_B")},
                              {QStringLiteral("a"), QStringLiteral("B")}));
        // caseMismatchedReader：负责回读生成内容的 reader 实例。
        AltiumPcbLibReader caseMismatchedReader;
        QVERIFY(!caseMismatchedReader.open(
            QDir(tempDir.path()).filePath(QStringLiteral("case-mismatched-pcb-section-key.PcbLib"))));
        QVERIFY(caseMismatchedReader.errorString().contains(QStringLiteral("SectionKeys 包含未知封装")));
    }
};

QTEST_GUILESS_MAIN(TestAltiumOle)
#include "test_altium_ole.moc"
