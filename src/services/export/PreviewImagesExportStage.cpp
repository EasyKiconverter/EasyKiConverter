#include "PreviewImagesExportStage.h"

#include "PreviewImagesExportWorker.h"
#include "models/ComponentData.h"

#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QPointer>

namespace EasyKiConverter {

/**
 * @brief 初始化预览图导出阶段及其并发度。
 * @param parent QObject 父对象。
 */
PreviewImagesExportStage::PreviewImagesExportStage(QObject* parent) : ExportTypeStage("PreviewImages", 4, parent) {}

PreviewImagesExportStage::~PreviewImagesExportStage() {
    cancel();
}

/**
 * @brief 准备预览图输出目录并启动导出任务。
 * @param componentIds 待导出的元器件编号列表。
 * @param cachedData 已预加载的元器件数据。
 */
void PreviewImagesExportStage::start(const QStringList& componentIds,
                                     const QMap<QString, QSharedPointer<ComponentData>>& cachedData) {
    if (m_isExporting.load()) {
        qWarning() << "PreviewImagesExportStage: Export already in progress";
        return;
    }

    m_threadPool.setMaxThreadCount(m_options.weakNetworkSupport ? 2 : 4);

    QString libName =
        m_options.libName.isEmpty() ? QStringLiteral("EasyKiConverter") : m_options.libName;  ///< 库名称。
    QString baseOutputDir = m_options.outputPath;  ///< 用户指定的输出根目录。
    if (baseOutputDir.isEmpty()) {
        baseOutputDir = QDir::currentPath() + QStringLiteral("/export");
    }
    QString outputDir = baseOutputDir + QDir::separator() + libName + QStringLiteral(".preview");  ///< 预览图输出目录。

    QDir dir;  ///< 用于创建预览图输出目录的目录对象。
    if (!dir.mkpath(outputDir)) {
        qCritical() << "PreviewImagesExportStage: Failed to create output directory:" << outputDir;
        m_isExporting.store(false);
        ExportTypeStage::start(QStringList(), cachedData);
        return;
    }

    m_tempManager.setOutputPath(outputDir);

    m_outputDirs.clear();
    m_componentTempPaths.clear();
    for (const QString& componentId : componentIds) {  // 当前元器件编号。
        m_outputDirs[componentId] = outputDir;
    }

    m_isExporting.store(true);

    ExportTypeStage::start(componentIds, cachedData);
}

/**
 * @brief 取消正在执行的预览图导出。
 */
void PreviewImagesExportStage::cancel() {
    if (!m_isExporting.load()) {
        return;
    }

    qDebug() << "PreviewImagesExportStage: Cancelling...";
    m_cancelled.store(true);
    m_tempManager.rollbackAll();
    m_isExporting.store(false);
    qDebug() << "PreviewImagesExportStage: Cancelled";
}

QMap<QString, QString> PreviewImagesExportStage::createTempPathsForComponent(const QString& componentId,
                                                                             int previewCount) {
    QMap<QString, QString> paths;  ///< 文件名到临时路径的映射。

    if (previewCount <= 0) {
        qWarning() << "PreviewImagesExportStage: No preview images to export for" << componentId;
        return paths;
    }

    QString outputDir = m_outputDirs.value(componentId);  ///< 当前元器件的最终输出目录。
    if (outputDir.isEmpty()) {
        qWarning() << "PreviewImagesExportStage: No output directory for" << componentId;
        return paths;
    }

    QString tempDir =
        m_tempManager.createTempDirectoryPath(componentId + QStringLiteral(".preview.tmp"));  ///< 临时目录路径。
    if (tempDir.isEmpty()) {
        qWarning() << "PreviewImagesExportStage: Failed to create temp dir path for" << componentId;
        return paths;
    }
    QDir dir;  ///< 用于创建临时目录的目录对象。
    if (!dir.mkpath(tempDir)) {
        qWarning() << "PreviewImagesExportStage: Failed to create temp dir for" << componentId << ":" << tempDir;
        return paths;
    }

    for (int i = 1; i <= previewCount; ++i) {  // 当前预览图序号。
        QString fileName = QStringLiteral("%1_preview_%2.png").arg(componentId).arg(i);  ///< 预览图文件名。
        QString tempPath = tempDir + QDir::separator() + fileName;  ///< 预览图临时文件路径。
        paths[fileName] = tempPath;
    }
    m_componentTempPaths[componentId] = paths;

    if (paths.size() != previewCount) {
        qWarning() << "PreviewImagesExportStage: Created" << paths.size() << "paths but expected" << previewCount;
    }

    return paths;
}

/**
 * @brief 创建预览图导出 Worker。
 * @return 新建的预览图导出 Worker。
 */
QObject* PreviewImagesExportStage::createWorker() {
    return new PreviewImagesExportWorker();
}

/**
 * @brief 配置并提交一个预览图导出 Worker。
 * @param worker 待配置的 Worker。
 * @param componentId 元器件编号。
 * @param data 预加载的元器件数据。
 */
void PreviewImagesExportStage::startWorker(QObject* worker,
                                           const QString& componentId,
                                           const QSharedPointer<ComponentData>& data) {
    auto* exportWorker = qobject_cast<PreviewImagesExportWorker*>(worker);  ///< 转换后的预览图 Worker。
    if (!exportWorker) {
        qWarning() << "PreviewImagesExportStage: Failed to cast worker to PreviewImagesExportWorker";
        return;
    }

    exportWorker->setOptions(m_options);
    exportWorker->setData(componentId, data, m_options);

    int previewCount = data->previewImageData().size();  ///< 当前元器件的预览图数量。
    QMap<QString, QString> tempPaths = createTempPathsForComponent(componentId, previewCount);  ///< 临时路径映射。
    if (!tempPaths.isEmpty()) {
        exportWorker->setTempPaths(tempPaths);
    } else if (previewCount > 0) {
        qWarning() << "PreviewImagesExportStage: Temp path creation failed for" << componentId
                   << "- will use direct write";
    }

    QPointer<PreviewImagesExportStage> stagePtr(this);  ///< 防止异步回调访问已销毁的阶段对象。
    connect(
        exportWorker,
        &PreviewImagesExportWorker::completed,
        this,
        [stagePtr, exportWorker, componentId](const QString&, bool success, const QString& error) {
            if (!stagePtr) {
                exportWorker->deleteLater();
                return;
            }

            if (success && !stagePtr->m_cancelled.load()) {
                QString outputDir = stagePtr->m_outputDirs.value(componentId);  ///< 当前元器件的最终输出目录。
                const QMap<QString, QString> tempPaths =
                    stagePtr->m_componentTempPaths.value(componentId);  ///< 当前元器件的临时路径映射。

                QVector<TempFileManager::CommitItem> commitItems;  ///< 待提交的临时文件事务项。
                for (auto it = tempPaths.constBegin(); it != tempPaths.constEnd(); ++it) {  // 当前临时文件映射。
                    commitItems.append({it.value(), outputDir + QDir::separator() + it.key(), false});
                }

                if (!commitItems.isEmpty()) {
                    success = stagePtr->m_tempManager.commitBatch(commitItems);
                    const QString tempDirPath =
                        QFileInfo(commitItems.first().tempPath).absolutePath();  ///< 临时目录路径。
                    QDir(tempDirPath).removeRecursively();
                }
            }

            stagePtr->completeItemProgress(exportWorker, componentId, success, error);
            exportWorker->deleteLater();
        },
        Qt::QueuedConnection);

    m_threadPool.start(exportWorker);
}

}  // namespace EasyKiConverter
