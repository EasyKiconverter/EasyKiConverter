#include "core/ExporterFactory.h"
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
#include <QJsonDocument>
#include <QJsonObject>
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
    /**
     * @brief 创建用于验证并发限制的测试阶段。
     * @param maxConcurrent 允许同时运行的最大 Worker 数量。
     */
    explicit ConcurrentStage(int maxConcurrent) : ExportTypeStage("Concurrent", maxConcurrent, nullptr) {}

    /**
     * @brief 返回测试阶段观察到的最大并发 Worker 数。
     */
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
    // m_startedWorkers：测试使用的数值结果。
    int m_startedWorkers = 0;
    // m_maxStartedWorkers：测试使用的数值结果。
    int m_maxStartedWorkers = 0;
};

class TestExportTypeStage : public QObject {
    Q_OBJECT

private slots:

    /**
     * @brief 验证所有已注册目标格式都能创建独立三维模型导出器。
     */
    void model3DExporterIsAvailableForAllTargets();

    /**
     * @brief 验证目标格式的符号、封装和三维导出入口与实际能力边界一致。
     */
    void exporterFactoryMatchesLibraryArtifactCapabilities();

    /**
     * @brief 验证组合库目标也能通过通用工厂创建符号导出器。
     */
    void combinedLibrarySymbolExportersAreAvailable();

    /**
     * @brief 验证预览图缓存缺少前置索引时仍会导出后续图片。
     */
    void previewImageExportLoadsNonContiguousCacheEntries() {
        // outputDir：测试使用的文件系统路径。
        QTemporaryDir outputDir;
        // cacheDir：测试使用的文件系统路径。
        QTemporaryDir cacheDir;
        QVERIFY(outputDir.isValid());
        QVERIFY(cacheDir.isValid());

        // cache：测试使用的缓存或元器件对象。
        ComponentCacheService* cache = ComponentCacheService::instance();
        // previousCacheDir：测试使用的文件系统路径。
        const QString previousCacheDir = cache->cacheDir();
        cache->setCacheDir(cacheDir.path());
        cache->clearAllCache();

        // metadata：测试使用的文本或数据内容。
        ComponentData metadata;
        metadata.setLcscId(QStringLiteral("C45001"));
        metadata.setName(QStringLiteral("Non-contiguous preview fixture"));
        cache->saveComponentMetadata(QStringLiteral("C45001"), metadata);

        QImage image(2, 2, QImage::Format_RGB32);
        image.fill(Qt::green);
        // buffer：测试用缓冲区。
        QBuffer buffer;
        QVERIFY(buffer.open(QIODevice::WriteOnly));
        QVERIFY(image.save(&buffer, "PNG"));
        cache->savePreviewImage(QStringLiteral("C45001"), buffer.data(), 1);

        // component：测试使用的缓存或元器件对象。
        auto component = QSharedPointer<ComponentData>::create();
        component->setLcscId(QStringLiteral("C45001"));
        component->setPreviewImageData({QByteArray(), QByteArray()});

        // options：当前测试使用的导出选项。
        ExportOptions options;
        options.outputPath = outputDir.path();
        options.libName = QStringLiteral("NonContiguousPreview");
        options.overwriteExistingFiles = true;

        // worker：测试用工作对象。
        PreviewImagesExportWorker worker;
        QSignalSpy completedSpy(&worker, &PreviewImagesExportWorker::completed);
        worker.setData(QStringLiteral("C45001"), component, options);
        worker.run();

        QCOMPARE(completedSpy.count(), 1);
        QVERIFY(completedSpy.at(0).at(1).toBool());
        // exportedPath：测试使用的文件系统路径。
        const QString exportedPath = outputDir.path() + QDir::separator() +
                                     QStringLiteral("NonContiguousPreview.preview") + QDir::separator() +
                                     QStringLiteral("C45001_preview_1.png");
        QVERIFY(QFileInfo::exists(exportedPath));

        cache->setCacheDir(previousCacheDir);
    }

    // 验证每个组件都能收到开始和完成状态。
    /**
     * @brief 验证 emitsItemStatusForEveryComponentIncludingLast 测试场景。
     */
    void emitsItemStatusForEveryComponentIncludingLast() {
        // stage：测试用导出阶段。
        ImmediateSuccessStage stage;
        QSignalSpy itemSpy(&stage, &ExportTypeStage::itemStatusChanged);
        QSignalSpy completedSpy(&stage, &ExportTypeStage::completed);

        // ids：测试使用的元器件标识或名称。
        QStringList ids = {"C1001", "C1002"};
        // cachedData：测试使用的文本或数据内容。
        QMap<QString, QSharedPointer<ComponentData>> cachedData;
        for (const QString& id : ids) {
            cachedData[id] = QSharedPointer<ComponentData>::create();
        }

        stage.start(ids, cachedData);

        // Each component emits 2 signals: InProgress (from startNextWorker) and Success (from completeItemProgress)
        QCOMPARE(itemSpy.count(), ids.size() * 2);
        QCOMPARE(completedSpy.count(), 1);

        // progress：测试用导出进度状态。
        const ExportTypeProgress progress = stage.getProgress();
        QCOMPARE(progress.totalCount, ids.size());
        QCOMPARE(progress.completedCount, ids.size());
        QCOMPARE(progress.successCount, ids.size());
        QCOMPARE(progress.failedCount, 0);
    }

    // 验证 completed 信号发出时阶段已经退出运行状态。
    /**
     * @brief 验证 completedSignalSeesStageNotRunning 测试场景。
     */
    void completedSignalSeesStageNotRunning() {
        // stage：测试用导出阶段。
        ImmediateSuccessStage stage;
        // wasRunningWhenCompleted：测试条件是否成立。
        bool wasRunningWhenCompleted = true;
        connect(&stage, &ExportTypeStage::completed, &stage, [&stage, &wasRunningWhenCompleted]() {
            wasRunningWhenCompleted = stage.isRunning();
        });

        // cachedData：测试使用的文本或数据内容。
        QMap<QString, QSharedPointer<ComponentData>> cachedData;
        cachedData[QStringLiteral("C1001")] = QSharedPointer<ComponentData>::create();

        stage.start({QStringLiteral("C1001")}, cachedData);

        QVERIFY(!wasRunningWhenCompleted);
        QVERIFY(!stage.isRunning());
    }

    // 验证取消阶段会等待活跃 Worker 完成后再结束。
    /**
     * @brief 验证 cancelledStageStaysRunningUntilActiveWorkersDrain 测试场景。
     */
    void cancelledStageStaysRunningUntilActiveWorkersDrain() {
        // stage：测试用导出阶段。
        DeferredStage stage;
        QSignalSpy completedSpy(&stage, &ExportTypeStage::completed);

        // ids：测试使用的元器件标识或名称。
        QStringList ids = {"C1001", "C1002", "C1003"};
        // cachedData：测试使用的文本或数据内容。
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
    /**
     * @brief 验证 footprintLibraryExportPreservesExistingFilesWhenNotOverwriting 测试场景。
     */
    void footprintLibraryExportPreservesExistingFilesWhenNotOverwriting() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // libName：测试使用的元器件标识或名称。
        const QString libName = QStringLiteral("RegressionLib");
        // prettyDir：测试使用的文件系统路径。
        const QString prettyDir = tempDir.path() + QDir::separator() + libName + QStringLiteral(".pretty");
        QVERIFY(QDir().mkpath(prettyDir));

        // oldFootprintPath：测试使用的文件系统路径。
        const QString oldFootprintPath = prettyDir + QDir::separator() + QStringLiteral("OldPackage.kicad_mod");
        QFile oldFootprint(oldFootprintPath);
        QVERIFY(oldFootprint.open(QIODevice::WriteOnly | QIODevice::Text));
        oldFootprint.write("(footprint easykiconverter:OldPackage)\n");
        oldFootprint.close();

        // stage：测试用导出阶段。
        FootprintExportStage stage;
        // options：当前测试使用的导出选项。
        ExportOptions options;
        options.outputPath = tempDir.path();
        options.libName = libName;
        options.overwriteExistingFiles = false;
        stage.setOptions(options);

        // componentId：测试使用的元器件标识或名称。
        const QString componentId = QStringLiteral("C2040");
        // cachedData：测试使用的文本或数据内容。
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

    /**
     * @brief 验证 LibrePCB 项目路径会同时安装项目库和 Workspace 本地库。
     */
    void librePcbProjectExportInstallsWorkspaceLibrary() {
        // temporary：测试隔离环境中的临时对象。
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());

        // workspaceRoot：测试使用的文件系统路径。
        const QString workspaceRoot = QDir(temporary.path()).filePath(QStringLiteral("workspace"));
        // projectRoot：测试使用的文件系统路径。
        const QString projectRoot = QDir(workspaceRoot).filePath(QStringLiteral("projects/Example"));
        QVERIFY(QDir().mkpath(QDir(workspaceRoot).filePath(QStringLiteral("data/libraries/local"))));
        QVERIFY(QDir().mkpath(QDir(workspaceRoot).filePath(QStringLiteral("projects"))));
        QVERIFY(QDir().mkpath(projectRoot));

        QFile projectMarker(QDir(projectRoot).filePath(QStringLiteral(".librepcb-project")));
        QVERIFY(projectMarker.open(QIODevice::WriteOnly));
        projectMarker.write("2\n");
        projectMarker.close();

        // stage：测试用导出阶段。
        FootprintExportStage stage;
        // options：当前测试使用的导出选项。
        ExportOptions options;
        options.outputPath = projectRoot;
        options.libName = QStringLiteral("GeneratedParts");
        options.targetFormat = TargetEdaFormat::LibrePcb;
        options.exportSymbol = false;
        options.exportFootprint = true;
        options.overwriteExistingFiles = true;
        stage.setOptions(options);

        // componentId：测试使用的元器件标识或名称。
        const QString componentId = QStringLiteral("C2040");
        // cachedData：测试使用的文本或数据内容。
        QMap<QString, QSharedPointer<ComponentData>> cachedData;
        cachedData[componentId] = makeFootprintComponent(componentId, QStringLiteral("C2040_Package"));

        QSignalSpy completedSpy(&stage, &FootprintExportStage::completed);
        stage.start({componentId}, cachedData);

        QVERIFY2(completedSpy.wait(3000), "LibrePCB project export should complete");
        QCOMPARE(completedSpy.count(), 1);
        QCOMPARE(completedSpy.at(0).at(0).toInt(), 1);
        QCOMPARE(completedSpy.at(0).at(1).toInt(), 0);

        // projectLibrary：项目库描述。
        const QString projectLibrary = QDir(projectRoot).filePath(QStringLiteral("library"));
        const QString workspaceLibrary =
            QDir(workspaceRoot).filePath(QStringLiteral("data/libraries/local/GeneratedParts.lplib"));
        QVERIFY(QFileInfo(projectLibrary).isDir());
        QVERIFY(QFileInfo(workspaceLibrary).isDir());
        QVERIFY(QFileInfo(QDir(projectLibrary).filePath(QStringLiteral("pkg"))).isDir());
        QVERIFY(QFileInfo(QDir(workspaceLibrary).filePath(QStringLiteral(".librepcb-lib"))).isFile());
    }

    // 验证封装导出可以生成绝对路径的三维模型引用。
    /**
     * @brief 验证 footprintLibraryExportCanUseAbsolute3DModelPaths 测试场景。
     */
    void footprintLibraryExportCanUseAbsolute3DModelPaths() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // libName：测试使用的元器件标识或名称。
        const QString libName = QStringLiteral("PathModeLib");
        // componentId：测试使用的元器件标识或名称。
        const QString componentId = QStringLiteral("C3001");
        // footprintName：测试使用的元器件标识或名称。
        const QString footprintName = QStringLiteral("PackageWith3D");
        // modelName：测试使用的元器件标识或名称。
        const QString modelName = QStringLiteral("ModelWith3D");

        // stage：测试用导出阶段。
        FootprintExportStage stage;
        // options：当前测试使用的导出选项。
        ExportOptions options;
        options.outputPath = tempDir.path();
        options.libName = libName;
        options.exportModel3DFormat = ExportOptions::MODEL_3D_FORMAT_WRL;
        options.exportModel3DPathMode = ExportOptions::MODEL_3D_PATH_ABSOLUTE;
        stage.setOptions(options);

        // cachedData：测试使用的文本或数据内容。
        QMap<QString, QSharedPointer<ComponentData>> cachedData;
        cachedData[componentId] = makeFootprintComponent(componentId, footprintName, modelName);

        QSignalSpy completedSpy(&stage, &FootprintExportStage::completed);
        stage.start({componentId}, cachedData);

        QVERIFY2(completedSpy.wait(3000), "Footprint export should complete");
        QCOMPARE(completedSpy.count(), 1);

        // footprintPath：测试使用的文件系统路径。
        const QString footprintPath = tempDir.path() + QDir::separator() + libName + QStringLiteral(".pretty") +
                                      QDir::separator() + footprintName + QStringLiteral(".kicad_mod");
        QFile footprintFile(footprintPath);
        QVERIFY(footprintFile.open(QIODevice::ReadOnly | QIODevice::Text));
        // content：测试使用的文本或数据内容。
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
    /**
     * @brief 验证 footprintLibraryExportUsesRelative3DModelPathsByDefault 测试场景。
     */
    void footprintLibraryExportUsesRelative3DModelPathsByDefault() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // libName：测试使用的元器件标识或名称。
        const QString libName = QStringLiteral("RelPathLib");
        // componentId：测试使用的元器件标识或名称。
        const QString componentId = QStringLiteral("C4001");
        // footprintName：测试使用的元器件标识或名称。
        const QString footprintName = QStringLiteral("RelPackage");
        // modelName：测试使用的元器件标识或名称。
        const QString modelName = QStringLiteral("RelModel");

        // stage：测试用导出阶段。
        FootprintExportStage stage;
        // options：当前测试使用的导出选项。
        ExportOptions options;
        options.outputPath = tempDir.path();
        options.libName = libName;
        options.exportModel3DFormat = ExportOptions::MODEL_3D_FORMAT_WRL;
        options.exportModel3DPathMode = ExportOptions::MODEL_3D_PATH_RELATIVE;
        stage.setOptions(options);

        // cachedData：测试使用的文本或数据内容。
        QMap<QString, QSharedPointer<ComponentData>> cachedData;
        cachedData[componentId] = makeFootprintComponent(componentId, footprintName, modelName);

        QSignalSpy completedSpy(&stage, &FootprintExportStage::completed);
        stage.start({componentId}, cachedData);

        QVERIFY2(completedSpy.wait(3000), "Footprint export should complete");

        // footprintPath：测试使用的文件系统路径。
        const QString footprintPath = tempDir.path() + QDir::separator() + libName + QStringLiteral(".pretty") +
                                      QDir::separator() + footprintName + QStringLiteral(".kicad_mod");
        QFile footprintFile(footprintPath);
        QVERIFY(footprintFile.open(QIODevice::ReadOnly | QIODevice::Text));
        // content：测试使用的文本或数据内容。
        const QString content = QString::fromUtf8(footprintFile.readAll());

        // expectedWrlPath：测试使用的文件系统路径。
        const QString expectedWrlPath = QStringLiteral("../%1.3dmodels/%2.wrl").arg(libName, modelName);
        QVERIFY2(content.contains(QStringLiteral("(model \"%1\"").arg(expectedWrlPath)),
                 "Footprint should contain relative WRL model path");

        // absolutePrefix：几何计算中的坐标或尺寸值。
        const QString absolutePrefix = QDir::cleanPath(tempDir.path());
        QVERIFY2(!content.contains(absolutePrefix), "Footprint should not contain absolute paths in relative mode");
    }

    /**
     * @brief 验证 Altium PcbLib 不会静默覆盖已有库
     */
    void altiumFootprintExportRejectsUnsupportedLibraryMerge() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // libName：测试使用的元器件标识或名称。
        const QString libName = QStringLiteral("ExistingPcbLib");
        // outputPath：测试使用的文件系统路径。
        const QString outputPath = tempDir.path() + QDir::separator() + libName + QStringLiteral(".PcbLib");
        QFile existingFile(outputPath);
        QVERIFY(existingFile.open(QIODevice::WriteOnly));
        // originalData：测试使用的文本或数据内容。
        const QByteArray originalData = QByteArrayLiteral("existing-pcblib");
        QCOMPARE(existingFile.write(originalData), originalData.size());
        existingFile.close();

        // stage：测试用导出阶段。
        FootprintExportStage stage;
        // options：当前测试使用的导出选项。
        ExportOptions options;
        options.outputPath = tempDir.path();
        options.libName = libName;
        options.targetFormat = TargetEdaFormat::Altium;
        options.overwriteExistingFiles = false;
        stage.setOptions(options);

        // cachedData：测试使用的文本或数据内容。
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
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // stage：测试用导出阶段。
        FootprintExportStage stage;
        // options：当前测试使用的导出选项。
        ExportOptions options;
        options.outputPath = tempDir.path();
        options.libName = QStringLiteral("InvalidPcbLib");
        options.targetFormat = TargetEdaFormat::Altium;
        options.overwriteExistingFiles = true;
        stage.setOptions(options);

        // cachedData：测试使用的文本或数据内容。
        QMap<QString, QSharedPointer<ComponentData>> cachedData;
        cachedData[QStringLiteral("C9101")] = makeFootprintComponent(QStringLiteral("C9101"), QStringLiteral("VALID"));
        cachedData[QStringLiteral("C9102")] =
            makeFootprintComponent(QStringLiteral("C9102"), QStringLiteral("INVALID"));
        // invalidInfo：测试使用的元器件标识或名称。
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

        // failedComponents：测试使用的缓存或元器件对象。
        QSet<QString> failedComponents;
        for (const QList<QVariant>& arguments : itemSpy) {
            // status：测试用状态对象。
            const ExportItemStatus status = qvariant_cast<ExportItemStatus>(arguments.at(1));
            if (status.status == ExportItemStatus::Status::Failed)
                failedComponents.insert(arguments.at(0).toString());
        }
        QCOMPARE(failedComponents, QSet<QString>({QStringLiteral("C9101"), QStringLiteral("C9102")}));
    }

    // 验证 Altium 封装阶段优先使用缓存 STEP，并报告嵌入成功。
    /**
     * @brief 验证 altiumFootprintUsesCachedStepModel 测试场景。
     */
    void altiumFootprintUsesCachedStepModel() {
        // outputDir：测试使用的文件系统路径。
        QTemporaryDir outputDir;
        // cacheDir：测试使用的文件系统路径。
        QTemporaryDir cacheDir;
        QVERIFY(outputDir.isValid());
        QVERIFY(cacheDir.isValid());

        // cache：测试使用的缓存或元器件对象。
        ComponentCacheService* cache = ComponentCacheService::instance();
        // previousCacheDir：测试使用的文件系统路径。
        const QString previousCacheDir = cache->cacheDir();
        cache->setCacheDir(cacheDir.path());
        cache->clearAllCache();

        // componentId：测试使用的元器件标识或名称。
        const QString componentId = QStringLiteral("C12345");
        // modelUuid：测试使用的元器件标识或名称。
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

        // component：测试使用的缓存或元器件对象。
        auto component = makeFootprintComponent(componentId, QStringLiteral("CACHED_PKG"), QStringLiteral("CACHED"));
        // model：测试使用的配置值或状态。
        Model3DData model = component->footprintData()->model3D();
        model.setUuid(modelUuid);
        component->footprintData()->setModel3D(model);
        // componentModel：测试使用的配置值或状态。
        auto componentModel = QSharedPointer<Model3DData>::create(model);
        componentModel->setRawObj(QStringLiteral("<html>component error</html>"));
        component->setModel3DData(componentModel);

        // stage：测试用导出阶段。
        FootprintExportStage stage;
        // options：当前测试使用的导出选项。
        ExportOptions options;
        options.outputPath = outputDir.path();
        options.libName = QStringLiteral("CachedAltium");
        options.targetFormat = TargetEdaFormat::Altium;
        options.exportModel3D = true;
        options.exportModel3DFormat = ExportOptions::MODEL_3D_FORMAT_WRL;
        options.overwriteExistingFiles = true;
        stage.setOptions(options);

        // cachedData：测试使用的文本或数据内容。
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
        // modelStatus：测试使用的配置值或状态。
        const ExportItemStatus modelStatus = qvariant_cast<ExportItemStatus>(modelSpy.at(0).at(1));
        QCOMPARE(modelStatus.status, ExportItemStatus::Status::Success);
        QVERIFY(QFileInfo::exists(outputDir.filePath(QStringLiteral("CachedAltium.PcbLib"))));

        cache->setCacheDir(previousCacheDir);
    }

    // 验证 Altium 封装阶段会缓存预加载的 STEP 模型。
    /**
     * @brief 验证 altiumFootprintCachesPreloadedStepModel 测试场景。
     */
    void altiumFootprintCachesPreloadedStepModel() {
        // outputDir：测试使用的文件系统路径。
        QTemporaryDir outputDir;
        // cacheDir：测试使用的文件系统路径。
        QTemporaryDir cacheDir;
        QVERIFY(outputDir.isValid());
        QVERIFY(cacheDir.isValid());

        // cache：测试使用的缓存或元器件对象。
        ComponentCacheService* cache = ComponentCacheService::instance();
        // previousCacheDir：测试使用的文件系统路径。
        const QString previousCacheDir = cache->cacheDir();
        cache->setCacheDir(cacheDir.path());
        cache->clearAllCache();

        // componentId：测试使用的元器件标识或名称。
        const QString componentId = QStringLiteral("C12348");
        // modelUuid：测试使用的元器件标识或名称。
        const QString modelUuid = QStringLiteral("preloaded-altium-step");
        const QByteArray stepData = QByteArrayLiteral("ISO-10303-21;\nDATA;\nENDSEC;\nEND-ISO-10303-21;\n");
        // objData：测试使用的文本或数据内容。
        const QByteArray objData = QByteArrayLiteral("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n");
        auto component =
            makeFootprintComponent(componentId, QStringLiteral("PRELOADED_PKG"), QStringLiteral("PRELOADED"));
        // footprintModel：测试使用的配置值或状态。
        Model3DData footprintModel = component->footprintData()->model3D();
        footprintModel.setUuid(modelUuid);
        component->footprintData()->setModel3D(footprintModel);
        // componentModel：测试使用的配置值或状态。
        auto componentModel = QSharedPointer<Model3DData>::create(footprintModel);
        componentModel->setStep(stepData);
        componentModel->setRawObj(QString::fromUtf8(objData));
        component->setModel3DData(componentModel);
        component->setModel3DObjRaw(objData);

        // stage：测试用导出阶段。
        FootprintExportStage stage;
        // options：当前测试使用的导出选项。
        ExportOptions options;
        options.outputPath = outputDir.path();
        options.libName = QStringLiteral("PreloadedAltium");
        options.targetFormat = TargetEdaFormat::Altium;
        options.exportModel3D = true;
        options.overwriteExistingFiles = true;
        stage.setOptions(options);

        // cachedData：测试使用的文本或数据内容。
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
    /**
     * @brief 验证 model3DWorkerCachesPreloadedObj 测试场景。
     */
    void model3DWorkerCachesPreloadedObj() {
        // outputDir：测试使用的文件系统路径。
        QTemporaryDir outputDir;
        // cacheDir：测试使用的文件系统路径。
        QTemporaryDir cacheDir;
        QVERIFY(outputDir.isValid());
        QVERIFY(cacheDir.isValid());

        // cache：测试使用的缓存或元器件对象。
        ComponentCacheService* cache = ComponentCacheService::instance();
        // previousCacheDir：测试使用的文件系统路径。
        const QString previousCacheDir = cache->cacheDir();
        cache->setCacheDir(cacheDir.path());
        cache->clearAllCache();

        // componentId：测试使用的元器件标识或名称。
        const QString componentId = QStringLiteral("C12346");
        // modelUuid：测试使用的元器件标识或名称。
        const QString modelUuid = QStringLiteral("preloaded-obj-model");
        // objData：测试使用的文本或数据内容。
        const QByteArray objData = QByteArrayLiteral("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n");
        // component：测试使用的缓存或元器件对象。
        auto component = QSharedPointer<ComponentData>::create();
        component->setLcscId(componentId);
        // model：测试使用的配置值或状态。
        auto model = QSharedPointer<Model3DData>::create();
        model->setUuid(modelUuid);
        model->setName(QStringLiteral("PRELOADED"));
        model->setRawObj(QString::fromUtf8(objData));
        component->setModel3DData(model);
        component->setModel3DObjRaw(objData);

        // options：当前测试使用的导出选项。
        ExportOptions options;
        options.outputPath = outputDir.path();
        options.exportModel3D = true;
        options.exportModel3DFormat = ExportOptions::MODEL_3D_FORMAT_WRL;
        options.overwriteExistingFiles = true;

        // worker：测试用工作对象。
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
    /**
     * @brief 验证 model3DWorkerRejectsStaleCacheGeneration 测试场景。
     */
    void model3DWorkerRejectsStaleCacheGeneration() {
        // outputDir：测试使用的文件系统路径。
        QTemporaryDir outputDir;
        // cacheDir：测试使用的文件系统路径。
        QTemporaryDir cacheDir;
        QVERIFY(outputDir.isValid());
        QVERIFY(cacheDir.isValid());

        // cache：测试使用的缓存或元器件对象。
        ComponentCacheService* cache = ComponentCacheService::instance();
        // previousCacheDir：测试使用的文件系统路径。
        const QString previousCacheDir = cache->cacheDir();
        cache->setCacheDir(cacheDir.path());
        cache->clearAllCache();

        // componentId：测试使用的元器件标识或名称。
        const QString componentId = QStringLiteral("C12348");
        // modelUuid：测试使用的元器件标识或名称。
        const QString modelUuid = QStringLiteral("stale-generation-model");
        // objData：测试使用的文本或数据内容。
        const QByteArray objData = QByteArrayLiteral("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n");
        // component：测试使用的缓存或元器件对象。
        auto component = QSharedPointer<ComponentData>::create();
        component->setLcscId(componentId);
        // model：测试使用的配置值或状态。
        auto model = QSharedPointer<Model3DData>::create();
        model->setUuid(modelUuid);
        model->setName(QStringLiteral("STALE_GENERATION"));
        model->setRawObj(QString::fromUtf8(objData));
        component->setModel3DData(model);
        component->setModel3DObjRaw(objData);

        // options：当前测试使用的导出选项。
        ExportOptions options;
        options.outputPath = outputDir.path();
        options.exportModel3D = true;
        options.exportModel3DFormat = ExportOptions::MODEL_3D_FORMAT_WRL;
        options.overwriteExistingFiles = true;

        // worker：测试用工作对象。
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
    /**
     * @brief 验证 model3DWorkerCachesPreloadedStep 测试场景。
     */
    void model3DWorkerCachesPreloadedStep() {
        // outputDir：测试使用的文件系统路径。
        QTemporaryDir outputDir;
        // cacheDir：测试使用的文件系统路径。
        QTemporaryDir cacheDir;
        QVERIFY(outputDir.isValid());
        QVERIFY(cacheDir.isValid());

        // cache：测试使用的缓存或元器件对象。
        ComponentCacheService* cache = ComponentCacheService::instance();
        // previousCacheDir：测试使用的文件系统路径。
        const QString previousCacheDir = cache->cacheDir();
        cache->setCacheDir(cacheDir.path());
        cache->clearAllCache();

        // componentId：测试使用的元器件标识或名称。
        const QString componentId = QStringLiteral("C12347");
        // modelUuid：测试使用的元器件标识或名称。
        const QString modelUuid = QStringLiteral("preloaded-step-model");
        const QByteArray stepData = QByteArrayLiteral("ISO-10303-21;\nDATA;\nENDSEC;\nEND-ISO-10303-21;\n");
        // component：测试使用的缓存或元器件对象。
        auto component = QSharedPointer<ComponentData>::create();
        component->setLcscId(componentId);
        // model：测试使用的配置值或状态。
        auto model = QSharedPointer<Model3DData>::create();
        model->setUuid(modelUuid);
        model->setName(QStringLiteral("PRELOADED_STEP"));
        model->setStep(stepData);
        component->setModel3DData(model);

        // options：当前测试使用的导出选项。
        ExportOptions options;
        options.outputPath = outputDir.path();
        options.exportModel3D = true;
        options.exportModel3DFormat = ExportOptions::MODEL_3D_FORMAT_STEP;
        options.overwriteExistingFiles = true;

        // worker：测试用工作对象。
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
    /**
     * @brief 验证 altiumMissingFootprintReportsEmbeddedModelFailure 测试场景。
     */
    void altiumMissingFootprintReportsEmbeddedModelFailure() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // stage：测试用导出阶段。
        FootprintExportStage stage;
        // options：当前测试使用的导出选项。
        ExportOptions options;
        options.outputPath = tempDir.path();
        options.targetFormat = TargetEdaFormat::Altium;
        options.exportModel3D = true;
        stage.setOptions(options);

        // componentId：测试使用的元器件标识或名称。
        const QString componentId = QStringLiteral("C_MISSING_FOOTPRINT");
        // cachedData：测试使用的文本或数据内容。
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
        // modelStatus：测试使用的配置值或状态。
        const ExportItemStatus modelStatus = qvariant_cast<ExportItemStatus>(modelSpy.at(0).at(1));
        QCOMPARE(modelStatus.status, ExportItemStatus::Status::Failed);
        QCOMPARE(modelStatus.errorMessage, QStringLiteral("No footprint data"));
    }

    // 验证多个符号可以合并写入同一个符号库。
    /**
     * @brief 验证 symbolLibraryExportMergesMultipleComponentsIntoOneLibrary 测试场景。
     */
    void symbolLibraryExportMergesMultipleComponentsIntoOneLibrary() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // stage：测试用导出阶段。
        SymbolExportStage stage;
        // options：当前测试使用的导出选项。
        ExportOptions options;
        options.outputPath = tempDir.path();
        options.libName = QStringLiteral("MergedSymbols");
        options.overwriteExistingFiles = true;
        stage.setOptions(options);

        // cachedData：测试使用的文本或数据内容。
        QMap<QString, QSharedPointer<ComponentData>> cachedData;
        cachedData[QStringLiteral("C5001")] = makeSymbolComponent(QStringLiteral("C5001"), QStringLiteral("SYM_A"));
        cachedData[QStringLiteral("C5002")] = makeSymbolComponent(QStringLiteral("C5002"), QStringLiteral("SYM_B"));

        QSignalSpy completedSpy(&stage, &SymbolExportStage::completed);
        stage.start({QStringLiteral("C5001"), QStringLiteral("C5002")}, cachedData);

        QVERIFY2(completedSpy.wait(3000), "Symbol export should complete");
        QCOMPARE(completedSpy.count(), 1);
        QCOMPARE(completedSpy.at(0).at(0).toInt(), 2);
        QCOMPARE(completedSpy.at(0).at(1).toInt(), 0);

        // symbolPath：测试使用的文件系统路径。
        const QString symbolPath = tempDir.path() + QDir::separator() + QStringLiteral("MergedSymbols.kicad_sym");
        QFile symbolFile(symbolPath);
        QVERIFY(symbolFile.open(QIODevice::ReadOnly | QIODevice::Text));
        // content：测试使用的文本或数据内容。
        const QString content = QString::fromUtf8(symbolFile.readAll());
        QVERIFY(content.contains(QStringLiteral("(symbol \"SYM_A\"")));
        QVERIFY(content.contains(QStringLiteral("(symbol \"SYM_B\"")));
        QVERIFY(!QDir(tempDir.path() + QDir::separator() + QStringLiteral(".tmp")).exists());
    }

    // 验证 PADS 符号图形和 Part Type 关联文件随同一导出事务提交。
    /**
     * @brief 验证 padsSymbolLibraryCommitsCompanionPartType 测试场景。
     */
    void padsSymbolLibraryCommitsCompanionPartType() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // stage：测试用导出阶段。
        SymbolExportStage stage;
        // options：当前测试使用的导出选项。
        ExportOptions options;
        options.outputPath = tempDir.path();
        options.libName = QStringLiteral("PadsSymbols");
        options.targetFormat = TargetEdaFormat::Pads;
        options.overwriteExistingFiles = true;
        stage.setOptions(options);

        // cachedData：测试使用的文本或数据内容。
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
        // partTypeContent：测试使用的元器件标识或名称。
        const QString partTypeContent = QString::fromUtf8(partTypeFile.readAll());
        QVERIFY(partTypeContent.contains(QStringLiteral("PADS_SYMBOL PKG_C_PADS_SYMBOL")));
    }

    // 验证首次创建 PADS 符号库时不会把“不覆盖”误判为追加模式。
    /**
     * @brief 验证 padsSymbolLibraryCreatesNewFileWithNoOverwrite 测试场景。
     */
    void padsSymbolLibraryCreatesNewFileWithNoOverwrite() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // stage：测试用导出阶段。
        SymbolExportStage stage;
        // options：当前测试使用的导出选项。
        ExportOptions options;
        options.outputPath = tempDir.path();
        options.libName = QStringLiteral("PadsFirstExport");
        options.targetFormat = TargetEdaFormat::Pads;
        options.overwriteExistingFiles = false;
        stage.setOptions(options);

        // cachedData：测试使用的文本或数据内容。
        QMap<QString, QSharedPointer<ComponentData>> cachedData;
        cachedData[QStringLiteral("C_PADS_FIRST")] =
            makeSymbolComponent(QStringLiteral("C_PADS_FIRST"), QStringLiteral("PADS_FIRST_SYMBOL"));

        QSignalSpy completedSpy(&stage, &SymbolExportStage::completed);
        stage.start({QStringLiteral("C_PADS_FIRST")}, cachedData);
        QVERIFY2(completedSpy.wait(3000), "PADS first symbol export should complete");
        QCOMPARE(completedSpy.count(), 1);
        QCOMPARE(completedSpy.at(0).at(0).toInt(), 1);
        QCOMPARE(completedSpy.at(0).at(1).toInt(), 0);
        QVERIFY(QFileInfo::exists(tempDir.filePath(QStringLiteral("PadsFirstExport_PADS.c"))));
        QVERIFY(QFileInfo::exists(tempDir.filePath(QStringLiteral("PadsFirstExport_PADS.p"))));
    }

    // 验证主符号文件缺失但伴随 Part Type 文件存在时不会覆盖旧文件。
    /**
     * @brief 验证 padsSymbolLibraryRejectsOrphanCompanionWithoutOverwrite 测试场景。
     */
    void padsSymbolLibraryRejectsOrphanCompanionWithoutOverwrite() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // companionPath：测试使用的文件系统路径。
        const QString companionPath = tempDir.filePath(QStringLiteral("PadsOrphan_PADS.p"));
        QFile existing(companionPath);
        QVERIFY(existing.open(QIODevice::WriteOnly));
        existing.write("keep-orphan-part-type");
        existing.close();

        // stage：测试用导出阶段。
        SymbolExportStage stage;
        // options：当前测试使用的导出选项。
        ExportOptions options;
        options.outputPath = tempDir.path();
        options.libName = QStringLiteral("PadsOrphan");
        options.targetFormat = TargetEdaFormat::Pads;
        options.overwriteExistingFiles = false;
        stage.setOptions(options);

        // cachedData：测试使用的文本或数据内容。
        QMap<QString, QSharedPointer<ComponentData>> cachedData;
        cachedData[QStringLiteral("C_PADS_ORPHAN")] =
            makeSymbolComponent(QStringLiteral("C_PADS_ORPHAN"), QStringLiteral("PADS_ORPHAN_SYMBOL"));

        QSignalSpy completedSpy(&stage, &SymbolExportStage::completed);
        stage.start({QStringLiteral("C_PADS_ORPHAN")}, cachedData);
        QVERIFY2(completedSpy.wait(3000), "PADS orphan companion policy should complete");
        QCOMPARE(completedSpy.count(), 1);
        QCOMPARE(completedSpy.at(0).at(0).toInt(), 0);
        QCOMPARE(completedSpy.at(0).at(1).toInt(), 1);

        QVERIFY(existing.open(QIODevice::ReadOnly));
        QCOMPARE(existing.readAll(), QByteArray("keep-orphan-part-type"));
    }

    // 验证 Eagle 组合库阶段同时提交 Symbol、Package、DeviceSet 和引脚焊盘关联。
    /**
     * @brief 验证 eagleCombinedLibraryStageWritesAllLibrarySections 测试场景。
     */
    void eagleCombinedLibraryStageWritesAllLibrarySections() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // stage：测试用导出阶段。
        FootprintExportStage stage;
        // options：当前测试使用的导出选项。
        ExportOptions options;
        options.outputPath = tempDir.path();
        options.libName = QStringLiteral("EagleCombined");
        options.targetFormat = TargetEdaFormat::Eagle;
        options.exportSymbol = true;
        options.exportFootprint = true;
        options.overwriteExistingFiles = true;
        stage.setOptions(options);

        // cachedData：测试使用的文本或数据内容。
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
        // content：测试使用的文本或数据内容。
        const QString content = QString::fromUtf8(output.readAll());
        QVERIFY(content.contains(QStringLiteral("<symbols>")));
        QVERIFY(content.contains(QStringLiteral("<packages>")));
        QVERIFY(content.contains(QStringLiteral("<devicesets>")));
        QVERIFY(content.contains(QStringLiteral("package=\"EAGLE_PACKAGE\"")));
        QVERIFY(content.contains(QStringLiteral("pin=\"PIN1\"")));
        QVERIFY(content.contains(QStringLiteral("pad=\"1\"")));
    }

    // 验证 Eagle 仅符号输出不要求封装缓存，也不生成虚假的器件关联。
    /**
     * @brief 验证 eagleSymbolOnlyLibraryStageWritesSymbols 测试场景。
     */
    void eagleSymbolOnlyLibraryStageWritesSymbols() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // stage：测试用导出阶段。
        FootprintExportStage stage;
        // options：当前测试使用的导出选项。
        ExportOptions options;
        options.outputPath = tempDir.path();
        options.libName = QStringLiteral("EagleSymbolsOnly");
        options.targetFormat = TargetEdaFormat::Eagle;
        options.exportSymbol = true;
        options.exportFootprint = false;
        options.overwriteExistingFiles = true;
        stage.setOptions(options);

        // cachedData：测试使用的文本或数据内容。
        QMap<QString, QSharedPointer<ComponentData>> cachedData;
        cachedData[QStringLiteral("C_EAGLE_SYMBOL")] =
            makeSymbolComponent(QStringLiteral("C_EAGLE_SYMBOL"), QStringLiteral("EAGLE_SYMBOL"));
        QSignalSpy completedSpy(&stage, &FootprintExportStage::completed);
        stage.start({QStringLiteral("C_EAGLE_SYMBOL")}, cachedData);
        QVERIFY2(completedSpy.wait(3000), "Eagle symbol-only export should complete");
        QCOMPARE(completedSpy.at(0).at(0).toInt(), 1);
        QCOMPARE(completedSpy.at(0).at(1).toInt(), 0);

        QFile output(tempDir.filePath(QStringLiteral("EagleSymbolsOnly.lbr")));
        QVERIFY(output.open(QIODevice::ReadOnly | QIODevice::Text));
        // content：测试使用的文本或数据内容。
        const QString content = QString::fromUtf8(output.readAll());
        QVERIFY(content.contains(QStringLiteral("<symbols>")));
        QVERIFY(content.contains(QStringLiteral("name=\"EAGLE_SYMBOL\"")));
        QVERIFY(!content.contains(QStringLiteral("<packages>")));
        QVERIFY(!content.contains(QStringLiteral("<devicesets>")));
    }

    // 验证 CADSTAR 组合库阶段同时提交 Component、Package、Pad 和 Part 关联。
    /**
     * @brief 验证 cadstarCombinedLibraryStageWritesAllLibrarySections 测试场景。
     */
    void cadstarCombinedLibraryStageWritesAllLibrarySections() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // stage：测试用导出阶段。
        FootprintExportStage stage;
        // options：当前测试使用的导出选项。
        ExportOptions options;
        options.outputPath = tempDir.path();
        options.libName = QStringLiteral("CadstarCombined");
        options.targetFormat = TargetEdaFormat::Cadstar;
        options.exportSymbol = true;
        options.exportFootprint = true;
        options.overwriteExistingFiles = true;
        stage.setOptions(options);

        // cachedData：测试使用的文本或数据内容。
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
        // content：测试使用的文本或数据内容。
        const QString content = QString::fromUtf8(output.readAll());
        QVERIFY(content.contains(QStringLiteral("COMPONENT \"CADSTAR_SYMBOL\"")));
        QVERIFY(content.contains(QStringLiteral("PACKAGE \"CADSTAR_PACKAGE\"")));
        QVERIFY(content.contains(QStringLiteral("PAD \"CADSTAR_PACKAGE_PAD_1\"")));
        QVERIFY(content.contains(QStringLiteral("PART \"C_CADSTAR_COMBINED\"")));
    }

    // 验证 CADSTAR 仅符号输出不要求封装缓存，也不生成虚假的 Part 关联。
    /**
     * @brief 验证 cadstarSymbolOnlyLibraryStageWritesSymbols 测试场景。
     */
    void cadstarSymbolOnlyLibraryStageWritesSymbols() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // stage：测试用导出阶段。
        FootprintExportStage stage;
        // options：当前测试使用的导出选项。
        ExportOptions options;
        options.outputPath = tempDir.path();
        options.libName = QStringLiteral("CadstarSymbolsOnly");
        options.targetFormat = TargetEdaFormat::Cadstar;
        options.exportSymbol = true;
        options.exportFootprint = false;
        options.overwriteExistingFiles = true;
        stage.setOptions(options);

        // cachedData：测试使用的文本或数据内容。
        QMap<QString, QSharedPointer<ComponentData>> cachedData;
        cachedData[QStringLiteral("C_CADSTAR_SYMBOL")] =
            makeSymbolComponent(QStringLiteral("C_CADSTAR_SYMBOL"), QStringLiteral("CADSTAR_SYMBOL_ONLY"));
        QSignalSpy completedSpy(&stage, &FootprintExportStage::completed);
        stage.start({QStringLiteral("C_CADSTAR_SYMBOL")}, cachedData);
        QVERIFY2(completedSpy.wait(3000), "CADSTAR symbol-only export should complete");
        QCOMPARE(completedSpy.at(0).at(0).toInt(), 1);
        QCOMPARE(completedSpy.at(0).at(1).toInt(), 0);

        QFile output(tempDir.filePath(QStringLiteral("CadstarSymbolsOnly.lib")));
        QVERIFY(output.open(QIODevice::ReadOnly | QIODevice::Text));
        // content：测试使用的文本或数据内容。
        const QByteArray content = output.readAll();
        QVERIFY(content.contains("COMPONENT \"CADSTAR_SYMBOL_ONLY\""));
        QVERIFY(!content.contains("PACKAGE "));
        QVERIFY(!content.contains("PART "));
    }

    // 验证 Xpedition 符号库不会在禁止覆盖时改写已有 ZIP。
    /**
     * @brief 验证 xpeditionSymbolRejectsNoOverwriteAndUpdateModes 测试场景。
     */
    void xpeditionSymbolRejectsNoOverwriteAndUpdateModes() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        // finalPath：测试使用的文件系统路径。
        const QString finalPath = tempDir.path() + QDir::separator() + QStringLiteral("Existing_Symbols.zip");
        QFile existing(finalPath);
        QVERIFY(existing.open(QIODevice::WriteOnly));
        existing.write("original-xpedition-symbol");
        existing.close();

        for (const auto mode : {0, 1, 2}) {
            // stage：测试用导出阶段。
            SymbolExportStage stage;
            // options：当前测试使用的导出选项。
            ExportOptions options;
            options.outputPath = tempDir.path();
            options.libName = QStringLiteral("Existing");
            options.targetFormat = TargetEdaFormat::Xpedition;
            options.overwriteExistingFiles = mode == 2;
            options.updateMode = mode == 1;
            options.retryMode = mode == 2;
            stage.setOptions(options);

            // cachedData：测试使用的文本或数据内容。
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
    /**
     * @brief 验证 xpeditionFootprintRejectsNoOverwriteAndUpdateModes 测试场景。
     */
    void xpeditionFootprintRejectsNoOverwriteAndUpdateModes() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        // finalPath：测试使用的文件系统路径。
        const QString finalPath = tempDir.path() + QDir::separator() + QStringLiteral("Existing_Footprints.zip");
        QFile existing(finalPath);
        QVERIFY(existing.open(QIODevice::WriteOnly));
        existing.write("original-xpedition-footprint");
        existing.close();

        for (const auto mode : {0, 1, 2}) {
            // stage：测试用导出阶段。
            FootprintExportStage stage;
            // options：当前测试使用的导出选项。
            ExportOptions options;
            options.outputPath = tempDir.path();
            options.libName = QStringLiteral("Existing");
            options.targetFormat = TargetEdaFormat::Xpedition;
            options.overwriteExistingFiles = mode == 2;
            options.updateMode = mode == 1;
            options.retryMode = mode == 2;
            stage.setOptions(options);

            // cachedData：测试使用的文本或数据内容。
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
    /**
     * @brief 验证 padsFootprintRejectsExistingDirectoryWithoutOverwrite 测试场景。
     */
    void padsFootprintRejectsExistingDirectoryWithoutOverwrite() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        // finalPath：测试使用的文件系统路径。
        const QString finalPath = tempDir.path() + QDir::separator() + QStringLiteral("Existing_PADS");
        QVERIFY(QDir().mkpath(finalPath));
        QFile marker(finalPath + QDir::separator() + QStringLiteral("keep.txt"));
        QVERIFY(marker.open(QIODevice::WriteOnly));
        marker.write("keep");
        marker.close();

        // stage：测试用导出阶段。
        FootprintExportStage stage;
        // options：当前测试使用的导出选项。
        ExportOptions options;
        options.outputPath = tempDir.path();
        options.libName = QStringLiteral("Existing");
        options.targetFormat = TargetEdaFormat::Pads;
        options.overwriteExistingFiles = false;
        stage.setOptions(options);

        // cachedData：测试使用的文本或数据内容。
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
    /**
     * @brief 验证 pcadFootprintRejectsExistingFileWithoutOverwrite 测试场景。
     */
    void pcadFootprintRejectsExistingFileWithoutOverwrite() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        // finalPath：测试使用的文件系统路径。
        const QString finalPath = tempDir.path() + QDir::separator() + QStringLiteral("Existing.lia");
        QFile existing(finalPath);
        QVERIFY(existing.open(QIODevice::WriteOnly));
        existing.write("keep-pcad");
        existing.close();

        // stage：测试用导出阶段。
        FootprintExportStage stage;
        // options：当前测试使用的导出选项。
        ExportOptions options;
        options.outputPath = tempDir.path();
        options.libName = QStringLiteral("Existing");
        options.targetFormat = TargetEdaFormat::Pcad;
        options.overwriteExistingFiles = false;
        stage.setOptions(options);

        // cachedData：测试使用的文本或数据内容。
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

    // 验证 P-CAD 的符号库和封装库由不同阶段生成，并保留符号到封装的关联。
    /**
     * @brief 验证 pcadLibraryStagesWriteSymbolAndFootprintArtifacts 测试场景。
     */
    void pcadLibraryStagesWriteSymbolAndFootprintArtifacts() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // componentId：测试使用的元器件标识或名称。
        const QString componentId = QStringLiteral("C_PCAD_STAGE");
        // footprintName：测试使用的元器件标识或名称。
        const QString footprintName = QStringLiteral("PCAD_STAGE_FOOTPRINT");
        // cachedData：测试使用的文本或数据内容。
        QMap<QString, QSharedPointer<ComponentData>> cachedData;
        cachedData.insert(componentId, makeCombinedLibraryComponent(componentId, footprintName));

        // symbolStage：测试用符号数据。
        SymbolExportStage symbolStage;
        // symbolOptions：当前测试使用的导出选项。
        ExportOptions symbolOptions;
        symbolOptions.outputPath = tempDir.path();
        symbolOptions.libName = QStringLiteral("PcadStage");
        symbolOptions.targetFormat = TargetEdaFormat::Pcad;
        symbolOptions.overwriteExistingFiles = true;
        symbolStage.setOptions(symbolOptions);

        QSignalSpy symbolCompleted(&symbolStage, &SymbolExportStage::completed);
        symbolStage.start({componentId}, cachedData);
        if (symbolCompleted.count() == 0)
            QVERIFY2(symbolCompleted.wait(3000), "P-CAD symbol export should complete");
        QCOMPARE(symbolCompleted.count(), 1);
        QCOMPARE(symbolCompleted.at(0).at(0).toInt(), 1);
        QCOMPARE(symbolCompleted.at(0).at(1).toInt(), 0);

        QFile symbolFile(tempDir.filePath(QStringLiteral("PcadStage_PCAD_SCH.lia")));
        QVERIFY(symbolFile.open(QIODevice::ReadOnly | QIODevice::Text));
        // symbolContent：测试使用的文本或数据内容。
        const QString symbolContent = QString::fromUtf8(symbolFile.readAll());
        QVERIFY(symbolContent.startsWith(QStringLiteral("(ACCEL_ASCII")));
        QVERIFY(symbolContent.contains(QStringLiteral("(compDef \"CADSTAR_SYMBOL\"")));
        QVERIFY(symbolContent.contains(
            QStringLiteral("(attachedPattern (patternNum 1) (patternName \"PCAD_STAGE_FOOTPRINT\"))")));

        // footprintStage：测试用封装数据。
        FootprintExportStage footprintStage;
        // footprintOptions：当前测试使用的导出选项。
        ExportOptions footprintOptions;
        footprintOptions.outputPath = tempDir.path();
        footprintOptions.libName = QStringLiteral("PcadStage");
        footprintOptions.targetFormat = TargetEdaFormat::Pcad;
        footprintOptions.overwriteExistingFiles = true;
        footprintStage.setOptions(footprintOptions);

        QSignalSpy footprintCompleted(&footprintStage, &FootprintExportStage::completed);
        footprintStage.start({componentId}, cachedData);
        if (footprintCompleted.count() == 0)
            QVERIFY2(footprintCompleted.wait(3000), "P-CAD footprint export should complete");
        QCOMPARE(footprintCompleted.count(), 1);
        QCOMPARE(footprintCompleted.at(0).at(0).toInt(), 1);
        QCOMPARE(footprintCompleted.at(0).at(1).toInt(), 0);

        QFile footprintFile(tempDir.filePath(QStringLiteral("PcadStage.lia")));
        QVERIFY(footprintFile.open(QIODevice::ReadOnly | QIODevice::Text));
        // footprintContent：测试使用的文本或数据内容。
        const QString footprintContent = QString::fromUtf8(footprintFile.readAll());
        QVERIFY(footprintContent.startsWith(QStringLiteral("(ACCEL_ASCII")));
        QVERIFY(footprintContent.contains(QStringLiteral("(patternDef \"PCAD_STAGE_FOOTPRINT\"")));
        QVERIFY(footprintContent.contains(QStringLiteral("(padNum \"1\")")));
    }

    // 验证符号输入诊断会随导出进度暴露给调用方。
    /**
     * @brief 验证 symbolLibraryExportEmitsInputDiagnostics 测试场景。
     */
    void symbolLibraryExportEmitsInputDiagnostics() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // stage：测试用导出阶段。
        SymbolExportStage stage;
        // options：当前测试使用的导出选项。
        ExportOptions options;
        options.outputPath = tempDir.path();
        options.libName = QStringLiteral("DiagnosticSymbols");
        options.overwriteExistingFiles = true;
        stage.setOptions(options);

        // cachedData：测试使用的文本或数据内容。
        QMap<QString, QSharedPointer<ComponentData>> cachedData;
        cachedData[QStringLiteral("C5101")] = makeSymbolComponent(QStringLiteral("C5101"), QStringLiteral("SYM_DIAG"));

        QSignalSpy itemSpy(&stage, &SymbolExportStage::itemStatusChanged);
        QSignalSpy completedSpy(&stage, &SymbolExportStage::completed);
        stage.start({QStringLiteral("C5101")}, cachedData);

        QVERIFY2(completedSpy.wait(3000), "Diagnostic symbol export should complete");
        // foundDiagnostics：测试期间收集的错误或诊断信息。
        bool foundDiagnostics = false;
        for (const QList<QVariant>& arguments : itemSpy) {
            // status：测试用状态对象。
            const ExportItemStatus status = qvariant_cast<ExportItemStatus>(arguments.at(1));
            if (status.status == ExportItemStatus::Status::Success) {
                QVERIFY(!status.diagnostics.isEmpty());
                foundDiagnostics = true;
            }
        }
        QVERIFY(foundDiagnostics);
    }

    // 验证缺少符号数据时会报告失败而不是生成空库。
    /**
     * @brief 验证 symbolLibraryExportReportsMissingSymbolData 测试场景。
     */
    void symbolLibraryExportReportsMissingSymbolData() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // stage：测试用导出阶段。
        SymbolExportStage stage;
        // options：当前测试使用的导出选项。
        ExportOptions options;
        options.outputPath = tempDir.path();
        options.libName = QStringLiteral("MissingSymbol");
        stage.setOptions(options);

        // cachedData：测试使用的文本或数据内容。
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
        // args：测试命令行参数。
        const auto args = itemSpy.takeFirst();
        QCOMPARE(args.at(0).toString(), QStringLiteral("C6001"));
        // status：测试用状态对象。
        const auto status = qvariant_cast<ExportItemStatus>(args.at(1));
        QCOMPARE(status.status, ExportItemStatus::Status::Failed);
        QCOMPARE(status.errorMessage, QStringLiteral("No symbol data"));
    }

    // 验证符号库提交失败时会回滚临时文件。
    /**
     * @brief 验证 symbolLibraryExportRollsBackTempFileOnCommitFailure 测试场景。
     */
    void symbolLibraryExportRollsBackTempFileOnCommitFailure() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // blockerPath：测试使用的文件系统路径。
        const QString blockerPath = tempDir.path() + QDir::separator() + QStringLiteral("blocked");
        QFile blocker(blockerPath);
        QVERIFY(blocker.open(QIODevice::WriteOnly | QIODevice::Text));
        blocker.write("not a directory");
        blocker.close();

        // stage：测试用导出阶段。
        SymbolExportStage stage;
        // options：当前测试使用的导出选项。
        ExportOptions options;
        options.outputPath = blockerPath;
        options.libName = QStringLiteral("RollbackSymbols");
        options.overwriteExistingFiles = true;
        stage.setOptions(options);

        // cachedData：测试使用的文本或数据内容。
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
    /**
     * @brief 验证 emptyListComponentpletesImmediately 测试场景。
     */
    void emptyListComponentpletesImmediately() {
        // stage：测试用导出阶段。
        ImmediateSuccessStage stage;
        QSignalSpy completedSpy(&stage, &ExportTypeStage::completed);

        // cachedData：测试使用的文本或数据内容。
        QMap<QString, QSharedPointer<ComponentData>> cachedData;
        stage.start(QStringList(), cachedData);

        QCOMPARE(completedSpy.count(), 1);
        QCOMPARE(completedSpy.at(0).at(0).toInt(), 0);  // successCount
        QCOMPARE(completedSpy.at(0).at(1).toInt(), 0);  // failedCount
        QCOMPARE(completedSpy.at(0).at(2).toInt(), 0);  // skippedCount
        QVERIFY(!stage.isRunning());
    }

    // 验证没有 3D 模型 UUID 的元器件计入跳过，而不是被统计为成功。
    /**
     * @brief 验证 model3DWithoutUuidIsSkipped 测试场景。
     */
    void model3DWithoutUuidIsSkipped() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // stage：测试用导出阶段。
        Model3DExportStage stage;
        // options：当前测试使用的导出选项。
        ExportOptions options;
        options.outputPath = tempDir.path();
        options.libName = QStringLiteral("SkippedModels");
        options.exportModel3DFormat = ExportOptions::MODEL_3D_FORMAT_WRL;
        stage.setOptions(options);

        // cachedData：测试使用的文本或数据内容。
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

        // progress：测试用导出进度状态。
        const ExportTypeProgress progress = stage.getProgress();
        QCOMPARE(progress.totalCount, 1);
        QCOMPARE(progress.completedCount, 1);
        QCOMPARE(progress.successCount, 0);
        QCOMPARE(progress.skippedCount, 1);
        QCOMPARE(progress.itemStatus.value(QStringLiteral("C_NO_MODEL")).status, ExportItemStatus::Status::Skipped);
        QVERIFY(itemSpy.count() >= 2);
    }

    // 验证输出目录创建失败时，每个元器件都会收敛为失败状态。
    /**
     * @brief 验证 model3DOutputDirectoryFailureIsReportedPerComponent 测试场景。
     */
    void model3DOutputDirectoryFailureIsReportedPerComponent() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // blockingFilePath：测试使用的文件系统路径。
        const QString blockingFilePath = tempDir.filePath(QStringLiteral("output-file"));
        QFile blockingFile(blockingFilePath);
        QVERIFY(blockingFile.open(QIODevice::WriteOnly));
        blockingFile.close();

        // stage：测试用导出阶段。
        Model3DExportStage stage;
        // options：当前测试使用的导出选项。
        ExportOptions options;
        options.outputPath = blockingFilePath;
        options.libName = QStringLiteral("UnwritableModels");
        options.exportModel3DFormat = ExportOptions::MODEL_3D_FORMAT_WRL;
        stage.setOptions(options);

        // componentId：测试使用的元器件标识或名称。
        const QString componentId = QStringLiteral("C_OUTPUT_FAILURE");
        // cachedData：测试使用的文本或数据内容。
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

        // progress：测试用导出进度状态。
        const ExportTypeProgress progress = stage.getProgress();
        QCOMPARE(progress.totalCount, 1);
        QCOMPARE(progress.completedCount, 1);
        QCOMPARE(progress.failedCount, 1);
        QCOMPARE(progress.itemStatus.value(componentId).status, ExportItemStatus::Status::Failed);
    }

    // 验证封装库阶段的失败统计包含预加载缺失和库写入失败的全部元器件。
    /**
     * @brief 验证 footprintLibraryFailureCountsAllComponents 测试场景。
     */
    void footprintLibraryFailureCountsAllComponents() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // blockerPath：测试使用的文件系统路径。
        const QString blockerPath = tempDir.filePath(QStringLiteral("blocked-output"));
        QFile blocker(blockerPath);
        QVERIFY(blocker.open(QIODevice::WriteOnly));
        blocker.close();

        // stage：测试用导出阶段。
        FootprintExportStage stage;
        // options：当前测试使用的导出选项。
        ExportOptions options;
        options.outputPath = blockerPath;
        options.libName = QStringLiteral("BrokenFootprints");
        options.targetFormat = TargetEdaFormat::KiCad;
        stage.setOptions(options);

        // component：测试使用的缓存或元器件对象。
        auto component = QSharedPointer<ComponentData>::create();
        component->setFootprintData(QSharedPointer<FootprintData>::create());
        // cachedData：测试使用的文本或数据内容。
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

        // progress：测试用导出进度状态。
        const ExportTypeProgress progress = stage.getProgress();
        QCOMPARE(progress.totalCount, 2);
        QCOMPARE(progress.completedCount, 2);
        QCOMPARE(progress.successCount, 0);
        QCOMPARE(progress.failedCount, 2);
        QCOMPARE(progress.inProgressCount, 0);
        QVERIFY(progressSpy.count() >= 2);
    }

    // 验证封装数据中的 3D UUID 也能驱动独立三维导出阶段。
    /**
     * @brief 验证 model3DUuidFromFootprintIsExported 测试场景。
     */
    void model3DUuidFromFootprintIsExported() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // stage：测试用导出阶段。
        Model3DExportStage stage;
        // options：当前测试使用的导出选项。
        ExportOptions options;
        options.outputPath = tempDir.path();
        options.libName = QStringLiteral("FootprintModels");
        options.exportModel3DFormat = ExportOptions::MODEL_3D_FORMAT_WRL;
        options.overwriteExistingFiles = true;
        stage.setOptions(options);

        // footprint：测试用封装数据。
        auto footprint = QSharedPointer<FootprintData>::create();
        // model：测试使用的配置值或状态。
        Model3DData model;
        model.setUuid(QStringLiteral("footprint-model-123"));
        model.setName(QStringLiteral("FOOTPRINT_MODEL"));
        footprint->setModel3D(model);
        // component：测试使用的缓存或元器件对象。
        auto component = QSharedPointer<ComponentData>::create();
        component->setModel3DData(QSharedPointer<Model3DData>::create());
        component->setFootprintData(footprint);
        component->setModel3DObjRaw(QByteArrayLiteral("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n"));

        // cachedData：测试使用的文本或数据内容。
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

    // 验证阶段级 BOTH 模式会同时提交 WRL 和 STEP 两种三维模型文件。
    /**
     * @brief 验证 model3DStageExportsWrlAndStepTogether 测试场景。
     */
    void model3DStageExportsWrlAndStepTogether() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // stage：测试用导出阶段。
        Model3DExportStage stage;
        // options：当前测试使用的导出选项。
        ExportOptions options;
        options.outputPath = tempDir.path();
        options.libName = QStringLiteral("BothModels");
        options.exportModel3DFormat = ExportOptions::MODEL_3D_FORMAT_BOTH;
        options.overwriteExistingFiles = true;
        stage.setOptions(options);

        // component：测试使用的缓存或元器件对象。
        auto component = QSharedPointer<ComponentData>::create();
        // model：测试使用的配置值或状态。
        auto model = QSharedPointer<Model3DData>::create();
        model->setUuid(QStringLiteral("both-model-uuid"));
        model->setName(QStringLiteral("Both Model"));
        model->setTranslation({1.0, 2.0, 3.0});
        model->setRotation({10.0, 20.0, 30.0});
        model->setStepOffsetMm({0.1, 0.2, 0.3});
        model->setStep(QByteArrayLiteral("ISO-10303-21;\nDATA;\nENDSEC;\nEND-ISO-10303-21;\n"));
        component->setModel3DData(model);
        component->setModel3DObjRaw(QByteArrayLiteral("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n"));

        // symbol：测试用符号数据。
        auto symbol = QSharedPointer<SymbolData>::create();
        // symbolInfo：测试用符号数据。
        SymbolInfo symbolInfo;
        symbolInfo.name = QStringLiteral("BOTH_SYMBOL");
        symbol->setInfo(symbolInfo);
        component->setSymbolData(symbol);
        // footprint：测试用封装数据。
        auto footprint = QSharedPointer<FootprintData>::create();
        // footprintInfo：测试用封装数据。
        FootprintInfo footprintInfo;
        footprintInfo.name = QStringLiteral("BOTH_FOOTPRINT");
        footprint->setInfo(footprintInfo);
        component->setFootprintData(footprint);

        // cachedData：测试使用的文本或数据内容。
        QMap<QString, QSharedPointer<ComponentData>> cachedData;
        cachedData.insert(QStringLiteral("C_BOTH_MODELS"), component);
        QSignalSpy completedSpy(&stage, &ExportTypeStage::completed);
        stage.start({QStringLiteral("C_BOTH_MODELS")}, cachedData);
        if (completedSpy.count() == 0)
            QVERIFY2(completedSpy.wait(3000), "Combined 3D model export should complete");
        QCOMPARE(completedSpy.count(), 1);
        QCOMPARE(completedSpy.at(0).at(0).toInt(), 1);
        QCOMPARE(completedSpy.at(0).at(1).toInt(), 0);
        QCOMPARE(completedSpy.at(0).at(2).toInt(), 0);
        // outputBase：测试得到的实际结果数据。
        const QString outputBase = QStringLiteral("BothModels.3dmodels/Both Model");
        QVERIFY(QFile::exists(tempDir.filePath(outputBase + QStringLiteral(".wrl"))));
        QVERIFY(QFile::exists(tempDir.filePath(outputBase + QStringLiteral(".step"))));

        QFile manifest(tempDir.filePath(QStringLiteral("BothModels.3dmodels/manifest.json")));
        QVERIFY(manifest.open(QIODevice::ReadOnly | QIODevice::Text));
        // manifestObject：测试构造的 JSON 数据对象。
        const QJsonObject manifestObject = QJsonDocument::fromJson(manifest.readAll()).object();
        QCOMPARE(manifestObject.value(QStringLiteral("format")).toString(),
                 QStringLiteral("EasyKiConverter.3d-model-manifest"));
        const QJsonObject componentObject =
            manifestObject.value(QStringLiteral("components")).toArray().first().toObject();
        QCOMPARE(componentObject.value(QStringLiteral("componentId")).toString(), QStringLiteral("C_BOTH_MODELS"));
        QCOMPARE(componentObject.value(QStringLiteral("symbol")).toString(), QStringLiteral("BOTH_SYMBOL"));
        QCOMPARE(componentObject.value(QStringLiteral("footprint")).toString(), QStringLiteral("BOTH_FOOTPRINT"));
        QCOMPARE(componentObject.value(QStringLiteral("files")).toObject().value(QStringLiteral("wrl")).toString(),
                 QStringLiteral("Both Model.wrl"));
        QCOMPARE(componentObject.value(QStringLiteral("files")).toObject().value(QStringLiteral("step")).toString(),
                 QStringLiteral("Both Model.step"));
        // modelObject：测试构造的 JSON 数据对象。
        const QJsonObject modelObject = componentObject.value(QStringLiteral("model")).toObject();
        QCOMPARE(modelObject.value(QStringLiteral("uuid")).toString(), QStringLiteral("both-model-uuid"));
        QCOMPARE(modelObject.value(QStringLiteral("translationMm")).toObject().value(QStringLiteral("x")).toDouble(),
                 0.254);
        QCOMPARE(modelObject.value(QStringLiteral("rotationDeg")).toObject().value(QStringLiteral("z")).toDouble(),
                 30.0);
        QCOMPARE(modelObject.value(QStringLiteral("stepOffsetMm")).toObject().value(QStringLiteral("y")).toDouble(),
                 0.2);
    }

    // 验证相同模型名称会被稳定去重，避免独立三维模型文件互相覆盖。
    /**
     * @brief 验证 duplicateModelNamesProduceDistinctFiles 测试场景。
     */
    void duplicateModelNamesProduceDistinctFiles() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // stage：测试用导出阶段。
        Model3DExportStage stage;
        // options：当前测试使用的导出选项。
        ExportOptions options;
        options.outputPath = tempDir.path();
        options.libName = QStringLiteral("DuplicateModels");
        options.exportModel3DFormat = ExportOptions::MODEL_3D_FORMAT_WRL;
        options.overwriteExistingFiles = true;
        stage.setOptions(options);

        // cachedData：测试使用的文本或数据内容。
        QMap<QString, QSharedPointer<ComponentData>> cachedData;
        for (const QString& componentId : {QStringLiteral("C_MODEL_A"), QStringLiteral("C_MODEL_B")}) {
            // component：测试使用的缓存或元器件对象。
            auto component = QSharedPointer<ComponentData>::create();
            // model：测试使用的配置值或状态。
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
    /**
     * @brief 验证 model3DNoOverwritePreservesExistingFile 测试场景。
     */
    void model3DNoOverwritePreservesExistingFile() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        // outputDir：测试使用的文件系统路径。
        const QString outputDir = tempDir.filePath(QStringLiteral("NoOverwriteModels.3dmodels"));
        QVERIFY(QDir().mkpath(outputDir));
        // existingPath：测试使用的文件系统路径。
        const QString existingPath = QDir(outputDir).filePath(QStringLiteral("Existing Model.wrl"));
        QFile existing(existingPath);
        QVERIFY(existing.open(QIODevice::WriteOnly));
        // originalData：测试使用的文本或数据内容。
        const QByteArray originalData = QByteArrayLiteral("original model\n");
        QVERIFY(existing.write(originalData) == originalData.size());
        existing.close();

        // stage：测试用导出阶段。
        Model3DExportStage stage;
        // options：当前测试使用的导出选项。
        ExportOptions options;
        options.outputPath = tempDir.path();
        options.libName = QStringLiteral("NoOverwriteModels");
        options.exportModel3DFormat = ExportOptions::MODEL_3D_FORMAT_WRL;
        options.overwriteExistingFiles = false;
        stage.setOptions(options);

        // component：测试使用的缓存或元器件对象。
        auto component = QSharedPointer<ComponentData>::create();
        // model：测试使用的配置值或状态。
        auto model = QSharedPointer<Model3DData>::create();
        model->setUuid(QStringLiteral("no-overwrite-model"));
        model->setName(QStringLiteral("Existing Model"));
        component->setModel3DData(model);
        component->setModel3DObjRaw(QByteArrayLiteral("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n"));

        // cachedData：测试使用的文本或数据内容。
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

    // 验证追加新模型时会合并已有三维关联清单，而不会丢失历史组件条目。
    /**
     * @brief 验证 model3DAppendMergesExistingManifest 测试场景。
     */
    void model3DAppendMergesExistingManifest() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        // outputDir：测试使用的文件系统路径。
        const QString outputDir = tempDir.filePath(QStringLiteral("ManifestConflict.3dmodels"));
        QVERIFY(QDir().mkpath(outputDir));
        // manifestPath：测试使用的文件系统路径。
        const QString manifestPath = QDir(outputDir).filePath(QStringLiteral("manifest.json"));
        // oldModelPath：已有历史组件的模型文件路径。
        const QString oldModelPath = QDir(outputDir).filePath(QStringLiteral("Old Model.wrl"));
        QFile oldModel(oldModelPath);
        QVERIFY(oldModel.open(QIODevice::WriteOnly | QIODevice::Text));
        QVERIFY(oldModel.write("old model\n") > 0);
        oldModel.close();

        QFile manifest(manifestPath);
        QVERIFY(manifest.open(QIODevice::WriteOnly | QIODevice::Text));
        // originalManifest：包含未参与本轮导出的历史组件条目。
        const QByteArray originalManifest = QByteArrayLiteral(
            "{\"format\":\"EasyKiConverter.3d-model-manifest\",\"version\":1,"
            "\"components\":[{\"componentId\":\"C_OLD\",\"modelStem\":\"Old Model\","
            "\"status\":\"success\",\"files\":{\"wrl\":\"Old Model.wrl\"}}]}\n");
        QVERIFY(manifest.write(originalManifest) == originalManifest.size());
        manifest.close();

        // stage：测试用导出阶段。
        Model3DExportStage stage;
        // options：当前测试使用的导出选项。
        ExportOptions options;
        options.outputPath = tempDir.path();
        options.libName = QStringLiteral("ManifestConflict");
        options.exportModel3DFormat = ExportOptions::MODEL_3D_FORMAT_WRL;
        options.overwriteExistingFiles = false;
        stage.setOptions(options);

        // component：测试使用的缓存或元器件对象。
        auto component = QSharedPointer<ComponentData>::create();
        // model：测试使用的配置值或状态。
        auto model = QSharedPointer<Model3DData>::create();
        model->setUuid(QStringLiteral("manifest-conflict-model"));
        model->setName(QStringLiteral("New Model"));
        component->setModel3DData(model);
        component->setModel3DObjRaw(QByteArrayLiteral("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n"));

        // cachedData：测试使用的文本或数据内容。
        QMap<QString, QSharedPointer<ComponentData>> cachedData;
        cachedData.insert(QStringLiteral("C_MANIFEST_CONFLICT"), component);
        QSignalSpy completedSpy(&stage, &ExportTypeStage::completed);
        stage.start({QStringLiteral("C_MANIFEST_CONFLICT")}, cachedData);
        if (completedSpy.count() == 0)
            QVERIFY2(completedSpy.wait(3000), "Manifest merge should complete");
        QCOMPARE(completedSpy.count(), 1);
        QCOMPARE(completedSpy.at(0).at(0).toInt(), 1);
        QCOMPARE(completedSpy.at(0).at(1).toInt(), 0);
        QCOMPARE(completedSpy.at(0).at(2).toInt(), 0);
        QVERIFY(QFile::exists(QDir(outputDir).filePath(QStringLiteral("New Model.wrl"))));

        QFile result(manifestPath);
        QVERIFY(result.open(QIODevice::ReadOnly | QIODevice::Text));
        // mergedManifest：追加导出后的合并清单。
        const QJsonObject mergedManifest = QJsonDocument::fromJson(result.readAll()).object();
        const QJsonArray mergedComponents =
            mergedManifest.value(QStringLiteral("components")).toArray();  // mergedComponents：合并后的组件条目。
        QCOMPARE(mergedComponents.size(), 2);
        bool foundOld = false;  // foundOld：是否保留历史组件条目。
        bool foundNew = false;  // foundNew：是否写入本轮成功组件条目。
        for (const QJsonValue& value : mergedComponents) {
            const QJsonObject componentObject = value.toObject();  // componentObject：当前合并组件对象。
            const QString componentId =
                componentObject.value(QStringLiteral("componentId")).toString();  // componentId：当前组件编号。
            foundOld = foundOld || componentId == QStringLiteral("C_OLD");
            if (componentId == QStringLiteral("C_MANIFEST_CONFLICT")) {
                foundNew = true;
                QCOMPARE(
                    componentObject.value(QStringLiteral("files")).toObject().value(QStringLiteral("wrl")).toString(),
                    QStringLiteral("New Model.wrl"));
            }
        }
        QVERIFY(foundOld);
        QVERIFY(foundNew);
    }

    // 验证追加模式只生成缺失的 STEP，并保留已存在的 WRL 与其他组件条目。
    /**
     * @brief 验证 model3DAppendCompletesPartialWrlStepPair 测试场景。
     */
    void model3DAppendCompletesPartialWrlStepPair() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        // outputDir：测试使用的文件系统路径。
        const QString outputDir = tempDir.filePath(QStringLiteral("PartialModels.3dmodels"));
        QVERIFY(QDir().mkpath(outputDir));
        // existingWrlPath：追加前已经存在的 WRL 文件路径。
        const QString existingWrlPath = QDir(outputDir).filePath(QStringLiteral("Partial Model.wrl"));
        QFile existingWrl(existingWrlPath);
        QVERIFY(existingWrl.open(QIODevice::WriteOnly | QIODevice::Text));
        QVERIFY(existingWrl.write("existing wrl\n") > 0);
        existingWrl.close();

        // manifest：追加前的历史关联清单文件对象。
        QFile manifest(QDir(outputDir).filePath(QStringLiteral("manifest.json")));
        QVERIFY(manifest.open(QIODevice::WriteOnly | QIODevice::Text));
        const QByteArray originalManifest = QByteArrayLiteral(
            "{\"format\":\"EasyKiConverter.3d-model-manifest\",\"version\":1,"
            "\"components\":[{\"componentId\":\"C_OTHER\",\"status\":\"success\","
            "\"files\":{\"wrl\":\"Other.wrl\"}},{\"componentId\":\"C_PARTIAL\","
            "\"status\":\"success\",\"files\":{\"wrl\":\"Partial Model.wrl\"}}]}\n");
        QVERIFY(manifest.write(originalManifest) == originalManifest.size());
        manifest.close();

        // stage：测试用导出阶段。
        Model3DExportStage stage;
        // options：追加模式下同时请求 WRL 和 STEP 的导出选项。
        ExportOptions options;
        options.outputPath = tempDir.path();
        options.libName = QStringLiteral("PartialModels");
        options.exportModel3DFormat = ExportOptions::MODEL_3D_FORMAT_BOTH;
        options.overwriteExistingFiles = false;
        options.updateMode = false;
        stage.setOptions(options);

        // component：拥有已存在 WRL 和缺失 STEP 的测试组件。
        auto component = QSharedPointer<ComponentData>::create();
        // model：测试组件的三维模型元数据。
        auto model = QSharedPointer<Model3DData>::create();
        model->setUuid(QStringLiteral("partial-model-uuid"));
        model->setName(QStringLiteral("Partial Model"));
        model->setStep(QByteArrayLiteral("ISO-10303-21;\nDATA;\nENDSEC;\nEND-ISO-10303-21;\n"));
        component->setModel3DData(model);
        component->setModel3DObjRaw(QByteArrayLiteral("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n"));

        // cachedData：待导出的组件缓存。
        QMap<QString, QSharedPointer<ComponentData>> cachedData;
        cachedData.insert(QStringLiteral("C_PARTIAL"), component);
        // completedSpy：阶段完成信号监听器。
        QSignalSpy completedSpy(&stage, &ExportTypeStage::completed);
        stage.start({QStringLiteral("C_PARTIAL")}, cachedData);
        if (completedSpy.count() == 0)
            QVERIFY2(completedSpy.wait(3000), "Partial WRL/STEP append should complete");
        QCOMPARE(completedSpy.count(), 1);
        QCOMPARE(completedSpy.at(0).at(0).toInt(), 1);
        QCOMPARE(completedSpy.at(0).at(1).toInt(), 0);
        QVERIFY(QFile::exists(existingWrlPath));
        QVERIFY(QFile::exists(QDir(outputDir).filePath(QStringLiteral("Partial Model.step"))));

        // result：读取追加后的关联清单。
        QFile result(QDir(outputDir).filePath(QStringLiteral("manifest.json")));
        QVERIFY(result.open(QIODevice::ReadOnly | QIODevice::Text));
        const QJsonArray components =
            QJsonDocument::fromJson(result.readAll()).object().value(QStringLiteral("components")).toArray();
        QCOMPARE(components.size(), 2);
        for (const QJsonValue& value : components) {
            const QJsonObject componentObject = value.toObject();  // componentObject：当前合并组件对象。
            if (componentObject.value(QStringLiteral("componentId")).toString() == QStringLiteral("C_PARTIAL")) {
                const QJsonObject files =
                    componentObject.value(QStringLiteral("files")).toObject();  // files：当前组件文件映射。
                QCOMPARE(files.value(QStringLiteral("wrl")).toString(), QStringLiteral("Partial Model.wrl"));
                QCOMPARE(files.value(QStringLiteral("step")).toString(), QStringLiteral("Partial Model.step"));
            }
        }
    }

    // 验证更新模式只替换当前组件条目，不会丢失其他历史组件。
    /**
     * @brief 验证 model3DUpdatePreservesOtherManifestEntries 测试场景。
     */
    void model3DUpdatePreservesOtherManifestEntries() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        // outputDir：测试使用的文件系统路径。
        const QString outputDir = tempDir.filePath(QStringLiteral("UpdateModels.3dmodels"));
        QVERIFY(QDir().mkpath(outputDir));
        // manifest：更新前的历史关联清单文件对象。
        QFile manifest(QDir(outputDir).filePath(QStringLiteral("manifest.json")));
        QVERIFY(manifest.open(QIODevice::WriteOnly | QIODevice::Text));
        const QByteArray originalManifest = QByteArrayLiteral(
            "{\"format\":\"EasyKiConverter.3d-model-manifest\",\"version\":1,"
            "\"components\":[{\"componentId\":\"C_KEEP\",\"status\":\"success\","
            "\"files\":{\"wrl\":\"Keep.wrl\"}},{\"componentId\":\"C_UPDATE\","
            "\"status\":\"success\",\"files\":{\"wrl\":\"Update.wrl\"}}]}\n");
        QVERIFY(manifest.write(originalManifest) == originalManifest.size());
        manifest.close();

        // stage：测试用导出阶段。
        Model3DExportStage stage;
        // options：允许更新当前组件但不启用完整覆盖的导出选项。
        ExportOptions options;
        options.outputPath = tempDir.path();
        options.libName = QStringLiteral("UpdateModels");
        options.exportModel3DFormat = ExportOptions::MODEL_3D_FORMAT_WRL;
        options.overwriteExistingFiles = false;
        options.updateMode = true;
        stage.setOptions(options);

        // component：需要更新三维模型的测试组件。
        auto component = QSharedPointer<ComponentData>::create();
        // model：更新后的三维模型元数据。
        auto model = QSharedPointer<Model3DData>::create();
        model->setUuid(QStringLiteral("update-model-uuid"));
        model->setName(QStringLiteral("Update"));
        component->setModel3DData(model);
        component->setModel3DObjRaw(QByteArrayLiteral("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n"));

        // cachedData：待更新的组件缓存。
        QMap<QString, QSharedPointer<ComponentData>> cachedData;
        cachedData.insert(QStringLiteral("C_UPDATE"), component);
        // completedSpy：阶段完成信号监听器。
        QSignalSpy completedSpy(&stage, &ExportTypeStage::completed);
        stage.start({QStringLiteral("C_UPDATE")}, cachedData);
        if (completedSpy.count() == 0)
            QVERIFY2(completedSpy.wait(3000), "3D model update should complete");
        QCOMPARE(completedSpy.count(), 1);
        QCOMPARE(completedSpy.at(0).at(0).toInt(), 1);
        QCOMPARE(completedSpy.at(0).at(1).toInt(), 0);

        // result：读取更新后的关联清单。
        QFile result(QDir(outputDir).filePath(QStringLiteral("manifest.json")));
        QVERIFY(result.open(QIODevice::ReadOnly | QIODevice::Text));
        const QJsonArray components =
            QJsonDocument::fromJson(result.readAll()).object().value(QStringLiteral("components")).toArray();
        QCOMPARE(components.size(), 2);
        bool foundKeep = false;  // foundKeep：是否保留未参与更新的历史组件。
        bool foundUpdate = false;  // foundUpdate：是否更新当前组件条目。
        for (const QJsonValue& value : components) {
            const QJsonObject componentObject = value.toObject();  // componentObject：当前合并组件对象。
            const QString componentId =
                componentObject.value(QStringLiteral("componentId")).toString();  // componentId：当前组件编号。
            foundKeep = foundKeep || componentId == QStringLiteral("C_KEEP");
            if (componentId == QStringLiteral("C_UPDATE")) {
                foundUpdate = true;
                QCOMPARE(
                    componentObject.value(QStringLiteral("files")).toObject().value(QStringLiteral("wrl")).toString(),
                    QStringLiteral("Update.wrl"));
            }
        }
        QVERIFY(foundKeep);
        QVERIFY(foundUpdate);
    }

    // 验证追加模式跳过已有模型时不会改写关联清单。
    /**
     * @brief 验证 model3DAppendSkipPreservesManifest 测试场景。
     */
    void model3DAppendSkipPreservesManifest() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        // outputDir：测试用三维模型输出目录。
        const QString outputDir = tempDir.filePath(QStringLiteral("SkipModels.3dmodels"));
        QVERIFY(QDir().mkpath(outputDir));
        // modelPath：追加前已经存在的模型文件路径。
        const QString modelPath = QDir(outputDir).filePath(QStringLiteral("Skip.wrl"));
        QFile modelFile(modelPath);
        QVERIFY(modelFile.open(QIODevice::WriteOnly | QIODevice::Text));
        // originalModel：追加前已有模型文件的原始内容。
        const QByteArray originalModel = QByteArrayLiteral("existing model\n");
        QVERIFY(modelFile.write(originalModel) == originalModel.size());
        modelFile.close();

        // manifestPath：已有关联清单路径。
        const QString manifestPath = QDir(outputDir).filePath(QStringLiteral("manifest.json"));
        const QByteArray originalManifest = QByteArrayLiteral(
            "{\"format\":\"EasyKiConverter.3d-model-manifest\",\"version\":1,"
            "\"components\":[{\"componentId\":\"C_SKIP\",\"modelStem\":\"Skip\","
            "\"status\":\"success\",\"files\":{\"wrl\":\"Skip.wrl\"}}]}\n");
        QFile manifestFile(manifestPath);
        QVERIFY(manifestFile.open(QIODevice::WriteOnly | QIODevice::Text));
        QVERIFY(manifestFile.write(originalManifest) == originalManifest.size());
        manifestFile.close();

        // stage：测试用导出阶段。
        Model3DExportStage stage;
        // options：追加模式下只请求 WRL 且不覆盖已有文件的导出选项。
        ExportOptions options;
        options.outputPath = tempDir.path();
        options.libName = QStringLiteral("SkipModels");
        options.exportModel3DFormat = ExportOptions::MODEL_3D_FORMAT_WRL;
        options.overwriteExistingFiles = false;
        options.updateMode = false;
        stage.setOptions(options);

        // component：模型文件已存在的测试组件。
        auto component = QSharedPointer<ComponentData>::create();
        // model：测试组件的三维模型元数据。
        auto model = QSharedPointer<Model3DData>::create();
        model->setUuid(QStringLiteral("skip-model-uuid"));
        model->setName(QStringLiteral("Skip"));
        component->setModel3DData(model);
        component->setModel3DObjRaw(QByteArrayLiteral("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n"));

        // cachedData：待追加的组件缓存。
        QMap<QString, QSharedPointer<ComponentData>> cachedData;
        cachedData.insert(QStringLiteral("C_SKIP"), component);
        // completedSpy：阶段完成信号监听器。
        QSignalSpy completedSpy(&stage, &ExportTypeStage::completed);
        stage.start({QStringLiteral("C_SKIP")}, cachedData);
        if (completedSpy.count() == 0)
            QVERIFY2(completedSpy.wait(3000), "Skipped 3D model append should complete");
        QCOMPARE(completedSpy.count(), 1);
        QCOMPARE(completedSpy.at(0).at(0).toInt(), 0);
        QCOMPARE(completedSpy.at(0).at(1).toInt(), 0);
        QCOMPARE(completedSpy.at(0).at(2).toInt(), 1);

        // resultManifest：跳过后读取的关联清单内容。
        QFile resultManifest(manifestPath);
        QVERIFY(resultManifest.open(QIODevice::ReadOnly | QIODevice::Text));
        QCOMPARE(resultManifest.readAll(), originalManifest);
        // resultModel：跳过后读取的模型内容。
        QFile resultModel(modelPath);
        QVERIFY(resultModel.open(QIODevice::ReadOnly | QIODevice::Text));
        QCOMPARE(resultModel.readAll(), originalModel);
    }

    // 验证关联清单无法解析时不会覆盖原始清单。
    /**
     * @brief 验证 model3DInvalidManifestPreservesOriginal 测试场景。
     */
    void model3DInvalidManifestPreservesOriginal() {
        // tempDir：测试使用的文件系统路径。
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        // outputDir：测试用三维模型输出目录。
        const QString outputDir = tempDir.filePath(QStringLiteral("InvalidManifest.3dmodels"));
        QVERIFY(QDir().mkpath(outputDir));
        // manifestPath：格式无效的旧关联清单路径。
        const QString manifestPath = QDir(outputDir).filePath(QStringLiteral("manifest.json"));
        // originalManifest：格式无效清单的原始内容。
        const QByteArray originalManifest = QByteArrayLiteral("not-json\n");
        QFile manifestFile(manifestPath);
        QVERIFY(manifestFile.open(QIODevice::WriteOnly | QIODevice::Text));
        QVERIFY(manifestFile.write(originalManifest) == originalManifest.size());
        manifestFile.close();

        // stage：测试用导出阶段。
        Model3DExportStage stage;
        // options：追加模式下导出 WRL 的选项。
        ExportOptions options;
        options.outputPath = tempDir.path();
        options.libName = QStringLiteral("InvalidManifest");
        options.exportModel3DFormat = ExportOptions::MODEL_3D_FORMAT_WRL;
        options.overwriteExistingFiles = false;
        options.updateMode = false;
        stage.setOptions(options);

        // component：用于触发清单处理的测试组件。
        auto component = QSharedPointer<ComponentData>::create();
        // model：测试组件的三维模型元数据。
        auto model = QSharedPointer<Model3DData>::create();
        model->setUuid(QStringLiteral("invalid-manifest-model-uuid"));
        model->setName(QStringLiteral("Invalid Manifest"));
        component->setModel3DData(model);
        component->setModel3DObjRaw(QByteArrayLiteral("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n"));

        // cachedData：待导出的组件缓存。
        QMap<QString, QSharedPointer<ComponentData>> cachedData;
        cachedData.insert(QStringLiteral("C_INVALID_MANIFEST"), component);
        // completedSpy：阶段完成信号监听器。
        QSignalSpy completedSpy(&stage, &ExportTypeStage::completed);
        stage.start({QStringLiteral("C_INVALID_MANIFEST")}, cachedData);
        if (completedSpy.count() == 0)
            QVERIFY2(completedSpy.wait(3000), "Invalid manifest export should complete");
        QCOMPARE(completedSpy.count(), 1);
        QCOMPARE(completedSpy.at(0).at(0).toInt(), 1);
        QCOMPARE(completedSpy.at(0).at(1).toInt(), 0);

        // resultManifest：清单解析失败后保留的原始内容。
        QFile resultManifest(manifestPath);
        QVERIFY(resultManifest.open(QIODevice::ReadOnly | QIODevice::Text));
        QCOMPARE(resultManifest.readAll(), originalManifest);
    }

    // 验证运行中的阶段会拒绝重复启动请求。
    /**
     * @brief 验证 duplicateStartWhileRunningIsIgnored 测试场景。
     */
    void duplicateStartWhileRunningIsIgnored() {
        // stage：测试用导出阶段。
        DeferredStage stage;
        QSignalSpy completedSpy(&stage, &ExportTypeStage::completed);

        // cachedData：测试使用的文本或数据内容。
        QMap<QString, QSharedPointer<ComponentData>> cachedData;
        cachedData[QStringLiteral("C8001")] = QSharedPointer<ComponentData>::create();
        cachedData[QStringLiteral("C8002")] = QSharedPointer<ComponentData>::create();

        stage.start({QStringLiteral("C8001")}, cachedData);
        QVERIFY(stage.isRunning());

        // 运行中再次 start 应被忽略
        stage.start({QStringLiteral("C8002")}, cachedData);

        QVERIFY2(completedSpy.wait(3000), "First start should complete");
        QCOMPARE(completedSpy.count(), 1);  // 只有 1 次 completed

        // progress：测试用导出进度状态。
        const ExportTypeProgress progress = stage.getProgress();
        QCOMPARE(progress.totalCount, 1);  // 只有 C8001，C8002 被忽略
    }

    // 验证 Worker 失败时会递增失败计数并保留错误信息。
    /**
     * @brief 验证 failedComponentIncrementsFailedCount 测试场景。
     */
    void failedComponentIncrementsFailedCount() {
        // stage：测试用导出阶段。
        FailStage stage;
        QSignalSpy completedSpy(&stage, &ExportTypeStage::completed);
        QSignalSpy itemSpy(&stage, &ExportTypeStage::itemStatusChanged);

        // ids：测试使用的元器件标识或名称。
        QStringList ids = {"C9001", "C9002", "C9003"};
        // cachedData：测试使用的文本或数据内容。
        QMap<QString, QSharedPointer<ComponentData>> cachedData;
        for (const QString& id : ids) {
            cachedData[id] = QSharedPointer<ComponentData>::create();
        }

        stage.start(ids, cachedData);

        QCOMPARE(completedSpy.count(), 1);
        QCOMPARE(completedSpy.at(0).at(0).toInt(), 0);  // successCount
        QCOMPARE(completedSpy.at(0).at(1).toInt(), 3);  // failedCount

        // progress：测试用导出进度状态。
        const ExportTypeProgress progress = stage.getProgress();
        QCOMPARE(progress.totalCount, 3);
        QCOMPARE(progress.completedCount, 3);
        QCOMPARE(progress.successCount, 0);
        QCOMPARE(progress.failedCount, 3);

        // 验证每个失败项都有错误信息
        int failedSignals = 0;
        for (int i = 0; i < itemSpy.count(); i++) {
            // status：测试用状态对象。
            const auto status = qvariant_cast<ExportItemStatus>(itemSpy.at(i).at(1));
            if (status.status == ExportItemStatus::Status::Failed) {
                QVERIFY(!status.errorMessage.isEmpty());
                failedSignals++;
            }
        }
        QCOMPARE(failedSignals, 3);
    }

    // 验证线程池并发数限制会约束同时运行的 Worker 数量。
    /**
     * @brief 验证 concurrencyLimitIsRespected 测试场景。
     */
    void concurrencyLimitIsRespected() {
        ConcurrentStage stage(2);
        QSignalSpy completedSpy(&stage, &ExportTypeStage::completed);

        // ids：测试使用的元器件标识或名称。
        QStringList ids = {"CA001", "CA002", "CA003", "CA004", "CA005"};
        // cachedData：测试使用的文本或数据内容。
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
    /**
     * @brief 验证 cancelStopsAfterActiveWorkersDrain 测试场景。
     * @param componentId 测试使用的标识或名称。
     * @param footprintName 测试使用的标识或名称。
     * @param model3DName 测试使用的标识或名称。
     */
    void cancelStopsAfterActiveWorkersDrain() {
        // stage：测试用导出阶段。
        DeferredStage stage;
        QSignalSpy completedSpy(&stage, &ExportTypeStage::completed);

        // ids：测试使用的元器件标识或名称。
        QStringList ids = {"CB001", "CB002", "CB003", "CB004", "CB005"};
        // cachedData：测试使用的文本或数据内容。
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
        // success：测试结果是否成功。
        int success = args.at(0).toInt();
        // failed：测试使用的数值结果。
        int failed = args.at(1).toInt();
        QCOMPARE(success, 0);
        QCOMPARE(failed, 1);  // 仅活跃的 1 个 worker 标记为 failed

        // pending 队列被清空，completedCount 仅反映实际处理的
        const ExportTypeProgress progress = stage.getProgress();
        QCOMPARE(progress.completedCount, 1);
    }

private:
    static QSharedPointer<ComponentData> makeSymbolComponent(const QString& componentId, const QString& symbolName) {
        // componentData：测试使用的文本或数据内容。
        auto componentData = QSharedPointer<ComponentData>::create();
        componentData->setLcscId(componentId);

        // symbolData：测试使用的文本或数据内容。
        auto symbolData = QSharedPointer<SymbolData>::create();
        // info：测试用元数据对象。
        SymbolInfo info;
        info.name = symbolName;
        info.prefix = QStringLiteral("U");
        info.package = QStringLiteral("PKG_%1").arg(componentId);
        symbolData->setInfo(info);
        symbolData->setBbox({0, 0, 0, 0});

        componentData->setSymbolData(symbolData);
        return componentData;
    }

    /**
     * @brief 验证 makeFootprintComponent 测试场景。
     * @param footprintName 测试使用的标识或名称。
     * @param componentId 测试使用的标识或名称。
     * @param model3DName 测试使用的标识或名称。
     */
    static QSharedPointer<ComponentData> makeFootprintComponent(const QString& componentId,
                                                                const QString& footprintName,
                                                                const QString& model3DName = QString()) {
        // componentData：测试使用的文本或数据内容。
        auto componentData = QSharedPointer<ComponentData>::create();
        componentData->setLcscId(componentId);

        // footprintData：测试使用的文本或数据内容。
        auto footprintData = QSharedPointer<FootprintData>::create();
        // info：测试用元数据对象。
        FootprintInfo info;
        info.name = footprintName;
        info.type = QStringLiteral("smd");
        footprintData->setInfo(info);

        // bbox：测试用几何边界框。
        FootprintBBox bbox;
        bbox.x = 0;
        bbox.y = 0;
        bbox.width = 1;
        bbox.height = 1;
        footprintData->setBbox(bbox);

        if (!model3DName.isEmpty()) {
            // model3D：测试使用的配置值或状态。
            Model3DData model3D;
            model3D.setName(model3DName);
            model3D.setUuid(QStringLiteral("uuid-%1").arg(componentId));
            footprintData->setModel3D(model3D);
        }

        componentData->setFootprintData(footprintData);
        return componentData;
    }

    // 构造同时具备符号和封装的最小组件，供组合库阶段测试使用。
    /**
     * @brief 验证 makeCombinedLibraryComponent 测试场景。
     * @param componentId 测试使用的标识或名称。
     * @param footprintName 测试使用的标识或名称。
     */
    static QSharedPointer<ComponentData> makeCombinedLibraryComponent(const QString& componentId,
                                                                      const QString& footprintName) {
        // componentData：测试使用的文本或数据内容。
        auto componentData = makeSymbolComponent(componentId, QStringLiteral("CADSTAR_SYMBOL"));
        componentData->setName(componentId);
        componentData->setPackage(footprintName);
        // symbolData：测试使用的文本或数据内容。
        auto symbolData = componentData->symbolData();
        // symbolInfo：测试用符号数据。
        SymbolInfo symbolInfo = symbolData->info();
        symbolInfo.package = footprintName;
        symbolData->setInfo(symbolInfo);
        // symbolPin：测试用符号数据。
        SymbolPin symbolPin;
        symbolPin.settings.spicePinNumber = QStringLiteral("1");
        symbolPin.settings.posX = 0.0;
        symbolPin.settings.posY = 0.0;
        symbolPin.name.text = QStringLiteral("PIN1");
        symbolPin.name.isDisplayed = true;
        symbolData->addPin(symbolPin);
        // footprintData：测试使用的文本或数据内容。
        auto footprintData = QSharedPointer<FootprintData>::create();
        // info：测试用元数据对象。
        FootprintInfo info;
        info.name = footprintName;
        info.type = QStringLiteral("smd");
        footprintData->setInfo(info);
        // bbox：测试用几何边界框。
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

/**
 * @brief 验证所有已注册目标格式都能创建独立三维模型导出器。
 */
void TestExportTypeStage::model3DExporterIsAvailableForAllTargets() {
    // formats：目标格式集合。
    const QList<TargetEdaFormat> formats = {TargetEdaFormat::KiCad,
                                            TargetEdaFormat::Altium,
                                            TargetEdaFormat::Xpedition,
                                            TargetEdaFormat::Allegro,
                                            TargetEdaFormat::Pads,
                                            TargetEdaFormat::Eagle,
                                            TargetEdaFormat::Pcad,
                                            TargetEdaFormat::Cadstar,
                                            TargetEdaFormat::Orcad};
    for (const TargetEdaFormat format : formats)
        QVERIFY2(ExporterFactory::createModel3DExporter(format) != nullptr, "目标格式缺少独立三维导出器");
}

/**
 * @brief 验证各目标格式的符号、封装和三维导出入口与其实际能力边界一致。
 */
void TestExportTypeStage::exporterFactoryMatchesLibraryArtifactCapabilities() {
    // libraryTargets：目标库集合。
    const QList<TargetEdaFormat> libraryTargets = {TargetEdaFormat::KiCad,
                                                   TargetEdaFormat::Altium,
                                                   TargetEdaFormat::Xpedition,
                                                   TargetEdaFormat::Allegro,
                                                   TargetEdaFormat::Pads,
                                                   TargetEdaFormat::Eagle,
                                                   TargetEdaFormat::Pcad,
                                                   TargetEdaFormat::Cadstar};
    for (const TargetEdaFormat format : libraryTargets) {
        QVERIFY2(ExporterFactory::createSymbolExporter(format) != nullptr, "目标格式缺少符号导出入口");
        QVERIFY2(ExporterFactory::createFootprintExporter(format) != nullptr, "目标格式缺少封装导出入口");
        QVERIFY2(ExporterFactory::createModel3DExporter(format) != nullptr, "目标格式缺少三维导出入口");
    }

    // OrCAD Capture 当前输出 XML 符号库和封装名称关联，不提供 Capture PCB 封装 writer。
    QVERIFY(ExporterFactory::createSymbolExporter(TargetEdaFormat::Orcad) != nullptr);
    QVERIFY(ExporterFactory::createFootprintExporter(TargetEdaFormat::Orcad) == nullptr);
    QVERIFY(ExporterFactory::createModel3DExporter(TargetEdaFormat::Orcad) != nullptr);
}

/**
 * @brief 验证 Eagle 和 CADSTAR 组合库能够通过通用工厂创建符号导出器。
 */
void TestExportTypeStage::combinedLibrarySymbolExportersAreAvailable() {
    QVERIFY(ExporterFactory::createSymbolExporter(TargetEdaFormat::Eagle) != nullptr);
    QVERIFY(ExporterFactory::createSymbolExporter(TargetEdaFormat::Cadstar) != nullptr);
    QVERIFY(ExporterFactory::createSymbolExporter(TargetEdaFormat::Allegro) != nullptr);
}

}  // namespace EasyKiConverter

QTEST_MAIN(EasyKiConverter::TestExportTypeStage)
#include "test_export_type_stage.moc"
