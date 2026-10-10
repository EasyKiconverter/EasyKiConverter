#include "Model3DExportWorker.h"

#include "../../core/kicad/Exporter3DModel.h"
#include "../../models/ComponentData.h"
#include "../../services/ComponentCacheService.h"
#include "../../utils/PathSecurity.h"
#include "DebugExportHelper.h"
#include "core/ir/Model3DDataConverter.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QMutexLocker>
#include <QtConcurrent>

namespace EasyKiConverter {

namespace {

/**
 * @brief 返回模型下载临界区，避免相同 UUID 并发穿透缓存。
 * @return 进程内共享的模型下载互斥量。
 */
QMutex& modelDownloadMutex() {
    static QMutex mutex;  ///< 保护相同模型 UUID 下载操作的进程内互斥量。
    return mutex;
}

}  // namespace

Model3DExportWorker::Model3DExportWorker(QObject* parent) : QObject(parent) {
    setAutoDelete(false);
}

Model3DExportWorker::~Model3DExportWorker() = default;

/**
 * @brief 设置三维模型导出所需的数据。
 * @param componentId 元器件编号。
 * @param data 预加载的元器件数据。
 * @param options 导出选项。
 */
void Model3DExportWorker::setData(const QString& componentId,
                                  const QSharedPointer<ComponentData>& data,
                                  const struct ExportOptions& options) {
    m_componentId = componentId;
    m_data = data;
    m_options = options;
    m_cacheGeneration = ComponentCacheService::instance()->currentGeneration();
}

/**
 * @brief 更新 Worker 使用的三维导出选项。
 * @param options 处理选项。
 */
void Model3DExportWorker::setOptions(const struct ExportOptions& options) {
    m_options = options;
}

/**
 * @brief 从组件数据或缓存解析模型并完成选定格式的导出。
 */
void Model3DExportWorker::run() {
    if (m_cancelled.load()) {
        emit completed(m_componentId, false, QStringLiteral("Cancelled"));
        return;
    }
    const uint64_t gen = m_cacheGeneration;  ///< 当前任务对应的缓存代次。

    if (!m_data) {
        emit completed(m_componentId, false, QStringLiteral("No data available"));
        return;
    }

    qDebug() << "Model3DExportWorker: Exporting" << m_componentId;

    Model3DData sourceModel;  ///< 当前元器件解析出的三维模型描述。
    if (m_data->model3DData())
        sourceModel = *m_data->model3DData();
    if (sourceModel.uuid().isEmpty() && m_data->footprintData()) {
        const Model3DData footprintModel = m_data->footprintData()->model3D();  ///< 封装关联的三维模型描述。
        if (!footprintModel.uuid().isEmpty())
            sourceModel = footprintModel;
    }

    QString uuid = sourceModel.uuid();  ///< 当前三维模型 UUID。
    if (uuid.isEmpty()) {
        QSharedPointer<ComponentData> cachedComponent =  // 从磁盘缓存读取的元器件数据。
            ComponentCacheService::instance()->loadComponentData(m_componentId);
        if (cachedComponent) {
            if (cachedComponent->model3DData()) {
                sourceModel = *cachedComponent->model3DData();
            }
            if (sourceModel.uuid().isEmpty() && cachedComponent->footprintData()) {
                sourceModel = cachedComponent->footprintData()->model3D();
            }
            uuid = sourceModel.uuid();
        }
    }
    if (uuid.isEmpty()) {
        qDebug() << "Model3DExportWorker: 3D model UUID not available, skipping" << m_componentId;
        emit completed(m_componentId, true, QStringLiteral("3D model UUID not available, skipped"));
        return;
    }

    // 根据选项决定是否导出 WRL 和 STEP
    const bool needWrl = m_options.needsModel3DWrl();  ///< 是否需要导出 WRL。
    const bool needStep = m_options.needsModel3DStep();  ///< 是否需要导出 STEP。

    qInfo() << "Model3DExportWorker::run() for" << m_componentId
            << "exportModel3DFormat:" << m_options.exportModel3DFormat << "needWrl:" << needWrl
            << "needStep:" << needStep;

    // 如果两种格式都不需要导出，直接完成
    if (!needWrl && !needStep) {
        qDebug() << "Model3DExportWorker: No format selected, skipping" << m_componentId;
        emit completed(m_componentId, true, QString());
        return;
    }

    // [NOTE] 直接使用 Exporter3DModel 而非 ExporterFactory，因为 Worker 依赖其
    // downloadObjDataSync/downloadStepDataSync 等 EasyEDA 特有方法，
    // 这些方法不在 IModel3DExporter 通用接口中。
    Exporter3DModel exporter;  ///< 用于下载和转换三维模型的导出器。

    // 获取模型名称用于命名文件（仅在需要时）
    QString modelName = sourceModel.name();  ///< 输出文件使用的模型名称。
    if (modelName.isEmpty()) {
        modelName = m_componentId;
    } else {
        // 使用 PathSecurity 清理文件名中的非法字符
        modelName = PathSecurity::sanitizeFilename(modelName);
    }

    // 构建输出路径
    QString outputDir = m_options.outputPath;  ///< 三维模型输出目录。
    if (outputDir.isEmpty()) {
        outputDir =
            QDir(QDir(QDir::currentPath()).filePath(QStringLiteral("export"))).filePath(QStringLiteral("3dmodels"));
    }

    QDir dir;  ///< 用于创建输出目录的目录对象。
    if (!dir.mkpath(outputDir)) {
        emit completed(m_componentId, false, QStringLiteral("Failed to create output directory"));
        return;
    }

    const QDir outputDirectory(outputDir);  ///< 三维模型输出目录对象。
    const QString wrlFinalPath =
        needWrl ? outputDirectory.filePath(modelName + QStringLiteral(".wrl")) : QString();  ///< WRL 最终路径。
    const QString stepFinalPath =
        needStep ? outputDirectory.filePath(modelName + QStringLiteral(".step")) : QString();  ///< STEP 最终路径。
    const QString wrlWritePath =
        m_outputPaths.wrlTempPath.isEmpty() ? wrlFinalPath : m_outputPaths.wrlTempPath;  ///< WRL 实际写入路径。
    const QString stepWritePath =
        m_outputPaths.stepTempPath.isEmpty() ? stepFinalPath : m_outputPaths.stepTempPath;  ///< STEP 实际写入路径。

    const auto ensureParentDir = [](const QString& filePath) {
        if (filePath.isEmpty())
            return true;
        QFileInfo fileInfo(filePath);  ///< 当前输出文件路径信息。
        return fileInfo.absoluteDir().exists() || QDir().mkpath(fileInfo.absolutePath());
    };

    if ((needWrl && !ensureParentDir(wrlWritePath)) || (needStep && !ensureParentDir(stepWritePath))) {
        emit completed(m_componentId, false, QStringLiteral("Failed to create 3D model output directory"));
        return;
    }

    if (!m_options.overwriteExistingFiles && m_outputPaths.wrlTempPath.isEmpty() &&
        m_outputPaths.stepTempPath.isEmpty()) {
        bool bothExist = true;  ///< 所有请求的最终模型文件是否都已存在。
        if (needWrl && !QFile::exists(wrlFinalPath))
            bothExist = false;
        if (needStep && !QFile::exists(stepFinalPath))
            bothExist = false;
        if (bothExist && (needWrl || needStep)) {
            qDebug() << "Model3DExportWorker: Files already exist, skipping" << wrlFinalPath << stepFinalPath;
            emit completed(m_componentId, true, QString());
            return;
        }
    }

    ComponentCacheService* cache = ComponentCacheService::instance();  ///< 组件缓存服务实例。

    // WRL 是由 OBJ 转换得到的派生文件；有 OBJ 时重新生成，避免旧 WRL 缓存携带过期归一化。
    // 只有在没有 OBJ 且不能下载时才复用 WRL 缓存，封装阶段会按该 WRL 的实际几何计算 STEP 偏移。
    // STEP 文件保持服务器原始坐标系，不使用派生缓存。

    auto buildModelData = [&uuid, &sourceModel]() {
        Model3DData modelData;  ///< 用于转换为目标模型格式的临时模型描述。
        modelData.setUuid(uuid);
        modelData.setName(sourceModel.name());
        modelData.setTranslation(sourceModel.translation());
        modelData.setRotation(sourceModel.rotation());
        return modelData;
    };

    QString error;  ///< 当前三维模型导出的错误信息。
    // 只在需要 WRL 时导出
    if (needWrl) {
        QByteArray objData;  ///< OBJ 原始模型数据。
        bool usedWrlCache = false;  ///< 是否直接复用了缓存中的 WRL。
        if (m_data && !m_data->model3DObjRaw().isEmpty()) {
            objData = m_data->model3DObjRaw();
        }
        if (!objData.isEmpty() && !Exporter3DModel::hasUsableObjGeometry(objData)) {
            qWarning() << "Model3DExportWorker: Ignoring malformed OBJ data for" << m_componentId << "uuid" << uuid;
            objData.clear();
        }
        if (!objData.isEmpty()) {
            // 预加载阶段已经取得有效 OBJ 时也写入公共缓存，避免后续导出重复下载。
            cache->saveModel3D(uuid, objData, QStringLiteral("obj"), gen);
        }
        if (objData.isEmpty()) {
            QMutexLocker downloadLocker(&modelDownloadMutex());  ///< 保护模型缓存读取和下载的锁。
            objData = cache->loadModel3D(uuid, QStringLiteral("obj"));
            if (!objData.isEmpty() && !Exporter3DModel::hasUsableObjGeometry(objData)) {
                qWarning() << "Model3DExportWorker: Ignoring malformed cached OBJ for" << m_componentId << "uuid"
                           << uuid;
                objData.clear();
            }
            const QByteArray cachedWrl = cache->loadModel3D(uuid, QStringLiteral("wrl"));  ///< 缓存中的 WRL 数据。
            if (objData.isEmpty() && !cachedWrl.isEmpty() && Exporter3DModel::hasUsableWrlGeometry(cachedWrl) &&
                cache->copyModel3DToFile(uuid, QStringLiteral("wrl"), wrlWritePath)) {
                usedWrlCache = true;
                qDebug() << "Model3DExportWorker: WRL cache fallback for" << uuid;
            } else if (objData.isEmpty() && !cachedWrl.isEmpty()) {
                qWarning() << "Model3DExportWorker: Ignoring malformed cached WRL for" << m_componentId << "uuid"
                           << uuid;
            }
            if (objData.isEmpty() && !usedWrlCache && !exporter.downloadObjDataSync(uuid, &objData, &error)) {
                if (error.isEmpty()) {
                    error = QStringLiteral("Failed to download OBJ data for WRL export");
                }
            }
            if (!objData.isEmpty() && !Exporter3DModel::hasUsableObjGeometry(objData)) {
                qWarning() << "Model3DExportWorker: Rejecting invalid downloaded OBJ for" << m_componentId << "uuid"
                           << uuid;
                objData.clear();
                error = QStringLiteral("Downloaded OBJ data has no usable geometry");
            }
            if (!objData.isEmpty()) {
                cache->saveModel3D(uuid, objData, QStringLiteral("obj"), gen);
            }
        }
        if (usedWrlCache) {
            // 已复制缓存中的 WRL，无需再次转换 OBJ。
        } else if (objData.isEmpty()) {
            error = QStringLiteral("OBJ data is empty for WRL export");
        } else {
            Model3DData modelData = buildModelData();  ///< WRL 转换用模型描述。
            modelData.setRawObj(QString::fromUtf8(objData));
            IR::Model3DIR modelIR = IR::toModel3DIR(modelData);  ///< WRL 导出用三维模型 IR。
            if (!exporter.exportToWrl(modelIR, wrlWritePath)) {
                error = QStringLiteral("Failed to convert OBJ to WRL");
            } else {
                QFile file(wrlWritePath);  ///< WRL 缓存读取文件对象。
                if (file.open(QIODevice::ReadOnly)) {
                    cache->saveModel3D(uuid, file.readAll(), QStringLiteral("wrl"), gen);
                    qDebug() << "Model3DExportWorker: Saved WRL to cache for" << m_componentId << "uuid" << uuid;
                }
            }
        }
    }
    // STEP 文件保持服务器原始坐标系，优先使用磁盘缓存
    if (error.isEmpty() && needStep && !m_cancelled.load()) {
        QByteArray stepData = sourceModel.step();  ///< STEP 原始模型数据。
        if (!stepData.isEmpty() && !Exporter3DModel::hasUsableStepData(stepData)) {
            qWarning() << "Model3DExportWorker: Ignoring malformed preloaded STEP for" << m_componentId << "uuid"
                       << uuid;
            stepData.clear();
        }
        if (!stepData.isEmpty()) {
            // 预加载阶段已经取得有效 STEP 时写入公共缓存，避免后续导出重复下载。
            cache->saveModel3D(uuid, stepData, QStringLiteral("step"), gen);
        }

        bool stepCacheHit = false;  ///< 是否命中并复制了 STEP 缓存。
        if (stepData.isEmpty()) {
            QMutexLocker downloadLocker(&modelDownloadMutex());  ///< 保护模型缓存读取和下载的锁。
            const QByteArray cachedStep = cache->loadModel3D(uuid, QStringLiteral("step"));  ///< 缓存中的 STEP 数据。
            if (!cachedStep.isEmpty() && Exporter3DModel::hasUsableStepData(cachedStep)) {
                stepCacheHit = cache->copyModel3DToFile(uuid, QStringLiteral("step"), stepWritePath);
            } else if (!cachedStep.isEmpty()) {
                qWarning() << "Model3DExportWorker: Ignoring malformed cached STEP for" << m_componentId << "uuid"
                           << uuid;
            }
            if (!stepCacheHit && !m_cancelled.load()) {
                if (!exporter.downloadStepDataSync(uuid, &stepData, &error) && error.isEmpty()) {
                    error = QStringLiteral("Failed to download STEP data");
                }
                if (!stepData.isEmpty() && !Exporter3DModel::hasUsableStepData(stepData)) {
                    qWarning() << "Model3DExportWorker: Rejecting invalid downloaded STEP for" << m_componentId
                               << "uuid" << uuid;
                    stepData.clear();
                    error = QStringLiteral("Downloaded STEP data has invalid structure");
                }
                if (!stepData.isEmpty()) {
                    cache->saveModel3D(uuid, stepData, QStringLiteral("step"), gen);
                }
            }
        }

        if (stepCacheHit) {
            qDebug() << "Model3DExportWorker: STEP cache hit for" << m_componentId << "uuid" << uuid;
        } else if (!stepData.isEmpty()) {
            Model3DData modelData = buildModelData();  ///< STEP 转换用模型描述。
            modelData.setStep(stepData);
            IR::Model3DIR modelIR = IR::toModel3DIR(modelData);  ///< STEP 导出用三维模型 IR。
            if (!exporter.exportToStep(modelIR, stepWritePath)) {
                error = QStringLiteral("Failed to write STEP file");
            }
        }
    }

    if (m_cancelled.load() && error.isEmpty()) {
        error = QStringLiteral("Cancelled");
    }

    if (error.isEmpty()) {
        qDebug() << "Model3DExportWorker: Successfully exported" << (needWrl ? "WRL" : "") << (needStep ? "STEP" : "")
                 << "for" << m_componentId;

        if (m_options.debugMode) {
            (void)QtConcurrent::run([componentId = m_componentId, data = m_data, outputPath = m_options.outputPath]() {
                DebugExportHelper::exportDebugData(componentId, data, outputPath);
            });
        }

        emit completed(m_componentId, true, QString());
        return;
    }

    qCritical() << "Model3DExportWorker: Export failed for" << m_componentId << "uuid" << uuid << "error:" << error;
    emit completed(m_componentId, false, error);
}

/**
 * @brief 将下载错误转换为统一的 Worker 完成失败信号。
 * @param error 错误信息。
 */
void Model3DExportWorker::onDownloadError(const QString& error) {
    qCritical("%s", qPrintable(QString("Model3DExportWorker: Download error: %1").arg(error)));
    emit completed(m_componentId, false, error);
}

/**
 * @brief 设置取消标志，让正在执行的模型任务尽快停止。
 */
void Model3DExportWorker::cancel() {
    m_cancelled.store(true);
}

}  // namespace EasyKiConverter
