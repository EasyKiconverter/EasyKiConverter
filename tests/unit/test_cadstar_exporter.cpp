#include "core/cadstar/ExporterCadstarLibrary.h"
#include "core/parser/CadstarModel.h"

#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

using namespace EasyKiConverter;

class TestCadstarExporter final : public QObject {
    Q_OBJECT

private slots:
    /** @brief 验证完整 CADSTAR ASCII 库包含符号、封装、焊盘和 Part 关联。 */
    void writesAndReadsCompleteLibrary();

    /** @brief 验证仅符号导出不会生成缺少封装的 Part 关联。 */
    void writesSymbolOnlyLibrary();

    /** @brief 验证 CADSTAR writer 不会静默退化不支持的焊盘形状。 */
    void rejectsUnsupportedPadShape();
};

/** @brief 验证完整库可以被项目解析器回读并保持关键关联。 */
void TestCadstarExporter::writesAndReadsCompleteLibrary() {
    IR::ComponentIR component;
    component.name = QStringLiteral("C2040");
    component.description = QStringLiteral("CADSTAR round trip");
    component.symbol.name = QStringLiteral("C2040_SYMBOL");
    component.symbol.designatorPrefix = QStringLiteral("U");
    component.symbol.rectangles.append({-2.0, -1.0, 2.0, 1.0});
    IR::SymbolPinIR symbolPin;
    symbolPin.designator = QStringLiteral("1");
    symbolPin.name = QStringLiteral("VDD");
    symbolPin.position = QPointF(-2.0, 0.0);
    symbolPin.length = 1.0;
    symbolPin.direction = IR::PinDirection::Left;
    symbolPin.electricalType = IR::PinElectricalType::Power;
    component.symbol.pins.append(symbolPin);
    component.footprint.name = QStringLiteral("QFN-24");
    component.footprint.description = QStringLiteral("QFN fixture");

    IR::FootprintPadIR pad;
    pad.number = QStringLiteral("1");
    pad.position = QPointF(-1.0, -1.0);
    pad.shape = IR::PadShape::Rect;
    pad.size = QSizeF(0.8, 1.2);
    component.footprint.pads.append(pad);
    component.footprint.circles.append({QPointF(0.0, 0.0), 2.0, 0.15});

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("library.lib"));
    ExporterCadstarLibrary exporter;
    QVERIFY(exporter.exportComponentLibrary({component}, QStringLiteral("Library"), path));
    QVERIFY2(exporter.diagnostics().isEmpty(), qPrintable(exporter.diagnostics().join('\n')));

    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    const Parser::CadstarLibrary parsed = Parser::CadstarParser::parseBytes(file.readAll(), path);
    QCOMPARE(parsed.components.size(), 1);
    QCOMPARE(parsed.packages.size(), 1);
    QCOMPARE(parsed.parts.size(), 1);
    QCOMPARE(parsed.pads.size(), 1);
    QCOMPARE(parsed.parts.first().componentName, QStringLiteral("C2040_SYMBOL"));
    QCOMPARE(parsed.parts.first().packageName, QStringLiteral("QFN-24"));
    QVERIFY(!parsed.diagnostics.hasErrors());
}

/** 验证 CADSTAR 符号单独导出保留 Component 定义并省略 Package 与 Part。 */
void TestCadstarExporter::writesSymbolOnlyLibrary() {
    IR::SymbolComponentIR symbol;
    symbol.name = QStringLiteral("U_SYMBOL_ONLY");
    symbol.designatorPrefix = QStringLiteral("U");
    symbol.rectangles.append({-1.0, -1.0, 1.0, 1.0});
    IR::SymbolPinIR pin;
    pin.designator = QStringLiteral("1");
    pin.name = QStringLiteral("IN");
    pin.position = QPointF(-2.0, 0.0);
    pin.length = 1.0;
    pin.direction = IR::PinDirection::Left;
    symbol.pins.append(pin);

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("symbols.lib"));
    ExporterCadstarLibrary exporter;
    QVERIFY(exporter.exportSymbolLibrary({symbol}, QStringLiteral("Symbols"), path));

    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QByteArray content = file.readAll();
    QVERIFY(content.contains("COMPONENT \"U_SYMBOL_ONLY\""));
    QVERIFY(!content.contains("PACKAGE "));
    QVERIFY(!content.contains("PART "));
}

/** @brief 验证不支持的焊盘形状会失败并生成明确诊断。 */
void TestCadstarExporter::rejectsUnsupportedPadShape() {
    IR::FootprintComponentIR footprint;
    footprint.name = QStringLiteral("BAD_PAD");
    IR::FootprintPadIR pad;
    pad.number = QStringLiteral("1");
    pad.shape = IR::PadShape::RoundRect;
    pad.size = QSizeF(1.0, 1.0);
    footprint.pads.append(pad);

    QTemporaryDir directory;
    ExporterCadstarLibrary exporter;
    QVERIFY(!exporter.exportFootprint(footprint, directory.filePath(QStringLiteral("bad.lib"))));
    QVERIFY(exporter.diagnostics().join('\n').contains(QStringLiteral("禁止静默降级")));
}

QTEST_GUILESS_MAIN(TestCadstarExporter)
#include "test_cadstar_exporter.moc"
