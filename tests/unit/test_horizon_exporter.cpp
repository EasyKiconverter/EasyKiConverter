#include "core/horizon/ExporterHorizonLibrary.h"
#include "core/horizon/HorizonUnits.h"
#include "core/horizon/HorizonUuid.h"

#include <QDirIterator>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest>

using namespace EasyKiConverter;

class TestHorizonExporter final : public QObject {
    Q_OBJECT

private slots:
    void writesCompletePoolAndReferences();
    void outputIsDeterministic();
    void rejectsMissingPinPadAssociation();
    void rejectsDuplicatePadNumber();
    void rejectsSanitizedSymbolNameCollision();
    void rejectsSanitizedFootprintNameCollision();
    void rejectsSanitizedComponentNameCollision();
    void keepsPinReferencesWhenComponentAndSymbolNamesDiffer();
    void convertsCommonGeometryWithExplicitApproximationDiagnostics();
    void writesEmbeddedModelAndPlacement();
    void keepsUnitsAndUuidDeterministic();
    void writesMultipartGatesAndPins();
};

static IR::ComponentIR fixture() {
    IR::ComponentIR component;
    component.name = QStringLiteral("R10K");
    component.prefix = QStringLiteral("R");
    component.manufacturer = QStringLiteral("Example");
    component.manufacturerPart = QStringLiteral("R10K-0603");
    component.symbol.name = QStringLiteral("R10K");
    component.symbol.designatorPrefix = QStringLiteral("R");
    for (const auto& entry :
         {qMakePair(QStringLiteral("1"), QPointF(-2.0, 0.0)), qMakePair(QStringLiteral("2"), QPointF(2.0, 0.0))}) {
        IR::SymbolPinIR pin;
        pin.designator = entry.first;
        pin.name = entry.first;
        pin.position = entry.second;
        pin.length = 1.0;
        pin.direction = IR::PinDirection::Left;
        pin.electricalType = IR::PinElectricalType::Passive;
        component.symbol.pins.append(pin);
    }
    component.symbol.rectangles.append({-1.0, -1.0, 1.0, 1.0});
    component.footprint.name = QStringLiteral("R0603");
    for (const auto& entry :
         {qMakePair(QStringLiteral("1"), QPointF(-0.75, 0.0)), qMakePair(QStringLiteral("2"), QPointF(0.75, 0.0))}) {
        IR::FootprintPadIR pad;
        pad.number = entry.first;
        pad.position = entry.second;
        pad.shape = IR::PadShape::Rect;
        pad.size = QSizeF(0.8, 0.9);
        component.footprint.pads.append(pad);
    }
    component.footprint.outlines.append(
        {{QPointF(-1.2, -0.6), QPointF(1.2, -0.6), QPointF(1.2, 0.6), QPointF(-1.2, 0.6), QPointF(-1.2, -0.6)},
         0.1,
         IR::LayerType::TopSilk});
    return component;
}

static QByteArray read(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return file.readAll();
}

static QStringList relativeFiles(const QString& root) {
    QStringList result;
    QDirIterator it(root, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext())
        result.append(QDir(root).relativeFilePath(it.next()));
    result.sort();
    return result;
}

void TestHorizonExporter::writesCompletePoolAndReferences() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    ExporterHorizonLibrary exporter;
    const QString root = QDir(temporary.path()).filePath(QStringLiteral("pool"));
    QVERIFY2(exporter.exportComponentLibrary({fixture()}, QStringLiteral("fixture"), root),
             qPrintable(exporter.diagnostics().join('\n')));
    QVERIFY(QFileInfo::exists(QDir(root).filePath(QStringLiteral("pool.json"))));
    const QString unitPath = QDir(root).filePath(QStringLiteral("units/R10K-1.json"));
    const QString symbolPath = QDir(root).filePath(QStringLiteral("symbols/R10K-1.json"));
    const QString entityPath = QDir(root).filePath(QStringLiteral("entities/R10K.json"));
    const QString packagePath = QDir(root).filePath(QStringLiteral("packages/R0603/package.json"));
    const QString partPath = QDir(root).filePath(QStringLiteral("parts/R10K.json"));
    QVERIFY(QFileInfo::exists(unitPath));
    QVERIFY(QFileInfo::exists(symbolPath));
    QVERIFY(QFileInfo::exists(entityPath));
    QVERIFY(QFileInfo::exists(packagePath));
    QVERIFY(QFileInfo::exists(partPath));

    const QJsonObject unit = QJsonDocument::fromJson(read(unitPath)).object();
    const QJsonObject symbol = QJsonDocument::fromJson(read(symbolPath)).object();
    const QJsonObject entity = QJsonDocument::fromJson(read(entityPath)).object();
    const QJsonObject package = QJsonDocument::fromJson(read(packagePath)).object();
    const QJsonObject part = QJsonDocument::fromJson(read(partPath)).object();
    QCOMPARE(unit.value(QStringLiteral("type")).toString(), QStringLiteral("unit"));
    QCOMPARE(unit.value(QStringLiteral("pins")).toObject().size(), 2);
    QCOMPARE(symbol.value(QStringLiteral("unit")).toString(), unit.value(QStringLiteral("uuid")).toString());
    QCOMPARE(entity.value(QStringLiteral("gates")).toObject().size(), 1);
    const QJsonObject gate = entity.value(QStringLiteral("gates")).toObject().constBegin().value().toObject();
    QCOMPARE(gate.value(QStringLiteral("unit")).toString(), unit.value(QStringLiteral("uuid")).toString());
    QCOMPARE(part.value(QStringLiteral("entity")).toString(), entity.value(QStringLiteral("uuid")).toString());
    QCOMPARE(part.value(QStringLiteral("package")).toString(), package.value(QStringLiteral("uuid")).toString());
    QCOMPARE(package.value(QStringLiteral("pads")).toObject().size(), 2);
    QCOMPARE(part.value(QStringLiteral("pad_map")).toObject().size(), 2);
    for (const QString& padUuid : part.value(QStringLiteral("pad_map")).toObject().keys()) {
        QVERIFY(package.value(QStringLiteral("pads")).toObject().contains(padUuid));
        const QJsonObject mapping = part.value(QStringLiteral("pad_map")).toObject().value(padUuid).toObject();
        QVERIFY(!mapping.value(QStringLiteral("gate")).toString().isEmpty());
        QVERIFY(
            unit.value(QStringLiteral("pins")).toObject().contains(mapping.value(QStringLiteral("pin")).toString()));
    }
}

void TestHorizonExporter::outputIsDeterministic() {
    QTemporaryDir first;
    QTemporaryDir second;
    QVERIFY(first.isValid());
    QVERIFY(second.isValid());
    ExporterHorizonLibrary left;
    ExporterHorizonLibrary right;
    QVERIFY(left.exportComponentLibrary({fixture()}, QStringLiteral("fixture"), QDir(first.path()).filePath("pool")));
    QVERIFY(right.exportComponentLibrary({fixture()}, QStringLiteral("fixture"), QDir(second.path()).filePath("pool")));
    const QStringList files = relativeFiles(QDir(first.path()).filePath("pool"));
    QCOMPARE(files, relativeFiles(QDir(second.path()).filePath("pool")));
    for (const QString& file : files)
        QCOMPARE(read(QDir(first.path()).filePath(QStringLiteral("pool/") + file)),
                 read(QDir(second.path()).filePath(QStringLiteral("pool/") + file)));
}

void TestHorizonExporter::rejectsMissingPinPadAssociation() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    IR::ComponentIR component = fixture();
    component.footprint.pads[1].number = QStringLiteral("3");
    ExporterHorizonLibrary exporter;
    QVERIFY(!exporter.exportComponentLibrary(
        {component}, QStringLiteral("missing"), QDir(temporary.path()).filePath("pool")));
    QVERIFY(exporter.diagnostics().join('\n').contains(QStringLiteral("找不到符号引脚")));
}

void TestHorizonExporter::rejectsDuplicatePadNumber() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    IR::ComponentIR component = fixture();
    component.footprint.pads[1].number = QStringLiteral("1");
    ExporterHorizonLibrary exporter;
    QVERIFY(!exporter.exportComponentLibrary(
        {component}, QStringLiteral("duplicate"), QDir(temporary.path()).filePath("pool")));
    QVERIFY(exporter.diagnostics().join('\n').contains(QStringLiteral("重复焊盘编号")));
}

void TestHorizonExporter::rejectsSanitizedSymbolNameCollision() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const IR::SymbolComponentIR source = fixture().symbol;
    IR::SymbolComponentIR first = source;
    IR::SymbolComponentIR second = source;
    first.name = QStringLiteral("A/B");
    second.name = QStringLiteral("A:B");
    ExporterHorizonLibrary exporter;
    const QString root = QDir(temporary.path()).filePath(QStringLiteral("pool"));
    QVERIFY(!exporter.exportSymbolLibrary({first, second}, QStringLiteral("collision"), root));
    QVERIFY(exporter.diagnostics().join('\n').contains(QStringLiteral("名称清洗后输出路径冲突")));
    QVERIFY(!QFileInfo::exists(QDir(root).filePath(QStringLiteral("pool.json"))));
}

void TestHorizonExporter::rejectsSanitizedFootprintNameCollision() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const IR::FootprintComponentIR source = fixture().footprint;
    IR::FootprintComponentIR first = source;
    IR::FootprintComponentIR second = source;
    first.name = QStringLiteral("A/B");
    second.name = QStringLiteral("A:B");
    ExporterHorizonLibrary exporter;
    const QString root = QDir(temporary.path()).filePath(QStringLiteral("pool"));
    QVERIFY(!exporter.exportFootprintLibrary({first, second}, QStringLiteral("collision"), root));
    QVERIFY(exporter.diagnostics().join('\n').contains(QStringLiteral("名称清洗后输出路径冲突")));
    QVERIFY(!QFileInfo::exists(QDir(root).filePath(QStringLiteral("pool.json"))));
}

void TestHorizonExporter::rejectsSanitizedComponentNameCollision() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    IR::ComponentIR first = fixture();
    IR::ComponentIR second = fixture();
    first.name = QStringLiteral("A/B");
    second.name = QStringLiteral("A:B");
    ExporterHorizonLibrary exporter;
    const QString root = QDir(temporary.path()).filePath(QStringLiteral("pool"));
    QVERIFY(!exporter.exportComponentLibrary({first, second}, QStringLiteral("collision"), root));
    QVERIFY(exporter.diagnostics().join('\n').contains(QStringLiteral("名称清洗后输出路径冲突")));
    QVERIFY(!QFileInfo::exists(QDir(root).filePath(QStringLiteral("pool.json"))));
}

void TestHorizonExporter::keepsPinReferencesWhenComponentAndSymbolNamesDiffer() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    IR::ComponentIR component = fixture();
    component.name = QStringLiteral("R10K_COMPONENT");
    ExporterHorizonLibrary exporter;
    const QString root = QDir(temporary.path()).filePath(QStringLiteral("pool"));
    QVERIFY2(exporter.exportComponentLibrary({component}, QStringLiteral("fixture"), root),
             qPrintable(exporter.diagnostics().join('\n')));
    const QJsonObject unit =
        QJsonDocument::fromJson(read(QDir(root).filePath(QStringLiteral("units/R10K_COMPONENT-1.json")))).object();
    const QJsonObject part =
        QJsonDocument::fromJson(read(QDir(root).filePath(QStringLiteral("parts/R10K_COMPONENT.json")))).object();
    for (const QJsonValue& mappingValue : part.value(QStringLiteral("pad_map")).toObject()) {
        const QString pinUuid = mappingValue.toObject().value(QStringLiteral("pin")).toString();
        QVERIFY(unit.value(QStringLiteral("pins")).toObject().contains(pinUuid));
    }
}

void TestHorizonExporter::convertsCommonGeometryWithExplicitApproximationDiagnostics() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    IR::ComponentIR component = fixture();
    IR::SymbolCircleIR symbolCircle;
    symbolCircle.center = QPointF(0.0, 0.0);
    symbolCircle.radius = 1.0;
    component.symbol.circles.append(symbolCircle);
    IR::FootprintCircleIR footprintCircle;
    footprintCircle.center = QPointF(0.0, 0.0);
    footprintCircle.radius = 1.5;
    component.footprint.circles.append(footprintCircle);
    IR::FootprintTextIR footprintText;
    footprintText.text = QStringLiteral("REF");
    footprintText.position = QPointF(0.0, 0.0);
    component.footprint.texts.append(footprintText);

    ExporterHorizonLibrary exporter;
    const QString root = QDir(temporary.path()).filePath(QStringLiteral("pool"));
    QVERIFY2(exporter.exportComponentLibrary({component}, QStringLiteral("fixture"), root),
             qPrintable(exporter.diagnostics().join('\n')));
    const QJsonObject symbol =
        QJsonDocument::fromJson(read(QDir(root).filePath(QStringLiteral("symbols/R10K-1.json")))).object();
    const QJsonObject package =
        QJsonDocument::fromJson(read(QDir(root).filePath(QStringLiteral("packages/R0603/package.json")))).object();
    QVERIFY(!symbol.value(QStringLiteral("polygons")).toObject().isEmpty());
    QVERIFY(!package.value(QStringLiteral("polygons")).toObject().isEmpty());
    QVERIFY(!package.value(QStringLiteral("texts")).toObject().isEmpty());
    QVERIFY(exporter.diagnostics().join('\n').contains(QStringLiteral("近似")));
}

void TestHorizonExporter::writesEmbeddedModelAndPlacement() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    IR::ComponentIR component = fixture();
    IR::Model3DIR model;
    model.setName(QStringLiteral("body.obj"));
    model.setRawObj(QStringLiteral("v 0 0 0\n"));
    model.setTranslation({1.0, 2.0, 3.0});
    model.setStepOffsetMm({0.1, 0.2, 0.3});
    model.setRotation({10.0, 20.0, 30.0});
    component.footprint.models3d.append(model);

    ExporterHorizonLibrary exporter;
    const QString root = QDir(temporary.path()).filePath(QStringLiteral("pool"));
    QVERIFY2(exporter.exportComponentLibrary({component}, QStringLiteral("fixture"), root, true),
             qPrintable(exporter.diagnostics().join('\n')));
    const QJsonObject package =
        QJsonDocument::fromJson(read(QDir(root).filePath(QStringLiteral("packages/R0603/package.json")))).object();
    const QJsonObject models = package.value(QStringLiteral("models")).toObject();
    QCOMPARE(models.size(), 1);
    const QJsonObject modelObject = models.constBegin().value().toObject();
    QCOMPARE(modelObject.value(QStringLiteral("x")).toInteger(), 1100000);
    QCOMPARE(modelObject.value(QStringLiteral("y")).toInteger(), 2200000);
    QCOMPARE(modelObject.value(QStringLiteral("z")).toInteger(), 3300000);
    QVERIFY(QFileInfo::exists(QDir(root).filePath(modelObject.value(QStringLiteral("filename")).toString())));
}

void TestHorizonExporter::keepsUnitsAndUuidDeterministic() {
    QCOMPARE(HorizonUnits::mm(1.0), qint64(1000000));
    QCOMPARE(HorizonUnits::mm(-0.000001), qint64(-1));
    QCOMPARE(HorizonUnits::mm(1.2345678), qint64(1234568));
    const QString first = HorizonUuid::make(QStringLiteral("unit"), QStringLiteral("R10K"));
    QCOMPARE(first, HorizonUuid::make(QStringLiteral("unit"), QStringLiteral("R10K")));
    QVERIFY(first != HorizonUuid::make(QStringLiteral("unit"), QStringLiteral("C10K")));
    QVERIFY(QUuid(first).isNull() == false);
}

void TestHorizonExporter::writesMultipartGatesAndPins() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    IR::ComponentIR component = fixture();
    component.symbol.partCount = 2;
    component.symbol.pins[0].partIndex = 0;
    component.symbol.pins[1].partIndex = 1;
    ExporterHorizonLibrary exporter;
    const QString root = QDir(temporary.path()).filePath(QStringLiteral("pool"));
    QVERIFY2(exporter.exportComponentLibrary({component}, QStringLiteral("multipart"), root),
             qPrintable(exporter.diagnostics().join('\n')));
    const QJsonObject entity =
        QJsonDocument::fromJson(read(QDir(root).filePath(QStringLiteral("entities/R10K.json")))).object();
    const QJsonObject part =
        QJsonDocument::fromJson(read(QDir(root).filePath(QStringLiteral("parts/R10K.json")))).object();
    QCOMPARE(entity.value(QStringLiteral("gates")).toObject().size(), 2);
    QCOMPARE(QJsonDocument::fromJson(read(QDir(root).filePath(QStringLiteral("units/R10K-1.json"))))
                 .object()
                 .value(QStringLiteral("pins"))
                 .toObject()
                 .size(),
             1);
    QCOMPARE(QJsonDocument::fromJson(read(QDir(root).filePath(QStringLiteral("units/R10K-2.json"))))
                 .object()
                 .value(QStringLiteral("pins"))
                 .toObject()
                 .size(),
             1);
    const QJsonObject padMap = part.value(QStringLiteral("pad_map")).toObject();
    QCOMPARE(padMap.size(), 2);
    auto firstMapping = padMap.constBegin();
    auto secondMapping = firstMapping;
    ++secondMapping;
    QVERIFY(firstMapping.value().toObject().value(QStringLiteral("gate")).toString() !=
            secondMapping.value().toObject().value(QStringLiteral("gate")).toString());
}

QTEST_GUILESS_MAIN(TestHorizonExporter)
#include "test_horizon_exporter.moc"
