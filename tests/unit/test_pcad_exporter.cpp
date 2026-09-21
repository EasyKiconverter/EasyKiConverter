#include "core/parser/PcadModel.h"
#include "core/pcad/ExporterPcadFootprint.h"
#include "core/pcad/ExporterPcadSymbol.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace EasyKiConverter;

class TestPcadExporter final : public QObject {
    Q_OBJECT

private slots:

    /** @brief 验证 P-CAD ASCII 库的头部、Pad Style 去重、Pattern 和图形可以回读。 */
    void writesReadableLibrary() {
        IR::FootprintComponentIR footprint;
        footprint.name = QStringLiteral("QFN_4");
        IR::FootprintPadIR pad;
        pad.number = QStringLiteral("1");
        pad.position = QPointF(0.0, 1.0);
        pad.shape = IR::PadShape::Rect;
        pad.size = QSizeF(1.0, 0.8);
        footprint.pads.append(pad);
        pad.number = QStringLiteral("2");
        pad.position = QPointF(1.0, 1.0);
        footprint.pads.append(pad);

        IR::FootprintRectangleIR rectangle;
        rectangle.bounds = QRectF(-1.0, -1.0, 3.0, 2.0);
        rectangle.layer = IR::LayerType::TopSilk;
        rectangle.strokeWidth = 0.15;
        footprint.rectangles.append(rectangle);

        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        ExporterPcadFootprint exporter;
        const QString path = directory.filePath(QStringLiteral("library.lia"));
        QVERIFY(exporter.exportFootprintLibrary({footprint}, QStringLiteral("TestLibrary"), path));
        QVERIFY2(exporter.diagnostics().isEmpty(), qPrintable(exporter.diagnostics().join('\n')));

        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QByteArray data = file.readAll();
        QVERIFY(data.contains("(asciiHeader"));
        QVERIFY(data.contains("(padStyleDef \"EK_1\""));
        QVERIFY(data.contains("(patternDef \"QFN_4\""));

        const Parser::PcadBoard parsed = Parser::PcadParser::parseBytes(data, path);
        QVERIFY(!parsed.diagnostics.hasErrors());
        QCOMPARE(parsed.unit, Parser::LengthUnit::Millimeter);
        QCOMPARE(parsed.padStyles.size(), 1);
        QCOMPARE(parsed.patterns.size(), 1);
        QCOMPARE(parsed.patterns.first().pads.size(), 2);
        QCOMPARE(parsed.patterns.first().graphics.size(), 4);
    }

    /** @brief 验证无法无损表达的独立安装孔会失败而不是被伪造为焊盘。 */
    void rejectsIndependentHole() {
        IR::FootprintComponentIR footprint;
        footprint.name = QStringLiteral("MOUNT");
        footprint.holes.append({QPointF(0.0, 0.0), 1.0, false});

        QTemporaryDir directory;
        ExporterPcadFootprint exporter;
        QVERIFY(!exporter.exportFootprint(footprint, directory.filePath(QStringLiteral("mount.lia"))));
        QVERIFY(exporter.diagnostics().join('\n').contains(QStringLiteral("独立安装孔")));
    }

    /** @brief 验证清洗后的封装名称冲突会被拒绝。 */
    void rejectsNameCollision() {
        IR::FootprintComponentIR first;
        first.name = QStringLiteral("A/B");
        IR::FootprintComponentIR second;
        second.name = QStringLiteral("A:B");

        QTemporaryDir directory;
        ExporterPcadFootprint exporter;
        QVERIFY(!exporter.exportFootprintLibrary(
            {first, second}, QStringLiteral("collision"), directory.filePath(QStringLiteral("collision.lia"))));
        QVERIFY(exporter.diagnostics().join('\n').contains(QStringLiteral("冲突")));
    }

    /** @brief 验证 P-CAD 符号库包含 symbolDef、compDef、引脚和封装关联。 */
    void writesSymbolAndComponentAssociation() {
        IR::SymbolComponentIR symbol;
        symbol.name = QStringLiteral("QFN_4");
        symbol.designatorPrefix = QStringLiteral("U");
        symbol.footprintName = QStringLiteral("QFN/4");

        IR::SymbolPinIR pin;
        pin.designator = QStringLiteral("1");
        pin.name = QStringLiteral("IN");
        pin.position = QPointF(-2.54, 0.0);
        pin.length = 2.54;
        pin.direction = IR::PinDirection::Right;
        pin.electricalType = IR::PinElectricalType::Input;
        symbol.pins.append(pin);

        IR::SymbolRectangleIR rectangle;
        rectangle.x0 = -1.0;
        rectangle.y0 = -1.0;
        rectangle.x1 = 1.0;
        rectangle.y1 = 1.0;
        rectangle.strokeWidth = 0.1;
        symbol.rectangles.append(rectangle);

        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        ExporterPcadSymbol exporter;
        const QString path = directory.filePath(QStringLiteral("symbols_PCAD_SCH.lia"));
        QVERIFY(exporter.exportSymbolLibrary({symbol}, QStringLiteral("Symbols"), path, false, false));
        QVERIFY2(exporter.diagnostics().size() == 1, qPrintable(exporter.diagnostics().join('\n')));

        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QByteArray data = file.readAll();
        QVERIFY(data.contains("(symbolDef \"QFN_4\""));
        QVERIFY(data.contains("(compDef \"QFN_4\""));
        QVERIFY(data.contains("(pin \"1\""));
        QVERIFY(data.contains("(compPin \"1\" \"IN\""));
        QVERIFY(data.contains("(attachedSymbol (partNum 1)"));
        QVERIFY(data.contains("(attachedPattern (patternNum 1) (patternName \"QFN_4\"))"));
    }

    /** @brief 验证不支持的符号曲线不会静默退化为折线。 */
    void rejectsUnsupportedSymbolGeometry() {
        IR::SymbolComponentIR symbol;
        symbol.name = QStringLiteral("ARC_SYMBOL");
        symbol.arcs.append(IR::SymbolArcIR{});

        QTemporaryDir directory;
        ExporterPcadSymbol exporter;
        QVERIFY(!exporter.exportSymbol(symbol, directory.filePath(QStringLiteral("arc.lia"))));
        QVERIFY(exporter.diagnostics().join('\n').contains(QStringLiteral("无法无损表达")));
    }
};

QTEST_GUILESS_MAIN(TestPcadExporter)
#include "test_pcad_exporter.moc"
