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
    /**
     * @brief 验证完整组件库的四类元素、版本标记和 UUID 引用均已生成。
     */
    void writesCompleteLibrary();
    /**
     * @brief 验证绝对输入坐标会归一化到 LibrePCB 封装原点。
     */
    void normalizesAbsoluteFootprintCoordinates();
    /**
     * @brief 验证重复焊盘编号会阻止生成不可关联的 LibrePCB 封装。
     */
    void rejectsDuplicatePadNumbers();
    /**
     * @brief 验证目标格式不能表达的椭圆长圆焊盘不会静默降级。
     */
    void rejectsUnrepresentablePadShape();
    /**
     * @brief 验证不在 LibrePCB 原理图栅格上的引脚会被明确拒绝。
     */
    void rejectsOffGridSymbolPin();
    /**
     * @brief 验证接近栅格的引脚会被自动对齐并保留诊断。
     */
    void snapsNearGridSymbolPin();
    /**
     * @brief 验证符号引脚连接方向、默认字段位置和异常文本坐标。
     */
    void normalizesSymbolPinConnectionsAndTextBounds();
    /**
     * @brief 验证追加和更新模式不会覆盖现有 LibrePCB 原生库。
     */
    void rejectsAppendAndUpdateModes();
    /**
     * @brief 验证不完整组件不会生成缺少绑定关系的原生器件库。
     */
    void rejectsIncompleteComponent();
    /**
     * @brief 验证槽孔和非镀通孔会生成可回读的孔路径并保留诊断。
     */
    void writesSlottedThroughHole();
    /**
     * @brief 在提供官方 CLI 时验证完整库的打开、保存和严格检查流程。
     */
    void validatesWithLibrePcbCli();
    /**
     * @brief 验证代表性 IR golden 场景均产生预期结构或明确拒绝。
     */
    void validatesRepresentativeGoldenCases();
};

static IR::ComponentIR makeComponent() {
    // component：测试使用的缓存或元器件对象。
    IR::ComponentIR component;
    component.name = QStringLiteral("R 10K");
    component.description = QStringLiteral("LibrePCB fixture");
    component.prefix = QStringLiteral("R");
    component.manufacturer = QStringLiteral("Example Manufacturer");
    component.manufacturerPart = QStringLiteral("R-10K-0603");
    component.symbol.name = QStringLiteral("R 10K Symbol");
    component.symbol.description = component.description;
    component.symbol.rectangles.append({-1.27, -2.54, 1.27, 2.54, 0.15});
    // symbolText：测试使用的文本或数据内容。
    IR::SymbolTextIR symbolText;
    symbolText.text = QStringLiteral("A \"quoted\"\\path");
    symbolText.position = QPointF(0.0, -3.81);
    symbolText.visible = true;
    component.symbol.texts.append(symbolText);
    // pin：测试用引脚数据。
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
    // pad：测试用焊盘数据。
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
    // model：测试使用的配置值或状态。
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
    // component：测试使用的缓存或元器件对象。
    IR::ComponentIR component = makeComponent();
    component.name = QStringLiteral("Golden %1").arg(index);
    component.symbol.name = QStringLiteral("Golden Symbol %1").arg(index);
    component.footprint.name = QStringLiteral("Golden Package %1").arg(index);
    component.manufacturerPart = QStringLiteral("GOLDEN-%1").arg(index);

    /**
     * @brief 验证 switch 测试场景。
     */
    switch (index) {
        case 2: {
            component.name = QStringLiteral("Diode");
            // circle：测试用圆形图元。
            IR::SymbolCircleIR circle;
            circle.center = QPointF(0.0, 0.0);
            circle.radius = 1.0;
            circle.strokeWidth = 0.15;
            component.symbol.circles.append(circle);
            break;
        }
        case 3: {
            component.name = QStringLiteral("Transistor");
            // pin：测试用引脚数据。
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
            for (int padIndex = 0; padIndex < component.footprint.pads.size(); ++padIndex) {
                // pad：测试用焊盘数据。
                auto& pad = component.footprint.pads[padIndex];
                pad.padType = IR::PadType::ThroughHole;
                pad.size = QSizeF(2.0, 2.0);
                pad.position = QPointF(-2.0 + padIndex * 4.0, 0.0);
                pad.holeSize = 0.8;
                pad.isPlated = true;
            }
            component.footprint.rectangles.clear();
            break;
        }
        case 5:
            component.name = QStringLiteral("SOP QFP");
            component.footprint.pads[0].size = QSizeF(0.5, 0.8);
            component.footprint.pads[0].position = QPointF(-3.2, 0.0);
            component.footprint.pads[1].size = QSizeF(0.5, 0.8);
            component.footprint.pads[1].position = QPointF(3.2, 0.0);
            for (int number = 3; number <= 8; ++number) {
                // pad：测试用焊盘数据。
                IR::FootprintPadIR pad = component.footprint.pads.constFirst();
                pad.number = QString::number(number);
                pad.size = QSizeF(0.5, 0.8);
                pad.position = QPointF(-2.0 + (number - 3) * 0.8, 0.0);
                component.footprint.pads.append(pad);
            }
            component.footprint.rectangles.clear();
            break;
        case 7:
            component.name = QStringLiteral("SMD");
            break;
        case 8: {
            component.name = QStringLiteral("Symbol Arc");
            // arc：测试用圆弧图元。
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
            // polygon：测试用多边形图元。
            IR::SymbolPolygonIR polygon;
            polygon.points = {QPointF(-1.27, -1.27), QPointF(1.27, -1.27), QPointF(0.0, 1.27)};
            polygon.strokeWidth = 0.15;
            polygon.isFilled = true;
            component.symbol.polygons.append(polygon);
            break;
        }
        case 10: {
            component.name = QStringLiteral("Complex Graphics");
            // circle：测试用圆形图元。
            IR::FootprintCircleIR circle;
            circle.center = QPointF(0.0, 0.0);
            circle.radius = 1.0;
            circle.strokeWidth = 0.15;
            component.footprint.circles.append(circle);
            // track：测试用线段图元。
            IR::FootprintTrackIR track;
            track.points = {QPointF(-1.0, -1.0), QPointF(1.0, 1.0)};
            track.width = 0.15;
            component.footprint.tracks.append(track);
            // text：测试使用的文本或数据内容。
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

/**
 * @brief 验证 writesCompleteLibrary 测试场景。
 */
void TestLibrePcbExporter::writesCompleteLibrary() {
    // temporary：测试隔离环境中的临时对象。
    QTemporaryDir temporary;
    // outputRoot：测试使用的文件系统路径。
    const QString outputRoot = qEnvironmentVariable("LIBREPCB_TEST_OUTPUT");
    if (outputRoot.isEmpty())
        QVERIFY(temporary.isValid());
    else
        QVERIFY(QDir().mkpath(outputRoot));
    // exporter：负责生成测试输出的导出器实例。
    ExporterLibrePcbLibrary exporter;
    const QString path =
        QDir(outputRoot.isEmpty() ? temporary.path() : outputRoot).filePath(QStringLiteral("library.lplib"));
    QVERIFY(exporter.exportComponentLibrary({makeComponent()}, QStringLiteral("Library"), path, true));
    QVERIFY(exporter.diagnostics().join('\n').contains(QStringLiteral("Package Outlines 回退")));
    QVERIFY(exporter.diagnostics().join('\n').contains(QStringLiteral("courtyard")));
    QVERIFY(QFileInfo::exists(QDir(path).filePath(QStringLiteral(".librepcb-lib"))));
    QVERIFY(QFileInfo::exists(QDir(path).filePath(QStringLiteral("library.lp"))));

    QDirIterator iterator(path, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    // files：测试使用的文件系统路径。
    QStringList files;
    while (iterator.hasNext())
        files.append(iterator.next());
    const auto findElement = [&files](const QString& directory, const QString& fileName) {
        /**
         * @brief 验证 any_of 测试场景。
         */
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
    // symbolData：测试使用的文本或数据内容。
    const QByteArray symbolData = symbolFile.readAll();
    QVERIFY(symbolData.contains(QStringLiteral("A \\\"quoted\\\"\\\\path").toUtf8()));
    QFile componentFile(componentPath);
    QVERIFY(componentFile.open(QIODevice::ReadOnly));
    // componentData：测试使用的文本或数据内容。
    const QByteArray componentData = componentFile.readAll();
    QVERIFY(componentData.startsWith("(librepcb_component "));
    QVERIFY(componentData.contains("(variant "));
    QVERIFY(componentData.contains("(signal "));

    const QString packagePath = *std::find_if(files.cbegin(), files.cend(), [](const QString& pathItem) {
        return pathItem.contains(QStringLiteral("/pkg/")) && pathItem.endsWith(QStringLiteral("package.lp"));
    });
    QFile packageFile(packagePath);
    QVERIFY(packageFile.open(QIODevice::ReadOnly));
    // packageData：测试使用的文本或数据内容。
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
    // deviceData：测试使用的文本或数据内容。
    const QByteArray deviceData = deviceFile.readAll();
    QVERIFY(deviceData.contains("(part \"R-10K-0603\""));

    QFile goldenFile(Test::TestPaths::goldenPath(QStringLiteral("librepcb/minimal_structure.txt")));
    QVERIFY(goldenFile.open(QIODevice::ReadOnly));
    // requiredTokens：测试解析得到的条目集合。
    const QList<QByteArray> requiredTokens = goldenFile.readAll().split('\n');
    // allData：测试使用的文本或数据内容。
    QByteArray allData = symbolData + componentData + packageData + deviceData;
    for (const QByteArray& token : requiredTokens) {
        if (!token.trimmed().isEmpty())
            QVERIFY2(allData.contains(token.trimmed()), token.constData());
    }
}

/**
 * @brief 验证 normalizesAbsoluteFootprintCoordinates 测试场景。
 */
void TestLibrePcbExporter::normalizesAbsoluteFootprintCoordinates() {
    // temporary：测试隔离环境中的临时对象。
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    // footprint：测试用封装数据。
    IR::FootprintComponentIR footprint;
    footprint.name = QStringLiteral("Absolute Coordinates");
    for (int index = 0; index < 2; ++index) {
        // pad：测试用焊盘数据。
        IR::FootprintPadIR pad;
        pad.number = QString::number(index + 1);
        pad.position = QPointF(1000.0 + index * 2.0, 765.0);
        pad.size = QSizeF(1.0, 1.0);
        footprint.pads.append(pad);
    }
    footprint.rectangles.append({QRectF(999.0, 764.0, 4.0, 2.0), 0.15, IR::LayerType::TopSilk, 0.0, false});

    // exporter：负责生成测试输出的导出器实例。
    ExporterLibrePcbLibrary exporter;
    // path：测试使用的文件系统路径。
    const QString path = QDir(temporary.path()).filePath(QStringLiteral("absolute.lplib"));
    QVERIFY(exporter.exportFootprintLibrary({footprint}, QStringLiteral("absolute"), path));

    QDirIterator iterator(path, {QStringLiteral("package.lp")}, QDir::Files, QDirIterator::Subdirectories);
    QVERIFY(iterator.hasNext());
    QFile packageFile(iterator.next());
    QVERIFY(packageFile.open(QIODevice::ReadOnly));
    // packageData：测试使用的文本或数据内容。
    const QByteArray packageData = packageFile.readAll();
    QVERIFY(packageData.contains("(position -1.0 0.0 )"));
    QVERIFY(packageData.contains("(position 1.0 0.0 )"));
    QVERIFY(!packageData.contains("1000.0"));
    QVERIFY(exporter.diagnostics().join('\n').contains(QStringLiteral("归一化到封装原点")));
}

/**
 * @brief 验证 rejectsDuplicatePadNumbers 测试场景。
 */
void TestLibrePcbExporter::rejectsDuplicatePadNumbers() {
    // temporary：测试隔离环境中的临时对象。
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    // footprint：测试用封装数据。
    IR::FootprintComponentIR footprint;
    footprint.name = QStringLiteral("duplicate");
    // first：第一个测试结果。
    IR::FootprintPadIR first;
    first.number = QStringLiteral("1");
    first.shape = IR::PadShape::Rect;
    first.size = QSizeF(1.0, 1.0);
    footprint.pads.append(first);
    first.number = QStringLiteral(" 1 ");
    footprint.pads.append(first);
    // exporter：负责生成测试输出的导出器实例。
    ExporterLibrePcbLibrary exporter;
    QVERIFY(!exporter.exportFootprintLibrary(
        {footprint}, QStringLiteral("duplicate"), QDir(temporary.path()).filePath(QStringLiteral("duplicate.lplib"))));
    QVERIFY(exporter.diagnostics().join('\n').contains(QStringLiteral("重复焊盘编号")));
}

/**
 * @brief 验证 rejectsUnrepresentablePadShape 测试场景。
 */
void TestLibrePcbExporter::rejectsUnrepresentablePadShape() {
    // temporary：测试隔离环境中的临时对象。
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    // footprint：测试用封装数据。
    IR::FootprintComponentIR footprint;
    footprint.name = QStringLiteral("oval");
    // pad：测试用焊盘数据。
    IR::FootprintPadIR pad;
    pad.number = QStringLiteral("1");
    pad.shape = IR::PadShape::Oval;
    pad.size = QSizeF(2.0, 1.0);
    footprint.pads.append(pad);
    // exporter：负责生成测试输出的导出器实例。
    ExporterLibrePcbLibrary exporter;
    QVERIFY(!exporter.exportFootprintLibrary(
        {footprint}, QStringLiteral("oval"), QDir(temporary.path()).filePath(QStringLiteral("oval.lplib"))));
    QVERIFY(exporter.diagnostics().join('\n').contains(QStringLiteral("椭圆长圆")));
}

/**
 * @brief 验证 rejectsOffGridSymbolPin 测试场景。
 */
void TestLibrePcbExporter::rejectsOffGridSymbolPin() {
    // temporary：测试隔离环境中的临时对象。
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    // component：测试使用的缓存或元器件对象。
    IR::ComponentIR component = makeComponent();
    component.symbol.pins[0].position = QPointF(-1.0, 0.0);
    // exporter：负责生成测试输出的导出器实例。
    ExporterLibrePcbLibrary exporter;
    QVERIFY(!exporter.exportComponentLibrary(
        {component}, QStringLiteral("off-grid"), QDir(temporary.path()).filePath(QStringLiteral("off-grid.lplib"))));
    QVERIFY(exporter.diagnostics().join('\n').contains(QStringLiteral("不在 2.54 mm 栅格")));
}

/**
 * @brief 验证 snapsNearGridSymbolPin 测试场景。
 */
void TestLibrePcbExporter::snapsNearGridSymbolPin() {
    // temporary：测试隔离环境中的临时对象。
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    // component：测试使用的缓存或元器件对象。
    IR::ComponentIR component = makeComponent();
    component.symbol.pins[1].position = QPointF(3.81, 0.0);
    // exporter：负责生成测试输出的导出器实例。
    ExporterLibrePcbLibrary exporter;
    QVERIFY(exporter.exportComponentLibrary({component},
                                            QStringLiteral("snap-near-grid"),
                                            QDir(temporary.path()).filePath(QStringLiteral("library.lplib"))));
    QVERIFY(exporter.diagnostics().join('\n').contains(QStringLiteral("已自动对齐到 2.54 mm 栅格")));
}

/**
 * @brief 验证 normalizesSymbolPinConnectionsAndTextBounds 测试场景。
 */
void TestLibrePcbExporter::normalizesSymbolPinConnectionsAndTextBounds() {
    // temporary：测试隔离环境中的临时对象。
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    // component：测试使用的缓存或元器件对象。
    IR::ComponentIR component = makeComponent();
    // outlier：越界几何点。
    IR::SymbolTextIR outlier;
    outlier.text = QStringLiteral("orphan source text");
    outlier.position = QPointF(-101.6, -25.4);
    component.symbol.texts.append(outlier);

    // exporter：负责生成测试输出的导出器实例。
    ExporterLibrePcbLibrary exporter;
    // path：测试使用的文件系统路径。
    const QString path = QDir(temporary.path()).filePath(QStringLiteral("normalized.lplib"));
    QVERIFY(exporter.exportComponentLibrary({component}, QStringLiteral("normalized"), path));

    QDirIterator iterator(path, {QStringLiteral("symbol.lp")}, QDir::Files, QDirIterator::Subdirectories);
    QVERIFY(iterator.hasNext());
    QFile symbolFile(iterator.next());
    QVERIFY(symbolFile.open(QIODevice::ReadOnly));
    // symbolData：测试使用的文本或数据内容。
    const QByteArray symbolData = symbolFile.readAll();

    QVERIFY(symbolData.contains("(rotation 0.0 )"));
    QVERIFY(symbolData.contains("(rotation 180.0 )"));
    QVERIFY(symbolData.contains("(position 0 -5.08 )"));
    QVERIFY(!symbolData.contains("-101.6 -25.4"));
    QVERIFY(symbolData.contains("orphan source text"));
    QVERIFY(exporter.diagnostics().join('\n').contains(QStringLiteral("超出图形边界")));
}

/**
 * @brief 验证 rejectsAppendAndUpdateModes 测试场景。
 */
void TestLibrePcbExporter::rejectsAppendAndUpdateModes() {
    // temporary：测试隔离环境中的临时对象。
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    // exporter：负责生成测试输出的导出器实例。
    ExporterLibrePcbLibrary exporter;
    // path：测试使用的文件系统路径。
    const QString path = QDir(temporary.path()).filePath(QStringLiteral("library.lplib"));
    QVERIFY(!exporter.exportSymbolLibrary({}, QStringLiteral("library"), path, true, false));
    QVERIFY(exporter.diagnostics().join('\n').contains(QStringLiteral("追加或更新")));
    QVERIFY(!exporter.exportSymbolLibrary({}, QStringLiteral("library"), path, false, true));
}

/**
 * @brief 验证 rejectsIncompleteComponent 测试场景。
 */
void TestLibrePcbExporter::rejectsIncompleteComponent() {
    // temporary：测试隔离环境中的临时对象。
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    // component：测试使用的缓存或元器件对象。
    IR::ComponentIR component;
    component.name = QStringLiteral("Incomplete");
    component.symbol.name = QStringLiteral("Only Symbol");
    // exporter：负责生成测试输出的导出器实例。
    ExporterLibrePcbLibrary exporter;
    QVERIFY(!exporter.exportComponentLibrary(
        {component}, QStringLiteral("Incomplete"), QDir(temporary.path()).filePath(QStringLiteral("library.lplib"))));
    QVERIFY(exporter.diagnostics().join('\n').contains(QStringLiteral("封装名称为空")));
}

/**
 * @brief 验证 writesSlottedThroughHole 测试场景。
 */
void TestLibrePcbExporter::writesSlottedThroughHole() {
    // temporary：测试隔离环境中的临时对象。
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    // footprint：测试用封装数据。
    IR::FootprintComponentIR footprint;
    footprint.name = QStringLiteral("Slotted Header");
    // pad：测试用焊盘数据。
    IR::FootprintPadIR pad;
    pad.number = QStringLiteral("1");
    pad.padType = IR::PadType::ThroughHole;
    pad.shape = IR::PadShape::Rect;
    pad.size = QSizeF(2.0, 2.0);
    pad.holeSize = 0.8;
    pad.holeLength = 1.2;
    pad.isPlated = false;
    footprint.pads.append(pad);
    // exporter：负责生成测试输出的导出器实例。
    ExporterLibrePcbLibrary exporter;
    // path：测试使用的文件系统路径。
    const QString path = QDir(temporary.path()).filePath(QStringLiteral("slotted.lplib"));
    QVERIFY(exporter.exportFootprintLibrary({footprint}, QStringLiteral("Slotted Header"), path));
    QVERIFY(exporter.diagnostics().join('\n').contains(QStringLiteral("非镀通孔")));

    QDirIterator iterator(path, QDir::Files, QDirIterator::Subdirectories);
    // packagePath：测试使用的文件系统路径。
    QString packagePath;
    while (iterator.hasNext()) {
        // candidate：测试使用的元器件标识或名称。
        const QString candidate = iterator.next();
        if (candidate.endsWith(QStringLiteral("package.lp"))) {
            packagePath = candidate;
            break;
        }
    }
    QVERIFY(!packagePath.isEmpty());
    QFile packageFile(packagePath);
    QVERIFY(packageFile.open(QIODevice::ReadOnly));
    // packageData：测试使用的文本或数据内容。
    const QByteArray packageData = packageFile.readAll();
    QVERIFY(packageData.contains("(position -0.6 0 "));
    QVERIFY(packageData.contains("(position 0.6 0 "));
}

/**
 * @brief 验证 validatesRepresentativeGoldenCases 测试场景。
 */
void TestLibrePcbExporter::validatesRepresentativeGoldenCases() {
    struct GoldenCase {
        // index：测试使用的数量或索引值。
        int index;
        // fileName：测试使用的文件系统路径。
        QString fileName;
        // succeeds：调用是否成功。
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
        // temporary：测试隔离环境中的临时对象。
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        QFile goldenFile(Test::TestPaths::goldenPath(QStringLiteral("librepcb/%1").arg(goldenCase.fileName)));
        QVERIFY(goldenFile.open(QIODevice::ReadOnly));
        // expectedToken：测试期望的结果数据。
        const QByteArray expectedToken = goldenFile.readAll().trimmed();
        QVERIFY(!expectedToken.isEmpty());
        // component：测试使用的缓存或元器件对象。
        const IR::ComponentIR component = makeGoldenComponent(goldenCase.index);
        // exporter：负责生成测试输出的导出器实例。
        ExporterLibrePcbLibrary exporter;
        const QString outputPath =
            QDir(temporary.path()).filePath(QStringLiteral("golden-%1.lplib").arg(goldenCase.index));
        // succeeded：前一步操作是否成功。
        const bool succeeded = exporter.exportComponentLibrary(
            {component}, QStringLiteral("Golden %1").arg(goldenCase.index), outputPath, true);
        QCOMPARE(succeeded, goldenCase.succeeds);
        // diagnostics：导出过程中收集的诊断信息。
        const QString diagnostics = exporter.diagnostics().join('\n');
        if (!goldenCase.succeeds) {
            QVERIFY(diagnostics.toUtf8().contains(expectedToken));
            continue;
        }

        QDirIterator iterator(outputPath, QDir::Files, QDirIterator::Subdirectories);
        // serialized：序列化后的文件内容。
        QByteArray serialized;
        while (iterator.hasNext()) {
            QFile file(iterator.next());
            if (file.open(QIODevice::ReadOnly))
                serialized.append(file.readAll());
        }
        QVERIFY2(serialized.contains(expectedToken), expectedToken.constData());
    }
}

/**
 * @brief 验证 validatesWithLibrePcbCli 测试场景。
 */
void TestLibrePcbExporter::validatesWithLibrePcbCli() {
    // cli：命令行导出器实例。
    const QString cli = qEnvironmentVariable("LIBREPCB_CLI");
    if (cli.isEmpty())
        QSKIP("设置 LIBREPCB_CLI 后运行 LibrePCB 2.1.1 官方 CLI 集成验证");

    // temporary：测试隔离环境中的临时对象。
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    // exporter：负责生成测试输出的导出器实例。
    ExporterLibrePcbLibrary exporter;
    // path：测试使用的文件系统路径。
    const QString path = QDir(temporary.path()).filePath(QStringLiteral("library.lplib"));
    QVERIFY(exporter.exportComponentLibrary({makeComponent()}, QStringLiteral("Library"), path, true));

    const auto runCli = [&cli, &path](const QStringList& arguments) {
        // process：用于观察导出状态的进程对象。
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

    // 代表性成功场景也必须经过同一套官方解析、保存和严格检查流程。
    for (int index = 1; index <= 11; ++index) {
        // goldenTemporary：测试隔离环境中的临时对象。
        QTemporaryDir goldenTemporary;
        QVERIFY(goldenTemporary.isValid());
        // goldenPath：测试使用的文件系统路径。
        const QString goldenPath = QDir(goldenTemporary.path()).filePath(QStringLiteral("golden-%1.lplib").arg(index));
        // goldenExporter：负责生成测试输出的导出器实例。
        ExporterLibrePcbLibrary goldenExporter;
        QVERIFY(goldenExporter.exportComponentLibrary(
            {makeGoldenComponent(index)}, QStringLiteral("Golden %1").arg(index), goldenPath, true));
        const auto runGoldenCli = [&cli, &goldenPath, index](const QStringList& arguments) {
            // process：用于观察导出状态的进程对象。
            QProcess process;
            process.start(cli, arguments);
            if (!process.waitForFinished(30000))
                return -1;
            if (process.exitCode() != 0)
                qWarning() << "LibrePCB CLI golden case failed" << index << process.readAllStandardError();
            return process.exitCode();
        };
        QCOMPARE(runGoldenCli(
                     {QStringLiteral("open-library"), QStringLiteral("--all"), QStringLiteral("--save"), goldenPath}),
                 0);
        QCOMPARE(runGoldenCli(
                     {QStringLiteral("open-library"), QStringLiteral("--all"), QStringLiteral("--check"), goldenPath}),
                 0);
        QCOMPARE(runGoldenCli({QStringLiteral("open-library"),
                               QStringLiteral("--all"),
                               QStringLiteral("--check"),
                               QStringLiteral("--strict"),
                               goldenPath}),
                 0);
    }
}

QTEST_MAIN(TestLibrePcbExporter)
#include "test_librepcb_exporter.moc"
