#include "core/ir/FootprintIR.h"
#include "core/ir/SymbolIR.h"
#include "core/xpedition/ExporterXpeditionFootprint.h"
#include "core/xpedition/ExporterXpeditionSymbol.h"
#include "core/xpedition/XpeditionHkpAdapter.h"
#include "core/xpedition/XpeditionSymbolAdapter.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace EasyKiConverter;

class TestXpeditionExporter : public QObject {
    Q_OBJECT

private slots:

    // 验证 HKP Cell、Padstack 和 PDB 符号关联可以聚合为统一 ComponentIR。
    void hkpAdapterAggregatesComponentIr() {
        Parser::XpeditionHkpModel model;
        model.pads.append({QStringLiteral("P1"), Parser::XpeditionPadShape::Round, QSizeF(1.0, 1.0), {}, {}, 1});
        model.padstacks.append({QStringLiteral("PS1"), {}, QStringLiteral("P1"), {}, {}, {}, {}, {}, {}, 2});
        Parser::XpeditionCellDefinition cell;
        cell.name = QStringLiteral("CELL1");
        cell.pins.append({QStringLiteral("1"), QPointF(2.0, 3.0), QStringLiteral("PS1"), 90.0, false, 3});
        model.cells.append(cell);
        model.cellNameVariants[cell.name].append(cell.name);
        Parser::XpeditionCellDefinition bottomCell = cell;
        bottomCell.name = QStringLiteral("CELL_BOTTOM");
        model.cells.append(bottomCell);
        model.cellNameVariants[bottomCell.name].append(bottomCell.name);
        model.padNameVariants[QStringLiteral("P1")].append(QStringLiteral("P1"));
        model.padstackNameVariants[QStringLiteral("PS1")].append(QStringLiteral("PS1"));

        Parser::XpeditionPartDefinition part;
        part.number = QStringLiteral("R100");
        part.name = QStringLiteral("RES_100");
        part.referencePrefix = QStringLiteral("R");
        part.topCell = QStringLiteral("CELL1");
        part.symbol = QStringLiteral("RES_SYMBOL");
        part.properties.insert(QStringLiteral("VALUE"), QStringLiteral("10K"));
        model.parts.append(part);
        Parser::XpeditionPartDefinition bottomPart = part;
        bottomPart.number = QStringLiteral("R101");
        bottomPart.name = QStringLiteral("RES_BOTTOM");
        bottomPart.topCell.clear();
        bottomPart.bottomCell = QStringLiteral("CELL_BOTTOM");
        model.parts.append(bottomPart);

        IR::SymbolComponentIR symbol;
        symbol.name = QStringLiteral("RES_SYMBOL");
        symbol.partCount = 1;
        QMap<QString, IR::SymbolComponentIR> symbols;
        symbols.insert(symbol.name, symbol);
        Parser::ParseDiagnostics diagnostics;
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

    // 验证 HKP 文档和多个符号文档可以通过组合入口完成跨文件关联。
    void hkpAdapterAssociatesSymbolDocuments() {
        Parser::XpeditionHkpDocument document;
        document.type = Parser::XpeditionHkpType::PartsDatabase;
        document.model.pads.append(
            {QStringLiteral("P1"), Parser::XpeditionPadShape::Round, QSizeF(1.0, 1.0), {}, {}, 1});
        document.model.padNameVariants[QStringLiteral("P1")].append(QStringLiteral("P1"));
        document.model.padstacks.append({QStringLiteral("PS1"), {}, QStringLiteral("P1"), {}, {}, {}, {}, {}, {}, 2});
        document.model.padstackNameVariants[QStringLiteral("PS1")].append(QStringLiteral("PS1"));
        Parser::XpeditionCellDefinition cell;
        cell.name = QStringLiteral("CELL1");
        cell.pins.append({QStringLiteral("1"), {}, QStringLiteral("PS1"), 0.0, false, 3});
        document.model.cells.append(cell);
        document.model.cellNameVariants[cell.name].append(cell.name);
        Parser::XpeditionPartDefinition part;
        part.number = QStringLiteral("R1");
        part.name = QStringLiteral("R1");
        part.topCell = QStringLiteral("CELL1");
        part.symbol = QStringLiteral("RES_SYMBOL");
        document.model.parts.append(part);

        const Parser::XpeditionSymbolDocument symbolDocument = Parser::XpeditionSymbolParser::parse(
            QStringLiteral("V 50\nK 1 RES_SYMBOL\nY 1\nP 1 100 0 0 0 0 2 0\nL 0 0 8 0 2 0 1 0 IN\nE\n"),
            QStringLiteral("RES_SYMBOL.1"));
        QVERIFY(symbolDocument.isRecognized());
        Parser::ParseDiagnostics diagnostics;
        const XpeditionHkpConversionResult result = XpeditionHkpAdapter::toIR(document, {symbolDocument}, &diagnostics);
        QCOMPARE(result.components.size(), 1);
        QCOMPARE(result.components.first().symbol.name, QStringLiteral("RES_SYMBOL"));
        QVERIFY(!diagnostics.hasErrors());
    }

    // 验证 Xpedition 符号格式模型可以通过 Adapter 转换为统一 IR。
    void symbolParserAdapterProducesIr() {
        const Parser::XpeditionSymbolDocument document =
            Parser::XpeditionSymbolParser::parse(QStringLiteral("V 50\nK 1 ADAPTER_TEST\nY 1\nP 1 100 0 0 0 0 2 0\n"
                                                                "L 0 0 8 0 2 0 1 0 IN\nA 100 0 8 0 3 3 #=1\nE\n"),
                                                 QStringLiteral("adapter.1"));
        QVERIFY(document.isRecognized());
        QVERIFY(!document.diagnostics.hasErrors());

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
    void symbolParserReadsBytesWithBom() {
        const QByteArray data = QByteArray("\xEF\xBB\xBFV 50\nK 1 RES_SYMBOL\nE\n");
        const Parser::XpeditionSymbolDocument document =
            Parser::XpeditionSymbolParser::parseBytes(data, QStringLiteral("RES_SYMBOL.1"));
        QVERIFY(document.isRecognized());
        QCOMPARE(document.model.name, QStringLiteral("RES_SYMBOL"));
        QVERIFY(!document.diagnostics.hasErrors());
    }

    // 验证 Xpedition 引脚编号范围会展开为独立编号，并拒绝反向范围。
    void symbolParserExpandsPinNumberRanges() {
        const Parser::XpeditionSymbolDocument document =
            Parser::XpeditionSymbolParser::parse(QStringLiteral("V 50\nK 1 RANGE_TEST\nY 1\nP 1 100 0 0 0 0 2 0\n"
                                                                "A 100 0 8 0 3 3 #=[1:5:2]\nE\n"),
                                                 QStringLiteral("range.1"));
        QVERIFY(document.isRecognized());
        QVERIFY(!document.diagnostics.hasErrors());
        QCOMPARE(document.model.pins.size(), 1);
        QCOMPARE(document.model.pins.first().numbers, QStringList({"1", "3", "5"}));

        const Parser::XpeditionSymbolDocument invalid = Parser::XpeditionSymbolParser::parse(
            QStringLiteral("V 50\nK 1 INVALID_RANGE\nY 1\nP 1 100 0 0 0 0 2 0\nA 100 0 8 0 3 3 #=[5:1]\nE\n"),
            QStringLiteral("invalid-range.1"));
        QVERIFY(invalid.diagnostics.hasErrors());
    }

    // 验证符号 ZIP 至少包含头部、引脚和矩形等基本 ASCII 记录。
    void symbolLibraryContainsAsciiEntry() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        IR::SymbolComponentIR symbol;
        symbol.name = QStringLiteral("TEST_SYMBOL");
        symbol.designatorPrefix = QStringLiteral("U");
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

        const QString outputPath = tempDir.filePath(QStringLiteral("symbols.zip"));
        ExporterXpeditionSymbol exporter;
        QVERIFY(exporter.exportSymbolLibrary({symbol}, QStringLiteral("Library"), outputPath, false, false));

        QFile output(outputPath);
        QVERIFY(output.open(QIODevice::ReadOnly));
        const QByteArray data = output.readAll();
        QVERIFY(data.startsWith("PK\x03\x04"));
        QVERIFY(data.contains("TEST_SYMBOL.1"));
        QVERIFY(data.contains("V 50"));
        QVERIFY(data.contains("P 1"));
        QVERIFY(data.contains("l 2"));
        QVERIFY(data.contains("PINTYPE=IN"));
        QVERIFY(data.contains("L 100.0000 0.0000 8 0 2 0 1 0 IN"));
        QVERIFY(data.contains("A 100.0000 0.0000 8 0 3 3 #=1"));
        QVERIFY(data.contains("b -100.0000 -50.0000 100.0000 50.0000"));
    }

    // 验证暂未支持的引脚装饰会生成诊断，并且不会静默改变符号正文。
    void unsupportedPinDecorationsAreReported() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        IR::SymbolComponentIR symbol;
        symbol.name = QStringLiteral("DECORATED_SYMBOL");
        IR::SymbolPinIR pin;
        pin.name = QStringLiteral("CLK");
        pin.designator = QStringLiteral("1");
        pin.position = QPointF(1.0, 0.0);
        pin.style.clock = true;
        symbol.pins.append(pin);

        const QString outputPath = tempDir.filePath(QStringLiteral("decorated-symbol.zip"));
        ExporterXpeditionSymbol exporter;
        QVERIFY(exporter.exportSymbolLibrary({symbol}, QStringLiteral("Library"), outputPath, false, false));
        QVERIFY(exporter.diagnostics().join(QStringLiteral("\n")).contains(QStringLiteral("引脚装饰")));
    }

    // 验证封装库同时写出 Padstack 与 Cell 两类目标文件。
    void footprintLibraryContainsPadAndCellEntries() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        IR::FootprintComponentIR footprint;
        footprint.name = QStringLiteral("TEST_FOOTPRINT");
        IR::FootprintPadIR pad;
        pad.number = QStringLiteral("1");
        pad.position = QPointF(0, 0);
        pad.size = QSizeF(1.27, 1.27);
        pad.shape = IR::PadShape::Rect;
        footprint.pads.append(pad);
        IR::FootprintTrackIR overlay;
        overlay.layer = IR::LayerType::TopOverlay;
        overlay.points = {QPointF(-1.0, 0.0), QPointF(1.0, 0.0)};
        overlay.width = 0.1;
        footprint.tracks.append(overlay);

        const QString outputPath = tempDir.filePath(QStringLiteral("footprints.zip"));
        ExporterXpeditionFootprint exporter;
        QVERIFY(exporter.exportFootprintLibrary({footprint}, QStringLiteral("Library"), outputPath));

        QFile output(outputPath);
        QVERIFY(output.open(QIODevice::ReadOnly));
        const QByteArray data = output.readAll();
        QVERIFY(data.startsWith("PK\x03\x04"));
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
    void throughHolePadUsesReferencedHoleDefinition() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        IR::FootprintComponentIR footprint;
        footprint.name = QStringLiteral("TH_FOOTPRINT");
        IR::FootprintPadIR pad;
        pad.number = QStringLiteral("1");
        pad.size = QSizeF(2.0, 2.0);
        pad.holeSize = 1.0;
        pad.padType = IR::PadType::ThroughHole;
        pad.shape = IR::PadShape::Ellipse;
        footprint.pads.append(pad);

        const QString outputPath = tempDir.filePath(QStringLiteral("through-hole.zip"));
        ExporterXpeditionFootprint exporter;
        QVERIFY(exporter.exportFootprintLibrary({footprint}, QStringLiteral("Library"), outputPath));

        QFile output(outputPath);
        QVERIFY(output.open(QIODevice::ReadOnly));
        const QByteArray data = output.readAll();
        QVERIFY(data.contains("...HOLE_NAME \"HOLE_39.3701\""));
        QVERIFY(data.contains(".Hole \"HOLE_39.3701\""));
    }

    // 验证外形相同但钻孔参数不同的通孔焊盘不会错误复用 Padstack。
    void throughHolePadsWithDifferentDrillsRemainDistinct() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        IR::FootprintComponentIR footprint;
        footprint.name = QStringLiteral("DISTINCT_DRILLS");
        IR::FootprintPadIR first;
        first.number = QStringLiteral("1");
        first.size = QSizeF(2.0, 2.0);
        first.holeSize = 0.8;
        first.padType = IR::PadType::ThroughHole;
        IR::FootprintPadIR second = first;
        second.number = QStringLiteral("2");
        second.holeSize = 1.0;
        footprint.pads = {first, second};

        const QString outputPath = tempDir.filePath(QStringLiteral("distinct-drills.zip"));
        ExporterXpeditionFootprint exporter;
        QVERIFY(exporter.exportFootprintLibrary({footprint}, QStringLiteral("Library"), outputPath));

        QFile output(outputPath);
        QVERIFY(output.open(QIODevice::ReadOnly));
        const QByteArray data = output.readAll();
        QVERIFY(data.contains(".PADSTACK \"PAD_RECTANGLE_78.7402x78.7402_H31.4961_L0.0000_TH\""));
        QVERIFY(data.contains(".PADSTACK \"PAD_RECTANGLE_78.7402x78.7402_H39.3701_L0.0000_TH\""));
        QVERIFY(data.contains(".Hole \"HOLE_31.4961\""));
        QVERIFY(data.contains(".Hole \"HOLE_39.3701\""));
    }

    // 验证相同几何参数但不同镀层属性的通孔焊盘不会错误复用钻孔定义。
    void throughHolePlatingModesRemainDistinct() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        IR::FootprintComponentIR footprint;
        footprint.name = QStringLiteral("PLATING_MODES");
        IR::FootprintPadIR plated;
        plated.number = QStringLiteral("1");
        plated.size = QSizeF(2.0, 2.0);
        plated.holeSize = 1.0;
        plated.padType = IR::PadType::ThroughHole;
        IR::FootprintPadIR nonPlated = plated;
        nonPlated.number = QStringLiteral("2");
        nonPlated.isPlated = false;
        footprint.pads = {plated, nonPlated};

        const QString outputPath = tempDir.filePath(QStringLiteral("plating-modes.zip"));
        ExporterXpeditionFootprint exporter;
        QVERIFY(exporter.exportFootprintLibrary({footprint}, QStringLiteral("Library"), outputPath));

        QFile output(outputPath);
        QVERIFY(output.open(QIODevice::ReadOnly));
        const QByteArray data = output.readAll();
        QVERIFY(data.contains("PADSTACK \"PAD_RECTANGLE_78.7402x78.7402_H39.3701_L0.0000_TH\""));
        QVERIFY(data.contains("PADSTACK \"PAD_RECTANGLE_78.7402x78.7402_H39.3701_L0.0000_NONPLATED_TH\""));
        QVERIFY(data.contains(
            ".Hole \"HOLE_39.3701\"\n..POSITIVE_TOLERANCE 0\n..NEGATIVE_TOLERANCE 0\n..HOLE_OPTIONS PLATED"));
        QVERIFY(
            data.contains(".Hole \"HOLE_39.3701_NONPLATED\"\n..POSITIVE_TOLERANCE 0\n..NEGATIVE_TOLERANCE "
                          "0\n..HOLE_OPTIONS NON_PLATED"));
    }

    // 验证底层表贴焊盘不会被错误写入顶层铜、阻焊和锡膏层。
    void bottomSmdPadUsesBottomAssignments() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        IR::FootprintComponentIR footprint;
        footprint.name = QStringLiteral("BOTTOM_SMD");
        IR::FootprintPadIR pad;
        pad.number = QStringLiteral("1");
        pad.size = QSizeF(1.0, 1.0);
        pad.layer = IR::LayerType::BottomCopper;
        footprint.pads.append(pad);

        const QString outputPath = tempDir.filePath(QStringLiteral("bottom-smd.zip"));
        ExporterXpeditionFootprint exporter;
        QVERIFY(exporter.exportFootprintLibrary({footprint}, QStringLiteral("Library"), outputPath));

        QFile output(outputPath);
        QVERIFY(output.open(QIODevice::ReadOnly));
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
    void slottedThroughHoleUsesSlotDefinition() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        IR::FootprintComponentIR footprint;
        footprint.name = QStringLiteral("SLOTTED_HOLE");
        IR::FootprintPadIR pad;
        pad.number = QStringLiteral("1");
        pad.size = QSizeF(3.0, 3.0);
        pad.holeSize = 1.0;
        pad.holeLength = 2.0;
        pad.padType = IR::PadType::ThroughHole;
        footprint.pads.append(pad);

        const QString outputPath = tempDir.filePath(QStringLiteral("slotted-hole.zip"));
        ExporterXpeditionFootprint exporter;
        QVERIFY(exporter.exportFootprintLibrary({footprint}, QStringLiteral("Library"), outputPath));

        QFile output(outputPath);
        QVERIFY(output.open(QIODevice::ReadOnly));
        const QByteArray data = output.readAll();
        QVERIFY(data.contains("SLOTTED_HOLE_Pads.hkp"));
        QVERIFY(data.contains("SLOTTED_HOLE_Cell.hkp"));
        QVERIFY(data.contains("PADSTACK \"PAD_RECTANGLE_118.1102x118.1102_H39.3701_L78.7402_TH\""));
        QVERIFY(data.contains("..HOLE_NAME \"HOLE_39.3701x78.7402\""));
        QVERIFY(data.contains("..SLOT\n...WIDTH 78.7402\n...HEIGHT 39.3701"));
    }

    // 验证圆弧、文本、填充区域和独立孔都能写入，并且诊断信息说明转换策略。
    void unsupportedFootprintElementsAreReported() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        IR::FootprintComponentIR footprint;
        footprint.name = QStringLiteral("DIAGNOSTIC_FOOTPRINT");
        IR::FootprintTextIR text;
        text.text = QStringLiteral("MARK");
        text.position = QPointF(1.0, 2.0);
        text.fontSize = 1.0;
        text.mirror = true;
        text.textPathPoints = {QPointF(0.0, 0.0), QPointF(1.0, 1.0)};
        footprint.texts.append(text);
        IR::FootprintArcIR arc;
        arc.center = QPointF(0.0, 0.0);
        arc.radius = 1.0;
        arc.startAngle = 0.0;
        arc.endAngle = 90.0;
        footprint.arcs.append(arc);
        IR::FootprintRegionIR region;
        region.vertices = {QPointF(-1.0, -1.0), QPointF(1.0, -1.0), QPointF(1.0, 1.0)};
        region.isKeepOut = true;
        footprint.regions.append(region);
        IR::FootprintOutlineIR unsupportedOutline;
        unsupportedOutline.layer = IR::LayerType::Mechanical1;
        unsupportedOutline.points = {QPointF(0.0, 0.0), QPointF(1.0, 0.0)};
        footprint.outlines.append(unsupportedOutline);
        footprint.shouldGenerateCourtyard = true;
        IR::FootprintHoleIR hole;
        hole.center = QPointF(3.0, 4.0);
        hole.radius = 0.5;
        footprint.holes.append(hole);

        const QString outputPath = tempDir.filePath(QStringLiteral("diagnostics.zip"));
        ExporterXpeditionFootprint exporter;
        QVERIFY(exporter.exportFootprintLibrary({footprint}, QStringLiteral("Library"), outputPath));
        const QStringList diagnostics = exporter.diagnostics();
        const QString diagnosticText = diagnostics.join(QStringLiteral("\n"));
        QVERIFY(diagnosticText.contains(QStringLiteral("折线近似")));
        QVERIFY(diagnosticText.contains(QStringLiteral("已写入")));
        QVERIFY(diagnosticText.contains(QStringLiteral("无法映射的图层")));
        QVERIFY(diagnosticText.contains(QStringLiteral("文本镜像或路径字形")));
        QVERIFY(diagnosticText.contains(QStringLiteral("KeepOut 标志")));
        QVERIFY(diagnosticText.contains(QStringLiteral("courtyard")));

        QFile output(outputPath);
        QVERIFY(output.open(QIODevice::ReadOnly));
        const QByteArray data = output.readAll();
        QVERIFY(data.contains("..TEXT \"MARK\""));
        QVERIFY(data.contains("POLYLINE_SHAPE"));
        QVERIFY(data.contains("MH1"));
    }

    // 验证圆弧、区域和可见文本会参与 Cell 原点计算，避免导出后整体偏移。
    void footprintOriginIncludesAllVisibleGeometry() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        IR::FootprintComponentIR footprint;
        footprint.name = QStringLiteral("ORIGIN_GEOMETRY");
        IR::FootprintPadIR pad;
        pad.number = QStringLiteral("1");
        pad.position = QPointF(0.0, 0.0);
        pad.size = QSizeF(1.0, 1.0);
        footprint.pads.append(pad);
        IR::FootprintArcIR arc;
        arc.center = QPointF(10.0, 0.0);
        arc.radius = 2.0;
        arc.endAngle = 90.0;
        footprint.arcs.append(arc);
        IR::FootprintTextIR text;
        text.text = QStringLiteral("VISIBLE");
        text.position = QPointF(0.0, 8.0);
        footprint.texts.append(text);

        const QString outputPath = tempDir.filePath(QStringLiteral("origin-geometry.zip"));
        ExporterXpeditionFootprint exporter;
        QVERIFY(exporter.exportFootprintLibrary({footprint}, QStringLiteral("Library"), outputPath));

        QFile output(outputPath);
        QVERIFY(output.open(QIODevice::ReadOnly));
        const QByteArray data = output.readAll();
        QVERIFY(data.contains("ORIGIN_GEOMETRY_Cell.hkp"));
        QVERIFY(data.contains("...XY (-226.3780, 118.1102)"));
    }

    // 验证旋转矩形按旋转后的顶点写入 Cell，而不是退化为未旋转矩形。
    void rotatedRectangleUsesRotatedVertices() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        IR::FootprintComponentIR footprint;
        footprint.name = QStringLiteral("ROTATED_RECTANGLE");
        IR::FootprintRectangleIR rectangle;
        rectangle.bounds = QRectF(-1.0, -2.0, 2.0, 4.0);
        rectangle.rotation = 90.0;
        rectangle.layer = IR::LayerType::TopSilk;
        footprint.rectangles.append(rectangle);

        const QString outputPath = tempDir.filePath(QStringLiteral("rotated-rectangle.zip"));
        ExporterXpeditionFootprint exporter;
        QVERIFY(exporter.exportFootprintLibrary({footprint}, QStringLiteral("Library"), outputPath));

        QFile output(outputPath);
        QVERIFY(output.open(QIODevice::ReadOnly));
        const QByteArray data = output.readAll();
        QVERIFY(data.contains("ROTATED_RECTANGLE_Cell.hkp"));
        QVERIFY(data.contains("....XY (78.7402, 39.3701)"));
    }

    // 验证单封装入口收到三维模型路径时会明确报告未建立关联。
    void singleFootprintModelPathProducesDiagnostic() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        IR::FootprintComponentIR footprint;
        footprint.name = QStringLiteral("MODEL_DIAGNOSTIC");

        const QString outputPath = tempDir.filePath(QStringLiteral("model-diagnostic.zip"));
        ExporterXpeditionFootprint exporter;
        QVERIFY(exporter.exportFootprint(footprint, outputPath, QStringLiteral("model.step")));

        const QString diagnosticText = exporter.diagnostics().join(QStringLiteral("\n"));
        QVERIFY(diagnosticText.contains(QStringLiteral("未写入 3D 模型关联")));
    }
};

QTEST_GUILESS_MAIN(TestXpeditionExporter)

#include "test_xpedition_exporter.moc"
// 使用最小但完整的符号数据，覆盖引脚位置和主体图元写入。
// 这些元素分别覆盖近似转换、文本转义、填充区域闭合和独立孔建模。
