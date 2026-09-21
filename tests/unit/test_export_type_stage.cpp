#include "models/ComponentData.h"
#include "services/ComponentCacheService.h"
#include "services/export/ExportTypeStage.h"
#include "services/export/FootprintExportStage.h"
#include "services/export/Model3DExportStage.h"
#include "services/export/Model3DExportWorker.h"
#include "services/export/PreviewImagesExportWorker.h"
#include "services/export/SymbolExportStage.h"

#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QSet>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest/QtTest>

namespace EasyKiConverter {

class ImmediateSuccessStage final : public ExportTypeStage {
public:
    ImmediateSuccessStage() : ExportTypeStage("ImmediateSuccess", 1, nullptr) {}

protected:
    QObject* createWorker() override {
        return new QObject();
    }

    void startWorker(QObject* worker, const QString& componentId, const QSharedPointer<ComponentData>& data) override {
        Q_UNUSED(data);
        completeItemProgress(worker, componentId, true);
        delete worker;
    }
};

class DeferredStage final : public ExportTypeStage {
public:
    DeferredStage() : ExportTypeStage("Deferred", 1, nullptr) {}

protected:
    QObject* createWorker() override {
        return new QObject();
    }

    void startWorker(QObject* worker, const QString& componentId, const QSharedPointer<ComponentData>& data) override {
        Q_UNUSED(data);
        QTimer::singleShot(0, worker, [this, worker, componentId]() {
            completeItemProgress(worker, componentId, false, QStringLiteral("Cancelled"));
            worker->deleteLater();
        });
    }
};

class FailStage final : public ExportTypeStage {
public:
    FailStage() : ExportTypeStage("Fail", 1, nullptr) {}

protected:
    QObject* createWorker() override {
        return new QObject();
    }

    void startWorker(QObject* worker, const QString& componentId, const QSharedPointer<ComponentData>& data) override {
        Q_UNUSED(data);
        completeItemProgress(worker, componentId, false, QStringLiteral("Export failed: %1").arg(componentId));
        delete worker;
    }
};

class ConcurrentStage final : public ExportTypeStage {
public:
    /** @brief 创建用于验证并发限制的测试阶段。 */
    explicit ConcurrentStage(int maxConcurrent) : ExportTypeStage("Concurrent", maxConcurrent, nullptr) {}

    /** @brief 返回测试阶段观察到的最大并发 Worker 数。 */
    int maxStartedWorkers() const {
        return m_maxStartedWorkers;
    }

protected:
    QObject* createWorker() override {
        return new QObject();
    }

    void startWorker(QObject* worker, const QString& componentId, const QSharedPointer<ComponentData>& data) override {
        Q_UNUSED(data);
        Q_UNUSED(componentId);
        m_startedWorkers++;
        m_maxStartedWorkers = qMax(m_maxStartedWorkers, m_startedWorkers);
        // 使用 deferred 完成，以便观察并发限制
        QTimer::singleShot(50, worker, [this, worker, componentId]() {
            m_startedWorkers--;
            completeItemProgress(worker, componentId, true);
            worker->deleteLater();
        });
    }

private:
    int m_startedWorkers = 0;
    int m_maxStartedWorkers = 0;
};

class TestExportTypeStage : public QObject {
    Q_OBJECT

private slots:

    /** @brief 验证预览图缓存缺少前置索引时仍会导出后续图片。 */
    void previewImageExportLoadsNonContiguousCacheEntries() {
        QTemporaryDir outputDir;
        QTemporaryDir cacheDir;
        QVERIFY(outputDir.isValid());
        QVERIFY(cacheDir.isValid());

        ComponentCacheService* cache = ComponentCacheService::instance();
        const QString previousCacheDir = cache->cacheDir();
        cache->setCacheDir(cacheDir.path());
        cache->clearAllCache();

        QImage image(2, 2, QImage::Format_RGB32);
        image.fill(Qt::green);
        QBuffer buffer;
        QVERIFY(buffer.open(QIODevice::WriteOnly));
        QVERIFY(image.save(&buffer, "PNG"));
        cache->savePreviewImage(QStringLiteral("C45001"), buffer.data(), 1);

        auto component = QSharedPointer<ComponentData>::create();
        component->setLcscId(QStringLiteral("C45001"));
        component->setPreviewImageData({QByteArray(), QByteArray()});

        ExportOptions options;
        options.outputPath = outputDir.path();
        options.libName = QStringLiteral("NonContiguousPreview");
        options.overwriteExistingFiles = true;

        PreviewImagesExportWorker worker;
        QSignalSpy completedSpy(&worker, &PreviewImagesExportWorker::completed);
        worker.setData(QStringLiteral("C45001"), component, options);
        worker.run();

        QCOMPARE(completedSpy.count(), 1);
        QVERIFY(completedSpy.at(0).at(1).toBool());
        const QString exportedPath = outputDir.path() + QDir::separator() +
                                     QStringLiteral("NonContiguousPreview.preview") + QDir::separator() +
                                     QStringLiteral("C45001_preview_1.png");
        QVERIFY(QFileInfo::exists(exportedPath));

        cache->setCacheDir(previousCacheDir);
    }

    // 验证每个组件都能收到开始和完成状态。
    void emitsItemStatusForEveryComponentIncludingLast() {
        ImmediateSuccessStage stage;
        QSignalSpy itemSpy(&stage, &ExportTypeStage::itemStatusChanged);
        QSignalSpy completedSpy(&stage, &ExportTypeStage::completed);

        QStringList ids = {"C1001", "C1002"};
        QMap<QString, QSharedPointer<ComponentData>> cachedData;
        for (const QString& id : ids) {
            cachedData[id] = QSharedPointer<ComponentData>::create();
        }

        stage.start(ids, cachedData);

        // Each component emits 2 signals: InProgress (from startNextWorker) and Success (from completeItemProgress)
        QCOMPARE(itemSpy.count(), ids.size() * 2);
        QCOMPARE(completedSpy.count(), 1);

        const ExportTypeProgress progress = stage.getProgress();
        QCOMPARE(progress.totalCount, ids.size());
        QCOMPARE(progress.completedCount, ids.size());
        QCOMPARE(progress.successCount, ids.size());
        QCOMPARE(progress.failedCount, 0);
    }

    // 验证 completed 信号发出时阶段已经退出运行状态。
    void completedSignalSeesStageNotRunning() {
        ImmediateSuccessStage stage;
        bool wasRunningWhenCompleted = true;
        connect(&stage, &ExportTypeStage::completed, &stage, [&stage, &wasRunningWhenCompleted]() {
            wasRunningWhenCompleted = stage.isRunning();
        });

        QMap<QString, QSharedPointer<ComponentData>> cachedData;
        cachedData[QStringLiteral("C1001")] = QSharedPointer<ComponentData>::create();

        stage.start({QStringLiteral("C1001")}, cachedData);

        QVERIFY(!wasRunningWhenCompleted);
        QVERIFY(!stage.isRunning());
    }

    // 验证取消阶段会等待活跃 Worker 完成后再结束。
    void cancelledStageStaysRunningUntilActiveWorkersDrain() {
        DeferredStage stage;
        QSignalSpy completedSpy(&stage, &ExportTypeStage::completed);

        QStringList ids = {"C1001", "C1002", "C1003"};
        QMap<QString, QSharedPointer<ComponentData>> cachedData;
        for (const QString& id : ids) {
            cachedData[id] = QSharedPointer<ComponentData>::create();
        }

        stage.start(ids, cachedData);
        stage.cancel();

        QVERIFY(stage.isRunning());
        QVERIFY2(completedSpy.wait(1000), "Cancelled stage should finish when active workers drain");
        QVERIFY(!stage.isRunning());
        QCOMPARE(completedSpy.count(), 1);
    }

    // 验证非覆盖模式会保留已有封装文件。
    void footprintLibraryExportPreservesExistingFilesWhenNotOverwriting() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString libName = QStringLiteral("RegressionLib");
        const QString prettyDir = tempDir.path() + QDir::separator() + libName + QStringLiteral(".pretty");
        QVERIFY(QDir().mkpath(prettyDir));

        const QString oldFootprintPath = prettyDir + QDir::separator() + QStringLiteral("OldPackage.kicad_mod");
        QFile oldFootprint(oldFootprintPath);
        QVERIFY(oldFootprint.open(QIODevice::WriteOnly | QIODevice::Text));
        oldFootprint.write("(footprint easykiconverter:OldPackage)\n");
        oldFootprint.close();

        FootprintExportStage stage;
        ExportOptions options;
        options.outputPath = tempDir.path();
        options.libName = libName;
        options.overwriteExistingFiles = false;
        stage.setOptions(options);

        const QString componentId = QStringLiteral("C2040");
        QMap<QString, QSharedPointer<ComponentData>> cachedData;
        cachedData[componentId] = makeFootprintComponent(componentId, QStringLiteral("NewPackage"));

        QSignalSpy completedSpy(&stage, &FootprintExportStage::completed);
        stage.start({componentId}, cachedData);

        QVERIFY2(completedSpy.wait(3000), "Footprint export should complete");
        QCOMPARE(completedSpy.count(), 1);
        QCOMPARE(completedSpy.at(0).at(0).toInt(), 1);
        QCOMPARE(completedSpy.at(0).at(1).toInt(), 0);

        QVERIFY2(QFile::exists(oldFootprintPath), "Existing footprint should be preserved");
        QVERIFY(QFile::exists(prettyDir + QDir::separator() + QStringLiteral("NewPackage.kicad_mod")));
    }

    // 验证封装导出可以生成绝对路径的三维模型引用。
    void footprintLibraryExportCanUseAbsolute3DModelPaths() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString libName = QStringLiteral("PathModeLib");
        const QString componentId = QStringLiteral("C3001");
        const QString footprintName = QStringLiteral("PackageWith3D");
        const QString modelName = QStringLiteral("ModelWith3D");

        FootprintExportStage stage;
        ExportOptions options;
        options.outputPath = tempDir.path();
        options.libName = libName;
        options.exportModel3DFormat = ExportOptions::MODEL_3D_FORMAT_WRL;
        options.exportModel3DPathMode = ExportOptions::MODEL_3D_PATH_ABSOLUTE;
        stage.setOptions(options);

        QMap<QString, QSharedPointer<ComponentData>> cachedData;
        cachedData[componentId] = makeFootprintComponent(componentId, footprintName, modelName);

        QSignalSpy completedSpy(&stage, &FootprintExportStage::completed);
        stage.start({componentId}, cachedData);

        QVERIFY2(completedSpy.wait(3000), "Footprint export should complete");
        QCOMPARE(completedSpy.count(), 1);

        const QString footprintPath = tempDir.path() + QDir::separator() + libName + QStringLiteral(".pretty") +
                                      QDir::separator() + footprintName + QStringLiteral(".kicad_mod");
        QFile footprintFile(footprintPath);
        QVERIFY(footprintFile.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString content = QString::fromUtf8(footprintFile.readAll());

        const QString expectedModelPath =
            QDir::cleanPath(QDir(tempDir.path())
                                .absoluteFilePath(libName + QStringLiteral(".3dmodels") + QDir::separator() +
                                                  modelName + QStringLiteral(".wrl")));
        QVERIFY2(content.contains(QStringLiteral("(model \"%1\"").arg(expectedModelPath)),
                 "Footprint should reference the absolute 3D model path");
        QVERIFY2(!content.contains(QStringLiteral("../%1.3dmodels/%2.wrl").arg(libName, modelName)),
                 "Footprint should not contain a relative 3D model path in absolute mode");
    }

    // 验证封装导出默认使用相对路径的三维模型引用。
    void footprintLibraryExportUsesRelative3DModelPathsByDefault() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString libName = QStringLiteral("RelPathLib");
        const QString componentId = QStringLiteral("C4001");
        const QString footprintName = QStringLiteral("RelPackage");
        const QString modelName = QStringLiteral("RelModel");

        FootprintExportStage stage;
        ExportOptions options;
        options.outputPath = tempDir.path();
        options.libName = libName;
        options.exportModel3DFormat = ExportOptions::MODEL_3D_FORMAT_WRL;
        options.exportModel3DPathMode = ExportOptions::MODEL_3D_PATH_RELATIVE;
        stage.setOptions(options);

        QMap<QString, QSharedPointer<ComponentData>> cachedData;
        cachedData[componentId] = makeFootprintComponent(componentId, footprintName, modelName);

        QSignalSpy completedSpy(&stage, &FootprintExportStage::completed);
        stage.start({componentId}, cachedData);

        QVERIFY2(completedSpy.wait(3000), "Footprint export should complete");

        const QString footprintPath = tempDir.path() + QDir::separator() + libName + QStringLiteral(".pretty") +
                                      QDir::separator() + footprintName + QStringLiteral(".kicad_mod");
        QFile footprintFile(footprintPath);
        QVERIFY(footprintFile.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString content = QString::fromUtf8(footprintFile.readAll());

        const QString expectedWrlPath = QStringLiteral("../%1.3dmodels/%2.wrl").arg(libName, modelName);
        QVERIFY2(content.contains(QStringLiteral("(model \"%1\"").arg(expectedWrlPath)),
                 "Footprint should contain relative WRL model path");

        const QString absolutePrefix = QDir::cleanPath(tempDir.path());
        QVERIFY2(!content.contains(absolutePrefix), "Footprint should not contain absolute paths in relative mode");
    }

    /**
     * @brief 验证 Altium PcbLib 不会静默覆盖已有库
     */
    void altiumFootprintExportRejectsUnsupportedLibraryMerge() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString libName = QStringLiteral("ExistingPcbLib");
        const QString outputPath = tempDir.path() + QDir::separator() + libName + QStringLiteral(".PcbLib");
        QFile existingFile(outputPath);
        QVERIFY(existingFile.open(QIODevice::WriteOnly));
        const QByteArray originalData = QByteArrayLiteral("existing-pcblib");
        QCOMPARE(existingFile.write(originalData), originalData.size());
        existingFile.close();

        FootprintExportStage stage;
        ExportOptions options;
        options.outputPath = tempDir.path();
        options.libName = libName;
        options.targetFormat = TargetEdaFormat::Altium;
        options.overwriteExistingFiles = false;
        stage.setOptions(options);

        QMap<QString, QSharedPointer<ComponentData>> cachedData;
        cachedData[QStringLiteral("C9100")] = makeFootprintComponent(QStringLiteral("C9100"), QStringLiteral("PKG"));

        QSignalSpy completedSpy(&stage, &FootprintExportStage::completed);
        stage.start({QStringLiteral("C9100")}, cachedData);
        QVERIFY2(completedSpy.wait(3000), "Altium footprint export should reject existing library merge");
        QCOMPARE(completedSpy.count(), 1);
        QCOMPARE(completedSpy.at(0).at(0).toInt(), 0);
        QCOMPARE(completedSpy.at(0).at(1).toInt(), 1);

        QVERIFY(existingFile.open(QIODevice::ReadOnly));
        QCOMPARE(existingFile.readAll(), originalData);
        existingFile.close();
    }

    /**
     * @brief 验证 Altium 封装库整体写入失败时所有已收集组件都会标记失败。
     */
    void altiumFootprintLibraryFailureMarksCollectedComponents() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        FootprintExportStage stage;
        ExportOptions options;
        options.outputPath = tempDir.path();
        options.libName = QStringLiteral("InvalidPcbLib");
        options.targetFormat = TargetEdaFormat::Altium;
        options.overwriteExistingFiles = true;
        stage.setOptions(options);

        QMap<QString, QSharedPointer<ComponentData>> cachedData;
        cachedData[QStringLiteral("C9101")] = makeFootprintComponent(QStringLiteral("C9101"), QStringLiteral("VALID"));
        cachedData[QStringLiteral("C9102")] =
            makeFootprintComponent(QStringLiteral("C9102"), QStringLiteral("INVALID"));
        FootprintInfo invalidInfo = cachedData[QStringLiteral("C9102")]->footprintData()->info();
        invalidInfo.name.clear();
        cachedData[QStringLiteral("C9102")]->footprintData()->setInfo(invalidInfo);

        QSignalSpy completedSpy(&stage, &FootprintExportStage::completed);
        QSignalSpy itemSpy(&stage, &FootprintExportStage::itemStatusChanged);
        stage.start({QStringLiteral("C9101"), QStringLiteral("C9102")}, cachedData);

        QVERIFY2(completedSpy.wait(3000), "Altium footprint export should complete with failure");
        QCOMPARE(completedSpy.count(), 1);
        QCOMPARE(completedSpy.at(0).at(0).toInt(), 0);
        QCOMPARE(completedSpy.at(0).at(1).toInt(), 2);

        QSet<QString> failedComponents;
        for (const QList<QVariant>& arguments : itemSpy) {
            const ExportItemStatus status = qvariant_cast<ExportItemStatus>(arguments.at(1));
            if (status.status == ExportItemStatus::Status::Failed)
                failedComponents.insert(arguments.at(0).toString());
        }
        QCOMPARE(failedComponents, QSet<QString>({QStringLiteral("C9101"), QStringLiteral("C9102")}));
    }

    // 验证 Altium 封装阶段优先使用缓存 STEP，并报告嵌入成功。
    void altiumFootprintUsesCachedStepModel() {
        QTemporaryDir outputDir;
        QTemporaryDir cacheDir;
        QVERIFY(outputDir.isValid());
        QVERIFY(cacheDir.isValid());

        ComponentCacheService* cache = ComponentCacheService::instance();
        const QString previousCacheDir = cache->cacheDir();
        cache->setCacheDir(cacheDir.path());
        cache->clearAllCache();

        const QString componentId = QStringLiteral("C12345");
        const QString modelUuid = QStringLiteral("cached-step-model");
        const QByteArray stepData = QByteArrayLiteral("ISO-10303-21;\nDATA;\nENDSEC;\nEND-ISO-10303-21;\n");
        cache->saveModel3D(modelUuid, stepData, QStringLiteral("step"));
        cache->saveModel3D(modelUuid, QByteArrayLiteral("<html>cached error</html>"), QStringLiteral("obj"));
        cache->saveModel3D(
            modelUuid,
            QByteArrayLiteral("#VRML V2.0 utf8\n"
                              "Shape { geometry IndexedFaceSet { coord Coordinate { point [0 0 0, 1 0 0, 0 1 0] } "
                              "coordIndex [0 1 2 -1] } }\n"),
            QStringLiteral("wrl"));

        auto component = makeFootprintComponent(componentId, QStringLiteral("CACHED_PKG"), QStringLiteral("CACHED"));
        Model3DData model = component->footprintData()->model3D();
        model.setUuid(modelUuid);
        component->footprintData()->setModel3D(model);
        auto componentModel = QSharedPointer<Model3DData>::create(model);
        componentModel->setRawObj(QStringLiteral("<html>component error</html>"));
        component->setModel3DData(componentModel);

        FootprintExportStage stage;
        ExportOptions options;
        options.outputPath = outputDir.path();
        options.libName = QStringLiteral("CachedAltium");
        options.targetFormat = TargetEdaFormat::Altium;
        options.exportModel3D = true;
        options.exportModel3DFormat = ExportOptions::MODEL_3D_FORMAT_WRL;
        options.overwriteExistingFiles = true;
        stage.setOptions(options);

        QMap<QString, QSharedPointer<ComponentData>> cachedData;
        cachedData.insert(componentId, component);
        QSignalSpy completedSpy(&stage, &FootprintExportStage::completed);
        QSignalSpy modelSpy(&stage, &FootprintExportStage::embeddedModel3DStatusChanged);
        stage.start({componentId}, cachedData);

        QVERIFY2(completedSpy.wait(3000), "Altium cached STEP export should complete");
        QCOMPARE(completedSpy.count(), 1);
        QCOMPARE(completedSpy.at(0).at(0).toInt(), 1);
        QCOMPARE(completedSpy.at(0).at(1).toInt(), 0);
        QCOMPARE(modelSpy.count(), 1);
        const ExportItemStatus modelStatus = qvariant_cast<ExportItemStatus>(modelSpy.at(0).at(1));
        QCOMPARE(modelStatus.status, ExportItemStatus::Status::Success);
        QVERIFY(QFileInfo::exists(outputDir.filePath(QStringLiteral("CachedAltium.PcbLib"))));

        cache->setCacheDir(previousCacheDir);
    }

    // 验证 Altium 封装阶段会缓存预加载的 STEP 模型。
    void altiumFootprintCachesPreloadedStepModel() {
        QTemporaryDir outputDir;
        QTemporaryDir cacheDir;
        QVERIFY(outputDir.isValid());
        QVERIFY(cacheDir.isValid());

        ComponentCacheService* cache = ComponentCacheService::instance();
        const QString previousCacheDir = cache->cacheDir();
        cache->setCacheDir(cacheDir.path());
        cache->clearAllCache();

        const QString componentId = QStringLiteral("C12348");
        const QString modelUuid = QStringLiteral("preloaded-altium-step");
        const QByteArray stepData = QByteArrayLiteral("ISO-10303-21;\nDATA;\nENDSEC;\nEND-ISO-10303-21;\n");
        const QByteArray objData = QByteArrayLiteral("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n");
        auto component =
            makeFootprintComponent(componentId, QStringLiteral("PRELOADED_PKG"), QStringLiteral("PRELOADED"));
        Model3DData footprintModel = component->footprintData()->model3D();
        footprintModel.setUuid(modelUuid);
        component->footprintData()->setModel3D(footprintModel);
        auto componentModel = QSharedPointer<Model3DData>::create(footprintModel);
        componentModel->setStep(stepData);
        componentModel->setRawObj(QString::fromUtf8(objData));
        component->setModel3DData(componentModel);
        component->setModel3DObjRaw(objData);

        FootprintExportStage stage;
        ExportOptions options;
        options.outputPath = outputDir.path();
        options.libName = QStringLiteral("PreloadedAltium");
        options.targetFormat = TargetEdaFormat::Altium;
        options.exportModel3D = true;
        options.overwriteExistingFiles = true;
        stage.setOptions(options);

        QMap<QString, QSharedPointer<ComponentData>> cachedData;
        cachedData.insert(componentId, component);
        QSignalSpy completedSpy(&stage, &FootprintExportStage::completed);
        stage.start({componentId}, cachedData);

        QVERIFY2(completedSpy.wait(3000), "Altium preloaded STEP export should complete");
        QCOMPARE(completedSpy.at(0).at(0).toInt(), 1);
        QCOMPARE(completedSpy.at(0).at(1).toInt(), 0);
        QCOMPARE(cache->loadModel3D(modelUuid, QStringLiteral("step")), stepData);
        QCOMPARE(cache->loadModel3D(modelUuid, QStringLiteral("obj")), objData);

        cache->setCacheDir(previousCacheDir);
    }

    // 验证预加载的有效 OBJ 会写入公共三维缓存。
    void model3DWorkerCachesPreloadedObj() {
        QTemporaryDir outputDir;
        QTemporaryDir cacheDir;
        QVERIFY(outputDir.isValid());
        QVERIFY(cacheDir.isValid());

        ComponentCacheService* cache = ComponentCacheService::instance();
        const QString previousCacheDir = cache->cacheDir();
        cache->setCacheDir(cacheDir.path());
        cache->clearAllCache();

        const QString componentId = QStringLiteral("C12346");
        const QString modelUuid = QStringLiteral("preloaded-obj-model");
        const QByteArray objData = QByteArrayLiteral("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n");
        auto component = QSharedPointer<ComponentData>::create();
        component->setLcscId(componentId);
        auto model = QSharedPointer<Model3DData>::create();
        model->setUuid(modelUuid);
        model->setName(QStringLiteral("PRELOADED"));
        model->setRawObj(QString::fromUtf8(objData));
        component->setModel3DData(model);
        component->setModel3DObjRaw(objData);

        ExportOptions options;
        options.outputPath = outputDir.path();
        options.exportModel3D = true;
        options.exportModel3DFormat = ExportOptions::MODEL_3D_FORMAT_WRL;
        options.overwriteExistingFiles = true;

        Model3DExportWorker worker;
        QSignalSpy completedSpy(&worker, &Model3DExportWorker::completed);
        worker.setData(componentId, component, options);
        worker.run();

        QCOMPARE(completedSpy.count(), 1);
        QVERIFY(completedSpy.at(0).at(1).toBool());
        QCOMPARE(cache->loadModel3D(modelUuid, QStringLiteral("obj")), objData);

        cache->setCacheDir(previousCacheDir);
    }

    // 验证缓存清空后已排队的三维导出任务不会重新写入旧模型。
    void model3DWorkerRejectsStaleCacheGeneration() {
        QTemporaryDir outputDir;
        QTemporaryDir cacheDir;
        QVERIFY(outputDir.isValid());
        QVERIFY(cacheDir.isValid());

        ComponentCacheService* cache = ComponentCacheService::instance();
        const QString previousCacheDir = cache->cacheDir();
        cache->setCacheDir(cacheDir.path());
        cache->clearAllCache();

        const QString componentId = QStringLiteral("C12348");
        const QString modelUuid = QStringLiteral("stale-generation-model");
        const QByteArray objData = QByteArrayLiteral("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n");
        auto component = QSharedPointer<ComponentData>::create();
        component->setLcscId(componentId);
        auto model = QSharedPointer<Model3DData>::create();
        model->setUuid(modelUuid);
        model->setName(QStringLiteral("STALE_GENERATION"));
        model->setRawObj(QString::fromUtf8(objData));
        component->setModel3DData(model);
        component->setModel3DObjRaw(objData);

        ExportOptions options;
        options.outputPath = outputDir.path();
        options.exportModel3D = true;
        options.exportModel3DFormat = ExportOptions::MODEL_3D_FORMAT_WRL;
        options.overwriteExistingFiles = true;

        Model3DExportWorker worker;
        QSignalSpy completedSpy(&worker, &Model3DExportWorker::completed);
        worker.setData(componentId, component, options);
        cache->clearAllCache();
        worker.run();

        QCOMPARE(completedSpy.count(), 1);
        QVERIFY(completedSpy.at(0).at(1).toBool());
        QVERIFY(cache->loadModel3D(modelUuid, QStringLiteral("obj")).isEmpty());

        cache->setCacheDir(previousCacheDir);
    }

    // 验证预加载的有效 STEP 会被直接导出并写入公共三维缓存。
    void model3DWorkerCachesPreloadedStep() {
        QTemporaryDir outputDir;
        QTemporaryDir cacheDir;
        QVERIFY(outputDir.isValid());
        QVERIFY(cacheDir.isValid());

        ComponentCacheService* cache = ComponentCacheService::instance();
        const QString previousCacheDir = cache->cacheDir();
        cache->setCacheDir(cacheDir.path());
        cache->clearAllCache();

        const QString componentId = QStringLiteral("C12347");
        const QString modelUuid = QStringLiteral("preloaded-step-model");
        const QByteArray stepData = QByteArrayLiteral("ISO-10303-21;\nDATA;\nENDSEC;\nEND-ISO-10303-21;\n");
        auto component = QSharedPointer<ComponentData>::create();
        component->setLcscId(componentId);
        auto model = QSharedPointer<Model3DData>::create();
        model->setUuid(modelUuid);
        model->setName(QStringLiteral("PRELOADED_STEP"));
        model->setStep(stepData);
        component->setModel3DData(model);

        ExportOptions options;
        options.outputPath = outputDir.path();
        options.exportModel3D = true;
        options.exportModel3DFormat = ExportOptions::MODEL_3D_FORMAT_STEP;
        options.overwriteExistingFiles = true;

        Model3DExportWorker worker;
        QSignalSpy completedSpy(&worker, &Model3DExportWorker::completed);
        worker.setData(componentId, component, options);
        worker.run();

        QCOMPARE(completedSpy.count(), 1);
        QVERIFY(completedSpy.at(0).at(1).toBool());
        QCOMPARE(cache->loadModel3D(modelUuid, QStringLiteral("step")), stepData);

        cache->setCacheDir(previousCacheDir);
    }

    // 验证 Altium 封装提前失败时仍会结束对应的 3D 统计状态。
    void altiumMissingFootprintReportsEmbeddedModelFailure() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        FootprintExportStage stage;
        ExportOptions options;
        options.outputPath = tempDir.path();
        options.targetFormat = TargetEdaFormat::Altium;
        options.exportModel3D = true;
        stage.setOptions(options);

        const QString componentId = QStringLiteral("C_MISSING_FOOTPRINT");
        QMap<QString, QSharedPointer<ComponentData>> cachedData;
        cachedData.insert(componentId, QSharedPointer<ComponentData>::create());

        QSignalSpy completedSpy(&stage, &FootprintExportStage::completed);
        QSignalSpy modelSpy(&stage, &FootprintExportStage::embeddedModel3DStatusChanged);
        stage.start({componentId}, cachedData);

        if (completedSpy.count() == 0) {
            QVERIFY2(completedSpy.wait(3000), "Altium missing footprint export should complete");
        }
        QCOMPARE(completedSpy.count(), 1);
        QCOMPARE(completedSpy.at(0).at(0).toInt(), 0);
        QCOMPARE(completedSpy.at(0).at(1).toInt(), 1);
        QCOMPARE(modelSpy.count(), 1);
        const ExportItemStatus modelStatus = qvariant_cast<ExportItemStatus>(modelSpy.at(0).at(1));
        QCOMPARE(modelStatus.status, ExportItemStatus::Status::Failed);
        QCOMPARE(modelStatus.errorMessage, QStringLiteral("No footprint data"));
    }

    // 验证多个符号可以合并写入同一个符号库。
    void symbolLibraryExportMergesMultipleComponentsIntoOneLibrary() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        SymbolExportStage stage;
        ExportOptions options;
        options.outputPath = tempDir.path();
        options.libName = QStringLiteral("MergedSymbols");
        options.overwriteExistingFiles = true;
        stage.setOptions(options);

        QMap<QString, QSharedPointer<ComponentData>> cachedData;
        cachedData[QStringLiteral("C5001")] = makeSymbolComponent(QStringLiteral("C5001"), QStringLiteral("SYM_A"));
        cachedData[QStringLiteral("C5002")] = makeSymbolComponent(QStringLiteral("C5002"), QStringLiteral("SYM_B"));

        QSignalSpy completedSpy(&stage, &SymbolExportStage::completed);
        stage.start({QStringLiteral("C5001"), QStringLiteral("C5002")}, cachedData);

        QVERIFY2(completedSpy.wait(3000), "Symbol export should complete");
        QCOMPARE(completedSpy.count(), 1);
        QCOMPARE(completedSpy.at(0).at(0).toInt(), 2);
        QCOMPARE(completedSpy.at(0).at(1).toInt(), 0);

        const QString symbolPath = tempDir.path() + QDir::separator() + QStringLiteral("MergedSymbols.kicad_sym");
        QFile symbolFile(symbolPath);
        QVERIFY(symbolFile.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString content = QString::fromUtf8(symbolFile.readAll());
        QVERIFY(content.contains(QStringLiteral("(symbol \"SYM_A\"")));
        QVERIFY(content.contains(QStringLiteral("(symbol \"SYM_B\"")));
        QVERIFY(!QDir(tempDir.path() + QDir::separator() + QStringLiteral(".tmp")).exists());
    }

    // 验证 PADS 符号图形和 Part Type 关联文件随同一导出事务提交。
    void padsSymbolLibraryCommitsCompanionPartType() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        SymbolExportStage stage;
        ExportOptions options;
        options.outputPath = tempDir.path();
        options.libName = QStringLiteral("PadsSymbols");
        options.targetFormat = TargetEdaFormat::Pads;
        options.overwriteExistingFiles = true;
        stage.setOptions(options);

        QMap<QString, QSharedPointer<ComponentData>> cachedData;
        cachedData[QStringLiteral("C_PADS_SYMBOL")] =
            makeSymbolComponent(QStringLiteral("C_PADS_SYMBOL"), QStringLiteral("PADS_SYMBOL"));

        QSignalSpy completedSpy(&stage, &SymbolExportStage::completed);
        stage.start({QStringLiteral("C_PADS_SYMBOL")}, cachedData);
        QVERIFY2(completedSpy.wait(3000), "PADS symbol export should complete");
        QCOMPARE(completedSpy.count(), 1);
        QCOMPARE(completedSpy.at(0).at(0).toInt(), 1);
        QCOMPARE(completedSpy.at(0).at(1).toInt(), 0);

        QFile schematicFile(tempDir.path() + QDir::separator() + QStringLiteral("PadsSymbols_PADS.c"));
        QVERIFY(schematicFile.exists());
        QFile partTypeFile(tempDir.path() + QDir::separator() + QStringLiteral("PadsSymbols_PADS.p"));
        QVERIFY(partTypeFile.exists());
        QVERIFY(partTypeFile.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString partTypeContent = QString::fromUtf8(partTypeFile.readAll());
        QVERIFY(partTypeContent.contains(QStringLiteral("PADS_SYMBOL PKG_C_PADS_SYMBOL")));
    }

    // 验证 Eagle 组合库阶段同时提交 Symbol、Package、DeviceSet 和引脚焊盘关联。
    void eagleCombinedLibraryStageWritesAllLibrarySections() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        FootprintExportStage stage;
        ExportOptions options;
        options.outputPath = tempDir.path();
        options.libName = QStringLiteral("EagleCombined");
        options.targetFormat = TargetEdaFormat::Eagle;
        options.exportSymbol = true;
        options.exportFootprint = true;
        options.overwriteExistingFiles = true;
        stage.setOptions(options);

        QMap<QString, QSharedPointer<ComponentData>> cachedData;
        cachedData[QStringLiteral("C_EAGLE_COMBINED")] =
            makeCombinedLibraryComponent(QStringLiteral("C_EAGLE_COMBINED"), QStringLiteral("EAGLE_PACKAGE"));

        QSignalSpy completedSpy(&stage, &FootprintExportStage::completed);
        stage.start({QStringLiteral("C_EAGLE_COMBINED")}, cachedData);
        QVERIFY2(completedSpy.wait(3000), "Eagle combined library export should complete");
        QCOMPARE(completedSpy.count(), 1);
        QCOMPARE(completedSpy.at(0).at(0).toInt(), 1);
        QCOMPARE(completedSpy.at(0).at(1).toInt(), 0);

        QFile output(tempDir.filePath(QStringLiteral("EagleCombined.lbr")));
        QVERIFY(output.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString content = QString::fromUtf8(output.readAll());
        QVERIFY(content.contains(QStringLiteral("<symbols>")));
        QVERIFY(content.contains(QStringLiteral("<packages>")));
        QVERIFY(content.contains(QStringLiteral("<devicesets>")));
        QVERIFY(content.contains(QStringLiteral("package=\"EAGLE_PACKAGE\"")));
        QVERIFY(content.contains(QStringLiteral("pin=\"PIN1\"")));
        QVERIFY(content.contains(QStringLiteral("pad=\"1\"")));
    }

    // 验证 CADSTAR 组合库阶段同时提交 Component、Package、Pad 和 Part 关联。
    void cadstarCombinedLibraryStageWritesAllLibrarySections() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        FootprintExportStage stage;
        ExportOptions options;
        options.outputPath = tempDir.path();
        options.libName = QStringLiteral("CadstarCombined");
        options.targetFormat = TargetEdaFormat::Cadstar;
        options.exportSymbol = true;
        options.exportFootprint = true;
        options.overwriteExistingFiles = true;
        stage.setOptions(options);

        QMap<QString, QSharedPointer<ComponentData>> cachedData;
        cachedData[QStringLiteral("C_CADSTAR_COMBINED")] =
            makeCombinedLibraryComponent(QStringLiteral("C_CADSTAR_COMBINED"), QStringLiteral("CADSTAR_PACKAGE"));

        QSignalSpy completedSpy(&stage, &FootprintExportStage::completed);
        stage.start({QStringLiteral("C_CADSTAR_COMBINED")}, cachedData);
        QVERIFY2(completedSpy.wait(3000), "CADSTAR combined library export should complete");
        QCOMPARE(completedSpy.count(), 1);
        QCOMPARE(completedSpy.at(0).at(0).toInt(), 1);
        QCOMPARE(completedSpy.at(0).at(1).toInt(), 0);

        QFile output(tempDir.filePath(QStringLiteral("CadstarCombined.lib")));
        QVERIFY(output.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString content = QString::fromUtf8(output.readAll());
        QVERIFY(content.contains(QStringLiteral("COMPONENT \"CADSTAR_SYMBOL\"")));
        QVERIFY(content.contains(QStringLiteral("PACKAGE \"CADSTAR_PACKAGE\"")));
        QVERIFY(content.contains(QStringLiteral("PAD \"CADSTAR_PACKAGE_PAD_1\"")));
        QVERIFY(content.contains(QStringLiteral("PART \"C_CADSTAR_COMBINED\"")));
    }

    // 验证 Xpedition 符号库不会在禁止覆盖时改写已有 ZIP。
    void xpeditionSymbolRejectsNoOverwriteAndUpdateModes() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString finalPath = tempDir.path() + QDir::separator() + QStringLiteral("Existing_Symbols.zip");
        QFile existing(finalPath);
        QVERIFY(existing.open(QIODevice::WriteOnly));
        existing.write("original-xpedition-symbol");
        existing.close();

        for (const auto mode : {0, 1, 2}) {
            SymbolExportStage stage;
            ExportOptions options;
            options.outputPath = tempDir.path();
            options.libName = QStringLiteral("Existing");
            options.targetFormat = TargetEdaFormat::Xpedition;
            options.overwriteExistingFiles = mode == 2;
            options.updateMode = mode == 1;
            options.retryMode = mode == 2;
            stage.setOptions(options);

            QMap<QString, QSharedPointer<ComponentData>> cachedData;
            cachedData[QStringLiteral("C_XPEDITION")] =
                makeSymbolComponent(QStringLiteral("C_XPEDITION"), QStringLiteral("XPEDITION_SYMBOL"));
            QSignalSpy completedSpy(&stage, &SymbolExportStage::completed);
            stage.start({QStringLiteral("C_XPEDITION")}, cachedData);
            QVERIFY2(completedSpy.wait(3000), "Xpedition symbol policy should complete");
            QCOMPARE(completedSpy.at(0).at(0).toInt(), 0);
            QCOMPARE(completedSpy.at(0).at(1).toInt(), 1);
            QVERIFY(stage.getProgress().diagnostics.join(QStringLiteral("\n")).contains(QStringLiteral("Xpedition")));
        }

        QFile unchanged(finalPath);
        QVERIFY(unchanged.open(QIODevice::ReadOnly));
        QCOMPARE(unchanged.readAll(), QByteArray("original-xpedition-symbol"));
    }

    // 验证 Xpedition 封装库的禁止覆盖、更新和重试模式不会绕过安全策略。
    void xpeditionFootprintRejectsNoOverwriteAndUpdateModes() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString finalPath = tempDir.path() + QDir::separator() + QStringLiteral("Existing_Footprints.zip");
        QFile existing(finalPath);
        QVERIFY(existing.open(QIODevice::WriteOnly));
        existing.write("original-xpedition-footprint");
        existing.close();

        for (const auto mode : {0, 1, 2}) {
            FootprintExportStage stage;
            ExportOptions options;
            options.outputPath = tempDir.path();
            options.libName = QStringLiteral("Existing");
            options.targetFormat = TargetEdaFormat::Xpedition;
            options.overwriteExistingFiles = mode == 2;
            options.updateMode = mode == 1;
            options.retryMode = mode == 2;
            stage.setOptions(options);

            QMap<QString, QSharedPointer<ComponentData>> cachedData;
            cachedData[QStringLiteral("C_XPEDITION")] =
                makeFootprintComponent(QStringLiteral("C_XPEDITION"), QStringLiteral("XPEDITION_FOOTPRINT"));
            QSignalSpy completedSpy(&stage, &FootprintExportStage::completed);
            stage.start({QStringLiteral("C_XPEDITION")}, cachedData);
            QVERIFY2(completedSpy.wait(3000), "Xpedition footprint policy should complete");
            QCOMPARE(completedSpy.at(0).at(0).toInt(), 0);
            QCOMPARE(completedSpy.at(0).at(1).toInt(), 1);
            QVERIFY(stage.getProgress()
                        .itemStatus.value(QStringLiteral("C_XPEDITION"))
                        .errorMessage.contains(QStringLiteral("Xpedition")));
        }

        QFile unchanged(finalPath);
        QVERIFY(unchanged.open(QIODevice::ReadOnly));
        QCOMPARE(unchanged.readAll(), QByteArray("original-xpedition-footprint"));
    }

    // 验证 PADS Decal 目录已有且禁止覆盖时不会被临时导出替换。
    void padsFootprintRejectsExistingDirectoryWithoutOverwrite() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString finalPath = tempDir.path() + QDir::separator() + QStringLiteral("Existing_PADS");
        QVERIFY(QDir().mkpath(finalPath));
        QFile marker(finalPath + QDir::separator() + QStringLiteral("keep.txt"));
        QVERIFY(marker.open(QIODevice::WriteOnly));
        marker.write("keep");
        marker.close();

        FootprintExportStage stage;
        ExportOptions options;
        options.outputPath = tempDir.path();
        options.libName = QStringLiteral("Existing");
        options.targetFormat = TargetEdaFormat::Pads;
        options.overwriteExistingFiles = false;
        stage.setOptions(options);

        QMap<QString, QSharedPointer<ComponentData>> cachedData;
        cachedData[QStringLiteral("C_PADS")] =
            makeFootprintComponent(QStringLiteral("C_PADS"), QStringLiteral("PADS_FOOTPRINT"));
        QSignalSpy completedSpy(&stage, &FootprintExportStage::completed);
        stage.start({QStringLiteral("C_PADS")}, cachedData);
        QVERIFY2(completedSpy.wait(3000), "PADS overwrite policy should complete");
        QCOMPARE(completedSpy.at(0).at(0).toInt(), 0);
        QCOMPARE(completedSpy.at(0).at(1).toInt(), 1);
        QVERIFY(stage.getProgress().diagnostics.join(QStringLiteral("\n")).contains(QStringLiteral("PADS")));
        QFile unchanged(marker.fileName());
        QVERIFY(unchanged.open(QIODevice::ReadOnly));
        QCOMPARE(unchanged.readAll(), QByteArray("keep"));
    }

    // 验证 P-CAD ASCII 单文件已有且禁止覆盖时不会替换原文件。
    void pcadFootprintRejectsExistingFileWithoutOverwrite() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString finalPath = tempDir.path() + QDir::separator() + QStringLiteral("Existing.lia");
        QFile existing(finalPath);
        QVERIFY(existing.open(QIODevice::WriteOnly));
        existing.write("keep-pcad");
        existing.close();

        FootprintExportStage stage;
        ExportOptions options;
        options.outputPath = tempDir.path();
        options.libName = QStringLiteral("Existing");
        options.targetFormat = TargetEdaFormat::Pcad;
        options.overwriteExistingFiles = false;
        stage.setOptions(options);

        QMap<QString, QSharedPointer<ComponentData>> cachedData;
        cachedData[QStringLiteral("C_PCAD")] =
            makeFootprintComponent(QStringLiteral("C_PCAD"), QStringLiteral("PCAD_FOOTPRINT"));
        QSignalSpy completedSpy(&stage, &FootprintExportStage::completed);
        stage.start({QStringLiteral("C_PCAD")}, cachedData);
        QVERIFY2(completedSpy.wait(3000), "P-CAD overwrite policy should complete");
        QCOMPARE(completedSpy.at(0).at(0).toInt(), 0);
        QCOMPARE(completedSpy.at(0).at(1).toInt(), 1);
        QVERIFY(stage.getProgress().diagnostics.join(QStringLiteral("\n")).contains(QStringLiteral("P-CAD")));

        QFile unchanged(finalPath);
        QVERIFY(unchanged.open(QIODevice::ReadOnly));
        QCOMPARE(unchanged.readAll(), QByteArray("keep-pcad"));
    }

    // 验证符号输入诊断会随导出进度暴露给调用方。
    void symbolLibraryExportEmitsInputDiagnostics() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        SymbolExportStage stage;
        ExportOptions options;
        options.outputPath = tempDir.path();
        options.libName = QStringLiteral("DiagnosticSymbols");
        options.overwriteExistingFiles = true;
        stage.setOptions(options);

        QMap<QString, QSharedPointer<ComponentData>> cachedData;
        cachedData[QStringLiteral("C5101")] = makeSymbolComponent(QStringLiteral("C5101"), QStringLiteral("SYM_DIAG"));

        QSignalSpy itemSpy(&stage, &SymbolExportStage::itemStatusChanged);
        QSignalSpy completedSpy(&stage, &SymbolExportStage::completed);
        stage.start({QStringLiteral("C5101")}, cachedData);

        QVERIFY2(completedSpy.wait(3000), "Diagnostic symbol export should complete");
        bool foundDiagnostics = false;
        for (const QList<QVariant>& arguments : itemSpy) {
            const ExportItemStatus status = qvariant_cast<ExportItemStatus>(arguments.at(1));
            if (status.status == ExportItemStatus::Status::Success) {
                QVERIFY(!status.diagnostics.isEmpty());
                foundDiagnostics = true;
            }
        }
        QVERIFY(foundDiagnostics);
    }

    // 验证缺少符号数据时会报告失败而不是生成空库。
    void symbolLibraryExportReportsMissingSymbolData() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        SymbolExportStage stage;
        ExportOptions options;
        options.outputPath = tempDir.path();
        options.libName = QStringLiteral("MissingSymbol");
        stage.setOptions(options);

        QMap<QString, QSharedPointer<ComponentData>> cachedData;
        cachedData[QStringLiteral("C6001")] = QSharedPointer<ComponentData>::create();

        QSignalSpy completedSpy(&stage, &SymbolExportStage::completed);
        QSignalSpy itemSpy(&stage, &SymbolExportStage::itemStatusChanged);
        stage.start({QStringLiteral("C6001")}, cachedData);

        QVERIFY2(completedSpy.wait(3000), "Symbol export should complete");
        QCOMPARE(completedSpy.count(), 1);
        QCOMPARE(completedSpy.at(0).at(0).toInt(), 0);
        QCOMPARE(completedSpy.at(0).at(1).toInt(), 1);
        QVERIFY(!QFile::exists(tempDir.path() + QDir::separator() + QStringLiteral("MissingSymbol.kicad_sym")));

        QVERIFY(itemSpy.count() >= 1);
        const auto args = itemSpy.takeFirst();
        QCOMPARE(args.at(0).toString(), QStringLiteral("C6001"));
        const auto status = qvariant_cast<ExportItemStatus>(args.at(1));
        QCOMPARE(status.status, ExportItemStatus::Status::Failed);
        QCOMPARE(status.errorMessage, QStringLiteral("No symbol data"));
    }

    // 验证符号库提交失败时会回滚临时文件。
    void symbolLibraryExportRollsBackTempFileOnCommitFailure() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString blockerPath = tempDir.path() + QDir::separator() + QStringLiteral("blocked");
        QFile blocker(blockerPath);
        QVERIFY(blocker.open(QIODevice::WriteOnly | QIODevice::Text));
        blocker.write("not a directory");
        blocker.close();

        SymbolExportStage stage;
        ExportOptions options;
        options.outputPath = blockerPath;
        options.libName = QStringLiteral("RollbackSymbols");
        options.overwriteExistingFiles = true;
        stage.setOptions(options);

        QMap<QString, QSharedPointer<ComponentData>> cachedData;
        cachedData[QStringLiteral("C7001")] = makeSymbolComponent(QStringLiteral("C7001"), QStringLiteral("SYM_FAIL"));

        QSignalSpy completedSpy(&stage, &SymbolExportStage::completed);
        stage.start({QStringLiteral("C7001")}, cachedData);

        QVERIFY2(completedSpy.wait(3000), "Symbol export should complete");
        QCOMPARE(completedSpy.count(), 1);
        QCOMPARE(completedSpy.at(0).at(0).toInt(), 0);
        QCOMPARE(completedSpy.at(0).at(1).toInt(), 1);
        QVERIFY(!QFile::exists(blockerPath + QDir::separator() + QStringLiteral("RollbackSymbols.kicad_sym")));
        QVERIFY(!QDir(blockerPath + QDir::separator() + QStringLiteral(".tmp")).exists());
    }

    // === 新增：ExportTypeStage 基类行为测试 ===

    // 验证空组件列表会立即完成且不创建 Worker。
    void emptyListComponentpletesImmediately() {
        ImmediateSuccessStage stage;
        QSignalSpy completedSpy(&stage, &ExportTypeStage::completed);

        QMap<QString, QSharedPointer<ComponentData>> cachedData;
        stage.start(QStringList(), cachedData);

        QCOMPARE(completedSpy.count(), 1);
        QCOMPARE(completedSpy.at(0).at(0).toInt(), 0);  // successCount
        QCOMPARE(completedSpy.at(0).at(1).toInt(), 0);  // failedCount
        QCOMPARE(completedSpy.at(0).at(2).toInt(), 0);  // skippedCount
        QVERIFY(!stage.isRunning());
    }

    // 验证没有 3D 模型 UUID 的元器件计入跳过，而不是被统计为成功。
    void model3DWithoutUuidIsSkipped() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        Model3DExportStage stage;
        ExportOptions options;
        options.outputPath = tempDir.path();
        options.libName = QStringLiteral("SkippedModels");
        options.exportModel3DFormat = ExportOptions::MODEL_3D_FORMAT_WRL;
        stage.setOptions(options);

        QMap<QString, QSharedPointer<ComponentData>> cachedData;
        cachedData[QStringLiteral("C_NO_MODEL")] = QSharedPointer<ComponentData>::create();

        QSignalSpy itemSpy(&stage, &ExportTypeStage::itemStatusChanged);
        QSignalSpy completedSpy(&stage, &ExportTypeStage::completed);
        stage.start({QStringLiteral("C_NO_MODEL")}, cachedData);

        if (completedSpy.count() == 0)
            QVERIFY2(completedSpy.wait(3000), "Model3D skip should complete");
        QCOMPARE(completedSpy.count(), 1);
        QCOMPARE(completedSpy.at(0).at(0).toInt(), 0);
        QCOMPARE(completedSpy.at(0).at(1).toInt(), 0);
        QCOMPARE(completedSpy.at(0).at(2).toInt(), 1);

        const ExportTypeProgress progress = stage.getProgress();
        QCOMPARE(progress.totalCount, 1);
        QCOMPARE(progress.completedCount, 1);
        QCOMPARE(progress.successCount, 0);
        QCOMPARE(progress.skippedCount, 1);
        QCOMPARE(progress.itemStatus.value(QStringLiteral("C_NO_MODEL")).status, ExportItemStatus::Status::Skipped);
        QVERIFY(itemSpy.count() >= 2);
    }

    // 验证输出目录创建失败时，每个元器件都会收敛为失败状态。
    void model3DOutputDirectoryFailureIsReportedPerComponent() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString blockingFilePath = tempDir.filePath(QStringLiteral("output-file"));
        QFile blockingFile(blockingFilePath);
        QVERIFY(blockingFile.open(QIODevice::WriteOnly));
        blockingFile.close();

        Model3DExportStage stage;
        ExportOptions options;
        options.outputPath = blockingFilePath;
        options.libName = QStringLiteral("UnwritableModels");
        options.exportModel3DFormat = ExportOptions::MODEL_3D_FORMAT_WRL;
        stage.setOptions(options);

        const QString componentId = QStringLiteral("C_OUTPUT_FAILURE");
        QMap<QString, QSharedPointer<ComponentData>> cachedData;
        cachedData[componentId] = QSharedPointer<ComponentData>::create();

        QSignalSpy completedSpy(&stage, &ExportTypeStage::completed);
        stage.start({componentId}, cachedData);

        if (completedSpy.count() == 0)
            QVERIFY2(completedSpy.wait(3000), "Model3D output failure should complete");
        QCOMPARE(completedSpy.count(), 1);
        QCOMPARE(completedSpy.at(0).at(0).toInt(), 0);
        QCOMPARE(completedSpy.at(0).at(1).toInt(), 1);
        QCOMPARE(completedSpy.at(0).at(2).toInt(), 0);

        const ExportTypeProgress progress = stage.getProgress();
        QCOMPARE(progress.totalCount, 1);
        QCOMPARE(progress.completedCount, 1);
        QCOMPARE(progress.failedCount, 1);
        QCOMPARE(progress.itemStatus.value(componentId).status, ExportItemStatus::Status::Failed);
    }

    // 验证封装库阶段的失败统计包含预加载缺失和库写入失败的全部元器件。
    void footprintLibraryFailureCountsAllComponents() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString blockerPath = tempDir.filePath(QStringLiteral("blocked-output"));
        QFile blocker(blockerPath);
        QVERIFY(blocker.open(QIODevice::WriteOnly));
        blocker.close();

        FootprintExportStage stage;
        ExportOptions options;
        options.outputPath = blockerPath;
        options.libName = QStringLiteral("BrokenFootprints");
        options.targetFormat = TargetEdaFormat::KiCad;
        stage.setOptions(options);

        auto component = QSharedPointer<ComponentData>::create();
        component->setFootprintData(QSharedPointer<FootprintData>::create());
        QMap<QString, QSharedPointer<ComponentData>> cachedData;
        cachedData[QStringLiteral("C_VALID")] = component;

        QSignalSpy progressSpy(&stage, &ExportTypeStage::progressChanged);
        QSignalSpy completedSpy(&stage, &ExportTypeStage::completed);
        stage.start({QStringLiteral("C_MISSING"), QStringLiteral("C_VALID")}, cachedData);

        if (completedSpy.count() == 0)
            QVERIFY2(completedSpy.wait(3000), "Footprint export should complete");
        QCOMPARE(completedSpy.count(), 1);
        QCOMPARE(completedSpy.at(0).at(0).toInt(), 0);
        QCOMPARE(completedSpy.at(0).at(1).toInt(), 2);
        QCOMPARE(completedSpy.at(0).at(2).toInt(), 0);

        const ExportTypeProgress progress = stage.getProgress();
        QCOMPARE(progress.totalCount, 2);
        QCOMPARE(progress.completedCount, 2);
        QCOMPARE(progress.successCount, 0);
        QCOMPARE(progress.failedCount, 2);
        QCOMPARE(progress.inProgressCount, 0);
        QVERIFY(progressSpy.count() >= 2);
    }

    // 验证封装数据中的 3D UUID 也能驱动独立三维导出阶段。
    void model3DUuidFromFootprintIsExported() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        Model3DExportStage stage;
        ExportOptions options;
        options.outputPath = tempDir.path();
        options.libName = QStringLiteral("FootprintModels");
        options.exportModel3DFormat = ExportOptions::MODEL_3D_FORMAT_WRL;
        options.overwriteExistingFiles = true;
        stage.setOptions(options);

        auto footprint = QSharedPointer<FootprintData>::create();
        Model3DData model;
        model.setUuid(QStringLiteral("footprint-model-123"));
        model.setName(QStringLiteral("FOOTPRINT_MODEL"));
        footprint->setModel3D(model);
        auto component = QSharedPointer<ComponentData>::create();
        component->setModel3DData(QSharedPointer<Model3DData>::create());
        component->setFootprintData(footprint);
        component->setModel3DObjRaw(QByteArrayLiteral("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n"));

        QMap<QString, QSharedPointer<ComponentData>> cachedData;
        cachedData[QStringLiteral("C_FOOTPRINT_MODEL")] = component;

        QSignalSpy completedSpy(&stage, &ExportTypeStage::completed);
        stage.start({QStringLiteral("C_FOOTPRINT_MODEL")}, cachedData);

        if (completedSpy.count() == 0)
            QVERIFY2(completedSpy.wait(3000), "Footprint model export should complete");
        QCOMPARE(completedSpy.count(), 1);
        QCOMPARE(completedSpy.at(0).at(0).toInt(), 1);
        QCOMPARE(completedSpy.at(0).at(1).toInt(), 0);
        QCOMPARE(completedSpy.at(0).at(2).toInt(), 0);
        QVERIFY(QFile::exists(tempDir.filePath(QStringLiteral("FootprintModels.3dmodels/C_FOOTPRINT_MODEL.wrl"))));
    }

    // 验证相同模型名称会被稳定去重，避免独立三维模型文件互相覆盖。
    void duplicateModelNamesProduceDistinctFiles() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        Model3DExportStage stage;
        ExportOptions options;
        options.outputPath = tempDir.path();
        options.libName = QStringLiteral("DuplicateModels");
        options.exportModel3DFormat = ExportOptions::MODEL_3D_FORMAT_WRL;
        options.overwriteExistingFiles = true;
        stage.setOptions(options);

        QMap<QString, QSharedPointer<ComponentData>> cachedData;
        for (const QString& componentId : {QStringLiteral("C_MODEL_A"), QStringLiteral("C_MODEL_B")}) {
            auto component = QSharedPointer<ComponentData>::create();
            auto model = QSharedPointer<Model3DData>::create();
            model->setUuid(componentId + QStringLiteral("-uuid"));
            model->setName(QStringLiteral("Shared Model"));
            component->setModel3DData(model);
            component->setModel3DObjRaw(QByteArrayLiteral("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n"));
            cachedData.insert(componentId, component);
        }

        QSignalSpy completedSpy(&stage, &ExportTypeStage::completed);
        stage.start({QStringLiteral("C_MODEL_A"), QStringLiteral("C_MODEL_B")}, cachedData);
        QVERIFY2(completedSpy.wait(3000), "Duplicate model name export should complete");
        QCOMPARE(completedSpy.count(), 1);
        QCOMPARE(completedSpy.at(0).at(0).toInt(), 2);
        QCOMPARE(completedSpy.at(0).at(1).toInt(), 0);
        QVERIFY(QFile::exists(tempDir.filePath(QStringLiteral("DuplicateModels.3dmodels/Shared Model.wrl"))));
        QVERIFY(QFile::exists(tempDir.filePath(QStringLiteral("DuplicateModels.3dmodels/Shared Model_2.wrl"))));
    }

    // 验证禁止覆盖时不会改写已有三维模型文件。
    void model3DNoOverwritePreservesExistingFile() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString outputDir = tempDir.filePath(QStringLiteral("NoOverwriteModels.3dmodels"));
        QVERIFY(QDir().mkpath(outputDir));
        const QString existingPath = QDir(outputDir).filePath(QStringLiteral("Existing Model.wrl"));
        QFile existing(existingPath);
        QVERIFY(existing.open(QIODevice::WriteOnly));
        const QByteArray originalData = QByteArrayLiteral("original model\n");
        QVERIFY(existing.write(originalData) == originalData.size());
        existing.close();

        Model3DExportStage stage;
        ExportOptions options;
        options.outputPath = tempDir.path();
        options.libName = QStringLiteral("NoOverwriteModels");
        options.exportModel3DFormat = ExportOptions::MODEL_3D_FORMAT_WRL;
        options.overwriteExistingFiles = false;
        stage.setOptions(options);

        auto component = QSharedPointer<ComponentData>::create();
        auto model = QSharedPointer<Model3DData>::create();
        model->setUuid(QStringLiteral("no-overwrite-model"));
        model->setName(QStringLiteral("Existing Model"));
        component->setModel3DData(model);
        component->setModel3DObjRaw(QByteArrayLiteral("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n"));

        QMap<QString, QSharedPointer<ComponentData>> cachedData;
        cachedData.insert(QStringLiteral("C_NO_OVERWRITE"), component);
        QSignalSpy completedSpy(&stage, &ExportTypeStage::completed);
        stage.start({QStringLiteral("C_NO_OVERWRITE")}, cachedData);
        if (completedSpy.count() == 0)
            QVERIFY2(completedSpy.wait(3000), "No-overwrite model export should complete");
        QCOMPARE(completedSpy.count(), 1);
        QCOMPARE(completedSpy.at(0).at(0).toInt(), 0);
        QCOMPARE(completedSpy.at(0).at(1).toInt(), 0);
        QCOMPARE(completedSpy.at(0).at(2).toInt(), 1);

        QFile result(existingPath);
        QVERIFY(result.open(QIODevice::ReadOnly));
        QCOMPARE(result.readAll(), originalData);
    }

    // 验证运行中的阶段会拒绝重复启动请求。
    void duplicateStartWhileRunningIsIgnored() {
        DeferredStage stage;
        QSignalSpy completedSpy(&stage, &ExportTypeStage::completed);

        QMap<QString, QSharedPointer<ComponentData>> cachedData;
        cachedData[QStringLiteral("C8001")] = QSharedPointer<ComponentData>::create();
        cachedData[QStringLiteral("C8002")] = QSharedPointer<ComponentData>::create();

        stage.start({QStringLiteral("C8001")}, cachedData);
        QVERIFY(stage.isRunning());

        // 运行中再次 start 应被忽略
        stage.start({QStringLiteral("C8002")}, cachedData);

        QVERIFY2(completedSpy.wait(3000), "First start should complete");
        QCOMPARE(completedSpy.count(), 1);  // 只有 1 次 completed

        const ExportTypeProgress progress = stage.getProgress();
        QCOMPARE(progress.totalCount, 1);  // 只有 C8001，C8002 被忽略
    }

    // 验证 Worker 失败时会递增失败计数并保留错误信息。
    void failedComponentIncrementsFailedCount() {
        FailStage stage;
        QSignalSpy completedSpy(&stage, &ExportTypeStage::completed);
        QSignalSpy itemSpy(&stage, &ExportTypeStage::itemStatusChanged);

        QStringList ids = {"C9001", "C9002", "C9003"};
        QMap<QString, QSharedPointer<ComponentData>> cachedData;
        for (const QString& id : ids) {
            cachedData[id] = QSharedPointer<ComponentData>::create();
        }

        stage.start(ids, cachedData);

        QCOMPARE(completedSpy.count(), 1);
        QCOMPARE(completedSpy.at(0).at(0).toInt(), 0);  // successCount
        QCOMPARE(completedSpy.at(0).at(1).toInt(), 3);  // failedCount

        const ExportTypeProgress progress = stage.getProgress();
        QCOMPARE(progress.totalCount, 3);
        QCOMPARE(progress.completedCount, 3);
        QCOMPARE(progress.successCount, 0);
        QCOMPARE(progress.failedCount, 3);

        // 验证每个失败项都有错误信息
        int failedSignals = 0;
        for (int i = 0; i < itemSpy.count(); i++) {
            const auto status = qvariant_cast<ExportItemStatus>(itemSpy.at(i).at(1));
            if (status.status == ExportItemStatus::Status::Failed) {
                QVERIFY(!status.errorMessage.isEmpty());
                failedSignals++;
            }
        }
        QCOMPARE(failedSignals, 3);
    }

    // 验证线程池并发数限制会约束同时运行的 Worker 数量。
    void concurrencyLimitIsRespected() {
        ConcurrentStage stage(2);
        QSignalSpy completedSpy(&stage, &ExportTypeStage::completed);

        QStringList ids = {"CA001", "CA002", "CA003", "CA004", "CA005"};
        QMap<QString, QSharedPointer<ComponentData>> cachedData;
        for (const QString& id : ids) {
            cachedData[id] = QSharedPointer<ComponentData>::create();
        }

        stage.start(ids, cachedData);

        QVERIFY2(completedSpy.wait(5000), "All components should complete");
        QCOMPARE(completedSpy.at(0).at(0).toInt(), 5);  // all success

        // 并发限制为 2，任何时候不应超过 2 个 worker 同时运行
        QVERIFY2(stage.maxStartedWorkers() <= 2,
                 qPrintable(QString("Max concurrent workers was %1, expected <= 2").arg(stage.maxStartedWorkers())));
    }

    // 验证取消请求会等待已经启动的 Worker 收敛完成。
    void cancelStopsAfterActiveWorkersDrain() {
        DeferredStage stage;
        QSignalSpy completedSpy(&stage, &ExportTypeStage::completed);

        QStringList ids = {"CB001", "CB002", "CB003", "CB004", "CB005"};
        QMap<QString, QSharedPointer<ComponentData>> cachedData;
        for (const QString& id : ids) {
            cachedData[id] = QSharedPointer<ComponentData>::create();
        }

        stage.start(ids, cachedData);
        stage.cancel();

        QVERIFY2(completedSpy.wait(3000), "Cancelled stage should complete");
        QVERIFY(!stage.isRunning());

        // cancel() 清空队列，仅活跃 worker（1个）会完成
        const auto args = completedSpy.at(0);
        int success = args.at(0).toInt();
        int failed = args.at(1).toInt();
        QCOMPARE(success, 0);
        QCOMPARE(failed, 1);  // 仅活跃的 1 个 worker 标记为 failed

        // pending 队列被清空，completedCount 仅反映实际处理的
        const ExportTypeProgress progress = stage.getProgress();
        QCOMPARE(progress.completedCount, 1);
    }

private:
    static QSharedPointer<ComponentData> makeSymbolComponent(const QString& componentId, const QString& symbolName) {
        auto componentData = QSharedPointer<ComponentData>::create();
        componentData->setLcscId(componentId);

        auto symbolData = QSharedPointer<SymbolData>::create();
        SymbolInfo info;
        info.name = symbolName;
        info.prefix = QStringLiteral("U");
        info.package = QStringLiteral("PKG_%1").arg(componentId);
        symbolData->setInfo(info);
        symbolData->setBbox({0, 0, 0, 0});

        componentData->setSymbolData(symbolData);
        return componentData;
    }

    static QSharedPointer<ComponentData> makeFootprintComponent(const QString& componentId,
                                                                const QString& footprintName,
                                                                const QString& model3DName = QString()) {
        auto componentData = QSharedPointer<ComponentData>::create();
        componentData->setLcscId(componentId);

        auto footprintData = QSharedPointer<FootprintData>::create();
        FootprintInfo info;
        info.name = footprintName;
        info.type = QStringLiteral("smd");
        footprintData->setInfo(info);

        FootprintBBox bbox;
        bbox.x = 0;
        bbox.y = 0;
        bbox.width = 1;
        bbox.height = 1;
        footprintData->setBbox(bbox);

        if (!model3DName.isEmpty()) {
            Model3DData model3D;
            model3D.setName(model3DName);
            model3D.setUuid(QStringLiteral("uuid-%1").arg(componentId));
            footprintData->setModel3D(model3D);
        }

        componentData->setFootprintData(footprintData);
        return componentData;
    }

    // 构造同时具备符号和封装的最小组件，供组合库阶段测试使用。
    static QSharedPointer<ComponentData> makeCombinedLibraryComponent(const QString& componentId,
                                                                      const QString& footprintName) {
        auto componentData = makeSymbolComponent(componentId, QStringLiteral("CADSTAR_SYMBOL"));
        componentData->setName(componentId);
        componentData->setPackage(footprintName);
        auto symbolData = componentData->symbolData();
        SymbolInfo symbolInfo = symbolData->info();
        symbolInfo.package = footprintName;
        symbolData->setInfo(symbolInfo);
        SymbolPin symbolPin;
        symbolPin.settings.spicePinNumber = QStringLiteral("1");
        symbolPin.settings.posX = 0.0;
        symbolPin.settings.posY = 0.0;
        symbolPin.name.text = QStringLiteral("PIN1");
        symbolPin.name.isDisplayed = true;
        symbolData->addPin(symbolPin);
        auto footprintData = QSharedPointer<FootprintData>::create();
        FootprintInfo info;
        info.name = footprintName;
        info.type = QStringLiteral("smd");
        footprintData->setInfo(info);
        FootprintBBox bbox;
        bbox.x = 0;
        bbox.y = 0;
        bbox.width = 1;
        bbox.height = 1;
        footprintData->setBbox(bbox);
        FootprintPad pad{};
        pad.shape = QStringLiteral("RECT");
        pad.centerX = 0.0;
        pad.centerY = 0.0;
        pad.width = 1.0;
        pad.height = 1.0;
        pad.layerId = 1;
        pad.number = QStringLiteral("1");
        pad.isPlated = true;
        footprintData->addPad(pad);
        componentData->setFootprintData(footprintData);
        return componentData;
    }
};

}  // namespace EasyKiConverter

QTEST_MAIN(EasyKiConverter::TestExportTypeStage)
#include "test_export_type_stage.moc"
