#include "core/easyeda/EasyedaFootprintImporter.h"
#include "core/ir/FootprintDataConverter.h"
#include "core/ir/Model3DDataConverter.h"
#include "core/ir/SymbolDataConverter.h"
#include "core/kicad/Exporter3DModel.h"
#include "models/ComponentData.h"
#include "models/FootprintData.h"
#include "models/FootprintDataSerializer.h"
#include "models/SymbolData.h"
#include "models/SymbolDataSerializer.h"

#include <QByteArray>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest>

#include <limits>

using namespace EasyKiConverter;

class TestModels : public QObject {
    Q_OBJECT

private slots:

    // 初始化模型测试套件。
    /**
     * @brief 验证 initTestCase 测试场景。
     */
    void initTestCase() {}

    // 清理模型测试套件。
    /**
     * @brief 清理测试用例使用的资源。
     */
    void cleanupTestCase() {}

    // 验证没有三维模型的封装不会在 IR 中生成空模型条目。
    /**
     * @brief 验证 testEmptyFootprintModelIsNotConverted 测试场景。
     */
    void testEmptyFootprintModelIsNotConverted() {
        // footprint：测试用封装数据。
        FootprintData footprint;
        // info：测试用元数据对象。
        FootprintInfo info;
        info.name = QStringLiteral("NO_MODEL");
        footprint.setInfo(info);

        // converted：转换后的测试数据。
        const IR::FootprintComponentIR converted = IR::toFootprintIR(footprint);

        QVERIFY(converted.models3d.isEmpty());
    }

    // 验证组件反序列化时不会保留旧的三维模型关联。
    /**
     * @brief 验证 testComponentFromJsonReplacesMissingModel 测试场景。
     */
    void testComponentFromJsonReplacesMissingModel() {
        // component：测试使用的缓存或元器件对象。
        ComponentData component;
        component.setLcscId(QStringLiteral("C12345"));
        // model：测试使用的配置值或状态。
        auto model = QSharedPointer<Model3DData>::create();
        model->setUuid(QStringLiteral("stale-model"));
        component.setModel3DData(model);
        component.setPreviewImages(QStringList{QStringLiteral("old-preview")});
        component.setDatasheetFormat(QStringLiteral("html"));
        component.setCinfoJsonRaw(QByteArrayLiteral("old-cinfo"));
        component.setCadJsonRaw(QByteArrayLiteral("old-cad"));
        component.setModel3DObjRaw(QByteArrayLiteral("old-obj"));

        // json：测试构造的 JSON 数据对象。
        QJsonObject json;
        json[QStringLiteral("lcsc_id")] = QStringLiteral("C54321");
        QVERIFY(component.fromJson(json));

        QCOMPARE(component.lcscId(), QStringLiteral("C54321"));
        QVERIFY(!component.model3DData());
        QVERIFY(component.previewImages().isEmpty());
        QCOMPARE(component.datasheetFormat(), QStringLiteral("pdf"));
        QVERIFY(component.cinfoJsonRaw().isEmpty());
        QVERIFY(component.cadJsonRaw().isEmpty());
        QVERIFY(component.model3DObjRaw().isEmpty());
    }

    // 验证清空组件数据时不会残留原始导出数据。
    /**
     * @brief 验证 testComponentClearRemovesRawData 测试场景。
     */
    void testComponentClearRemovesRawData() {
        // component：测试使用的缓存或元器件对象。
        ComponentData component;
        component.setDatasheetFormat(QStringLiteral("html"));
        component.setCinfoJsonRaw(QByteArrayLiteral("cinfo"));
        component.setCadJsonRaw(QByteArrayLiteral("cad"));
        component.setModel3DObjRaw(QByteArrayLiteral("obj"));

        component.clear();

        QCOMPARE(component.datasheetFormat(), QStringLiteral("pdf"));
        QVERIFY(component.cinfoJsonRaw().isEmpty());
        QVERIFY(component.cadJsonRaw().isEmpty());
        QVERIFY(component.model3DObjRaw().isEmpty());
    }

    // 验证符号数据的 JSON 往返序列化。
    /**
     * @brief 验证 testSymbolDataRoundTrip 测试场景。
     */
    void testSymbolDataRoundTrip() {
        // original：测试输入的原始数据。
        SymbolData original;

        // 设置基本信息
        SymbolInfo info;
        info.name = "Test Component";
        info.lcscId = "C12345";
        info.prefix = "U";
        original.setInfo(info);

        // 设置边界
        SymbolBBox bbox = {0, 0, 100, 100};
        original.setBbox(bbox);

        // 添加一个引脚
        SymbolPin pin;
        pin.settings.spicePinNumber = "1";
        pin.settings.posX = 10.0;
        pin.settings.posY = 20.0;
        pin.name.text = "VCC";
        original.addPin(pin);
        original.addGraphicOrder({QStringLiteral("P"), 0});

        // 序列化
        QJsonObject json = original.toJson();

        // 反序列化
        SymbolData restored;
        // ok：操作是否成功。
        bool ok = restored.fromJson(json);

        QVERIFY(ok);
        QCOMPARE(restored.info().lcscId, original.info().lcscId);
        QCOMPARE(restored.info().name, original.info().name);
        QCOMPARE(restored.pins().size(), original.pins().size());
        QCOMPARE(restored.pins().at(0).settings.spicePinNumber, original.pins().at(0).settings.spicePinNumber);
        QCOMPARE(restored.pins().at(0).name.text, original.pins().at(0).name.text);
        QCOMPARE(restored.graphicOrder().size(), 1);
        QCOMPARE(restored.graphicOrder().first().type, QStringLiteral("P"));
        QCOMPARE(restored.graphicOrder().first().index, 0);
    }

    // 验证符号校验会报告全部几何问题。
    /**
     * @brief 验证 testSymbolValidationReportsAllGeometryIssues 测试场景。
     */
    void testSymbolValidationReportsAllGeometryIssues() {
        // symbol：测试用符号数据。
        SymbolData symbol;
        // info：测试用元数据对象。
        SymbolInfo info;
        info.name = QStringLiteral("INVALID_SYMBOL");
        symbol.setInfo(info);
        symbol.setBbox(SymbolBBox{1.0, 1.0, 10.0, 10.0});

        // rectangle：测试用矩形图元。
        SymbolRectangle rectangle;
        rectangle.width = 0.0;
        rectangle.height = 2.0;
        rectangle.rx = -1.0;
        rectangle.ry = 0.0;
        symbol.addRectangle(rectangle);

        // path：测试使用的文件系统路径。
        SymbolPath path;
        symbol.addPath(path);
        // text：测试使用的文本或数据内容。
        SymbolText text;
        symbol.addText(text);
        // image：测试用图像数据。
        SymbolImage image;
        image.width = -1.0;
        image.height = 2.0;
        image.data = QByteArrayLiteral("image-data");
        symbol.addImage(image);

        // errors：测试期间收集的错误或诊断信息。
        const QStringList errors = symbol.validationErrors();
        QVERIFY(errors.size() >= 4);
        QVERIFY(errors.contains(QStringLiteral("Rectangle 0 has a non-positive size")));
        QVERIFY(errors.contains(QStringLiteral("Rectangle 0 has a negative or non-finite corner radius")));
        QVERIFY(errors.contains(QStringLiteral("Path 0 has no commands")));
        QVERIFY(errors.contains(QStringLiteral("Text 0 is empty")));
        QVERIFY(errors.contains(QStringLiteral("Image 0 has invalid bounds or rotation")));
        QCOMPARE(symbol.validate(), errors.first());
    }

    // 验证多部分符号中的几何问题也会被检查。
    /**
     * @brief 验证 testSymbolValidationChecksMultipartGeometry 测试场景。
     */
    void testSymbolValidationChecksMultipartGeometry() {
        // symbol：测试用符号数据。
        SymbolData symbol;
        // info：测试用元数据对象。
        SymbolInfo info;
        info.name = QStringLiteral("MULTIPART_SYMBOL");
        symbol.setInfo(info);
        symbol.setBbox(SymbolBBox{1.0, 1.0, 10.0, 10.0});

        // part：测试使用的元器件标识或名称。
        SymbolPart part;
        part.unitNumber = 1;
        // invalidPath：测试使用的文件系统路径。
        SymbolPath invalidPath;
        part.paths.append(invalidPath);
        // invalidImage：测试使用的元器件标识或名称。
        SymbolImage invalidImage;
        invalidImage.width = 1.0;
        invalidImage.height = -2.0;
        invalidImage.data = QByteArrayLiteral("image-data");
        part.images.append(invalidImage);
        symbol.addPart(part);

        // errors：测试期间收集的错误或诊断信息。
        const QStringList errors = symbol.validationErrors();
        QVERIFY(errors.contains(QStringLiteral("Part 0 Path 0 has no commands")));
        QVERIFY(errors.contains(QStringLiteral("Part 0 Image 0 has invalid bounds or rotation")));
    }

    // 验证符号引脚位置的有限性校验。
    /**
     * @brief 验证 testSymbolValidationChecksPinGeometry 测试场景。
     */
    void testSymbolValidationChecksPinGeometry() {
        // symbol：测试用符号数据。
        SymbolData symbol;
        // info：测试用元数据对象。
        SymbolInfo info;
        info.name = QStringLiteral("INVALID_PIN_SYMBOL");
        symbol.setInfo(info);
        symbol.setBbox(SymbolBBox{0.0, 0.0, 10.0, 10.0});

        // pin：测试用引脚数据。
        SymbolPin pin;
        pin.settings.spicePinNumber = QStringLiteral("1");
        pin.settings.posX = std::numeric_limits<double>::quiet_NaN();
        symbol.addPin(pin);

        // errors：测试期间收集的错误或诊断信息。
        const QStringList errors = symbol.validationErrors();
        QVERIFY(errors.contains(QStringLiteral("Pin 0 has a non-finite position")));
        QVERIFY(!symbol.isValid());
    }

    // 验证公共部分往返序列化以及 IR 映射关系。
    /**
     * @brief 验证 testCommonPartRoundTripAndIrMapping 测试场景。
     */
    void testCommonPartRoundTripAndIrMapping() {
        // symbol：测试用符号数据。
        SymbolData symbol;
        // info：测试用元数据对象。
        SymbolInfo info;
        info.name = QStringLiteral("COMMON_PART_SYMBOL");
        symbol.setInfo(info);
        symbol.setBbox(SymbolBBox{0.0, 0.0, 20.0, 20.0});

        // commonPart：测试使用的元器件标识或名称。
        SymbolPart commonPart;
        commonPart.unitNumber = 0;
        commonPart.commonToAllParts = true;
        // commonPin：测试用公共引脚。
        SymbolPin commonPin;
        commonPin.settings.spicePinNumber = QStringLiteral("VCC");
        commonPin.settings.posX = 0.0;
        commonPin.settings.posY = 0.0;
        commonPart.pins.append(commonPin);
        commonPart.graphicOrder.append({QStringLiteral("P"), 0});

        // visiblePart：测试使用的元器件标识或名称。
        SymbolPart visiblePart;
        visiblePart.unitNumber = 1;
        // visiblePin：测试用引脚数据。
        SymbolPin visiblePin;
        visiblePin.settings.spicePinNumber = QStringLiteral("IN");
        visiblePin.settings.posX = 10.0;
        visiblePin.settings.posY = 0.0;
        visiblePart.pins.append(visiblePin);
        // visibleText：测试使用的文本或数据内容。
        SymbolText visibleText;
        visibleText.text = QStringLiteral("LABEL");
        visibleText.anchor = QStringLiteral("end");
        visibleText.textSize = 8.0;
        visiblePart.texts.append(visibleText);
        // visibleRectangle：测试用矩形图元。
        SymbolRectangle visibleRectangle;
        visibleRectangle.posX = 0.0;
        visibleRectangle.posY = 0.0;
        visibleRectangle.rx = 0.0;
        visibleRectangle.ry = 0.0;
        visibleRectangle.width = 4.0;
        visibleRectangle.height = 2.0;
        visiblePart.rectangles.append(visibleRectangle);
        visiblePart.graphicOrder.append({QStringLiteral("P"), 0});
        visiblePart.graphicOrder.append({QStringLiteral("R"), 0});
        visiblePart.graphicOrder.append({QStringLiteral("T"), 0});

        symbol.addPart(commonPart);
        symbol.addPart(visiblePart);

        // restored：恢复后的测试数据。
        SymbolData restored;
        QVERIFY(restored.fromJson(symbol.toJson()));
        QCOMPARE(restored.parts().size(), 2);
        QVERIFY(restored.parts().first().commonToAllParts);
        QCOMPARE(restored.parts().at(1).rectangles.size(), 1);
        QCOMPARE(restored.parts().at(1).graphicOrder.size(), 3);
        QCOMPARE(restored.parts().at(1).graphicOrder.at(1).type, QStringLiteral("R"));
        QCOMPARE(restored.parts().at(1).graphicOrder.at(2).type, QStringLiteral("T"));

        // ir：测试用统一中间表示数据。
        const IR::SymbolComponentIR ir = IR::toSymbolIR(restored);
        QCOMPARE(ir.partCount, 1);
        QCOMPARE(ir.pins.size(), 2);
        QVERIFY(ir.pins.at(0).commonToAllParts);
        QCOMPARE(ir.pins.at(0).partIndex, -1);
        QVERIFY(!ir.pins.at(1).commonToAllParts);
        QCOMPARE(ir.pins.at(1).partIndex, 0);
        QCOMPARE(ir.texts.size(), 1);
        QCOMPARE(ir.texts.first().anchor, QStringLiteral("end"));
        QCOMPARE(ir.graphicOrder.at(0).partIndex, -1);
        QCOMPARE(ir.graphicOrder.at(1).partIndex, 0);
    }

    // 验证符号文本的位置、尺寸和锚点校验。
    /**
     * @brief 验证 testSymbolValidationChecksTextGeometry 测试场景。
     */
    void testSymbolValidationChecksTextGeometry() {
        // symbol：测试用符号数据。
        SymbolData symbol;
        // info：测试用元数据对象。
        SymbolInfo info;
        info.name = QStringLiteral("INVALID_TEXT_SYMBOL");
        symbol.setInfo(info);
        symbol.setBbox(SymbolBBox{0.0, 0.0, 10.0, 10.0});

        // text：测试使用的文本或数据内容。
        SymbolText text;
        text.posX = std::numeric_limits<double>::quiet_NaN();
        text.text = QStringLiteral("LABEL");
        text.visible = true;
        text.textSize = 0.0;
        text.anchor = QStringLiteral("baseline");
        symbol.addText(text);

        // errors：测试期间收集的错误或诊断信息。
        const QStringList errors = symbol.validationErrors();
        QVERIFY(errors.contains(QStringLiteral("Text 0 has a non-finite position or rotation")));
        QVERIFY(errors.contains(QStringLiteral("Text 0 has a non-positive font size")));
        QVERIFY(errors.contains(QStringLiteral("Text 0 has an unsupported anchor baseline")));
        QVERIFY(!symbol.isValid());
    }

    // 验证符号路径命令和参数的完整性校验。
    /**
     * @brief 验证 testSymbolValidationChecksPathCommands 测试场景。
     */
    void testSymbolValidationChecksPathCommands() {
        // symbol：测试用符号数据。
        SymbolData symbol;
        // info：测试用元数据对象。
        SymbolInfo info;
        info.name = QStringLiteral("INVALID_PATH_SYMBOL");
        symbol.setInfo(info);
        symbol.setBbox(SymbolBBox{0.0, 0.0, 10.0, 10.0});

        // exponentPath：测试使用的文件系统路径。
        SymbolPath exponentPath;
        exponentPath.paths = QStringLiteral("M 0 0 L 1e-3 2e-3");
        symbol.addPath(exponentPath);

        // unsupportedPath：测试使用的文件系统路径。
        SymbolPath unsupportedPath;
        unsupportedPath.paths = QStringLiteral("M 0 0 X 10 10");
        symbol.addPath(unsupportedPath);

        // moveOnlyPath：测试使用的文件系统路径。
        SymbolPath moveOnlyPath;
        moveOnlyPath.paths = QStringLiteral("M 2 2");
        symbol.addPath(moveOnlyPath);

        // noMovePath：测试使用的文件系统路径。
        SymbolPath noMovePath;
        noMovePath.paths = QStringLiteral("L 4 4");
        symbol.addPath(noMovePath);

        // incompletePath：测试使用的文件系统路径。
        SymbolPath incompletePath;
        incompletePath.paths = QStringLiteral("M 0 0 L 1");
        symbol.addPath(incompletePath);

        // invalidTokenPath：测试使用的文件系统路径。
        SymbolPath invalidTokenPath;
        invalidTokenPath.paths = QStringLiteral("M 0 0 L 1 1 @");
        symbol.addPath(invalidTokenPath);

        // repeatedMovePath：测试使用的文件系统路径。
        SymbolPath repeatedMovePath;
        repeatedMovePath.paths = QStringLiteral("M 0 0 1 1");
        symbol.addPath(repeatedMovePath);

        // nonFinitePath：测试使用的文件系统路径。
        SymbolPath nonFinitePath;
        nonFinitePath.paths = QStringLiteral("M 0 0 L 1e309 2");
        symbol.addPath(nonFinitePath);

        // errors：测试期间收集的错误或诊断信息。
        const QStringList errors = symbol.validationErrors();
        QVERIFY(!errors.contains(QStringLiteral("Path 0 has invalid command parameters")));
        QVERIFY(errors.contains(QStringLiteral("Path 1 contains an unsupported command")));
        QVERIFY(errors.contains(QStringLiteral("Path 2 has no drawable commands")));
        QVERIFY(errors.contains(QStringLiteral("Path 3 has no initial move command")));
        QVERIFY(errors.contains(QStringLiteral("Path 4 has invalid command parameters")));
        QVERIFY(errors.contains(QStringLiteral("Path 5 has invalid command parameters")));
        QVERIFY(!errors.contains(QStringLiteral("Path 6 has no drawable commands")));
        QVERIFY(!errors.contains(QStringLiteral("Path 6 has invalid command parameters")));
        QVERIFY(errors.contains(QStringLiteral("Path 7 contains a non-finite numeric parameter")));
    }

    // 验证符号图元绘制顺序引用的重复和越界检查。
    /**
     * @brief 验证 testSymbolValidationChecksGraphicOrderReferences 测试场景。
     */
    void testSymbolValidationChecksGraphicOrderReferences() {
        // symbol：测试用符号数据。
        SymbolData symbol;
        // info：测试用元数据对象。
        SymbolInfo info;
        info.name = QStringLiteral("INVALID_ORDER_SYMBOL");
        symbol.setInfo(info);
        symbol.setBbox(SymbolBBox{0.0, 0.0, 10.0, 10.0});

        // rectangle：测试用矩形图元。
        SymbolRectangle rectangle;
        rectangle.width = 1.0;
        rectangle.height = 1.0;
        symbol.addRectangle(rectangle);
        symbol.addGraphicOrder({QStringLiteral("R"), 0});
        symbol.addGraphicOrder({QStringLiteral("R"), 0});
        symbol.addGraphicOrder({QStringLiteral("PT"), 0});
        symbol.addGraphicOrder({QStringLiteral("UNKNOWN"), 0});

        // errors：测试期间收集的错误或诊断信息。
        const QStringList errors = symbol.validationErrors();
        QVERIFY(errors.contains(QStringLiteral("Graphic order 1 duplicates R index 0")));
        QVERIFY(errors.contains(QStringLiteral("Graphic order 2 has out-of-range PT index 0")));
        QVERIFY(errors.contains(QStringLiteral("Graphic order 3 has unknown type UNKNOWN")));
    }

    // 验证以原点为边界起点的符号数据可以通过校验。
    /**
     * @brief 验证 testSymbolValidationAcceptsOriginBasedBoundingBox 测试场景。
     */
    void testSymbolValidationAcceptsOriginBasedBoundingBox() {
        // symbol：测试用符号数据。
        SymbolData symbol;
        // info：测试用元数据对象。
        SymbolInfo info;
        info.name = QStringLiteral("ORIGIN_SYMBOL");
        symbol.setInfo(info);
        symbol.setBbox(SymbolBBox{0.0, 0.0, 10.0, 5.0});

        QVERIFY(symbol.validationErrors().isEmpty());
        QVERIFY(symbol.isValid());
    }

    // 验证封装数据的 JSON 往返序列化。
    /**
     * @brief 验证 testFootprintDataRoundTrip 测试场景。
     */
    void testFootprintDataRoundTrip() {
        // original：测试输入的原始数据。
        FootprintData original;

        // 设置基本信息
        FootprintInfo info;
        info.name = "SOT-23";
        info.uuid = "uuid-123";
        original.setInfo(info);

        // 添加一个焊盘
        FootprintPad pad;
        pad.number = "1";
        pad.centerX = 1.0;
        pad.centerY = 2.0;
        pad.width = 0.5;
        pad.height = 0.8;
        pad.shape = "RECT";
        original.addPad(pad);

        // 序列化
        QJsonObject json = original.toJson();

        // 反序列化
        FootprintData restored;
        // ok：操作是否成功。
        bool ok = restored.fromJson(json);

        QVERIFY(ok);
        QCOMPARE(restored.info().name, original.info().name);
        QCOMPARE(restored.pads().size(), original.pads().size());
        QCOMPARE(restored.pads().at(0).number, original.pads().at(0).number);
        QCOMPARE(restored.pads().at(0).shape, original.pads().at(0).shape);
    }

    // 验证清空封装数据时不会残留旧的三维模型关联。
    /**
     * @brief 验证 testFootprintClearRemovesModel3D 测试场景。
     */
    void testFootprintClearRemovesModel3D() {
        // footprint：测试用封装数据。
        FootprintData footprint;
        // model：测试使用的配置值或状态。
        Model3DData model;
        model.setUuid(QStringLiteral("stale-model"));
        model.setName(QStringLiteral("STALE_MODEL"));
        model.setStep(QByteArrayLiteral("STEP_DATA"));
        footprint.setModel3D(model);

        footprint.clear();

        QVERIFY(footprint.model3D().uuid().isEmpty());
        QVERIFY(footprint.model3D().name().isEmpty());
        QVERIFY(footprint.model3D().step().isEmpty());
    }

    // 验证绝对三维模型原点的导入坐标。
    /**
     * @brief 验证 testModel3DAbsoluteOriginImport 测试场景。
     */
    void testModel3DAbsoluteOriginImport() {
        // importer：负责读取测试输入的导入器实例。
        EasyedaFootprintImporter importer;
        // cadData：测试使用的文本或数据内容。
        const QJsonObject cadData = makeCadDataWithModelOrigin(QStringLiteral("3998.803,2982.7698"));

        // footprint：测试用封装数据。
        const QSharedPointer<FootprintData> footprint = importer.importFootprintData(cadData);

        QVERIFY(footprint);
        QCOMPARE(footprint->model3D().translation().x, 3998.803);
        QCOMPARE(footprint->model3D().translation().y, 2982.7698);
    }

    /**
     * @brief 验证乱序预览图回调会按索引扩容而不是追加到错误位置。
     */
    void testPreviewImageDataExpandsForIndexedCallback() {
        // component：测试使用的缓存或元器件对象。
        ComponentData component;
        // imageData：测试使用的文本或数据内容。
        const QByteArray imageData = QByteArrayLiteral("preview-image");

        component.addPreviewImageData(imageData, 2);

        QCOMPARE(component.previewImageData().size(), 3);
        QVERIFY(component.previewImageData().at(0).isEmpty());
        QVERIFY(component.previewImageData().at(1).isEmpty());
        QCOMPARE(component.previewImageData().at(2), imageData);
    }

    // 验证相对三维模型原点的导入坐标。
    /**
     * @brief 验证 testModel3DRelativeOriginImport 测试场景。
     */
    void testModel3DRelativeOriginImport() {
        // importer：负责读取测试输入的导入器实例。
        EasyedaFootprintImporter importer;
        // cadData：测试使用的文本或数据内容。
        const QJsonObject cadData = makeCadDataWithModelOrigin(QStringLiteral("1.5,-2"));

        // footprint：测试用封装数据。
        const QSharedPointer<FootprintData> footprint = importer.importFootprintData(cadData);

        QVERIFY(footprint);
        QCOMPARE(footprint->model3D().translation().x, 4000.3);
        QCOMPARE(footprint->model3D().translation().y, 2980.35);
    }

    // 验证无关原点会回退到封装中心坐标。
    /**
     * @brief 验证 testModel3DUnrelatedOriginFallsBackToFootprintCenter 测试场景。
     */
    void testModel3DUnrelatedOriginFallsBackToFootprintCenter() {
        // importer：负责读取测试输入的导入器实例。
        EasyedaFootprintImporter importer;
        // cadData：测试使用的文本或数据内容。
        const QJsonObject cadData = makeCadDataWithModelOrigin(QStringLiteral("400,300"));

        // footprint：测试用封装数据。
        const QSharedPointer<FootprintData> footprint = importer.importFootprintData(cadData);

        QVERIFY(footprint);
        QCOMPARE(footprint->model3D().translation().x, 3998.8);
        QCOMPARE(footprint->model3D().translation().y, 2982.35);
    }

    // 验证三维模型解析失败时不会部分覆盖已有状态。
    /**
     * @brief 验证 testModel3DRejectsMalformedJsonAtomically 测试场景。
     */
    void testModel3DRejectsMalformedJsonAtomically() {
        // model：测试使用的配置值或状态。
        Model3DData model;
        model.setName(QStringLiteral("cached-model"));
        model.setUuid(QStringLiteral("cached-uuid"));
        model.setTranslation(Model3DBase(1.0, 2.0, 3.0));

        // malformed：构造的非法测试数据。
        QJsonObject malformed;
        malformed.insert(QStringLiteral("name"), QStringLiteral("new-model"));
        malformed.insert(QStringLiteral("uuid"), QStringLiteral("new-uuid"));
        // translation：测试用坐标变换。
        QJsonObject translation;
        translation.insert(QStringLiteral("x"), QStringLiteral("not-a-number"));
        translation.insert(QStringLiteral("y"), 20.0);
        translation.insert(QStringLiteral("z"), 30.0);
        malformed.insert(QStringLiteral("translation"), translation);

        QVERIFY(!model.fromJson(malformed));
        QCOMPARE(model.name(), QStringLiteral("cached-model"));
        QCOMPARE(model.uuid(), QStringLiteral("cached-uuid"));
        QCOMPARE(model.translation().x, 1.0);
        QCOMPARE(model.translation().y, 2.0);
        QCOMPARE(model.translation().z, 3.0);
    }

    // 验证 STEP 导出保留装配结构中的关键记录。
    /**
     * @brief 验证 testStepExportPreservesStepAssemblyStructure 测试场景。
     */
    void testStepExportPreservesStepAssemblyStructure() {
        // modelData：测试使用的文本或数据内容。
        Model3DData modelData;
        modelData.setStep(
            QByteArray("ISO-10303-21;\n"
                       "DATA;\n"
                       "#1=CARTESIAN_POINT('',(10.,20.,5.));\n"
                       "#2=CARTESIAN_POINT('',(14.,24.,7.));\n"
                       "#3=CARTESIAN_POINT('',(0.,0.,0.));\n"
                       "#4=VERTEX_POINT('',#1);\n"
                       "#5=VERTEX_POINT('',#2);\n"
                       "#6=DIRECTION('',(0.,0.,1.));\n"
                       "ENDSEC;\n"
                       "END-ISO-10303-21;\n"));

        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        // stepPath：测试使用的文件系统路径。
        const QString stepPath = tempDir.filePath(QStringLiteral("model.step"));

        // exporter：负责生成测试输出的导出器实例。
        Exporter3DModel exporter;
        // irModel：测试使用的配置值或状态。
        auto irModel = IR::toModel3DIR(modelData);
        QVERIFY(exporter.exportToStep(irModel, stepPath));

        QFile stepFile(stepPath);
        QVERIFY(stepFile.open(QIODevice::ReadOnly));
        // content：测试使用的文本或数据内容。
        const QString content = QString::fromLatin1(stepFile.readAll());

        QVERIFY(content.contains(QStringLiteral("#1=CARTESIAN_POINT('',(10.,20.,5.));")));
        QVERIFY(content.contains(QStringLiteral("#2=CARTESIAN_POINT('',(14.,24.,7.));")));
        QVERIFY(content.contains(QStringLiteral("#3=CARTESIAN_POINT('',(0.,0.,0.));")));
        QVERIFY(content.contains(QStringLiteral("#6=DIRECTION('',(0.,0.,1.));")));
    }

    // 验证 OBJ 最低 Z 与 WRL 归一化坐标计算保持一致。
    /**
     * @brief 验证 testObjMinZMatchesExportedWrlNormalization 测试场景。
     */
    void testObjMinZMatchesExportedWrlNormalization() {
        const QByteArray positiveObj(
            "v 0 0 2.54\n"
            "v 1 1 5.08\n"
            "f 1 2 1\n");
        QCOMPARE(Exporter3DModel::calculateObjMinZ(positiveObj), 1.0);

        const QByteArray negativeObj(
            "v 0 0 -2.54\n"
            "v 1 1 2.54\n"
            "f 1 2 1\n");
        QCOMPARE(Exporter3DModel::calculateObjMinZ(negativeObj), -1.0);

        QCOMPARE(Exporter3DModel::calculateObjMinZ(QByteArray("f 1 2 3\n")), std::numeric_limits<double>::max());

        /**
         * @brief 验证 wrlData 测试场景。
         */
        const QByteArray wrlData(
            "Shape {\n"
            "  geometry IndexedFaceSet {\n"
            "    coord Coordinate {\n"
            "      point [\n"
            "        0 0 -0.5,\n"
            "        0 0 1.0,\n"
            "      ]\n"
            "    }\n"
            "  }\n"
            "}\n");
        QCOMPARE(Exporter3DModel::calculateWrlDisplayMinZ(wrlData), -1.27);
    }

    // 验证符号基础几何图元的序列化和恢复。
    /**
     * @brief 验证 testSymbolGeometrySerialization 测试场景。
     */
    void testSymbolGeometrySerialization() {
        // original：测试输入的原始数据。
        SymbolData original;

        // 添加矩形
        SymbolRectangle rect;
        rect.posX = 10;
        rect.posY = 10;
        rect.width = 50;
        rect.height = 30;
        rect.strokeWidth = 1.0;
        original.addRectangle(rect);

        // 添加圆
        SymbolCircle circle;
        circle.centerX = 100;
        circle.centerY = 100;
        circle.radius = 20;
        original.addCircle(circle);

        // json：测试构造的 JSON 数据对象。
        QJsonObject json = original.toJson();

        // restored：恢复后的测试数据。
        SymbolData restored;
        restored.fromJson(json);

        QCOMPARE(restored.rectangles().size(), 1);
        QCOMPARE(restored.rectangles().at(0).width, 50.0);
        QCOMPARE(restored.circles().size(), 1);
        QCOMPARE(restored.circles().at(0).radius, 20.0);
    }

private:
    // 构造带指定三维模型原点的 CAD 测试数据。
    /**
     * @brief 验证 makeCadDataWithModelOrigin 测试场景。
     * @param origin 测试坐标原点。
     */
    QJsonObject makeCadDataWithModelOrigin(const QString& origin) const {
        // cadData：测试使用的文本或数据内容。
        QJsonObject cadData;
        cadData.insert(QStringLiteral("SMT"), true);

        // cPara：测试用 JSON 对象。
        QJsonObject cPara;
        cPara.insert(QStringLiteral("package"), QStringLiteral("TEST_FOOTPRINT"));
        cPara.insert(QStringLiteral("3DModel"), QStringLiteral("TEST_MODEL"));

        // head：测试用 JSON 对象。
        QJsonObject head;
        head.insert(QStringLiteral("c_para"), cPara);

        // bbox：测试用几何边界框。
        QJsonObject bbox;
        bbox.insert(QStringLiteral("x"), 3977.3);
        bbox.insert(QStringLiteral("y"), 2971.0);
        bbox.insert(QStringLiteral("width"), 43.0);
        bbox.insert(QStringLiteral("height"), 22.7);

        // attrs：测试用 JSON 对象。
        QJsonObject attrs;
        attrs.insert(QStringLiteral("c_etype"), QStringLiteral("outline3D"));
        attrs.insert(QStringLiteral("uuid"), QStringLiteral("model-uuid"));
        attrs.insert(QStringLiteral("title"), QStringLiteral("TEST_MODEL"));
        attrs.insert(QStringLiteral("c_origin"), origin);
        attrs.insert(QStringLiteral("c_rotation"), QStringLiteral("0,0,0"));
        attrs.insert(QStringLiteral("z"), QStringLiteral("0"));

        // svgNode：测试用 JSON 对象。
        QJsonObject svgNode;
        svgNode.insert(QStringLiteral("attrs"), attrs);

        // shapes：测试用图形集合。
        QJsonArray shapes;
        shapes.append(QStringLiteral("SVGNODE~") +
                      QString::fromUtf8(QJsonDocument(svgNode).toJson(QJsonDocument::Compact)));

        // dataStr：测试使用的文本或数据内容。
        QJsonObject dataStr;
        dataStr.insert(QStringLiteral("head"), head);
        dataStr.insert(QStringLiteral("BBox"), bbox);
        dataStr.insert(QStringLiteral("shape"), shapes);

        // packageDetail：测试用 JSON 对象。
        QJsonObject packageDetail;
        packageDetail.insert(QStringLiteral("dataStr"), dataStr);
        cadData.insert(QStringLiteral("packageDetail"), packageDetail);

        return cadData;
    }
};

QTEST_GUILESS_MAIN(TestModels)
#include "test_models.moc"
