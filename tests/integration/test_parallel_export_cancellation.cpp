#include "core/eagle/ExporterEagleFootprint.h"
#include "core/easyeda/EasyedaFootprintImporter.h"
#include "core/easyeda/EasyedaSymbolImporter.h"
#include "core/ir/ComponentDataConverter.h"
#include "models/ComponentData.h"
#include "models/Model3DData.h"
#include "services/ComponentCacheService.h"
#include "services/export/ParallelExportService.h"
#include "tests/common/TestPaths.hpp"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QXmlStreamReader>

using namespace EasyKiConverter;
using namespace EasyKiConverter::Test;

Q_DECLARE_METATYPE(QList<EasyKiConverter::ComponentData>)

class TestParallelExportCancellation : public QObject {
    Q_OBJECT

private slots:

    // 验证阶段完成统计不会覆盖预加载失败项的逐项状态。
    void testStageCompletionPreservesPreloadFailures() {
        ParallelExportService service;

        ExportItemStatus preloadFailure;
        preloadFailure.status = ExportItemStatus::Status::Failed;
        preloadFailure.errorMessage = QStringLiteral("Component preload data missing");
        QVERIFY(QMetaObject::invokeMethod(&service,
                                          "onExportItemStatusChanged",
                                          Qt::DirectConnection,
                                          Q_ARG(QString, QStringLiteral("C90005")),
                                          Q_ARG(QString, QStringLiteral("Model3D")),
                                          Q_ARG(ExportItemStatus, preloadFailure)));

        ExportItemStatus exported;
        exported.status = ExportItemStatus::Status::Success;
        QVERIFY(QMetaObject::invokeMethod(&service,
                                          "onExportItemStatusChanged",
                                          Qt::DirectConnection,
                                          Q_ARG(QString, QStringLiteral("C90006")),
                                          Q_ARG(QString, QStringLiteral("Model3D")),
                                          Q_ARG(ExportItemStatus, exported)));

        QVERIFY(QMetaObject::invokeMethod(&service,
                                          "onExportTypeCompleted",
                                          Qt::DirectConnection,
                                          Q_ARG(QString, QStringLiteral("Model3D")),
                                          Q_ARG(int, 1),
                                          Q_ARG(int, 0),
                                          Q_ARG(int, 0)));

        const ExportTypeProgress progress = service.getTypeProgress(QStringLiteral("Model3D"));
        QCOMPARE(progress.successCount, 1);
        QCOMPARE(progress.failedCount, 1);
        QCOMPARE(progress.completedCount, 2);
    }

    // 注册跨线程测试所需的 Qt 元类型。
    void initTestCase() {
        qRegisterMetaType<ExportOverallProgress>();
        qRegisterMetaType<ExportItemStatus>();
        qRegisterMetaType<QList<ComponentData>>();
    }

    // 验证实际夹具数据可以完成完整导出流程。
    void testFixtureDataCompletesExportPipeline() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        ParallelExportService service;
        const QString libName = QStringLiteral("FixturePipeline");
        service.setOptions(makeOptions(tempDir.path(), libName));
        service.setOutputPath(tempDir.path());

        const QStringList componentIds = makeComponentIds(3);
        const QList<ComponentData> componentData = makeFixtureComponents(componentIds);

        service.startPreload(componentIds);

        QSignalSpy preloadSpy(&service, &ParallelExportService::preloadCompleted);
        const bool preloadInjected = QMetaObject::invokeMethod(
            &service, "onAllComponentDataCollected", Qt::DirectConnection, Q_ARG(QList<ComponentData>, componentData));
        QVERIFY(preloadInjected);
        QCOMPARE(preloadSpy.count(), 1);

        QSignalSpy completedSpy(&service, &ParallelExportService::completed);
        QSignalSpy cancelledSpy(&service, &ParallelExportService::cancelled);
        QSignalSpy failedSpy(&service, &ParallelExportService::failed);

        service.startExport();

        QVERIFY2(completedSpy.wait(30000), "Parallel export should complete with fixture data");
        QCOMPARE(completedSpy.count(), 1);
        QCOMPARE(completedSpy.at(0).at(0).toInt(), componentIds.size());
        QCOMPARE(completedSpy.at(0).at(1).toInt(), 0);
        QCOMPARE(cancelledSpy.count(), 0);
        QCOMPARE(failedSpy.count(), 0);
        QCOMPARE(service.getProgress().currentStage, ExportOverallProgress::Stage::Completed);

        QString error;
        const QString symbolLibraryPath = tempDir.filePath(libName + QStringLiteral(".kicad_sym"));
        QVERIFY2(QFileInfo::exists(symbolLibraryPath), qPrintable(symbolLibraryPath));
        const QString symbolContent = TestPaths::readText(symbolLibraryPath, &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QVERIFY(symbolContent.contains(QStringLiteral("(kicad_symbol_lib")));
        QVERIFY(symbolContent.contains(QStringLiteral("(symbol \"CANCEL_SYM_0\"")));
        QVERIFY(symbolContent.contains(QStringLiteral("\"FixturePipeline:CANCEL_FP_0\"")));

        const QString prettyDirPath = tempDir.filePath(libName + QStringLiteral(".pretty"));
        QVERIFY2(QDir(prettyDirPath).exists(), qPrintable(prettyDirPath));
        const QString footprintPath = QDir(prettyDirPath).filePath(QStringLiteral("CANCEL_FP_0.kicad_mod"));
        QVERIFY2(QFileInfo::exists(footprintPath), qPrintable(footprintPath));
        const QString footprintContent = TestPaths::readText(footprintPath, &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QVERIFY(footprintContent.contains(QStringLiteral("(footprint easykiconverter:CANCEL_FP_0")));
        QVERIFY(footprintContent.contains(QStringLiteral("(pad 1 smd rect")));
    }

    // 验证完整导出流程同时生成符号库、封装库和独立三维模型关联。
    void testPipelineExportsSymbolFootprintAndModel() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        ParallelExportService service;
        ExportOptions options = makeOptions(tempDir.path(), QStringLiteral("CompletePipeline"));
        options.targetFormat = TargetEdaFormat::KiCad;
        options.exportModel3D = true;
        options.exportModel3DFormat = ExportOptions::MODEL_3D_FORMAT_WRL;
        service.setOptions(options);
        service.setOutputPath(tempDir.path());

        const QStringList componentIds = {QStringLiteral("C91001")};
        QList<ComponentData> componentData = makeFixtureComponents(componentIds);
        QVERIFY(componentData.size() == 1);

        // KiCad 只接受本测试需要的矩形和引脚，显式移除 EasyEDA 夹具中的非兼容图元。
        const QSharedPointer<SymbolData> sourceSymbol = componentData.first().symbolData();
        auto symbol = QSharedPointer<SymbolData>::create();
        symbol->setInfo(sourceSymbol->info());
        SymbolBBox bbox = sourceSymbol->bbox();
        bbox.x = -2.0;
        bbox.y = -3.0;
        bbox.width = 4.0;
        bbox.height = 6.0;
        symbol->setBbox(bbox);
        symbol->setPins(sourceSymbol->pins());
        symbol->setRectangles(sourceSymbol->rectangles());
        componentData.first().setSymbolData(symbol);

        // 使用本地 OBJ 夹具，确保集成测试不依赖真实网络或用户缓存。
        Model3DData model;
        model.setName(QStringLiteral("KiCadCompleteModel"));
        model.setUuid(QStringLiteral("kicad-complete-model"));
        componentData.first().setModel3DData(QSharedPointer<Model3DData>::create(model));
        componentData.first().setModel3DObjRaw(QByteArrayLiteral("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n"));

        service.startPreload(componentIds);
        QSignalSpy preloadSpy(&service, &ParallelExportService::preloadCompleted);
        QVERIFY(QMetaObject::invokeMethod(
            &service, "onAllComponentDataCollected", Qt::DirectConnection, Q_ARG(QList<ComponentData>, componentData)));
        QCOMPARE(preloadSpy.count(), 1);

        QSignalSpy completedSpy(&service, &ParallelExportService::completed);
        QSignalSpy failedSpy(&service, &ParallelExportService::failed);
        service.startExport();

        QVERIFY2(completedSpy.wait(30000), "KiCad complete export should finish");
        QCOMPARE(completedSpy.count(), 1);
        QCOMPARE(completedSpy.at(0).at(0).toInt(), 1);
        QCOMPARE(completedSpy.at(0).at(1).toInt(), 0);
        QCOMPARE(failedSpy.count(), 0);

        QString error;
        const QString symbolPath = tempDir.filePath(QStringLiteral("CompletePipeline.kicad_sym"));
        const QString footprintDir = tempDir.filePath(QStringLiteral("CompletePipeline.pretty"));
        const QString footprintPath = QDir(footprintDir).filePath(QStringLiteral("CANCEL_FP_0.kicad_mod"));
        QVERIFY2(QFileInfo::exists(symbolPath), qPrintable(symbolPath));
        QVERIFY2(QFileInfo::exists(footprintDir), qPrintable(footprintDir));
        QVERIFY2(QFileInfo::exists(footprintPath), qPrintable(footprintPath));
        QVERIFY(TestPaths::readText(symbolPath, &error).contains(QStringLiteral("(kicad_symbol_lib")));
        QVERIFY(TestPaths::readText(footprintPath, &error).contains(QStringLiteral("(footprint")));

        const QString modelDir = tempDir.filePath(QStringLiteral("CompletePipeline.3dmodels"));
        const QString manifestPath = QDir(modelDir).filePath(QStringLiteral("manifest.json"));
        QVERIFY2(QFileInfo::exists(manifestPath), qPrintable(manifestPath));
        const QJsonObject manifest = TestPaths::readJsonObject(manifestPath, &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(manifest.value(QStringLiteral("targetFormat")).toInt(), static_cast<int>(TargetEdaFormat::KiCad));
        const QJsonArray components = manifest.value(QStringLiteral("components")).toArray();
        QCOMPARE(components.size(), 1);
        const QJsonObject component = components.first().toObject();
        QCOMPARE(component.value(QStringLiteral("symbol")).toString(), QStringLiteral("CANCEL_SYM_0"));
        QCOMPARE(component.value(QStringLiteral("footprint")).toString(), QStringLiteral("CANCEL_FP_0"));
        QCOMPARE(component.value(QStringLiteral("status")).toString(), QStringLiteral("success"));
        QVERIFY(QFileInfo::exists(QDir(modelDir).filePath(QStringLiteral("KiCadCompleteModel.wrl"))));
    }

    // 验证 PADS 的符号、封装、器件关联文件和独立三维模型可以由同一条导出管线完成。
    void testPadsPipelineExportsAllLibraryArtifacts() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString componentId = QStringLiteral("C91003");
        QList<ComponentData> componentData = makeFixtureComponents({componentId});
        QVERIFY(componentData.size() == 1);

        // PADS 当前只对本测试使用的矩形和引脚提供稳定 ASCII 映射，其他源图元仍由生产代码拒绝并报告诊断。
        const QSharedPointer<SymbolData> sourceSymbol = componentData.first().symbolData();
        auto symbol = QSharedPointer<SymbolData>::create();
        symbol->setInfo(sourceSymbol->info());
        SymbolBBox bbox = sourceSymbol->bbox();
        bbox.x = -2.0;
        bbox.y = -3.0;
        bbox.width = 4.0;
        bbox.height = 6.0;
        symbol->setBbox(bbox);
        symbol->setPins(sourceSymbol->pins());
        QList<SymbolRectangle> rectangles = sourceSymbol->rectangles();
        for (SymbolRectangle& rectangle : rectangles) {
            rectangle.rx = 0.0;
            rectangle.ry = 0.0;
        }
        symbol->setRectangles(rectangles);
        componentData.first().setSymbolData(symbol);

        Model3DData model;
        model.setName(QStringLiteral("PadsCompleteModel"));
        model.setUuid(QStringLiteral("pads-complete-model"));
        componentData.first().setModel3DData(QSharedPointer<Model3DData>::create(model));
        componentData.first().setModel3DObjRaw(QByteArrayLiteral("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n"));

        ParallelExportService service;
        ExportOptions options = makeOptions(tempDir.path(), QStringLiteral("PadsCompletePipeline"));
        options.targetFormat = TargetEdaFormat::Pads;
        options.exportModel3D = true;
        options.exportModel3DFormat = ExportOptions::MODEL_3D_FORMAT_WRL;
        service.setOptions(options);
        service.setOutputPath(tempDir.path());

        service.startPreload({componentId});
        QSignalSpy preloadSpy(&service, &ParallelExportService::preloadCompleted);
        QVERIFY(QMetaObject::invokeMethod(
            &service, "onAllComponentDataCollected", Qt::DirectConnection, Q_ARG(QList<ComponentData>, componentData)));
        QCOMPARE(preloadSpy.count(), 1);

        QSignalSpy completedSpy(&service, &ParallelExportService::completed);
        QSignalSpy failedSpy(&service, &ParallelExportService::failed);
        service.startExport();

        QVERIFY2(completedSpy.wait(30000), "PADS complete export should finish");
        QCOMPARE(completedSpy.count(), 1);
        QCOMPARE(completedSpy.at(0).at(0).toInt(), 1);
        QCOMPARE(completedSpy.at(0).at(1).toInt(), 0);
        QCOMPARE(failedSpy.count(), 0);

        QString error;
        const QString symbolPath = tempDir.filePath(QStringLiteral("PadsCompletePipeline_PADS.c"));
        const QString partTypePath = tempDir.filePath(QStringLiteral("PadsCompletePipeline_PADS.p"));
        const QString footprintDir = tempDir.filePath(QStringLiteral("PadsCompletePipeline_PADS"));
        const QString footprintPath = QDir(footprintDir).filePath(QStringLiteral("CANCEL_FP_0.d"));
        QVERIFY2(QFileInfo::exists(symbolPath), qPrintable(symbolPath));
        QVERIFY2(QFileInfo::exists(partTypePath), qPrintable(partTypePath));
        QVERIFY2(QFileInfo::exists(footprintPath), qPrintable(footprintPath));
        QVERIFY(TestPaths::readText(symbolPath, &error).contains(QStringLiteral("*PADS-LIBRARY-SCH-DECALS-V9*")));
        QVERIFY(TestPaths::readText(partTypePath, &error).contains(QStringLiteral("*PADS-LIBRARY-PART-TYPES-V9*")));
        QVERIFY(TestPaths::readText(partTypePath, &error).contains(QStringLiteral("CANCEL_FP_0")));
        QVERIFY(TestPaths::readText(footprintPath, &error).contains(QStringLiteral("CANCEL_FP_0 I")));

        const QString modelDir = tempDir.filePath(QStringLiteral("PadsCompletePipeline.3dmodels"));
        const QString manifestPath = QDir(modelDir).filePath(QStringLiteral("manifest.json"));
        QVERIFY2(QFileInfo::exists(manifestPath), qPrintable(manifestPath));
        const QJsonObject manifest = TestPaths::readJsonObject(manifestPath, &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(manifest.value(QStringLiteral("targetFormat")).toInt(), static_cast<int>(TargetEdaFormat::Pads));
        const QJsonArray components = manifest.value(QStringLiteral("components")).toArray();
        QCOMPARE(components.size(), 1);
        const QJsonObject component = components.first().toObject();
        QCOMPARE(component.value(QStringLiteral("symbol")).toString(), QStringLiteral("CANCEL_SYM_0"));
        QCOMPARE(component.value(QStringLiteral("footprint")).toString(), QStringLiteral("CANCEL_FP_0"));
        QCOMPARE(component.value(QStringLiteral("status")).toString(), QStringLiteral("success"));
        QVERIFY(QFileInfo::exists(QDir(modelDir).filePath(QStringLiteral("PadsCompleteModel.wrl"))));
    }

    // 验证 CADSTAR 组合库会同时输出符号、封装、Part 关联和独立三维模型。
    void testCadstarPipelineExportsAllLibraryArtifacts() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString componentId = QStringLiteral("C91004");
        QList<ComponentData> componentData = makeFixtureComponents({componentId});
        QVERIFY(componentData.size() == 1);

        // CADSTAR ASCII 组合库只保留当前 writer 可无损表达的符号图元。
        const QSharedPointer<SymbolData> sourceSymbol = componentData.first().symbolData();
        auto symbol = QSharedPointer<SymbolData>::create();
        symbol->setInfo(sourceSymbol->info());
        SymbolBBox bbox = sourceSymbol->bbox();
        bbox.x = -2.0;
        bbox.y = -3.0;
        bbox.width = 4.0;
        bbox.height = 6.0;
        symbol->setBbox(bbox);
        symbol->setPins(sourceSymbol->pins());
        QList<SymbolRectangle> rectangles = sourceSymbol->rectangles();
        for (SymbolRectangle& rectangle : rectangles) {
            rectangle.rx = 0.0;
            rectangle.ry = 0.0;
        }
        symbol->setRectangles(rectangles);
        componentData.first().setSymbolData(symbol);

        Model3DData model;
        model.setName(QStringLiteral("CadstarCompleteModel"));
        model.setUuid(QStringLiteral("cadstar-complete-model"));
        componentData.first().setModel3DData(QSharedPointer<Model3DData>::create(model));
        componentData.first().setModel3DObjRaw(QByteArrayLiteral("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n"));

        ParallelExportService service;
        ExportOptions options = makeOptions(tempDir.path(), QStringLiteral("CadstarCompletePipeline"));
        options.targetFormat = TargetEdaFormat::Cadstar;
        options.exportModel3D = true;
        options.exportModel3DFormat = ExportOptions::MODEL_3D_FORMAT_WRL;
        service.setOptions(options);
        service.setOutputPath(tempDir.path());

        service.startPreload({componentId});
        QSignalSpy preloadSpy(&service, &ParallelExportService::preloadCompleted);
        QVERIFY(QMetaObject::invokeMethod(
            &service, "onAllComponentDataCollected", Qt::DirectConnection, Q_ARG(QList<ComponentData>, componentData)));
        QCOMPARE(preloadSpy.count(), 1);

        QSignalSpy completedSpy(&service, &ParallelExportService::completed);
        QSignalSpy failedSpy(&service, &ParallelExportService::failed);
        service.startExport();

        QVERIFY2(completedSpy.wait(30000), "CADSTAR complete export should finish");
        QCOMPARE(completedSpy.count(), 1);
        QCOMPARE(completedSpy.at(0).at(0).toInt(), 1);
        QCOMPARE(completedSpy.at(0).at(1).toInt(), 0);
        QCOMPARE(failedSpy.count(), 0);

        QString error;
        const QString libraryPath = tempDir.filePath(QStringLiteral("CadstarCompletePipeline.lib"));
        QVERIFY2(QFileInfo::exists(libraryPath), qPrintable(libraryPath));
        const QString libraryText = TestPaths::readText(libraryPath, &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QVERIFY(libraryText.contains(QStringLiteral("COMPONENT")));
        QVERIFY(libraryText.contains(QStringLiteral("PACKAGE")));
        QVERIFY(libraryText.contains(QStringLiteral("PART")));

        const QString modelDir = tempDir.filePath(QStringLiteral("CadstarCompletePipeline.3dmodels"));
        const QString manifestPath = QDir(modelDir).filePath(QStringLiteral("manifest.json"));
        QVERIFY2(QFileInfo::exists(manifestPath), qPrintable(manifestPath));
        const QJsonObject manifest = TestPaths::readJsonObject(manifestPath, &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(manifest.value(QStringLiteral("targetFormat")).toInt(), static_cast<int>(TargetEdaFormat::Cadstar));
        const QJsonArray components = manifest.value(QStringLiteral("components")).toArray();
        QCOMPARE(components.size(), 1);
        const QJsonObject component = components.first().toObject();
        QCOMPARE(component.value(QStringLiteral("symbol")).toString(), QStringLiteral("CANCEL_SYM_0"));
        QCOMPARE(component.value(QStringLiteral("footprint")).toString(), QStringLiteral("CANCEL_FP_0"));
        QCOMPARE(component.value(QStringLiteral("status")).toString(), QStringLiteral("success"));
        QVERIFY(QFileInfo::exists(QDir(modelDir).filePath(QStringLiteral("CadstarCompleteModel.wrl"))));
    }

    // 验证 Eagle 组合库会同时输出符号、封装、器件映射和独立三维模型。
    void testEaglePipelineExportsAllLibraryArtifacts() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        ParallelExportService service;
        ExportOptions options = makeOptions(tempDir.path(), QStringLiteral("EagleCompletePipeline"));
        options.targetFormat = TargetEdaFormat::Eagle;
        options.exportModel3D = true;
        options.exportModel3DFormat = ExportOptions::MODEL_3D_FORMAT_WRL;
        service.setOptions(options);
        service.setOutputPath(tempDir.path());

        const QString componentId = QStringLiteral("C91002");
        QList<ComponentData> componentData = makeFixtureComponents({componentId});
        QVERIFY(componentData.size() == 1);

        // Eagle XML 组合库只接受本测试需要的矩形和引脚，移除 EasyEDA 夹具中的不可表达图元。
        const QSharedPointer<SymbolData> sourceSymbol = componentData.first().symbolData();
        auto symbol = QSharedPointer<SymbolData>::create();
        symbol->setInfo(sourceSymbol->info());
        const auto isEagleRectangle = [](const SymbolRectangle& rectangle) {
            return (rectangle.fillColor.isEmpty() || rectangle.fillColor == QStringLiteral("none")) &&
                   (rectangle.strokeStyle.isEmpty() || rectangle.strokeStyle == QStringLiteral("solid"));
        };
        if (sourceSymbol->isMultiPart()) {
            QList<SymbolPart> eagleParts;
            for (SymbolPart part : sourceSymbol->parts()) {
                QList<SymbolRectangle> eagleRectangles;
                for (const SymbolRectangle& rectangle : part.rectangles) {
                    if (isEagleRectangle(rectangle))
                        eagleRectangles.append(rectangle);
                }
                part.rectangles = eagleRectangles;
                eagleParts.append(part);
            }
            symbol->setParts(eagleParts);
        } else {
            symbol->setPins(sourceSymbol->pins());
            QList<SymbolRectangle> eagleRectangles;
            for (const SymbolRectangle& rectangle : sourceSymbol->rectangles()) {
                if (isEagleRectangle(rectangle))
                    eagleRectangles.append(rectangle);
            }
            symbol->setRectangles(eagleRectangles);
        }
        componentData.first().setSymbolData(symbol);

        // 使用本地 OBJ 数据验证三维阶段，不允许测试隐式访问网络。
        Model3DData model;
        model.setName(QStringLiteral("EagleCompleteModel"));
        model.setUuid(QStringLiteral("eagle-complete-model"));
        componentData.first().setModel3DData(QSharedPointer<Model3DData>::create(model));
        componentData.first().setModel3DObjRaw(QByteArrayLiteral("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n"));

        // 在启动并行阶段前验证同一份 IR，确保失败信息来自 Eagle writer 而不是线程调度。
        ExporterEagleFootprint directExporter;
        const QString directLibraryPath = tempDir.filePath(QStringLiteral("direct-eagle.lbr"));
        const IR::ComponentIR directComponent = IR::toComponentIR(componentData.first());
        QVERIFY2(directExporter.exportComponentLibrary({directComponent}, QStringLiteral("direct"), directLibraryPath),
                 qPrintable(directExporter.diagnostics().join(QStringLiteral("\n"))));

        service.startPreload({componentId});
        QSignalSpy preloadSpy(&service, &ParallelExportService::preloadCompleted);
        QVERIFY(QMetaObject::invokeMethod(
            &service, "onAllComponentDataCollected", Qt::DirectConnection, Q_ARG(QList<ComponentData>, componentData)));
        QCOMPARE(preloadSpy.count(), 1);

        QSignalSpy completedSpy(&service, &ParallelExportService::completed);
        QSignalSpy failedSpy(&service, &ParallelExportService::failed);
        service.startExport();

        QVERIFY2(completedSpy.wait(30000), "Eagle complete export should finish");
        QCOMPARE(completedSpy.count(), 1);
        QCOMPARE(completedSpy.at(0).at(0).toInt(), 1);
        QCOMPARE(completedSpy.at(0).at(1).toInt(), 0);
        QCOMPARE(failedSpy.count(), 0);

        QString error;
        const QString libraryPath = tempDir.filePath(QStringLiteral("EagleCompletePipeline.lbr"));
        QVERIFY2(QFileInfo::exists(libraryPath), qPrintable(libraryPath));
        QFile libraryFile(libraryPath);
        QVERIFY(libraryFile.open(QIODevice::ReadOnly));
        QXmlStreamReader reader(&libraryFile);
        bool symbolSeen = false;
        bool packageSeen = false;
        bool deviceSetSeen = false;
        bool connectSeen = false;
        while (!reader.atEnd()) {
            reader.readNext();
            if (!reader.isStartElement())
                continue;
            symbolSeen = symbolSeen || reader.name() == QStringLiteral("symbol");
            packageSeen = packageSeen || reader.name() == QStringLiteral("package");
            deviceSetSeen = deviceSetSeen || reader.name() == QStringLiteral("deviceset");
            connectSeen = connectSeen || reader.name() == QStringLiteral("connect");
        }
        QVERIFY2(!reader.hasError(), qPrintable(reader.errorString()));
        QVERIFY(symbolSeen);
        QVERIFY(packageSeen);
        QVERIFY(deviceSetSeen);
        QVERIFY(connectSeen);

        const QString modelDir = tempDir.filePath(QStringLiteral("EagleCompletePipeline.3dmodels"));
        const QString manifestPath = QDir(modelDir).filePath(QStringLiteral("manifest.json"));
        QVERIFY2(QFileInfo::exists(manifestPath), qPrintable(manifestPath));
        const QJsonObject manifest = TestPaths::readJsonObject(manifestPath, &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(manifest.value(QStringLiteral("targetFormat")).toInt(), static_cast<int>(TargetEdaFormat::Eagle));
        const QJsonArray components = manifest.value(QStringLiteral("components")).toArray();
        QCOMPARE(components.size(), 1);
        const QJsonObject component = components.first().toObject();
        QCOMPARE(component.value(QStringLiteral("symbol")).toString(), QStringLiteral("CANCEL_SYM_0"));
        QCOMPARE(component.value(QStringLiteral("footprint")).toString(), QStringLiteral("CANCEL_FP_0"));
        QCOMPARE(component.value(QStringLiteral("status")).toString(), QStringLiteral("success"));
        QVERIFY(QFileInfo::exists(QDir(modelDir).filePath(QStringLiteral("EagleCompleteModel.wrl"))));
    }

    // 验证预加载数据不完整时导出流程会明确失败。
    void testMissingPreloadedDataFailsExportPipeline() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        ParallelExportService service;
        const QString libName = QStringLiteral("MissingDataPipeline");
        service.setOptions(makeOptions(tempDir.path(), libName));
        service.setOutputPath(tempDir.path());

        const QStringList componentIds = makeComponentIds(2);
        const QList<ComponentData> invalidComponentData = makeInvalidComponents(componentIds);

        service.startPreload(componentIds);

        QSignalSpy preloadSpy(&service, &ParallelExportService::preloadCompleted);
        const bool preloadInjected = QMetaObject::invokeMethod(&service,
                                                               "onAllComponentDataCollected",
                                                               Qt::DirectConnection,
                                                               Q_ARG(QList<ComponentData>, invalidComponentData));
        QVERIFY(preloadInjected);
        QCOMPARE(preloadSpy.count(), 1);
        QCOMPARE(preloadSpy.at(0).at(0).toInt(), 0);
        QCOMPARE(preloadSpy.at(0).at(1).toInt(), componentIds.size());
        QVERIFY(service.cachedData().isEmpty());

        QSignalSpy completedSpy(&service, &ParallelExportService::completed);
        QSignalSpy cancelledSpy(&service, &ParallelExportService::cancelled);
        QSignalSpy failedSpy(&service, &ParallelExportService::failed);

        service.startExport();

        // Service state is set synchronously; always check first
        QCOMPARE(service.getProgress().currentStage, ExportOverallProgress::Stage::Failed);
        QVERIFY(!service.isRunning());
        QTest::qWait(100);
        QCOMPARE(failedSpy.count(), 1);
        QCOMPARE(failedSpy.at(0).at(0).toString(), QStringLiteral("No exportable components after preload"));
        QCOMPARE(completedSpy.count(), 0);
        QCOMPARE(cancelledSpy.count(), 0);
        QCOMPARE(service.getProgress().currentStage, ExportOverallProgress::Stage::Failed);
        QVERIFY(!service.isRunning());
        QVERIFY(!QFileInfo::exists(tempDir.filePath(libName + QStringLiteral(".kicad_sym"))));
        QVERIFY(!QDir(tempDir.filePath(libName + QStringLiteral(".pretty"))).exists());
    }

    // 验证未注入 ComponentService 时，预加载可以使用实际磁盘缓存完成。
    void testPreloadFallsBackToDiskCacheWithoutComponentService() {
        QTemporaryDir cacheDir;
        QVERIFY(cacheDir.isValid());

        ComponentCacheService* cache = ComponentCacheService::instance();
        const QString originalCacheDir = cache->cacheDir();
        cache->setCacheDir(cacheDir.path());

        const QString componentId = QStringLiteral("C77777");
        ComponentData metadata;
        metadata.setLcscId(componentId);
        metadata.setName(QStringLiteral("Disk Cached Fixture"));
        cache->saveComponentMetadata(componentId, metadata);

        QString error;
        const QString cadData =
            TestPaths::readText(TestPaths::fixturePath(QStringLiteral("easyeda/cad_basic.json")), &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QVERIFY(!cadData.isEmpty());
        cache->saveCadDataJson(componentId, cadData.toUtf8());

        ParallelExportService service;
        const QStringList componentIds = {componentId};
        QSignalSpy preloadSpy(&service, &ParallelExportService::preloadCompleted);
        service.startPreload(componentIds);

        QCOMPARE(preloadSpy.count(), 1);
        QCOMPARE(preloadSpy.at(0).at(0).toInt(), 1);
        QCOMPARE(preloadSpy.at(0).at(1).toInt(), 0);
        QCOMPARE(service.cachedData().size(), 1);
        QVERIFY(service.cachedData().value(componentId)->symbolData() != nullptr);
        QVERIFY(service.cachedData().value(componentId)->footprintData() != nullptr);

        cache->clearAllCache();
        cache->setCacheDir(originalCacheDir);
    }

    // 验证空组件列表会立即结束预加载并报告零成功、零失败。
    void testEmptyPreloadCompletesImmediately() {
        ParallelExportService service;
        QSignalSpy preloadSpy(&service, &ParallelExportService::preloadCompleted);

        service.startPreload({});

        QCOMPARE(preloadSpy.count(), 1);
        QCOMPARE(preloadSpy.at(0).at(0).toInt(), 0);
        QCOMPARE(preloadSpy.at(0).at(1).toInt(), 0);
        QCOMPARE(service.getProgress().currentStage, ExportOverallProgress::Stage::Idle);
        QVERIFY(!service.isRunning());
    }

    // 验证预加载入口会统一大小写并删除重复或空的组件编号。
    void testPreloadNormalizesAndDeduplicatesComponentIds() {
        ParallelExportService service;
        QSignalSpy preloadSpy(&service, &ParallelExportService::preloadCompleted);

        service.startPreload({QStringLiteral("c90007"), QStringLiteral("C90007"), QString()});

        QCOMPARE(preloadSpy.count(), 1);
        QCOMPARE(preloadSpy.at(0).at(0).toInt(), 0);
        QCOMPARE(preloadSpy.at(0).at(1).toInt(), 1);
        const ExportOverallProgress progress = service.getProgress();
        QCOMPARE(progress.totalComponents, 1);
        QCOMPARE(progress.preloadProgress.totalCount, 1);
        QCOMPARE(progress.preloadProgress.completedCount, 1);
    }

    // 验证状态更新会保留先前产生的诊断信息。
    void testItemStatusPreservesDiagnosticsAcrossUpdates() {
        ParallelExportService service;

        ExportItemStatus initialStatus;
        initialStatus.status = ExportItemStatus::Status::Success;
        initialStatus.diagnostics = {QStringLiteral("输入图元 UNKNOWN 未支持")};
        QVERIFY(QMetaObject::invokeMethod(&service,
                                          "onExportItemStatusChanged",
                                          Qt::DirectConnection,
                                          Q_ARG(QString, QStringLiteral("C90001")),
                                          Q_ARG(QString, QStringLiteral("Symbol")),
                                          Q_ARG(ExportItemStatus, initialStatus)));

        ExportItemStatus failedStatus;
        failedStatus.status = ExportItemStatus::Status::Failed;
        failedStatus.errorMessage = QStringLiteral("库写入失败");
        QVERIFY(QMetaObject::invokeMethod(&service,
                                          "onExportItemStatusChanged",
                                          Qt::DirectConnection,
                                          Q_ARG(QString, QStringLiteral("C90001")),
                                          Q_ARG(QString, QStringLiteral("Symbol")),
                                          Q_ARG(ExportItemStatus, failedStatus)));

        const ExportOverallProgress progress = service.getProgress();
        QVERIFY(progress.exportTypeProgress.contains(QStringLiteral("Symbol")));
        const ExportItemStatus finalStatus =
            progress.exportTypeProgress.value(QStringLiteral("Symbol")).itemStatus.value(QStringLiteral("C90001"));
        QCOMPARE(finalStatus.status, ExportItemStatus::Status::Failed);
        QCOMPARE(finalStatus.errorMessage, QStringLiteral("库写入失败"));
        QVERIFY(finalStatus.diagnostics.contains(QStringLiteral("输入图元 UNKNOWN 未支持")));
    }

    // 验证 Altium 三维失败状态不会被封装成功覆盖。
    void testAltiumModel3DFailureIsNotOverwrittenByFootprintSuccess() {
        ParallelExportService service;
        ExportOptions options;
        options.targetFormat = TargetEdaFormat::Altium;
        options.exportModel3D = true;
        service.setOptions(options);

        ExportItemStatus modelFailure;
        modelFailure.status = ExportItemStatus::Status::Failed;
        modelFailure.errorMessage = QStringLiteral("STEP 3D model was not embedded in PcbLib");
        QVERIFY(QMetaObject::invokeMethod(&service,
                                          "onExportItemStatusChanged",
                                          Qt::DirectConnection,
                                          Q_ARG(QString, QStringLiteral("C90003")),
                                          Q_ARG(QString, QStringLiteral("Model3D")),
                                          Q_ARG(ExportItemStatus, modelFailure)));

        ExportItemStatus footprintSuccess;
        footprintSuccess.status = ExportItemStatus::Status::Success;
        QVERIFY(QMetaObject::invokeMethod(&service,
                                          "onExportItemStatusChanged",
                                          Qt::DirectConnection,
                                          Q_ARG(QString, QStringLiteral("C90003")),
                                          Q_ARG(QString, QStringLiteral("Footprint")),
                                          Q_ARG(ExportItemStatus, footprintSuccess)));

        const ExportTypeProgress modelProgress = service.getTypeProgress(QStringLiteral("Model3D"));
        const ExportItemStatus finalStatus = modelProgress.itemStatus.value(QStringLiteral("C90003"));
        QCOMPARE(finalStatus.status, ExportItemStatus::Status::Failed);
        QCOMPARE(finalStatus.errorMessage, modelFailure.errorMessage);
    }

    // 验证 Altium 最终库写入失败会覆盖此前的三维成功状态。
    void testAltiumModel3DFinalFailureReplacesEarlierSuccess() {
        ParallelExportService service;
        ExportOptions options;
        options.targetFormat = TargetEdaFormat::Altium;
        options.exportModel3D = true;
        service.setOptions(options);

        ExportItemStatus modelSuccess;
        modelSuccess.status = ExportItemStatus::Status::Success;
        QVERIFY(QMetaObject::invokeMethod(&service,
                                          "onExportItemStatusChanged",
                                          Qt::DirectConnection,
                                          Q_ARG(QString, QStringLiteral("C90004")),
                                          Q_ARG(QString, QStringLiteral("Model3D")),
                                          Q_ARG(ExportItemStatus, modelSuccess)));

        ExportItemStatus finalFailure;
        finalFailure.status = ExportItemStatus::Status::Failed;
        finalFailure.errorMessage = QStringLiteral("Altium PcbLib 导出失败，3D 模型未写入最终库");
        QVERIFY(QMetaObject::invokeMethod(&service,
                                          "onExportItemStatusChanged",
                                          Qt::DirectConnection,
                                          Q_ARG(QString, QStringLiteral("C90004")),
                                          Q_ARG(QString, QStringLiteral("Model3D")),
                                          Q_ARG(ExportItemStatus, finalFailure)));

        const ExportItemStatus finalStatus =
            service.getTypeProgress(QStringLiteral("Model3D")).itemStatus.value(QStringLiteral("C90004"));
        QCOMPARE(finalStatus.status, ExportItemStatus::Status::Failed);
        QCOMPARE(finalStatus.errorMessage, finalFailure.errorMessage);
    }

    // 验证取消报告会保留已经收集到的诊断信息。
    void testCancellationReportPreservesCollectedDiagnostics() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        ParallelExportService service;
        ExportOptions options = makeOptions(tempDir.path(), QStringLiteral("CancelledReport"));
        options.debugMode = true;
        service.setOptions(options);
        service.setOutputPath(tempDir.path());

        ExportItemStatus status;
        status.status = ExportItemStatus::Status::Success;
        status.diagnostics = {QStringLiteral("输入图元 UNKNOWN 未支持")};
        QVERIFY(QMetaObject::invokeMethod(&service,
                                          "onExportItemStatusChanged",
                                          Qt::DirectConnection,
                                          Q_ARG(QString, QStringLiteral("C90002")),
                                          Q_ARG(QString, QStringLiteral("Symbol")),
                                          Q_ARG(ExportItemStatus, status)));

        service.cancelExport();
        QTest::qWait(100);

        const QString reportPath = tempDir.filePath(QStringLiteral("easykiconverter_export_detailed_report.md"));
        QVERIFY2(QFileInfo::exists(reportPath), qPrintable(reportPath));
        const QString report = TestPaths::readText(reportPath);
        QVERIFY(report.contains(QStringLiteral("export-cancelled")));
        QVERIFY(report.contains(QStringLiteral("输入图元 UNKNOWN 未支持")));
    }

    // 验证取消请求可以停止正在运行的完整导出流程。
    void testCancellationStopsRunningExportPipeline() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        ParallelExportService service;
        service.setOptions(makeOptions(tempDir.path(), QStringLiteral("CancelledPipeline")));
        service.setOutputPath(tempDir.path());

        const QStringList componentIds = makeComponentIds(80);
        const QList<ComponentData> componentData = makeFixtureComponents(componentIds);
        QCOMPARE(componentData.size(), componentIds.size());

        service.startPreload(componentIds);

        QSignalSpy preloadSpy(&service, &ParallelExportService::preloadCompleted);
        const bool preloadInjected = QMetaObject::invokeMethod(
            &service, "onAllComponentDataCollected", Qt::DirectConnection, Q_ARG(QList<ComponentData>, componentData));
        QVERIFY(preloadInjected);
        QCOMPARE(preloadSpy.count(), 1);
        QCOMPARE(preloadSpy.at(0).at(0).toInt(), componentIds.size());
        QCOMPARE(preloadSpy.at(0).at(1).toInt(), 0);
        QCOMPARE(service.cachedData().size(), componentIds.size());

        QSignalSpy failedSpy(&service, &ParallelExportService::failed);
        QSignalSpy cancelledSpy(&service, &ParallelExportService::cancelled);
        QSignalSpy progressSpy(&service, &ParallelExportService::progressChanged);

        service.startExport();
        // 等待进度变化，确保导出已真正开始
        QVERIFY2(progressSpy.wait(5000), "Export did not start within 5s");
        QVERIFY(service.isRunning());

        service.cancelExport();

        // cancelExport() 无条件将状态转为 Cancelled 并发射 cancelled 信号
        QCOMPARE(failedSpy.count(), 0);
        QCOMPARE(cancelledSpy.count(), 1);
        QCOMPARE(service.getProgress().currentStage, ExportOverallProgress::Stage::Cancelled);
        QVERIFY(!service.isRunning());
    }

private:
    static ExportOptions makeOptions(const QString& outputPath, const QString& libName) {
        ExportOptions options;
        options.outputPath = outputPath;
        options.libName = libName;
        options.exportSymbol = true;
        options.exportFootprint = true;
        options.exportModel3D = false;
        options.exportPreviewImages = false;
        options.exportDatasheet = false;
        options.overwriteExistingFiles = true;
        return options;
    }

    static QStringList makeComponentIds(int count) {
        QStringList ids;
        ids.reserve(count);
        for (int i = 0; i < count; ++i) {
            ids.append(QStringLiteral("C9%1").arg(10000 + i));
        }
        return ids;
    }

    static QList<ComponentData> makeInvalidComponents(const QStringList& componentIds) {
        QList<ComponentData> components;
        components.reserve(componentIds.size());

        for (const QString& componentId : componentIds) {
            ComponentData component;
            component.setLcscId(componentId);
            component.setName(QStringLiteral("Invalid Fixture %1").arg(componentId));
            components.append(component);
        }

        return components;
    }

    static QList<ComponentData> makeFixtureComponents(const QStringList& componentIds) {
        QString error;
        const QJsonObject symbolFixture =
            TestPaths::readJsonObject(TestPaths::fixturePath(QStringLiteral("easyeda/symbol_basic.json")), &error);
        if (!error.isEmpty()) {
            qFatal("Unable to read symbol fixture: %s", qPrintable(error));
        }

        const QJsonObject footprintFixture =
            TestPaths::readJsonObject(TestPaths::fixturePath(QStringLiteral("easyeda/footprint_basic.json")), &error);
        if (!error.isEmpty()) {
            qFatal("Unable to read footprint fixture: %s", qPrintable(error));
        }

        EasyedaSymbolImporter symbolImporter;
        EasyedaFootprintImporter footprintImporter;
        const QSharedPointer<SymbolData> baseSymbol = symbolImporter.importSymbolData(symbolFixture);
        const QSharedPointer<FootprintData> baseFootprint = footprintImporter.importFootprintData(footprintFixture);
        if (!baseSymbol || !baseFootprint) {
            qFatal("Unable to import EasyEDA fixtures");
        }

        QList<ComponentData> components;
        components.reserve(componentIds.size());

        for (int i = 0; i < componentIds.size(); ++i) {
            const QString& componentId = componentIds.at(i);
            const QString suffix = QString::number(i);

            auto symbol = QSharedPointer<SymbolData>::create(*baseSymbol);
            SymbolInfo symbolInfo = symbol->info();
            symbolInfo.name = QStringLiteral("CANCEL_SYM_%1").arg(suffix);
            symbolInfo.package = QStringLiteral("CANCEL_FP_%1").arg(suffix);
            symbolInfo.lcscId = componentId;
            symbol->setInfo(symbolInfo);

            auto footprint = QSharedPointer<FootprintData>::create(*baseFootprint);
            FootprintInfo footprintInfo = footprint->info();
            footprintInfo.name = QStringLiteral("CANCEL_FP_%1").arg(suffix);
            footprint->setInfo(footprintInfo);

            ComponentData component;
            component.setLcscId(componentId);
            component.setName(QStringLiteral("Cancellation Fixture %1").arg(suffix));
            component.setSymbolData(symbol);
            component.setFootprintData(footprint);
            components.append(component);
        }

        return components;
    }
};

QTEST_GUILESS_MAIN(TestParallelExportCancellation)
#include "test_parallel_export_cancellation.moc"
