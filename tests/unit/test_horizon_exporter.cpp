/**
 * @file test_horizon_exporter.cpp
 * @brief 验证 Horizon Pool 导出结构、引用完整性和失败诊断。
 */

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

#include <functional>

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
    void rejectsGenericKeepOutGeometryInsteadOfWritingCopper();
    void ignoresHiddenKeepOutText();
    void rejectsKeepOutBeforeCreatingPoolFiles();
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
    IR::ComponentIR component;  ///< 元器件 IR 对象。
    component.name = QStringLiteral("R10K");
    component.prefix = QStringLiteral("R");
    component.manufacturer = QStringLiteral("Example");
    component.manufacturerPart = QStringLiteral("R10K-0603");
    component.symbol.name = QStringLiteral("R10K");
    component.symbol.designatorPrefix = QStringLiteral("R");
    for (const auto& entry :
         {qMakePair(QStringLiteral("1"), QPointF(-2.0, 0.0)), qMakePair(QStringLiteral("2"), QPointF(2.0, 0.0))}) {
        IR::SymbolPinIR pin;  ///< 引脚。
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
        IR::FootprintPadIR pad;  ///< 焊盘。
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
    QStringList result;  ///< 结果。
    QDirIterator it(root, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext())
        result.append(QDir(root).relativeFilePath(it.next()));
    result.sort();
    return result;
}

/**
 * @brief 验证完整组件库能够生成 Pool、符号、封装和 Part 引用。
 */
void TestHorizonExporter::writesCompletePoolAndReferences() {
    QTemporaryDir temporary;  ///< 测试临时目录。
    QVERIFY(temporary.isValid());
    ExporterHorizonLibrary exporter;  ///< Horizon 导出器。
    const QString root = QDir(temporary.path()).filePath(QStringLiteral("pool"));  ///< 根目录。
    QVERIFY2(exporter.exportComponentLibrary({fixture()}, QStringLiteral("fixture"), root),
             qPrintable(exporter.diagnostics().join('\n')));
    QVERIFY(QFileInfo::exists(QDir(root).filePath(QStringLiteral("pool.json"))));
    const QString unitPath = QDir(root).filePath(QStringLiteral("units/R10K-1.json"));  ///< 路径。
    const QString symbolPath = QDir(root).filePath(QStringLiteral("symbols/R10K-1.json"));  ///< 路径。
    const QString entityPath = QDir(root).filePath(QStringLiteral("entities/R10K.json"));  ///< 路径。
    const QString packagePath = QDir(root).filePath(QStringLiteral("packages/R0603/package.json"));  ///< 路径。
    const QString partPath = QDir(root).filePath(QStringLiteral("parts/R10K.json"));  ///< 路径。
    QVERIFY(QFileInfo::exists(unitPath));
    QVERIFY(QFileInfo::exists(symbolPath));
    QVERIFY(QFileInfo::exists(entityPath));
    QVERIFY(QFileInfo::exists(packagePath));
    QVERIFY(QFileInfo::exists(partPath));

    const QJsonObject unit = QJsonDocument::fromJson(read(unitPath)).object();  ///< 单元 JSON 对象。
    const QJsonObject symbol = QJsonDocument::fromJson(read(symbolPath)).object();  ///< 符号 JSON 对象。
    const QJsonObject entity = QJsonDocument::fromJson(read(entityPath)).object();  ///< 实体 JSON 对象。
    const QJsonObject package = QJsonDocument::fromJson(read(packagePath)).object();  ///< 封装 JSON 对象。
    const QJsonObject part = QJsonDocument::fromJson(read(partPath)).object();  ///< 当前 JSON 对象。
    QCOMPARE(unit.value(QStringLiteral("type")).toString(), QStringLiteral("unit"));
    QCOMPARE(unit.value(QStringLiteral("pins")).toObject().size(), 2);
    QCOMPARE(symbol.value(QStringLiteral("unit")).toString(), unit.value(QStringLiteral("uuid")).toString());
    QCOMPARE(entity.value(QStringLiteral("gates")).toObject().size(), 1);
    const QJsonObject gate =
        entity.value(QStringLiteral("gates")).toObject().constBegin().value().toObject();  ///< 符号门 JSON 对象。
    QCOMPARE(gate.value(QStringLiteral("unit")).toString(), unit.value(QStringLiteral("uuid")).toString());
    QCOMPARE(part.value(QStringLiteral("entity")).toString(), entity.value(QStringLiteral("uuid")).toString());
    QCOMPARE(part.value(QStringLiteral("package")).toString(), package.value(QStringLiteral("uuid")).toString());
    QCOMPARE(package.value(QStringLiteral("pads")).toObject().size(), 2);
    QCOMPARE(part.value(QStringLiteral("pad_map")).toObject().size(), 2);
    for (const QString& padUuid : part.value(QStringLiteral("pad_map")).toObject().keys()) {
        QVERIFY(package.value(QStringLiteral("pads")).toObject().contains(padUuid));
        const QJsonObject mapping =
            part.value(QStringLiteral("pad_map")).toObject().value(padUuid).toObject();  ///< 焊盘映射 JSON 对象。
        QVERIFY(!mapping.value(QStringLiteral("gate")).toString().isEmpty());
        QVERIFY(
            unit.value(QStringLiteral("pins")).toObject().contains(mapping.value(QStringLiteral("pin")).toString()));
    }
}

/**
 * @brief 验证仅符号导出能够生成 Horizon Pool 目录。
 */
void TestHorizonExporter::writesSymbolOnlyPoolDirectory() {
    QTemporaryDir temporary;  ///< 测试临时目录。
    QVERIFY(temporary.isValid());

    ExporterHorizonLibrary exporter;  ///< Horizon 导出器。
    QVERIFY(exporter.isDirectoryOutput());
    const QString root = QDir(temporary.path()).filePath(QStringLiteral("symbols.pool"));  ///< 根目录。
    const IR::SymbolComponentIR symbol = fixture().symbol;  ///< 符号 JSON 对象。
    QVERIFY2(exporter.exportSymbolLibrary({symbol}, QStringLiteral("symbols"), root, false, false),
             qPrintable(exporter.diagnostics().join('\n')));
    QVERIFY(QDir(root).exists());
    QVERIFY(QFileInfo::exists(QDir(root).filePath(QStringLiteral("pool.json"))));
    QVERIFY(QFileInfo::exists(QDir(root).filePath(QStringLiteral("units/R10K-1.json"))));
    QVERIFY(QFileInfo::exists(QDir(root).filePath(QStringLiteral("symbols/R10K-1.json"))));
    QVERIFY(QFileInfo::exists(QDir(root).filePath(QStringLiteral("entities/R10K.json"))));
}

/**
 * @brief 验证相同输入生成的 Horizon 文件内容保持确定性。
 */
void TestHorizonExporter::outputIsDeterministic() {
    QTemporaryDir first;  ///< 第一份测试数据。
    QTemporaryDir second;  ///< 第二份测试数据。
    QVERIFY(first.isValid());
    QVERIFY(second.isValid());
    ExporterHorizonLibrary left;  ///< 左子节点索引。
    ExporterHorizonLibrary right;  ///< 右子节点索引。
    QVERIFY(left.exportComponentLibrary({fixture()}, QStringLiteral("fixture"), QDir(first.path()).filePath("pool")));
    QVERIFY(right.exportComponentLibrary({fixture()}, QStringLiteral("fixture"), QDir(second.path()).filePath("pool")));
    const QStringList files = relativeFiles(QDir(first.path()).filePath("pool"));  ///< 输出文件列表。
    QCOMPARE(files, relativeFiles(QDir(second.path()).filePath("pool")));
    for (const QString& file : files)
        QCOMPARE(read(QDir(first.path()).filePath(QStringLiteral("pool/") + file)),
                 read(QDir(second.path()).filePath(QStringLiteral("pool/") + file)));
}

/**
 * @brief 验证缺失引脚焊盘关联时导出失败并报告诊断。
 */
void TestHorizonExporter::rejectsMissingPinPadAssociation() {
    QTemporaryDir temporary;  ///< 测试临时目录。
    QVERIFY(temporary.isValid());
    IR::ComponentIR component = fixture();  ///< 元器件 IR 对象。
    component.footprint.pads[1].number = QStringLiteral("3");
    ExporterHorizonLibrary exporter;  ///< Horizon 导出器。
    QVERIFY(!exporter.exportComponentLibrary(
        {component}, QStringLiteral("missing"), QDir(temporary.path()).filePath("pool")));
    QVERIFY(exporter.diagnostics().join('\n').contains(QStringLiteral("找不到符号引脚")));
}

/**
 * @brief 验证重复焊盘编号不会生成歧义的 Pad 映射。
 */
void TestHorizonExporter::rejectsDuplicatePadNumber() {
    QTemporaryDir temporary;  ///< 测试临时目录。
    QVERIFY(temporary.isValid());
    IR::ComponentIR component = fixture();  ///< 元器件 IR 对象。
    component.footprint.pads[1].number = QStringLiteral("1");
    ExporterHorizonLibrary exporter;  ///< Horizon 导出器。
    QVERIFY(!exporter.exportComponentLibrary(
        {component}, QStringLiteral("duplicate"), QDir(temporary.path()).filePath("pool")));
    QVERIFY(exporter.diagnostics().join('\n').contains(QStringLiteral("重复焊盘编号")));
}

/**
 * @brief 验证清洗后的符号名称冲突会被拒绝。
 */
void TestHorizonExporter::rejectsSanitizedSymbolNameCollision() {
    QTemporaryDir temporary;  ///< 测试临时目录。
    QVERIFY(temporary.isValid());
    const IR::SymbolComponentIR source = fixture().symbol;  ///< 源 IR 对象。
    IR::SymbolComponentIR first = source;  ///< 第一份测试数据。
    IR::SymbolComponentIR second = source;  ///< 第二份测试数据。
    first.name = QStringLiteral("A/B");
    second.name = QStringLiteral("A:B");
    ExporterHorizonLibrary exporter;  ///< Horizon 导出器。
    const QString root = QDir(temporary.path()).filePath(QStringLiteral("pool"));  ///< 根目录。
    QVERIFY(!exporter.exportSymbolLibrary({first, second}, QStringLiteral("collision"), root));
    QVERIFY(exporter.diagnostics().join('\n').contains(QStringLiteral("名称清洗后输出路径冲突")));
    QVERIFY(!QFileInfo::exists(QDir(root).filePath(QStringLiteral("pool.json"))));
}

/**
 * @brief 验证清洗后的封装名称冲突会被拒绝。
 */
void TestHorizonExporter::rejectsSanitizedFootprintNameCollision() {
    QTemporaryDir temporary;  ///< 测试临时目录。
    QVERIFY(temporary.isValid());
    const IR::FootprintComponentIR source = fixture().footprint;  ///< 源 IR 对象。
    IR::FootprintComponentIR first = source;  ///< 第一份测试数据。
    IR::FootprintComponentIR second = source;  ///< 第二份测试数据。
    first.name = QStringLiteral("A/B");
    second.name = QStringLiteral("A:B");
    ExporterHorizonLibrary exporter;  ///< Horizon 导出器。
    const QString root = QDir(temporary.path()).filePath(QStringLiteral("pool"));  ///< 根目录。
    QVERIFY(!exporter.exportFootprintLibrary({first, second}, QStringLiteral("collision"), root));
    QVERIFY(exporter.diagnostics().join('\n').contains(QStringLiteral("名称清洗后输出路径冲突")));
    QVERIFY(!QFileInfo::exists(QDir(root).filePath(QStringLiteral("pool.json"))));
}

/**
 * @brief 验证清洗后的组件名称冲突会被拒绝。
 */
void TestHorizonExporter::rejectsSanitizedComponentNameCollision() {
    QTemporaryDir temporary;  ///< 测试临时目录。
    QVERIFY(temporary.isValid());
    IR::ComponentIR first = fixture();  ///< 第一份测试数据。
    IR::ComponentIR second = fixture();  ///< 第二份测试数据。
    first.name = QStringLiteral("A/B");
    second.name = QStringLiteral("A:B");
    ExporterHorizonLibrary exporter;  ///< Horizon 导出器。
    const QString root = QDir(temporary.path()).filePath(QStringLiteral("pool"));  ///< 根目录。
    QVERIFY(!exporter.exportComponentLibrary({first, second}, QStringLiteral("collision"), root));
    QVERIFY(exporter.diagnostics().join('\n').contains(QStringLiteral("名称清洗后输出路径冲突")));
    QVERIFY(!QFileInfo::exists(QDir(root).filePath(QStringLiteral("pool.json"))));
}

/**
 * @brief 验证组件名与符号名不同时引脚引用仍保持有效。
 */
void TestHorizonExporter::keepsPinReferencesWhenComponentAndSymbolNamesDiffer() {
    QTemporaryDir temporary;  ///< 测试临时目录。
    QVERIFY(temporary.isValid());
    IR::ComponentIR component = fixture();  ///< 元器件 IR 对象。
    component.name = QStringLiteral("R10K_COMPONENT");
    ExporterHorizonLibrary exporter;  ///< Horizon 导出器。
    const QString root = QDir(temporary.path()).filePath(QStringLiteral("pool"));  ///< 根目录。
    QVERIFY2(exporter.exportComponentLibrary({component}, QStringLiteral("fixture"), root),
             qPrintable(exporter.diagnostics().join('\n')));
    const QJsonObject unit =
        QJsonDocument::fromJson(read(QDir(root).filePath(QStringLiteral("units/R10K_COMPONENT-1.json")))).object();
    const QJsonObject part =
        QJsonDocument::fromJson(read(QDir(root).filePath(QStringLiteral("parts/R10K_COMPONENT.json")))).object();
    for (const QJsonValue& mappingValue : part.value(QStringLiteral("pad_map")).toObject()) {
        const QString pinUuid = mappingValue.toObject().value(QStringLiteral("pin")).toString();  ///< 标识符。
        QVERIFY(unit.value(QStringLiteral("pins")).toObject().contains(pinUuid));
    }
}

/**
 * @brief 验证常见几何转换及其必要的近似诊断。
 */
void TestHorizonExporter::convertsCommonGeometryWithExplicitApproximationDiagnostics() {
    QTemporaryDir temporary;  ///< 测试临时目录。
    QVERIFY(temporary.isValid());
    IR::ComponentIR component = fixture();  ///< 元器件 IR 对象。
    IR::SymbolCircleIR symbolCircle;  ///< 符号圆形图元。
    symbolCircle.center = QPointF(0.0, 0.0);
    symbolCircle.radius = 1.0;
    component.symbol.circles.append(symbolCircle);
    IR::FootprintCircleIR footprintCircle;  ///< 封装圆形图元。
    footprintCircle.center = QPointF(0.0, 0.0);
    footprintCircle.radius = 1.5;
    footprintCircle.strokeWidth = 0.1;
    component.footprint.circles.append(footprintCircle);
    IR::FootprintRectangleIR footprintRectangle;  ///< 封装矩形图元。
    footprintRectangle.bounds = QRectF(-2.0, -1.0, 4.0, 2.0);
    footprintRectangle.strokeWidth = 0.1;
    component.footprint.rectangles.append(footprintRectangle);
    IR::FootprintTextIR footprintText;  ///< 文本。
    footprintText.text = QStringLiteral("REF");
    footprintText.position = QPointF(0.0, 0.0);
    component.footprint.texts.append(footprintText);

    ExporterHorizonLibrary exporter;  ///< Horizon 导出器。
    const QString root = QDir(temporary.path()).filePath(QStringLiteral("pool"));  ///< 根目录。
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

/**
 * @brief 验证普通 KeepOut 图元不会被错误写入铜层。
 */
void TestHorizonExporter::rejectsGenericKeepOutGeometryInsteadOfWritingCopper() {
    struct KeepOutCase {
        QString name;  ///< 名称。
        std::function<void(IR::FootprintComponentIR&)> add;
    };

    const QList<KeepOutCase> cases = {
        {QStringLiteral("pad"),
         [](IR::FootprintComponentIR& footprint) { footprint.pads[0].layer = IR::LayerType::KeepOut; }},
        {QStringLiteral("outline"),
         [](IR::FootprintComponentIR& footprint) { footprint.outlines[0].layer = IR::LayerType::KeepOut; }},
        {QStringLiteral("track"),
         [](IR::FootprintComponentIR& footprint) {
             IR::FootprintTrackIR track;  ///< 封装走线图元。
             track.points = {QPointF(-1.0, 0.0), QPointF(1.0, 0.0)};
             track.layer = IR::LayerType::KeepOut;
             footprint.tracks.append(track);
         }},
        {QStringLiteral("circle"),
         [](IR::FootprintComponentIR& footprint) {
             IR::FootprintCircleIR circle;  ///< 封装圆形图元。
             circle.layer = IR::LayerType::KeepOut;
             circle.radius = 1.0;
             footprint.circles.append(circle);
         }},
        {QStringLiteral("rectangle"),
         [](IR::FootprintComponentIR& footprint) {
             IR::FootprintRectangleIR rectangle;  ///< 封装矩形图元。
             rectangle.layer = IR::LayerType::KeepOut;
             rectangle.bounds = QRectF(-1.0, -1.0, 2.0, 2.0);
             footprint.rectangles.append(rectangle);
         }},
        {QStringLiteral("arc"),
         [](IR::FootprintComponentIR& footprint) {
             IR::FootprintArcIR arc;  ///< 封装圆弧图元。
             arc.layer = IR::LayerType::KeepOut;
             arc.radius = 1.0;
             arc.endAngle = 90.0;
             footprint.arcs.append(arc);
         }},
        {QStringLiteral("text"),
         [](IR::FootprintComponentIR& footprint) {
             IR::FootprintTextIR text;  ///< 文本。
             text.layer = IR::LayerType::KeepOut;
             text.text = QStringLiteral("KO");
             text.isDisplayed = true;
             footprint.texts.append(text);
         }},
    };

    for (const KeepOutCase& testCase : cases) {
        QTemporaryDir temporary;  ///< 测试临时目录。
        QVERIFY(temporary.isValid());
        IR::ComponentIR component = fixture();  ///< 元器件 IR 对象。
        testCase.add(component.footprint);
        ExporterHorizonLibrary exporter;  ///< Horizon 导出器。
        const QString root = QDir(temporary.path()).filePath(testCase.name);  ///< 根目录。
        QVERIFY2(!exporter.exportComponentLibrary({component}, QStringLiteral("keepout"), root),
                 qPrintable(testCase.name));
        const QString diagnostics = exporter.diagnostics().join('\n');  ///< 诊断列表。
        QVERIFY2(
            diagnostics.contains(QStringLiteral("KeepOut 层")) && diagnostics.contains(QStringLiteral("已拒绝导出")),
            qPrintable(diagnostics));
        QVERIFY2(diagnostics.contains(testCase.name, Qt::CaseInsensitive), qPrintable(diagnostics));
    }
}

/**
 * @brief 验证隐藏 KeepOut 文本不会阻止导出或写入目标库。
 */
void TestHorizonExporter::ignoresHiddenKeepOutText() {
    QTemporaryDir temporary;  ///< 测试临时目录。
    QVERIFY(temporary.isValid());
    IR::ComponentIR component = fixture();  ///< 元器件 IR 对象。

    IR::FootprintTextIR text;  ///< 文本。
    text.layer = IR::LayerType::KeepOut;
    text.text = QStringLiteral("HIDDEN");
    text.isDisplayed = false;
    component.footprint.texts.append(text);

    ExporterHorizonLibrary exporter;  ///< Horizon 导出器。
    const QString root = QDir(temporary.path()).filePath(QStringLiteral("pool"));  ///< 根目录。
    QVERIFY2(exporter.exportComponentLibrary({component}, QStringLiteral("hidden-keepout"), root),
             qPrintable(exporter.diagnostics().join('\n')));

    const QJsonObject package =
        QJsonDocument::fromJson(read(QDir(root).filePath(QStringLiteral("packages/R0603/package.json")))).object();
    const QJsonObject texts = package.value(QStringLiteral("texts")).toObject();  ///< 当前 JSON 对象。
    for (const QJsonValue& value : texts)
        QVERIFY(value.toObject().value(QStringLiteral("text")).toString() != QStringLiteral("HIDDEN"));
    QVERIFY(!exporter.diagnostics().join('\n').contains(QStringLiteral("KeepOut")));
}

/**
 * @brief 验证三种封装导出入口在预检失败时不会创建不完整的 Pool。
 */
void TestHorizonExporter::rejectsKeepOutBeforeCreatingPoolFiles() {
    IR::ComponentIR component = fixture();  ///< 元器件 IR 对象。
    IR::FootprintTextIR text;  ///< 文本。
    text.layer = IR::LayerType::KeepOut;
    text.text = QStringLiteral("VISIBLE");
    text.isDisplayed = true;
    component.footprint.texts.append(text);

    const auto verifyNoPoolFiles = [](const QString& root) {
        QVERIFY(!QFileInfo::exists(QDir(root).filePath(QStringLiteral("pool.json"))));
        QVERIFY(!QFileInfo::exists(QDir(root).filePath(QStringLiteral("padstacks/default-via.json"))));
        QDirIterator iterator(root, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
        QVERIFY(!iterator.hasNext());
    };

    {
        QTemporaryDir temporary;  ///< 测试临时目录。
        QVERIFY(temporary.isValid());
        const QString root = QDir(temporary.path()).filePath(QStringLiteral("single"));  ///< 根目录。
        ExporterHorizonLibrary exporter;  ///< Horizon 导出器。
        QVERIFY(!exporter.exportFootprint(component.footprint, root));
        verifyNoPoolFiles(root);
    }
    {
        QTemporaryDir temporary;  ///< 测试临时目录。
        QVERIFY(temporary.isValid());
        const QString root = QDir(temporary.path()).filePath(QStringLiteral("library"));  ///< 根目录。
        ExporterHorizonLibrary exporter;  ///< Horizon 导出器。
        QVERIFY(!exporter.exportFootprintLibrary({component.footprint}, QStringLiteral("keepout"), root));
        verifyNoPoolFiles(root);
    }
    {
        QTemporaryDir temporary;  ///< 测试临时目录。
        QVERIFY(temporary.isValid());
        const QString root = QDir(temporary.path()).filePath(QStringLiteral("component"));  ///< 根目录。
        ExporterHorizonLibrary exporter;  ///< Horizon 导出器。
        QVERIFY(!exporter.exportComponentLibrary({component}, QStringLiteral("keepout"), root));
        verifyNoPoolFiles(root);
    }
}

/**
 * @brief 验证圆角矩形焊盘轮廓能够保留为自定义几何。
 */
void TestHorizonExporter::preservesRoundRectCustomOutline() {
    QTemporaryDir temporary;  ///< 测试临时目录。
    QVERIFY(temporary.isValid());
    IR::ComponentIR component = fixture();  ///< 元器件 IR 对象。
    component.footprint.pads[0].shape = IR::PadShape::RoundRect;
    component.footprint.pads[0].customShapePoints = {
        QPointF(-0.4, -0.45), QPointF(0.4, -0.45), QPointF(0.4, 0.45), QPointF(-0.4, 0.45)};

    ExporterHorizonLibrary exporter;  ///< Horizon 导出器。
    const QString root = QDir(temporary.path()).filePath(QStringLiteral("pool"));  ///< 根目录。
    QVERIFY2(exporter.exportComponentLibrary({component}, QStringLiteral("fixture"), root),
             qPrintable(exporter.diagnostics().join('\n')));
    const QJsonObject padstack =
        QJsonDocument::fromJson(read(QDir(root).filePath(QStringLiteral("padstacks/R0603-1.json")))).object();
    QVERIFY(!padstack.value(QStringLiteral("polygons")).toObject().isEmpty());
    const QString parameterProgram = padstack.value(QStringLiteral("parameter_program")).toString();  ///< 当前文本值。
    QVERIFY(parameterProgram.contains(QStringLiteral("expand-polygon [ mask")));
    QVERIFY(parameterProgram.contains(QStringLiteral("expand-polygon [ paste")));
    QVERIFY(parameterProgram.contains(QStringLiteral("solder_mask_expansion")));
    QVERIFY(parameterProgram.contains(QStringLiteral("paste_mask_contraction")));
    QVERIFY(!parameterProgram.contains(QStringLiteral("nm")));
    QVERIFY(exporter.diagnostics().join('\n').contains(QStringLiteral("自定义轮廓")));
    QVERIFY(!exporter.diagnostics().join('\n').contains(QStringLiteral("按 rectangle 输出")));
}

/**
 * @brief 验证底层自定义焊盘层能够正确序列化。
 */
void TestHorizonExporter::writesBottomCustomPadstackLayers() {
    QTemporaryDir temporary;  ///< 测试临时目录。
    QVERIFY(temporary.isValid());
    IR::ComponentIR component = fixture();  ///< 元器件 IR 对象。
    component.footprint.pads[0].shape = IR::PadShape::Polygon;
    component.footprint.pads[0].layer = IR::LayerType::BottomCopper;
    component.footprint.pads[0].customShapePoints = {
        QPointF(-0.4, -0.45), QPointF(0.4, -0.45), QPointF(0.4, 0.45), QPointF(-0.4, 0.45)};

    ExporterHorizonLibrary exporter;  ///< Horizon 导出器。
    const QString root = QDir(temporary.path()).filePath(QStringLiteral("pool"));  ///< 根目录。
    QVERIFY2(exporter.exportComponentLibrary({component}, QStringLiteral("fixture"), root),
             qPrintable(exporter.diagnostics().join('\n')));
    const QJsonObject padstack =
        QJsonDocument::fromJson(read(QDir(root).filePath(QStringLiteral("padstacks/R0603-1.json")))).object();
    const QJsonObject polygons = padstack.value(QStringLiteral("polygons")).toObject();  ///< 当前 JSON 对象。
    QSet<int> layers;  ///< 当前集合。
    for (const QJsonValue& polygonValue : polygons)
        layers.insert(polygonValue.toObject().value(QStringLiteral("layer")).toInt());
    QCOMPARE(layers, QSet<int>({-100, -110, -130}));
}

/**
 * @brief 验证无效三维模型数据会导致导出失败。
 */
void TestHorizonExporter::rejectsInvalid3dModelData() {
    QTemporaryDir temporary;  ///< 测试临时目录。
    QVERIFY(temporary.isValid());
    IR::ComponentIR component = fixture();  ///< 元器件 IR 对象。
    component.footprint.models3d.append(IR::Model3DIR{});

    ExporterHorizonLibrary exporter;  ///< Horizon 导出器。
    const QString root = QDir(temporary.path()).filePath(QStringLiteral("pool"));  ///< 根目录。
    QVERIFY(!exporter.exportComponentLibrary({component}, QStringLiteral("fixture"), root, true));
    QVERIFY(exporter.diagnostics().join('\n').contains(QStringLiteral("没有有效的 STEP 或 OBJ 数据")));
}

/**
 * @brief 验证仅有 OBJ 数据时按当前 Horizon 能力拒绝导出。
 */
void TestHorizonExporter::rejectsObjOnlyModel() {
    QTemporaryDir temporary;  ///< 测试临时目录。
    QVERIFY(temporary.isValid());
    IR::ComponentIR component = fixture();  ///< 元器件 IR 对象。
    IR::Model3DIR model;  ///< 模型。
    model.setName(QStringLiteral("body.obj"));
    model.setRawObj(QStringLiteral("v 0 0 0\n"));
    component.footprint.models3d.append(model);

    ExporterHorizonLibrary exporter;  ///< Horizon 导出器。
    const QString root = QDir(temporary.path()).filePath(QStringLiteral("pool"));  ///< 根目录。
    QVERIFY(!exporter.exportComponentLibrary({component}, QStringLiteral("fixture"), root, true));
    QVERIFY(exporter.diagnostics().join('\n').contains(QStringLiteral("只有 OBJ 数据")));
    QVERIFY(exporter.diagnostics().join('\n').contains(QStringLiteral("仅支持 STEP")));
}

/**
 * @brief 验证 STEP 模型及其位置变换能够写入 Pool。
 */
void TestHorizonExporter::writesEmbeddedModelAndPlacement() {
    QTemporaryDir temporary;  ///< 测试临时目录。
    QVERIFY(temporary.isValid());
    IR::ComponentIR component = fixture();  ///< 元器件 IR 对象。
    IR::Model3DIR model;  ///< 模型。
    model.setName(QStringLiteral("body.step"));
    model.setStepData("ISO-10303-21;\n");
    model.setTranslation({1.0, 2.0, 3.0});
    model.setStepOffsetMm({0.1, 0.2, 0.3});
    model.setRotation({10.0, 20.0, 30.0});
    component.footprint.models3d.append(model);
    IR::Model3DIR secondModel;  ///< 模型。
    secondModel.setName(QStringLiteral("detail.step"));
    secondModel.setStepData("ISO-10303-21;\n");
    secondModel.setTranslation({-1.0, -2.0, -3.0});
    secondModel.setRotation({-10.0, 5.0, 90.0});
    component.footprint.models3d.append(secondModel);

    ExporterHorizonLibrary exporter;  ///< Horizon 导出器。
    const QString root = QDir(temporary.path()).filePath(QStringLiteral("pool"));  ///< 根目录。
    QVERIFY2(exporter.exportComponentLibrary({component}, QStringLiteral("fixture"), root, true),
             qPrintable(exporter.diagnostics().join('\n')));
    const QJsonObject package =
        QJsonDocument::fromJson(read(QDir(root).filePath(QStringLiteral("packages/R0603/package.json")))).object();
    const QJsonObject models = package.value(QStringLiteral("models")).toObject();  ///< 模型列表。
    QCOMPARE(models.size(), 2);
    const QString defaultModel = package.value(QStringLiteral("default_model")).toString();  ///< 模型。
    QVERIFY(models.contains(defaultModel));
    const QJsonObject modelObject = models.value(defaultModel).toObject();  ///< 对象。
    QCOMPARE(modelObject.value(QStringLiteral("x")).toInteger(), 1100000);
    QCOMPARE(modelObject.value(QStringLiteral("y")).toInteger(), 2200000);
    QCOMPARE(modelObject.value(QStringLiteral("z")).toInteger(), 3300000);
    QVERIFY(QFileInfo::exists(QDir(root).filePath(modelObject.value(QStringLiteral("filename")).toString())));
    int modelFiles = 0;  ///< 当前数量或索引值。
    for (const QJsonValue& modelValue : models) {
        if (QFileInfo::exists(QDir(root).filePath(modelValue.toObject().value(QStringLiteral("filename")).toString())))
            ++modelFiles;
    }
    QCOMPARE(modelFiles, 2);
}

/**
 * @brief 验证封装库导出选项控制三维模型文件是否写入。
 */
void TestHorizonExporter::respectsFootprintModelExportOption() {
    QTemporaryDir temporary;  ///< 测试临时目录。
    QVERIFY(temporary.isValid());
    IR::FootprintComponentIR footprint = fixture().footprint;  ///< 当前处理对象。
    IR::Model3DIR model;  ///< 模型。
    model.setName(QStringLiteral("body.step"));
    model.setStepData("ISO-10303-21;\n");
    footprint.models3d.append(model);

    ExporterHorizonLibrary exporter;  ///< Horizon 导出器。
    const QString withoutModels = QDir(temporary.path()).filePath(QStringLiteral("without-models"));  ///< 模型列表。
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

    const QString withModels = QDir(temporary.path()).filePath(QStringLiteral("with-models"));  ///< 模型列表。
    QVERIFY2(exporter.exportFootprintLibrary({footprint}, QStringLiteral("fixture"), withModels, true, true),
             qPrintable(exporter.diagnostics().join('\n')));
    const QJsonObject packageWithModels =
        QJsonDocument::fromJson(read(QDir(withModels).filePath(QStringLiteral("packages/R0603/package.json"))))
            .object();
    QCOMPARE(packageWithModels.value(QStringLiteral("models")).toObject().size(), 1);
    QVERIFY(QDir(QDir(withModels).filePath(QStringLiteral("3d_models"))).exists());
}

/**
 * @brief 验证多部件 Unit 和 UUID 生成保持稳定。
 */
void TestHorizonExporter::keepsUnitsAndUuidDeterministic() {
    QCOMPARE(HorizonUnits::mm(1.0), qint64(1000000));
    QCOMPARE(HorizonUnits::mm(-0.000001), qint64(-1));
    QCOMPARE(HorizonUnits::mm(1.2345678), qint64(1234568));
    const QString first = HorizonUuid::make(QStringLiteral("unit"), QStringLiteral("R10K"));  ///< 第一份测试数据。
    QCOMPARE(first, HorizonUuid::make(QStringLiteral("unit"), QStringLiteral("R10K")));
    QVERIFY(first != HorizonUuid::make(QStringLiteral("unit"), QStringLiteral("C10K")));
    QVERIFY(QUuid(first).isNull() == false);
}

/**
 * @brief 验证多部件符号的 Gate、Pin 和 Part 关系。
 */
void TestHorizonExporter::writesMultipartGatesAndPins() {
    QTemporaryDir temporary;  ///< 测试临时目录。
    QVERIFY(temporary.isValid());
    IR::ComponentIR component = fixture();  ///< 元器件 IR 对象。
    component.symbol.partCount = 2;
    component.symbol.pins[0].partIndex = 0;
    component.symbol.pins[1].partIndex = 1;
    ExporterHorizonLibrary exporter;  ///< Horizon 导出器。
    const QString root = QDir(temporary.path()).filePath(QStringLiteral("pool"));  ///< 根目录。
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
    const QJsonObject padMap = part.value(QStringLiteral("pad_map")).toObject();  ///< 当前 JSON 对象。
    QCOMPARE(padMap.size(), 2);
    auto firstMapping = padMap.constBegin();  ///< 当前处理对象。
    auto secondMapping = firstMapping;  ///< 当前处理对象。
    ++secondMapping;
    QVERIFY(firstMapping.value().toObject().value(QStringLiteral("gate")).toString() !=
            secondMapping.value().toObject().value(QStringLiteral("gate")).toString());
}

/**
 * @brief 验证锡膏、安装孔和原生 KeepOut Region 的序列化。
 */
void TestHorizonExporter::writesPasteMountingHoleAndKeepoutSemantics() {
    QTemporaryDir temporary;  ///< 测试临时目录。
    QVERIFY(temporary.isValid());
    IR::ComponentIR component = fixture();  ///< 元器件 IR 对象。

    IR::FootprintHoleIR hole;  ///< 当前处理对象。
    hole.center = QPointF(2.0, 1.0);
    hole.radius = 1.25;
    component.footprint.holes.append(hole);

    IR::FootprintRegionIR keepout;  ///< 当前处理对象。
    keepout.layer = IR::LayerType::KeepOut;
    keepout.isKeepOut = true;
    keepout.vertices = {QPointF(-2.0, -2.0), QPointF(2.0, -2.0), QPointF(2.0, 2.0), QPointF(-2.0, 2.0)};
    component.footprint.regions.append(keepout);

    ExporterHorizonLibrary exporter;  ///< Horizon 导出器。
    const QString root = QDir(temporary.path()).filePath(QStringLiteral("pool"));  ///< 根目录。
    QVERIFY2(exporter.exportComponentLibrary({component}, QStringLiteral("fixture"), root),
             qPrintable(exporter.diagnostics().join('\n')));

    const QJsonObject package =
        QJsonDocument::fromJson(read(QDir(root).filePath(QStringLiteral("packages/R0603/package.json")))).object();
    const QJsonObject pads = package.value(QStringLiteral("pads")).toObject();  ///< 当前 JSON 对象。
    QCOMPARE(pads.size(), 3);
    const QJsonObject keepouts = package.value(QStringLiteral("keepouts")).toObject();  ///< 当前 JSON 对象。
    QCOMPARE(keepouts.size(), 1);
    const QString polygonUuid =
        keepouts.constBegin().value().toObject().value(QStringLiteral("polygon")).toString();  ///< 标识符。
    QVERIFY(package.value(QStringLiteral("polygons")).toObject().contains(polygonUuid));

    const QJsonObject holePadstack =
        QJsonDocument::fromJson(read(QDir(root).filePath(QStringLiteral("padstacks/R0603-MH1.json")))).object();
    QCOMPARE(holePadstack.value(QStringLiteral("padstack_type")).toString(), QStringLiteral("mechanical"));
    QCOMPARE(holePadstack.value(QStringLiteral("holes")).toObject().size(), 1);

    const QJsonObject smdPadstack =
        QJsonDocument::fromJson(read(QDir(root).filePath(QStringLiteral("padstacks/R0603-1.json")))).object();
    const QString smdParameterProgram =
        smdPadstack.value(QStringLiteral("parameter_program")).toString();  ///< 当前文本值。
    QVERIFY(smdParameterProgram.contains(QStringLiteral("solder_mask_expansion")));
    QVERIFY(smdParameterProgram.contains(QStringLiteral("paste_mask_contraction")));
    QVERIFY(smdParameterProgram.contains(QStringLiteral("mm")));
    QVERIFY(!smdParameterProgram.contains(QStringLiteral("nm")));
    bool hasPaste = false;  ///< 当前状态标志。
    for (const QJsonValue& shapeValue : smdPadstack.value(QStringLiteral("shapes")).toObject()) {
        if (shapeValue.toObject().value(QStringLiteral("parameter_class")).toString() == QStringLiteral("paste")) {
            hasPaste = true;
            break;
        }
    }
    QVERIFY(hasPaste);
    const QJsonObject holeShapes = holePadstack.value(QStringLiteral("shapes")).toObject();  ///< 当前文本值。
    QCOMPARE(holeShapes.size(), 2);
    for (const QJsonValue& shapeValue : holeShapes)
        QCOMPARE(shapeValue.toObject().value(QStringLiteral("parameter_class")).toString(), QStringLiteral("mask"));

    component.footprint.pads[0].padType = IR::PadType::ThroughHole;
    component.footprint.pads[0].holeSize = 0.4;
    component.footprint.pads[0].holeLength = 0.8;
    component.footprint.pads[0].isPlated = true;
    ExporterHorizonLibrary throughHoleExporter;  ///< 当前处理对象。
    const QString throughRoot = QDir(temporary.path()).filePath(QStringLiteral("through-pool"));  ///< 根目录。
    QVERIFY2(throughHoleExporter.exportComponentLibrary({component}, QStringLiteral("fixture"), throughRoot),
             qPrintable(throughHoleExporter.diagnostics().join('\n')));
    const QJsonObject throughPadstack =
        QJsonDocument::fromJson(read(QDir(throughRoot).filePath(QStringLiteral("padstacks/R0603-1.json")))).object();
    QCOMPARE(throughPadstack.value(QStringLiteral("padstack_type")).toString(), QStringLiteral("through"));
    QCOMPARE(throughPadstack.value(QStringLiteral("holes")).toObject().size(), 1);
    int copperShapeCount = 0;  ///< 通过测试数据中的铜层形状数量。
    int maskShapeCount = 0;  ///< 通过测试数据中的阻焊层形状数量。
    for (const QJsonValue& shapeValue : throughPadstack.value(QStringLiteral("shapes")).toObject()) {
        // 当前 Padstack 形状的制造参数类别。
        const QString parameterClass = shapeValue.toObject().value(QStringLiteral("parameter_class")).toString();
        copperShapeCount += parameterClass == QStringLiteral("copper");
        maskShapeCount += parameterClass == QStringLiteral("mask");
    }
    QCOMPARE(copperShapeCount, 6);
    QCOMPARE(maskShapeCount, 2);

    IR::ComponentIR npthComponent = fixture();  ///< 当前处理对象。
    IR::FootprintPadIR npth;  ///< 当前处理对象。
    npth.position = QPointF(3.0, 0.0);
    npth.padType = IR::PadType::ThroughHole;
    npth.shape = IR::PadShape::Ellipse;
    npth.size = QSizeF(2.0, 2.0);
    npth.holeSize = 1.0;
    npth.isPlated = false;
    npthComponent.footprint.pads.append(npth);
    ExporterHorizonLibrary npthExporter;  ///< 当前处理对象。
    const QString npthRoot = QDir(temporary.path()).filePath(QStringLiteral("npth-pool"));  ///< 根目录。
    QVERIFY2(npthExporter.exportComponentLibrary({npthComponent}, QStringLiteral("fixture"), npthRoot),
             qPrintable(npthExporter.diagnostics().join('\n')));
    const QJsonObject npthPart =
        QJsonDocument::fromJson(read(QDir(npthRoot).filePath(QStringLiteral("parts/R10K.json")))).object();
    QCOMPARE(npthPart.value(QStringLiteral("pad_map")).toObject().size(), 2);
    QVERIFY(QFileInfo::exists(QDir(npthRoot).filePath(QStringLiteral("padstacks/R0603-NPTH3.json"))));
}

/**
 * @brief 验证缺少官方 Pool 集成时注册操作会失败。
 */
void TestHorizonExporter::rejectsPoolRegistrationWithoutOfficialPool() {
    QTemporaryDir temporary;  ///< 测试临时目录。
    QVERIFY(temporary.isValid());
    QStringList diagnostics;  ///< 诊断列表。
    QVERIFY(!HorizonPoolIntegration::updateAndRegister(temporary.path(), diagnostics));
    QVERIFY(!diagnostics.isEmpty());
    QVERIFY(diagnostics.constFirst().contains(QStringLiteral("pool.json")));
}

/**
 * @brief 验证注入的 Pool 集成接口能够收到注册调用。
 */
void TestHorizonExporter::invokesInjectedPoolIntegrationContract() {
    QTemporaryDir temporary;  ///< 测试临时目录。
    QVERIFY(temporary.isValid());
    const QString poolPath = QDir(temporary.path()).filePath(QStringLiteral("pool"));  ///< 路径。
    QVERIFY(QDir().mkpath(poolPath));
    QFile poolInfo(QDir(poolPath).filePath(QStringLiteral("pool.json")));
    QVERIFY(poolInfo.open(QIODevice::WriteOnly | QIODevice::Text));
    QVERIFY(poolInfo.write(
                QJsonDocument(QJsonObject{{QStringLiteral("uuid"), QUuid::createUuid().toString(QUuid::WithoutBraces)}})
                    .toJson()) > 0);
    poolInfo.close();

    const QString moduleDir = QDir(temporary.path()).filePath(QStringLiteral("python"));  ///< 目录。
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

    const QByteArray previousPython = qgetenv("EASYKICONVERTER_HORIZON_PYTHON");  ///< 当前二进制数据。
    const QByteArray previousPythonPath = qgetenv("EASYKICONVERTER_HORIZON_PYTHONPATH");  ///< 路径。
    qputenv("EASYKICONVERTER_HORIZON_PYTHON", "python3");
    qputenv("EASYKICONVERTER_HORIZON_PYTHONPATH", moduleDir.toLocal8Bit());

    QStringList diagnostics;  ///< 诊断列表。
    const bool success = HorizonPoolIntegration::updateAndRegister(poolPath, diagnostics);  ///< 当前状态标志。

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

    const QString relativeModuleDir = QDir::current().relativeFilePath(moduleDir);  ///< 目录。
    qputenv("EASYKICONVERTER_HORIZON_PYTHONPATH", relativeModuleDir.toLocal8Bit());
    diagnostics.clear();
    QVERIFY2(HorizonPoolIntegration::updateAndRegister(poolPath, diagnostics), qPrintable(diagnostics.join('\n')));

    const QString conflictPoolPath = QDir(temporary.path()).filePath(QStringLiteral("conflict-pool"));  ///< 路径。
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
    const QByteArray previousConflictPath = qgetenv("HORIZON_CONFLICT_PATH");  ///< 路径。
    qputenv("HORIZON_CONFLICT_PATH", conflictPoolPath.toLocal8Bit());
    diagnostics.clear();
    QVERIFY(!HorizonPoolIntegration::updateAndRegister(conflictPoolPath, diagnostics));
    QVERIFY(diagnostics.join('\n').contains(QStringLiteral("different UUID")));
    if (previousConflictPath.isNull())
        qunsetenv("HORIZON_CONFLICT_PATH");
    else
        qputenv("HORIZON_CONFLICT_PATH", previousConflictPath);

    const QString failedPoolPath = QDir(temporary.path()).filePath(QStringLiteral("failed-pool"));  ///< 路径。
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
