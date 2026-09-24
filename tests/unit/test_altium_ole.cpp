/**
 * @file test_altium_ole.cpp
 * @brief Altium OLE 复合文档写入器的单元测试
 * @details 验证 OLECompoundWriter 的 CFB 格式正确性，包括：
 *          - 迷你流和常规流的读写往返
 *          - DIFAT 扩展（大文件 > 5.6MB）
 *          - 无效/重名条目的拒绝
 *          - SchLib/PcbLib 完整文件写入
 *          - 焊盘扩展块、UID 图元信息、元件体
 *          - UTF-8 参数编码
 *          - 导出器的多部件和折线保持
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

/** @brief CFB 扇区大小（字节） */
constexpr qint64 CFB_SECTOR_SIZE = 512;
/** @brief CFB 迷你扇区大小（字节） */
constexpr qint64 CFB_MINI_SECTOR_SIZE = 64;
/** @brief CFB FAT 链终结标记 */
constexpr quint32 CFB_ENDOFCHAIN = 0xFFFFFFFE;
/** @brief CFB 空闲扇区标记 */
constexpr quint32 CFB_FREESECT = 0xFFFFFFFF;
/** @brief CFB FAT 扇区标记 */
constexpr quint32 CFB_FATSECT = 0xFFFFFFFD;
/** @brief CFB DIFAT 扇区标记 */
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
    int count = 0;
    for (int offset = 0; offset + 4 <= data.size();) {
        const int payloadSize = static_cast<int>(readU32(data, offset) & 0x00FFFFFFU);
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
    QString name;
    quint8 type = 0;
    quint8 color = 1;
    quint32 leftChild = CFB_FREESECT;
    quint32 rightChild = CFB_FREESECT;
    quint32 child = CFB_FREESECT;
    quint32 startSector = CFB_ENDOFCHAIN;
    quint64 streamSize = 0;
};

/**
 * @brief 读取 CFB 文件的指定扇区
 * @param fileData 完整文件数据
 * @param sector 扇区编号
 * @return 扇区数据（512 字节）
 */
QByteArray readCfbSector(const QByteArray& fileData, quint32 sector) {
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
    QByteArray result;
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
    QByteArray result;
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
    const QByteArray data = file.readAll();

    if (data.size() < CFB_SECTOR_SIZE || data.left(8) != QByteArray::fromHex("d0cf11e0a1b11ae1")) {
        return false;
    }

    const quint32 numFatSectors = readU32(data, 44);
    const quint32 firstDirSector = readU32(data, 48);
    const quint32 firstMiniFatSector = readU32(data, 60);
    const quint32 numMiniFatSectors = readU32(data, 64);
    const quint32 firstDifatSector = readU32(data, 68);
    const quint32 numDifatSectors = readU32(data, 72);

    QVector<quint32> fatSectorList;
    for (int i = 0; i < 109 && static_cast<int>(fatSectorList.size()) < static_cast<int>(numFatSectors); ++i) {
        const quint32 sector = readU32(data, 76 + i * 4);
        if (sector == CFB_ENDOFCHAIN || sector == CFB_FREESECT) {
            break;
        }
        fatSectorList.append(sector);
    }
    quint32 difatSector = firstDifatSector;
    for (quint32 i = 0; i < numDifatSectors && difatSector != CFB_ENDOFCHAIN; ++i) {
        const QByteArray sectorData = readCfbSector(data, difatSector);
        for (int j = 0; j < 127 && fatSectorList.size() < static_cast<int>(numFatSectors); ++j) {
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

    QVector<quint32> fat;
    for (quint32 fatSector : fatSectorList) {
        const QByteArray sectorData = readCfbSector(data, fatSector);
        for (int i = 0; i < CFB_SECTOR_SIZE / 4; ++i) {
            fat.append(readU32(sectorData, i * 4));
        }
    }

    QVector<CfbDirectoryEntry> entries;
    quint32 dirSector = firstDirSector;
    while (dirSector != CFB_ENDOFCHAIN && dirSector != CFB_FREESECT) {
        const QByteArray sectorData = readCfbSector(data, dirSector);
        for (int i = 0; i < CFB_SECTOR_SIZE / 128; ++i) {
            const qint64 offset = i * 128;
            CfbDirectoryEntry entry;
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

    const QByteArray miniStream = readRegularStream(data, fat, entries.at(0).startSector, entries.at(0).streamSize);
    QVector<quint32> miniFat;
    quint32 miniFatSector = firstMiniFatSector;
    for (quint32 i = 0; i < numMiniFatSectors; ++i) {
        if (miniFatSector == CFB_ENDOFCHAIN || miniFatSector == CFB_FREESECT) {
            return false;
        }
        const QByteArray sectorData = readCfbSector(data, miniFatSector);
        for (int j = 0; j < CFB_SECTOR_SIZE / 4; ++j) {
            miniFat.append(readU32(sectorData, j * 4));
        }
        miniFatSector = fat.value(static_cast<int>(miniFatSector), CFB_FREESECT);
    }
    if (numMiniFatSectors > 0 && miniFatSector != CFB_ENDOFCHAIN) {
        return false;
    }

    const QStringList pathParts = streamPath.split('/');
    int entryIndex = 0;
    for (const QString& part : pathParts) {
        int node = entries.at(entryIndex).child;
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

    const int headerSize = static_cast<int>(readU32(storage, 0) & 0x00FFFFFFU);
    int offset = 4 + headerSize;
    if (offset > storage.size())
        return false;

    while (offset < storage.size()) {
        if (offset + 4 > storage.size())
            return false;
        const quint32 blockHeader = readU32(storage, offset);
        if ((blockHeader >> 24) != 1)
            return false;
        const int entrySize = static_cast<int>(blockHeader & 0x00FFFFFFU);
        const int entryOffset = offset + 4;
        const int entryEnd = entryOffset + entrySize;
        if (entrySize < 6 || entryEnd > storage.size())
            return false;
        if (static_cast<quint8>(storage.at(entryOffset)) != 0xD0)
            return false;

        const int nameLength = static_cast<quint8>(storage.at(entryOffset + 1));
        const int compressedLengthOffset = entryOffset + 2 + nameLength;
        if (compressedLengthOffset + 4 > entryEnd)
            return false;
        const QString name = QString::fromLocal8Bit(storage.constData() + entryOffset + 2, nameLength);
        if (name.isEmpty() || compressedImages.contains(name))
            return false;

        const int compressedLength = static_cast<int>(readU32(storage, compressedLengthOffset));
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
    const int leftHeight = validateRedBlackSubtree(entries, entry.leftChild, lowerBound, &entry.name, visited, valid);
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
 *          以及 ExporterAltiumSymbol 和 ExporterAltiumFootprint 的转换正确性。
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

        IR::SymbolComponentIR exportedSymbol;
        exportedSymbol.name = QStringLiteral("HORIZONTAL_PIN_NUMBERS");
        exportedSymbol.pins = {positionedPin, fallbackPin};
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString libraryPath = tempDir.filePath(QStringLiteral("horizontal-pin-numbers.SchLib"));
        ExporterAltiumSymbol exporter;
        QVERIFY(exporter.exportSymbolLibrary(
            {exportedSymbol}, QStringLiteral("horizontal-pin-numbers"), libraryPath, false, false));
        QByteArray symbolData;
        QVERIFY(readCfbStream(libraryPath, QStringLiteral("HORIZONTAL_PIN_NUMBERS/Data"), symbolData));
        for (const QByteArray& designator : {QByteArrayLiteral("1"), QByteArrayLiteral("2")}) {
            const int textOffset = symbolData.indexOf("Text=" + designator);
            QVERIFY(textOffset > 0);
            const int textRecordOffset = symbolData.lastIndexOf("|RECORD=4|", textOffset);
            QVERIFY(textRecordOffset >= 0);
            const int nextRecordOffset = symbolData.indexOf("|RECORD=", textOffset);
            QVERIFY(nextRecordOffset > textOffset);
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
        AltiumSchComponent component;
        AltiumSchRectangle body;
        body.locationX = -1000000;
        body.locationY = -500000;
        body.cornerX = 1000000;
        body.cornerY = 500000;
        component.rectangles.append(body);

        AltiumSchText text;
        text.locationX = 9000000;
        text.locationY = 1000000;
        text.orientation = 3;
        component.texts.append(text);

        AltiumSchText pinLabel;
        pinLabel.locationX = 500000;
        pinLabel.locationY = 500000;
        pinLabel.isPinLabel = true;
        component.texts.append(pinLabel);

        AltiumSchParameter parameter;
        parameter.locationX = 8000000;
        parameter.locationY = 1000000;
        parameter.isHidden = false;
        parameter.orientation = 2;
        component.parameters.append(parameter);

        AltiumSchSymbolGeometryNormalizer::normalize(component);

        const AltiumSchText& movedText = component.texts.first();
        const AltiumSchText& preservedPinLabel = component.texts.at(1);
        const AltiumSchParameter& movedParameter = component.parameters.first();
        const AltiumSchRectangle& normalizedBody = component.rectangles.first();
        const int graphicCenterX = (normalizedBody.locationX + normalizedBody.cornerX) / 2;
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
     * @brief 验证缺少源编号坐标时编号文本跟随投影后的引脚主体端。
     * @details 覆盖水平和垂直引脚，确保主体边界投影及连接点量化不会造成编号错位。
     */
    void fallbackPinNumbersFollowNormalizedPins() {
        AltiumSchComponent component;
        AltiumSchRectangle body;
        body.locationX = -1000000;
        body.locationY = -1000000;
        body.cornerX = 1000000;
        body.cornerY = 1000000;
        component.rectangles.append(body);

        AltiumSchPin horizontalPin;
        horizontalPin.locationX = 4500000;
        horizontalPin.locationY = 250000;
        horizontalPin.length = 350000;
        horizontalPin.orientation = AltiumModels::PinOrientation::Right;
        component.pins.append(horizontalPin);
        AltiumSchText horizontalNumber;
        horizontalNumber.locationX = 4500000;
        horizontalNumber.locationY = 250000;
        horizontalNumber.isPinLabel = true;
        horizontalNumber.isFallbackPinNumber = true;
        horizontalNumber.sourcePinIndex = 0;
        component.texts.append(horizontalNumber);

        AltiumSchPin verticalPin;
        verticalPin.locationX = -250000;
        verticalPin.locationY = 4500000;
        verticalPin.length = 350000;
        verticalPin.orientation = AltiumModels::PinOrientation::Up;
        component.pins.append(verticalPin);
        AltiumSchText verticalNumber = horizontalNumber;
        verticalNumber.locationX = -250000;
        verticalNumber.locationY = 4500000;
        verticalNumber.sourcePinIndex = 1;
        component.texts.append(verticalNumber);

        AltiumSchSymbolGeometryNormalizer::normalize(component);

        QCOMPARE(component.texts.at(0).locationX, component.pins.at(0).locationX);
        QCOMPARE(component.texts.at(0).locationY, component.pins.at(0).locationY);
        QCOMPARE(component.texts.at(1).locationX, component.pins.at(1).locationX);
        QCOMPARE(component.texts.at(1).locationY, component.pins.at(1).locationY);
    }

    /**
     * @brief 验证包含迷你流和常规流的 CFB 文件可被正确解析
     * @details 写入多种大小的流，验证红黑树平衡性，以及嵌套存储路径的读取
     */
    void writesReadableCfbWithMiniAndRegularStreams() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        OLECompoundWriter writer;
        QVERIFY(writer.create());
        QVERIFY(writer.writeStream("FileHeader", QByteArrayLiteral("header payload")));
        QVERIFY(writer.addStorage("Library"));
        QVERIFY(writer.writeStream("Library", "Header", QByteArrayLiteral("hdr")));
        QVERIFY(writer.addStorage("Component"));

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

        const QString filePath = QDir(tempDir.path()).filePath(QStringLiteral("test.cfb"));
        QVERIFY(writer.saveToFile(filePath));

        const QString gsf = QStandardPaths::findExecutable(QStringLiteral("gsf"));
        if (!gsf.isEmpty()) {
            QProcess validator;
            validator.start(gsf, {QStringLiteral("list"), filePath});
            QVERIFY(validator.waitForFinished(10000));
            QCOMPARE(validator.exitCode(), 0);
            const QByteArray diagnostics = validator.readAllStandardError() + validator.readAllStandardOutput();
            QVERIFY2(!diagnostics.contains("invalid OLE"), diagnostics.constData());
        }

        QByteArray headerPayload;
        QVector<CfbDirectoryEntry> directory;
        QVERIFY(readCfbStream(filePath, QStringLiteral("FileHeader"), headerPayload, &directory));
        QCOMPARE(headerPayload, QByteArrayLiteral("header payload"));

        QVERIFY(directory.at(static_cast<int>(directory.at(0).child)).color == 1);
        QSet<quint32> visited;
        bool validTree = true;
        validateRedBlackSubtree(directory, directory.at(0).child, nullptr, nullptr, visited, validTree);
        QVERIFY(validTree);

        const auto emptyEntry = std::find_if(directory.cbegin(), directory.cend(), [](const CfbDirectoryEntry& entry) {
            return entry.name == QStringLiteral("Empty");
        });
        QVERIFY(emptyEntry != directory.cend());
        QCOMPARE(emptyEntry->streamSize, quint64(0));
        QCOMPARE(emptyEntry->startSector, CFB_ENDOFCHAIN);

        QByteArray libraryHeader;
        QVERIFY(readCfbStream(filePath, QStringLiteral("Library/Header"), libraryHeader));
        QCOMPARE(libraryHeader, QByteArrayLiteral("hdr"));

        QByteArray regularPayload;
        QVERIFY(readCfbStream(filePath, QStringLiteral("Component/Data"), regularPayload));
        QCOMPARE(regularPayload, regularData);

        QByteArray smallPayload;
        QVERIFY(readCfbStream(filePath, QStringLiteral("Small"), smallPayload));
        QCOMPARE(smallPayload, QByteArrayLiteral("small stream"));
    }

    /**
     * @brief 验证大文件（>109 扇区）正确生成 DIFAT 扩展扇区
     * @details 写入 8MB 数据，验证 DIFAT 链和往返一致性
     */
    void writesDifatForLargeCompoundFiles() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        QByteArray payload(8 * 1024 * 1024, 0);
        for (int i = 0; i < payload.size(); ++i) {
            payload[i] = static_cast<char>((i * 17 + 3) & 0xFF);
        }
        OLECompoundWriter writer;
        QVERIFY(writer.create());
        QVERIFY(writer.writeStream(QStringLiteral("LargeModel"), payload));
        const QString filePath = QDir(tempDir.path()).filePath(QStringLiteral("large.cfb"));
        QVERIFY(writer.saveToFile(filePath));

        QFile file(filePath);
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QByteArray header = file.read(512);
        QVERIFY(readU32(header, 44) > 109);
        QVERIFY(readU32(header, 72) > 0);
        QCOMPARE(readU32(readCfbSector(header + file.readAll(), readU32(header, 68)), 127 * 4), CFB_ENDOFCHAIN);

        QByteArray roundTrip;
        QVERIFY(readCfbStream(filePath, QStringLiteral("LargeModel"), roundTrip));
        QCOMPARE(roundTrip, payload);

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
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        AltiumSchLibWriter writer;
        const QString outputPath = QDir(tempDir.path()).filePath(QStringLiteral("invalid-input.SchLib"));
        QVERIFY(!writer.write({}, outputPath));
        QVERIFY(writer.diagnostics().join('\n').contains(QStringLiteral("输入组件为空")));

        AltiumSchComponent unnamed;
        QVERIFY(!writer.write({unnamed}, outputPath));
        QVERIFY(writer.diagnostics().join('\n').contains(QStringLiteral("组件名称为空")));
        QVERIFY(!writer.diagnostics().join('\n').contains(QStringLiteral("输入组件为空")));

        AltiumSchComponent oversizedName;
        oversizedName.name = QString(256, QChar('A'));
        QVERIFY(!writer.write({oversizedName}, outputPath));
        QVERIFY(writer.diagnostics().join('\n').contains(QStringLiteral("名称超过 255 字节")));

        AltiumSchComponent nonLatinName;
        nonLatinName.name = QStringLiteral("中文元件");
        QVERIFY(!writer.write({nonLatinName}, outputPath));
        QVERIFY(writer.diagnostics().join('\n').contains(QStringLiteral("名称包含无法编码的字符")));

        AltiumSchComponent oversizedPinText;
        oversizedPinText.name = QStringLiteral("OVERSIZED_PIN_TEXT");
        AltiumSchPin oversizedPin;
        oversizedPin.name = QString(256, QChar('N'));
        oversizedPinText.pins.append(oversizedPin);
        QVERIFY(!writer.write({oversizedPinText}, outputPath));
        QVERIFY(writer.diagnostics().join('\n').contains(QStringLiteral("引脚 0 名称超过 255 字节")));

        AltiumSchComponent nonLatinPinText;
        nonLatinPinText.name = QStringLiteral("NON_LATIN_PIN");
        AltiumSchPin nonLatinPin;
        nonLatinPin.name = QStringLiteral("中文引脚");
        nonLatinPinText.pins.append(nonLatinPin);
        QVERIFY(!writer.write({nonLatinPinText}, outputPath));
        QVERIFY(writer.diagnostics().join('\n').contains(QStringLiteral("引脚 0 名称包含无法编码的字符")));

        AltiumSchComponent invalidPinType;
        invalidPinType.name = QStringLiteral("INVALID_PIN_TYPE");
        AltiumSchPin invalidPin;
        invalidPin.orientation = static_cast<AltiumModels::PinOrientation>(4);
        invalidPinType.pins.append(invalidPin);
        QVERIFY(!writer.write({invalidPinType}, outputPath));
        QVERIFY(writer.diagnostics().join('\n').contains(QStringLiteral("引脚 0 方向无效")));

        AltiumSchComponent invalidImageBounds;
        invalidImageBounds.name = QStringLiteral("INVALID_IMAGE_BOUNDS");
        AltiumSchImage invalidImage;
        invalidImage.fileName = QStringLiteral("image.png");
        invalidImage.locationX = 100;
        invalidImage.cornerX = 100;
        invalidImage.cornerY = 100;
        invalidImageBounds.images.append(invalidImage);
        QVERIFY(!writer.write({invalidImageBounds}, outputPath));
        QVERIFY(writer.diagnostics().join('\n').contains(QStringLiteral("图片边界尺寸无效")));

        AltiumSchComponent named;
        named.name = QStringLiteral("VALID");
        QVERIFY(!writer.write({named}, QString()));
        QVERIFY(writer.diagnostics().join('\n').contains(QStringLiteral("输出路径为空")));
        QVERIFY(!writer.diagnostics().join('\n').contains(QStringLiteral("组件名称为空")));

        AltiumSchComponent duplicateName;
        duplicateName.name = QStringLiteral("valid");
        QVERIFY(!writer.write({named, duplicateName}, outputPath));
        QVERIFY(writer.diagnostics().join('\n').contains(QStringLiteral("组件名称重复（不区分大小写）")));

        AltiumSchComponent invalidPartCount;
        invalidPartCount.name = QStringLiteral("INVALID_PART_COUNT");
        invalidPartCount.partCount = 0;
        AltiumSchText invalidFontSizeText;
        invalidFontSizeText.text = QStringLiteral("invalid font size");
        invalidFontSizeText.fontName = QStringLiteral("Arial");
        invalidFontSizeText.fontSizeMm = std::numeric_limits<double>::infinity();
        invalidFontSizeText.anchor = QStringLiteral("unsupported");
        invalidPartCount.texts.append(invalidFontSizeText);
        AltiumSchText emptyText;
        invalidPartCount.texts.append(emptyText);
        AltiumSchPin invalidOwnerPin;
        invalidOwnerPin.name = QStringLiteral("A");
        invalidOwnerPin.designator = QStringLiteral("1");
        invalidOwnerPin.ownerPartId = 0;
        invalidPartCount.pins.append(invalidOwnerPin);
        AltiumSchRectangle invalidOwnerRectangle;
        invalidOwnerRectangle.cornerX = 100000;
        invalidOwnerRectangle.cornerY = 100000;
        invalidOwnerRectangle.ownerPartId = 0;
        invalidPartCount.rectangles.append(invalidOwnerRectangle);
        AltiumSchArc invalidArc;
        invalidArc.radius = 100000;
        invalidArc.startAngle = std::numeric_limits<double>::quiet_NaN();
        invalidArc.endAngle = std::numeric_limits<double>::infinity();
        invalidPartCount.arcs.append(invalidArc);
        AltiumSchPie invalidPie;
        invalidPie.radius = 100000;
        invalidPie.startAngle = std::numeric_limits<double>::infinity();
        invalidPartCount.pies.append(invalidPie);
        AltiumSchEllipticalArc invalidEllipticalArc;
        invalidEllipticalArc.radiusX = 100000;
        invalidEllipticalArc.radiusY = 50000;
        invalidEllipticalArc.endAngle = std::numeric_limits<double>::quiet_NaN();
        invalidPartCount.ellipticalArcs.append(invalidEllipticalArc);
        AltiumSchTextFrame emptyTextFrame;
        emptyTextFrame.cornerX = 100000;
        emptyTextFrame.cornerY = 100000;
        invalidPartCount.textFrames.append(emptyTextFrame);
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

        AltiumSchComponent invalidTextMargin;
        invalidTextMargin.name = QStringLiteral("INVALID_TEXT_MARGIN");
        AltiumSchTextFrame negativeTextMargin;
        negativeTextMargin.cornerX = 100000;
        negativeTextMargin.cornerY = 100000;
        negativeTextMargin.textMargin = -1;
        invalidTextMargin.textFrames.append(negativeTextMargin);
        QVERIFY(!writer.write({invalidTextMargin}, outputPath));
        QVERIFY(writer.diagnostics().join('\n').contains(QStringLiteral("文本框文本边距无效")));

        AltiumSchComponent invalidTextFrameBounds;
        invalidTextFrameBounds.name = QStringLiteral("INVALID_TEXT_FRAME_BOUNDS");
        AltiumSchTextFrame zeroTextFrame;
        zeroTextFrame.cornerX = 100000;
        invalidTextFrameBounds.textFrames.append(zeroTextFrame);
        QVERIFY(!writer.write({invalidTextFrameBounds}, outputPath));
        QVERIFY(writer.diagnostics().join('\n').contains(QStringLiteral("文本框边界尺寸无效")));

        AltiumSchComponent invalidRectangleBounds;
        invalidRectangleBounds.name = QStringLiteral("INVALID_RECTANGLE_BOUNDS");
        AltiumSchRectangle zeroRectangle;
        zeroRectangle.cornerX = 100000;
        invalidRectangleBounds.rectangles.append(zeroRectangle);
        QVERIFY(!writer.write({invalidRectangleBounds}, outputPath));
        QVERIFY(writer.diagnostics().join('\n').contains(QStringLiteral("矩形图元边界尺寸无效")));

        AltiumSchComponent invalidRoundRectangleBounds;
        invalidRoundRectangleBounds.name = QStringLiteral("INVALID_ROUND_RECTANGLE_BOUNDS");
        AltiumSchRoundRectangle zeroRoundRectangle;
        zeroRoundRectangle.cornerY = 100000;
        invalidRoundRectangleBounds.roundRectangles.append(zeroRoundRectangle);
        QVERIFY(!writer.write({invalidRoundRectangleBounds}, outputPath));
        QVERIFY(writer.diagnostics().join('\n').contains(QStringLiteral("圆角矩形图元边界尺寸无效")));

        AltiumSchComponent invalidLineGeometry;
        invalidLineGeometry.name = QStringLiteral("INVALID_LINE_GEOMETRY");
        invalidLineGeometry.lines.append(AltiumSchLine{});
        QVERIFY(!writer.write({invalidLineGeometry}, outputPath));
        QVERIFY(writer.diagnostics().join('\n').contains(QStringLiteral("线段图元长度无效")));

        AltiumSchComponent invalidStroke;
        invalidStroke.name = QStringLiteral("INVALID_STROKE_WIDTH");
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

        AltiumSchComponent invalidOrientation;
        invalidOrientation.name = QStringLiteral("INVALID_ORIENTATION");
        AltiumSchText invalidOrientationText;
        invalidOrientationText.text = QStringLiteral("invalid orientation");
        invalidOrientationText.orientation = 4;
        invalidOrientation.texts.append(invalidOrientationText);
        QVERIFY(!writer.write({invalidOrientation}, outputPath));
        QVERIFY(writer.diagnostics().join('\n').contains(QStringLiteral("文本图元方向无效: 4")));

        AltiumSchComponent invalidIeee;
        invalidIeee.name = QStringLiteral("INVALID_IEEE");
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

        AltiumSchComponent oversizedPartCount;
        oversizedPartCount.name = QStringLiteral("OVERSIZED_PART_COUNT");
        oversizedPartCount.partCount = std::numeric_limits<int>::max();
        QVERIFY(!writer.write({oversizedPartCount}, outputPath));
        QVERIFY(writer.diagnostics().join('\n').contains(QStringLiteral("partCount 超出支持范围")));

        QByteArray header;
        QVERIFY(readCfbStream(outputPath, QStringLiteral("FileHeader"), header));
        QVERIFY(header.contains("PARTCOUNT0=2"));

        AltiumSchLibReader emptyImageReader;
        QVERIFY2(emptyImageReader.open(outputPath), qPrintable(emptyImageReader.errorString()));
        QVector<AltiumSchLibReader::ImageStorageEntry> emptyImageEntries;
        QVERIFY2(emptyImageReader.readImageStorage(&emptyImageEntries), qPrintable(emptyImageReader.errorString()));
        QVERIFY(emptyImageEntries.isEmpty());

        QVector<AltiumSchLibReader::Record> invalidOwnerRecords;
        QVERIFY2(emptyImageReader.readComponentRecords(QStringLiteral("INVALID_PART_COUNT"), &invalidOwnerRecords),
                 qPrintable(emptyImageReader.errorString()));
        const int invalidWeightOffset = header.indexOf("WEIGHT=");
        const int invalidWeightEnd = header.indexOf('|', invalidWeightOffset);
        QVERIFY(invalidWeightOffset >= 0);
        QVERIFY(invalidWeightEnd > invalidWeightOffset);
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

        AltiumSchComponent invalidGeometry;
        invalidGeometry.name = QStringLiteral("INVALID_GEOMETRY");
        AltiumSchArc invalidRadiusArc;
        invalidGeometry.arcs.append(invalidRadiusArc);
        QVERIFY(!writer.write({invalidGeometry}, outputPath));
        QVERIFY(writer.diagnostics().join('\n').contains(QStringLiteral("圆弧半径无效")));

        AltiumSchComponent invalidImageGeometry;
        invalidImageGeometry.name = QStringLiteral("INVALID_IMAGE_GEOMETRY");
        AltiumSchImage invalidRotationImage;
        invalidRotationImage.rotation = std::numeric_limits<double>::quiet_NaN();
        invalidImageGeometry.images.append(invalidRotationImage);
        QVERIFY(!writer.write({invalidImageGeometry}, outputPath));
        QVERIFY(writer.diagnostics().join('\n').contains(QStringLiteral("图片旋转角度无效")));

        AltiumSchComponent invalidPolygon;
        invalidPolygon.name = QStringLiteral("INVALID_POLYGON");
        AltiumSchPolygon invalidPolygonData;
        invalidPolygonData.vertices = {QPointF(0, 0), QPointF(100000, 0)};
        invalidPolygon.polygons.append(invalidPolygonData);
        QVERIFY(!writer.write({invalidPolygon}, outputPath));
        QVERIFY(writer.diagnostics().join('\n').contains(QStringLiteral("多边形顶点数量不足")));

        AltiumSchComponent invalidPartOwner;
        invalidPartOwner.name = QStringLiteral("INVALID_PART_OWNER");
        invalidPartOwner.partCount = 2;
        AltiumSchRectangle outOfRangeRectangle;
        outOfRangeRectangle.cornerX = 100000;
        outOfRangeRectangle.cornerY = 100000;
        outOfRangeRectangle.ownerPartId = 3;
        invalidPartOwner.rectangles.append(outOfRangeRectangle);
        QVERIFY(!writer.write({invalidPartOwner}, outputPath));
        QVERIFY(writer.diagnostics().join('\n').contains(QStringLiteral("矩形图元 OWNERPARTID=3 超出部件范围")));

        AltiumSchComponent invalidCommonOwner;
        invalidCommonOwner.name = QStringLiteral("INVALID_COMMON_OWNER");
        AltiumSchRectangle invalidCommonRectangle;
        invalidCommonRectangle.cornerX = 100000;
        invalidCommonRectangle.cornerY = 100000;
        invalidCommonRectangle.ownerPartId = -2;
        invalidCommonOwner.rectangles.append(invalidCommonRectangle);
        QVERIFY(!writer.write({invalidCommonOwner}, outputPath));
        QVERIFY(writer.diagnostics().join('\n').contains(QStringLiteral("矩形图元 OWNERPARTID=-2 超出部件范围")));

        AltiumSchComponent invalidImplementationParameter;
        invalidImplementationParameter.name = QStringLiteral("INVALID_IMPLEMENTATION_PARAMETER");
        AltiumSchComponent::Implementation invalidImplementation;
        invalidImplementation.modelName = QStringLiteral("MODEL");
        invalidImplementation.parameters.insert(QStringLiteral("BAD|KEY"), QStringLiteral("value"));
        invalidImplementationParameter.implementations.append(invalidImplementation);
        QVERIFY(!writer.write({invalidImplementationParameter}, outputPath));
        QVERIFY(writer.diagnostics().join('\n').contains(QStringLiteral("实现参数键无效")));

        AltiumSchComponent invalidSourceMetadata;
        invalidSourceMetadata.name = QStringLiteral("INVALID_SOURCE_METADATA");
        invalidSourceMetadata.sourceMetadata.insert(QStringLiteral("BAD|KEY"), QStringLiteral("value"));
        QVERIFY(!writer.write({invalidSourceMetadata}, outputPath));
        QVERIFY(writer.diagnostics().join('\n').contains(QStringLiteral("源元数据参数名无效")));

        AltiumSchComponent invalidParameterName;
        invalidParameterName.name = QStringLiteral("INVALID_PARAMETER_NAME");
        AltiumSchParameter invalidParameter;
        invalidParameter.name = QStringLiteral("BAD|NAME");
        invalidParameter.value = QStringLiteral("value");
        invalidParameterName.parameters.append(invalidParameter);
        QVERIFY(!writer.write({invalidParameterName}, outputPath));
        QVERIFY(writer.diagnostics().join('\n').contains(QStringLiteral("参数参数名无效")));

        AltiumSchComponent invalidParameterValue;
        invalidParameterValue.name = QStringLiteral("INVALID_PARAMETER_VALUE");
        AltiumSchText invalidTextValue;
        invalidTextValue.text = QStringLiteral("BAD|TEXT");
        invalidParameterValue.texts.append(invalidTextValue);
        QVERIFY(!writer.write({invalidParameterValue}, outputPath));
        QVERIFY(writer.diagnostics().join('\n').contains(QStringLiteral("文本内容包含参数分隔符或 NUL")));

        AltiumPcbComponent invalidPcb;
        invalidPcb.name = QStringLiteral("INVALID_PCB_FLOATS");
        AltiumPcbPad invalidPad;
        invalidPad.sizeTopX = invalidPad.sizeTopY = 1000;
        invalidPad.sizeMidX = invalidPad.sizeMidY = 1000;
        invalidPad.sizeBotX = invalidPad.sizeBotY = 1000;
        invalidPad.holeSize = 1000;
        invalidPad.rotation = std::numeric_limits<double>::quiet_NaN();
        invalidPad.holeRotation = std::numeric_limits<double>::infinity();
        invalidPcb.pads.append(invalidPad);
        AltiumPcbArc invalidPcbArc;
        invalidPcbArc.radius = 1000;
        invalidPcbArc.width = 1000;
        invalidPcbArc.startAngle = std::numeric_limits<double>::infinity();
        invalidPcbArc.endAngle = std::numeric_limits<double>::quiet_NaN();
        invalidPcb.arcs.append(invalidPcbArc);
        AltiumPcbText invalidPcbText;
        invalidPcbText.rotation = std::numeric_limits<double>::quiet_NaN();
        invalidPcb.texts.append(invalidPcbText);
        AltiumPcbFill invalidPcbFill;
        invalidPcbFill.rotation = std::numeric_limits<double>::infinity();
        invalidPcb.fills.append(invalidPcbFill);
        AltiumPcbRegion invalidPcbRegion;
        invalidPcbRegion.vertices.append(QPointF(std::numeric_limits<double>::quiet_NaN(), 0.0));
        invalidPcbRegion.vertices.append(QPointF(1000.0, 0.0));
        invalidPcbRegion.vertices.append(QPointF(0.0, 1000.0));
        invalidPcb.regions.append(invalidPcbRegion);
        AltiumPcbComponentBody invalidBody;
        invalidBody.bodyOpacity3d = std::numeric_limits<double>::infinity();
        invalidBody.model2dRotation = std::numeric_limits<double>::quiet_NaN();
        invalidBody.outline.append(QPointF(0.0, std::numeric_limits<double>::quiet_NaN()));
        invalidPcb.bodies.append(invalidBody);
        AltiumPcbComponent::Model3D invalidModel;
        invalidModel.name = QStringLiteral("invalid.step");
        invalidModel.stepData = QByteArrayLiteral("ISO-10303-21;");
        invalidModel.rotX = std::numeric_limits<double>::infinity();
        invalidModel.rotY = std::numeric_limits<double>::quiet_NaN();
        invalidModel.rotZ = std::numeric_limits<double>::infinity();
        invalidModel.dz = std::numeric_limits<double>::quiet_NaN();
        invalidPcb.models.append(invalidModel);
        invalidPcb.bodies[0].modelId = invalidModel.id.isEmpty() ? invalidModel.name : invalidModel.id;
        AltiumPcbLibWriter pcbWriter;
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

        IR::FootprintComponentIR invalidFootprint;
        invalidFootprint.name = QStringLiteral("INVALID_FOOTPRINT_FLOATS");
        IR::FootprintPadIR invalidFootprintPad;
        invalidFootprintPad.number = QStringLiteral("1");
        invalidFootprintPad.size = QSizeF(1.0, 1.0);
        invalidFootprintPad.rotation = std::numeric_limits<double>::quiet_NaN();
        invalidFootprint.pads.append(invalidFootprintPad);
        ExporterAltiumFootprint footprintExporter;
        const QString footprintOutputPath = QDir(tempDir.path()).filePath(QStringLiteral("invalid-footprint.PcbLib"));
        QVERIFY(footprintExporter.exportFootprint(invalidFootprint, footprintOutputPath));
        QVERIFY(footprintExporter.diagnostics().join('\n').contains(QStringLiteral("焊盘旋转角度无效")));

        AltiumPcbLibWriter invalidInputWriter;
        AltiumPcbComponent unnamedPcb;
        QVERIFY(!invalidInputWriter.write({unnamedPcb}, pcbOutputPath));
        QVERIFY(invalidInputWriter.diagnostics().join('\n').contains(QStringLiteral("封装名称为空")));
        AltiumPcbComponent invalidHeightPcb;
        invalidHeightPcb.name = QStringLiteral("INVALID_HEIGHT");
        invalidHeightPcb.height = -1.0;
        QVERIFY(!invalidInputWriter.write({invalidHeightPcb}, pcbOutputPath));
        QVERIFY(invalidInputWriter.diagnostics().join('\n').contains(QStringLiteral("高度必须为非负有限值")));
        QVERIFY(!invalidInputWriter.write({unnamedPcb}, pcbOutputPath + QStringLiteral("|invalid")));
        QVERIFY(
            invalidInputWriter.diagnostics().join('\n').contains(QStringLiteral("输出路径包含参数分隔符、换行或 NUL")));
        AltiumPcbComponent oversizedPcbName;
        oversizedPcbName.name = QString(256, QChar('A'));
        QVERIFY(!invalidInputWriter.write({oversizedPcbName}, pcbOutputPath));
        QVERIFY(invalidInputWriter.diagnostics().join('\n').contains(QStringLiteral("名称超过 255 字节")));
        AltiumPcbComponent nonLatinPcbName;
        nonLatinPcbName.name = QStringLiteral("中文封装");
        QVERIFY(!invalidInputWriter.write({nonLatinPcbName}, pcbOutputPath));
        QVERIFY(invalidInputWriter.diagnostics().join('\n').contains(QStringLiteral("名称包含无法编码的字符")));
        AltiumPcbComponent invalidPadPcb;
        invalidPadPcb.name = QStringLiteral("INVALID_PAD_SIZE");
        invalidPadPcb.pads.append(AltiumPcbPad());
        QVERIFY(!invalidInputWriter.write({invalidPadPcb}, pcbOutputPath));
        QVERIFY(invalidInputWriter.diagnostics().join('\n').contains(QStringLiteral("非正焊盘尺寸")));
        AltiumPcbComponent oversizedPadDesignator;
        oversizedPadDesignator.name = QStringLiteral("OVERSIZED_PAD_DESIGNATOR");
        AltiumPcbPad oversizedPad;
        oversizedPad.designator = QString(256, QChar('A'));
        oversizedPadDesignator.pads.append(oversizedPad);
        QVERIFY(!invalidInputWriter.write({oversizedPadDesignator}, pcbOutputPath));
        QVERIFY(invalidInputWriter.diagnostics().join('\n').contains(QStringLiteral("焊盘编号超过 255 字节")));
        AltiumPcbComponent nonLatinPadDesignator;
        nonLatinPadDesignator.name = QStringLiteral("NON_LATIN_PAD");
        AltiumPcbPad nonLatinPad;
        nonLatinPad.designator = QStringLiteral("中文焊盘");
        nonLatinPadDesignator.pads.append(nonLatinPad);
        QVERIFY(!invalidInputWriter.write({nonLatinPadDesignator}, pcbOutputPath));
        QVERIFY(invalidInputWriter.diagnostics().join('\n').contains(QStringLiteral("焊盘编号包含无法编码的字符")));
        AltiumPcbComponent oversizedPcbText;
        oversizedPcbText.name = QStringLiteral("OVERSIZED_PCB_TEXT");
        AltiumPcbText oversizedText;
        oversizedText.text = QString(256, QChar('A'));
        oversizedPcbText.texts.append(oversizedText);
        QVERIFY(!invalidInputWriter.write({oversizedPcbText}, pcbOutputPath));
        QVERIFY(invalidInputWriter.diagnostics().join('\n').contains(QStringLiteral("文本内容超过 255 字节")));
        AltiumPcbComponent nonLatinPcbText;
        nonLatinPcbText.name = QStringLiteral("NON_LATIN_TEXT");
        AltiumPcbText nonLatinText;
        nonLatinText.text = QStringLiteral("中文文本");
        nonLatinPcbText.texts.append(nonLatinText);
        QVERIFY(!invalidInputWriter.write({nonLatinPcbText}, pcbOutputPath));
        QVERIFY(invalidInputWriter.diagnostics().join('\n').contains(QStringLiteral("文本内容包含无法编码的字符")));
        AltiumPcbComponent nonLatinModelComponent;
        nonLatinModelComponent.name = QStringLiteral("NON_LATIN_MODEL");
        AltiumPcbComponent::Model3D nonLatinModel;
        nonLatinModel.name = QStringLiteral("中文模型.step");
        nonLatinModel.stepData = QByteArrayLiteral("ISO-10303-21;");
        nonLatinModelComponent.models.append(nonLatinModel);
        QVERIFY(!invalidInputWriter.write({nonLatinModelComponent}, pcbOutputPath));
        QVERIFY(invalidInputWriter.diagnostics().join('\n').contains(QStringLiteral("3D 模型名称包含无法编码的字符")));
        AltiumPcbComponent invalidLayerPcb;
        invalidLayerPcb.name = QStringLiteral("INVALID_LAYER");
        AltiumPcbTrack invalidLayerTrack;
        invalidLayerTrack.layer = 0;
        invalidLayerPcb.tracks.append(invalidLayerTrack);
        QVERIFY(!invalidInputWriter.write({invalidLayerPcb}, pcbOutputPath));
        QVERIFY(invalidInputWriter.diagnostics().join('\n').contains(QStringLiteral("无效走线层号")));

        AltiumPcbComponent invalidPadShapePcb;
        invalidPadShapePcb.name = QStringLiteral("INVALID_PAD_SHAPE");
        AltiumPcbPad invalidPadShape;
        invalidPadShape.sizeTopX = invalidPadShape.sizeTopY = 1000;
        invalidPadShape.sizeMidX = invalidPadShape.sizeMidY = 1000;
        invalidPadShape.sizeBotX = invalidPadShape.sizeBotY = 1000;
        invalidPadShape.holeSize = 1000;
        invalidPadShape.shapeTop = 4;
        invalidPadShapePcb.pads.append(invalidPadShape);
        QVERIFY(!invalidInputWriter.write({invalidPadShapePcb}, pcbOutputPath));
        QVERIFY(invalidInputWriter.diagnostics().join('\n').contains(QStringLiteral("无效焊盘形状")));

        AltiumPcbComponent invalidTrackWidthPcb;
        invalidTrackWidthPcb.name = QStringLiteral("INVALID_TRACK_WIDTH");
        AltiumPcbTrack invalidTrackWidth;
        invalidTrackWidth.width = 0;
        invalidTrackWidthPcb.tracks.append(invalidTrackWidth);
        QVERIFY(!invalidInputWriter.write({invalidTrackWidthPcb}, pcbOutputPath));
        QVERIFY(invalidInputWriter.diagnostics().join('\n').contains(QStringLiteral("非正走线宽度")));

        AltiumPcbComponent invalidHolePcb;
        invalidHolePcb.name = QStringLiteral("INVALID_SLOT");
        AltiumPcbPad invalidSlot;
        invalidSlot.sizeTopX = invalidSlot.sizeTopY = 1000;
        invalidSlot.sizeMidX = invalidSlot.sizeMidY = 1000;
        invalidSlot.sizeBotX = invalidSlot.sizeBotY = 1000;
        invalidSlot.holeSize = 1000;
        invalidSlot.holeType = 2;
        invalidHolePcb.pads.append(invalidSlot);
        QVERIFY(!invalidInputWriter.write({invalidHolePcb}, pcbOutputPath));
        QVERIFY(invalidInputWriter.diagnostics().join('\n').contains(QStringLiteral("槽孔长度非正")));

        AltiumPcbComponent duplicatePcbName;
        duplicatePcbName.name = QStringLiteral("duplicate-pcb");
        AltiumPcbComponent duplicatePcbNameCase;
        duplicatePcbNameCase.name = QStringLiteral("DUPLICATE-PCB");
        QVERIFY(!invalidInputWriter.write({duplicatePcbName, duplicatePcbNameCase}, pcbOutputPath));
        QVERIFY(invalidInputWriter.diagnostics().join('\n').contains(QStringLiteral("封装名称重复（不区分大小写）")));

        AltiumPcbComponent invalidExtendedInfo;
        invalidExtendedInfo.name = QStringLiteral("INVALID_EXTENDED_INFO");
        AltiumPcbPad extendedInfoPad;
        extendedInfoPad.sizeTopX = extendedInfoPad.sizeTopY = 1000;
        extendedInfoPad.sizeMidX = extendedInfoPad.sizeMidY = 1000;
        extendedInfoPad.sizeBotX = extendedInfoPad.sizeBotY = 1000;
        extendedInfoPad.holeSize = 1000;
        invalidExtendedInfo.pads.append(extendedInfoPad);
        AltiumPcbExtendedPrimitiveInfo invalidExtendedEntry;
        invalidExtendedEntry.primitiveIndex = 1;
        invalidExtendedEntry.objectName = QStringLiteral("Pad");
        invalidExtendedInfo.extendedPrimitives.append(invalidExtendedEntry);
        QVERIFY(!invalidInputWriter.write({invalidExtendedInfo}, pcbOutputPath));
        QVERIFY(invalidInputWriter.diagnostics().join('\n').contains(QStringLiteral("扩展图元索引无效")));

        AltiumPcbComponent invalidModelData;
        invalidModelData.name = QStringLiteral("INVALID_MODEL_DATA");
        AltiumPcbComponent::Model3D emptyModel;
        emptyModel.name = QStringLiteral("empty.step");
        invalidModelData.models.append(emptyModel);
        QVERIFY(!invalidInputWriter.write({invalidModelData}, pcbOutputPath));
        QVERIFY(invalidInputWriter.diagnostics().join('\n').contains(QStringLiteral("3D 模型数据为空")));

        AltiumPcbComponent duplicateModelIdComponent;
        duplicateModelIdComponent.name = QStringLiteral("DUPLICATE_MODEL_ID");
        AltiumPcbComponent::Model3D firstModel;
        firstModel.name = QStringLiteral("first.step");
        firstModel.id = QStringLiteral("model-id");
        firstModel.stepData = QByteArrayLiteral("ISO-10303-21;");
        AltiumPcbComponent::Model3D secondModel = firstModel;
        secondModel.name = QStringLiteral("second.step");
        secondModel.id = QStringLiteral("MODEL-ID");
        duplicateModelIdComponent.models = {firstModel, secondModel};
        QVERIFY(!invalidInputWriter.write({duplicateModelIdComponent}, pcbOutputPath));
        QVERIFY(invalidInputWriter.diagnostics().join('\n').contains(QStringLiteral("3D 模型 ID 重复")));

        AltiumPcbComponent invalidModelIdComponent;
        invalidModelIdComponent.name = QStringLiteral("INVALID_MODEL_ID");
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

        AltiumPcbComponent invalidBodyComponent;
        invalidBodyComponent.name = QStringLiteral("INVALID_BODY");
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

        AltiumPcbComponent invalidBodyAssociation;
        invalidBodyAssociation.name = QStringLiteral("INVALID_BODY_ASSOCIATION");
        invalidBodyAssociation.models = {firstModel};
        AltiumPcbComponentBody unassociatedBody;
        unassociatedBody.modelId = QStringLiteral("unknown-model");
        invalidBodyAssociation.bodies = {unassociatedBody};
        QVERIFY(!invalidInputWriter.write({invalidBodyAssociation}, pcbOutputPath));
        QVERIFY(invalidInputWriter.diagnostics().join('\n').contains(QStringLiteral("未关联有效模型")));

        AltiumPcbComponent invalidCStringComponent;
        invalidCStringComponent.name = QStringLiteral("INVALID_CSTRING");
        AltiumPcbRegion invalidCStringRegion;
        invalidCStringRegion.name = QStringLiteral("BAD|REGION");
        invalidCStringRegion.vertices = {QPointF(0.0, 0.0), QPointF(1000.0, 0.0), QPointF(0.0, 1000.0)};
        invalidCStringComponent.regions.append(invalidCStringRegion);
        QVERIFY(!invalidInputWriter.write({invalidCStringComponent}, pcbOutputPath));
        QVERIFY(invalidInputWriter.diagnostics().join('\n').contains(QStringLiteral("区域名称包含参数分隔符或 NUL")));

        invalidCStringComponent.regions.clear();
        AltiumPcbComponentBody invalidCStringBody;
        invalidCStringBody.name = QStringLiteral("BAD\0BODY");
        invalidCStringComponent.bodies.append(invalidCStringBody);
        QVERIFY(!invalidInputWriter.write({invalidCStringComponent}, pcbOutputPath));
        QVERIFY(
            invalidInputWriter.diagnostics().join('\n').contains(QStringLiteral("3D 元件体名称包含参数分隔符或 NUL")));

        invalidCStringComponent.bodies.clear();
        AltiumPcbRegion extensionRegion;
        extensionRegion.vertices = {QPointF(0.0, 0.0), QPointF(1000.0, 0.0), QPointF(0.0, 1000.0)};
        invalidCStringComponent.regions.append(extensionRegion);
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
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        AltiumSchComponent component;
        component.name = QStringLiteral("REUSED_WRITER");
        AltiumSchRectangle rectangle;
        rectangle.cornerX = 100000;
        rectangle.cornerY = 100000;
        component.rectangles.append(rectangle);

        AltiumSchLibWriter writer;
        const QString firstPath = QDir(tempDir.path()).filePath(QStringLiteral("first.SchLib"));
        const QString secondPath = QDir(tempDir.path()).filePath(QStringLiteral("second.SchLib"));
        QVERIFY(writer.write({component}, firstPath));
        QVERIFY(writer.write({component}, secondPath));

        AltiumSchLibReader reader;
        QVERIFY2(reader.open(secondPath), qPrintable(reader.errorString()));
        QVector<AltiumSchLibReader::Record> records;
        QVERIFY2(reader.readComponentRecords(0, &records), qPrintable(reader.errorString()));
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
    void validatesEmbeddedImageStorageNamesAndPayloads() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QByteArray imageData = QByteArrayLiteral("embedded-image-payload");
        const auto makeImage = [&](const QString& fileName, const QByteArray& data) {
            AltiumSchImage image;
            image.fileName = fileName;
            image.data = data;
            image.embedImage = true;
            image.cornerX = 100000;
            image.cornerY = 100000;
            return image;
        };

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

        AltiumSchLibWriter writer;
        const QString outputPath = QDir(tempDir.path()).filePath(QStringLiteral("image-diagnostics.SchLib"));
        QVERIFY(writer.write({component}, outputPath));
        const QString diagnostics = writer.diagnostics().join('\n');
        QVERIFY(diagnostics.contains(QStringLiteral("嵌入文件名 SAME.PNG 重复")));
        QVERIFY(diagnostics.contains(QStringLiteral("嵌入文件名无效: invalid?.png")));
        QVERIFY(diagnostics.contains(QStringLiteral("嵌入文件名无效:    ，已跳过 Storage")));
        QVERIFY(diagnostics.contains(QStringLiteral("嵌入文件名无效: CON.png")));
        QVERIFY(diagnostics.contains(QStringLiteral("嵌入文件名无效: trailing.png ")));
        QVERIFY(diagnostics.contains(QStringLiteral("嵌入文件名无效: zero")));
        QVERIFY(diagnostics.contains(QStringLiteral("嵌入文件名无效:")));
        QVERIFY(diagnostics.contains(QStringLiteral("嵌入数据为空")));

        AltiumSchLibReader reader;
        QVERIFY2(reader.open(outputPath), qPrintable(reader.errorString()));
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
     *          验证 FileHeader 参数、Data 流内容、Library 元数据和图元记录布局
     */
    void writesSchLibAndPcbLibStorages() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        AltiumSchComponent symbol;
        symbol.name = QStringLiteral("C2040");
        symbol.partCount = 1;
        symbol.sourceMetadata.insert(QStringLiteral("manufacturer"), QStringLiteral("Example Corp"));
        symbol.sourceMetadata.insert(QStringLiteral("lcscId"), QStringLiteral("C2040"));
        symbol.aliases = {QStringLiteral("C2040_ALIAS")};
        AltiumSchParameter parameter;
        parameter.name = QStringLiteral("Temperature Coefficient");
        parameter.value = QStringLiteral("±50ppm/K");
        parameter.readOnly = true;
        symbol.parameters.append(parameter);

        AltiumSchPin pin;
        pin.name = QStringLiteral("A");
        pin.designator = QStringLiteral("1");
        pin.locationX = 100000;
        pin.locationY = 200000;
        pin.length = 100000;
        pin.electricalType = AltiumModels::PinElectricalType::Passive;
        pin.orientation = AltiumModels::PinOrientation::Right;
        symbol.pins.append(pin);

        AltiumSchRectangle rect;
        rect.locationX = 100000;
        rect.locationY = 100000;
        rect.cornerX = 500000;
        rect.cornerY = 500000;
        rect.lineStyle = 1;
        symbol.rectangles.append(rect);

        AltiumSchRoundRectangle roundRect;
        roundRect.locationX = 600000;
        roundRect.locationY = 100000;
        roundRect.cornerX = 900000;
        roundRect.cornerY = 500000;
        roundRect.cornerXRadius = 50000;
        roundRect.cornerYRadius = 75000;
        symbol.roundRectangles.append(roundRect);

        AltiumSchPath path;
        path.vertices = {QPointF(0, 0), QPointF(100000, 100000), QPointF(200000, 0)};
        path.ownerPartId = 1;
        symbol.paths.append(path);
        AltiumSchIeee ieee;
        ieee.symbol = 18;  // Pi
        ieee.locationX = 300000;
        ieee.locationY = 300000;
        ieee.mirrored = true;
        symbol.ieeeSymbols.append(ieee);
        AltiumSchPie pie;
        pie.centerX = 1000000;
        pie.centerY = 300000;
        pie.radius = 100000;
        pie.startAngle = 30.0;
        pie.endAngle = 150.0;
        symbol.pies.append(pie);
        AltiumSchEllipticalArc ellipticalArc;
        ellipticalArc.centerX = 1200000;
        ellipticalArc.centerY = 300000;
        ellipticalArc.radiusX = 150000;
        ellipticalArc.radiusY = 80000;
        ellipticalArc.startAngle = 45.0;
        ellipticalArc.endAngle = 270.0;
        symbol.ellipticalArcs.append(ellipticalArc);
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
        AltiumSchText fallbackFontText;
        fallbackFontText.locationX = 1600000;
        fallbackFontText.locationY = 700000;
        fallbackFontText.text = QStringLiteral("Fallback font");
        fallbackFontText.fontId = 99;
        symbol.texts.append(fallbackFontText);
        AltiumSchText sizeOnlyText;
        sizeOnlyText.locationX = 1650000;
        sizeOnlyText.locationY = 700000;
        sizeOnlyText.text = QStringLiteral("Size-only font");
        sizeOnlyText.fontId = 1;
        sizeOnlyText.fontSizeMm = 25.4 / 72.0 * 12.0;
        symbol.texts.append(sizeOnlyText);
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

        AltiumSchComponent::Implementation impl;
        impl.modelName = QStringLiteral("模型-高精度");
        impl.modelType = QStringLiteral("PCBLIB");
        impl.parameters.insert(QStringLiteral("热模型"), QStringLiteral("高精度"));
        impl.pinMappings.insert(QStringLiteral("1"), QStringLiteral("A1"));
        symbol.implementations.append(impl);
        AltiumSchComponent::Implementation simulationImpl;
        simulationImpl.modelName = QStringLiteral("C2040_SPICE");
        simulationImpl.modelType = QStringLiteral("SIM");
        symbol.implementations.append(simulationImpl);

        const QString schPath = QDir(tempDir.path()).filePath(QStringLiteral("easyeda_convertlib.SchLib"));
        AltiumSchLibWriter schWriter;
        QVERIFY(schWriter.write({symbol}, schPath, QStringLiteral("easyeda_convertlib")));

        AltiumSchComponent invalidBezierComponent;
        invalidBezierComponent.name = QStringLiteral("INVALID_DIRECT_BEZIER");
        AltiumSchBezier invalidBezier;
        invalidBezier.controlPoints = {QPointF(0, 0), QPointF(100000, 0), QPointF(200000, 0)};
        invalidBezierComponent.beziers.append(invalidBezier);
        AltiumSchLibWriter invalidBezierWriter;
        const QString invalidBezierOutput =
            QDir(tempDir.path()).filePath(QStringLiteral("invalid-direct-bezier.SchLib"));
        QVERIFY(!invalidBezierWriter.write({invalidBezierComponent}, invalidBezierOutput));
        QVERIFY(invalidBezierWriter.diagnostics().join('\n').contains(QStringLiteral("Bézier 控制点数量无效")));

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

        QByteArray schData;
        QVERIFY(readCfbStream(schPath, QStringLiteral("C2040/Data"), schData));
        const int weightOffset = schHeader.indexOf("WEIGHT=");
        const int weightEnd = schHeader.indexOf('|', weightOffset);
        QVERIFY(weightOffset >= 0);
        QVERIFY(weightEnd > weightOffset);
        bool weightOk = false;
        const int headerWeight =
            QString::fromLatin1(schHeader.mid(weightOffset + 7, weightEnd - weightOffset - 7)).toInt(&weightOk);
        QVERIFY(weightOk);
        QCOMPARE(countSchLibRecords(schData), headerWeight);
        QVERIFY(schData.mid(4).startsWith("|RECORD=1|"));
        QVERIFY(schData.contains("LibReference=C2040"));
        QVERIFY(schData.contains("RECORD=14"));
        QVERIFY(!schData.contains("|IndexInSheet=0|"));
        QVERIFY(schData.contains("OWNERPARTDISPLAYMODE=1"));
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
        const int fallbackTextOffset = schData.indexOf("Text=Fallback font");
        const int fallbackRecordOffset = schData.lastIndexOf("|RECORD=4|", fallbackTextOffset);
        const int nextFallbackRecordOffset = schData.indexOf("|RECORD=", fallbackTextOffset + 1);
        QVERIFY(fallbackTextOffset > fallbackRecordOffset);
        QVERIFY(nextFallbackRecordOffset > fallbackRecordOffset);
        const QByteArray fallbackRecord =
            schData.mid(fallbackRecordOffset, nextFallbackRecordOffset - fallbackRecordOffset);
        QVERIFY(fallbackRecord.contains("FontID=1"));
        const int sizeOnlyTextOffset = schData.indexOf("Text=Size-only font");
        const int sizeOnlyRecordOffset = schData.lastIndexOf("|RECORD=4|", sizeOnlyTextOffset);
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
        QByteArray imageStorage;
        QVERIFY(readCfbStream(schPath, QStringLiteral("Storage"), imageStorage));
        QVERIFY(imageStorage.contains("logo.png"));
        const int imageNameOffset = imageStorage.indexOf("logo.png");
        QVERIFY(imageNameOffset >= 2);
        QCOMPARE(static_cast<uint8_t>(imageStorage.at(imageNameOffset - 2)), static_cast<uint8_t>(0xD0));
        QCOMPARE(static_cast<uint8_t>(imageStorage.at(imageNameOffset - 1)), static_cast<uint8_t>(8));
        const int imageLengthOffset = imageNameOffset + 8;
        QVERIFY(imageLengthOffset + 4 <= imageStorage.size());
        const quint32 compressedLength = readU32(imageStorage, imageLengthOffset);
        const int imageDataOffset = imageLengthOffset + 4;
        QVERIFY(compressedLength > 0);
        QVERIFY(imageDataOffset + static_cast<int>(compressedLength) <= imageStorage.size());
        QByteArray expectedSize(4, '\0');
        expectedSize[3] = static_cast<char>(image.data.size());
        const QByteArray restoredImage =
            qUncompress(expectedSize + imageStorage.mid(imageDataOffset, compressedLength));
        QCOMPARE(restoredImage, image.data);
        AltiumSchLibReader imageReader;
        QVERIFY2(imageReader.open(schPath), qPrintable(imageReader.errorString()));
        QVector<AltiumSchLibReader::ImageStorageEntry> imageEntries;
        QVERIFY2(imageReader.readImageStorage(&imageEntries), qPrintable(imageReader.errorString()));
        QCOMPARE(imageEntries.size(), 1);
        QCOMPARE(imageEntries.first().flags, quint8(1));
        QCOMPARE(imageEntries.first().name, QStringLiteral("logo.png"));
        QCOMPARE(qUncompress(expectedSize + imageEntries.first().compressedData), image.data);
        QVector<AltiumSchLibReader::Record> implementationRecords;
        QVERIFY2(imageReader.readComponentRecords(QStringLiteral("C2040"), &implementationRecords),
                 qPrintable(imageReader.errorString()));
        bool foundUnicodeImplementation = false;
        bool foundUnicodeImplementationParameters = false;
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

        AltiumPcbComponent footprint;
        footprint.name = QStringLiteral("LQFN-56_L7.0-W7.0-P0.4-EP");
        footprint.description = QStringLiteral("LQFN-56");
        footprint.height = 0.7;

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

        AltiumPcbTrack track;
        track.startX = 0;
        track.startY = 0;
        track.endX = 10000;
        track.endY = 10000;
        track.width = 5000;
        track.layer = 33;
        footprint.tracks.append(track);

        AltiumPcbArc arc;
        arc.centerX = 30000;
        arc.centerY = 40000;
        arc.radius = 5000;
        arc.startAngle = 15.0;
        arc.endAngle = 225.0;
        arc.width = 800;
        arc.layer = 1;
        footprint.arcs.append(arc);

        const QString pcbPath = QDir(tempDir.path()).filePath(QStringLiteral("easyeda_convertlib.PcbLib"));
        AltiumPcbLibWriter pcbWriter;
        QVERIFY(pcbWriter.write({footprint}, pcbPath, QStringLiteral("easyeda_convertlib")));

        QByteArray pcbHeader;
        QVERIFY(readCfbStream(pcbPath, QStringLiteral("FileHeader"), pcbHeader));
        QCOMPARE(pcbHeader.size(), 32);
        QCOMPARE(readU32(pcbHeader, 0), quint32(27));
        QCOMPARE(static_cast<quint8>(pcbHeader.at(4)), quint8(27));
        QCOMPARE(pcbHeader.mid(5), QByteArrayLiteral("PCB 6.0 Binary Library File"));

        QByteArray libraryData;
        QVERIFY(readCfbStream(pcbPath, QStringLiteral("Library/Data"), libraryData));
        QVERIFY(libraryData.contains("LQFN-56_L7.0-W7.0-P0.4-EP"));
        QVERIFY((readU32(libraryData, 0) & 0x00FFFFFFU) > 10000);
        QVERIFY(libraryData.contains("KIND=Protel_Advanced_PCB"));
        QVERIFY(libraryData.contains("LAYER82NAME=Via Holes"));

        QByteArray footprintParameters;
        QVERIFY(readCfbStream(pcbPath, QStringLiteral("LQFN-56_L7.0-W7.0-P0.4-EP/Parameters"), footprintParameters));
        QVERIFY(footprintParameters.mid(4).startsWith("|PATTERN="));

        QByteArray footprintData;
        QVERIFY(readCfbStream(pcbPath, QStringLiteral("LQFN-56_L7.0-W7.0-P0.4-EP/Data"), footprintData));
        QVERIFY(footprintData.contains("LQFN-56_L7.0-W7.0-P0.4-EP"));
        int primitiveOffset = 4 + static_cast<int>(readU32(footprintData, 0));
        QCOMPARE(static_cast<quint8>(footprintData.at(primitiveOffset++)), quint8(2));
        for (int i = 0; i < 4; ++i) {
            primitiveOffset += 4 + static_cast<int>(readU32(footprintData, primitiveOffset) & 0x00FFFFFFU);
        }
        primitiveOffset += 4 + static_cast<int>(readU32(footprintData, primitiveOffset) & 0x00FFFFFFU);
        primitiveOffset += 4 + static_cast<int>(readU32(footprintData, primitiveOffset) & 0x00FFFFFFU);
        QCOMPARE(static_cast<quint8>(footprintData.at(primitiveOffset++)), quint8(4));
        QCOMPARE(readU32(footprintData, primitiveOffset) & 0x00FFFFFFU, quint32(36));

        OLECompoundReader schReader;
        QVERIFY2(schReader.open(schPath), qPrintable(schReader.errorString()));
        QVERIFY(schReader.containsStream(QStringLiteral("C2040/Data")));
        QByteArray schReaderData;
        QVERIFY(schReader.readStream(QStringLiteral("C2040/Data"), &schReaderData));
        QVERIFY(schReaderData.contains("RECORD=41"));

        OLECompoundReader pcbReader;
        QVERIFY2(pcbReader.open(pcbPath), qPrintable(pcbReader.errorString()));
        QVERIFY(pcbReader.containsStream(QStringLiteral("Library/Data")));
        QByteArray pcbReaderData;
        QVERIFY(pcbReader.readStream(QStringLiteral("Library/Data"), &pcbReaderData));
        QVERIFY(!pcbReaderData.isEmpty());

        AltiumPcbLibReader pcbLibraryReader;
        QVERIFY2(pcbLibraryReader.open(pcbPath), qPrintable(pcbLibraryReader.errorString()));
        const auto pcbComponents = pcbLibraryReader.components();
        QCOMPARE(pcbComponents.size(), 1);
        QCOMPARE(pcbComponents.first().name, QStringLiteral("LQFN-56_L7.0-W7.0-P0.4-EP"));
        QCOMPARE(pcbComponents.first().sectionKey, pcbComponents.first().name);
        QCOMPARE(pcbLibraryReader.fileVersion(), QStringLiteral("PCB 6.0 Binary Library File"));
        QVERIFY(pcbLibraryReader.libraryMetadata().contains("KIND=Protel_Advanced_PCB"));
        QByteArray footprintReaderData;
        QVERIFY(pcbLibraryReader.readFootprintStream(0, QStringLiteral("Data"), &footprintReaderData));
        QVERIFY(footprintReaderData.contains("LQFN-56_L7.0-W7.0-P0.4-EP"));
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
        QByteArray reconstructedFootprintObjects;
        for (const auto& object : footprintObjects)
            reconstructedFootprintObjects.append(object.encoded);
        const int footprintNameSize = static_cast<int>(readU32(footprintReaderData, 0));
        QCOMPARE(reconstructedFootprintObjects, footprintReaderData.mid(4 + footprintNameSize));
        QByteArray footprintHeaderReaderData;
        QVERIFY(pcbLibraryReader.readFootprintStream(0, QStringLiteral("Header"), &footprintHeaderReaderData));
        AltiumBinaryReader footprintHeaderReader(footprintHeaderReaderData);
        uint32_t primitiveCount = 0;
        QVERIFY(footprintHeaderReader.readUInt32(&primitiveCount));
        QVERIFY(primitiveCount > 0);

        AltiumSchLibReader schLibraryReader;
        QVERIFY2(schLibraryReader.open(schPath), qPrintable(schLibraryReader.errorString()));
        const auto schComponents = schLibraryReader.components();
        QCOMPARE(schComponents.size(), 1);
        QCOMPARE(schComponents.first().name, QStringLiteral("C2040"));
        QCOMPARE(schComponents.first().sectionKey, QStringLiteral("C2040"));
        QCOMPARE(schComponents.first().partCount, 1);
        QCOMPARE(schLibraryReader.headerParameters().value(QStringLiteral("COMPCOUNT")), QStringLiteral("1"));
        const auto fonts = schLibraryReader.fonts();
        QVERIFY(fonts.size() >= 2);
        QCOMPARE(fonts.at(0).name, QStringLiteral("Times New Roman"));
        QCOMPARE(fonts.at(1).name, QStringLiteral("Arial"));
        QCOMPARE(fonts.at(1).size, 8);
        QVERIFY(fonts.at(1).bold);
        QVERIFY(fonts.at(1).italic);
        QByteArray componentData;
        QVERIFY(schLibraryReader.readComponentData(0, &componentData));
        QVERIFY(componentData.contains("LibReference=C2040"));
        QVector<AltiumSchLibReader::Record> records;
        QVERIFY(schLibraryReader.readComponentRecords(0, &records));
        QCOMPARE(records.size(), headerWeight);
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
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        AltiumPcbComponent footprint;
        footprint.name = QStringLiteral("TEST_PAD");

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

        AltiumPcbText text;
        text.text = QStringLiteral("REF**");
        text.layer = 33;
        footprint.texts.append(text);

        const QString pcbPath = QDir(tempDir.path()).filePath(QStringLiteral("test_pad.PcbLib"));
        AltiumPcbLibWriter pcbWriter;
        QVERIFY(pcbWriter.write({footprint}, pcbPath));

        QByteArray footprintData;
        QVERIFY(readCfbStream(pcbPath, QStringLiteral("TEST_PAD/Data"), footprintData));
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

        AltiumPcbLibReader pcbReader;
        QVERIFY2(pcbReader.open(pcbPath), qPrintable(pcbReader.errorString()));
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
            OLECompoundWriter writer;
            if (!writer.create() || !writer.writeStream(QStringLiteral("FileHeader"), [&] {
                    QByteArray data;
                    AltiumBinaryWriter binaryWriter(data);
                    binaryWriter.writeInt32(27);
                    binaryWriter.writePascalShortString(QStringLiteral("PCB 6.0 Binary Library File"));
                    return data;
                }()))
                return false;

            if (!writer.addStorage(QStringLiteral("Library")))
                return false;
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
                    QByteArray data;
                    AltiumBinaryWriter binaryWriter(data);
                    binaryWriter.writeCStringParameterBlock({});
                    return data;
                }()))
                return false;
            return writer.saveToFile(filePath);
        };

        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        QByteArray truncatedHeaderData;
        AltiumBinaryWriter truncatedHeaderWriter(truncatedHeaderData);
        truncatedHeaderWriter.writeStringBlock(QStringLiteral("BROKEN"));
        truncatedHeaderWriter.writeUInt8(AltiumConstants::PCB_OBJECT_TRACK);
        truncatedHeaderWriter.beginBlock();
        truncatedHeaderWriter.writeBytes(QByteArray(12, '\0'));
        truncatedHeaderWriter.endBlock();
        const QString truncatedHeaderPath = QDir(tempDir.path()).filePath(QStringLiteral("truncated-header.PcbLib"));
        QVERIFY(writeMalformedLibrary(truncatedHeaderPath, truncatedHeaderData));

        AltiumPcbLibReader truncatedHeaderReader;
        QVERIFY2(truncatedHeaderReader.open(truncatedHeaderPath), qPrintable(truncatedHeaderReader.errorString()));
        QVector<AltiumPcbLibReader::PrimitiveRecord> objects;
        QVERIFY(!truncatedHeaderReader.readFootprintObjects(QStringLiteral("BROKEN"), &objects));
        QVERIFY(objects.isEmpty());
        QVERIFY(truncatedHeaderReader.hasError());
        QVERIFY(truncatedHeaderReader.errorString().contains(QStringLiteral("公共头部")));
        QByteArray rawData;
        QVERIFY(truncatedHeaderReader.readFootprintStream(QStringLiteral("BROKEN"), QStringLiteral("Data"), &rawData));
        QCOMPARE(rawData, truncatedHeaderData);

        QByteArray malformedStringData;
        AltiumBinaryWriter malformedStringWriter(malformedStringData);
        malformedStringWriter.writeStringBlock(QStringLiteral("BROKEN"));
        malformedStringWriter.writeUInt8(AltiumConstants::PCB_OBJECT_PAD);
        malformedStringWriter.beginBlock();
        malformedStringWriter.writeBytes(QByteArrayLiteral("bad"));
        malformedStringWriter.endBlock();
        const QString malformedStringPath = QDir(tempDir.path()).filePath(QStringLiteral("malformed-string.PcbLib"));
        QVERIFY(writeMalformedLibrary(malformedStringPath, malformedStringData));

        AltiumPcbLibReader malformedStringReader;
        QVERIFY2(malformedStringReader.open(malformedStringPath), qPrintable(malformedStringReader.errorString()));
        QVERIFY(!malformedStringReader.readFootprintObjects(QStringLiteral("BROKEN"), &objects));
        QVERIFY(objects.isEmpty());
        QVERIFY(malformedStringReader.hasError());
        QVERIFY(malformedStringReader.errorString().contains(QStringLiteral("字符串子块")));
        QVERIFY(malformedStringReader.readFootprintStream(0, QStringLiteral("Data"), &rawData));
        QCOMPARE(rawData, malformedStringData);

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
        const QString invalidArcPath = QDir(tempDir.path()).filePath(QStringLiteral("invalid-arc-fields.PcbLib"));
        QVERIFY(writeMalformedLibrary(invalidArcPath, invalidArcData));

        AltiumPcbLibReader invalidArcReader;
        QVERIFY2(invalidArcReader.open(invalidArcPath), qPrintable(invalidArcReader.errorString()));
        QVERIFY(!invalidArcReader.readFootprintObjects(QStringLiteral("BROKEN"), &objects));
        QVERIFY(invalidArcReader.errorString().contains(QStringLiteral("弧线半径必须为正")));

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
        const QString invalidRegionPath = QDir(tempDir.path()).filePath(QStringLiteral("invalid-region-fields.PcbLib"));
        QVERIFY(writeMalformedLibrary(invalidRegionPath, invalidRegionData));

        AltiumPcbLibReader invalidRegionReader;
        QVERIFY2(invalidRegionReader.open(invalidRegionPath), qPrintable(invalidRegionReader.errorString()));
        QVERIFY(!invalidRegionReader.readFootprintObjects(QStringLiteral("BROKEN"), &objects));
        QVERIFY(invalidRegionReader.errorString().contains(QStringLiteral("区域至少需要三个顶点")));

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

        AltiumPcbLibReader invalidBodyOpacityReader;
        QVERIFY2(invalidBodyOpacityReader.open(invalidBodyOpacityPath),
                 qPrintable(invalidBodyOpacityReader.errorString()));
        QVERIFY(!invalidBodyOpacityReader.readFootprintObjects(QStringLiteral("BROKEN"), &objects));
        QVERIFY(invalidBodyOpacityReader.errorString().contains(QStringLiteral("透明度必须在 0..1 范围内")));

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
        const QString invalidLayerPath = QDir(tempDir.path()).filePath(QStringLiteral("invalid-layer.PcbLib"));
        QVERIFY(writeMalformedLibrary(invalidLayerPath, invalidLayerData));

        AltiumPcbLibReader invalidLayerReader;
        QVERIFY2(invalidLayerReader.open(invalidLayerPath), qPrintable(invalidLayerReader.errorString()));
        QVERIFY(!invalidLayerReader.readFootprintObjects(QStringLiteral("BROKEN"), &objects));
        QVERIFY(invalidLayerReader.errorString().contains(QStringLiteral("图元层号必须在 1..74")));

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

        AltiumPcbLibReader invalidTrackWidthReader;
        QVERIFY2(invalidTrackWidthReader.open(invalidTrackWidthPath),
                 qPrintable(invalidTrackWidthReader.errorString()));
        QVERIFY(!invalidTrackWidthReader.readFootprintObjects(QStringLiteral("BROKEN"), &objects));
        QVERIFY(invalidTrackWidthReader.errorString().contains(QStringLiteral("走线宽度必须为正")));

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
        const QString invalidArcWidthPath = QDir(tempDir.path()).filePath(QStringLiteral("invalid-arc-width.PcbLib"));
        QVERIFY(writeMalformedLibrary(invalidArcWidthPath, invalidArcWidthData));

        AltiumPcbLibReader invalidArcWidthReader;
        QVERIFY2(invalidArcWidthReader.open(invalidArcWidthPath), qPrintable(invalidArcWidthReader.errorString()));
        QVERIFY(!invalidArcWidthReader.readFootprintObjects(QStringLiteral("BROKEN"), &objects));
        QVERIFY(invalidArcWidthReader.errorString().contains(QStringLiteral("弧线宽度必须为正")));

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
        QByteArray textPayload;
        AltiumBinaryWriter textPayloadWriter(textPayload);
        textPayloadWriter.writeUInt8(1);
        textPayloadWriter.writeBytes(QByteArrayLiteral("T"));
        invalidTextSizeWriter.beginBlock();
        invalidTextSizeWriter.writeBytes(textPayload);
        invalidTextSizeWriter.endBlock();
        const QString invalidTextSizePath = QDir(tempDir.path()).filePath(QStringLiteral("invalid-text-size.PcbLib"));
        QVERIFY(writeMalformedLibrary(invalidTextSizePath, invalidTextSizeData));

        AltiumPcbLibReader invalidTextSizeReader;
        QVERIFY2(invalidTextSizeReader.open(invalidTextSizePath), qPrintable(invalidTextSizeReader.errorString()));
        QVERIFY(!invalidTextSizeReader.readFootprintObjects(QStringLiteral("BROKEN"), &objects));
        QVERIFY(invalidTextSizeReader.errorString().contains(QStringLiteral("文本尺寸必须有效")));

        QByteArray invalidTextIndexData = invalidTextSizeData;
        const int wideStringIndexOffset = 4 + 7 + 1 + 4 + 13 + 12 + 2 + 8 + 1 + 4 + 4 + 71;
        QVERIFY(wideStringIndexOffset + 4 <= invalidTextIndexData.size());
        const int textHeightOffset = 4 + 7 + 1 + 4 + 13 + 8;
        invalidTextIndexData[textHeightOffset] = static_cast<char>(1);
        invalidTextIndexData[textHeightOffset + 1] = '\0';
        invalidTextIndexData[textHeightOffset + 2] = '\0';
        invalidTextIndexData[textHeightOffset + 3] = '\0';
        invalidTextIndexData[wideStringIndexOffset] = static_cast<char>(7);
        invalidTextIndexData[wideStringIndexOffset + 1] = '\0';
        invalidTextIndexData[wideStringIndexOffset + 2] = '\0';
        invalidTextIndexData[wideStringIndexOffset + 3] = '\0';
        const QString invalidTextIndexPath = QDir(tempDir.path()).filePath(QStringLiteral("invalid-text-index.PcbLib"));
        QVERIFY(writeMalformedLibrary(invalidTextIndexPath, invalidTextIndexData));

        AltiumPcbLibReader invalidTextIndexReader;
        QVERIFY2(invalidTextIndexReader.open(invalidTextIndexPath), qPrintable(invalidTextIndexReader.errorString()));
        QVERIFY(!invalidTextIndexReader.readFootprintObjects(QStringLiteral("BROKEN"), &objects));
        QVERIFY(invalidTextIndexReader.errorString().contains(QStringLiteral("不存在的 WideStrings 索引")));

        IR::FootprintComponentIR validPadFootprint;
        validPadFootprint.name = QStringLiteral("VALID_PAD");
        IR::FootprintPadIR validPad;
        validPad.number = QStringLiteral("1");
        validPad.position = QPointF(0.0, 0.0);
        validPad.size = QSizeF(1.0, 1.0);
        validPadFootprint.pads.append(validPad);
        const QString validPadPath = QDir(tempDir.path()).filePath(QStringLiteral("valid-pad.PcbLib"));
        ExporterAltiumFootprint validPadExporter;
        QVERIFY(
            validPadExporter.exportFootprintLibrary({validPadFootprint}, QStringLiteral("valid-pad"), validPadPath));

        AltiumPcbLibReader validPadReader;
        QVERIFY2(validPadReader.open(validPadPath), qPrintable(validPadReader.errorString()));
        QByteArray validPadData;
        QVERIFY(validPadReader.readFootprintStream(QStringLiteral("VALID_PAD"), QStringLiteral("Data"), &validPadData));
        AltiumBinaryReader validPadBinaryReader(validPadData);
        QString validPadName;
        QVERIFY(validPadBinaryReader.readStringBlock(&validPadName));
        uint8_t padObjectId = 0;
        QVERIFY(validPadBinaryReader.readUInt8(&padObjectId));
        QByteArray invalidPadData;
        AltiumBinaryWriter invalidPadWriter(invalidPadData);
        invalidPadWriter.writeStringBlock(validPadName);
        invalidPadWriter.writeUInt8(padObjectId);
        for (int blockIndex = 0; blockIndex < 6; ++blockIndex) {
            QByteArray payload;
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
        const QString invalidPadPath = QDir(tempDir.path()).filePath(QStringLiteral("invalid-pad-shape.PcbLib"));
        QVERIFY(writeMalformedLibrary(invalidPadPath, invalidPadData));

        AltiumPcbLibReader invalidPadReader;
        QVERIFY2(invalidPadReader.open(invalidPadPath), qPrintable(invalidPadReader.errorString()));
        QVERIFY(!invalidPadReader.readFootprintObjects(QStringLiteral("BROKEN"), &objects));
        QVERIFY(invalidPadReader.errorString().contains(QStringLiteral("焊盘形状无效")));

        AltiumBinaryReader invalidThroughHoleBinaryReader(validPadData);
        QString invalidThroughHolePadName;
        QVERIFY(invalidThroughHoleBinaryReader.readStringBlock(&invalidThroughHolePadName));
        uint8_t throughHolePadObjectId = 0;
        QVERIFY(invalidThroughHoleBinaryReader.readUInt8(&throughHolePadObjectId));
        QByteArray invalidThroughHolePadData;
        AltiumBinaryWriter invalidThroughHolePadWriter(invalidThroughHolePadData);
        invalidThroughHolePadWriter.writeStringBlock(invalidThroughHolePadName);
        invalidThroughHolePadWriter.writeUInt8(throughHolePadObjectId);
        for (int blockIndex = 0; blockIndex < 6; ++blockIndex) {
            QByteArray payload;
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
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        OLECompoundWriter writer;
        QVERIFY(writer.create());
        QByteArray headerData;
        AltiumBinaryWriter headerWriter(headerData);
        headerWriter.writeCStringParameterBlock({{QStringLiteral("COMPCOUNT"), QStringLiteral("1")}});
        headerWriter.writeInt32(1);
        headerWriter.writeStringBlock(QStringLiteral("BROKEN"));
        QVERIFY(writer.writeStream(QStringLiteral("FileHeader"), headerData));
        QVERIFY(writer.addStorage(QStringLiteral("BROKEN")));
        const QByteArray malformedData(2, '\0');
        QVERIFY(writer.writeStream(QStringLiteral("BROKEN"), QStringLiteral("Data"), malformedData));
        QByteArray malformedStorage;
        AltiumBinaryWriter storageWriter(malformedStorage);
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
        const QString path = QDir(tempDir.path()).filePath(QStringLiteral("malformed-record.SchLib"));
        QVERIFY(writer.saveToFile(path));

        AltiumSchLibReader reader;
        QVERIFY2(reader.open(path), qPrintable(reader.errorString()));
        QVector<AltiumSchLibReader::Record> records;
        QVERIFY(!reader.readComponentRecords(QStringLiteral("BROKEN"), &records));
        QVERIFY(records.isEmpty());
        QVERIFY(reader.hasError());
        QVERIFY(reader.errorString().contains(QStringLiteral("记录失败")));
        QVector<AltiumSchLibReader::ImageStorageEntry> imageEntries;
        QVERIFY(!reader.readImageStorage(&imageEntries));
        QVERIFY(imageEntries.isEmpty());
        QVERIFY(reader.errorString().contains(QStringLiteral("重复或非法文件名")));
        QByteArray rawData;
        QVERIFY(reader.readComponentData(QStringLiteral("BROKEN"), &rawData));
        QCOMPARE(rawData, malformedData);
    }

    /**
     * @brief 验证 SchLib 图片 Storage 的损坏压缩数据会被拒绝
     */
    void rejectsCorruptSchLibImageCompression() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        OLECompoundWriter writer;
        QVERIFY(writer.create());
        QByteArray headerData;
        AltiumBinaryWriter headerWriter(headerData);
        headerWriter.writeCStringParameterBlock({{QStringLiteral("COMPCOUNT"), QStringLiteral("1")}});
        headerWriter.writeInt32(1);
        headerWriter.writeStringBlock(QStringLiteral("BROKEN"));
        QVERIFY(writer.writeStream(QStringLiteral("FileHeader"), headerData));
        QVERIFY(writer.addStorage(QStringLiteral("BROKEN")));
        QVERIFY(writer.writeStream(QStringLiteral("BROKEN"), QStringLiteral("Data"), QByteArray()));

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

        const QString path = QDir(tempDir.path()).filePath(QStringLiteral("corrupt-image.SchLib"));
        QVERIFY(writer.saveToFile(path));

        AltiumSchLibReader reader;
        QVERIFY2(reader.open(path), qPrintable(reader.errorString()));
        QVector<AltiumSchLibReader::ImageStorageEntry> entries;
        QVERIFY(!reader.readImageStorage(&entries));
        QVERIFY(entries.isEmpty());
        QVERIFY(reader.errorString().contains(QStringLiteral("压缩数据无效")));
    }

    /**
     * @brief 验证 SchLib 图片 Storage 的压缩长度字段必须与实际载荷一致。
     */
    void rejectsMismatchedSchLibImageCompressionLength() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        OLECompoundWriter writer;
        QVERIFY(writer.create());
        QByteArray headerData;
        AltiumBinaryWriter headerWriter(headerData);
        headerWriter.writeCStringParameterBlock({{QStringLiteral("COMPCOUNT"), QStringLiteral("1")}});
        headerWriter.writeInt32(1);
        headerWriter.writeStringBlock(QStringLiteral("BROKEN_LENGTH"));
        QVERIFY(writer.writeStream(QStringLiteral("FileHeader"), headerData));
        QVERIFY(writer.addStorage(QStringLiteral("BROKEN_LENGTH")));
        QVERIFY(writer.writeStream(QStringLiteral("BROKEN_LENGTH"), QStringLiteral("Data"), QByteArray()));

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

        const QString path = QDir(tempDir.path()).filePath(QStringLiteral("mismatched-image-length.SchLib"));
        QVERIFY(writer.saveToFile(path));

        AltiumSchLibReader reader;
        QVERIFY2(reader.open(path), qPrintable(reader.errorString()));
        QVector<AltiumSchLibReader::ImageStorageEntry> entries;
        QVERIFY(!reader.readImageStorage(&entries));
        QVERIFY(entries.isEmpty());
        QVERIFY(reader.errorString().contains(QStringLiteral("条目内容无效")));
    }

    /**
     * @brief 验证 SchLib 图片 Storage 的空解压数据会被拒绝。
     */
    void rejectsEmptySchLibImageCompression() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        OLECompoundWriter writer;
        QVERIFY(writer.create());
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

        const QString path = QDir(tempDir.path()).filePath(QStringLiteral("empty-image.SchLib"));
        QVERIFY(writer.saveToFile(path));

        AltiumSchLibReader reader;
        QVERIFY2(reader.open(path), qPrintable(reader.errorString()));
        QVector<AltiumSchLibReader::ImageStorageEntry> entries;
        QVERIFY(!reader.readImageStorage(&entries));
        QVERIFY(entries.isEmpty());
        QVERIFY(reader.errorString().contains(QStringLiteral("压缩数据无效")));
    }

    /**
     * @brief 验证 SchLib 参数数值字段损坏时不会静默回退
     */
    void rejectsMalformedSchLibRecordMetadata() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        OLECompoundWriter writer;
        QVERIFY(writer.create());
        QByteArray headerData;
        AltiumBinaryWriter headerWriter(headerData);
        headerWriter.writeCStringParameterBlock(
            {{QStringLiteral("COMPCOUNT"), QStringLiteral("1")}, {QStringLiteral("PARTCOUNT0"), QStringLiteral("2")}});
        headerWriter.writeInt32(1);
        headerWriter.writeStringBlock(QStringLiteral("BROKEN_METADATA"));
        QVERIFY(writer.writeStream(QStringLiteral("FileHeader"), headerData));
        QVERIFY(writer.addStorage(QStringLiteral("BROKEN_METADATA")));
        QByteArray componentData;
        AltiumBinaryWriter componentWriter(componentData);
        componentWriter.writeCStringParameterBlock({{QStringLiteral("RECORD"), QStringLiteral("1")},
                                                    {QStringLiteral("FONTID"), QStringLiteral("not-a-number")}});
        QVERIFY(writer.writeStream(QStringLiteral("BROKEN_METADATA"), QStringLiteral("Data"), componentData));
        const QString path = QDir(tempDir.path()).filePath(QStringLiteral("malformed-record-metadata.SchLib"));
        QVERIFY(writer.saveToFile(path));

        AltiumSchLibReader reader;
        QVERIFY2(reader.open(path), qPrintable(reader.errorString()));
        QVector<AltiumSchLibReader::Record> records;
        QVERIFY(!reader.readComponentRecords(QStringLiteral("BROKEN_METADATA"), &records));
        QVERIFY(records.isEmpty());
        QVERIFY(reader.errorString().contains(QStringLiteral("FONTID 无效")));
        QByteArray rawData;
        QVERIFY(reader.readComponentData(QStringLiteral("BROKEN_METADATA"), &rawData));
        QCOMPARE(rawData, componentData);
    }

    /**
     * @brief 验证 SchLib 记录不会接受小于 -1 的 OWNERPARTID
     */
    void rejectsInvalidSchLibOwnerPartId() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        OLECompoundWriter writer;
        QVERIFY(writer.create());
        QByteArray headerData;
        AltiumBinaryWriter headerWriter(headerData);
        headerWriter.writeCStringParameterBlock(
            {{QStringLiteral("COMPCOUNT"), QStringLiteral("1")}, {QStringLiteral("PARTCOUNT0"), QStringLiteral("2")}});
        headerWriter.writeInt32(1);
        headerWriter.writeStringBlock(QStringLiteral("INVALID_OWNER"));
        QVERIFY(writer.writeStream(QStringLiteral("FileHeader"), headerData));
        QVERIFY(writer.addStorage(QStringLiteral("INVALID_OWNER")));

        QByteArray componentData;
        AltiumBinaryWriter componentWriter(componentData);
        componentWriter.writeCStringParameterBlock(
            {{QStringLiteral("RECORD"), QStringLiteral("14")}, {QStringLiteral("OWNERPARTID"), QStringLiteral("-2")}});
        QVERIFY(writer.writeStream(QStringLiteral("INVALID_OWNER"), QStringLiteral("Data"), componentData));
        const QString path = QDir(tempDir.path()).filePath(QStringLiteral("invalid-owner.SchLib"));
        QVERIFY(writer.saveToFile(path));

        AltiumSchLibReader reader;
        QVERIFY2(reader.open(path), qPrintable(reader.errorString()));
        QVector<AltiumSchLibReader::Record> records;
        QVERIFY(!reader.readComponentRecords(QStringLiteral("INVALID_OWNER"), &records));
        QVERIFY(records.isEmpty());
        QVERIFY(reader.errorString().contains(QStringLiteral("OWNERPARTID 小于 -1")));
    }

    /**
     * @brief 验证 SchLib 组件名称和 SectionKeys 映射不会重复
     */
    void rejectsDuplicateSchLibComponentsAndSectionKeys() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const auto createLibrary = [&](const QString& fileName,
                                       const QStringList& names,
                                       const QStringList& sectionKeys,
                                       const QStringList& sectionKeyRefs) {
            OLECompoundWriter writer;
            if (!writer.create())
                return false;

            QByteArray headerData;
            AltiumBinaryWriter headerWriter(headerData);
            headerWriter.writeCStringParameterBlock({{QStringLiteral("COMPCOUNT"), QString::number(names.size())}});
            headerWriter.writeInt32(names.size());
            for (const QString& name : names)
                headerWriter.writeStringBlock(name);
            if (!writer.writeStream(QStringLiteral("FileHeader"), headerData))
                return false;

            if (!sectionKeys.isEmpty()) {
                QByteArray sectionKeyData;
                AltiumBinaryWriter sectionKeyWriter(sectionKeyData);
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
        AltiumSchLibReader duplicateComponentReader;
        QVERIFY(!duplicateComponentReader.open(
            QDir(tempDir.path()).filePath(QStringLiteral("duplicate-components.SchLib"))));
        QVERIFY(duplicateComponentReader.errorString().contains(QStringLiteral("组件名称重复")));

        QVERIFY(createLibrary(QStringLiteral("duplicate-section-keys.SchLib"),
                              {QStringLiteral("A"), QStringLiteral("B")},
                              {QStringLiteral("SHARED"), QStringLiteral("shared")},
                              {}));
        AltiumSchLibReader duplicateSectionKeyReader;
        QVERIFY(!duplicateSectionKeyReader.open(
            QDir(tempDir.path()).filePath(QStringLiteral("duplicate-section-keys.SchLib"))));
        QVERIFY(duplicateSectionKeyReader.errorString().contains(QStringLiteral("SectionKeys 映射重复")));

        QVERIFY(createLibrary(QStringLiteral("case-mismatched-section-key.SchLib"),
                              {QStringLiteral("A"), QStringLiteral("B")},
                              {QStringLiteral("KEY_A"), QStringLiteral("KEY_B")},
                              {QStringLiteral("a"), QStringLiteral("B")}));
        AltiumSchLibReader caseMismatchedReader;
        QVERIFY(!caseMismatchedReader.open(
            QDir(tempDir.path()).filePath(QStringLiteral("case-mismatched-section-key.SchLib"))));
        QVERIFY(caseMismatchedReader.errorString().contains(QStringLiteral("SectionKeys 包含未知组件")));
    }

    /**
     * @brief 验证 SchLib 同一字段的大小写别名不能携带冲突值
     */
    void rejectsConflictingSchLibParameterAliases() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        OLECompoundWriter writer;
        QVERIFY(writer.create());
        QByteArray headerData;
        AltiumBinaryWriter headerWriter(headerData);
        headerWriter.writeCStringParameterBlock(
            {{QStringLiteral("COMPCOUNT"), QStringLiteral("1")}, {QStringLiteral("PARTCOUNT0"), QStringLiteral("2")}});
        headerWriter.writeInt32(1);
        headerWriter.writeStringBlock(QStringLiteral("CONFLICTING_ALIASES"));
        QVERIFY(writer.writeStream(QStringLiteral("FileHeader"), headerData));
        QVERIFY(writer.addStorage(QStringLiteral("CONFLICTING_ALIASES")));

        QByteArray componentData;
        AltiumBinaryWriter componentWriter(componentData);
        componentWriter.writeCStringParameterBlock({{QStringLiteral("RECORD"), QStringLiteral("14")},
                                                    {QStringLiteral("OWNERPARTID"), QStringLiteral("1")},
                                                    {QStringLiteral("OwnerPartId"), QStringLiteral("2")}});
        QVERIFY(writer.writeStream(QStringLiteral("CONFLICTING_ALIASES"), QStringLiteral("Data"), componentData));
        const QString path = QDir(tempDir.path()).filePath(QStringLiteral("conflicting-aliases.SchLib"));
        QVERIFY(writer.saveToFile(path));

        AltiumSchLibReader reader;
        QVERIFY2(reader.open(path), qPrintable(reader.errorString()));
        QVector<AltiumSchLibReader::Record> records;
        QVERIFY(!reader.readComponentRecords(QStringLiteral("CONFLICTING_ALIASES"), &records));
        QVERIFY(records.isEmpty());
        QVERIFY(reader.errorString().contains(QStringLiteral("数值字段无效")));
    }

    /**
     * @brief 验证 SchLib 内容记录索引不能重复或倒序。
     */
    void rejectsNonMonotonicSchLibRecordIndexes() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        OLECompoundWriter writer;
        QVERIFY(writer.create());
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

        QByteArray componentData;
        AltiumBinaryWriter componentWriter(componentData);
        componentWriter.writeCStringParameterBlock({{QStringLiteral("RECORD"), QStringLiteral("13")},
                                                    {QStringLiteral("OWNERPARTID"), QStringLiteral("1")},
                                                    {QStringLiteral("IndexInSheet"), QStringLiteral("1")}});
        componentWriter.writeCStringParameterBlock({{QStringLiteral("RECORD"), QStringLiteral("13")},
                                                    {QStringLiteral("OWNERPARTID"), QStringLiteral("1")},
                                                    {QStringLiteral("IndexInSheet"), QStringLiteral("1")}});
        QVERIFY(writer.writeStream(QStringLiteral("NON_MONOTONIC"), QStringLiteral("Data"), componentData));
        const QString path = QDir(tempDir.path()).filePath(QStringLiteral("non-monotonic.SchLib"));
        QVERIFY(writer.saveToFile(path));

        AltiumSchLibReader reader;
        QVERIFY2(reader.open(path), qPrintable(reader.errorString()));
        QVector<AltiumSchLibReader::Record> records;
        QVERIFY(!reader.readComponentRecords(QStringLiteral("NON_MONOTONIC"), &records));
        QVERIFY(records.isEmpty());
        QVERIFY(reader.errorString().contains(QStringLiteral("IndexInSheet 非递增")));
    }

    /**
     * @brief 验证二进制引脚也参与 SchLib 内容序号连续性校验。
     */
    void rejectsSchLibRecordIndexGapAcrossBinaryPin() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        OLECompoundWriter writer;
        QVERIFY(writer.create());
        QByteArray headerData;
        AltiumBinaryWriter headerWriter(headerData);
        headerWriter.writeCStringParameterBlock(
            {{QStringLiteral("COMPCOUNT"), QStringLiteral("1")}, {QStringLiteral("PARTCOUNT0"), QStringLiteral("2")}});
        headerWriter.writeInt32(1);
        headerWriter.writeStringBlock(QStringLiteral("INDEX_GAP"));
        QVERIFY(writer.writeStream(QStringLiteral("FileHeader"), headerData));
        QVERIFY(writer.addStorage(QStringLiteral("INDEX_GAP")));

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
        const QString path = QDir(tempDir.path()).filePath(QStringLiteral("index-gap.SchLib"));
        QVERIFY(writer.saveToFile(path));

        AltiumSchLibReader reader;
        QVERIFY2(reader.open(path), qPrintable(reader.errorString()));
        QVector<AltiumSchLibReader::Record> records;
        QVERIFY(!reader.readComponentRecords(QStringLiteral("INDEX_GAP"), &records));
        QVERIFY(records.isEmpty());
        QVERIFY(reader.errorString().contains(QStringLiteral("IndexInSheet 不连续")));
    }

    /**
     * @brief 验证 SchLib 已知几何记录的半径和点列字段不会静默接受无效值。
     */
    void rejectsInvalidSchLibGeometryRecords() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        OLECompoundWriter writer;
        QVERIFY(writer.create());
        QByteArray headerData;
        AltiumBinaryWriter headerWriter(headerData);
        headerWriter.writeCStringParameterBlock(
            {{QStringLiteral("COMPCOUNT"), QStringLiteral("1")}, {QStringLiteral("PARTCOUNT0"), QStringLiteral("2")}});
        headerWriter.writeInt32(1);
        headerWriter.writeStringBlock(QStringLiteral("INVALID_GEOMETRY_RECORD"));
        QVERIFY(writer.writeStream(QStringLiteral("FileHeader"), headerData));
        QVERIFY(writer.addStorage(QStringLiteral("INVALID_GEOMETRY_RECORD")));

        QByteArray componentData;
        AltiumBinaryWriter componentWriter(componentData);
        componentWriter.writeCStringParameterBlock({{QStringLiteral("RECORD"), QStringLiteral("8")},
                                                    {QStringLiteral("Radius"), QStringLiteral("0")},
                                                    {QStringLiteral("SecondaryRadius"), QStringLiteral("1")}});
        QVERIFY(writer.writeStream(QStringLiteral("INVALID_GEOMETRY_RECORD"), QStringLiteral("Data"), componentData));
        const QString path = QDir(tempDir.path()).filePath(QStringLiteral("invalid-geometry-record.SchLib"));
        QVERIFY(writer.saveToFile(path));

        AltiumSchLibReader reader;
        QVERIFY2(reader.open(path), qPrintable(reader.errorString()));
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
                QTemporaryDir tempDir;
                QVERIFY(tempDir.isValid());

                OLECompoundWriter writer;
                QVERIFY(writer.create());
                QByteArray headerData;
                AltiumBinaryWriter headerWriter(headerData);
                headerWriter.writeCStringParameterBlock({{QStringLiteral("COMPCOUNT"), QStringLiteral("1")}});
                headerWriter.writeInt32(1);
                headerWriter.writeStringBlock(componentName);
                QVERIFY(writer.writeStream(QStringLiteral("FileHeader"), headerData));
                QVERIFY(writer.addStorage(componentName));

                QByteArray componentData;
                AltiumBinaryWriter componentWriter(componentData);
                componentWriter.writeCStringParameterBlock(parameters);
                QVERIFY(writer.writeStream(componentName, QStringLiteral("Data"), componentData));
                const QString path = QDir(tempDir.path()).filePath(componentName + QStringLiteral(".SchLib"));
                QVERIFY(writer.saveToFile(path));

                AltiumSchLibReader reader;
                QVERIFY2(reader.open(path), qPrintable(reader.errorString()));
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
                QTemporaryDir tempDir;
                QVERIFY(tempDir.isValid());

                OLECompoundWriter writer;
                QVERIFY(writer.create());
                QByteArray headerData;
                AltiumBinaryWriter headerWriter(headerData);
                headerWriter.writeCStringParameterBlock({{QStringLiteral("COMPCOUNT"), QStringLiteral("1")}});
                headerWriter.writeInt32(1);
                headerWriter.writeStringBlock(componentName);
                QVERIFY(writer.writeStream(QStringLiteral("FileHeader"), headerData));
                QVERIFY(writer.addStorage(componentName));

                QByteArray componentData;
                AltiumBinaryWriter componentWriter(componentData);
                componentWriter.writeCStringParameterBlock(parameters);
                QVERIFY(writer.writeStream(componentName, QStringLiteral("Data"), componentData));
                const QString path = QDir(tempDir.path()).filePath(componentName + QStringLiteral(".SchLib"));
                QVERIFY(writer.saveToFile(path));

                AltiumSchLibReader reader;
                QVERIFY2(reader.open(path), qPrintable(reader.errorString()));
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
                QTemporaryDir tempDir;
                QVERIFY(tempDir.isValid());

                OLECompoundWriter writer;
                QVERIFY(writer.create());
                QByteArray headerData;
                AltiumBinaryWriter headerWriter(headerData);
                headerWriter.writeCStringParameterBlock({{QStringLiteral("COMPCOUNT"), QStringLiteral("1")}});
                headerWriter.writeInt32(1);
                headerWriter.writeStringBlock(componentName);
                QVERIFY(writer.writeStream(QStringLiteral("FileHeader"), headerData));
                QVERIFY(writer.addStorage(componentName));

                QByteArray componentData;
                AltiumBinaryWriter componentWriter(componentData);
                componentWriter.writeCStringParameterBlock(parameters);
                QVERIFY(writer.writeStream(componentName, QStringLiteral("Data"), componentData));
                const QString path = QDir(tempDir.path()).filePath(componentName + QStringLiteral(".SchLib"));
                QVERIFY(writer.saveToFile(path));

                AltiumSchLibReader reader;
                QVERIFY2(reader.open(path), qPrintable(reader.errorString()));
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
            auto parameters = degenerateBounds;
            parameters.insert(QStringLiteral("RECORD"), testCase.first);
            verifyRejectedRecord(parameters, testCase.second, QStringLiteral("DEGENERATE_") + testCase.first);
        }

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
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        AltiumPcbComponent footprint;
        footprint.name = QStringLiteral("TEST_UID");

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

        AltiumPcbTrack track;
        track.startX = 0;
        track.startY = 0;
        track.endX = 10000;
        track.endY = 0;
        track.width = 1000;
        track.layer = 33;
        footprint.tracks.append(track);

        const QString pcbPath = QDir(tempDir.path()).filePath(QStringLiteral("test_uid.PcbLib"));
        AltiumPcbLibWriter pcbWriter;
        QVERIFY(pcbWriter.write({footprint}, pcbPath));

        // 验证 UniqueIdPrimitiveInformation 流存在
        QByteArray uidHeader;
        QVERIFY(readCfbStream(pcbPath, QStringLiteral("TEST_UID/UniqueIdPrimitiveInformation/Header"), uidHeader));
        QCOMPARE(readU32(uidHeader, 0), quint32(2));  // 1 pad + 1 track = 2 图元

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
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

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

        AltiumPcbComponentBody body;
        body.layerName = QStringLiteral(" mechanical1 ");
        body.name = QStringLiteral("__BODY__");
        body.modelId = model.id;
        body.modelName = model.name;
        body.overallHeightRaw = 10000;  // 1mil
        body.outline = {QPointF(-50000, -50000), QPointF(50000, -50000), QPointF(50000, 50000), QPointF(-50000, 50000)};
        footprint.bodies.append(body);

        AltiumPcbFill fill;
        fill.corner1X = -20000;
        fill.corner1Y = -10000;
        fill.corner2X = 20000;
        fill.corner2Y = 10000;
        fill.rotation = 15.0;
        fill.layer = 33;
        footprint.fills.append(fill);

        AltiumPcbRegion region;
        region.layer = 33;
        region.kind = 2;
        region.name = QStringLiteral("COURTYARD");
        region.vertices = {
            QPointF(-30000, -30000), QPointF(30000, -30000), QPointF(30000, 30000), QPointF(-30000, 30000)};
        footprint.regions.append(region);

        const QString pcbPath = QDir(tempDir.path()).filePath(QStringLiteral("test_body.PcbLib"));
        AltiumPcbLibWriter pcbWriter;
        QVERIFY(pcbWriter.write({footprint}, pcbPath));

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

        AltiumPcbLibReader reader;
        QVERIFY2(reader.open(pcbPath), qPrintable(reader.errorString()));
        QVector<AltiumPcbLibReader::PrimitiveRecord> objects;
        QVERIFY2(reader.readFootprintObjects(QStringLiteral("TEST_BODY"), &objects), qPrintable(reader.errorString()));

        bool foundFill = false;
        bool foundRegion = false;
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
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        AltiumSchComponent symbol;
        symbol.name = QStringLiteral("TEST_UTF8");
        symbol.description = QStringLiteral("测试中文描述");
        symbol.partCount = 1;
        symbol.sourceMetadata.insert(QStringLiteral("manufacturer"), QStringLiteral("测试厂商"));

        AltiumSchPin pin;
        pin.name = QStringLiteral("A");
        pin.designator = QStringLiteral("1");
        pin.locationX = 100000;
        pin.locationY = 200000;
        pin.length = 100000;
        pin.electricalType = AltiumModels::PinElectricalType::Passive;
        pin.orientation = AltiumModels::PinOrientation::Right;
        symbol.pins.append(pin);

        AltiumSchText text;
        text.text = QStringLiteral("中文文本");
        text.locationX = 300000;
        text.locationY = 300000;
        text.fontId = 1;
        text.color = 0;
        symbol.texts.append(text);
        AltiumSchText arialText;
        arialText.text = QStringLiteral("Arial 文本");
        arialText.fontName = QStringLiteral("Arial");
        arialText.fontSizeMm = 25.4 / 72.0 * 10.0;
        symbol.texts.append(arialText);
        AltiumSchText lowercaseArialText = arialText;
        lowercaseArialText.text = QStringLiteral("arial 文本");
        lowercaseArialText.fontName = QStringLiteral("arial");
        symbol.texts.append(lowercaseArialText);
        AltiumSchParameter invalidFontParameter;
        invalidFontParameter.name = QStringLiteral("InvalidFont");
        invalidFontParameter.value = QStringLiteral("回退字体");
        invalidFontParameter.fontId = 99;
        symbol.parameters.append(invalidFontParameter);

        const QString schPath = QDir(tempDir.path()).filePath(QStringLiteral("test_utf8.SchLib"));
        AltiumSchLibWriter schWriter;
        QVERIFY(schWriter.write({symbol}, schPath));

        QByteArray schData;
        QVERIFY(readCfbStream(schPath, QStringLiteral("TEST_UTF8/Data"), schData));

        // 验证包含 %UTF8% 参数
        QVERIFY(schData.contains("%UTF8%ComponentDescription=") || schData.contains("ComponentDescription="));
        QVERIFY(schData.contains("%UTF8%Text=") || schData.contains("Text="));
        QVERIFY(schData.contains(QStringLiteral("测试中文描述").toUtf8()));
        QVERIFY(schData.contains(QStringLiteral("测试厂商").toUtf8()));
        QVERIFY(schData.contains(QStringLiteral("中文文本").toUtf8()));

        AltiumSchLibReader reader;
        QVERIFY2(reader.open(schPath), qPrintable(reader.errorString()));
        QVector<AltiumSchLibReader::Record> records;
        QVERIFY2(reader.readComponentRecords(QStringLiteral("TEST_UTF8"), &records), qPrintable(reader.errorString()));
        bool sawFontReference = false;
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

        IR::SymbolComponentIR rotatedTextSymbol;
        rotatedTextSymbol.name = QStringLiteral("ROTATED_TEXT");
        IR::SymbolTextIR rotatedText;
        rotatedText.text = QStringLiteral("方向");
        rotatedText.rotation = -90.0;
        rotatedText.fontSizeMm = 2.5;
        rotatedTextSymbol.texts.append(rotatedText);
        IR::SymbolParameterIR rotatedParameter;
        rotatedParameter.name = QStringLiteral("角度参数");
        rotatedParameter.value = QStringLiteral("值");
        rotatedParameter.rotation = -90.0;
        rotatedParameter.fontSizeMm = 2.5;
        rotatedTextSymbol.parameters.append(rotatedParameter);
        IR::SymbolParameterIR firstPartParameter;
        firstPartParameter.name = QStringLiteral("第一部件参数");
        firstPartParameter.value = QStringLiteral("P1");
        firstPartParameter.partIndex = 0;
        rotatedTextSymbol.parameters.append(firstPartParameter);
        IR::SymbolParameterIR sharedFirstPartParameter;
        sharedFirstPartParameter.name = QStringLiteral("Shared");
        sharedFirstPartParameter.value = QStringLiteral("P1");
        sharedFirstPartParameter.partIndex = 0;
        rotatedTextSymbol.parameters.append(sharedFirstPartParameter);
        IR::SymbolParameterIR sharedSecondPartParameter = sharedFirstPartParameter;
        sharedSecondPartParameter.value = QStringLiteral("P2");
        sharedSecondPartParameter.partIndex = 1;
        rotatedTextSymbol.parameters.append(sharedSecondPartParameter);
        IR::SymbolParameterIR commonParameter;
        commonParameter.name = QStringLiteral("公共参数");
        commonParameter.value = QStringLiteral("COMMON");
        commonParameter.partIndex = -1;
        rotatedTextSymbol.parameters.append(commonParameter);
        rotatedTextSymbol.partCount = 2;
        const QString rotatedTextPath = QDir(tempDir.path()).filePath(QStringLiteral("rotated-text.SchLib"));
        ExporterAltiumSymbol rotatedTextExporter;
        QVERIFY(rotatedTextExporter.exportSymbolLibrary(
            {rotatedTextSymbol}, QStringLiteral("rotated-text"), rotatedTextPath, false, false));
        QByteArray rotatedTextData;
        QVERIFY(readCfbStream(rotatedTextPath, QStringLiteral("ROTATED_TEXT/Data"), rotatedTextData));
        QVERIFY(rotatedTextData.contains("Orientation=3"));
        QVERIFY(rotatedTextData.contains("NAME=第一部件参数"));
        QVERIFY(rotatedTextData.contains("NAME=公共参数"));
        QCOMPARE(rotatedTextData.count("NAME=Shared"), 2);
        const int rotatedParameterOffset = rotatedTextData.indexOf("NAME=角度参数");
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
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString outputPath = QDir(tempDir.path()).filePath(QStringLiteral("existing.SchLib"));
        QFile existingFile(outputPath);
        QVERIFY(existingFile.open(QIODevice::WriteOnly));
        const QByteArray originalData = QByteArrayLiteral("existing-library");
        QCOMPARE(existingFile.write(originalData), originalData.size());
        existingFile.close();

        IR::SymbolComponentIR symbol;
        symbol.name = QStringLiteral("NEW_SYMBOL");
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
     *          安装孔转为非电镀 MultiLayer pad，板框保留层和线宽
     */
    void exportersPreserveMultipartAndPolylineStructure() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        IR::SymbolComponentIR symbol;
        symbol.name = QStringLiteral("MULTIPART");
        symbol.designatorPrefix = QStringLiteral("U");
        symbol.partCount = 2;
        IR::SymbolPinIR pin;
        pin.name = QStringLiteral("B");
        pin.designator = QStringLiteral("2");
        pin.length = 2.54;
        pin.partIndex = 1;
        pin.style.decoration = IR::PinDecoration::AnalogInput;
        symbol.pins.append(pin);
        IR::SymbolPinIR commonPin;
        commonPin.name = QStringLiteral("VCC");
        commonPin.designator = QStringLiteral("3");
        commonPin.length = 2.54;
        commonPin.commonToAllParts = true;
        symbol.pins.append(commonPin);
        IR::SymbolPinIR secondPartPin;
        secondPartPin.name = QStringLiteral("A");
        secondPartPin.designator = QStringLiteral("1");
        secondPartPin.length = 2.54;
        secondPartPin.partIndex = 0;
        symbol.pins.append(secondPartPin);
        IR::SymbolPathIR path;
        path.points = {QPointF(0, 0), QPointF(1, 1), QPointF(2, 0)};
        path.partIndex = 1;
        symbol.paths.append(path);
        IR::SymbolBezierIR bezier;
        bezier.controlPoints = {QPointF(-1, 0), QPointF(-0.5, 2), QPointF(0.5, -2), QPointF(1, 0)};
        bezier.partIndex = 1;
        symbol.beziers.append(bezier);
        IR::SymbolRectangleIR roundedRectangle;
        roundedRectangle.x0 = -2.0;
        roundedRectangle.y0 = -1.0;
        roundedRectangle.x1 = 2.0;
        roundedRectangle.y1 = 1.0;
        roundedRectangle.cornerRadiusX = 0.25;
        roundedRectangle.cornerRadiusY = 0.2;
        roundedRectangle.partIndex = 1;
        symbol.rectangles.append(roundedRectangle);
        IR::SymbolRectangleIR commonRectangle;
        commonRectangle.x0 = -3.0;
        commonRectangle.y0 = -2.0;
        commonRectangle.x1 = -2.0;
        commonRectangle.y1 = -1.0;
        commonRectangle.partIndex = -1;
        symbol.rectangles.append(commonRectangle);
        IR::SymbolRectangleIR secondPartRectangle;
        secondPartRectangle.x0 = 4.0;
        secondPartRectangle.y0 = 3.0;
        secondPartRectangle.x1 = 5.0;
        secondPartRectangle.y1 = 4.0;
        secondPartRectangle.partIndex = 0;
        symbol.rectangles.append(secondPartRectangle);
        IR::SymbolPolygonIR firstPartPolygon;
        firstPartPolygon.points = {QPointF(10.0, 10.0), QPointF(11.0, 10.0), QPointF(10.0, 11.0)};
        firstPartPolygon.partIndex = 1;
        symbol.polygons.append(firstPartPolygon);
        IR::SymbolPolygonIR secondPartPolygon;
        secondPartPolygon.points = {QPointF(20.0, 20.0), QPointF(21.0, 20.0), QPointF(20.0, 21.0)};
        secondPartPolygon.partIndex = 0;
        symbol.polygons.append(secondPartPolygon);
        IR::SymbolTextIR commonText;
        commonText.text = QStringLiteral("COMMON_TEXT");
        commonText.partIndex = -1;
        symbol.texts.append(commonText);
        IR::SymbolTextIR partText;
        partText.text = QStringLiteral("PART_TEXT");
        partText.partIndex = 1;
        symbol.texts.append(partText);
        IR::SymbolParameterIR partParameter;
        partParameter.name = QStringLiteral("PartOnly");
        partParameter.value = QStringLiteral("Part 1 value");
        partParameter.partIndex = 1;
        symbol.parameters.append(partParameter);
        IR::SymbolImageIR image;
        image.x0 = -1.0;
        image.y0 = -0.5;
        image.x1 = 1.0;
        image.y1 = 0.5;
        image.fileName = QStringLiteral("multipart.png");
        image.data = QByteArrayLiteral("multipart-image");
        symbol.images.append(image);
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

        const QString schPath = QDir(tempDir.path()).filePath(QStringLiteral("multipart.SchLib"));
        ExporterAltiumSymbol symbolExporter;
        QVERIFY(symbolExporter.exportSymbolLibrary({symbol}, QStringLiteral("multipart"), schPath, false, false));
        QByteArray symbolData;
        QVERIFY(readCfbStream(schPath, QStringLiteral("MULTIPART/Data"), symbolData));
        const int pinOffset = 4 + static_cast<int>(readU32(symbolData, 0) & 0x00FFFFFFU);
        QCOMPARE(readU32(symbolData, pinOffset + 4), quint32(2));
        QCOMPARE(readU16(symbolData, pinOffset + 9), quint16(2));
        QCOMPARE(static_cast<uint8_t>(symbolData.at(pinOffset + 15)), static_cast<uint8_t>(5));
        QList<quint16> orderedPinOwners;
        for (int offset = pinOffset; offset + 13 <= symbolData.size();) {
            const int payloadSize = static_cast<int>(readU32(symbolData, offset) & 0x00FFFFFFU);
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
        const int firstPolygonRecord = symbolData.indexOf("RECORD=7");
        const int secondPolygonRecord = symbolData.indexOf("RECORD=7", firstPolygonRecord + 1);
        QVERIFY(firstPolygonRecord > 0);
        QVERIFY(secondPolygonRecord > firstPolygonRecord);
        QVERIFY(symbolData.mid(firstPolygonRecord, secondPolygonRecord - firstPolygonRecord).contains("OWNERPARTID=2"));
        QVERIFY(symbolData.mid(secondPolygonRecord).contains("OWNERPARTID=1"));
        QVERIFY(symbolData.contains("RECORD=30"));
        const int roundedRectangleRecord = symbolData.indexOf("RECORD=10");
        const int pathRecord = symbolData.indexOf("RECORD=6");
        const int rectangleRecord = symbolData.indexOf("RECORD=14");
        QVERIFY(roundedRectangleRecord > pinOffset);
        QVERIFY(pathRecord > roundedRectangleRecord);
        QVERIFY(rectangleRecord > pathRecord);
        QVERIFY(symbolData.contains("EmbedImage=T"));
        QVERIFY(symbolData.contains("FileName=multipart.png"));
        QVERIFY(symbolData.contains("FileName=multipart_2.png"));
        const int commonTextOffset = symbolData.indexOf("Text=COMMON_TEXT");
        const int commonTextRecordOffset = symbolData.lastIndexOf("|RECORD=4|", commonTextOffset);
        QVERIFY(commonTextOffset > commonTextRecordOffset);
        QVERIFY(symbolData.mid(commonTextRecordOffset, commonTextOffset - commonTextRecordOffset)
                    .contains("OWNERPARTID=-1"));
        const int partTextOffset = symbolData.indexOf("Text=PART_TEXT");
        const int partTextRecordOffset = symbolData.lastIndexOf("|RECORD=4|", partTextOffset);
        QVERIFY(partTextOffset > partTextRecordOffset);
        QVERIFY(symbolData.mid(partTextRecordOffset, partTextOffset - partTextRecordOffset).contains("OWNERPARTID=2"));
        const int partParameterOffset = symbolData.indexOf("NAME=PartOnly");
        const int partParameterRecordOffset = symbolData.lastIndexOf("|RECORD=41|", partParameterOffset);
        QVERIFY(partParameterOffset > partParameterRecordOffset);
        const int nextParameterRecordOffset = symbolData.indexOf("|RECORD=", partParameterRecordOffset + 1);
        QVERIFY(nextParameterRecordOffset > partParameterRecordOffset);
        const QByteArray partParameterRecord =
            symbolData.mid(partParameterRecordOffset, nextParameterRecordOffset - partParameterRecordOffset);
        QVERIFY(partParameterRecord.contains("OWNERPARTID=2"));
        QVERIFY(partParameterRecord.contains("OWNERPARTDISPLAYMODE=1"));

        AltiumSchLibReader multipartReader;
        QVERIFY2(multipartReader.open(schPath), qPrintable(multipartReader.errorString()));
        QCOMPARE(multipartReader.components().size(), 1);
        QCOMPARE(multipartReader.components().first().partCount, 2);
        QVector<AltiumSchLibReader::Record> multipartRecords;
        QVERIFY2(multipartReader.readComponentRecords(0, &multipartRecords), qPrintable(multipartReader.errorString()));
        bool sawCommonPin = false;
        bool sawCommonRectangle = false;
        bool sawCommonText = false;
        bool sawPartPin = false;
        for (const auto& record : multipartRecords) {
            if (record.recordType == 2) {
                QCOMPARE(record.ownerPartDisplayMode, 1);
                sawCommonPin |= record.ownerPartId == -1;
                sawPartPin |= record.ownerPartId == 1 || record.ownerPartId == 2;
            } else if (record.hasParameters && (record.recordType == 14 || record.recordType == 4) &&
                       record.ownerPartId == -1) {
                QCOMPARE(record.ownerPartDisplayMode, 1);
                sawCommonRectangle |= record.recordType == 14;
                sawCommonText |= record.recordType == 4;
            }
        }
        QVERIFY(sawCommonPin);
        QVERIFY(sawCommonRectangle);
        QVERIFY(sawCommonText);
        QVERIFY(sawPartPin);

        QByteArray multipartStorage;
        QVERIFY(readCfbStream(schPath, QStringLiteral("Storage"), multipartStorage));
        QVERIFY(multipartStorage.contains("multipart.png"));
        QVERIFY(multipartStorage.contains("multipart_2.png"));
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

        AltiumSchComponent diagnosticSymbol;
        diagnosticSymbol.name = QStringLiteral("IMAGE_DIAGNOSTICS");
        AltiumSchImage emptyImage;
        emptyImage.embedImage = true;
        emptyImage.fileName = QStringLiteral("empty.png");
        emptyImage.cornerX = 100000;
        emptyImage.cornerY = 100000;
        diagnosticSymbol.images.append(emptyImage);
        AltiumSchImage invalidNameImage;
        invalidNameImage.embedImage = true;
        invalidNameImage.fileName = QStringLiteral("bad|name.png");
        invalidNameImage.data = QByteArrayLiteral("image");
        invalidNameImage.cornerX = 100000;
        invalidNameImage.cornerY = 100000;
        diagnosticSymbol.images.append(invalidNameImage);
        diagnosticSymbol.graphicOrder = {{QStringLiteral("T"), 0, 0}};
        AltiumSchPolygon fallbackPolygon;
        fallbackPolygon.vertices = {QPointF(100000, 100000), QPointF(200000, 100000), QPointF(100000, 200000)};
        diagnosticSymbol.polygons.append(fallbackPolygon);
        AltiumSchLibWriter diagnosticWriter;
        const QString diagnosticPath = QDir(tempDir.path()).filePath(QStringLiteral("image-diagnostics.SchLib"));
        QVERIFY(diagnosticWriter.write({diagnosticSymbol}, diagnosticPath, QStringLiteral("IMAGE_DIAGNOSTICS")));
        QByteArray fallbackData;
        QVERIFY(readCfbStream(diagnosticPath, QStringLiteral("IMAGE_DIAGNOSTICS/Data"), fallbackData));
        QVERIFY(fallbackData.contains("RECORD=7"));
        QVERIFY(fallbackData.contains("FileName=empty.png"));
        QVERIFY(!fallbackData.contains("FileName=empty.png|EmbedImage=T"));
        QVERIFY(diagnosticWriter.diagnostics().contains(
            QStringLiteral("组件 IMAGE_DIAGNOSTICS 图片 0 的嵌入数据为空，已跳过 Storage")));
        QVERIFY(diagnosticWriter.diagnostics().contains(
            QStringLiteral("组件 IMAGE_DIAGNOSTICS 图片 1 的嵌入文件名无效: bad|name.png，已跳过 Storage")));
        AltiumSchImage missingExternalNameImage;
        missingExternalNameImage.cornerX = 100000;
        missingExternalNameImage.cornerY = 100000;
        diagnosticSymbol.images.append(missingExternalNameImage);
        AltiumSchImage invalidWindowsNameImage;
        invalidWindowsNameImage.embedImage = true;
        invalidWindowsNameImage.fileName = QStringLiteral("bad:name.png");
        invalidWindowsNameImage.data = QByteArrayLiteral("image");
        invalidWindowsNameImage.cornerX = 100000;
        invalidWindowsNameImage.cornerY = 100000;
        diagnosticSymbol.images.append(invalidWindowsNameImage);
        AltiumSchImage oversizedNameImage;
        oversizedNameImage.embedImage = true;
        oversizedNameImage.fileName = QString(256, QLatin1Char('a')) + QStringLiteral(".png");
        oversizedNameImage.data = QByteArrayLiteral("image");
        oversizedNameImage.cornerX = 100000;
        oversizedNameImage.cornerY = 100000;
        diagnosticSymbol.images.append(oversizedNameImage);
        const QString oversizedDiagnosticPath = QDir(tempDir.path()).filePath(QStringLiteral("oversized-image.SchLib"));
        QVERIFY(
            diagnosticWriter.write({diagnosticSymbol}, oversizedDiagnosticPath, QStringLiteral("IMAGE_DIAGNOSTICS")));
        QVERIFY(diagnosticWriter.diagnostics().join('\n').contains(QStringLiteral("图片 3 的嵌入文件名无效")));
        QVERIFY(diagnosticWriter.diagnostics().join('\n').contains(QStringLiteral("图片 4 的嵌入文件名超过 255 字节")));
        QVERIFY(diagnosticWriter.diagnostics().join('\n').contains(
            QStringLiteral("组件 IMAGE_DIAGNOSTICS 图片 2 的外部文件名为空，已跳过文件引用")));
        QByteArray diagnosticData;
        QVERIFY(readCfbStream(oversizedDiagnosticPath, QStringLiteral("IMAGE_DIAGNOSTICS/Data"), diagnosticData));
        QVERIFY(!diagnosticData.contains("FileName=bad|name.png"));
        QVERIFY(!diagnosticData.contains("FileName=aaaaaaaa"));

        IR::SymbolComponentIR invalidBezierSymbol;
        invalidBezierSymbol.name = QStringLiteral("INVALID_BEZIER");
        IR::SymbolBezierIR invalidBezier;
        invalidBezier.controlPoints = {QPointF(0.0, 0.0), QPointF(1.0, 1.0)};
        invalidBezierSymbol.beziers.append(invalidBezier);
        ExporterAltiumSymbol invalidBezierExporter;
        const QString invalidBezierPath = QDir(tempDir.path()).filePath(QStringLiteral("invalid-bezier.SchLib"));
        QVERIFY(invalidBezierExporter.exportSymbol(invalidBezierSymbol, invalidBezierPath));
        QVERIFY(invalidBezierExporter.diagnostics().contains(
            QStringLiteral("符号 INVALID_BEZIER Bézier 图元 0 的控制点参数无效（数量为 2），已跳过")));

        IR::SymbolComponentIR invalidWriterInputSymbol;
        invalidWriterInputSymbol.name = QStringLiteral("INVALID|WRITER_INPUT");
        ExporterAltiumSymbol invalidWriterInputExporter;
        const QString invalidWriterInputPath =
            QDir(tempDir.path()).filePath(QStringLiteral("invalid-writer-input.SchLib"));
        QVERIFY(!invalidWriterInputExporter.exportSymbol(invalidWriterInputSymbol, invalidWriterInputPath));
        QVERIFY(invalidWriterInputExporter.diagnostics().contains(
            QStringLiteral("Altium SchLib 组件 INVALID|WRITER_INPUT 的组件名称包含参数分隔符或 NUL，已拒绝写入")));

        ExporterAltiumSymbol invalidWriterLibraryExporter;
        const QString invalidWriterLibraryPath =
            QDir(tempDir.path()).filePath(QStringLiteral("invalid-writer-library.SchLib"));
        QVERIFY(!invalidWriterLibraryExporter.exportSymbolLibrary(
            {invalidWriterInputSymbol}, QStringLiteral("invalid-writer-library"), invalidWriterLibraryPath, false));
        QVERIFY(invalidWriterLibraryExporter.diagnostics().contains(
            QStringLiteral("Altium SchLib 组件 INVALID|WRITER_INPUT 的组件名称包含参数分隔符或 NUL，已拒绝写入")));

        IR::SymbolComponentIR invalidPrimitivePointsSymbol;
        invalidPrimitivePointsSymbol.name = QStringLiteral("INVALID_PRIMITIVE_POINTS");
        IR::SymbolPolylineIR invalidPolyline;
        invalidPolyline.points = {QPointF(0.0, 0.0)};
        invalidPrimitivePointsSymbol.polylines.append(invalidPolyline);
        IR::SymbolBezierIR invalidFiniteBezier;
        invalidFiniteBezier.controlPoints = {QPointF(0.0, 0.0),
                                             QPointF(1.0, 0.0),
                                             QPointF(std::numeric_limits<double>::infinity(), 1.0),
                                             QPointF(2.0, 0.0)};
        invalidPrimitivePointsSymbol.beziers.append(invalidFiniteBezier);
        ExporterAltiumSymbol invalidPrimitivePointsExporter;
        const QString invalidPrimitivePointsFile =
            QDir(tempDir.path()).filePath(QStringLiteral("invalid-primitive-points.SchLib"));
        QVERIFY(invalidPrimitivePointsExporter.exportSymbol(invalidPrimitivePointsSymbol, invalidPrimitivePointsFile));
        QVERIFY(invalidPrimitivePointsExporter.diagnostics().contains(
            QStringLiteral("符号 INVALID_PRIMITIVE_POINTS 折线图元 0 的点列无效，已跳过")));
        QVERIFY(invalidPrimitivePointsExporter.diagnostics().contains(
            QStringLiteral("符号 INVALID_PRIMITIVE_POINTS Bézier 图元 0 的控制点参数无效（数量为 4），已跳过")));

        IR::SymbolComponentIR invalidGeometrySymbol;
        invalidGeometrySymbol.name = QStringLiteral("INVALID_GEOMETRY");
        IR::SymbolCircleIR invalidCircle;
        invalidCircle.radius = -1.0;
        invalidGeometrySymbol.circles.append(invalidCircle);
        IR::SymbolCircleIR invalidStrokeCircle;
        invalidStrokeCircle.strokeWidth = std::numeric_limits<double>::quiet_NaN();
        invalidGeometrySymbol.circles.append(invalidStrokeCircle);
        IR::SymbolArcIR degenerateArc;
        degenerateArc.startPoint = QPointF(0.0, 0.0);
        degenerateArc.midPoint = QPointF(1.0, 1.0);
        degenerateArc.endPoint = QPointF(2.0, 2.0);
        invalidGeometrySymbol.arcs.append(degenerateArc);
        ExporterAltiumSymbol invalidGeometryExporter;
        const QString invalidGeometryPath = QDir(tempDir.path()).filePath(QStringLiteral("invalid-geometry.SchLib"));
        QVERIFY(invalidGeometryExporter.exportSymbol(invalidGeometrySymbol, invalidGeometryPath));
        QVERIFY(invalidGeometryExporter.diagnostics().contains(
            QStringLiteral("符号 INVALID_GEOMETRY 圆图元 0 的半径无效，已跳过")));
        QVERIFY(invalidGeometryExporter.diagnostics().contains(
            QStringLiteral("符号 INVALID_GEOMETRY 圆图元 1 的中心或线宽无效，已跳过")));
        QVERIFY(invalidGeometryExporter.diagnostics().contains(
            QStringLiteral("符号 INVALID_GEOMETRY 圆弧图元 0 三点退化，已使用安全回退圆心")));

        IR::SymbolComponentIR invalidArcGeometrySymbol;
        invalidArcGeometrySymbol.name = QStringLiteral("INVALID_ARC_GEOMETRY");
        IR::SymbolPieIR invalidPie;
        invalidPie.radius = -1.0;
        invalidPie.startAngle = std::numeric_limits<double>::quiet_NaN();
        invalidArcGeometrySymbol.pies.append(invalidPie);
        IR::SymbolEllipticalArcIR invalidEllipticalArc;
        invalidEllipticalArc.radiusX = 0.0;
        invalidEllipticalArc.radiusY = 1.0;
        invalidArcGeometrySymbol.ellipticalArcs.append(invalidEllipticalArc);
        ExporterAltiumSymbol invalidArcGeometryExporter;
        const QString invalidArcGeometryPath =
            QDir(tempDir.path()).filePath(QStringLiteral("invalid-arc-geometry.SchLib"));
        QVERIFY(invalidArcGeometryExporter.exportSymbol(invalidArcGeometrySymbol, invalidArcGeometryPath));
        QVERIFY(invalidArcGeometryExporter.diagnostics().contains(
            QStringLiteral("符号 INVALID_ARC_GEOMETRY 扇形图元 0 的几何参数无效，已跳过")));
        QVERIFY(invalidArcGeometryExporter.diagnostics().contains(
            QStringLiteral("符号 INVALID_ARC_GEOMETRY 椭圆弧图元 0 的几何参数无效，已跳过")));

        IR::SymbolComponentIR invalidTextSymbol;
        invalidTextSymbol.name = QStringLiteral("INVALID_TEXT_DATA");
        IR::SymbolTextIR invalidText;
        invalidText.text = QStringLiteral("label");
        invalidText.position = QPointF(std::numeric_limits<double>::quiet_NaN(), 0.0);
        invalidTextSymbol.texts.append(invalidText);
        IR::SymbolTextFrameIR invalidTextFrame;
        invalidTextFrame.textMargin = -1.0;
        invalidTextSymbol.textFrames.append(invalidTextFrame);
        IR::SymbolParameterIR invalidParameter;
        invalidParameter.name = QStringLiteral("Custom");
        invalidParameter.position = QPointF(0.0, std::numeric_limits<double>::infinity());
        invalidTextSymbol.parameters.append(invalidParameter);
        ExporterAltiumSymbol invalidTextExporter;
        const QString invalidTextPath = QDir(tempDir.path()).filePath(QStringLiteral("invalid-text-data.SchLib"));
        QVERIFY(invalidTextExporter.exportSymbol(invalidTextSymbol, invalidTextPath));
        QVERIFY(invalidTextExporter.diagnostics().contains(
            QStringLiteral("符号 INVALID_TEXT_DATA 文本图元 0 的内容或几何参数无效，已跳过")));
        QVERIFY(invalidTextExporter.diagnostics().contains(
            QStringLiteral("符号 INVALID_TEXT_DATA 文本框图元 0 的几何参数无效，已跳过")));
        QVERIFY(invalidTextExporter.diagnostics().contains(
            QStringLiteral("符号 INVALID_TEXT_DATA 参数 Custom 的位置、旋转角度或名称无效，已跳过")));

        IR::SymbolComponentIR invalidTextAnchorSymbol;
        invalidTextAnchorSymbol.name = QStringLiteral("INVALID_TEXT_ANCHOR");
        IR::SymbolTextIR invalidTextAnchor;
        invalidTextAnchor.text = QStringLiteral("Anchor");
        invalidTextAnchor.anchor = QStringLiteral("unsupported");
        invalidTextAnchorSymbol.texts.append(invalidTextAnchor);
        IR::SymbolPinIR invalidPinAnchor;
        invalidPinAnchor.name = QStringLiteral("Pin");
        invalidPinAnchor.designator = QStringLiteral("7");
        invalidPinAnchor.position = QPointF(0.0, 0.0);
        invalidPinAnchor.length = 2.54;
        invalidPinAnchor.hasNamePosition = true;
        invalidPinAnchor.namePosition = QPointF(0.0, 0.0);
        invalidPinAnchor.nameAnchor = QStringLiteral("unsupported");
        invalidTextAnchorSymbol.pins.append(invalidPinAnchor);
        ExporterAltiumSymbol invalidTextAnchorExporter;
        const QString invalidTextAnchorPath =
            QDir(tempDir.path()).filePath(QStringLiteral("invalid-text-anchor.SchLib"));
        QVERIFY(invalidTextAnchorExporter.exportSymbol(invalidTextAnchorSymbol, invalidTextAnchorPath));
        QVERIFY2(invalidTextAnchorExporter.diagnostics().join('\n').contains(
                     QStringLiteral("符号 INVALID_TEXT_ANCHOR 文本图元 0 的对齐锚点无效")),
                 qPrintable(invalidTextAnchorExporter.diagnostics().join('\n')));
        QVERIFY(invalidTextAnchorExporter.diagnostics().join('\n').contains(
            QStringLiteral("符号 INVALID_TEXT_ANCHOR 引脚 7 名称文本 的对齐锚点无效")));

        IR::SymbolParameterIR invalidParameterFont;
        invalidParameterFont.name = QStringLiteral("FontInvalid");
        invalidParameterFont.fontSizeMm = std::numeric_limits<double>::quiet_NaN();
        invalidTextSymbol.parameters.append(invalidParameterFont);
        ExporterAltiumSymbol invalidParameterFontExporter;
        const QString invalidParameterFontPath =
            QDir(tempDir.path()).filePath(QStringLiteral("invalid-parameter-font.SchLib"));
        QVERIFY(invalidParameterFontExporter.exportSymbol(invalidTextSymbol, invalidParameterFontPath));
        QVERIFY(invalidParameterFontExporter.diagnostics().contains(
            QStringLiteral("符号 INVALID_TEXT_DATA 参数 FontInvalid 的字体大小无效，已跳过")));

        IR::SymbolComponentIR invalidCommonPartIndexSymbol;
        invalidCommonPartIndexSymbol.name = QStringLiteral("INVALID_COMMON_PART_INDEX");
        IR::SymbolRectangleIR invalidCommonPartIndexRectangle;
        invalidCommonPartIndexRectangle.partIndex = -2;
        invalidCommonPartIndexRectangle.x0 = 0.0;
        invalidCommonPartIndexRectangle.y0 = 0.0;
        invalidCommonPartIndexRectangle.x1 = 1.0;
        invalidCommonPartIndexRectangle.y1 = 1.0;
        invalidCommonPartIndexSymbol.rectangles.append(invalidCommonPartIndexRectangle);
        ExporterAltiumSymbol invalidCommonPartIndexExporter;
        const QString invalidCommonPartIndexPath =
            QDir(tempDir.path()).filePath(QStringLiteral("invalid-common-part-index.SchLib"));
        QVERIFY(!invalidCommonPartIndexExporter.exportSymbol(invalidCommonPartIndexSymbol, invalidCommonPartIndexPath));
        QVERIFY(invalidCommonPartIndexExporter.diagnostics().contains(QStringLiteral(
            "Altium SchLib 组件 INVALID_COMMON_PART_INDEX 的矩形图元 OWNERPARTID=-2 超出部件范围，已拒绝写入")));

        IR::SymbolComponentIR invalidBoundsSymbol;
        invalidBoundsSymbol.name = QStringLiteral("INVALID_BOUNDS_DATA");
        IR::SymbolRectangleIR invalidRectangle;
        invalidRectangle.x1 = std::numeric_limits<double>::quiet_NaN();
        invalidBoundsSymbol.rectangles.append(invalidRectangle);
        IR::SymbolTextFrameIR zeroTextFrame;
        zeroTextFrame.x1 = 0.0;
        zeroTextFrame.y1 = 0.0;
        invalidBoundsSymbol.textFrames.append(zeroTextFrame);
        ExporterAltiumSymbol invalidBoundsExporter;
        const QString invalidBoundsPath = QDir(tempDir.path()).filePath(QStringLiteral("invalid-bounds-data.SchLib"));
        QVERIFY(invalidBoundsExporter.exportSymbol(invalidBoundsSymbol, invalidBoundsPath));
        QVERIFY(invalidBoundsExporter.diagnostics().contains(
            QStringLiteral("符号 INVALID_BOUNDS_DATA 矩形图元 0 的边界无效，已跳过")));
        QVERIFY(invalidBoundsExporter.diagnostics().contains(
            QStringLiteral("符号 INVALID_BOUNDS_DATA 文本框图元 0 的几何参数无效，已跳过")));

        IR::SymbolComponentIR invalidImageSymbol;
        invalidImageSymbol.name = QStringLiteral("INVALID_IMAGE_DATA");
        IR::SymbolImageIR invalidImage;
        invalidImage.x1 = std::numeric_limits<double>::quiet_NaN();
        invalidImage.fileName = QStringLiteral("image.png");
        invalidImageSymbol.images.append(invalidImage);
        ExporterAltiumSymbol invalidImageExporter;
        const QString invalidImagePath = QDir(tempDir.path()).filePath(QStringLiteral("invalid-image-data.SchLib"));
        QVERIFY(invalidImageExporter.exportSymbol(invalidImageSymbol, invalidImagePath));
        QVERIFY(invalidImageExporter.diagnostics().contains(
            QStringLiteral("符号 INVALID_IMAGE_DATA 图片图元 0 的边界、线宽或资源无效，已跳过")));

        IR::SymbolComponentIR malformedDataUrlImageSymbol;
        malformedDataUrlImageSymbol.name = QStringLiteral("MALFORMED_DATA_URL_IMAGE");
        IR::SymbolImageIR malformedDataUrlImage;
        malformedDataUrlImage.x1 = 1.0;
        malformedDataUrlImage.y1 = 1.0;
        malformedDataUrlImage.fileName = QStringLiteral("data:image/png;base64,not-decoded");
        malformedDataUrlImageSymbol.images.append(malformedDataUrlImage);
        ExporterAltiumSymbol malformedDataUrlImageExporter;
        const QString malformedDataUrlImagePath =
            QDir(tempDir.path()).filePath(QStringLiteral("malformed-data-url-image.SchLib"));
        QVERIFY(malformedDataUrlImageExporter.exportSymbol(malformedDataUrlImageSymbol, malformedDataUrlImagePath));
        QVERIFY(malformedDataUrlImageExporter.diagnostics().contains(
            QStringLiteral("符号 MALFORMED_DATA_URL_IMAGE 图片图元 0 包含未解析的 data URL，已跳过")));
        QByteArray malformedDataUrlImageData;
        QVERIFY(readCfbStream(
            malformedDataUrlImagePath, QStringLiteral("MALFORMED_DATA_URL_IMAGE/Data"), malformedDataUrlImageData));
        QVERIFY(!malformedDataUrlImageData.contains("RECORD=30"));

        IR::SymbolComponentIR invalidArcSymbol;
        invalidArcSymbol.name = QStringLiteral("INVALID_ARC_DATA");
        IR::SymbolArcIR invalidArc;
        invalidArc.startPoint = QPointF(std::numeric_limits<double>::quiet_NaN(), 0.0);
        invalidArcSymbol.arcs.append(invalidArc);
        IR::SymbolIeeeIR invalidIeee;
        invalidIeee.position = QPointF(0.0, std::numeric_limits<double>::infinity());
        invalidArcSymbol.ieeeSymbols.append(invalidIeee);
        ExporterAltiumSymbol invalidArcExporter;
        const QString invalidArcPath = QDir(tempDir.path()).filePath(QStringLiteral("invalid-arc-data.SchLib"));
        QVERIFY(invalidArcExporter.exportSymbol(invalidArcSymbol, invalidArcPath));
        QVERIFY(invalidArcExporter.diagnostics().contains(
            QStringLiteral("符号 INVALID_ARC_DATA 圆弧图元 0 的点列或线宽无效，已跳过")));
        QVERIFY(invalidArcExporter.diagnostics().contains(
            QStringLiteral("符号 INVALID_ARC_DATA IEEE 图形 0 的位置无效，已跳过")));

        IR::SymbolComponentIR zeroRadiusArcSymbol;
        zeroRadiusArcSymbol.name = QStringLiteral("ZERO_RADIUS_ARC");
        IR::SymbolArcIR zeroRadiusArc;
        zeroRadiusArc.startPoint = QPointF(0.0, 0.0);
        zeroRadiusArc.midPoint = QPointF(0.0, 0.0);
        zeroRadiusArc.endPoint = QPointF(0.0, 0.0);
        zeroRadiusArc.strokeWidth = 0.1;
        zeroRadiusArcSymbol.arcs.append(zeroRadiusArc);
        ExporterAltiumSymbol zeroRadiusArcExporter;
        const QString zeroRadiusArcPath = QDir(tempDir.path()).filePath(QStringLiteral("zero-radius-arc.SchLib"));
        QVERIFY(zeroRadiusArcExporter.exportSymbol(zeroRadiusArcSymbol, zeroRadiusArcPath));
        QVERIFY(zeroRadiusArcExporter.diagnostics().contains(
            QStringLiteral("符号 ZERO_RADIUS_ARC 圆弧图元 0 半径量化后无效，已跳过")));

        IR::SymbolComponentIR invalidPathSymbol;
        invalidPathSymbol.name = QStringLiteral("INVALID_PATH_SEGMENT");
        IR::SymbolPathIR invalidPath;
        IR::SymbolPathSegmentIR invalidEllipseArc;
        invalidEllipseArc.type = IR::SymbolPathSegmentIR::Type::EllipticalArc;
        invalidEllipseArc.arcCenter = QPointF(0.0, 0.0);
        invalidEllipseArc.radiusX = 0.0;
        invalidEllipseArc.radiusY = 1.0;
        invalidEllipseArc.arcStartAngle = 0.0;
        invalidEllipseArc.arcEndAngle = 90.0;
        invalidPath.segments.append(invalidEllipseArc);
        invalidPathSymbol.paths.append(invalidPath);
        ExporterAltiumSymbol invalidPathExporter;
        const QString invalidPathFile = QDir(tempDir.path()).filePath(QStringLiteral("invalid-path-segment.SchLib"));
        QVERIFY(invalidPathExporter.exportSymbol(invalidPathSymbol, invalidPathFile));
        QVERIFY(invalidPathExporter.diagnostics().contains(
            QStringLiteral("符号 INVALID_PATH_SEGMENT 路径图元 0 的段 0 参数无效，已跳过")));

        IR::SymbolComponentIR invalidPathPointsSymbol;
        invalidPathPointsSymbol.name = QStringLiteral("INVALID_PATH_POINTS");
        IR::SymbolPathIR invalidFilledPath;
        invalidFilledPath.isFilled = true;
        invalidFilledPath.points = {QPointF(0.0, 0.0), QPointF(std::numeric_limits<double>::quiet_NaN(), 1.0)};
        invalidPathPointsSymbol.paths.append(invalidFilledPath);
        ExporterAltiumSymbol invalidPathPointsExporter;
        const QString invalidPathPointsFile =
            QDir(tempDir.path()).filePath(QStringLiteral("invalid-path-points.SchLib"));
        QVERIFY(invalidPathPointsExporter.exportSymbol(invalidPathPointsSymbol, invalidPathPointsFile));
        QVERIFY(invalidPathPointsExporter.diagnostics().contains(
            QStringLiteral("符号 INVALID_PATH_POINTS 路径图元 0 的点列无效，已跳过")));

        IR::SymbolComponentIR nativeArcSymbol;
        nativeArcSymbol.name = QStringLiteral("NATIVE_ARC");
        IR::SymbolPathIR nativeArcPath;
        nativeArcPath.strokeWidth = 0.1;
        IR::SymbolPathSegmentIR nativeArcSegment;
        nativeArcSegment.type = IR::SymbolPathSegmentIR::Type::CircularArc;
        nativeArcSegment.start = QPointF(0.0, 0.0);
        nativeArcSegment.arcMid = QPointF(1.0, 1.0);
        nativeArcSegment.end = QPointF(2.0, 0.0);
        nativeArcPath.segments.append(nativeArcSegment);
        nativeArcSymbol.paths.append(nativeArcPath);
        const QString nativeArcPathFile = QDir(tempDir.path()).filePath(QStringLiteral("native-arc.SchLib"));
        ExporterAltiumSymbol nativeArcExporter;
        QVERIFY(nativeArcExporter.exportSymbolLibrary(
            {nativeArcSymbol}, QStringLiteral("native-arc"), nativeArcPathFile, false, false));
        QByteArray nativeArcData;
        QVERIFY(readCfbStream(nativeArcPathFile, QStringLiteral("NATIVE_ARC/Data"), nativeArcData));
        QVERIFY(nativeArcData.contains("RECORD=12"));

        IR::SymbolComponentIR zeroRadiusNativeArcSymbol;
        zeroRadiusNativeArcSymbol.name = QStringLiteral("ZERO_RADIUS_NATIVE_ARC");
        IR::SymbolPathIR zeroRadiusNativeArcPath;
        zeroRadiusNativeArcPath.strokeWidth = 0.1;
        IR::SymbolPathSegmentIR zeroRadiusNativeArcSegment;
        zeroRadiusNativeArcSegment.type = IR::SymbolPathSegmentIR::Type::CircularArc;
        zeroRadiusNativeArcSegment.start = QPointF(0.0, 0.0);
        zeroRadiusNativeArcSegment.arcMid = QPointF(0.0, 0.0);
        zeroRadiusNativeArcSegment.end = QPointF(0.0, 0.0);
        zeroRadiusNativeArcPath.segments.append(zeroRadiusNativeArcSegment);
        zeroRadiusNativeArcSymbol.paths.append(zeroRadiusNativeArcPath);
        const QString zeroRadiusNativeArcFile =
            QDir(tempDir.path()).filePath(QStringLiteral("zero-radius-native-arc.SchLib"));
        ExporterAltiumSymbol zeroRadiusNativeArcExporter;
        QVERIFY(zeroRadiusNativeArcExporter.exportSymbol(zeroRadiusNativeArcSymbol, zeroRadiusNativeArcFile));
        QVERIFY(zeroRadiusNativeArcExporter.diagnostics().contains(
            QStringLiteral("符号 ZERO_RADIUS_NATIVE_ARC 路径图元 0 的段 0 圆弧半径量化后无效，已跳过")));

        IR::SymbolComponentIR quantizedRadiusSymbol;
        quantizedRadiusSymbol.name = QStringLiteral("QUANTIZED_RADIUS_GRAPHICS");
        IR::SymbolCircleIR tinyCircle;
        tinyCircle.center = QPointF(0.0, 0.0);
        tinyCircle.radius = 0.000001;
        tinyCircle.strokeWidth = 0.1;
        quantizedRadiusSymbol.circles.append(tinyCircle);
        IR::SymbolEllipseIR tinyEllipse;
        tinyEllipse.center = QPointF(1.0, 0.0);
        tinyEllipse.radiusX = 0.000001;
        tinyEllipse.radiusY = 1.0;
        tinyEllipse.strokeWidth = 0.1;
        quantizedRadiusSymbol.ellipses.append(tinyEllipse);
        IR::SymbolPieIR tinyPie;
        tinyPie.center = QPointF(2.0, 0.0);
        tinyPie.radius = 0.000001;
        tinyPie.strokeWidth = 0.1;
        quantizedRadiusSymbol.pies.append(tinyPie);
        IR::SymbolEllipticalArcIR tinyEllipticalArc;
        tinyEllipticalArc.center = QPointF(3.0, 0.0);
        tinyEllipticalArc.radiusX = 0.000001;
        tinyEllipticalArc.radiusY = 1.0;
        tinyEllipticalArc.strokeWidth = 0.1;
        quantizedRadiusSymbol.ellipticalArcs.append(tinyEllipticalArc);
        IR::SymbolPathIR tinyEllipticalArcPath;
        tinyEllipticalArcPath.strokeWidth = 0.1;
        IR::SymbolPathSegmentIR tinyEllipticalArcSegment;
        tinyEllipticalArcSegment.type = IR::SymbolPathSegmentIR::Type::EllipticalArc;
        tinyEllipticalArcSegment.arcCenter = QPointF(4.0, 0.0);
        tinyEllipticalArcSegment.radiusX = 0.000001;
        tinyEllipticalArcSegment.radiusY = 1.0;
        tinyEllipticalArcPath.segments.append(tinyEllipticalArcSegment);
        quantizedRadiusSymbol.paths.append(tinyEllipticalArcPath);
        ExporterAltiumSymbol quantizedRadiusExporter;
        const QString quantizedRadiusFile =
            QDir(tempDir.path()).filePath(QStringLiteral("quantized-radius-graphics.SchLib"));
        QVERIFY(quantizedRadiusExporter.exportSymbol(quantizedRadiusSymbol, quantizedRadiusFile));
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

        IR::SymbolComponentIR nativeQuadraticSymbol;
        nativeQuadraticSymbol.name = QStringLiteral("NATIVE_QUADRATIC");
        IR::SymbolPathIR nativeQuadraticPath;
        nativeQuadraticPath.strokeWidth = 0.1;
        IR::SymbolPathSegmentIR nativeQuadraticSegment;
        nativeQuadraticSegment.type = IR::SymbolPathSegmentIR::Type::QuadraticBezier;
        nativeQuadraticSegment.start = QPointF(0.0, 0.0);
        nativeQuadraticSegment.control1 = QPointF(1.0, 2.0);
        nativeQuadraticSegment.end = QPointF(2.0, 0.0);
        nativeQuadraticPath.segments.append(nativeQuadraticSegment);
        IR::SymbolPathSegmentIR nativeCubicSegment;
        nativeCubicSegment.type = IR::SymbolPathSegmentIR::Type::CubicBezier;
        nativeCubicSegment.start = QPointF(2.0, 0.0);
        nativeCubicSegment.control1 = QPointF(3.0, 1.0);
        nativeCubicSegment.control2 = QPointF(4.0, 1.0);
        nativeCubicSegment.end = QPointF(5.0, 0.0);
        nativeQuadraticPath.segments.append(nativeCubicSegment);
        IR::SymbolPathSegmentIR nativeCircularArcSegment;
        nativeCircularArcSegment.type = IR::SymbolPathSegmentIR::Type::CircularArc;
        nativeCircularArcSegment.start = QPointF(5.0, 0.0);
        nativeCircularArcSegment.arcMid = QPointF(6.0, 1.0);
        nativeCircularArcSegment.end = QPointF(7.0, 0.0);
        nativeQuadraticPath.segments.append(nativeCircularArcSegment);
        nativeQuadraticSymbol.paths.append(nativeQuadraticPath);
        const QString nativeQuadraticPathFile =
            QDir(tempDir.path()).filePath(QStringLiteral("native-quadratic.SchLib"));
        ExporterAltiumSymbol nativeQuadraticExporter;
        QVERIFY(nativeQuadraticExporter.exportSymbolLibrary(
            {nativeQuadraticSymbol}, QStringLiteral("native-quadratic"), nativeQuadraticPathFile, false, false));
        QByteArray nativeQuadraticData;
        QVERIFY(readCfbStream(nativeQuadraticPathFile, QStringLiteral("NATIVE_QUADRATIC/Data"), nativeQuadraticData));
        QCOMPARE(nativeQuadraticData.count("RECORD=5"), 2);
        QVERIFY(nativeQuadraticData.contains("RECORD=12"));
        QVERIFY(nativeQuadraticData.contains("LocationCount=4"));

        IR::SymbolComponentIR nativeEllipticalArcSymbol;
        nativeEllipticalArcSymbol.name = QStringLiteral("NATIVE_ELLIPTICAL_ARC");
        IR::SymbolPathIR nativeEllipticalArcPath;
        nativeEllipticalArcPath.strokeWidth = 0.1;
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
        ExporterAltiumSymbol nativeEllipticalArcExporter;
        QVERIFY(nativeEllipticalArcExporter.exportSymbolLibrary({nativeEllipticalArcSymbol},
                                                                QStringLiteral("native-elliptical-arc"),
                                                                nativeEllipticalArcFile,
                                                                false,
                                                                false));
        QByteArray nativeEllipticalArcData;
        QVERIFY(readCfbStream(
            nativeEllipticalArcFile, QStringLiteral("NATIVE_ELLIPTICAL_ARC/Data"), nativeEllipticalArcData));
        QVERIFY(nativeEllipticalArcData.contains("RECORD=11"));
        QVERIFY(nativeEllipticalArcData.contains("SecondaryRadius="));

        IR::FootprintComponentIR footprint;
        footprint.name = QStringLiteral("SEGMENTS");
        IR::FootprintTrackIR polyline;
        polyline.points = {QPointF(0, 0), QPointF(1, 0), QPointF(1, 1)};
        polyline.width = 0.2;
        polyline.layer = IR::LayerType::TopSilk;
        footprint.tracks.append(polyline);
        IR::FootprintHoleIR hole;
        hole.center = QPointF(0.5, 0.5);
        hole.radius = 0.25;
        footprint.holes.append(hole);
        IR::FootprintOutlineIR outline;
        outline.points = {QPointF(-1, -1), QPointF(2, -1)};
        outline.strokeWidth = 0.1;
        footprint.outlines.append(outline);

        const QString pcbPath = QDir(tempDir.path()).filePath(QStringLiteral("segments.PcbLib"));
        ExporterAltiumFootprint footprintExporter;
        QVERIFY(footprintExporter.exportFootprintLibrary({footprint}, QStringLiteral("segments"), pcbPath));
        QByteArray footprintHeader;
        QVERIFY(readCfbStream(pcbPath, QStringLiteral("SEGMENTS/Header"), footprintHeader));
        QCOMPARE(readU32(footprintHeader, 0), quint32(4));  // 2 track segments + 1 hole pad + 1 outline segment
    }

    /**
     * @brief 验证 Altium 3D 元件体保留 STEP 原点补偿。
     */
    void altiumFootprintPreservesStepOffsetInComponentBody() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        IR::FootprintComponentIR footprint;
        footprint.name = QStringLiteral("STEP_OFFSET");
        IR::FootprintPadIR pad;
        pad.number = QStringLiteral("1");
        pad.position = QPointF(0.0, 0.0);
        pad.size = QSizeF(1.0, 1.0);
        footprint.pads.append(pad);

        IR::Model3DIR model;
        model.setName(QStringLiteral("offset.step"));
        model.setStepData(QByteArrayLiteral("ISO-10303-21;"));
        model.setTranslation(IR::Model3DVec3(1.0, 2.0, 3.0));
        model.setStepOffsetMm(IR::Model3DVec3(4.0, 5.0, 6.0));
        footprint.models3d.append(model);
        IR::Model3DIR secondModel;
        secondModel.setName(QStringLiteral("second.step"));
        secondModel.setStepData(QByteArrayLiteral("ISO-10303-21;SECOND"));
        secondModel.setTranslation(IR::Model3DVec3(10.0, 11.0, 12.0));
        secondModel.setStepOffsetMm(IR::Model3DVec3(1.0, 2.0, 3.0));
        footprint.models3d.append(secondModel);

        const QString pcbPath = QDir(tempDir.path()).filePath(QStringLiteral("step-offset.PcbLib"));
        ExporterAltiumFootprint exporter;
        QVERIFY(exporter.exportFootprintLibrary({footprint}, QStringLiteral("step-offset"), pcbPath));

        AltiumPcbLibReader reader;
        QVERIFY2(reader.open(pcbPath), qPrintable(reader.errorString()));
        QVector<AltiumPcbLibReader::PrimitiveRecord> objects;
        QVERIFY2(reader.readFootprintObjects(QStringLiteral("STEP_OFFSET"), &objects),
                 qPrintable(reader.errorString()));

        QSet<QString> bodyIds;
        for (const auto& object : objects) {
            if (object.objectId != AltiumConstants::PCB_OBJECT_COMPONENT_BODY)
                continue;

            QVERIFY(object.hasComponentBodyFields);
            const auto& params = object.componentBody.parameters;
            const auto expectedMil = [](double millimeters) {
                return QString::number(AltiumCoord::mmToRaw(millimeters) / 10000.0, 'f', 4) + QStringLiteral("mil");
            };
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
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        AltiumSchComponent symbol;
        symbol.name = QStringLiteral("INCOMPLETE_ORDER");
        AltiumSchRectangle first;
        first.sourceGraphicIndex = 0;
        first.sourcePartIndex = 0;
        first.locationX = 100000;
        first.cornerX = 200000;
        first.cornerY = 100000;
        symbol.rectangles.append(first);
        AltiumSchRectangle second;
        second.sourceGraphicIndex = 1;
        second.sourcePartIndex = 0;
        second.locationX = 300000;
        second.cornerX = 400000;
        second.cornerY = 100000;
        symbol.rectangles.append(second);
        symbol.graphicOrder = {{QStringLiteral("R"), 0, 0}};

        AltiumSchLibWriter writer;
        const QString path = QDir(tempDir.path()).filePath(QStringLiteral("incomplete-order.SchLib"));
        QVERIFY(writer.write({symbol}, path, QStringLiteral("incomplete-order")));
        QVERIFY(writer.diagnostics().contains(
            QStringLiteral("符号 INCOMPLETE_ORDER 的 graphicOrder 不完整或包含无效引用，已回退到默认图元顺序")));

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
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        AltiumSchComponent symbol;
        symbol.name = QStringLiteral("DUPLICATE_PATH_SEGMENT");
        for (int i = 0; i < 2; ++i) {
            AltiumSchPath path;
            path.vertices = {QPointF(i * 100000, 0), QPointF(i * 100000 + 50000, 50000)};
            path.sourceGraphicType = QStringLiteral("PT");
            path.sourceGraphicIndex = 0;
            path.sourceSegmentIndex = 0;
            symbol.paths.append(path);
        }
        symbol.graphicOrder = {{QStringLiteral("PT"), 0, 0}};

        AltiumSchLibWriter writer;
        const QString path = QDir(tempDir.path()).filePath(QStringLiteral("duplicate-path-segment.SchLib"));
        QVERIFY(writer.write({symbol}, path, QStringLiteral("duplicate-path-segment")));
        QVERIFY(writer.diagnostics().contains(
            QStringLiteral("符号 DUPLICATE_PATH_SEGMENT 的 graphicOrder 不完整或包含无效引用，已回退到默认图元顺序")));

        QByteArray data;
        QVERIFY(readCfbStream(path, QStringLiteral("DUPLICATE_PATH_SEGMENT/Data"), data));
        QCOMPARE(data.count(QByteArrayLiteral("RECORD=6")), 2);
    }

    /**
     * @brief 验证有序图元模式下未参与来源顺序的图片只写出一次。
     */
    void orderedGraphicsDoNotDuplicateUnindexedImages() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        AltiumSchComponent symbol;
        symbol.name = QStringLiteral("ORDERED_IMAGE");
        AltiumSchRectangle rectangle;
        rectangle.sourceGraphicIndex = 0;
        rectangle.sourcePartIndex = 0;
        rectangle.cornerX = 100000;
        rectangle.cornerY = 100000;
        symbol.rectangles.append(rectangle);
        AltiumSchImage image;
        image.fileName = QStringLiteral("unindexed.png");
        image.cornerX = 100000;
        image.cornerY = 100000;
        image.sourceGraphicIndex = -1;
        image.sourcePartIndex = 0;
        symbol.images.append(image);
        symbol.graphicOrder = {{QStringLiteral("R"), 0, 0}};

        AltiumSchLibWriter writer;
        const QString path = QDir(tempDir.path()).filePath(QStringLiteral("ordered-image.SchLib"));
        QVERIFY(writer.write({symbol}, path, QStringLiteral("ordered-image")));

        QByteArray data;
        QVERIFY(readCfbStream(path, QStringLiteral("ORDERED_IMAGE/Data"), data));
        QCOMPARE(data.count(QByteArrayLiteral("RECORD=14")), 1);
        QCOMPARE(data.count(QByteArrayLiteral("RECORD=30")), 1);
    }

    /**
     * @brief 验证普通矩形与圆角矩形索引冲突时回退并保留全部图元。
     */
    void conflictingRectangleOrderFallsBackWithoutDroppingGraphics() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        AltiumSchComponent symbol;
        symbol.name = QStringLiteral("CONFLICTING_RECTANGLE_ORDER");
        AltiumSchRectangle rectangle;
        rectangle.sourceGraphicIndex = 0;
        rectangle.sourcePartIndex = 0;
        rectangle.cornerX = 100000;
        rectangle.cornerY = 100000;
        symbol.rectangles.append(rectangle);
        AltiumSchRoundRectangle roundRectangle;
        roundRectangle.sourceGraphicIndex = 0;
        roundRectangle.sourcePartIndex = 0;
        roundRectangle.cornerX = 200000;
        roundRectangle.cornerY = 200000;
        symbol.roundRectangles.append(roundRectangle);
        symbol.graphicOrder = {{QStringLiteral("R"), 0, 0}};

        AltiumSchLibWriter writer;
        const QString path = QDir(tempDir.path()).filePath(QStringLiteral("conflicting-rectangle-order.SchLib"));
        QVERIFY(writer.write({symbol}, path, QStringLiteral("conflicting-rectangle-order")));
        QVERIFY(writer.diagnostics().contains(QStringLiteral(
            "符号 CONFLICTING_RECTANGLE_ORDER 的 graphicOrder 不完整或包含无效引用，已回退到默认图元顺序")));

        QByteArray data;
        QVERIFY(readCfbStream(path, QStringLiteral("CONFLICTING_RECTANGLE_ORDER/Data"), data));
        QCOMPARE(data.count(QByteArrayLiteral("RECORD=14")), 1);
        QCOMPARE(data.count(QByteArrayLiteral("RECORD=10")), 1);
    }

    /**
     * @brief 验证同类图元来源索引冲突时不会静默丢失记录。
     */
    void conflictingPolygonOrderFallsBackWithoutDroppingGraphics() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        AltiumSchComponent symbol;
        symbol.name = QStringLiteral("CONFLICTING_POLYGON_ORDER");
        for (const int offset : {0, 100000}) {
            AltiumSchPolygon polygon;
            polygon.sourceGraphicIndex = 0;
            polygon.sourcePartIndex = 0;
            polygon.vertices = {QPointF(offset, 0), QPointF(offset + 10000, 0), QPointF(offset, 10000)};
            symbol.polygons.append(polygon);
        }
        symbol.graphicOrder = {{QStringLiteral("PG"), 0, 0}};

        AltiumSchLibWriter writer;
        const QString path = QDir(tempDir.path()).filePath(QStringLiteral("conflicting-polygon-order.SchLib"));
        QVERIFY(writer.write({symbol}, path, QStringLiteral("conflicting-polygon-order")));
        QVERIFY(writer.diagnostics().contains(QStringLiteral(
            "符号 CONFLICTING_POLYGON_ORDER 的 graphicOrder 不完整或包含无效引用，已回退到默认图元顺序")));

        QByteArray data;
        QVERIFY(readCfbStream(path, QStringLiteral("CONFLICTING_POLYGON_ORDER/Data"), data));
        QCOMPARE(data.count(QByteArrayLiteral("RECORD=7")), 2);
    }

    /**
     * @brief 路径分段编号出现缺口时仍按实际索引写出全部图元。
     */
    void gappedPathSegmentsPreserveGraphics() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        AltiumSchComponent symbol;
        symbol.name = QStringLiteral("GAPPED_PATH");
        for (const int segmentIndex : {0, 2}) {
            AltiumSchPath path;
            path.sourceGraphicType = QStringLiteral("PT");
            path.sourceGraphicIndex = 0;
            path.sourceSegmentIndex = segmentIndex;
            path.sourcePartIndex = 0;
            path.vertices = {QPointF(segmentIndex * 1000, 0), QPointF(segmentIndex * 1000 + 1000, 0)};
            symbol.paths.append(path);
        }
        symbol.graphicOrder = {{QStringLiteral("PT"), 0, 0}};

        AltiumSchLibWriter writer;
        const QString path = QDir(tempDir.path()).filePath(QStringLiteral("gapped-path.SchLib"));
        QVERIFY(writer.write({symbol}, path, QStringLiteral("gapped-path")));
        QVERIFY(!writer.diagnostics().contains(
            QStringLiteral("符号 GAPPED_PATH 的 graphicOrder 不完整或包含无效引用，已回退到默认图元顺序")));

        QByteArray data;
        QVERIFY(readCfbStream(path, QStringLiteral("GAPPED_PATH/Data"), data));
        QCOMPARE(data.count(QByteArrayLiteral("RECORD=6")), 2);
    }

    /**
     * @brief 来源图元索引缺失时回退，避免有序写出丢失弧线。
     */
    void unresolvedOrderedGraphicFallsBackWithoutDroppingGraphics() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        AltiumSchComponent symbol;
        symbol.name = QStringLiteral("UNRESOLVED_ORDER");

        AltiumSchRectangle rectangle;
        rectangle.sourceGraphicIndex = 0;
        rectangle.sourcePartIndex = 0;
        rectangle.cornerX = 100000;
        rectangle.cornerY = 100000;
        symbol.rectangles.append(rectangle);

        AltiumSchArc arc;
        arc.radius = 50000;
        arc.sourceGraphicType = QStringLiteral("A");
        arc.sourceGraphicIndex = -1;
        arc.sourcePartIndex = 0;
        symbol.arcs.append(arc);
        symbol.graphicOrder = {{QStringLiteral("R"), 0, 0}};

        AltiumSchLibWriter writer;
        const QString path = QDir(tempDir.path()).filePath(QStringLiteral("unresolved-order.SchLib"));
        QVERIFY(writer.write({symbol}, path, QStringLiteral("unresolved-order")));
        QVERIFY(writer.diagnostics().contains(
            QStringLiteral("符号 UNRESOLVED_ORDER 的 graphicOrder 不完整或包含无效引用，已回退到默认图元顺序")));

        QByteArray data;
        QVERIFY(readCfbStream(path, QStringLiteral("UNRESOLVED_ORDER/Data"), data));
        QCOMPARE(data.count(QByteArrayLiteral("RECORD=14")), 1);
        QCOMPARE(data.count(QByteArrayLiteral("RECORD=12")), 1);
    }

    /**
     * @brief 验证 Pie 和 IEEE 图元遵循无 UniqueID 的兼容记录格式。
     */
    void pieAndIeeeRecordsOmitUniqueIds() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        AltiumSchComponent symbol;
        symbol.name = QStringLiteral("UNIQUE_ID_GRAPHICS");
        AltiumSchPie pie;
        pie.centerX = 100000;
        pie.radius = 200000;
        symbol.pies.append(pie);
        AltiumSchIeee ieee;
        ieee.locationX = 300000;
        ieee.locationY = 400000;
        symbol.ieeeSymbols.append(ieee);

        AltiumSchLibWriter writer;
        const QString path = QDir(tempDir.path()).filePath(QStringLiteral("unique-id-graphics.SchLib"));
        QVERIFY(writer.write({symbol}, path, QStringLiteral("unique-id-graphics")));

        QByteArray data;
        QVERIFY(readCfbStream(path, QStringLiteral("UNIQUE_ID_GRAPHICS/Data"), data));
        const int pieOffset = data.indexOf("RECORD=9");
        const int ieeeOffset = data.indexOf("RECORD=3");
        QVERIFY(pieOffset >= 0);
        QVERIFY(ieeeOffset >= 0);
        QVERIFY(!data.mid(pieOffset, ieeeOffset - pieOffset).contains("UniqueID="));
        const int nextRecordOffset = data.indexOf("|RECORD=", ieeeOffset + 1);
        QVERIFY(nextRecordOffset > ieeeOffset);
        QVERIFY(!data.mid(ieeeOffset, nextRecordOffset - ieeeOffset).contains("UniqueID="));
    }

    /**
     * @brief 验证二进制引脚会占用共享内容索引，且首条用户参数不会写出索引 0。
     */
    void contentIndexIsSharedAcrossPinsGraphicsAndParameters() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        AltiumSchComponent symbol;
        symbol.name = QStringLiteral("CONTENT_INDEX");
        symbol.designatorPrefix = QStringLiteral("U");

        AltiumSchPin pin;
        pin.name = QStringLiteral("IN");
        pin.designator = QStringLiteral("1");
        symbol.pins.append(pin);

        AltiumSchRectangle rectangle;
        rectangle.locationX = -100000;
        rectangle.locationY = -100000;
        rectangle.cornerX = 100000;
        rectangle.cornerY = 100000;
        symbol.rectangles.append(rectangle);

        AltiumSchParameter parameter;
        parameter.name = QStringLiteral("Custom");
        parameter.value = QStringLiteral("value");
        parameter.ownerPartId = 1;
        symbol.parameters.append(parameter);
        AltiumSchParameter commonParameter;
        commonParameter.name = QStringLiteral("Common");
        commonParameter.value = QStringLiteral("shared");
        commonParameter.ownerPartId = -1;
        symbol.parameters.append(commonParameter);
        AltiumSchParameter secondPartParameter;
        secondPartParameter.name = QStringLiteral("PartSpecific");
        secondPartParameter.value = QStringLiteral("part");
        secondPartParameter.ownerPartId = 1;
        symbol.parameters.append(secondPartParameter);

        AltiumSchLibWriter writer;
        const QString path = QDir(tempDir.path()).filePath(QStringLiteral("content-index.SchLib"));
        QVERIFY(writer.write({symbol}, path, QStringLiteral("content-index")));

        QByteArray data;
        QVERIFY(readCfbStream(path, QStringLiteral("CONTENT_INDEX/Data"), data));
        QVERIFY(!data.contains("IndexInSheet=0"));
        QVERIFY(data.contains("RECORD=14"));
        QVERIFY(data.contains("IndexInSheet=1"));
        QVERIFY(data.contains("OWNERPARTDISPLAYMODE=1"));
        QVERIFY(data.contains("OWNERPARTID=1"));
        QVERIFY(data.contains("RECORD=41"));
        QVERIFY(data.contains("IndexInSheet=2"));
        const int customParameterOffset = data.indexOf("NAME=Custom");
        const int parameterOffset = data.lastIndexOf("|RECORD=41|", customParameterOffset);
        const int nextRecordOffset = data.indexOf("|RECORD=", parameterOffset + 1);
        QVERIFY(parameterOffset >= 0);
        QVERIFY(customParameterOffset > parameterOffset);
        QVERIFY(nextRecordOffset > parameterOffset);
        const QByteArray parameterRecord = data.mid(parameterOffset, nextRecordOffset - parameterOffset);
        QVERIFY(parameterRecord.contains("OWNERPARTID=1"));
        QVERIFY(!parameterRecord.contains("ISNOTACCESIBLE"));

        const int commonParameterOffset = data.indexOf("NAME=Common");
        const int commonRecordOffset = data.lastIndexOf("|RECORD=41|", commonParameterOffset);
        const int nextCommonRecordOffset = data.indexOf("|RECORD=", commonParameterOffset + 1);
        QVERIFY(commonRecordOffset >= 0);
        QVERIFY(commonParameterOffset > commonRecordOffset);
        QVERIFY(nextCommonRecordOffset > commonRecordOffset);
        const QByteArray commonRecord = data.mid(commonRecordOffset, nextCommonRecordOffset - commonRecordOffset);
        QVERIFY(commonRecord.contains("OWNERPARTID=-1"));
        QVERIFY(!commonRecord.contains("IndexInSheet="));

        const int partSpecificOffset = data.indexOf("NAME=PartSpecific");
        const int partSpecificRecordOffset = data.lastIndexOf("|RECORD=41|", partSpecificOffset);
        const int nextPartSpecificRecordOffset = data.indexOf("|RECORD=", partSpecificOffset + 1);
        QVERIFY(partSpecificRecordOffset >= 0);
        QVERIFY(partSpecificOffset > partSpecificRecordOffset);
        QVERIFY(nextPartSpecificRecordOffset > partSpecificRecordOffset);
        const QByteArray partSpecificRecord =
            data.mid(partSpecificRecordOffset, nextPartSpecificRecordOffset - partSpecificRecordOffset);
        QVERIFY(partSpecificRecord.contains("OWNERPARTID=1"));
        QVERIFY(partSpecificRecord.contains("IndexInSheet=3"));

        AltiumSchLibReader reader;
        QVERIFY2(reader.open(path), qPrintable(reader.errorString()));
        QVector<AltiumSchLibReader::Record> records;
        QVERIFY2(reader.readComponentRecords(QStringLiteral("CONTENT_INDEX"), &records),
                 qPrintable(reader.errorString()));
        bool foundGraphic = false;
        bool foundParameter = false;
        bool foundCommonParameter = false;
        bool foundPartSpecificParameter = false;
        for (const AltiumSchLibReader::Record& record : records) {
            if (!record.hasParameters)
                continue;
            if (record.recordType == 14) {
                QCOMPARE(record.ownerPartId, 1);
                QCOMPARE(record.ownerPartDisplayMode, 1);
                foundGraphic = true;
            } else if (record.recordType == 41 &&
                       record.parameters.value(QStringLiteral("NAME")) == QStringLiteral("Custom")) {
                QCOMPARE(record.ownerPartId, 1);
                QCOMPARE(record.ownerPartDisplayMode, 1);
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
     * @brief 验证生产 OLE 读取器能够读取目录、迷你流和普通流。
     */
    void compoundReaderReadsMiniAndRegularStreams() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

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

        const QString path = QDir(tempDir.path()).filePath(QStringLiteral("reader-roundtrip.cfb"));
        QVERIFY(writer.saveToFile(path));

        OLECompoundReader reader;
        QVERIFY(reader.open(path));
        QCOMPARE(reader.streamPaths(),
                 QStringList({QStringLiteral("Nested/Deep/Data"), QStringLiteral("Regular"), QStringLiteral("Tiny")}));
        QVERIFY(reader.containsStream(QStringLiteral("Nested/Deep/Data")));

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
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString path = QDir(tempDir.path()).filePath(QStringLiteral("invalid.cfb"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QVERIFY(file.write("not-an-ole-document") > 0);
        file.close();

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
        int16_t signedValue = 0;
        uint32_t unsignedValue = 0;
        float floatValue = 0.0F;
        double doubleValue = 0.0;
        QVERIFY(reader.readInt16(&signedValue));
        QVERIFY(reader.readUInt32(&unsignedValue));
        QVERIFY(reader.readFloat(&floatValue));
        QVERIFY(reader.readDouble(&doubleValue));
        QCOMPARE(signedValue, int16_t(-1234));
        QCOMPARE(unsignedValue, uint32_t(0xA1B2C3D4U));
        QCOMPARE(floatValue, 1.25F);
        QCOMPARE(doubleValue, -3.5);

        QByteArray payload;
        uint8_t flags = 0;
        QVERIFY(reader.readBlock(&payload, &flags));
        QCOMPARE(flags, uint8_t(7));
        QCOMPARE(payload, QByteArrayLiteral("payload"));

        QString value;
        QVERIFY(reader.readPascalShortString(&value));
        QCOMPARE(value, QStringLiteral("???"));  // Writer 的 Pascal 短字符串使用 Latin-1。
        QVERIFY(reader.readStringBlock(&value));
        QCOMPARE(value, QStringLiteral("string-block"));
        QVERIFY(reader.readPascalString(&value));
        QCOMPARE(value, QStringLiteral("pascal-block"));

        QMap<QString, QString> params;
        QVERIFY(reader.readCStringParameterBlock(&params));
        QCOMPARE(params.value(QStringLiteral("ASCII")), QStringLiteral("value"));
        QCOMPARE(params.value(QStringLiteral("DESCRIPTION")), QStringLiteral("参数值"));
        QCOMPARE(reader.remaining(), 0);
        QVERIFY(!reader.hasError());

        QByteArray duplicateData;
        AltiumBinaryWriter duplicateWriter(duplicateData);
        duplicateWriter.writeCStringParameterBlockRaw(QByteArrayLiteral("|DUPLICATE=first|DUPLICATE=second|"));
        AltiumBinaryReader duplicateReader(duplicateData);
        QMap<QString, QString> duplicateParameters;
        QVERIFY(!duplicateReader.readCStringParameterBlock(&duplicateParameters));
        QVERIFY(duplicateReader.errorString().contains(QStringLiteral("重复键")));
    }

    /**
     * @brief 验证 SchLib SectionKeys 能恢复截断和冲突后的存储名称。
     */
    void schLibReaderResolvesSectionKeys() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        AltiumSchComponent first;
        first.name = QStringLiteral("A/B");
        AltiumSchComponent second;
        second.name = QStringLiteral("A:B");
        const QString path = QDir(tempDir.path()).filePath(QStringLiteral("section-keys.SchLib"));
        AltiumSchLibWriter writer;
        QVERIFY(writer.write({first, second}, path, QStringLiteral("section-keys")));

        AltiumSchLibReader reader;
        QVERIFY2(reader.open(path), qPrintable(reader.errorString()));
        const auto components = reader.components();
        QCOMPARE(components.size(), 2);
        QCOMPARE(components.at(0).name, QStringLiteral("A/B"));
        QCOMPARE(components.at(1).name, QStringLiteral("A:B"));
        QVERIFY(components.at(0).sectionKey != components.at(1).sectionKey);
        QByteArray data;
        QVERIFY(reader.readComponentData(QStringLiteral("A/B"), &data));
        QVERIFY(data.contains("LibReference=A/B"));
        QVERIFY(reader.readComponentData(QStringLiteral("A:B"), &data));
        QVERIFY(data.contains("LibReference=A:B"));

        int totalRecordCount = 0;
        for (int componentIndex = 0; componentIndex < components.size(); ++componentIndex) {
            QVector<AltiumSchLibReader::Record> records;
            QVERIFY2(reader.readComponentRecords(componentIndex, &records), qPrintable(reader.errorString()));
            QVERIFY(!records.isEmpty());
            QCOMPARE(records.first().recordType, 1);
            totalRecordCount += records.size();
        }
        bool weightOk = false;
        const int declaredWeight = reader.headerParameters().value(QStringLiteral("WEIGHT")).toInt(&weightOk);
        QVERIFY(weightOk);
        QCOMPARE(declaredWeight, totalRecordCount);
    }

    /**
     * @brief 验证 PcbLib SectionKeys 能恢复冲突后的封装存储名称。
     */
    void pcbLibReaderResolvesSectionKeys() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        AltiumPcbComponent first;
        first.name = QStringLiteral("A/B");
        AltiumPcbComponent second;
        second.name = QStringLiteral("A:B");
        const QString path = QDir(tempDir.path()).filePath(QStringLiteral("section-keys.PcbLib"));
        AltiumPcbLibWriter writer;
        QVERIFY(writer.write({first, second}, path, QStringLiteral("section-keys")));

        AltiumPcbLibReader reader;
        QVERIFY2(reader.open(path), qPrintable(reader.errorString()));
        const auto components = reader.components();
        QCOMPARE(components.size(), 2);
        QCOMPARE(components.at(0).name, QStringLiteral("A/B"));
        QCOMPARE(components.at(1).name, QStringLiteral("A:B"));
        QVERIFY(components.at(0).sectionKey != components.at(1).sectionKey);
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
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const auto createLibrary = [&](const QString& fileName,
                                       const QStringList& names,
                                       const QStringList& sectionKeys,
                                       const QStringList& sectionKeyRefs) {
            OLECompoundWriter writer;
            if (!writer.create())
                return false;

            QByteArray fileHeader;
            AltiumBinaryWriter fileHeaderWriter(fileHeader);
            const QByteArray version = QByteArrayLiteral("PCB 6.0 Binary Library File");
            fileHeaderWriter.writeInt32(version.size());
            fileHeaderWriter.writePascalShortString(QString::fromLatin1(version));
            if (!writer.writeStream(QStringLiteral("FileHeader"), fileHeader))
                return false;

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
        AltiumPcbLibReader duplicateComponentReader;
        QVERIFY(!duplicateComponentReader.open(
            QDir(tempDir.path()).filePath(QStringLiteral("duplicate-pcb-components.PcbLib"))));
        QVERIFY(duplicateComponentReader.errorString().contains(QStringLiteral("封装名称重复")));

        QVERIFY(createLibrary(QStringLiteral("duplicate-pcb-section-keys.PcbLib"),
                              {QStringLiteral("A"), QStringLiteral("B")},
                              {QStringLiteral("SHARED"), QStringLiteral("shared")},
                              {}));
        AltiumPcbLibReader duplicateSectionKeyReader;
        QVERIFY(!duplicateSectionKeyReader.open(
            QDir(tempDir.path()).filePath(QStringLiteral("duplicate-pcb-section-keys.PcbLib"))));
        QVERIFY(duplicateSectionKeyReader.errorString().contains(QStringLiteral("SectionKeys 映射重复")));

        QVERIFY(createLibrary(QStringLiteral("case-mismatched-pcb-section-key.PcbLib"),
                              {QStringLiteral("A"), QStringLiteral("B")},
                              {QStringLiteral("KEY_A"), QStringLiteral("KEY_B")},
                              {QStringLiteral("a"), QStringLiteral("B")}));
        AltiumPcbLibReader caseMismatchedReader;
        QVERIFY(!caseMismatchedReader.open(
            QDir(tempDir.path()).filePath(QStringLiteral("case-mismatched-pcb-section-key.PcbLib"))));
        QVERIFY(caseMismatchedReader.errorString().contains(QStringLiteral("SectionKeys 包含未知封装")));
    }
};

QTEST_GUILESS_MAIN(TestAltiumOle)
#include "test_altium_ole.moc"
