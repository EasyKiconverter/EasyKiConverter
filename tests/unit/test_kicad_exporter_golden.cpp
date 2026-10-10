#include "core/ir/FootprintDataConverter.h"
#include "core/ir/Model3DDataConverter.h"
#include "core/ir/SymbolDataConverter.h"
#include "core/kicad/ExporterFootprint.h"
#include "core/kicad/ExporterSymbol.h"
#include "models/FootprintData.h"
#include "models/Model3DData.h"
#include "models/SymbolData.h"
#include "tests/common/TestPaths.hpp"

#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace EasyKiConverter;
using namespace EasyKiConverter::Test;

class TestKiCadExporterGolden : public QObject {
    Q_OBJECT

private slots:

    // 验证基本符号库导出结果与黄金文件一致。
    /**
     * @brief 验证 testSymbolLibraryMatchesGolden 测试场景。
     */
    void testSymbolLibraryMatchesGolden() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        // outputPath：测试使用的文件系统路径。
        const QString outputPath = tempDir.filePath(QStringLiteral("GoldenLib.kicad_sym"));

        // exporter：负责生成测试输出的导出器实例。
        ExporterSymbol exporter;
        // irSymbol：测试用符号数据。
        auto irSymbol = IR::toSymbolIR(makeSymbol());
        QVERIFY(exporter.exportSymbolLibrary({irSymbol}, QStringLiteral("GoldenLib"), outputPath, false, false));

        // error：测试期间收集的错误或诊断信息。
        QString error;
        // actual：测试得到的实际结果数据。
        const QString actual = TestPaths::readText(outputPath, &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QVERIFY2(
            TestPaths::compareTextToGolden(actual, QStringLiteral("kicad/golden_symbol_library.kicad_sym"), &error),
            qPrintable(error));
    }

    // 验证基本封装导出结果与黄金文件一致。
    /**
     * @brief 验证 testFootprintMatchesGolden 测试场景。
     */
    void testFootprintMatchesGolden() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        // outputPath：测试使用的文件系统路径。
        const QString outputPath = tempDir.filePath(QStringLiteral("GOLDEN_FOOTPRINT.kicad_mod"));

        // exporter：负责生成测试输出的导出器实例。
        ExporterFootprint exporter;
        // irFootprint：测试用封装数据。
        auto irFootprint = IR::toFootprintIR(makeFootprint());
        QVERIFY(exporter.exportFootprint(irFootprint, outputPath));

        // error：测试期间收集的错误或诊断信息。
        QString error;
        // actual：测试得到的实际结果数据。
        const QString actual = TestPaths::readText(outputPath, &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QVERIFY2(TestPaths::compareTextToGolden(actual, QStringLiteral("kicad/golden_footprint.kicad_mod"), &error),
                 qPrintable(error));
    }

    // 验证带三维模型引用的封装导出结果。
    /**
     * @brief 验证 testFootprintWith3DMatchesGolden 测试场景。
     */
    void testFootprintWith3DMatchesGolden() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        // outputPath：测试使用的文件系统路径。
        const QString outputPath = tempDir.filePath(QStringLiteral("FOOTPRINT_WITH_3D.kicad_mod"));

        // exporter：负责生成测试输出的导出器实例。
        ExporterFootprint exporter;
        // irFootprint：测试用封装数据。
        auto irFootprint = IR::toFootprintIR(makeFootprintWith3D());
        QVERIFY(exporter.exportFootprint(irFootprint,
                                         outputPath,
                                         QStringLiteral("../GoldenLib.3dmodels/FOOTPRINT_WITH_3D.wrl"),
                                         QStringLiteral("../GoldenLib.3dmodels/FOOTPRINT_WITH_3D.step")));

        // error：测试期间收集的错误或诊断信息。
        QString error;
        // actual：测试得到的实际结果数据。
        const QString actual = TestPaths::readText(outputPath, &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QVERIFY2(
            TestPaths::compareTextToGolden(actual, QStringLiteral("kicad/golden_footprint_with_3d.kicad_mod"), &error),
            qPrintable(error));
    }

    // 验证复杂焊盘、文本和区域的封装导出结果。
    /**
     * @brief 验证 testComplexFootprintMatchesGolden 测试场景。
     */
    void testComplexFootprintMatchesGolden() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        // outputPath：测试使用的文件系统路径。
        const QString outputPath = tempDir.filePath(QStringLiteral("COMPLEX_FOOTPRINT.kicad_mod"));

        // exporter：负责生成测试输出的导出器实例。
        ExporterFootprint exporter;
        // irComplexFootprint：测试用封装数据。
        auto irComplexFootprint = IR::toFootprintIR(makeComplexFootprint());
        QVERIFY(exporter.exportFootprint(irComplexFootprint, outputPath));

        // error：测试期间收集的错误或诊断信息。
        QString error;
        // actual：测试得到的实际结果数据。
        const QString actual = TestPaths::readText(outputPath, &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QVERIFY2(
            TestPaths::compareTextToGolden(actual, QStringLiteral("kicad/golden_complex_footprint.kicad_mod"), &error),
            qPrintable(error));
    }

    // 验证多个符号合并到同一库文件时的输出结果。
    /**
     * @brief 验证 testMultiComponentLibraryMatchesGolden 测试场景。
     */
    void testMultiComponentLibraryMatchesGolden() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        // outputPath：测试使用的文件系统路径。
        const QString outputPath = tempDir.filePath(QStringLiteral("MultiLib.kicad_sym"));

        // exporter：负责生成测试输出的导出器实例。
        ExporterSymbol exporter;
        // irCap：测试用电容器件 IR 数据。
        auto irCap = IR::toSymbolIR(makeCapacitorSymbol());
        // irInd：测试用电感器件 IR 数据。
        auto irInd = IR::toSymbolIR(makeInductorSymbol());
        QVERIFY(exporter.exportSymbolLibrary({irCap, irInd}, QStringLiteral("MultiLib"), outputPath, false, false));

        // error：测试期间收集的错误或诊断信息。
        QString error;
        // actual：测试得到的实际结果数据。
        const QString actual = TestPaths::readText(outputPath, &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QVERIFY2(TestPaths::compareTextToGolden(
                     actual, QStringLiteral("kicad/golden_multi_component_lib.kicad_sym"), &error),
                 qPrintable(error));
    }

    // 验证单部分符号使用来源中心并保留引脚名称元数据。
    /**
     * @brief 验证 testSinglePartSymbolUsesHeadCenterAndPinNameMetadata 测试场景。
     */
    void testSinglePartSymbolUsesHeadCenterAndPinNameMetadata() {
        // symbol：测试用符号数据。
        SymbolData symbol;
        // info：测试用元数据对象。
        SymbolInfo info;
        info.name = QStringLiteral("MT2492");
        symbol.setInfo(info);

        // bbox：测试用几何边界框。
        SymbolBBox bbox;
        bbox.x = 349.0;
        bbox.y = 279.6;
        bbox.width = 102.0;
        bbox.height = 40.4;
        bbox.headX = 400.0;
        bbox.headY = 300.0;
        bbox.hasHeadCenter = true;
        symbol.setBbox(bbox);

        // pin：测试用引脚数据。
        SymbolPin pin;
        pin.settings.posX = 350.0;
        pin.settings.posY = 290.0;
        pin.settings.rotation = 0;
        pin.settings.spicePinNumber = QStringLiteral("1");
        pin.name.posX = 372.0;
        pin.name.posY = 290.0;
        pin.name.isDisplayed = true;
        pin.name.text = QStringLiteral("IN");
        pin.name.textAnchor = QStringLiteral("start");
        symbol.setPins({pin});

        // ir：测试用统一中间表示数据。
        const IR::SymbolComponentIR ir = IR::toSymbolIR(symbol);
        QCOMPARE(ir.pins.size(), 1);
        QVERIFY(qAbs(ir.pins.first().position.x() + 12.7) < 1e-6);
        QVERIFY(qAbs(ir.pins.first().position.y() - 2.54) < 1e-6);
        QVERIFY(qAbs(ir.pins.first().namePosition.x() + 7.112) < 1e-6);
        QVERIFY(qAbs(ir.pins.first().namePosition.y() - 2.54) < 1e-6);
        QCOMPARE(ir.pins.first().nameAnchor, QStringLiteral("start"));
    }

    // 验证 C146335 的实际引脚坐标在最终 KiCad 文件中保持栅格对齐。
    /**
     * @brief 验证 testC146335PreservesLogicalOriginForGridAlignedPins 测试场景。
     */
    void testC146335PreservesLogicalOriginForGridAlignedPins() {
        // symbol：测试用符号数据。
        SymbolData symbol;
        // info：测试用元数据对象。
        SymbolInfo info;
        info.name = QStringLiteral("CUS10S30,H3F");
        info.prefix = QStringLiteral("D");
        symbol.setInfo(info);

        // C146335 的实际 EasyEDA CAD 数据：head=(400,300)，BBox=(379,293,42,15)。
        SymbolBBox bbox;
        bbox.x = 379.0;
        bbox.y = 293.0;
        bbox.width = 42.0;
        bbox.height = 15.0;
        bbox.headX = 400.0;
        bbox.headY = 300.0;
        bbox.hasHeadCenter = true;
        symbol.setBbox(bbox);

        // pinRight：测试用引脚数据。
        SymbolPin pinRight;
        pinRight.settings.posX = 420.0;
        pinRight.settings.posY = 300.0;
        pinRight.settings.rotation = 0;
        pinRight.settings.spicePinNumber = QStringLiteral("2");
        pinRight.pinPath.path = QStringLiteral("M 420 300 h -15");
        symbol.addPin(pinRight);

        // pinLeft：测试用引脚数据。
        SymbolPin pinLeft;
        pinLeft.settings.posX = 380.0;
        pinLeft.settings.posY = 300.0;
        pinLeft.settings.rotation = 180;
        pinLeft.settings.spicePinNumber = QStringLiteral("1");
        pinLeft.pinPath.path = QStringLiteral("M 380 300 h 15");
        symbol.addPin(pinLeft);

        // body：测试用主体数据。
        SymbolPolyline body;
        body.points = QStringLiteral("392 294 392 293 394 293 394 308 396 308 396 307");
        symbol.addPolyline(body);

        // triangle：测试用三角形图元。
        SymbolPath triangle;
        triangle.paths = QStringLiteral("M 405 294 L 395 300 L 405 307 Z");
        symbol.addPath(triangle);

        // ir：测试用统一中间表示数据。
        const IR::SymbolComponentIR ir = IR::toSymbolIR(symbol);
        QVERIFY(ir.preserveLogicalOrigin);
        QCOMPARE(ir.pins.size(), 2);
        QVERIFY(qAbs(ir.pins[0].position.y()) < 1e-9);
        QVERIFY(qAbs(ir.pins[1].position.y()) < 1e-9);

        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        // outputPath：测试使用的文件系统路径。
        const QString outputPath = tempDir.filePath(QStringLiteral("C146335.kicad_sym"));
        // exporter：负责生成测试输出的导出器实例。
        ExporterSymbol exporter;
        QVERIFY(exporter.exportSymbol(ir, outputPath));

        // error：测试期间收集的错误或诊断信息。
        QString error;
        // actual：测试得到的实际结果数据。
        const QString actual = TestPaths::readText(outputPath, &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QVERIFY(actual.contains(QStringLiteral("      (at 5.08 0.00 180)")));
        QVERIFY(actual.contains(QStringLiteral("      (at -5.08 0.00 0)")));
    }

    // 验证多部件符号各单元的图形和引脚导出结果。
    /**
     * @brief 验证 testMultiPartSymbolMatchesGolden 测试场景。
     */
    void testMultiPartSymbolMatchesGolden() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        // outputPath：测试使用的文件系统路径。
        const QString outputPath = tempDir.filePath(QStringLiteral("MultiPartLib.kicad_sym"));

        // exporter：负责生成测试输出的导出器实例。
        ExporterSymbol exporter;
        // irMultiPart：测试使用的元器件标识或名称。
        auto irMultiPart = IR::toSymbolIR(makeMultiPartSymbol());
        QVERIFY(exporter.exportSymbolLibrary({irMultiPart}, QStringLiteral("MultiPartLib"), outputPath, false, false));

        // error：测试期间收集的错误或诊断信息。
        QString error;
        // actual：测试得到的实际结果数据。
        const QString actual = TestPaths::readText(outputPath, &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QVERIFY2(
            TestPaths::compareTextToGolden(actual, QStringLiteral("kicad/golden_multi_part_symbol.kicad_sym"), &error),
            qPrintable(error));
    }

    // 验证特殊字符属性在符号库中的转义和布局结果。
    /**
     * @brief 验证 testSymbolWithSpecialCharsMatchesGolden 测试场景。
     */
    void testSymbolWithSpecialCharsMatchesGolden() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        // outputPath：测试使用的文件系统路径。
        const QString outputPath = tempDir.filePath(QStringLiteral("SpecialLib.kicad_sym"));

        // exporter：负责生成测试输出的导出器实例。
        ExporterSymbol exporter;
        // irResistor：测试用电阻器件 IR 数据。
        auto irResistor = IR::toSymbolIR(makeResistorSymbol());
        QVERIFY(exporter.exportSymbolLibrary({irResistor}, QStringLiteral("SpecialLib"), outputPath, false, false));

        // error：测试期间收集的错误或诊断信息。
        QString error;
        // actual：测试得到的实际结果数据。
        const QString actual = TestPaths::readText(outputPath, &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QVERIFY2(TestPaths::compareTextToGolden(
                     actual, QStringLiteral("kicad/golden_symbol_with_special_chars.kicad_sym"), &error),
                 qPrintable(error));
    }

private:
    // 构造基本符号黄金测试数据。
    /**
     * @brief 验证 makeSymbol 测试场景。
     */
    SymbolData makeSymbol() const {
        // symbol：测试用符号数据。
        SymbolData symbol;

        // info：测试用元数据对象。
        SymbolInfo info;
        info.name = QStringLiteral("GOLDEN_SYMBOL");
        info.prefix = QStringLiteral("U");
        info.package = QStringLiteral("GOLDEN_FOOTPRINT");
        info.lcscId = QStringLiteral("C12345");
        symbol.setInfo(info);
        symbol.setBbox({0, 0, 0, 0});

        return symbol;
    }

    // 构造基本封装黄金测试数据。
    /**
     * @brief 验证 makeFootprint 测试场景。
     */
    FootprintData makeFootprint() const {
        // footprint：测试用封装数据。
        FootprintData footprint;

        // info：测试用元数据对象。
        FootprintInfo info;
        info.name = QStringLiteral("GOLDEN_FOOTPRINT");
        footprint.setInfo(info);
        footprint.setBbox({0, 0, 0, 0});

        FootprintPad pad{};
        pad.shape = QStringLiteral("rect");
        pad.centerX = 0;
        pad.centerY = 0;
        pad.width = 20;
        pad.height = 10;
        pad.layerId = 1;
        pad.number = QStringLiteral("1");
        pad.rotation = 0;
        footprint.addPad(pad);

        return footprint;
    }

    // 构造覆盖复杂图元的封装测试数据。
    /**
     * @brief 验证 makeComplexFootprint 测试场景。
     */
    FootprintData makeComplexFootprint() const {
        // footprint：测试用封装数据。
        FootprintData footprint;

        // info：测试用元数据对象。
        FootprintInfo info;
        info.name = QStringLiteral("COMPLEX_FOOTPRINT");
        info.description = QStringLiteral("Through-hole and custom pad coverage");
        footprint.setInfo(info);
        footprint.setBbox({-20, -15, 40, 30});

        FootprintPad thruHolePad{};
        thruHolePad.shape = QStringLiteral("circle");
        thruHolePad.centerX = -10;
        thruHolePad.centerY = 0;
        thruHolePad.width = 8;
        thruHolePad.height = 8;
        thruHolePad.layerId = 11;
        thruHolePad.number = QStringLiteral("1");
        thruHolePad.holeRadius = 2;
        thruHolePad.rotation = 90;
        footprint.addPad(thruHolePad);

        FootprintPad customPad{};
        customPad.shape = QStringLiteral("polygon");
        customPad.centerX = 10;
        customPad.centerY = 0;
        customPad.width = 4;
        customPad.height = 4;
        customPad.layerId = 1;
        customPad.number = QStringLiteral("2");
        customPad.points = QStringLiteral("8 -2 12 -2 12 2 8 2");
        footprint.addPad(customPad);

        FootprintText text{};
        text.type = QStringLiteral("L");
        text.centerX = 0;
        text.centerY = -10;
        text.strokeWidth = 0.6;
        text.layerId = 3;
        text.fontSize = 5;
        text.text = QStringLiteral("PIN1");
        text.isDisplayed = true;
        footprint.addText(text);

        FootprintSolidRegion fabRegion{};
        fabRegion.path = QStringLiteral("M -15 -5 L 15 -5 L 15 5 L -15 5 Z");
        fabRegion.layerId = 100;
        fabRegion.fillStyle = QStringLiteral("solid");
        footprint.addSolidRegion(fabRegion);

        FootprintSolidRegion courtyard{};
        courtyard.path = QStringLiteral("M -20 -15 L 20 -15 L 20 15 L -20 15 Z");
        courtyard.layerId = 99;
        courtyard.fillStyle = QStringLiteral("solid");
        footprint.addSolidRegion(courtyard);

        return footprint;
    }

    // 构造带三维模型信息的封装测试数据。
    /**
     * @brief 验证 makeFootprintWith3D 测试场景。
     */
    FootprintData makeFootprintWith3D() const {
        // footprint：测试用封装数据。
        FootprintData footprint;

        // info：测试用元数据对象。
        FootprintInfo info;
        info.name = QStringLiteral("FOOTPRINT_WITH_3D");
        info.description = QStringLiteral("Footprint with 3D model references");
        info.type = QStringLiteral("smd");
        footprint.setInfo(info);
        footprint.setBbox({0, 0, 0, 0});

        FootprintPad pad1{};
        pad1.shape = QStringLiteral("rect");
        pad1.centerX = -6;
        pad1.centerY = 0;
        pad1.width = 5;
        pad1.height = 7;
        pad1.layerId = 1;
        pad1.number = QStringLiteral("1");
        pad1.rotation = 0;
        footprint.addPad(pad1);

        FootprintPad pad2{};
        pad2.shape = QStringLiteral("rect");
        pad2.centerX = 6;
        pad2.centerY = 0;
        pad2.width = 5;
        pad2.height = 7;
        pad2.layerId = 1;
        pad2.number = QStringLiteral("2");
        pad2.rotation = 0;
        footprint.addPad(pad2);

        // model3D：测试使用的配置值或状态。
        Model3DData model3D;
        model3D.setName(QStringLiteral("FOOTPRINT_WITH_3D"));
        model3D.setTranslation({0.0, -0.1, 0.0});
        footprint.setModel3D(model3D);

        return footprint;
    }

    // 构造电容符号黄金测试数据。
    /**
     * @brief 验证 makeCapacitorSymbol 测试场景。
     */
    SymbolData makeCapacitorSymbol() const {
        // symbol：测试用符号数据。
        SymbolData symbol;

        // info：测试用元数据对象。
        SymbolInfo info;
        info.name = QStringLiteral("CAPACITOR_0402");
        info.prefix = QStringLiteral("C");
        info.package = QStringLiteral("0402_C");
        info.lcscId = QStringLiteral("C23186");
        symbol.setInfo(info);
        symbol.setBbox({-6, -6, 12, 12});

        // pl1：测试用第一层数据。
        SymbolPolyline pl1;
        pl1.points = QStringLiteral("-6 -6 6 -6");
        pl1.strokeWidth = 0;
        pl1.fillColor = false;
        pl1.isLocked = true;
        symbol.addPolyline(pl1);

        // pl2：测试用第二层数据。
        SymbolPolyline pl2;
        pl2.points = QStringLiteral("-6 6 6 6");
        pl2.strokeWidth = 0;
        pl2.fillColor = false;
        pl2.isLocked = true;
        symbol.addPolyline(pl2);

        SymbolPin pin1{};
        pin1.settings.posX = 0;
        pin1.settings.posY = 20;
        pin1.settings.rotation = 90;
        pin1.settings.spicePinNumber = QStringLiteral("1");
        pin1.settings.type = PinType::Unspecified;
        pin1.pinPath.path = QStringLiteral("M 0 20 h -14");
        pin1.name.text = QStringLiteral("~");
        symbol.addPin(pin1);

        SymbolPin pin2{};
        pin2.settings.posX = 0;
        pin2.settings.posY = -20;
        pin2.settings.rotation = 270;
        pin2.settings.spicePinNumber = QStringLiteral("2");
        pin2.settings.type = PinType::Unspecified;
        pin2.pinPath.path = QStringLiteral("M 0 -20 h 14");
        pin2.name.text = QStringLiteral("~");
        symbol.addPin(pin2);

        return symbol;
    }

    // 构造电感符号黄金测试数据。
    /**
     * @brief 验证 makeInductorSymbol 测试场景。
     */
    SymbolData makeInductorSymbol() const {
        // symbol：测试用符号数据。
        SymbolData symbol;

        // info：测试用元数据对象。
        SymbolInfo info;
        info.name = QStringLiteral("INDUCTOR_0603");
        info.prefix = QStringLiteral("L");
        info.package = QStringLiteral("0603_L");
        info.lcscId = QStringLiteral("C34567");
        symbol.setInfo(info);
        symbol.setBbox({0, -5, 20, 10});

        // arc1：测试用第一条圆弧。
        SymbolArc arc1;
        arc1.path.append(QPointF(0, 0));
        arc1.path.append(QPointF(5, -5));
        arc1.path.append(QPointF(10, 0));
        arc1.strokeWidth = 0;
        arc1.fillColor = false;
        arc1.isLocked = true;
        symbol.addArc(arc1);

        // arc2：测试用第二条圆弧。
        SymbolArc arc2;
        arc2.path.append(QPointF(10, 0));
        arc2.path.append(QPointF(15, -5));
        arc2.path.append(QPointF(20, 0));
        arc2.strokeWidth = 0;
        arc2.fillColor = false;
        arc2.isLocked = true;
        symbol.addArc(arc2);

        SymbolPin pin1{};
        pin1.settings.posX = -10;
        pin1.settings.posY = 0;
        pin1.settings.rotation = 0;
        pin1.settings.spicePinNumber = QStringLiteral("1");
        pin1.settings.type = PinType::Unspecified;
        pin1.pinPath.path = QStringLiteral("M -10 0 h -10");
        pin1.name.text = QStringLiteral("~");
        symbol.addPin(pin1);

        SymbolPin pin2{};
        pin2.settings.posX = 30;
        pin2.settings.posY = 0;
        pin2.settings.rotation = 180;
        pin2.settings.spicePinNumber = QStringLiteral("2");
        pin2.settings.type = PinType::Unspecified;
        pin2.pinPath.path = QStringLiteral("M 30 0 h 10");
        pin2.name.text = QStringLiteral("~");
        symbol.addPin(pin2);

        return symbol;
    }

    // 构造多部件符号黄金测试数据。
    /**
     * @brief 验证 makeMultiPartSymbol 测试场景。
     */
    SymbolData makeMultiPartSymbol() const {
        // symbol：测试用符号数据。
        SymbolData symbol;

        // info：测试用元数据对象。
        SymbolInfo info;
        info.name = QStringLiteral("MULTI_PART_LOGIC");
        info.prefix = QStringLiteral("U");
        info.package = QStringLiteral("SOIC-8");
        info.lcscId = QStringLiteral("C424242");
        symbol.setInfo(info);
        symbol.setBbox({0, 0, 40, 40});

        SymbolPart part1{};
        part1.unitNumber = 0;

        SymbolPolygon polygon{};
        polygon.points = QStringLiteral("0 0 10 0 10 10 0 10");
        polygon.strokeWidth = 0;
        polygon.fillColor = true;
        part1.polygons.append(polygon);

        SymbolText text{};
        text.posX = 5;
        text.posY = 5;
        text.textSize = 5;
        text.text = QStringLiteral("A");
        text.visible = true;
        part1.texts.append(text);

        // invertedPin：测试用引脚数据。
        SymbolPin invertedPin = makePin(-10, 5, 0, QStringLiteral("IN A"), QStringLiteral("1"), PinType::Input);
        invertedPin.dot.isDisplayed = true;
        part1.pins.append(invertedPin);

        // clockPin：测试用引脚数据。
        SymbolPin clockPin = makePin(20, 5, 180, QStringLiteral("CLK"), QStringLiteral("2"), PinType::Input);
        clockPin.clock.isDisplayed = true;
        part1.pins.append(clockPin);

        SymbolPart part2{};
        part2.unitNumber = 1;

        SymbolPath path{};
        path.paths = QStringLiteral("M 0 20 L 10 20 L 10 30 L 0 30 Z");
        path.strokeWidth = 0;
        path.fillColor = false;
        part2.paths.append(path);

        SymbolPin invertedClockPin =
            makePin(-10, 25, 0, QStringLiteral("IN B"), QStringLiteral("3"), PinType::Bidirectional);
        invertedClockPin.dot.isDisplayed = true;
        invertedClockPin.clock.isDisplayed = true;
        part2.pins.append(invertedClockPin);

        // outputPin：测试得到的实际结果数据。
        SymbolPin outputPin = makePin(20, 25, 180, QStringLiteral("OUT"), QStringLiteral("4"), PinType::Output);
        part2.pins.append(outputPin);

        symbol.setParts({part1, part2});
        return symbol;
    }

    // 构造包含特殊字符属性的电阻符号测试数据。
    /**
     * @brief 验证 makeResistorSymbol 测试场景。
     * @param y 测试图元的坐标值。
     * @param rotation 测试图元的旋转角度。
     * @param name 测试使用的标识或名称。
     * @param number 引脚编号。
     * @param type 测试引脚或图元的类型。
     */
    SymbolData makeResistorSymbol() const {
        // symbol：测试用符号数据。
        SymbolData symbol;

        // info：测试用元数据对象。
        SymbolInfo info;
        info.name = QStringLiteral("RESISTOR_0402");
        info.prefix = QStringLiteral("R");
        info.package = QStringLiteral("0603_R");
        info.lcscId = QStringLiteral("C12345");
        info.description = QStringLiteral("10K \"thin film\"\n±1%");
        symbol.setInfo(info);
        symbol.setBbox({-5, -10, 10, 30});

        // pl1：测试用第一层数据。
        SymbolPolyline pl1;
        pl1.points = QStringLiteral("0 20 0 10");
        pl1.strokeWidth = 0;
        pl1.fillColor = false;
        pl1.isLocked = true;
        symbol.addPolyline(pl1);

        // pl2：测试用第二层数据。
        SymbolPolyline pl2;
        pl2.points = QStringLiteral("0 -10 0 -20");
        pl2.strokeWidth = 0;
        pl2.fillColor = false;
        pl2.isLocked = true;
        symbol.addPolyline(pl2);

        // rect：测试用矩形图元。
        SymbolRectangle rect;
        rect.posX = -5;
        rect.posY = 10;
        rect.width = 10;
        rect.height = -20;
        rect.strokeWidth = 0;
        rect.fillColor = QString();
        rect.isLocked = true;
        symbol.addRectangle(rect);

        SymbolPin pin1{};
        pin1.settings.posX = 0;
        pin1.settings.posY = 30;
        pin1.settings.rotation = 90;
        pin1.settings.spicePinNumber = QStringLiteral("1");
        pin1.settings.type = PinType::Unspecified;
        pin1.pinPath.path = QStringLiteral("M 0 30 h -10");
        pin1.name.text = QStringLiteral("~");
        symbol.addPin(pin1);

        SymbolPin pin2{};
        pin2.settings.posX = 0;
        pin2.settings.posY = -30;
        pin2.settings.rotation = 270;
        pin2.settings.spicePinNumber = QStringLiteral("2");
        pin2.settings.type = PinType::Unspecified;
        pin2.pinPath.path = QStringLiteral("M 0 -30 h 10");
        pin2.name.text = QStringLiteral("~");
        symbol.addPin(pin2);

        return symbol;
    }

    /**
     * @brief 验证 makePin 测试场景。
     * @param x 测试图元的坐标值。
     * @param y 测试图元的坐标值。
     * @param rotation 测试图元的旋转角度。
     * @param name 测试使用的标识或名称。
     * @param number 引脚编号。
     * @param type 测试引脚或图元的类型。
     */
    SymbolPin makePin(double x, double y, int rotation, const QString& name, const QString& number, PinType type)
        const {
        SymbolPin pin{};
        pin.settings.posX = x;
        pin.settings.posY = y;
        pin.settings.rotation = rotation;
        pin.settings.spicePinNumber = number;
        pin.settings.type = type;
        pin.pinPath.path = QStringLiteral("M 0 0 h 10");
        pin.name.text = name;
        return pin;
    }
};

QTEST_GUILESS_MAIN(TestKiCadExporterGolden)
#include "test_kicad_exporter_golden.moc"
