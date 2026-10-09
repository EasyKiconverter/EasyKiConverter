#include "PreviewImagesExportWorker.h"

#include "../../models/ComponentData.h"
#include "../../services/ComponentCacheService.h"
#include "DebugExportHelper.h"

#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QSaveFile>
#include <QtConcurrent>

namespace EasyKiConverter {

PreviewImagesExportWorker::PreviewImagesExportWorker() : QObject(nullptr) {
    setAutoDelete(false);
}

/**
 * @brief 设置预览图导出所需的数据。
 * @param componentId 元器件编号。
 * @param data 预加载的元器件数据。
 * @param options 导出选项。
 */
void PreviewImagesExportWorker::setData(const QString& componentId,
                                        const QSharedPointer<ComponentData>& data,
                                        const struct ExportOptions& options) {
    m_componentId = componentId;
    m_data = data;
    m_options = options;
    m_cacheGeneration = ComponentCacheService::instance()->currentGeneration();
}

/**
 * @brief 更新预览图导出选项。
 * @param options 导出选项。
 */
void PreviewImagesExportWorker::setOptions(const struct ExportOptions& options) {
    m_options = options;
}

/**
 * @brief 执行预览图缓存读取、下载和文件写入。
 */
void PreviewImagesExportWorker::run() {
    if (m_cancelled.load()) {
        emit completed(m_componentId, false, QStringLiteral("Cancelled"));
        return;
    }

    if (!m_data) {
        emit completed(m_componentId, false, QStringLiteral("No data available"));
        return;
    }

    qDebug() << "PreviewImagesExportWorker: Exporting" << m_componentId;

    // 检查预览图数据是否可用
    QList<QByteArray> previewDataList = m_data->previewImageData();  ///< 内存中的预览图数据列表。

    // 用磁盘缓存补齐内存列表中的空槽位，避免部分回调形成的空洞阻断缓存复用。
    ComponentCacheService* cache = ComponentCacheService::instance();  ///< 组件缓存服务实例。
    QList<QByteArray> cachedPreviewData(3);  ///< 从磁盘缓存加载的预览图数据槽位。
    for (int i = 0; i < cachedPreviewData.size(); ++i) {  // 当前预览图索引。
        if (i < previewDataList.size() && !previewDataList.at(i).isEmpty()) {
            cachedPreviewData[i] = previewDataList.at(i);
            continue;
        }
        cachedPreviewData[i] = cache->loadPreviewImage(m_componentId, i);
    }
    previewDataList.clear();
    for (const QByteArray& imageData : std::as_const(cachedPreviewData)) {  // 当前缓存预览图数据。
        if (!imageData.isEmpty()) {
            previewDataList.append(imageData);
        }
    }
    qDebug() << "PreviewImagesExportWorker: Loaded" << previewDataList.size() << "preview images from cache for"
             << m_componentId;

    // 如果缓存被清空，但内存里仍保留了预览图 URL，则在导出阶段直接回补下载。
    if (previewDataList.isEmpty()) {
        const QStringList previewUrls = m_data->previewImages();  ///< 可用于补回下载的预览图 URL 列表。
        if (!previewUrls.isEmpty()) {
            ComponentCacheService* cache = ComponentCacheService::instance();  ///< 用于下载预览图的缓存服务实例。
            for (int i = 0; i < previewUrls.size(); ++i) {  // 当前预览图 URL 索引。
                // 当前 URL 下载得到的预览图数据。
                const QByteArray imageData = cache->downloadPreviewImage(m_componentId,
                                                                         previewUrls[i],
                                                                         i,
                                                                         nullptr,
                                                                         nullptr,
                                                                         m_options.weakNetworkSupport,
                                                                         m_cacheGeneration);
                if (!imageData.isEmpty()) {
                    previewDataList.append(imageData);
                }
            }
            qDebug() << "PreviewImagesExportWorker: Re-downloaded" << previewDataList.size()
                     << "preview images from URLs for" << m_componentId;
        }
    }

    if (previewDataList.isEmpty()) {
        // 无预览图数据时按跳过处理，不阻塞整体导出
        emit completed(m_componentId, true, QStringLiteral("Preview image data not available, skipped"));
        return;
    }

    // 构建输出路径
    // 注意：如果没有使用临时路径（previewCount <= 0 的情况），
    // m_options.outputPath 仍然是根目录，需要手动添加 .preview 子目录
    QString outputDir;  ///< 当前元器件的预览图输出目录。
    if (m_tempPaths.isEmpty()) {
        // 没有使用临时路径，使用 .preview 子目录
        QString libName =
            m_options.libName.isEmpty() ? QStringLiteral("EasyKiConverter") : m_options.libName;  ///< 库名称。
        QString baseOutputDir = m_options.outputPath;  ///< 用户指定的输出根目录。
        if (baseOutputDir.isEmpty()) {
            baseOutputDir = QDir::currentPath() + QStringLiteral("/export");
        }
        outputDir = baseOutputDir + QDir::separator() + libName + QStringLiteral(".preview");
    } else {
        // 使用 setTempPaths 设置的路径
        outputDir = m_options.outputPath;
    }

    QDir dir;  ///< 用于创建输出目录的目录对象。
    if (!dir.mkpath(outputDir)) {
        emit completed(m_componentId, false, QStringLiteral("Failed to create output directory"));
        return;
    }

    // 遍历导出所有预览图
    int successCount = 0;  ///< 成功写入的预览图数量。
    int totalCount = previewDataList.size();  ///< 待写入的预览图总数。

    for (int i = 0; i < totalCount; ++i) {  // 当前预览图索引。
        if (m_cancelled.load()) {
            emit completed(m_componentId, false, QStringLiteral("Cancelled"));
            return;
        }

        const auto& previewData = previewDataList[i];  ///< 当前预览图二进制数据。
        QString fileName = QStringLiteral("%1_preview_%2.png").arg(m_componentId).arg(i + 1);  ///< 最终文件名。
        QString finalPath = outputDir + QStringLiteral("/") + fileName;  ///< 最终输出路径。

        // 使用temp路径如果可用
        QString filePath = m_tempPaths.contains(fileName) ? m_tempPaths[fileName] : finalPath;  ///< 实际写入路径。

        // 确保目标目录存在（temp目录按组件拆分，可能尚未创建）
        const QString targetDir = QFileInfo(filePath).absolutePath();  ///< 实际写入文件的父目录。
        if (!QDir().mkpath(targetDir)) {
            qWarning() << "PreviewImagesExportWorker: Failed to create target dir" << targetDir;
            continue;
        }

        // 检查文件是否已存在（仅对最终路径且非temp模式）
        if (filePath == finalPath && QFile::exists(finalPath) && !m_options.overwriteExistingFiles) {
            qDebug() << "PreviewImagesExportWorker: File already exists, skipping" << finalPath;
            successCount++;
            continue;
        }

        try {
            // 解码并保存预览图
            QImage image = QImage::fromData(previewData);  ///< 解码后的预览图对象。
            if (image.isNull()) {
                qWarning() << "PreviewImagesExportWorker: Failed to decode preview image" << i;
                continue;
            }

            QSaveFile file(filePath);  ///< 负责原子写入预览图的文件对象。
            if (!file.open(QIODevice::WriteOnly)) {
                qWarning() << "PreviewImagesExportWorker: Failed to open file" << filePath << file.errorString();
                continue;
            }

            if (!image.save(&file, "PNG")) {
                qWarning() << "PreviewImagesExportWorker: Failed to save image" << filePath;
                continue;
            }

            if (!file.commit()) {
                qWarning() << "PreviewImagesExportWorker: Failed to commit file" << filePath;
                continue;
            }

            successCount++;

        } catch (const std::exception& e) {
            qWarning() << "PreviewImagesExportWorker: Exception during export:" << e.what();
        }
    }

    if (successCount == totalCount) {
        qDebug() << "PreviewImagesExportWorker: Successfully exported all previews for" << m_componentId;

        if (m_options.debugMode) {
            (void)QtConcurrent::run([componentId = m_componentId, data = m_data, outputPath = m_options.outputPath]() {
                DebugExportHelper::exportDebugData(componentId, data, outputPath);
            });
        }

        emit completed(m_componentId, true, QString());
    } else if (successCount > 0) {
        // 部分成功也视为失败，避免提交不完整的临时文件
        qWarning() << "PreviewImagesExportWorker: Partially exported" << successCount << "/" << totalCount
                   << "- treating as failure";
        emit completed(
            m_componentId,
            false,
            QStringLiteral("Partial export failure: %1 of %2 images exported").arg(successCount).arg(totalCount));
    } else {
        emit completed(m_componentId, false, QStringLiteral("Failed to export any preview images"));
    }
}

/**
 * @brief 设置取消标志，使正在执行的导出尽快停止。
 */
void PreviewImagesExportWorker::cancel() {
    m_cancelled.store(true);
}

}  // namespace EasyKiConverter
