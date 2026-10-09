#include "DatasheetExportWorker.h"

#include "../../models/ComponentData.h"
#include "../ComponentCacheService.h"
#include "DebugExportHelper.h"

#include <QDebug>
#include <QDir>
#include <QFile>

namespace EasyKiConverter {

DatasheetExportWorker::DatasheetExportWorker() : QObject(nullptr) {
    setAutoDelete(false);
}

/**
 * @brief 设置数据手册导出任务的数据。
 * @param componentId 元器件编号。
 * @param data 预加载的元器件数据。
 * @param options 导出选项。
 */
void DatasheetExportWorker::setData(const QString& componentId,
                                    const QSharedPointer<ComponentData>& data,
                                    const struct ExportOptions& options) {
    m_componentId = componentId;
    m_data = data;
    m_options = options;
    m_cacheGeneration = ComponentCacheService::instance()->currentGeneration();
}

/**
 * @brief 更新数据手册导出选项。
 * @param options 导出选项。
 */
void DatasheetExportWorker::setOptions(const struct ExportOptions& options) {
    m_options = options;
}

/**
 * @brief 执行数据手册缓存读取、必要时下载并写入输出文件。
 */
void DatasheetExportWorker::run() {
    if (m_cancelled.load()) {
        emit completed(m_componentId, false, QStringLiteral("Cancelled"));
        return;
    }

    if (!m_data) {
        emit completed(m_componentId, false, QStringLiteral("No data available"));
        return;
    }

    // 检查数据手册数据是否可用（URL或数据二选一）
    QString datasheetUrl = m_data->datasheet();  ///< 数据手册来源 URL。
    QByteArray datasheetData = m_data->datasheetData();  ///< 内存中的数据手册原始数据。

    qDebug() << "DatasheetExportWorker: Exporting" << m_componentId;

    // 如果内存中没有数据手册数据，尝试从磁盘缓存加载
    if (datasheetData.isEmpty() && !datasheetUrl.isEmpty()) {
        ComponentCacheService* cache = ComponentCacheService::instance();  ///< 组件缓存服务实例。
        datasheetData = cache->loadDatasheet(m_componentId);  ///< 从磁盘缓存读取的数据手册数据。
        if (!datasheetData.isEmpty()) {
            qDebug() << "DatasheetExportWorker: Loaded datasheet from cache for" << m_componentId;
        }
    }

    if (datasheetUrl.isEmpty() && datasheetData.isEmpty()) {
        emit completed(m_componentId, false, QStringLiteral("Datasheet not available"));
        return;
    }

    // 构建输出路径
    QString outputDir = m_options.outputPath;  ///< 数据手册输出目录。
    if (outputDir.isEmpty()) {
        outputDir = QDir::currentPath() + QStringLiteral("/export/datasheets");
    }

    QDir dir;  ///< 用于创建输出目录的目录对象。
    if (!dir.mkpath(outputDir)) {
        emit completed(m_componentId, false, QStringLiteral("Failed to create output directory"));
        return;
    }

    // 从格式获取扩展名
    QString format = m_data->datasheetFormat();  ///< 数据手册文件格式。
    if (format.isEmpty()) {
        format = QStringLiteral("pdf");
    }

    QString fileName = m_componentId + QStringLiteral(".") + format;  ///< 最终数据手册文件名。
    QString finalPath = outputDir + QStringLiteral("/") + fileName;  ///< 最终数据手册文件路径。

    // 使用temp路径如果可用
    QString filePath = m_tempPath.isEmpty() ? finalPath : m_tempPath;  ///< 实际写入路径。

    // 检查文件是否已存在（仅对最终路径且非temp模式）
    if (filePath == finalPath && QFile::exists(finalPath) && !m_options.overwriteExistingFiles) {
        qDebug() << "DatasheetExportWorker: File already exists, skipping" << finalPath;
        emit completed(m_componentId, true, QString());
        return;
    }

    // 执行数据手册导出
    try {
        if (m_cancelled.load()) {
            emit completed(m_componentId, false, QStringLiteral("Cancelled"));
            return;
        }

        if (!datasheetData.isEmpty()) {
            // 使用内存中的数据写入文件
            QFile file(filePath);  ///< 内存数据写入文件对象。
            if (!file.open(QIODevice::WriteOnly)) {
                emit completed(m_componentId, false, QStringLiteral("Failed to open file for writing"));
                return;
            }

            qint64 written = file.write(datasheetData);  ///< 实际写入的字节数。
            file.close();

            if (written == datasheetData.size()) {
                qDebug() << "DatasheetExportWorker: Successfully exported" << filePath;
                emit completed(m_componentId, true, QString());
            } else {
                emit completed(m_componentId, false, QStringLiteral("Failed to write datasheet data"));
            }
        } else if (!datasheetUrl.isEmpty()) {
            QString format = m_data->datasheetFormat();  ///< 下载后使用的数据手册格式。
            QByteArray downloadedData =
                ComponentCacheService::instance()->downloadDatasheet(m_componentId,
                                                                     datasheetUrl,
                                                                     &format,
                                                                     nullptr,
                                                                     nullptr,
                                                                     m_options.weakNetworkSupport,
                                                                     m_cacheGeneration);

            if (downloadedData.isEmpty()) {
                emit completed(m_componentId, false, QStringLiteral("Failed to download datasheet from cached URL"));
                return;
            }

            QFile file(filePath);  ///< 下载数据写入文件对象。
            if (!file.open(QIODevice::WriteOnly)) {
                emit completed(m_componentId, false, QStringLiteral("Failed to open file for writing"));
                return;
            }

            qint64 written = file.write(downloadedData);  ///< 实际写入的下载字节数。
            file.close();

            if (written == downloadedData.size()) {
                qDebug() << "DatasheetExportWorker: Successfully downloaded and exported" << filePath;
                emit completed(m_componentId, true, QString());
            } else {
                emit completed(m_componentId, false, QStringLiteral("Failed to write downloaded datasheet data"));
            }
        } else {
            qWarning() << "DatasheetExportWorker: URL exists but data is empty for" << m_componentId
                       << "- no datasheet URL or cached data available";
            emit completed(m_componentId, false, QStringLiteral("No datasheet data or URL available"));
        }

    } catch (const std::exception& e) {
        qCritical() << "DatasheetExportWorker: Exception during export:" << e.what();
        emit completed(m_componentId, false, QString::fromUtf8(e.what()));
    } catch (...) {
        qCritical() << "DatasheetExportWorker: Unknown exception during export";
        emit completed(m_componentId, false, QStringLiteral("Unknown error"));
    }
}

/**
 * @brief 设置取消标志，使正在执行的导出尽快停止。
 */
void DatasheetExportWorker::cancel() {
    m_cancelled.store(true);
}

}  // namespace EasyKiConverter
