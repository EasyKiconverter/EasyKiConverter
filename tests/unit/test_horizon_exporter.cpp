#include "core/horizon/ExporterHorizonLibrary.h"
#include "core/horizon/HorizonPoolIntegration.h"
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
    void writesSymbolOnlyPoolDirectory();
    void outputIsDeterministic();
    void rejectsMissingPinPadAssociation();
    void rejectsDuplicatePadNumber();
    void rejectsSanitizedSymbolNameCollision();
    void rejectsSanitizedFootprintNameCollision();
    void rejectsSanitizedComponentNameCollision();
    void keepsPinReferencesWhenComponentAndSymbolNamesDiffer();
    void convertsCommonGeometryWithExplicitApproximationDiagnostics();
    void preservesRoundRectCustomOutline();
    void writesBottomCustomPadstackLayers();
    void rejectsInvalid3dModelData();
    void rejectsObjOnlyModel();
    void writesEmbeddedModelAndPlacement();
    void respectsFootprintModelExportOption();
    void keepsUnitsAndUuidDeterministic();
    void writesMultipartGatesAndPins();
    void writesPasteMountingHoleAndKeepoutSemantics();
    void rejectsPoolRegistrationWithoutOfficialPool();
    void invokesInjectedPoolIntegrationContract();
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

void TestHorizonExporter::writesSymbolOnlyPoolDirectory() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());

    ExporterHorizonLibrary exporter;
    QVERIFY(exporter.isDirectoryOutput());
    const QString root = QDir(temporary.path()).filePath(QStringLiteral("symbols.pool"));
    const IR::SymbolComponentIR symbol = fixture().symbol;
    QVERIFY2(exporter.exportSymbolLibrary({symbol}, QStringLiteral("symbols"), root, false, false),
             qPrintable(exporter.diagnostics().join('\n')));
    QVERIFY(QDir(root).exists());
    QVERIFY(QFileInfo::exists(QDir(root).filePath(QStringLiteral("pool.json"))));
    QVERIFY(QFileInfo::exists(QDir(root).filePath(QStringLiteral("units/R10K-1.json"))));
    QVERIFY(QFileInfo::exists(QDir(root).filePath(QStringLiteral("symbols/R10K-1.json"))));
    QVERIFY(QFileInfo::exists(QDir(root).filePath(QStringLiteral("entities/R10K.json"))));
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
    footprintCircle.strokeWidth = 0.1;
    component.footprint.circles.append(footprintCircle);
    IR::FootprintRectangleIR footprintRectangle;
    footprintRectangle.bounds = QRectF(-2.0, -1.0, 4.0, 2.0);
    footprintRectangle.strokeWidth = 0.1;
    component.footprint.rectangles.append(footprintRectangle);
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
    QVERIFY(!package.value(QStringLiteral("arcs")).toObject().isEmpty());
    QVERIFY(package.value(QStringLiteral("lines")).toObject().size() >= 4);
    QVERIFY(!package.value(QStringLiteral("texts")).toObject().isEmpty());
    QVERIFY(exporter.diagnostics().join('\n').contains(QStringLiteral("近似")));
    QVERIFY(!exporter.diagnostics().join('\n').contains(QStringLiteral("线宽未保留")));
}

void TestHorizonExporter::preservesRoundRectCustomOutline() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    IR::ComponentIR component = fixture();
    component.footprint.pads[0].shape = IR::PadShape::RoundRect;
    component.footprint.pads[0].customShapePoints = {
        QPointF(-0.4, -0.45), QPointF(0.4, -0.45), QPointF(0.4, 0.45), QPointF(-0.4, 0.45)};

    ExporterHorizonLibrary exporter;
    const QString root = QDir(temporary.path()).filePath(QStringLiteral("pool"));
    QVERIFY2(exporter.exportComponentLibrary({component}, QStringLiteral("fixture"), root),
             qPrintable(exporter.diagnostics().join('\n')));
    const QJsonObject padstack =
        QJsonDocument::fromJson(read(QDir(root).filePath(QStringLiteral("padstacks/R0603-1.json")))).object();
    QVERIFY(!padstack.value(QStringLiteral("polygons")).toObject().isEmpty());
    const QString parameterProgram = padstack.value(QStringLiteral("parameter_program")).toString();
    QVERIFY(parameterProgram.contains(QStringLiteral("expand-polygon [ mask")));
    QVERIFY(parameterProgram.contains(QStringLiteral("expand-polygon [ paste")));
    QVERIFY(parameterProgram.contains(QStringLiteral("solder_mask_expansion")));
    QVERIFY(parameterProgram.contains(QStringLiteral("paste_mask_contraction")));
    QVERIFY(!parameterProgram.contains(QStringLiteral("nm")));
    QVERIFY(exporter.diagnostics().join('\n').contains(QStringLiteral("自定义轮廓")));
    QVERIFY(!exporter.diagnostics().join('\n').contains(QStringLiteral("按 rectangle 输出")));
}

void TestHorizonExporter::writesBottomCustomPadstackLayers() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    IR::ComponentIR component = fixture();
    component.footprint.pads[0].shape = IR::PadShape::Polygon;
    component.footprint.pads[0].layer = IR::LayerType::BottomCopper;
    component.footprint.pads[0].customShapePoints = {
        QPointF(-0.4, -0.45), QPointF(0.4, -0.45), QPointF(0.4, 0.45), QPointF(-0.4, 0.45)};

    ExporterHorizonLibrary exporter;
    const QString root = QDir(temporary.path()).filePath(QStringLiteral("pool"));
    QVERIFY2(exporter.exportComponentLibrary({component}, QStringLiteral("fixture"), root),
             qPrintable(exporter.diagnostics().join('\n')));
    const QJsonObject padstack =
        QJsonDocument::fromJson(read(QDir(root).filePath(QStringLiteral("padstacks/R0603-1.json")))).object();
    const QJsonObject polygons = padstack.value(QStringLiteral("polygons")).toObject();
    QSet<int> layers;
    for (const QJsonValue& polygonValue : polygons)
        layers.insert(polygonValue.toObject().value(QStringLiteral("layer")).toInt());
    QCOMPARE(layers, QSet<int>({-100, -110, -130}));
}

void TestHorizonExporter::rejectsInvalid3dModelData() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    IR::ComponentIR component = fixture();
    component.footprint.models3d.append(IR::Model3DIR{});

    ExporterHorizonLibrary exporter;
    const QString root = QDir(temporary.path()).filePath(QStringLiteral("pool"));
    QVERIFY(!exporter.exportComponentLibrary({component}, QStringLiteral("fixture"), root, true));
    QVERIFY(exporter.diagnostics().join('\n').contains(QStringLiteral("没有有效的 STEP 或 OBJ 数据")));
}

void TestHorizonExporter::rejectsObjOnlyModel() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    IR::ComponentIR component = fixture();
    IR::Model3DIR model;
    model.setName(QStringLiteral("body.obj"));
    model.setRawObj(QStringLiteral("v 0 0 0\n"));
    component.footprint.models3d.append(model);

    ExporterHorizonLibrary exporter;
    const QString root = QDir(temporary.path()).filePath(QStringLiteral("pool"));
    QVERIFY(!exporter.exportComponentLibrary({component}, QStringLiteral("fixture"), root, true));
    QVERIFY(exporter.diagnostics().join('\n').contains(QStringLiteral("只有 OBJ 数据")));
    QVERIFY(exporter.diagnostics().join('\n').contains(QStringLiteral("仅支持 STEP")));
}

void TestHorizonExporter::writesEmbeddedModelAndPlacement() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    IR::ComponentIR component = fixture();
    IR::Model3DIR model;
    model.setName(QStringLiteral("body.step"));
    model.setStepData("ISO-10303-21;\n");
    model.setTranslation({1.0, 2.0, 3.0});
    model.setStepOffsetMm({0.1, 0.2, 0.3});
    model.setRotation({10.0, 20.0, 30.0});
    component.footprint.models3d.append(model);
    IR::Model3DIR secondModel;
    secondModel.setName(QStringLiteral("detail.step"));
    secondModel.setStepData("ISO-10303-21;\n");
    secondModel.setTranslation({-1.0, -2.0, -3.0});
    secondModel.setRotation({-10.0, 5.0, 90.0});
    component.footprint.models3d.append(secondModel);

    ExporterHorizonLibrary exporter;
    const QString root = QDir(temporary.path()).filePath(QStringLiteral("pool"));
    QVERIFY2(exporter.exportComponentLibrary({component}, QStringLiteral("fixture"), root, true),
             qPrintable(exporter.diagnostics().join('\n')));
    const QJsonObject package =
        QJsonDocument::fromJson(read(QDir(root).filePath(QStringLiteral("packages/R0603/package.json")))).object();
    const QJsonObject models = package.value(QStringLiteral("models")).toObject();
    QCOMPARE(models.size(), 2);
    const QString defaultModel = package.value(QStringLiteral("default_model")).toString();
    QVERIFY(models.contains(defaultModel));
    const QJsonObject modelObject = models.value(defaultModel).toObject();
    QCOMPARE(modelObject.value(QStringLiteral("x")).toInteger(), 1100000);
    QCOMPARE(modelObject.value(QStringLiteral("y")).toInteger(), 2200000);
    QCOMPARE(modelObject.value(QStringLiteral("z")).toInteger(), 3300000);
    QVERIFY(QFileInfo::exists(QDir(root).filePath(modelObject.value(QStringLiteral("filename")).toString())));
    int modelFiles = 0;
    for (const QJsonValue& modelValue : models) {
        if (QFileInfo::exists(QDir(root).filePath(modelValue.toObject().value(QStringLiteral("filename")).toString())))
            ++modelFiles;
    }
    QCOMPARE(modelFiles, 2);
}

void TestHorizonExporter::respectsFootprintModelExportOption() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    IR::FootprintComponentIR footprint = fixture().footprint;
    IR::Model3DIR model;
    model.setName(QStringLiteral("body.step"));
    model.setStepData("ISO-10303-21;\n");
    footprint.models3d.append(model);

    ExporterHorizonLibrary exporter;
    const QString withoutModels = QDir(temporary.path()).filePath(QStringLiteral("without-models"));
    QVERIFY2(exporter.exportFootprintLibrary({footprint}, QStringLiteral("fixture"), withoutModels, true, false),
             qPrintable(exporter.diagnostics().join('\n')));
    const QJsonObject packageWithoutModels =
        QJsonDocument::fromJson(read(QDir(withoutModels).filePath(QStringLiteral("packages/R0603/package.json"))))
            .object();
    QCOMPARE(packageWithoutModels.value(QStringLiteral("models")).toObject().size(), 0);
    QCOMPARE(packageWithoutModels.value(QStringLiteral("default_model")).toString(),
             QStringLiteral("00000000-0000-0000-0000-000000000000"));
    const QDir withoutModelDir(QDir(withoutModels).filePath(QStringLiteral("3d_models")));
    QVERIFY(!withoutModelDir.exists() || withoutModelDir.entryList(QDir::Files).isEmpty());

    const QString withModels = QDir(temporary.path()).filePath(QStringLiteral("with-models"));
    QVERIFY2(exporter.exportFootprintLibrary({footprint}, QStringLiteral("fixture"), withModels, true, true),
             qPrintable(exporter.diagnostics().join('\n')));
    const QJsonObject packageWithModels =
        QJsonDocument::fromJson(read(QDir(withModels).filePath(QStringLiteral("packages/R0603/package.json"))))
            .object();
    QCOMPARE(packageWithModels.value(QStringLiteral("models")).toObject().size(), 1);
    QVERIFY(QDir(QDir(withModels).filePath(QStringLiteral("3d_models"))).exists());
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

void TestHorizonExporter::writesPasteMountingHoleAndKeepoutSemantics() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    IR::ComponentIR component = fixture();

    IR::FootprintHoleIR hole;
    hole.center = QPointF(2.0, 1.0);
    hole.radius = 1.25;
    component.footprint.holes.append(hole);

    IR::FootprintRegionIR keepout;
    keepout.layer = IR::LayerType::KeepOut;
    keepout.isKeepOut = true;
    keepout.vertices = {QPointF(-2.0, -2.0), QPointF(2.0, -2.0), QPointF(2.0, 2.0), QPointF(-2.0, 2.0)};
    component.footprint.regions.append(keepout);

    ExporterHorizonLibrary exporter;
    const QString root = QDir(temporary.path()).filePath(QStringLiteral("pool"));
    QVERIFY2(exporter.exportComponentLibrary({component}, QStringLiteral("fixture"), root),
             qPrintable(exporter.diagnostics().join('\n')));

    const QJsonObject package =
        QJsonDocument::fromJson(read(QDir(root).filePath(QStringLiteral("packages/R0603/package.json")))).object();
    const QJsonObject pads = package.value(QStringLiteral("pads")).toObject();
    QCOMPARE(pads.size(), 3);
    const QJsonObject keepouts = package.value(QStringLiteral("keepouts")).toObject();
    QCOMPARE(keepouts.size(), 1);
    const QString polygonUuid = keepouts.constBegin().value().toObject().value(QStringLiteral("polygon")).toString();
    QVERIFY(package.value(QStringLiteral("polygons")).toObject().contains(polygonUuid));

    const QJsonObject holePadstack =
        QJsonDocument::fromJson(read(QDir(root).filePath(QStringLiteral("padstacks/R0603-MH1.json")))).object();
    QCOMPARE(holePadstack.value(QStringLiteral("padstack_type")).toString(), QStringLiteral("mechanical"));
    QCOMPARE(holePadstack.value(QStringLiteral("holes")).toObject().size(), 1);

    const QJsonObject smdPadstack =
        QJsonDocument::fromJson(read(QDir(root).filePath(QStringLiteral("padstacks/R0603-1.json")))).object();
    const QString smdParameterProgram = smdPadstack.value(QStringLiteral("parameter_program")).toString();
    QVERIFY(smdParameterProgram.contains(QStringLiteral("solder_mask_expansion")));
    QVERIFY(smdParameterProgram.contains(QStringLiteral("paste_mask_contraction")));
    QVERIFY(smdParameterProgram.contains(QStringLiteral("mm")));
    QVERIFY(!smdParameterProgram.contains(QStringLiteral("nm")));
    bool hasPaste = false;
    for (const QJsonValue& shapeValue : smdPadstack.value(QStringLiteral("shapes")).toObject()) {
        if (shapeValue.toObject().value(QStringLiteral("parameter_class")).toString() == QStringLiteral("paste")) {
            hasPaste = true;
            break;
        }
    }
    QVERIFY(hasPaste);
    const QJsonObject holeShapes = holePadstack.value(QStringLiteral("shapes")).toObject();
    QCOMPARE(holeShapes.size(), 2);
    for (const QJsonValue& shapeValue : holeShapes)
        QCOMPARE(shapeValue.toObject().value(QStringLiteral("parameter_class")).toString(), QStringLiteral("mask"));

    component.footprint.pads[0].padType = IR::PadType::ThroughHole;
    component.footprint.pads[0].holeSize = 0.4;
    component.footprint.pads[0].holeLength = 0.8;
    component.footprint.pads[0].isPlated = true;
    ExporterHorizonLibrary throughHoleExporter;
    const QString throughRoot = QDir(temporary.path()).filePath(QStringLiteral("through-pool"));
    QVERIFY2(throughHoleExporter.exportComponentLibrary({component}, QStringLiteral("fixture"), throughRoot),
             qPrintable(throughHoleExporter.diagnostics().join('\n')));
    const QJsonObject throughPadstack =
        QJsonDocument::fromJson(read(QDir(throughRoot).filePath(QStringLiteral("padstacks/R0603-1.json")))).object();
    QCOMPARE(throughPadstack.value(QStringLiteral("padstack_type")).toString(), QStringLiteral("through"));
    QCOMPARE(throughPadstack.value(QStringLiteral("holes")).toObject().size(), 1);
    int copperShapeCount = 0;
    int maskShapeCount = 0;
    for (const QJsonValue& shapeValue : throughPadstack.value(QStringLiteral("shapes")).toObject()) {
        const QString parameterClass = shapeValue.toObject().value(QStringLiteral("parameter_class")).toString();
        copperShapeCount += parameterClass == QStringLiteral("copper");
        maskShapeCount += parameterClass == QStringLiteral("mask");
    }
    QCOMPARE(copperShapeCount, 6);
    QCOMPARE(maskShapeCount, 2);

    IR::ComponentIR npthComponent = fixture();
    IR::FootprintPadIR npth;
    npth.position = QPointF(3.0, 0.0);
    npth.padType = IR::PadType::ThroughHole;
    npth.shape = IR::PadShape::Ellipse;
    npth.size = QSizeF(2.0, 2.0);
    npth.holeSize = 1.0;
    npth.isPlated = false;
    npthComponent.footprint.pads.append(npth);
    ExporterHorizonLibrary npthExporter;
    const QString npthRoot = QDir(temporary.path()).filePath(QStringLiteral("npth-pool"));
    QVERIFY2(npthExporter.exportComponentLibrary({npthComponent}, QStringLiteral("fixture"), npthRoot),
             qPrintable(npthExporter.diagnostics().join('\n')));
    const QJsonObject npthPart =
        QJsonDocument::fromJson(read(QDir(npthRoot).filePath(QStringLiteral("parts/R10K.json")))).object();
    QCOMPARE(npthPart.value(QStringLiteral("pad_map")).toObject().size(), 2);
    QVERIFY(QFileInfo::exists(QDir(npthRoot).filePath(QStringLiteral("padstacks/R0603-NPTH3.json"))));
}

void TestHorizonExporter::rejectsPoolRegistrationWithoutOfficialPool() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    QStringList diagnostics;
    QVERIFY(!HorizonPoolIntegration::updateAndRegister(temporary.path(), diagnostics));
    QVERIFY(!diagnostics.isEmpty());
    QVERIFY(diagnostics.constFirst().contains(QStringLiteral("pool.json")));
}

void TestHorizonExporter::invokesInjectedPoolIntegrationContract() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QString poolPath = QDir(temporary.path()).filePath(QStringLiteral("pool"));
    QVERIFY(QDir().mkpath(poolPath));
    QFile poolInfo(QDir(poolPath).filePath(QStringLiteral("pool.json")));
    QVERIFY(poolInfo.open(QIODevice::WriteOnly | QIODevice::Text));
    QVERIFY(poolInfo.write(
                QJsonDocument(QJsonObject{{QStringLiteral("uuid"), QUuid::createUuid().toString(QUuid::WithoutBraces)}})
                    .toJson()) > 0);
    poolInfo.close();

    const QString moduleDir = QDir(temporary.path()).filePath(QStringLiteral("python"));
    QVERIFY(QDir().mkpath(moduleDir));
    QFile module(QDir(moduleDir).filePath(QStringLiteral("horizon.py")));
    QVERIFY(module.open(QIODevice::WriteOnly | QIODevice::Text));
    const QByteArray moduleSource =
        "import os\n"
        "class Pool:\n"
        "    @staticmethod\n"
        "    def update(path):\n"
        "        open(os.path.join(path, 'pool.db'), 'wb').write(b'updated')\n"
        "class PoolManager:\n"
        "    pools = {}\n"
        "    @staticmethod\n"
        "    def get_pools():\n"
        "        return dict(PoolManager.pools)\n"
        "    @staticmethod\n"
        "    def add_pool(path):\n"
        "        PoolManager.pools[os.path.abspath(path)] = 'pool-uuid'\n";
    QVERIFY(module.write(moduleSource) == moduleSource.size());
    module.close();

    const QByteArray previousPython = qgetenv("EASYKICONVERTER_HORIZON_PYTHON");
    const QByteArray previousPythonPath = qgetenv("EASYKICONVERTER_HORIZON_PYTHONPATH");
    qputenv("EASYKICONVERTER_HORIZON_PYTHON", "python3");
    qputenv("EASYKICONVERTER_HORIZON_PYTHONPATH", moduleDir.toLocal8Bit());

    QStringList diagnostics;
    const bool success = HorizonPoolIntegration::updateAndRegister(poolPath, diagnostics);

    if (previousPython.isNull())
        qunsetenv("EASYKICONVERTER_HORIZON_PYTHON");
    else
        qputenv("EASYKICONVERTER_HORIZON_PYTHON", previousPython);
    if (previousPythonPath.isNull())
        qunsetenv("EASYKICONVERTER_HORIZON_PYTHONPATH");
    else
        qputenv("EASYKICONVERTER_HORIZON_PYTHONPATH", previousPythonPath);

    QVERIFY2(success, qPrintable(diagnostics.join('\n')));
    QVERIFY(QFileInfo::exists(QDir(poolPath).filePath(QStringLiteral("pool.db"))));
    QVERIFY(diagnostics.join('\n').contains(QStringLiteral("手动重新加载或重启 Horizon")));

    const QString relativeModuleDir = QDir::current().relativeFilePath(moduleDir);
    qputenv("EASYKICONVERTER_HORIZON_PYTHONPATH", relativeModuleDir.toLocal8Bit());
    diagnostics.clear();
    QVERIFY2(HorizonPoolIntegration::updateAndRegister(poolPath, diagnostics), qPrintable(diagnostics.join('\n')));

    const QString conflictPoolPath = QDir(temporary.path()).filePath(QStringLiteral("conflict-pool"));
    QVERIFY(QDir().mkpath(conflictPoolPath));
    QFile conflictPoolInfo(QDir(conflictPoolPath).filePath(QStringLiteral("pool.json")));
    QVERIFY(conflictPoolInfo.open(QIODevice::WriteOnly | QIODevice::Text));
    QVERIFY(conflictPoolInfo.write(
                QJsonDocument(QJsonObject{{QStringLiteral("uuid"), QUuid::createUuid().toString(QUuid::WithoutBraces)}})
                    .toJson()) > 0);
    conflictPoolInfo.close();
    QVERIFY(module.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text));
    const QByteArray conflictingModuleSource =
        "import os\n"
        "class Pool:\n"
        "    @staticmethod\n"
        "    def update(path):\n"
        "        open(os.path.join(path, 'pool.db'), 'wb').write(b'updated')\n"
        "class PoolManager:\n"
        "    @staticmethod\n"
        "    def get_pools():\n"
        "        return {os.path.abspath(os.environ['HORIZON_CONFLICT_PATH']): 'different-uuid'}\n"
        "    @staticmethod\n"
        "    def add_pool(path):\n"
        "        pass\n";
    QVERIFY(module.write(conflictingModuleSource) == conflictingModuleSource.size());
    module.close();
    const QByteArray previousConflictPath = qgetenv("HORIZON_CONFLICT_PATH");
    qputenv("HORIZON_CONFLICT_PATH", conflictPoolPath.toLocal8Bit());
    diagnostics.clear();
    QVERIFY(!HorizonPoolIntegration::updateAndRegister(conflictPoolPath, diagnostics));
    QVERIFY(diagnostics.join('\n').contains(QStringLiteral("different UUID")));
    if (previousConflictPath.isNull())
        qunsetenv("HORIZON_CONFLICT_PATH");
    else
        qputenv("HORIZON_CONFLICT_PATH", previousConflictPath);

    const QString failedPoolPath = QDir(temporary.path()).filePath(QStringLiteral("failed-pool"));
    QVERIFY(QDir().mkpath(failedPoolPath));
    QFile failedPoolInfo(QDir(failedPoolPath).filePath(QStringLiteral("pool.json")));
    QVERIFY(failedPoolInfo.open(QIODevice::WriteOnly | QIODevice::Text));
    QVERIFY(failedPoolInfo.write(
                QJsonDocument(QJsonObject{{QStringLiteral("uuid"), QUuid::createUuid().toString(QUuid::WithoutBraces)}})
                    .toJson()) > 0);
    failedPoolInfo.close();
    QVERIFY(module.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text));
    const QByteArray failingModuleSource =
        "class Pool:\n"
        "    @staticmethod\n"
        "    def update(path):\n"
        "        pass\n"
        "class PoolManager:\n"
        "    @staticmethod\n"
        "    def get_pools():\n"
        "        return {}\n"
        "    @staticmethod\n"
        "    def add_pool(path):\n"
        "        pass\n";
    QVERIFY(module.write(failingModuleSource) == failingModuleSource.size());
    module.close();
    qputenv("EASYKICONVERTER_HORIZON_PYTHON", "python3");
    qputenv("EASYKICONVERTER_HORIZON_PYTHONPATH", moduleDir.toLocal8Bit());
    diagnostics.clear();
    QVERIFY(!HorizonPoolIntegration::updateAndRegister(failedPoolPath, diagnostics));
    QVERIFY(diagnostics.join('\n').contains(QStringLiteral("pool.db")));
    QVERIFY(!QFileInfo::exists(QDir(failedPoolPath).filePath(QStringLiteral("pool.db"))));
    if (previousPython.isNull())
        qunsetenv("EASYKICONVERTER_HORIZON_PYTHON");
    else
        qputenv("EASYKICONVERTER_HORIZON_PYTHON", previousPython);
    if (previousPythonPath.isNull())
        qunsetenv("EASYKICONVERTER_HORIZON_PYTHONPATH");
    else
        qputenv("EASYKICONVERTER_HORIZON_PYTHONPATH", previousPythonPath);
}

QTEST_GUILESS_MAIN(TestHorizonExporter)
#include "test_horizon_exporter.moc"
