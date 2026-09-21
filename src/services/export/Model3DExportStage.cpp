#include "Model3DExportStage.h"

#include "Model3DExportWorker.h"
#include "core/kicad/Exporter3DModel.h"
#include "models/ComponentData.h"
#include "services/ComponentCacheService.h"
#include "utils/PathSecurity.h"

#include <QDebug>
#include <QDir>
#include <QPointer>

namespace EasyKiConverter {

Model3DExportStage::Model3DExportStage(QObject* parent) : ExportTypeStage("Model3D", 2, parent) {}

Model3DExportStage::~Model3DExportStage() {
    cancel();
}

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

    QString libName = m_options.libName.isEmpty() ? QStringLiteral("EasyKiConverter") : m_options.libName;
    QString baseOutputDir = m_options.outputPath;
    if (baseOutputDir.isEmpty()) {
        baseOutputDir = QDir::currentPath() + QStringLiteral("/export");
    }
    QString outputDir = baseOutputDir + QDir::separator() + libName + QStringLiteral(".3dmodels");

    QDir dir;
    const bool needWrl = m_options.needsModel3DWrl();
    const bool needStep = m_options.needsModel3DStep();

    qInfo() << "Model3DExportStage::start() - exportModel3DFormat:" << m_options.exportModel3DFormat
            << "needWrl:" << needWrl << "needStep:" << needStep;

    const auto hasModel3DUuid = [&cachedData](const QString& componentId) {
        auto it = cachedData.constFind(componentId);
        if (it != cachedData.constEnd() && it.value()) {
            if (it.value()->model3DData() && !it.value()->model3DData()->uuid().isEmpty())
                return true;
            if (it.value()->footprintData() && !it.value()->footprintData()->model3D().uuid().isEmpty())
                return true;
        }

        QSharedPointer<ComponentData> cachedComponent =
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
    QSet<QString> usedModelStems;
    for (const QString& componentId : componentIds) {
        QString modelName;
        const auto cachedIt = cachedData.constFind(componentId);
        if (cachedIt != cachedData.cend() && cachedIt.value()) {
            const auto& data = cachedIt.value();
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

        const QString baseName = modelName;
        int suffix = 2;
        while (usedModelStems.contains(modelName.toCaseFolded()))
            modelName = QStringLiteral("%1_%2").arg(baseName).arg(suffix++);
        usedModelStems.insert(modelName.toCaseFolded());
        m_modelFileStems.insert(componentId, modelName);
    }

    // 输出目录创建失败时仍然交给基类建立逐项状态，避免主服务留下 Pending 项。
    if (!dir.exists(outputDir) && !dir.mkpath(outputDir)) {
        qCritical() << "Model3DExportStage: Failed to create output directory:" << outputDir;
        const QString error = QStringLiteral("Failed to create 3D model output directory");
        for (const QString& componentId : componentIds)
            m_preflightErrors.insert(componentId, error);
        m_isExporting.store(true);
        ExportTypeStage::start(componentIds, cachedData);
        return;
    }

    m_tempManager.setOutputPath(outputDir);

    for (const QString& componentId : componentIds) {
        if (!needWrl && !needStep) {
            m_skippedComponents.insert(componentId);
            continue;
        }
        if ((needWrl || needStep) && !hasModel3DUuid(componentId)) {
            qDebug() << "Model3DExportStage: No 3D model UUID, skipping temp paths for" << componentId;
            m_skippedComponents.insert(componentId);
            continue;
        }

        TempFilePaths paths;
        if (needWrl) {
            paths.wrlTempPath =
                m_tempManager.createTempFilePath(componentId + QStringLiteral("_wrl"), QStringLiteral(".wrl"));
        }
        if (needStep) {
            paths.stepTempPath =
                m_tempManager.createTempFilePath(componentId + QStringLiteral("_step"), QStringLiteral(".step"));
        }

        const bool hasAllRequiredTempPaths =
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

    // Paths must be ready before workers read m_componentPaths.
    ExportTypeStage::start(componentIds, cachedData);
}

// 取消三维模型导出并回滚尚未提交的临时文件。
void Model3DExportStage::cancel() {
    if (!m_isRunning.load() && !m_isExporting.load()) {
        return;
    }

    qDebug() << "Model3DExportStage: cancelling...";
    m_cancelled.store(true);

    bool hasActiveWorkers = false;
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

// 创建三维模型导出 Worker。
QObject* Model3DExportStage::createWorker() {
    return new Model3DExportWorker();
}

// 为有有效三维数据的元器件准备输出路径并提交 Worker。
void Model3DExportStage::startWorker(QObject* worker,
                                     const QString& componentId,
                                     const QSharedPointer<ComponentData>& data) {
    auto* exportWorker = qobject_cast<Model3DExportWorker*>(worker);
    if (!exportWorker) {
        qWarning() << "Model3DExportStage: Failed to cast worker to Model3DExportWorker";
        return;
    }

    if (m_skippedComponents.contains(componentId)) {
        completeSkippedItemProgress(exportWorker, componentId, QStringLiteral("No 3D model or format selected"));
        delete exportWorker;
        return;
    }

    const auto preflightError = m_preflightErrors.constFind(componentId);
    if (preflightError != m_preflightErrors.cend()) {
        completeItemProgress(exportWorker, componentId, false, preflightError.value());
        delete exportWorker;
        return;
    }

    exportWorker->setOptions(m_options);
    exportWorker->setData(componentId, data, m_options);

    const QString modelName = m_modelFileStems.value(componentId, PathSecurity::sanitizeFilename(componentId));

    const bool needWrl = m_options.needsModel3DWrl();
    const bool needStep = m_options.needsModel3DStep();
    QString libName = m_options.libName.isEmpty() ? QStringLiteral("EasyKiConverter") : m_options.libName;
    QString baseOutputDir = m_options.outputPath;
    if (baseOutputDir.isEmpty()) {
        baseOutputDir = QDir::currentPath() + QStringLiteral("/export");
    }
    QString outputDir = baseOutputDir + QDir::separator() + libName + QStringLiteral(".3dmodels");

    if (m_componentPaths.contains(componentId)) {
        TempFilePaths& paths = m_componentPaths[componentId];
        paths.wrlFinalPath = needWrl ? (outputDir + QDir::separator() + modelName + QStringLiteral(".wrl")) : QString();
        paths.stepFinalPath =
            needStep ? (outputDir + QDir::separator() + modelName + QStringLiteral(".step")) : QString();
        exportWorker->setOutputPaths({paths.wrlTempPath, paths.stepTempPath});
    }

    QPointer<Model3DExportStage> stagePtr(this);
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
                const TempFilePaths paths = stagePtr->m_componentPaths.value(componentId);
                const bool needWrl = stagePtr->m_options.needsModel3DWrl();
                const bool needStep = stagePtr->m_options.needsModel3DStep();

                QVector<TempFileManager::CommitItem> commitItems;
                if (needWrl) {
                    commitItems.append({paths.wrlTempPath, paths.wrlFinalPath, false});
                }
                if (needStep) {
                    commitItems.append({paths.stepTempPath, paths.stepFinalPath, false});
                }
                if (!stagePtr->m_tempManager.commitBatch(commitItems)) {
                    success = false;
                }
            }

            stagePtr->completeItemProgress(exportWorker, componentId, success, error);
            exportWorker->deleteLater();
        },
        Qt::QueuedConnection);

    m_threadPool.start(exportWorker);
}

}  // namespace EasyKiConverter
