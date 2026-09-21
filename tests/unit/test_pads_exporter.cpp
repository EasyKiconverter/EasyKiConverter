#include "core/pads/ExporterPadsFootprint.h"
#include "core/pads/ExporterPadsSymbol.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

using namespace EasyKiConverter;

class TestPadsExporter final : public QObject {
    Q_OBJECT

private slots:
    /** @brief 验证基本 PADS Decal 文件包含图元、焊盘栈和通孔层记录。 */
    void writesAsciiDecal();
    /** @brief 验证清洗后的封装名称冲突会阻止覆盖错误文件。 */
    void rejectsSanitizedNameCollision();
    /** @brief 验证无法无损表达的形状和机械孔不会被静默丢失。 */
    void rejectsUnsupportedGeometry();
    /** @brief 验证 PADS Schematic Decal 符号文件的引脚、图元和结束记录。 */
    void writesSchematicDecal();
    /** @brief 验证 PADS 符号的追加和更新模式被明确拒绝。 */
    void rejectsSymbolMergeModes();
};

static IR::FootprintComponentIR makeFixture(const QString& name) {
    IR::FootprintComponentIR footprint;
    footprint.name = name;

    IR::FootprintCircleIR circle;
    circle.center = QPointF(0.0, 0.0);
    circle.radius = 2.0;
    circle.strokeWidth = 0.15;
    footprint.circles.append(circle);

    IR::FootprintPadIR smd;
    smd.number = QStringLiteral("1");
    smd.position = QPointF(-1.0, 0.0);
    smd.shape = IR::PadShape::Rect;
    smd.size = QSizeF(1.0, 0.6);
    footprint.pads.append(smd);

    IR::FootprintPadIR through;
    through.number = QStringLiteral("2");
    through.position = QPointF(1.0, 0.0);
    through.padType = IR::PadType::ThroughHole;
    through.shape = IR::PadShape::Ellipse;
    through.size = QSizeF(1.6, 1.6);
    through.holeSize = 0.8;
    through.isPlated = false;
    footprint.pads.append(through);
    return footprint;
}

/** 验证 PADS ASCII Decal 输出的基本结构和单位标识。 */
void TestPadsExporter::writesAsciiDecal() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());

    ExporterPadsFootprint exporter;
    QVERIFY(exporter.exportFootprintLibrary(
        {makeFixture(QStringLiteral("QFN 4"))}, QStringLiteral("qfn"), temporary.path()));

    QFile file(QDir(temporary.path()).filePath(QStringLiteral("QFN_4.d")));
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QString content = QString::fromUtf8(file.readAll());
    QVERIFY(content.startsWith(QStringLiteral("QFN_4 I 0 0")));
    QVERIFY(content.contains(QStringLiteral("TIMESTAMP ")));
    QVERIFY(content.contains(QStringLiteral("CIRCLE ")));
    QVERIFY(content.contains(QStringLiteral("PAD 1 1")));
    QVERIFY(content.contains(QStringLiteral("PAD 2 2")));
    QVERIFY(content.contains(QStringLiteral("-0 ")));
}

/** 验证两个不同原名清洗为同一 Decal 名称时会失败并返回诊断。 */
void TestPadsExporter::rejectsSanitizedNameCollision() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());

    ExporterPadsFootprint exporter;
    const QList<IR::FootprintComponentIR> footprints = {makeFixture(QStringLiteral("A B")),
                                                        makeFixture(QStringLiteral("A/B"))};
    QVERIFY(!exporter.exportFootprintLibrary(footprints, QStringLiteral("collision"), temporary.path()));
    QVERIFY(exporter.diagnostics().join(QStringLiteral("\n")).contains(QStringLiteral("冲突")));
}

/** 验证当前 PADS ASCII 后端拒绝无法无损表达的几何数据。 */
void TestPadsExporter::rejectsUnsupportedGeometry() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());

    IR::FootprintComponentIR footprint = makeFixture(QStringLiteral("unsupported"));
    footprint.pads[0].shape = IR::PadShape::RoundRect;
    footprint.holes.append({QPointF(0.0, 0.0), 0.5, false});

    ExporterPadsFootprint exporter;
    QVERIFY(!exporter.exportFootprintLibrary({footprint}, QStringLiteral("unsupported"), temporary.path()));
    QVERIFY(exporter.diagnostics().join(QStringLiteral("\n")).contains(QStringLiteral("机械孔")));
}

/** 验证符号库使用 PADS 规范头部并保留基本图元和引脚。 */
void TestPadsExporter::writesSchematicDecal() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());

    IR::SymbolComponentIR symbol;
    symbol.name = QStringLiteral("U_TEST");
    symbol.footprintName = QStringLiteral("QFN-24");
    symbol.partCount = 2;

    IR::SymbolRectangleIR rectangle;
    rectangle.x0 = -2.0;
    rectangle.y0 = -1.0;
    rectangle.x1 = 2.0;
    rectangle.y1 = 1.0;
    rectangle.strokeWidth = 0.1;
    symbol.rectangles.append(rectangle);

    IR::SymbolPinIR pin;
    pin.name = QStringLiteral("IN");
    pin.designator = QStringLiteral("1");
    pin.position = QPointF(-3.0, 0.0);
    pin.length = 1.0;
    pin.partIndex = 1;
    symbol.pins.append(pin);

    IR::SymbolTextIR text;
    text.text = QStringLiteral("VALUE");
    text.position = QPointF(0.0, 2.0);
    text.fontSizeMm = 1.0;
    symbol.texts.append(text);

    ExporterPadsSymbol exporter;
    const QString path = QDir(temporary.path()).filePath(QStringLiteral("library_PADS.c"));
    QVERIFY(exporter.exportSymbolLibrary({symbol}, QStringLiteral("library"), path, false, false));

    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QString content = QString::fromUtf8(file.readAll());
    QVERIFY(content.startsWith(QStringLiteral("*PADS-LIBRARY-SCH-DECALS-V9*")));
    QVERIFY(content.contains(QStringLiteral("U_TEST 0 0")));
    QVERIFY(content.contains(QStringLiteral("U_TEST_P2 0 0")));
    QVERIFY(content.contains(QStringLiteral("CLOSED 5")));
    QVERIFY(content.contains(QStringLiteral("T -118.110236 0")));
    QVERIFY(content.endsWith(QStringLiteral("*END*\n")));
    const auto companionFiles = exporter.companionFiles();
    QVERIFY(companionFiles.contains(QStringLiteral("library_PADS.p")));
    const QString partType = QString::fromUtf8(companionFiles.value(QStringLiteral("library_PADS.p")));
    QVERIFY(partType.startsWith(QStringLiteral("*PADS-LIBRARY-PART-TYPES-V9*")));
    QVERIFY(partType.contains(QStringLiteral("U_TEST QFN-24 I STD 0 2 0 0 0")));
    QVERIFY(partType.contains(QStringLiteral("GATE 1 1 0\nU_TEST_P2")));
    QVERIFY(partType.endsWith(QStringLiteral("*END*\n")));
}

/** 验证符号库不能把未经实现的追加或更新语义伪装成成功。 */
void TestPadsExporter::rejectsSymbolMergeModes() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    IR::SymbolComponentIR symbol;
    symbol.name = QStringLiteral("R_TEST");
    symbol.footprintName = QStringLiteral("R-0603");

    ExporterPadsSymbol exporter;
    const QString path = QDir(temporary.path()).filePath(QStringLiteral("library_PADS.c"));
    QVERIFY(!exporter.exportSymbolLibrary({symbol}, QStringLiteral("library"), path, true, false));
    QVERIFY(exporter.diagnostics().join(QStringLiteral("\n")).contains(QStringLiteral("追加")));
    QVERIFY(!exporter.exportSymbolLibrary({symbol}, QStringLiteral("library"), path, false, true));
    QVERIFY(exporter.diagnostics().join(QStringLiteral("\n")).contains(QStringLiteral("更新")));
}

QTEST_MAIN(TestPadsExporter)
#include "test_pads_exporter.moc"
