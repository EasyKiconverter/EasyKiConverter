#include "core/librepcb/ExporterLibrePcbLibrary.h"
#include "tests/common/TestPaths.hpp"

#include <QDirIterator>
#include <QFile>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest>

#include <algorithm>

using namespace EasyKiConverter;

class TestLibrePcbExporter final : public QObject {
    Q_OBJECT

private slots:
    /** @brief 验证完整组件库的四类元素、版本标记和 UUID 引用均已生成。 */
    void writesCompleteLibrary();
    /** @brief 验证重复焊盘编号会阻止生成不可关联的 LibrePCB 封装。 */
    void rejectsDuplicatePadNumbers();
    /** @brief 验证目标格式不能表达的椭圆长圆焊盘不会静默降级。 */
    void rejectsUnrepresentablePadShape();
    /** @brief 验证不在 LibrePCB 原理图栅格上的引脚会被明确拒绝。 */
    void rejectsOffGridSymbolPin();
    /** @brief 验证追加和更新模式不会覆盖现有 LibrePCB 原生库。 */
    void rejectsAppendAndUpdateModes();
    /** @brief 验证不完整组件不会生成缺少绑定关系的原生器件库。 */
    void rejectsIncompleteComponent();
    /** @brief 验证槽孔和非镀通孔会生成可回读的孔路径并保留诊断。 */
    void writesSlottedThroughHole();
    /** @brief 在提供官方 CLI 时验证完整库的打开、保存和严格检查流程。 */
    void validatesWithLibrePcbCli();
    /** @brief 验证代表性 IR golden 场景均产生预期结构或明确拒绝。 */
    void validatesRepresentativeGoldenCases();
};

static IR::ComponentIR makeComponent() {
    IR::ComponentIR component;
    component.name = QStringLiteral("R 10K");
    component.description = QStringLiteral("LibrePCB fixture");
    component.prefix = QStringLiteral("R");
    component.manufacturer = QStringLiteral("Example Manufacturer");
    component.manufacturerPart = QStringLiteral("R-10K-0603");
    component.symbol.name = QStringLiteral("R 10K Symbol");
    component.symbol.description = component.description;
    component.symbol.rectangles.append({-1.27, -2.54, 1.27, 2.54, 0.15});
    IR::SymbolTextIR symbolText;
    symbolText.text = QStringLiteral("A \"quoted\"\\path");
    symbolText.position = QPointF(0.0, -3.81);
    symbolText.visible = true;
    component.symbol.texts.append(symbolText);
    IR::SymbolPinIR pin;
    pin.name = QStringLiteral("1");
    pin.designator = QStringLiteral("1");
    pin.position = QPointF(-2.54, 0.0);
    pin.direction = IR::PinDirection::Left;
    pin.length = 2.54;
    component.symbol.pins.append(pin);
    pin.name = QStringLiteral("2");
    pin.designator = QStringLiteral("2");
    pin.position = QPointF(2.54, 0.0);
    pin.direction = IR::PinDirection::Right;
    component.symbol.pins.append(pin);
    component.footprint.name = QStringLiteral("R 10K Package");
    component.footprint.description = component.description;
    IR::FootprintPadIR pad;
    pad.number = QStringLiteral("1");
    pad.position = QPointF(-1.0, 0.0);
    pad.shape = IR::PadShape::Rect;
    pad.size = QSizeF(1.2, 0.8);
    component.footprint.pads.append(pad);
    pad.number = QStringLiteral("2");
    pad.position = QPointF(1.0, 0.0);
    component.footprint.pads.append(pad);
    component.footprint.rectangles.append({QRectF(-2.0, -1.0, 4.0, 2.0), 0.15, IR::LayerType::TopSilk, 0.0, false});
    IR::Model3DIR model;
    model.setName(QStringLiteral("Test Model"));
    model.setStepData(QByteArrayLiteral("ISO-10303-21;\nHEADER;\nENDSEC;\nDATA;\nENDSEC;\nEND-ISO-10303-21;\n"));
    component.footprint.models3d.append(model);
    return component;
}

/**
 * @brief 根据编号构造代表性 LibrePCB golden 场景。
 * @param index 场景编号，范围为 1 到 12。
 * @return 由统一 IR 构造的测试组件。
 */
static IR::ComponentIR makeGoldenComponent(int index) {
    IR::ComponentIR component = makeComponent();
    component.name = QStringLiteral("Golden %1").arg(index);
    component.symbol.name = QStringLiteral("Golden Symbol %1").arg(index);
    component.footprint.name = QStringLiteral("Golden Package %1").arg(index);
    component.manufacturerPart = QStringLiteral("GOLDEN-%1").arg(index);

    switch (index) {
        case 2: {
            component.name = QStringLiteral("Diode");
            IR::SymbolCircleIR circle;
            circle.center = QPointF(0.0, 0.0);
            circle.radius = 1.0;
            circle.strokeWidth = 0.15;
            component.symbol.circles.append(circle);
            break;
        }
        case 3: {
            component.name = QStringLiteral("Transistor");
            IR::SymbolPinIR pin = component.symbol.pins.constLast();
            pin.name = QStringLiteral("3");
            pin.designator = QStringLiteral("3");
            pin.position = QPointF(0.0, 2.54);
            pin.direction = IR::PinDirection::Up;
            component.symbol.pins.append(pin);
            break;
        }
        case 4:
        case 6: {
            component.name = index == 4 ? QStringLiteral("DIP") : QStringLiteral("Through Hole");
            for (auto& pad : component.footprint.pads) {
                pad.padType = IR::PadType::ThroughHole;
                pad.holeSize = 0.8;
                pad.isPlated = true;
            }
            break;
        }
        case 5:
            component.name = QStringLiteral("SOP QFP");
            for (int number = 3; number <= 8; ++number) {
                IR::FootprintPadIR pad = component.footprint.pads.constFirst();
                pad.number = QString::number(number);
                pad.position = QPointF(-1.0 + (number - 3) * 0.4, 1.0);
                component.footprint.pads.append(pad);
            }
            break;
        case 7:
            component.name = QStringLiteral("SMD");
            break;
        case 8: {
            component.name = QStringLiteral("Symbol Arc");
            IR::SymbolArcIR arc;
            arc.startPoint = QPointF(-1.27, 0.0);
            arc.midPoint = QPointF(0.0, 1.27);
            arc.endPoint = QPointF(1.27, 0.0);
            arc.strokeWidth = 0.15;
            component.symbol.arcs.append(arc);
            break;
        }
        case 9: {
            component.name = QStringLiteral("Symbol Polygon");
            IR::SymbolPolygonIR polygon;
            polygon.points = {QPointF(-1.27, -1.27), QPointF(1.27, -1.27), QPointF(0.0, 1.27)};
            polygon.strokeWidth = 0.15;
            polygon.isFilled = true;
            component.symbol.polygons.append(polygon);
            break;
        }
        case 10: {
            component.name = QStringLiteral("Complex Graphics");
            IR::FootprintCircleIR circle;
            circle.center = QPointF(0.0, 0.0);
            circle.radius = 1.0;
            circle.strokeWidth = 0.15;
            component.footprint.circles.append(circle);
            IR::FootprintTrackIR track;
            track.points = {QPointF(-1.0, -1.0), QPointF(1.0, 1.0)};
            track.width = 0.15;
            component.footprint.tracks.append(track);
            IR::FootprintTextIR text;
            text.text = QStringLiteral("COMPLEX");
            text.position = QPointF(0.0, 2.0);
            text.fontSize = 1.0;
            component.footprint.texts.append(text);
            break;
        }
        case 11:
            component.name = QStringLiteral("Mapped Pins");
            component.symbol.pins[0].designator = QStringLiteral("2");
            component.symbol.pins[1].designator = QStringLiteral("1");
            component.footprint.pads[0].number = QStringLiteral("2");
            component.footprint.pads[1].number = QStringLiteral("1");
            break;
        case 12:
            component.name = QStringLiteral("Multiple Units");
            component.symbol.partCount = 2;
            break;
        default:
            break;
    }
    return component;
}

void TestLibrePcbExporter::writesCompleteLibrary() {
    QTemporaryDir temporary;
    const QString outputRoot = qEnvironmentVariable("LIBREPCB_TEST_OUTPUT");
    if (outputRoot.isEmpty())
        QVERIFY(temporary.isValid());
    else
        QVERIFY(QDir().mkpath(outputRoot));
    ExporterLibrePcbLibrary exporter;
    const QString path =
        QDir(outputRoot.isEmpty() ? temporary.path() : outputRoot).filePath(QStringLiteral("library.lplib"));
    QVERIFY(exporter.exportComponentLibrary({makeComponent()}, QStringLiteral("Library"), path, true));
    QVERIFY(exporter.diagnostics().join('\n').contains(QStringLiteral("Package Outlines 回退")));
    QVERIFY(exporter.diagnostics().join('\n').contains(QStringLiteral("courtyard")));
    QVERIFY(QFileInfo::exists(QDir(path).filePath(QStringLiteral(".librepcb-lib"))));
    QVERIFY(QFileInfo::exists(QDir(path).filePath(QStringLiteral("library.lp"))));

    QDirIterator iterator(path, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    QStringList files;
    while (iterator.hasNext())
        files.append(iterator.next());
    const auto findElement = [&files](const QString& directory, const QString& fileName) {
        return std::any_of(files.cbegin(), files.cend(), [&directory, &fileName](const QString& pathItem) {
            return pathItem.contains(QStringLiteral("/") + directory + QStringLiteral("/")) &&
                   pathItem.endsWith(fileName);
        });
    };
    QVERIFY(findElement(QStringLiteral("sym"), QStringLiteral("symbol.lp")));
    QVERIFY(findElement(QStringLiteral("pkg"), QStringLiteral("package.lp")));
    QVERIFY(findElement(QStringLiteral("cmp"), QStringLiteral("component.lp")));
    QVERIFY(findElement(QStringLiteral("dev"), QStringLiteral("device.lp")));

    const QString componentPath = *std::find_if(files.cbegin(), files.cend(), [](const QString& pathItem) {
        return pathItem.contains(QStringLiteral("/cmp/")) && pathItem.endsWith(QStringLiteral("component.lp"));
    });
    const QString symbolPath = *std::find_if(files.cbegin(), files.cend(), [](const QString& pathItem) {
        return pathItem.contains(QStringLiteral("/sym/")) && pathItem.endsWith(QStringLiteral("symbol.lp"));
    });
    QFile symbolFile(symbolPath);
    QVERIFY(symbolFile.open(QIODevice::ReadOnly));
    const QByteArray symbolData = symbolFile.readAll();
    QVERIFY(symbolData.contains(QStringLiteral("A \\\"quoted\\\"\\\\path").toUtf8()));
    QFile componentFile(componentPath);
    QVERIFY(componentFile.open(QIODevice::ReadOnly));
    const QByteArray componentData = componentFile.readAll();
    QVERIFY(componentData.startsWith("(librepcb_component "));
    QVERIFY(componentData.contains("(variant "));
    QVERIFY(componentData.contains("(signal "));

    const QString packagePath = *std::find_if(files.cbegin(), files.cend(), [](const QString& pathItem) {
        return pathItem.contains(QStringLiteral("/pkg/")) && pathItem.endsWith(QStringLiteral("package.lp"));
    });
    QFile packageFile(packagePath);
    QVERIFY(packageFile.open(QIODevice::ReadOnly));
    const QByteArray packageData = packageFile.readAll();
    QVERIFY(packageData.startsWith("(librepcb_package "));
    QVERIFY(packageData.contains("(package_pad "));
    QVERIFY(packageData.contains("function standard"));
    QVERIFY(packageData.contains("(3d_model "));

    const QString devicePath = *std::find_if(files.cbegin(), files.cend(), [](const QString& pathItem) {
        return pathItem.contains(QStringLiteral("/dev/")) && pathItem.endsWith(QStringLiteral("device.lp"));
    });
    QFile deviceFile(devicePath);
    QVERIFY(deviceFile.open(QIODevice::ReadOnly));
    const QByteArray deviceData = deviceFile.readAll();
    QVERIFY(deviceData.contains("(part \"R-10K-0603\""));

    QFile goldenFile(Test::TestPaths::goldenPath(QStringLiteral("librepcb/minimal_structure.txt")));
    QVERIFY(goldenFile.open(QIODevice::ReadOnly));
    const QList<QByteArray> requiredTokens = goldenFile.readAll().split('\n');
    QByteArray allData = symbolData + componentData + packageData + deviceData;
    for (const QByteArray& token : requiredTokens) {
        if (!token.trimmed().isEmpty())
            QVERIFY2(allData.contains(token.trimmed()), token.constData());
    }
}

void TestLibrePcbExporter::rejectsDuplicatePadNumbers() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    IR::FootprintComponentIR footprint;
    footprint.name = QStringLiteral("duplicate");
    IR::FootprintPadIR first;
    first.number = QStringLiteral("1");
    first.shape = IR::PadShape::Rect;
    first.size = QSizeF(1.0, 1.0);
    footprint.pads.append(first);
    first.number = QStringLiteral(" 1 ");
    footprint.pads.append(first);
    ExporterLibrePcbLibrary exporter;
    QVERIFY(!exporter.exportFootprintLibrary(
        {footprint}, QStringLiteral("duplicate"), QDir(temporary.path()).filePath(QStringLiteral("duplicate.lplib"))));
    QVERIFY(exporter.diagnostics().join('\n').contains(QStringLiteral("重复焊盘编号")));
}

void TestLibrePcbExporter::rejectsUnrepresentablePadShape() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    IR::FootprintComponentIR footprint;
    footprint.name = QStringLiteral("oval");
    IR::FootprintPadIR pad;
    pad.number = QStringLiteral("1");
    pad.shape = IR::PadShape::Oval;
    pad.size = QSizeF(2.0, 1.0);
    footprint.pads.append(pad);
    ExporterLibrePcbLibrary exporter;
    QVERIFY(!exporter.exportFootprintLibrary(
        {footprint}, QStringLiteral("oval"), QDir(temporary.path()).filePath(QStringLiteral("oval.lplib"))));
    QVERIFY(exporter.diagnostics().join('\n').contains(QStringLiteral("椭圆长圆")));
}

void TestLibrePcbExporter::rejectsOffGridSymbolPin() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    IR::ComponentIR component = makeComponent();
    component.symbol.pins[0].position = QPointF(-1.0, 0.0);
    ExporterLibrePcbLibrary exporter;
    QVERIFY(!exporter.exportComponentLibrary(
        {component}, QStringLiteral("off-grid"), QDir(temporary.path()).filePath(QStringLiteral("off-grid.lplib"))));
    QVERIFY(exporter.diagnostics().join('\n').contains(QStringLiteral("不在 2.54 mm 栅格")));
}

void TestLibrePcbExporter::rejectsAppendAndUpdateModes() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    ExporterLibrePcbLibrary exporter;
    const QString path = QDir(temporary.path()).filePath(QStringLiteral("library.lplib"));
    QVERIFY(!exporter.exportSymbolLibrary({}, QStringLiteral("library"), path, true, false));
    QVERIFY(exporter.diagnostics().join('\n').contains(QStringLiteral("追加或更新")));
    QVERIFY(!exporter.exportSymbolLibrary({}, QStringLiteral("library"), path, false, true));
}

void TestLibrePcbExporter::rejectsIncompleteComponent() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    IR::ComponentIR component;
    component.name = QStringLiteral("Incomplete");
    component.symbol.name = QStringLiteral("Only Symbol");
    ExporterLibrePcbLibrary exporter;
    QVERIFY(!exporter.exportComponentLibrary(
        {component}, QStringLiteral("Incomplete"), QDir(temporary.path()).filePath(QStringLiteral("library.lplib"))));
    QVERIFY(exporter.diagnostics().join('\n').contains(QStringLiteral("封装名称为空")));
}

void TestLibrePcbExporter::writesSlottedThroughHole() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    IR::FootprintComponentIR footprint;
    footprint.name = QStringLiteral("Slotted Header");
    IR::FootprintPadIR pad;
    pad.number = QStringLiteral("1");
    pad.padType = IR::PadType::ThroughHole;
    pad.shape = IR::PadShape::Rect;
    pad.size = QSizeF(2.0, 2.0);
    pad.holeSize = 0.8;
    pad.holeLength = 1.2;
    pad.isPlated = false;
    footprint.pads.append(pad);
    ExporterLibrePcbLibrary exporter;
    const QString path = QDir(temporary.path()).filePath(QStringLiteral("slotted.lplib"));
    QVERIFY(exporter.exportFootprintLibrary({footprint}, QStringLiteral("Slotted Header"), path));
    QVERIFY(exporter.diagnostics().join('\n').contains(QStringLiteral("非镀通孔")));

    QDirIterator iterator(path, QDir::Files, QDirIterator::Subdirectories);
    QString packagePath;
    while (iterator.hasNext()) {
        const QString candidate = iterator.next();
        if (candidate.endsWith(QStringLiteral("package.lp"))) {
            packagePath = candidate;
            break;
        }
    }
    QVERIFY(!packagePath.isEmpty());
    QFile packageFile(packagePath);
    QVERIFY(packageFile.open(QIODevice::ReadOnly));
    const QByteArray packageData = packageFile.readAll();
    QVERIFY(packageData.contains("(position -0.6 0 "));
    QVERIFY(packageData.contains("(position 0.6 0 "));
}

void TestLibrePcbExporter::validatesRepresentativeGoldenCases() {
    struct GoldenCase {
        int index;
        QString fileName;
        bool succeeds;
    };

    const QList<GoldenCase> cases = {
        {1, QStringLiteral("representative-01.txt"), true},
        {2, QStringLiteral("representative-02.txt"), true},
        {3, QStringLiteral("representative-03.txt"), true},
        {4, QStringLiteral("representative-04.txt"), true},
        {5, QStringLiteral("representative-05.txt"), true},
        {6, QStringLiteral("representative-06.txt"), true},
        {7, QStringLiteral("representative-07.txt"), true},
        {8, QStringLiteral("representative-08.txt"), true},
        {9, QStringLiteral("representative-09.txt"), true},
        {10, QStringLiteral("representative-10.txt"), true},
        {11, QStringLiteral("representative-11.txt"), true},
        {12, QStringLiteral("representative-12.txt"), false},
    };

    for (const GoldenCase& goldenCase : cases) {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        QFile goldenFile(Test::TestPaths::goldenPath(QStringLiteral("librepcb/%1").arg(goldenCase.fileName)));
        QVERIFY(goldenFile.open(QIODevice::ReadOnly));
        const QByteArray expectedToken = goldenFile.readAll().trimmed();
        QVERIFY(!expectedToken.isEmpty());
        const IR::ComponentIR component = makeGoldenComponent(goldenCase.index);
        ExporterLibrePcbLibrary exporter;
        const QString outputPath =
            QDir(temporary.path()).filePath(QStringLiteral("golden-%1.lplib").arg(goldenCase.index));
        const bool succeeded = exporter.exportComponentLibrary(
            {component}, QStringLiteral("Golden %1").arg(goldenCase.index), outputPath, true);
        QCOMPARE(succeeded, goldenCase.succeeds);
        const QString diagnostics = exporter.diagnostics().join('\n');
        if (!goldenCase.succeeds) {
            QVERIFY(diagnostics.toUtf8().contains(expectedToken));
            continue;
        }

        QDirIterator iterator(outputPath, QDir::Files, QDirIterator::Subdirectories);
        QByteArray serialized;
        while (iterator.hasNext()) {
            QFile file(iterator.next());
            if (file.open(QIODevice::ReadOnly))
                serialized.append(file.readAll());
        }
        QVERIFY2(serialized.contains(expectedToken), expectedToken.constData());
    }
}

void TestLibrePcbExporter::validatesWithLibrePcbCli() {
    const QString cli = qEnvironmentVariable("LIBREPCB_CLI");
    if (cli.isEmpty())
        QSKIP("设置 LIBREPCB_CLI 后运行 LibrePCB 2.1.1 官方 CLI 集成验证");

    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    ExporterLibrePcbLibrary exporter;
    const QString path = QDir(temporary.path()).filePath(QStringLiteral("library.lplib"));
    QVERIFY(exporter.exportComponentLibrary({makeComponent()}, QStringLiteral("Library"), path, true));

    const auto runCli = [&cli, &path](const QStringList& arguments) {
        QProcess process;
        process.start(cli, arguments);
        if (!process.waitForFinished(30000))
            return -1;
        return process.exitCode();
    };
    QCOMPARE(runCli({QStringLiteral("open-library"), QStringLiteral("--all"), QStringLiteral("--save"), path}), 0);
    QCOMPARE(runCli({QStringLiteral("open-library"), QStringLiteral("--all"), QStringLiteral("--check"), path}), 0);
    QCOMPARE(runCli({QStringLiteral("open-library"),
                     QStringLiteral("--all"),
                     QStringLiteral("--check"),
                     QStringLiteral("--strict"),
                     path}),
             0);
}

QTEST_MAIN(TestLibrePcbExporter)
#include "test_librepcb_exporter.moc"
