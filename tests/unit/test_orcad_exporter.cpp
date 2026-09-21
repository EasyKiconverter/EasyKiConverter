#include "core/orcad/ExporterOrcadSymbol.h"

#include <QFile>
#include <QTemporaryDir>
#include <QXmlStreamReader>
#include <QtTest>

using namespace EasyKiConverter;

class TestOrcadExporter final : public QObject {
    Q_OBJECT

private slots:
    /** @brief 验证 OrCAD Capture XML 输出包含符号、封装名称和引脚关联。 */
    void writesCaptureXmlLibrary();
    /** @brief 验证 XML writer 不会静默丢弃当前不支持的圆形图元。 */
    void rejectsUnsupportedGraphics();
    /** @brief 验证重复符号名称不会生成歧义定义。 */
    void rejectsDuplicateNames();
    /** @brief 验证追加和更新请求不会被静默当作完整重写。 */
    void rejectsMergeModes();
};

static IR::SymbolComponentIR makeSymbol(const QString& name) {
    IR::SymbolComponentIR symbol;
    symbol.name = name;
    symbol.description = QStringLiteral("OrCAD XML fixture");
    symbol.designatorPrefix = QStringLiteral("U");
    symbol.footprintName = QStringLiteral("QFN-4");
    symbol.rectangles.append({-1.27, -1.27, 1.27, 1.27});

    IR::SymbolPinIR pin;
    pin.name = QStringLiteral("IN");
    pin.designator = QStringLiteral("1");
    pin.position = QPointF(-1.27, 0.0);
    pin.length = 1.27;
    pin.direction = IR::PinDirection::Left;
    pin.electricalType = IR::PinElectricalType::Input;
    symbol.pins.append(pin);
    return symbol;
}

/** @brief 验证 XML 回读能发现符号、封装名称和引脚关联。 */
void TestOrcadExporter::writesCaptureXmlLibrary() {
    // 回读完整 XML，确认符号、封装名称和电气引脚关联均已写入。
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    ExporterOrcadSymbol exporter;
    const QString path = temporary.filePath(QStringLiteral("library.xml"));
    QVERIFY(
        exporter.exportSymbolLibrary({makeSymbol(QStringLiteral("MCU_A"))}, QStringLiteral("library"), path, false));

    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QXmlStreamReader reader(&file);
    bool packageSeen = false;
    bool pinSeen = false;
    bool physicalPinSeen = false;
    QString packageFootprint;
    while (!reader.atEnd()) {
        reader.readNext();
        if (!reader.isStartElement())
            continue;
        if (reader.name() == QStringLiteral("Package"))
            packageSeen = true;
        if (reader.name() == QStringLiteral("Defn") &&
            reader.attributes().hasAttribute(QStringLiteral("pcbFootprint"))) {
            packageFootprint = reader.attributes().value(QStringLiteral("pcbFootprint")).toString();
        }
        pinSeen = pinSeen || reader.name() == QStringLiteral("SymbolPinScalar");
        physicalPinSeen = physicalPinSeen || reader.name() == QStringLiteral("PinNumber");
    }
    QVERIFY2(!reader.hasError(), qPrintable(reader.errorString()));
    QVERIFY(packageSeen);
    QVERIFY(pinSeen);
    QVERIFY(physicalPinSeen);
    QCOMPARE(packageFootprint, QStringLiteral("QFN-4"));
}

/** @brief 验证未实现图元会被拒绝并产生诊断。 */
void TestOrcadExporter::rejectsUnsupportedGraphics() {
    // 不支持的图元必须失败，避免生成看似成功但缺少几何的符号。
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    ExporterOrcadSymbol exporter;
    IR::SymbolComponentIR symbol = makeSymbol(QStringLiteral("CircleSymbol"));
    symbol.circles.append({QPointF(0.0, 0.0), 1.0});
    QVERIFY(
        !exporter.exportSymbolLibrary({symbol}, QStringLiteral("library"), temporary.filePath("circle.xml"), false));
    QVERIFY(exporter.diagnostics().join(QStringLiteral("\n")).contains(QStringLiteral("未实现的图元")));
}

/** @brief 验证大小写折叠后的重复符号名称会被拒绝。 */
void TestOrcadExporter::rejectsDuplicateNames() {
    // 名称比较使用大小写折叠，防止目标库出现难以区分的重复定义。
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    ExporterOrcadSymbol exporter;
    QVERIFY(!exporter.exportSymbolLibrary({makeSymbol(QStringLiteral("A")), makeSymbol(QStringLiteral("a"))},
                                          QStringLiteral("library"),
                                          temporary.filePath("duplicate.xml"),
                                          false));
    QVERIFY(exporter.diagnostics().join(QStringLiteral("\n")).contains(QStringLiteral("名称冲突")));
}

/** @brief 验证追加和更新请求都会返回明确的不支持诊断。 */
void TestOrcadExporter::rejectsMergeModes() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    ExporterOrcadSymbol exporter;
    const QString appendPath = temporary.filePath(QStringLiteral("append.xml"));
    QVERIFY(!exporter.exportSymbolLibrary(
        {makeSymbol(QStringLiteral("Append"))}, QStringLiteral("library"), appendPath, true, false));
    QVERIFY(exporter.diagnostics().join(QStringLiteral("\n")).contains(QStringLiteral("不支持追加或更新")));

    const QString updatePath = temporary.filePath(QStringLiteral("update.xml"));
    QVERIFY(!exporter.exportSymbolLibrary(
        {makeSymbol(QStringLiteral("Update"))}, QStringLiteral("library"), updatePath, false, true));
    QVERIFY(exporter.diagnostics().join(QStringLiteral("\n")).contains(QStringLiteral("不支持追加或更新")));
}

QTEST_MAIN(TestOrcadExporter)

#include "test_orcad_exporter.moc"
