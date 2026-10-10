#include "core/ir/FootprintIR.h"
#include "core/ir/SymbolIR.h"
#include "core/xpedition/ExporterXpeditionFootprint.h"
#include "core/xpedition/ExporterXpeditionSymbol.h"
#include "core/xpedition/XpeditionHkpAdapter.h"
#include "core/xpedition/XpeditionSymbolAdapter.h"

#include <QFile>
#include <QSet>
#include <QTemporaryDir>
#include <QTest>

using namespace EasyKiConverter;

namespace {

// 读取 ZIP 二进制字段，验证导出文件具备完整的本地文件头、中央目录和结束记录。
/**
 * @brief 验证 readLittleEndian32 测试场景。
 * @param data 待验证的文本或数据内容。
 * @param offset 文件中的字节偏移量。
 */
quint32 readLittleEndian32(const QByteArray& data, qsizetype offset) {
    return static_cast<quint32>(static_cast<unsigned char>(data.at(offset))) |
           (static_cast<quint32>(static_cast<unsigned char>(data.at(offset + 1))) << 8) |
           (static_cast<quint32>(static_cast<unsigned char>(data.at(offset + 2))) << 16) |
           (static_cast<quint32>(static_cast<unsigned char>(data.at(offset + 3))) << 24);
}

// 读取 ZIP 二进制字段中的 16 位小端整数。
/**
 * @brief 验证 readLittleEndian16 测试场景。
 * @param expectedEntries 期望出现的条目集合。
 */
quint16 readLittleEndian16(const QByteArray& data, qsizetype offset) {
    return static_cast<quint16>(static_cast<unsigned char>(data.at(offset))) |
           (static_cast<quint16>(static_cast<unsigned char>(data.at(offset + 1))) << 8);
}

// 验证 ZIP 结构和指定条目名称，避免测试只检查压缩包魔数而漏掉截断文件。
/**
 * @brief 验证 hasValidZipStructure 测试场景。
 * @param data 待验证的文本或数据内容。
 */
bool hasValidZipStructure(const QByteArray& data, const QSet<QByteArray>& expectedEntries) {
    // localHeader：本地文件头数据。
    constexpr quint32 localHeader = 0x04034b50;
    // centralHeader：中央目录文件头数据。
    constexpr quint32 centralHeader = 0x02014b50;
    // endOfDirectory：测试使用的文件系统路径。
    constexpr quint32 endOfDirectory = 0x06054b50;
    // endOffset：中央目录结束记录偏移量。
    const qsizetype endOffset = data.lastIndexOf(QByteArray("PK\x05\x06", 4));
    if (endOffset < 0 || endOffset + 22 > data.size())
        return false;

    // entryCount：测试使用的数量或索引值。
    const quint16 entryCount = readLittleEndian16(data, endOffset + 10);
    // centralSize：测试使用的数量或索引值。
    const quint32 centralSize = readLittleEndian32(data, endOffset + 12);
    // centralOffset：中心目录偏移量。
    const quint32 centralOffset = readLittleEndian32(data, endOffset + 16);
    if (readLittleEndian32(data, endOffset) != endOfDirectory ||
        static_cast<quint64>(centralOffset) + centralSize > static_cast<quint64>(endOffset))
        return false;

    // actualEntries：测试得到的实际结果数据。
    QSet<QByteArray> actualEntries;
    // offset：测试用文件偏移量。
    qsizetype offset = 0;
    while (offset < static_cast<qsizetype>(centralOffset)) {
        if (offset + 30 > data.size() || readLittleEndian32(data, offset) != localHeader)
            return false;
        // flags：测试用标志位集合。
        const quint16 flags = readLittleEndian16(data, offset + 6);
        // nameSize：测试使用的元器件标识或名称。
        const quint16 nameSize = readLittleEndian16(data, offset + 26);
        // extraSize：测试使用的数量或索引值。
        const quint16 extraSize = readLittleEndian16(data, offset + 28);
        // compressedSize：测试使用的数量或索引值。
        const quint32 compressedSize = readLittleEndian32(data, offset + 18);
        // nameOffset：测试使用的元器件标识或名称。
        const qsizetype nameOffset = offset + 30;
        // payloadOffset：数据载荷偏移量。
        const qsizetype payloadOffset = nameOffset + nameSize + extraSize;
        if (flags & 0x0008 || payloadOffset < nameOffset || payloadOffset + compressedSize > data.size() ||
            payloadOffset + compressedSize > centralOffset)
            return false;
        actualEntries.insert(data.mid(nameOffset, nameSize));
        offset = payloadOffset + compressedSize;
    }
    for (const QByteArray& expectedEntry : expectedEntries) {
        if (!actualEntries.contains(expectedEntry))
            return false;
    }
    if (offset != static_cast<qsizetype>(centralOffset) || actualEntries.size() != entryCount)
        return false;

    // centralEntryOffset：测试使用的缓存或元器件对象。
    qsizetype centralEntryOffset = centralOffset;
    // centralEntryCount：测试使用的数量或索引值。
    quint16 centralEntryCount = 0;
    while (centralEntryOffset < endOffset) {
        if (centralEntryOffset + 46 > data.size() || readLittleEndian32(data, centralEntryOffset) != centralHeader)
            return false;
        // nameSize：测试使用的元器件标识或名称。
        const quint16 nameSize = readLittleEndian16(data, centralEntryOffset + 28);
        // extraSize：测试使用的数量或索引值。
        const quint16 extraSize = readLittleEndian16(data, centralEntryOffset + 30);
        // commentSize：测试使用的数量或索引值。
        const quint16 commentSize = readLittleEndian16(data, centralEntryOffset + 32);
        // nameOffset：测试使用的元器件标识或名称。
        const qsizetype nameOffset = centralEntryOffset + 46;
        // nextOffset：下一个测试偏移量。
        const qsizetype nextOffset = nameOffset + nameSize + extraSize + commentSize;
        if (nextOffset > endOffset)
            return false;
        centralEntryOffset = nextOffset;
        ++centralEntryCount;
    }
    return centralEntryOffset == endOffset && centralEntryCount == entryCount;
}

}  // namespace

class TestXpeditionExporter : public QObject {
    Q_OBJECT

private slots:

    // 验证 HKP Cell、Padstack 和 PDB 符号关联可以聚合为统一 ComponentIR。
    /**
     * @brief 验证 hkpAdapterAggregatesComponentIr 测试场景。
     */
    void hkpAdapterAggregatesComponentIr() {
        // model：测试使用的配置值或状态。
        Parser::XpeditionHkpModel model;
        model.pads.append({QStringLiteral("P1"), Parser::XpeditionPadShape::Round, QSizeF(1.0, 1.0), {}, {}, 1});
        model.padstacks.append({QStringLiteral("PS1"), {}, QStringLiteral("P1"), {}, {}, {}, {}, {}, {}, 2});
        // cell：测试用封装单元。
        Parser::XpeditionCellDefinition cell;
        cell.name = QStringLiteral("CELL1");
        cell.pins.append({QStringLiteral("1"), QPointF(2.0, 3.0), QStringLiteral("PS1"), 90.0, false, 3});
        model.cells.append(cell);
        model.cellNameVariants[cell.name].append(cell.name);
        // bottomCell：底层封装单元。
        Parser::XpeditionCellDefinition bottomCell = cell;
        bottomCell.name = QStringLiteral("CELL_BOTTOM");
        model.cells.append(bottomCell);
        model.cellNameVariants[bottomCell.name].append(bottomCell.name);
        model.padNameVariants[QStringLiteral("P1")].append(QStringLiteral("P1"));
        model.padstackNameVariants[QStringLiteral("PS1")].append(QStringLiteral("PS1"));

        // part：测试使用的元器件标识或名称。
        Parser::XpeditionPartDefinition part;
        part.number = QStringLiteral("R100");
        part.name = QStringLiteral("RES_100");
        part.referencePrefix = QStringLiteral("R");
        part.topCell = QStringLiteral("CELL1");
        part.symbol = QStringLiteral("RES_SYMBOL");
        part.properties.insert(QStringLiteral("VALUE"), QStringLiteral("10K"));
        model.parts.append(part);
        // bottomPart：测试使用的元器件标识或名称。
        Parser::XpeditionPartDefinition bottomPart = part;
        bottomPart.number = QStringLiteral("R101");
        bottomPart.name = QStringLiteral("RES_BOTTOM");
        bottomPart.topCell.clear();
        bottomPart.bottomCell = QStringLiteral("CELL_BOTTOM");
        model.parts.append(bottomPart);

        // symbol：测试用符号数据。
        IR::SymbolComponentIR symbol;
        symbol.name = QStringLiteral("RES_SYMBOL");
        symbol.partCount = 1;
        // symbols：测试用符号集合。
        QMap<QString, IR::SymbolComponentIR> symbols;
        symbols.insert(symbol.name, symbol);
        // diagnostics：导出过程中收集的诊断信息。
        Parser::ParseDiagnostics diagnostics;
        // result：测试得到的实际结果数据。
        const XpeditionHkpConversionResult result = XpeditionHkpAdapter::toIR(model, symbols, &diagnostics);
        QCOMPARE(result.footprints.size(), 2);
        QCOMPARE(result.components.size(), 2);
        QCOMPARE(result.components.first().name, QStringLiteral("RES_100"));
        QCOMPARE(result.components.first().package, QStringLiteral("CELL1"));
        QCOMPARE(result.components.first().footprint.pads.first().number, QStringLiteral("1"));
        QCOMPARE(result.components.first().sourceMetadata.value(QStringLiteral("VALUE")).toString(),
                 QStringLiteral("10K"));
        QCOMPARE(result.components.at(1).package, QStringLiteral("CELL_BOTTOM"));
        QVERIFY(!diagnostics.hasErrors());
    }

    // 验证 HKP Cell 轮廓映射为通用轮廓 IR，而不是退化为普通走线。
    /**
     * @brief 验证 hkpAdapterPreservesCellOutlineSemantics 测试场景。
     */
    void hkpAdapterPreservesCellOutlineSemantics() {
        // model：测试使用的配置值或状态。
        Parser::XpeditionHkpModel model;
        model.pads.append({QStringLiteral("P1"), Parser::XpeditionPadShape::Round, QSizeF(1.0, 1.0), {}, {}, 1});
        model.padNameVariants[QStringLiteral("P1")].append(QStringLiteral("P1"));
        model.padstacks.append({QStringLiteral("PS1"), {}, QStringLiteral("P1"), {}, {}, {}, {}, {}, {}, 2});
        model.padstackNameVariants[QStringLiteral("PS1")].append(QStringLiteral("PS1"));
        // cell：测试用封装单元。
        Parser::XpeditionCellDefinition cell;
        cell.name = QStringLiteral("CELL_OUTLINE");
        cell.pins.append({QStringLiteral("1"), {}, QStringLiteral("PS1"), 0.0, false, 3});
        cell.outlines.append({QStringLiteral("SILKSCREEN_OUTLINE"), {QPointF(0, 0), QPointF(2, 0), QPointF(2, 2)}, 4});
        model.cells.append(cell);
        model.cellNameVariants[cell.name].append(cell.name);

        // diagnostics：导出过程中收集的诊断信息。
        Parser::ParseDiagnostics diagnostics;
        // result：测试得到的实际结果数据。
        const IR::FootprintComponentIR result = XpeditionHkpAdapter::toFootprint(model, cell, &diagnostics);
        QCOMPARE(result.outlines.size(), 1);
        QCOMPARE(result.outlines.first().points.size(), 3);
        QCOMPARE(result.outlines.first().layer, IR::LayerType::TopSilk);
        QVERIFY(result.tracks.isEmpty());
        QVERIFY(!diagnostics.hasErrors());
    }

    // 验证 HKP 文档和多个符号文档可以通过组合入口完成跨文件关联。
    /**
     * @brief 验证 hkpAdapterAssociatesSymbolDocuments 测试场景。
     */
    void hkpAdapterAssociatesSymbolDocuments() {
        // document：测试用文档对象。
        Parser::XpeditionHkpDocument document;
        document.type = Parser::XpeditionHkpType::PartsDatabase;
        document.model.pads.append(
            {QStringLiteral("P1"), Parser::XpeditionPadShape::Round, QSizeF(1.0, 1.0), {}, {}, 1});
        document.model.padNameVariants[QStringLiteral("P1")].append(QStringLiteral("P1"));
        document.model.padstacks.append({QStringLiteral("PS1"), {}, QStringLiteral("P1"), {}, {}, {}, {}, {}, {}, 2});
        document.model.padstackNameVariants[QStringLiteral("PS1")].append(QStringLiteral("PS1"));
        // cell：测试用封装单元。
        Parser::XpeditionCellDefinition cell;
        cell.name = QStringLiteral("CELL1");
        cell.pins.append({QStringLiteral("1"), {}, QStringLiteral("PS1"), 0.0, false, 3});
        document.model.cells.append(cell);
        document.model.cellNameVariants[cell.name].append(cell.name);
        // part：测试使用的元器件标识或名称。
        Parser::XpeditionPartDefinition part;
        part.number = QStringLiteral("R1");
        part.name = QStringLiteral("R1");
        part.topCell = QStringLiteral("CELL1");
        part.symbol = QStringLiteral("RES_SYMBOL");
        document.model.parts.append(part);

        // symbolDocument：测试用符号数据。
        const Parser::XpeditionSymbolDocument symbolDocument = Parser::XpeditionSymbolParser::parse(
            QStringLiteral("V 50\nK 1 RES_SYMBOL\nY 1\nP 1 100 0 0 0 0 2 0\nL 0 0 8 0 2 0 1 0 IN\nE\n"),
            QStringLiteral("RES_SYMBOL.1"));
        QVERIFY(symbolDocument.isRecognized());
        // diagnostics：导出过程中收集的诊断信息。
        Parser::ParseDiagnostics diagnostics;
        // result：测试得到的实际结果数据。
        const XpeditionHkpConversionResult result = XpeditionHkpAdapter::toIR(document, {symbolDocument}, &diagnostics);
        QCOMPARE(result.components.size(), 1);
        QCOMPARE(result.components.first().symbol.name, QStringLiteral("RES_SYMBOL"));
        QVERIFY(!diagnostics.hasErrors());
    }

    // 验证 Xpedition 符号格式模型可以通过 Adapter 转换为统一 IR。
    /**
     * @brief 验证 symbolParserAdapterProducesIr 测试场景。
     */
    void symbolParserAdapterProducesIr() {
        const Parser::XpeditionSymbolDocument document =
            Parser::XpeditionSymbolParser::parse(QStringLiteral("V 50\nK 1 ADAPTER_TEST\nY 1\nP 1 100 0 0 0 0 2 0\n"
                                                                "L 0 0 8 0 2 0 1 0 IN\nA 100 0 8 0 3 3 #=1\nE\n"),
                                                 QStringLiteral("adapter.1"));
        QVERIFY(document.isRecognized());
        QVERIFY(!document.diagnostics.hasErrors());

        // symbol：测试用符号数据。
        const IR::SymbolComponentIR symbol = XpeditionSymbolAdapter::toIR(document);
        QCOMPARE(symbol.name, QStringLiteral("ADAPTER_TEST"));
        QCOMPARE(symbol.pins.size(), 1);
        QCOMPARE(symbol.pins.first().designator, QStringLiteral("1"));
        QCOMPARE(symbol.pins.first().name, QStringLiteral("IN"));
        QCOMPARE(symbol.pins.first().partIndex, 0);
        QVERIFY(qAbs(symbol.pins.first().position.x() - 2.54) < 1e-9);
        QVERIFY(qAbs(symbol.pins.first().length - 2.54) < 1e-9);
    }

    // 验证 Xpedition 符号字节入口统一经过编码检测，并保留 UTF-8 BOM 后的符号内容。
    /**
     * @brief 验证 symbolParserReadsBytesWithBom 测试场景。
     */
    void symbolParserReadsBytesWithBom() {
        // data：测试使用的文本或数据内容。
        const QByteArray data = QByteArray("\xEF\xBB\xBFV 50\nK 1 RES_SYMBOL\nE\n");
        const Parser::XpeditionSymbolDocument document =
            Parser::XpeditionSymbolParser::parseBytes(data, QStringLiteral("RES_SYMBOL.1"));
        QVERIFY(document.isRecognized());
        QCOMPARE(document.model.name, QStringLiteral("RES_SYMBOL"));
        QVERIFY(!document.diagnostics.hasErrors());
    }

    // 验证 Xpedition 引脚编号范围会展开为独立编号，并拒绝反向范围。
    /**
     * @brief 验证 symbolParserExpandsPinNumberRanges 测试场景。
     */
    void symbolParserExpandsPinNumberRanges() {
        const Parser::XpeditionSymbolDocument document =
            Parser::XpeditionSymbolParser::parse(QStringLiteral("V 50\nK 1 RANGE_TEST\nY 1\nP 1 100 0 0 0 0 2 0\n"
                                                                "A 100 0 8 0 3 3 #=[1:5:2]\nE\n"),
                                                 QStringLiteral("range.1"));
        QVERIFY(document.isRecognized());
        QVERIFY(!document.diagnostics.hasErrors());
        QCOMPARE(document.model.pins.size(), 1);
        QCOMPARE(document.model.pins.first().numbers, QStringList({"1", "3", "5"}));

        // invalid：测试使用的元器件标识或名称。
        const Parser::XpeditionSymbolDocument invalid = Parser::XpeditionSymbolParser::parse(
            QStringLiteral("V 50\nK 1 INVALID_RANGE\nY 1\nP 1 100 0 0 0 0 2 0\nA 100 0 8 0 3 3 #=[5:1]\nE\n"),
            QStringLiteral("invalid-range.1"));
        QVERIFY(invalid.diagnostics.hasErrors());
    }

    // 验证符号 ZIP 至少包含头部、引脚和矩形等基本 ASCII 记录。
    /**
     * @brief 验证 symbolLibraryContainsAsciiEntry 测试场景。
     */
    void symbolLibraryContainsAsciiEntry() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // symbol：测试用符号数据。
        IR::SymbolComponentIR symbol;
        symbol.name = QStringLiteral("TEST_SYMBOL");
        symbol.designatorPrefix = QStringLiteral("U");
        // pin：测试用引脚数据。
        IR::SymbolPinIR pin;
        pin.name = QStringLiteral("IN");
        pin.designator = QStringLiteral("1");
        pin.position = QPointF(2.54, 0);
        pin.namePosition = QPointF(1.27, 0);
        pin.numberPosition = QPointF(1.27, -1.27);
        pin.length = 2.54;
        pin.direction = IR::PinDirection::Right;
        pin.electricalType = IR::PinElectricalType::Input;
        symbol.pins.append(pin);
        symbol.rectangles.append({-2.54, -1.27, 2.54, 1.27});

        // outputPath：测试使用的文件系统路径。
        const QString outputPath = tempDir.filePath(QStringLiteral("symbols.zip"));
        // exporter：负责生成测试输出的导出器实例。
        ExporterXpeditionSymbol exporter;
        QVERIFY(exporter.exportSymbolLibrary({symbol}, QStringLiteral("Library"), outputPath, false, false));

        QFile output(outputPath);
        QVERIFY(output.open(QIODevice::ReadOnly));
        // data：测试使用的文本或数据内容。
        const QByteArray data = output.readAll();
        QVERIFY(data.startsWith("PK\x03\x04"));
        QVERIFY(hasValidZipStructure(data, {QByteArray("TEST_SYMBOL.1")}));
        QVERIFY(data.contains("TEST_SYMBOL.1"));
        QVERIFY(data.contains("V 50"));
        QVERIFY(data.contains("P 1"));
        QVERIFY(data.contains("l 2"));
        QVERIFY(data.contains("PINTYPE=IN"));
        QVERIFY(data.contains("L 100.0000 0.0000 8 0 2 0 1 0 IN"));
        QVERIFY(data.contains("A 100.0000 0.0000 8 0 3 3 #=1"));
        QVERIFY(data.contains("b -100.0000 -50.0000 100.0000 50.0000"));
    }

    /**
     * @brief 验证 Xpedition 符号库直接接口拒绝未实现的追加和更新模式。
     */
    void symbolLibraryRejectsMergeModes() {
        // temporary：测试隔离环境中的临时对象。
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());

        // symbol：测试用符号数据。
        IR::SymbolComponentIR symbol;
        symbol.name = QStringLiteral("MERGE_GUARD");
        symbol.partCount = 1;

        // exporter：负责生成测试输出的导出器实例。
        ExporterXpeditionSymbol exporter;
        // appendPath：测试使用的文件系统路径。
        const QString appendPath = temporary.filePath(QStringLiteral("append.zip"));
        QVERIFY(!exporter.exportSymbolLibrary({symbol}, QStringLiteral("Library"), appendPath, true, false));
        QVERIFY(exporter.diagnostics().join(QStringLiteral("\n")).contains(QStringLiteral("不支持追加或更新")));

        // updatePath：测试使用的文件系统路径。
        const QString updatePath = temporary.filePath(QStringLiteral("update.zip"));
        QVERIFY(!exporter.exportSymbolLibrary({symbol}, QStringLiteral("Library"), updatePath, false, true));
        QVERIFY(exporter.diagnostics().join(QStringLiteral("\n")).contains(QStringLiteral("不支持追加或更新")));
    }

    // 验证暂未支持的引脚装饰会生成诊断，并且不会静默改变符号正文。
    /**
     * @brief 验证 unsupportedPinDecorationsAreReported 测试场景。
     */
    void unsupportedPinDecorationsAreReported() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // symbol：测试用符号数据。
        IR::SymbolComponentIR symbol;
        symbol.name = QStringLiteral("DECORATED_SYMBOL");
        // pin：测试用引脚数据。
        IR::SymbolPinIR pin;
        pin.name = QStringLiteral("CLK");
        pin.designator = QStringLiteral("1");
        pin.position = QPointF(1.0, 0.0);
        pin.style.clock = true;
        symbol.pins.append(pin);

        // outputPath：测试使用的文件系统路径。
        const QString outputPath = tempDir.filePath(QStringLiteral("decorated-symbol.zip"));
        // exporter：负责生成测试输出的导出器实例。
        ExporterXpeditionSymbol exporter;
        QVERIFY(exporter.exportSymbolLibrary({symbol}, QStringLiteral("Library"), outputPath, false, false));
        QVERIFY(exporter.diagnostics().join(QStringLiteral("\n")).contains(QStringLiteral("引脚装饰")));
    }

    // 验证封装库同时写出 Padstack 与 Cell 两类目标文件。
    /**
     * @brief 验证 footprintLibraryContainsPadAndCellEntries 测试场景。
     */
    void footprintLibraryContainsPadAndCellEntries() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // footprint：测试用封装数据。
        IR::FootprintComponentIR footprint;
        footprint.name = QStringLiteral("TEST_FOOTPRINT");
        // pad：测试用焊盘数据。
        IR::FootprintPadIR pad;
        pad.number = QStringLiteral("1");
        pad.position = QPointF(0, 0);
        pad.size = QSizeF(1.27, 1.27);
        pad.shape = IR::PadShape::Rect;
        footprint.pads.append(pad);
        // overlay：测试用覆盖层图元。
        IR::FootprintTrackIR overlay;
        overlay.layer = IR::LayerType::TopOverlay;
        overlay.points = {QPointF(-1.0, 0.0), QPointF(1.0, 0.0)};
        overlay.width = 0.1;
        footprint.tracks.append(overlay);

        // outputPath：测试使用的文件系统路径。
        const QString outputPath = tempDir.filePath(QStringLiteral("footprints.zip"));
        // exporter：负责生成测试输出的导出器实例。
        ExporterXpeditionFootprint exporter;
        QVERIFY(exporter.exportFootprintLibrary({footprint}, QStringLiteral("Library"), outputPath));

        QFile output(outputPath);
        QVERIFY(output.open(QIODevice::ReadOnly));
        // data：测试使用的文本或数据内容。
        const QByteArray data = output.readAll();
        QVERIFY(data.startsWith("PK\x03\x04"));
        QVERIFY(
            hasValidZipStructure(data, {QByteArray("TEST_FOOTPRINT_Pads.hkp"), QByteArray("TEST_FOOTPRINT_Cell.hkp")}));
        QVERIFY(data.contains("TEST_FOOTPRINT_Pads.hkp"));
        QVERIFY(data.contains("TEST_FOOTPRINT_Cell.hkp"));
        QVERIFY(data.contains(".PADSTACK"));
        QVERIFY(data.contains("TOP_SOLDERPASTE_PAD"));
        QVERIFY(data.contains("TOP_SOLDERMASK_PAD"));
        QVERIFY(data.contains("_MASK"));
        QVERIFY(data.contains("..PIN \"1\""));
        QVERIFY(data.contains("SILKSCREEN_OUTLINE"));
        QVERIFY(data.contains("MNT_SIDE"));
    }

    // 验证通孔焊盘会建立可被 Padstack 引用的钻孔定义。
    /**
     * @brief 验证 throughHolePadUsesReferencedHoleDefinition 测试场景。
     */
    void throughHolePadUsesReferencedHoleDefinition() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // footprint：测试用封装数据。
        IR::FootprintComponentIR footprint;
        footprint.name = QStringLiteral("TH_FOOTPRINT");
        // pad：测试用焊盘数据。
        IR::FootprintPadIR pad;
        pad.number = QStringLiteral("1");
        pad.size = QSizeF(2.0, 2.0);
        pad.holeSize = 1.0;
        pad.padType = IR::PadType::ThroughHole;
        pad.shape = IR::PadShape::Ellipse;
        footprint.pads.append(pad);

        // outputPath：测试使用的文件系统路径。
        const QString outputPath = tempDir.filePath(QStringLiteral("through-hole.zip"));
        // exporter：负责生成测试输出的导出器实例。
        ExporterXpeditionFootprint exporter;
        QVERIFY(exporter.exportFootprintLibrary({footprint}, QStringLiteral("Library"), outputPath));

        QFile output(outputPath);
        QVERIFY(output.open(QIODevice::ReadOnly));
        // data：测试使用的文本或数据内容。
        const QByteArray data = output.readAll();
        QVERIFY(data.contains("...HOLE_NAME \"HOLE_39.3701\""));
        QVERIFY(data.contains(".Hole \"HOLE_39.3701\""));
    }

    // 验证外形相同但钻孔参数不同的通孔焊盘不会错误复用 Padstack。
    /**
     * @brief 验证 throughHolePadsWithDifferentDrillsRemainDistinct 测试场景。
     */
    void throughHolePadsWithDifferentDrillsRemainDistinct() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // footprint：测试用封装数据。
        IR::FootprintComponentIR footprint;
        footprint.name = QStringLiteral("DISTINCT_DRILLS");
        // first：第一个测试结果。
        IR::FootprintPadIR first;
        first.number = QStringLiteral("1");
        first.size = QSizeF(2.0, 2.0);
        first.holeSize = 0.8;
        first.padType = IR::PadType::ThroughHole;
        // second：第二个测试结果。
        IR::FootprintPadIR second = first;
        second.number = QStringLiteral("2");
        second.holeSize = 1.0;
        footprint.pads = {first, second};

        // outputPath：测试使用的文件系统路径。
        const QString outputPath = tempDir.filePath(QStringLiteral("distinct-drills.zip"));
        // exporter：负责生成测试输出的导出器实例。
        ExporterXpeditionFootprint exporter;
        QVERIFY(exporter.exportFootprintLibrary({footprint}, QStringLiteral("Library"), outputPath));

        QFile output(outputPath);
        QVERIFY(output.open(QIODevice::ReadOnly));
        // data：测试使用的文本或数据内容。
        const QByteArray data = output.readAll();
        QVERIFY(data.contains(".PADSTACK \"PAD_RECTANGLE_78.7402x78.7402_H31.4961_L0.0000_TH\""));
        QVERIFY(data.contains(".PADSTACK \"PAD_RECTANGLE_78.7402x78.7402_H39.3701_L0.0000_TH\""));
        QVERIFY(data.contains(".Hole \"HOLE_31.4961\""));
        QVERIFY(data.contains(".Hole \"HOLE_39.3701\""));
    }

    // 验证相同几何参数但不同镀层属性的通孔焊盘不会错误复用钻孔定义。
    /**
     * @brief 验证 throughHolePlatingModesRemainDistinct 测试场景。
     */
    void throughHolePlatingModesRemainDistinct() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // footprint：测试用封装数据。
        IR::FootprintComponentIR footprint;
        footprint.name = QStringLiteral("PLATING_MODES");
        // plated：镀层属性测试值。
        IR::FootprintPadIR plated;
        plated.number = QStringLiteral("1");
        plated.size = QSizeF(2.0, 2.0);
        plated.holeSize = 1.0;
        plated.padType = IR::PadType::ThroughHole;
        // nonPlated：非镀层属性测试值。
        IR::FootprintPadIR nonPlated = plated;
        nonPlated.number = QStringLiteral("2");
        nonPlated.isPlated = false;
        footprint.pads = {plated, nonPlated};

        // outputPath：测试使用的文件系统路径。
        const QString outputPath = tempDir.filePath(QStringLiteral("plating-modes.zip"));
        // exporter：负责生成测试输出的导出器实例。
        ExporterXpeditionFootprint exporter;
        QVERIFY(exporter.exportFootprintLibrary({footprint}, QStringLiteral("Library"), outputPath));

        QFile output(outputPath);
        QVERIFY(output.open(QIODevice::ReadOnly));
        // data：测试使用的文本或数据内容。
        const QByteArray data = output.readAll();
        QVERIFY(data.contains("PADSTACK \"PAD_RECTANGLE_78.7402x78.7402_H39.3701_L0.0000_TH\""));
        QVERIFY(data.contains("PADSTACK \"PAD_RECTANGLE_78.7402x78.7402_H39.3701_L0.0000_NONPLATED_TH\""));
        QVERIFY(data.contains(
            ".Hole \"HOLE_39.3701\"\n..POSITIVE_TOLERANCE 0\n..NEGATIVE_TOLERANCE 0\n..HOLE_OPTIONS PLATED"));
        QVERIFY(
            data.contains(".Hole \"HOLE_39.3701_NONPLATED\"\n..POSITIVE_TOLERANCE 0\n..NEGATIVE_TOLERANCE "
                          "0\n..HOLE_OPTIONS NON_PLATED"));
    }

    // 验证独立安装孔的 Cell 引用与 Padstack 定义使用完全相同的名称。
    /**
     * @brief 验证 standaloneMountingHoleUsesExistingPadstackReference 测试场景。
     */
    void standaloneMountingHoleUsesExistingPadstackReference() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // footprint：测试用封装数据。
        IR::FootprintComponentIR footprint;
        footprint.name = QStringLiteral("MOUNTING_HOLE");
        // hole：测试用孔径数据。
        IR::FootprintHoleIR hole;
        hole.center = QPointF(2.0, 3.0);
        hole.radius = 0.5;
        footprint.holes.append(hole);

        // outputPath：测试使用的文件系统路径。
        const QString outputPath = tempDir.filePath(QStringLiteral("mounting-hole.zip"));
        // exporter：负责生成测试输出的导出器实例。
        ExporterXpeditionFootprint exporter;
        QVERIFY(exporter.exportFootprintLibrary({footprint}, QStringLiteral("Library"), outputPath));

        QFile output(outputPath);
        QVERIFY(output.open(QIODevice::ReadOnly));
        // data：测试使用的文本或数据内容。
        const QByteArray data = output.readAll();
        // stackReference：焊盘栈引用。
        const QByteArray stackReference = "...PADSTACK \"HOLE_39.3701_NONPLATED_TH\"";
        QVERIFY(data.contains(".PADSTACK \"HOLE_39.3701_NONPLATED_TH\""));
        QVERIFY(data.contains(stackReference));
        QVERIFY(!data.contains("...PADSTACK \"HOLE_39.3701_TH\""));
    }

    // 验证非圆椭圆和带顶点的圆角/梯形焊盘不会静默退化为矩形。
    /**
     * @brief 验证 padShapeMappingsPreserveSupportedGeometry 测试场景。
     */
    void padShapeMappingsPreserveSupportedGeometry() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // footprint：测试用封装数据。
        IR::FootprintComponentIR footprint;
        footprint.name = QStringLiteral("PAD_SHAPES");
        // ellipse：测试用椭圆焊盘。
        IR::FootprintPadIR ellipse;
        ellipse.number = QStringLiteral("1");
        ellipse.size = QSizeF(2.0, 1.0);
        ellipse.shape = IR::PadShape::Ellipse;
        footprint.pads.append(ellipse);
        // custom：测试用自定义图形。
        IR::FootprintPadIR custom;
        custom.number = QStringLiteral("2");
        custom.size = QSizeF(2.0, 2.0);
        custom.shape = IR::PadShape::RoundRect;
        custom.customShapePoints = {QPointF(-1.0, -0.5), QPointF(1.0, -0.5), QPointF(1.0, 0.5), QPointF(-1.0, 0.5)};
        footprint.pads.append(custom);

        // outputPath：测试使用的文件系统路径。
        const QString outputPath = tempDir.filePath(QStringLiteral("pad-shapes.zip"));
        // exporter：负责生成测试输出的导出器实例。
        ExporterXpeditionFootprint exporter;
        QVERIFY(exporter.exportFootprintLibrary({footprint}, QStringLiteral("Library"), outputPath));

        QFile output(outputPath);
        QVERIFY(output.open(QIODevice::ReadOnly));
        // data：测试使用的文本或数据内容。
        const QByteArray data = output.readAll();
        QVERIFY(data.contains("..OBLONG\n...WIDTH 78.7402\n...HEIGHT 39.3701"));
        QVERIFY(data.contains("..CUSTOM\n...POLYLINE_SHAPE"));
        QVERIFY(!data.contains("ROUNDRECT"));
    }

    // 验证无法由 IR 顶点表达的异形焊盘会阻止导出并返回明确诊断。
    /**
     * @brief 验证 unsupportedPadShapeFailsWithDiagnostic 测试场景。
     */
    void unsupportedPadShapeFailsWithDiagnostic() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // footprint：测试用封装数据。
        IR::FootprintComponentIR footprint;
        footprint.name = QStringLiteral("UNSUPPORTED_PAD");
        // pad：测试用焊盘数据。
        IR::FootprintPadIR pad;
        pad.number = QStringLiteral("1");
        pad.shape = IR::PadShape::Trapezoid;
        pad.size = QSizeF(1.0, 1.0);
        footprint.pads.append(pad);

        // exporter：负责生成测试输出的导出器实例。
        ExporterXpeditionFootprint exporter;
        QVERIFY(!exporter.exportFootprintLibrary(
            {footprint}, QStringLiteral("Library"), tempDir.filePath(QStringLiteral("unsupported.zip"))));
        QVERIFY(exporter.diagnostics().join(QStringLiteral("\n")).contains(QStringLiteral("形状无法表达")));
    }

    // 验证底层表贴焊盘不会被错误写入顶层铜、阻焊和锡膏层。
    /**
     * @brief 验证 bottomSmdPadUsesBottomAssignments 测试场景。
     */
    void bottomSmdPadUsesBottomAssignments() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // footprint：测试用封装数据。
        IR::FootprintComponentIR footprint;
        footprint.name = QStringLiteral("BOTTOM_SMD");
        // pad：测试用焊盘数据。
        IR::FootprintPadIR pad;
        pad.number = QStringLiteral("1");
        pad.size = QSizeF(1.0, 1.0);
        pad.layer = IR::LayerType::BottomCopper;
        footprint.pads.append(pad);

        // outputPath：测试使用的文件系统路径。
        const QString outputPath = tempDir.filePath(QStringLiteral("bottom-smd.zip"));
        // exporter：负责生成测试输出的导出器实例。
        ExporterXpeditionFootprint exporter;
        QVERIFY(exporter.exportFootprintLibrary({footprint}, QStringLiteral("Library"), outputPath));

        QFile output(outputPath);
        QVERIFY(output.open(QIODevice::ReadOnly));
        // data：测试使用的文本或数据内容。
        const QByteArray data = output.readAll();
        QVERIFY(data.contains("PADSTACK \"PAD_RECTANGLE_39.3701x39.3701_BOTTOM_SMD\""));
        QVERIFY(data.contains("...BOTTOM_PAD \"PAD_RECTANGLE_39.3701x39.3701\""));
        QVERIFY(data.contains("...BOTTOM_SOLDERMASK_PAD \"PAD_RECTANGLE_39.3701x39.3701_MASK\""));
        QVERIFY(data.contains("...BOTTOM_SOLDERPASTE_PAD \"PAD_RECTANGLE_39.3701x39.3701\""));
        QVERIFY(!data.contains("...TOP_PAD \"PAD_RECTANGLE_39.3701x39.3701\""));
        QVERIFY(!data.contains("...TOP_SOLDERMASK_PAD \"PAD_RECTANGLE_39.3701x39.3701_MASK\""));
        QVERIFY(!data.contains("...TOP_SOLDERPASTE_PAD \"PAD_RECTANGLE_39.3701x39.3701\""));
    }

    // 验证槽孔会写出独立的宽高定义，而不是退化为圆孔。
    /**
     * @brief 验证 slottedThroughHoleUsesSlotDefinition 测试场景。
     */
    void slottedThroughHoleUsesSlotDefinition() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // footprint：测试用封装数据。
        IR::FootprintComponentIR footprint;
        footprint.name = QStringLiteral("SLOTTED_HOLE");
        // pad：测试用焊盘数据。
        IR::FootprintPadIR pad;
        pad.number = QStringLiteral("1");
        pad.size = QSizeF(3.0, 3.0);
        pad.holeSize = 1.0;
        pad.holeLength = 2.0;
        pad.padType = IR::PadType::ThroughHole;
        footprint.pads.append(pad);

        // outputPath：测试使用的文件系统路径。
        const QString outputPath = tempDir.filePath(QStringLiteral("slotted-hole.zip"));
        // exporter：负责生成测试输出的导出器实例。
        ExporterXpeditionFootprint exporter;
        QVERIFY(exporter.exportFootprintLibrary({footprint}, QStringLiteral("Library"), outputPath));

        QFile output(outputPath);
        QVERIFY(output.open(QIODevice::ReadOnly));
        // data：测试使用的文本或数据内容。
        const QByteArray data = output.readAll();
        QVERIFY(data.contains("SLOTTED_HOLE_Pads.hkp"));
        QVERIFY(data.contains("SLOTTED_HOLE_Cell.hkp"));
        QVERIFY(data.contains("PADSTACK \"PAD_RECTANGLE_118.1102x118.1102_H39.3701_L78.7402_TH\""));
        QVERIFY(data.contains("..HOLE_NAME \"HOLE_39.3701x78.7402\""));
        QVERIFY(data.contains("..SLOT\n...WIDTH 78.7402\n...HEIGHT 39.3701"));
    }

    // 验证圆弧、文本、填充区域和独立孔都能写入，并且诊断信息说明转换策略。
    /**
     * @brief 验证 unsupportedFootprintElementsAreReported 测试场景。
     */
    void unsupportedFootprintElementsAreReported() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // footprint：测试用封装数据。
        IR::FootprintComponentIR footprint;
        footprint.name = QStringLiteral("DIAGNOSTIC_FOOTPRINT");
        // text：测试使用的文本或数据内容。
        IR::FootprintTextIR text;
        text.text = QStringLiteral("MARK");
        text.position = QPointF(1.0, 2.0);
        text.fontSize = 1.0;
        text.mirror = true;
        text.textPathPoints = {QPointF(0.0, 0.0), QPointF(1.0, 1.0)};
        footprint.texts.append(text);
        // arc：测试用圆弧图元。
        IR::FootprintArcIR arc;
        arc.center = QPointF(0.0, 0.0);
        arc.radius = 1.0;
        arc.startAngle = 0.0;
        arc.endAngle = 90.0;
        footprint.arcs.append(arc);
        // region：测试用区域图元。
        IR::FootprintRegionIR region;
        region.vertices = {QPointF(-1.0, -1.0), QPointF(1.0, -1.0), QPointF(1.0, 1.0)};
        region.isKeepOut = true;
        footprint.regions.append(region);
        // unsupportedOutline：测试用不支持轮廓。
        IR::FootprintOutlineIR unsupportedOutline;
        unsupportedOutline.layer = IR::LayerType::Mechanical1;
        unsupportedOutline.points = {QPointF(0.0, 0.0), QPointF(1.0, 0.0)};
        footprint.outlines.append(unsupportedOutline);
        footprint.shouldGenerateCourtyard = true;
        // hole：测试用孔径数据。
        IR::FootprintHoleIR hole;
        hole.center = QPointF(3.0, 4.0);
        hole.radius = 0.5;
        footprint.holes.append(hole);

        // outputPath：测试使用的文件系统路径。
        const QString outputPath = tempDir.filePath(QStringLiteral("diagnostics.zip"));
        // exporter：负责生成测试输出的导出器实例。
        ExporterXpeditionFootprint exporter;
        QVERIFY(exporter.exportFootprintLibrary({footprint}, QStringLiteral("Library"), outputPath));
        // diagnostics：导出过程中收集的诊断信息。
        const QStringList diagnostics = exporter.diagnostics();
        // diagnosticText：测试使用的文本或数据内容。
        const QString diagnosticText = diagnostics.join(QStringLiteral("\n"));
        QVERIFY(diagnosticText.contains(QStringLiteral("折线近似")));
        QVERIFY(diagnosticText.contains(QStringLiteral("已写入")));
        QVERIFY(diagnosticText.contains(QStringLiteral("无法映射的图层")));
        QVERIFY(diagnosticText.contains(QStringLiteral("文本镜像或路径字形")));
        QVERIFY(diagnosticText.contains(QStringLiteral("KeepOut 标志")));
        QVERIFY(diagnosticText.contains(QStringLiteral("courtyard")));

        QFile output(outputPath);
        QVERIFY(output.open(QIODevice::ReadOnly));
        // data：测试使用的文本或数据内容。
        const QByteArray data = output.readAll();
        QVERIFY(data.contains("..TEXT \"MARK\""));
        QVERIFY(data.contains("POLYLINE_SHAPE"));
        QVERIFY(data.contains("MH1"));
    }

    // 验证圆弧、区域和可见文本会参与 Cell 原点计算，避免导出后整体偏移。
    /**
     * @brief 验证 footprintOriginIncludesAllVisibleGeometry 测试场景。
     */
    void footprintOriginIncludesAllVisibleGeometry() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // footprint：测试用封装数据。
        IR::FootprintComponentIR footprint;
        footprint.name = QStringLiteral("ORIGIN_GEOMETRY");
        // pad：测试用焊盘数据。
        IR::FootprintPadIR pad;
        pad.number = QStringLiteral("1");
        pad.position = QPointF(0.0, 0.0);
        pad.size = QSizeF(1.0, 1.0);
        footprint.pads.append(pad);
        // arc：测试用圆弧图元。
        IR::FootprintArcIR arc;
        arc.center = QPointF(10.0, 0.0);
        arc.radius = 2.0;
        arc.endAngle = 90.0;
        footprint.arcs.append(arc);
        // text：测试使用的文本或数据内容。
        IR::FootprintTextIR text;
        text.text = QStringLiteral("VISIBLE");
        text.position = QPointF(0.0, 8.0);
        footprint.texts.append(text);

        // outputPath：测试使用的文件系统路径。
        const QString outputPath = tempDir.filePath(QStringLiteral("origin-geometry.zip"));
        // exporter：负责生成测试输出的导出器实例。
        ExporterXpeditionFootprint exporter;
        QVERIFY(exporter.exportFootprintLibrary({footprint}, QStringLiteral("Library"), outputPath));

        QFile output(outputPath);
        QVERIFY(output.open(QIODevice::ReadOnly));
        // data：测试使用的文本或数据内容。
        const QByteArray data = output.readAll();
        QVERIFY(data.contains("ORIGIN_GEOMETRY_Cell.hkp"));
        QVERIFY(data.contains("...XY (-226.3780, 118.1102)"));
    }

    // 验证旋转矩形按旋转后的顶点写入 Cell，而不是退化为未旋转矩形。
    /**
     * @brief 验证 rotatedRectangleUsesRotatedVertices 测试场景。
     */
    void rotatedRectangleUsesRotatedVertices() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // footprint：测试用封装数据。
        IR::FootprintComponentIR footprint;
        footprint.name = QStringLiteral("ROTATED_RECTANGLE");
        // rectangle：测试用矩形图元。
        IR::FootprintRectangleIR rectangle;
        rectangle.bounds = QRectF(-1.0, -2.0, 2.0, 4.0);
        rectangle.rotation = 90.0;
        rectangle.layer = IR::LayerType::TopSilk;
        footprint.rectangles.append(rectangle);

        // outputPath：测试使用的文件系统路径。
        const QString outputPath = tempDir.filePath(QStringLiteral("rotated-rectangle.zip"));
        // exporter：负责生成测试输出的导出器实例。
        ExporterXpeditionFootprint exporter;
        QVERIFY(exporter.exportFootprintLibrary({footprint}, QStringLiteral("Library"), outputPath));

        QFile output(outputPath);
        QVERIFY(output.open(QIODevice::ReadOnly));
        // data：测试使用的文本或数据内容。
        const QByteArray data = output.readAll();
        QVERIFY(data.contains("ROTATED_RECTANGLE_Cell.hkp"));
        QVERIFY(data.contains("....XY (78.7402, 39.3701)"));
    }

    // 验证单封装入口收到三维模型路径时会明确报告未建立关联。
    /**
     * @brief 验证 singleFootprintModelPathProducesDiagnostic 测试场景。
     */
    void singleFootprintModelPathProducesDiagnostic() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // footprint：测试用封装数据。
        IR::FootprintComponentIR footprint;
        footprint.name = QStringLiteral("MODEL_DIAGNOSTIC");

        // outputPath：测试使用的文件系统路径。
        const QString outputPath = tempDir.filePath(QStringLiteral("model-diagnostic.zip"));
        // exporter：负责生成测试输出的导出器实例。
        ExporterXpeditionFootprint exporter;
        QVERIFY(exporter.exportFootprint(footprint, outputPath, QStringLiteral("model.step")));

        // diagnosticText：测试使用的文本或数据内容。
        const QString diagnosticText = exporter.diagnostics().join(QStringLiteral("\n"));
        QVERIFY(diagnosticText.contains(QStringLiteral("未写入 3D 模型关联")));
    }

    // 验证符号安全名称冲突会稳定加后缀，且文本字段会转义引号和反斜杠。
    /**
     * @brief 验证 symbolNamesAndTextFieldsAreSafe 测试场景。
     */
    void symbolNamesAndTextFieldsAreSafe() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // first：第一个测试结果。
        IR::SymbolComponentIR first;
        first.name = QStringLiteral("A B");
        first.designatorPrefix = QStringLiteral("U\"");
        // pin：测试用引脚数据。
        IR::SymbolPinIR pin;
        pin.name = QString::fromUtf8("名称\"\\");
        pin.designator = QStringLiteral("1\\\"");
        first.pins.append(pin);
        // second：第二个测试结果。
        IR::SymbolComponentIR second = first;
        second.name = QStringLiteral("A_B");

        // outputPath：测试使用的文件系统路径。
        const QString outputPath = tempDir.filePath(QStringLiteral("safe-symbols.zip"));
        // exporter：负责生成测试输出的导出器实例。
        ExporterXpeditionSymbol exporter;
        QVERIFY(exporter.exportSymbolLibrary({first, second}, QStringLiteral("Library"), outputPath, false, false));

        QFile output(outputPath);
        QVERIFY(output.open(QIODevice::ReadOnly));
        // data：测试使用的文本或数据内容。
        const QByteArray data = output.readAll();
        QVERIFY(data.contains("A_B.1"));
        QVERIFY(data.contains("A_B_2.1"));
        QVERIFY(data.contains("U\\\""));
        QVERIFY(data.contains(QString::fromUtf8("名称\\\"\\\\").toUtf8()));
        QVERIFY(exporter.diagnostics().join(QStringLiteral("\n")).contains(QStringLiteral("名称重复")));
    }
};

QTEST_GUILESS_MAIN(TestXpeditionExporter)

#include "test_xpedition_exporter.moc"
// 使用最小但完整的符号数据，覆盖引脚位置和主体图元写入。
// 这些元素分别覆盖近似转换、文本转义、填充区域闭合和独立孔建模。
