#include "core/eagle/ExporterEagleFootprint.h"

#include <QFile>
#include <QTemporaryDir>
#include <QXmlStreamReader>
#include <QtTest>

using namespace EasyKiConverter;

class TestEagleExporter final : public QObject {
    Q_OBJECT

private slots:
    /** @brief 验证 Eagle XML library 包含 package、Pad 和独立机械孔。 */
    void writesXmlLibrary();
    /** @brief 验证清洗名称冲突会阻止生成歧义 package。 */
    void rejectsNameCollision();
    /** @brief 验证完整 Eagle XML 库包含符号、器件集和引脚映射。 */
    void writesCompleteComponentLibrary();
    /** @brief 验证缺失封装焊盘时拒绝生成无效器件关联。 */
    void rejectsMissingPadMapping();
};

static IR::FootprintComponentIR makeFixture(const QString& name) {
    IR::FootprintComponentIR footprint;
    footprint.name = name;
    footprint.description = QStringLiteral("Eagle XML fixture");

    IR::FootprintPadIR smd;
    smd.number = QStringLiteral("1");
    smd.position = QPointF(-1.0, 0.0);
    smd.shape = IR::PadShape::RoundRect;
    smd.size = QSizeF(1.2, 0.8);
    footprint.pads.append(smd);

    IR::FootprintPadIR through;
    through.number = QStringLiteral("2");
    through.position = QPointF(1.0, 0.0);
    through.padType = IR::PadType::ThroughHole;
    through.shape = IR::PadShape::Ellipse;
    through.size = QSizeF(1.8, 1.8);
    through.holeSize = 0.9;
    footprint.pads.append(through);
    footprint.holes.append({QPointF(0.0, 2.0), 0.4, false});
    footprint.circles.append({QPointF(0.0, 0.0), 2.0, 0.15, IR::LayerType::TopSilk, false});
    return footprint;
}

static IR::ComponentIR makeComponentFixture() {
    IR::ComponentIR component;
    component.name = QStringLiteral("R_10K");
    component.description = QStringLiteral("完整 Eagle 组件测试");
    component.prefix = QStringLiteral("R");
    component.symbol.name = QStringLiteral("R_10K_SYMBOL");
    component.symbol.designatorPrefix = QStringLiteral("R");
    component.symbol.description = component.description;
    component.symbol.rectangles.append({-1.27, -2.54, 1.27, 2.54, 0.15});
    IR::SymbolPinIR pin;
    pin.name = QStringLiteral("1");
    pin.designator = QStringLiteral("1");
    pin.position = QPointF(-3.81, 0.0);
    pin.direction = IR::PinDirection::Left;
    pin.partIndex = 0;
    component.symbol.pins.append(pin);
    component.footprint = makeFixture(QStringLiteral("R_10K_PACKAGE"));
    component.footprint.pads.removeLast();
    component.footprint.pads.first().number = QStringLiteral("1");
    return component;
}

/** 验证 Eagle XML library 可解析且包含基础封装元素。 */
void TestEagleExporter::writesXmlLibrary() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    ExporterEagleFootprint exporter;
    const QString path = temporary.path() + QStringLiteral("/library.lbr");
    QVERIFY(exporter.exportFootprintLibrary({makeFixture(QStringLiteral("QFN 4"))}, QStringLiteral("library"), path));

    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QXmlStreamReader reader(&file);
    bool packageSeen = false;
    bool smdSeen = false;
    bool padSeen = false;
    bool holeSeen = false;
    while (!reader.atEnd()) {
        reader.readNext();
        if (!reader.isStartElement())
            continue;
        if (reader.name() == QStringLiteral("package"))
            packageSeen = true;
        if (reader.name() == QStringLiteral("smd"))
            smdSeen = true;
        if (reader.name() == QStringLiteral("pad"))
            padSeen = true;
        if (reader.name() == QStringLiteral("hole"))
            holeSeen = true;
    }
    QVERIFY2(!reader.hasError(), qPrintable(reader.errorString()));
    QVERIFY(packageSeen);
    QVERIFY(smdSeen);
    QVERIFY(padSeen);
    QVERIFY(holeSeen);
}

/** 验证不同原名清洗为同一 package 名称时会失败。 */
void TestEagleExporter::rejectsNameCollision() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    ExporterEagleFootprint exporter;
    QVERIFY(!exporter.exportFootprintLibrary({makeFixture(QStringLiteral("A B")), makeFixture(QStringLiteral("A/B"))},
                                             QStringLiteral("library"),
                                             temporary.path() + QStringLiteral("/library.lbr")));
    QVERIFY(exporter.diagnostics().join(QStringLiteral("\n")).contains(QStringLiteral("冲突")));
}

/** 验证完整 Eagle XML library 可被回读并保留组件关联关系。 */
void TestEagleExporter::writesCompleteComponentLibrary() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    ExporterEagleFootprint exporter;
    const QString path = temporary.path() + QStringLiteral("/complete.lbr");
    QVERIFY(exporter.exportComponentLibrary({makeComponentFixture()}, QStringLiteral("complete"), path));

    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QXmlStreamReader reader(&file);
    bool symbolSeen = false;
    bool devicesetSeen = false;
    bool gateSeen = false;
    bool deviceSeen = false;
    bool connectSeen = false;
    while (!reader.atEnd()) {
        reader.readNext();
        if (!reader.isStartElement())
            continue;
        symbolSeen = symbolSeen || reader.name() == QStringLiteral("symbol");
        devicesetSeen = devicesetSeen || reader.name() == QStringLiteral("deviceset");
        gateSeen = gateSeen || reader.name() == QStringLiteral("gate");
        deviceSeen = deviceSeen || reader.name() == QStringLiteral("device");
        connectSeen = connectSeen || reader.name() == QStringLiteral("connect");
    }
    QVERIFY2(!reader.hasError(), qPrintable(reader.errorString()));
    QVERIFY(symbolSeen);
    QVERIFY(devicesetSeen);
    QVERIFY(gateSeen);
    QVERIFY(deviceSeen);
    QVERIFY(connectSeen);
}

/** 验证 pin-to-pad 关联缺失时导出失败且报告原因。 */
void TestEagleExporter::rejectsMissingPadMapping() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    ExporterEagleFootprint exporter;
    IR::ComponentIR component = makeComponentFixture();
    component.symbol.pins.first().designator = QStringLiteral("99");
    QVERIFY(!exporter.exportComponentLibrary(
        {component}, QStringLiteral("invalid"), temporary.path() + QStringLiteral("/invalid.lbr")));
    QVERIFY(exporter.diagnostics().join(QStringLiteral("\n")).contains(QStringLiteral("找不到对应焊盘")));
}

QTEST_MAIN(TestEagleExporter)
#include "test_eagle_exporter.moc"
