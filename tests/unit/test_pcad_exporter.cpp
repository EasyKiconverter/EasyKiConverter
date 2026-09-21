#include "core/parser/PcadModel.h"
#include "core/pcad/ExporterPcadFootprint.h"

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
};

QTEST_GUILESS_MAIN(TestPcadExporter)
#include "test_pcad_exporter.moc"
