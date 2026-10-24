#include "Model3DExportStage.h"

#include "Model3DExportWorker.h"
#include "core/ir/Model3DDataConverter.h"
#include "core/kicad/Exporter3DModel.h"
#include "models/ComponentData.h"
#include "services/ComponentCacheService.h"
#include "utils/PathSecurity.h"

#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QPointer>
#include <QSaveFile>

namespace EasyKiConverter {

/**
 * @brief 初始化三维模型导出阶段及其并发度。
 * @param parent QObject 父对象。
 */
Model3DExportStage::Model3DExportStage(QObject* parent) : ExportTypeStage("Model3D", 2, parent) {
    // 在所有模型任务收敛后写入关联清单，确保清单只引用最终文件路径。
    connect(
        this,
        &ExportTypeStage::completed,
        this,
        [this](int, int, int) { writeAssociationManifest(); },
        Qt::DirectConnection);
}

Model3DExportStage::~Model3DExportStage() {
    cancel();
}

/**
 * @brief 准备三维模型路径并启动导出任务。
 * @param componentIds 待导出的元器件编号列表。
 * @param cachedData 已预加载的元器件数据。
 */
void Model3DExportStage::start(const QStringList& componentIds,
                               const QMap<QString, QSharedPointer<ComponentData>>& cachedData) {
    if (m_isExporting.load()) {
        qWarning() << "Model3DExportStage: Export already in progress";
        return;
    }

    m_threadPool.setMaxThreadCount(m_options.weakNetworkSupport ? 1 : 2);

    if (componentIds.isEmpty()) {
        qWarning() << "Model3DExportStage: No components to export";
        emit completed(0, 0, 0);
        return;
    }

    QString libName =
        m_options.libName.isEmpty() ? QStringLiteral("EasyKiConverter") : m_options.libName;  ///< 输出库名称。
    QString baseOutputDir = m_options.outputPath;  ///< 用户指定的输出根目录。
    if (baseOutputDir.isEmpty()) {
        baseOutputDir = QDir::currentPath() + QStringLiteral("/export");
    }
    QString outputDir =
        baseOutputDir + QDir::separator() + libName + QStringLiteral(".3dmodels");  ///< 三维模型输出目录。

    QDir dir;  ///< 用于创建输出目录的目录对象。
    const bool needWrl = m_options.needsModel3DWrl();  ///< 是否需要导出 WRL。
    const bool needStep = m_options.needsModel3DStep();  ///< 是否需要导出 STEP。

    qInfo() << "Model3DExportStage::start() - exportModel3DFormat:" << m_options.exportModel3DFormat
            << "needWrl:" << needWrl << "needStep:" << needStep;

    const auto hasModel3DUuid = [&cachedData](const QString& componentId) {  // 检查元器件是否具备三维模型 UUID。
        auto it = cachedData.constFind(componentId);  ///< 当前元器件缓存数据迭代器。
        if (it != cachedData.constEnd() && it.value()) {
            if (it.value()->model3DData() && !it.value()->model3DData()->uuid().isEmpty())
                return true;
            if (it.value()->footprintData() && !it.value()->footprintData()->model3D().uuid().isEmpty())
                return true;
        }

        QSharedPointer<ComponentData> cachedComponent =  // 从磁盘缓存读取的元器件数据。
            ComponentCacheService::instance()->loadComponentData(componentId);
        if (!cachedComponent)
            return false;
        if (cachedComponent->model3DData() && !cachedComponent->model3DData()->uuid().isEmpty())
            return true;
        return cachedComponent->footprintData() && !cachedComponent->footprintData()->model3D().uuid().isEmpty();
    };

    m_componentPaths.clear();
    m_modelFileStems.clear();
    m_skippedComponents.clear();
    m_preflightErrors.clear();

    // 先按输入顺序分配唯一文件名，避免多个元件共用模型名称时互相覆盖。
    QSet<QString> usedModelStems;  ///< 已使用的三维模型文件名主体集合。
    for (const QString& componentId : componentIds) {  // 当前元器件编号。
        QString modelName;  ///< 当前模型候选名称。
        const auto cachedIt = cachedData.constFind(componentId);  ///< 当前元器件缓存数据迭代器。
        if (cachedIt != cachedData.cend() && cachedIt.value()) {
            const auto& data = cachedIt.value();  ///< 当前元器件缓存数据。
            if (data->model3DData())
                modelName = data->model3DData()->name();
            if (modelName.isEmpty() && data->footprintData())
                modelName = data->footprintData()->info().name;
        }
        if (modelName.isEmpty())
            modelName = componentId;
        modelName = PathSecurity::sanitizeFilename(modelName);
        if (modelName.isEmpty())
            modelName = QStringLiteral("model");

        const QString baseName = modelName;  ///< 未添加冲突后缀的模型名。
        int suffix = 2;  ///< 模型名冲突时使用的后缀序号。
        while (usedModelStems.contains(modelName.toCaseFolded()))
            modelName = QStringLiteral("%1_%2").arg(baseName).arg(suffix++);
        usedModelStems.insert(modelName.toCaseFolded());
        m_modelFileStems.insert(componentId, modelName);
    }

    // 输出目录创建失败时仍然交给基类建立逐项状态，避免主服务留下 Pending 项。
    if (!dir.exists(outputDir) && !dir.mkpath(outputDir)) {
        qCritical() << "Model3DExportStage: Failed to create output directory:" << outputDir;
        const QString error = QStringLiteral("Failed to create 3D model output directory");  ///< 目录创建失败原因。
        for (const QString& componentId : componentIds)  // 当前需要标记失败的元器件编号。
            m_preflightErrors.insert(componentId, error);
        m_isExporting.store(true);
        ExportTypeStage::start(componentIds, cachedData);
        return;
    }

    m_tempManager.setOutputPath(outputDir);

    for (const QString& componentId : componentIds) {  // 当前元器件编号。
        if (!needWrl && !needStep) {
            m_skippedComponents.insert(componentId);
            continue;
        }
        if ((needWrl || needStep) && !hasModel3DUuid(componentId)) {
            qDebug() << "Model3DExportStage: No 3D model UUID, skipping temp paths for" << componentId;
            m_skippedComponents.insert(componentId);
            continue;
        }

        TempFilePaths paths;  ///< 当前元器件的 WRL/STEP 临时路径。
        if (needWrl) {
            paths.wrlTempPath =
                m_tempManager.createTempFilePath(componentId + QStringLiteral("_wrl"), QStringLiteral(".wrl"));
        }
        if (needStep) {
            paths.stepTempPath =
                m_tempManager.createTempFilePath(componentId + QStringLiteral("_step"), QStringLiteral(".step"));
        }

        const bool hasAllRequiredTempPaths =  // 当前元器件是否获得全部所需临时路径。
            (!needWrl || !paths.wrlTempPath.isEmpty()) && (!needStep || !paths.stepTempPath.isEmpty());
        if (hasAllRequiredTempPaths) {
            m_componentPaths[componentId] = paths;
        } else if (needWrl || needStep) {
            qWarning() << "Model3DExportStage: Failed to create temp path for component:" << componentId
                       << "needWrl:" << needWrl << "needStep:" << needStep;
            m_preflightErrors.insert(componentId, QStringLiteral("Failed to create 3D model temporary path"));
        }
    }

    m_isExporting.store(true);

    /**
     * @brief Paths must be ready before workers read m_componentPaths.
     */
    ExportTypeStage::start(componentIds, cachedData);
}

/**
 * @brief 取消三维模型导出并回滚尚未提交的临时文件。
 */
void Model3DExportStage::cancel() {
    if (!m_isRunning.load() && !m_isExporting.load()) {
        return;
    }

    qDebug() << "Model3DExportStage: cancelling...";
    m_cancelled.store(true);

    bool hasActiveWorkers = false;  ///< 是否仍有三维模型 Worker 在执行。
    {
        QMutexLocker locker(&m_workerMutex);
        m_pendingComponents.clear();
        hasActiveWorkers = !m_activeWorkers.isEmpty();
        if (!hasActiveWorkers) {
            m_isRunning.store(false);
        }
    }

    m_tempManager.rollbackAll();
    m_isExporting.store(false);

    qDebug() << "Model3DExportStage: cancelled";
}

/**
 * @brief 创建三维模型导出 Worker。
 */
QObject* Model3DExportStage::createWorker() {
    return new Model3DExportWorker();
}

/**
 * @brief 为有有效三维数据的元器件准备输出路径并提交 Worker。
 * @param worker 工作对象。
 * @param componentId 元器件编号。
 * @param data 数据缓冲区。
 */
void Model3DExportStage::startWorker(QObject* worker,
                                     const QString& componentId,
                                     const QSharedPointer<ComponentData>& data) {
    auto* exportWorker = qobject_cast<Model3DExportWorker*>(worker);  ///< 转换后的三维模型 Worker。
    if (!exportWorker) {
        qWarning() << "Model3DExportStage: Failed to cast worker to Model3DExportWorker";
        return;
    }

    if (m_skippedComponents.contains(componentId)) {
        completeSkippedItemProgress(exportWorker, componentId, QStringLiteral("No 3D model or format selected"));
        delete exportWorker;
        return;
    }

    const auto preflightError = m_preflightErrors.constFind(componentId);  ///< 当前元器件的预检错误迭代器。
    if (preflightError != m_preflightErrors.cend()) {
        completeItemProgress(exportWorker, componentId, false, preflightError.value());
        delete exportWorker;
        return;
    }

    const QString modelName =
        m_modelFileStems.value(componentId, PathSecurity::sanitizeFilename(componentId));  ///< 当前模型文件主体名。

    const bool needWrl = m_options.needsModel3DWrl();  ///< 是否需要 WRL 文件。
    const bool needStep = m_options.needsModel3DStep();  ///< 是否需要 STEP 文件。
    QString libName =
        m_options.libName.isEmpty() ? QStringLiteral("EasyKiConverter") : m_options.libName;  ///< 输出库名称。
    QString baseOutputDir = m_options.outputPath;  ///< 用户指定的输出根目录。
    if (baseOutputDir.isEmpty()) {
        baseOutputDir = QDir::currentPath() + QStringLiteral("/export");
    }
    QString outputDir =
        baseOutputDir + QDir::separator() + libName + QStringLiteral(".3dmodels");  ///< 三维模型输出目录。

    // workerOptions：当前元器件实际执行的三维模型导出选项。
    ExportOptions workerOptions = m_options;
    if (m_componentPaths.contains(componentId)) {
        TempFilePaths& paths = m_componentPaths[componentId];  ///< 当前元器件的可写临时路径记录。
        paths.wrlFinalPath = needWrl ? (outputDir + QDir::separator() + modelName + QStringLiteral(".wrl")) : QString();
        paths.stepFinalPath =
            needStep ? (outputDir + QDir::separator() + modelName + QStringLiteral(".step")) : QString();

        // 临时路径存在时，Worker 无法自行判断最终文件是否已存在，因此在提交前显式执行不覆盖策略。
        const bool wrlExists =
            !paths.wrlFinalPath.isEmpty() && QFile::exists(paths.wrlFinalPath);  ///< WRL 最终文件是否存在。
        const bool stepExists =
            !paths.stepFinalPath.isEmpty() && QFile::exists(paths.stepFinalPath);  ///< STEP 最终文件是否存在。
        const bool anyExisting = wrlExists || stepExists;  ///< 是否已有任一模型文件。
        const bool allowOverwrite =
            m_options.overwriteExistingFiles || m_options.updateMode;  ///< 当前是否允许更新已有文件。
        if (!allowOverwrite && anyExisting) {
            const bool allRequestedExist =
                (!needWrl || wrlExists) && (!needStep || stepExists);  ///< 所有请求模型是否均已存在。
            if (allRequestedExist) {
                m_componentPaths.remove(componentId);
                completeSkippedItemProgress(exportWorker, componentId, QStringLiteral("3D model file already exists"));
                delete exportWorker;
                return;
            }
            if (needWrl && wrlExists) {
                paths.wrlTempPath.clear();
                paths.wrlFinalPath.clear();
                workerOptions.exportModel3DFormat &= ~ExportOptions::MODEL_3D_FORMAT_WRL;
            }
            if (needStep && stepExists) {
                paths.stepTempPath.clear();
                paths.stepFinalPath.clear();
                workerOptions.exportModel3DFormat &= ~ExportOptions::MODEL_3D_FORMAT_STEP;
            }
            if (workerOptions.exportModel3DFormat == ExportOptions::MODEL_3D_FORMAT_NONE) {
                m_componentPaths.remove(componentId);
                completeSkippedItemProgress(exportWorker, componentId, QStringLiteral("3D model file already exists"));
                delete exportWorker;
                return;
            }
        }
        exportWorker->setOutputPaths({paths.wrlTempPath, paths.stepTempPath});
    }
    exportWorker->setOptions(workerOptions);
    exportWorker->setData(componentId, data, workerOptions);

    QPointer<Model3DExportStage> stagePtr(this);  ///< 防止异步回调访问已销毁的阶段对象。
    qInfo() << "Model3DExportStage: Connecting worker completed signal for" << componentId;
    connect(
        exportWorker,
        &Model3DExportWorker::completed,
        this,
        [stagePtr, exportWorker, componentId](const QString&, bool success, const QString& error) {
            qInfo() << "Model3DExportStage: Worker completed signal received for" << componentId
                    << "success:" << success;
            if (!stagePtr) {
                qInfo() << "Model3DExportStage: stagePtr is null!";
                exportWorker->deleteLater();
                return;
            }

            if (success && !stagePtr->m_cancelled.load() && stagePtr->m_componentPaths.contains(componentId)) {
                const TempFilePaths paths = stagePtr->m_componentPaths.value(componentId);  ///< 当前元器件路径记录。
                const bool needWrl = stagePtr->m_options.needsModel3DWrl();  ///< 回调时是否需要 WRL。
                const bool needStep = stagePtr->m_options.needsModel3DStep();  ///< 回调时是否需要 STEP。

                QVector<TempFileManager::CommitItem> commitItems;  ///< 待提交的三维模型事务项。
                if (needWrl && !paths.wrlTempPath.isEmpty()) {
                    commitItems.append({paths.wrlTempPath, paths.wrlFinalPath, false});
                }
                if (needStep && !paths.stepTempPath.isEmpty()) {
                    commitItems.append({paths.stepTempPath, paths.stepFinalPath, false});
                }
                if (!commitItems.isEmpty() && !stagePtr->m_tempManager.commitBatch(commitItems)) {
                    success = false;
                }
            }

            stagePtr->completeItemProgress(exportWorker, componentId, success, error);
            exportWorker->deleteLater();
        },
        Qt::QueuedConnection);

    m_threadPool.start(exportWorker);
}

/**
 * @brief 写入组件、符号、封装与独立三维模型文件之间的项目级关联清单。
 */
void Model3DExportStage::writeAssociationManifest() {
    if (m_componentIds.isEmpty())
        return;

    // reportManifestDiagnostic：向当前导出进度追加清单处理诊断并通知界面。
    const auto reportManifestDiagnostic = [this](const QString& diagnostic) {
        QMutexLocker locker(&m_progressMutex);
        if (!m_progress.diagnostics.contains(diagnostic))
            m_progress.diagnostics.append(diagnostic);
        const ExportTypeProgress snapshot = m_progress;  // snapshot：发送给界面的清单诊断进度快照。
        locker.unlock();
        emit progressChanged(snapshot);
    };

    const QString libName =
        m_options.libName.isEmpty() ? QStringLiteral("EasyKiConverter") : m_options.libName;  ///< 输出库名称。
    QString outputDir = m_options.outputPath;  ///< 三维模型输出目录。
    if (outputDir.isEmpty())
        outputDir = QDir::currentPath() + QStringLiteral("/export");
    outputDir += QDir::separator() + libName + QStringLiteral(".3dmodels");

    const QString manifestPath = outputDir + QDir::separator() + QStringLiteral("manifest.json");  ///< 关联清单路径。
    const bool manifestExists = QFileInfo::exists(manifestPath);  ///< 旧清单是否存在。
    QJsonObject root;  ///< 关联清单根 JSON 对象。
    if (manifestExists) {
        // existingManifestFile：读取旧清单的文件对象。
        QFile existingManifestFile(manifestPath);
        if (!existingManifestFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            reportManifestDiagnostic(QStringLiteral("无法读取已有 3D 关联清单：%1").arg(manifestPath));
            return;
        }
        // parseError：旧清单 JSON 解析错误信息。
        QJsonParseError parseError;
        // existingManifestDocument：解析后的旧清单文档。
        const QJsonDocument existingManifestDocument =
            QJsonDocument::fromJson(existingManifestFile.readAll(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !existingManifestDocument.isObject()) {
            reportManifestDiagnostic(QStringLiteral("已有 3D 关联清单格式无效，已保留原文件：%1").arg(manifestPath));
            return;
        }
        root = existingManifestDocument.object();
    }

    // components：合并后的组件关联信息数组。
    QJsonArray components = root.value(QStringLiteral("components")).toArray();
    // componentIndexes：已有组件编号到清单数组位置的映射。
    QHash<QString, int> componentIndexes;
    for (int index = 0; index < components.size(); ++index) {
        // existingComponent：旧清单中的当前组件对象。
        const QJsonObject existingComponent = components.at(index).toObject();
        const QString componentId =
            existingComponent.value(QStringLiteral("componentId")).toString();  // componentId：旧组件编号。
        if (!componentId.isEmpty() && !componentIndexes.contains(componentId))
            componentIndexes.insert(componentId, index);
    }

    const ExportTypeProgress progress = getProgress();  ///< 当前导出进度快照。
    const auto vectorToJson = [](const IR::Model3DVec3& vector) {
        QJsonObject value;  ///< 三维向量 JSON 对象。
        value.insert(QStringLiteral("x"), vector.x);
        value.insert(QStringLiteral("y"), vector.y);
        value.insert(QStringLiteral("z"), vector.z);
        return value;
    };
    // hasSuccessfulUpdate：本轮是否至少有一个组件成功生成并需要更新清单。
    bool hasSuccessfulUpdate = false;
    for (const QString& componentId : m_componentIds) {  // 当前元器件编号。
        // status：当前组件的导出状态。
        const ExportItemStatus status = progress.itemStatus.value(componentId);
        if (status.status != ExportItemStatus::Status::Success)
            continue;

        // modelStem：当前成功组件的模型文件主体名。
        const QString modelStem = m_modelFileStems.value(componentId);
        // files：当前成功生成且实际存在的模型文件映射。
        QJsonObject files;
        // allFilesPresent：当前清单要求的模型文件是否全部已经落盘。
        bool allFilesPresent = true;
        if (m_options.needsModel3DWrl()) {
            const QString relativePath = modelStem + QStringLiteral(".wrl");  ///< WRL 相对路径。
            if (QFileInfo::exists(outputDir + QDir::separator() + relativePath))
                files.insert(QStringLiteral("wrl"), relativePath);
            else
                allFilesPresent = false;
        }
        if (m_options.needsModel3DStep()) {
            const QString relativePath = modelStem + QStringLiteral(".step");  ///< STEP 相对路径。
            if (QFileInfo::exists(outputDir + QDir::separator() + relativePath))
                files.insert(QStringLiteral("step"), relativePath);
            else
                allFilesPresent = false;
        }
        if (!allFilesPresent) {
            reportManifestDiagnostic(
                QStringLiteral("组件 %1 导出成功但模型文件不完整，未更新 3D 关联清单").arg(componentId));
            continue;
        }

        QJsonObject componentObject;  ///< 当前元器件关联信息对象。
        componentObject.insert(QStringLiteral("componentId"), componentId);
        componentObject.insert(QStringLiteral("modelStem"), modelStem);

        const auto data = m_cachedData.value(componentId);  ///< 当前元器件缓存数据。
        if (data) {
            if (data->symbolData())
                componentObject.insert(QStringLiteral("symbol"), data->symbolData()->info().name);
            if (data->footprintData())
                componentObject.insert(QStringLiteral("footprint"), data->footprintData()->info().name);

            Model3DData model;  ///< 当前元器件的三维模型描述。
            if (data->model3DData())
                model = *data->model3DData();
            if (model.uuid().isEmpty() && data->footprintData())
                model = data->footprintData()->model3D();
            if (!model.uuid().isEmpty() || !model.name().isEmpty()) {
                const IR::Model3DIR modelIr = IR::toModel3DIR(model);  ///< 三维模型 IR。
                QJsonObject modelObject;  ///< 三维模型 JSON 对象。
                modelObject.insert(QStringLiteral("name"), model.name());
                modelObject.insert(QStringLiteral("uuid"), model.uuid());
                modelObject.insert(QStringLiteral("translationMm"), vectorToJson(modelIr.translation()));
                modelObject.insert(QStringLiteral("rotationDeg"), vectorToJson(modelIr.rotation()));
                modelObject.insert(QStringLiteral("stepOffsetMm"), vectorToJson(modelIr.stepOffsetMm()));
                componentObject.insert(QStringLiteral("model"), modelObject);
            }
        }

        componentObject.insert(QStringLiteral("status"), QStringLiteral("success"));
        componentObject.insert(QStringLiteral("files"), files);
        if (componentIndexes.contains(componentId))
            components.replace(componentIndexes.value(componentId), componentObject);
        else {
            componentIndexes.insert(componentId, components.size());
            components.append(componentObject);
        }
        hasSuccessfulUpdate = true;
    }

    if (!hasSuccessfulUpdate)
        return;

    root.insert(QStringLiteral("format"), QStringLiteral("EasyKiConverter.3d-model-manifest"));
    root.insert(QStringLiteral("version"), 1);
    root.insert(QStringLiteral("library"), libName);
    root.insert(QStringLiteral("targetFormat"), static_cast<int>(m_options.targetFormat));
    root.insert(QStringLiteral("components"), components);

    QSaveFile manifest(manifestPath);  ///< 原子写入关联清单的文件对象。
    // serializedManifest：待原子提交的清单 JSON 内容。
    const QByteArray serializedManifest = QJsonDocument(root).toJson(QJsonDocument::Indented);
    if (!manifest.open(QIODevice::WriteOnly | QIODevice::Text) ||
        manifest.write(serializedManifest) != serializedManifest.size() || !manifest.commit())
        reportManifestDiagnostic(QStringLiteral("无法写入 3D 关联清单，原文件保持不变：%1").arg(manifestPath));
}

}  // namespace EasyKiConverter
