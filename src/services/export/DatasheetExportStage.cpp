#include "DatasheetExportStage.h"

#include "DatasheetExportWorker.h"
#include "models/ComponentData.h"

#include <QDebug>
#include <QDir>
#include <QPointer>

namespace EasyKiConverter {

DatasheetExportStage::DatasheetExportStage(QObject* parent) : ExportTypeStage("Datasheet", 2, parent) {}

DatasheetExportStage::~DatasheetExportStage() {
    cancel();
}

/**
 * @brief 初始化并启动数据手册导出阶段。
 * @param componentIds 待导出的元器件编号列表。
 * @param cachedData 已预加载的元器件数据。
 */
void DatasheetExportStage::start(const QStringList& componentIds,
                                 const QMap<QString, QSharedPointer<ComponentData>>& cachedData) {
    if (m_isExporting.load()) {
        qWarning() << "DatasheetExportStage: Export already in progress";
        return;
    }

    m_threadPool.setMaxThreadCount(m_options.weakNetworkSupport ? 1 : 2);

    QString libName =
        m_options.libName.isEmpty() ? QStringLiteral("EasyKiConverter") : m_options.libName;  ///< 数据手册库名称。
    QString baseOutputDir = m_options.outputPath;  ///< 用户指定的基础输出目录。
    if (baseOutputDir.isEmpty()) {
        baseOutputDir = QDir::currentPath() + QStringLiteral("/export");
    }
    QString outputDir =
        baseOutputDir + QDir::separator() + libName + QStringLiteral(".datasheet");  ///< 数据手册输出目录。

    QDir dir;  ///< 用于创建输出目录的目录对象。
    if (!dir.mkpath(outputDir)) {
        qCritical() << "DatasheetExportStage: Failed to create output directory:" << outputDir;
        m_isExporting.store(false);
        ExportTypeStage::start(QStringList(), cachedData);
        return;
    }

    m_tempManager.setOutputPath(outputDir);

    m_tempPaths.clear();
    m_finalPaths.clear();
    for (const QString& componentId : componentIds) {  ///< 当前待准备路径的元器件编号。
        QString format = QStringLiteral("pdf");  ///< 当前数据手册文件格式。
        auto it = cachedData.find(componentId);  ///< 当前元器件缓存迭代器。
        if (it != cachedData.end() && it.value()) {
            QString dataFormat = it.value()->datasheetFormat();  ///< 缓存中记录的数据手册格式。
            if (!dataFormat.isEmpty()) {
                format = dataFormat;
            }
        }

        QString fileName = componentId + QStringLiteral(".") + format;  ///< 最终数据手册文件名。
        QString finalPath = outputDir + QDir::separator() + fileName;  ///< 最终数据手册路径。
        QString tempPath =
            m_tempManager.createTempFilePath(componentId, QStringLiteral(".") + format);  ///< 临时数据手册路径。

        if (!tempPath.isEmpty()) {
            m_tempPaths[componentId] = tempPath;
            m_finalPaths[componentId] = finalPath;
        }
    }

    m_isExporting.store(true);

    ExportTypeStage::start(componentIds, cachedData);
}

/**
 * @brief 取消当前数据手册导出阶段并回滚临时文件。
 */
void DatasheetExportStage::cancel() {
    if (!m_isExporting.load()) {
        return;
    }

    qDebug() << "DatasheetExportStage: Cancelling...";
    m_cancelled.store(true);
    m_tempManager.rollbackAll();
    m_isExporting.store(false);
    qDebug() << "DatasheetExportStage: Cancelled";
}

/**
 * @brief 创建一个数据手册导出工作对象。
 * @return 新建的数据手册导出工作对象。
 */
QObject* DatasheetExportStage::createWorker() {
    return new DatasheetExportWorker();
}

/**
 * @brief 配置并启动一个数据手册导出工作对象。
 * @param worker 待配置的工作对象。
 * @param componentId 元器件编号。
 * @param data 预加载的元器件数据。
 */
void DatasheetExportStage::startWorker(QObject* worker,
                                       const QString& componentId,
                                       const QSharedPointer<ComponentData>& data) {
    auto* exportWorker = qobject_cast<DatasheetExportWorker*>(worker);  ///< 转换后的数据手册工作对象。
    if (!exportWorker) {
        qWarning() << "DatasheetExportStage: Failed to cast worker to DatasheetExportWorker";
        return;
    }

    exportWorker->setOptions(m_options);
    exportWorker->setData(componentId, data, m_options);

    if (m_tempPaths.contains(componentId)) {
        exportWorker->setTempPath(m_tempPaths[componentId]);
    }

    QPointer<DatasheetExportStage> stagePtr(this);  ///< 防止异步回调访问已销毁的导出阶段。
    connect(
        exportWorker,
        &DatasheetExportWorker::completed,
        this,
        [stagePtr, exportWorker, componentId](const QString&, bool success, const QString& error) {
            if (!stagePtr) {
                exportWorker->deleteLater();
                return;
            }

            if (success && !stagePtr->m_cancelled.load() && stagePtr->m_finalPaths.contains(componentId)) {
                QString tempPath = stagePtr->m_tempPaths.value(componentId);  ///< 当前元器件临时文件路径。
                QString finalPath = stagePtr->m_finalPaths.value(componentId);  ///< 当前元器件最终文件路径。
                success = stagePtr->m_tempManager.commitWithBackup(tempPath, finalPath);
            }

            stagePtr->completeItemProgress(exportWorker, componentId, success, error);
            exportWorker->deleteLater();
        },
        Qt::QueuedConnection);

    m_threadPool.start(exportWorker);
}

}  // namespace EasyKiConverter
