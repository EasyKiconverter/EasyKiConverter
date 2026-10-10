#include "PipelineCompletionHandler.h"

#include "models/ComponentData.h"
#include "services/ComponentCacheService.h"
#include "ui/viewmodels/ComponentListViewModel.h"
#include "ui/viewmodels/ExportProgressViewModel.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QRegularExpression>

namespace EasyKiConverter {

const QRegularExpression PipelineCompletionHandler::INVALID_FILENAME_CHARS(  ///< 文件名中需要替换的非法字符。
    "[<>:\"/\\\\|?*]");

/**
 * @brief 创建流水线完成处理器。
 * @param parent Qt 对象父级。
 */
PipelineCompletionHandler::PipelineCompletionHandler(QObject* parent) : QObject(parent) {}

/**
 * @brief 销毁流水线完成处理器。
 */
PipelineCompletionHandler::~PipelineCompletionHandler() = default;

/**
 * @brief 导出预加载数据中的预览图片。
 * @param preloadedData 按元器件编号索引的预加载数据。
 * @param options 导出选项。
 */
void PipelineCompletionHandler::exportPreviewImages(const QMap<QString, QSharedPointer<ComponentData>>& preloadedData,
                                                    const ExportOptions& options) {
    if (!options.exportPreviewImages) {
        return;
    }

    int successCount = 0;  // 成功导出预览图片的元器件数量。
    for (auto it = preloadedData.constBegin(); it != preloadedData.constEnd(); ++it) {
        const QSharedPointer<ComponentData>& componentData = it.value();  // 当前元器件数据。
        QString componentId = componentData->lcscId();  // 当前元器件编号。

        QList<QByteArray> previewImageDataList = componentData->previewImageData();  // 当前元器件的图片数据。

        if (!previewImageDataList.isEmpty()) {
            // 用于生成文件名的元器件名称。
            QString componentName = componentData->name().isEmpty() ? componentId : componentData->name();

            if (exportPreviewImagesFromMemory(previewImageDataList, options.outputPath, componentName)) {
                successCount++;
            } else {
                qWarning() << "Failed to export preview images for component:" << componentId;
            }
        }
    }
    qDebug() << "Preview images export completed:" << successCount << "components";
    emit previewImagesExported(successCount);
}

/**
 * @brief 导出预加载数据中的数据手册。
 * @param preloadedData 按元器件编号索引的预加载数据。
 * @param options 导出选项。
 */
void PipelineCompletionHandler::exportDatasheets(const QMap<QString, QSharedPointer<ComponentData>>& preloadedData,
                                                 const ExportOptions& options) {
    if (!options.exportDatasheet) {
        return;
    }

    int successCount = 0;  // 成功导出数据手册的元器件数量。
    for (auto it = preloadedData.constBegin(); it != preloadedData.constEnd(); ++it) {
        const QSharedPointer<ComponentData>& componentData = it.value();  // 当前元器件数据。
        if (!componentData->datasheetData().isEmpty()) {
            // 用于生成数据手册文件名的元器件名称。
            QString componentName = componentData->name().isEmpty() ? componentData->lcscId() : componentData->name();

            if (exportDatasheetFromMemory(componentData->datasheetData(),
                                          options.outputPath,
                                          componentName,
                                          componentData->datasheetFormat())) {
                successCount++;
            }
        }
    }
    qDebug() << "Datasheets export completed:" << successCount << "components";
    emit datasheetsExported(successCount);
}

/**
 * @brief 将内存中的预览图片写入输出目录。
 * @param imageDataList 预览图片二进制数据列表。
 * @param outputPath 输出根目录。
 * @param componentName 用于生成文件名的元器件名称。
 * @return 所有非空图片均写入成功时返回 true。
 */
bool PipelineCompletionHandler::exportPreviewImagesFromMemory(const QList<QByteArray>& imageDataList,
                                                              const QString& outputPath,
                                                              const QString& componentName) {
    QDir outputDir(outputPath);  // 导出根目录对象。
    QString imagesDir = outputDir.filePath("images");  // 预览图片输出目录。
    if (!outputDir.exists(imagesDir)) {
        if (!outputDir.mkpath(imagesDir)) {
            qWarning() << "Failed to create images directory:" << imagesDir;
            return false;
        }
    }

    QString safeName = componentName;  // 替换非法字符后的文件名前缀。
    safeName.replace(INVALID_FILENAME_CHARS, "_");

    bool allSuccess = true;  // 所有图片是否均写入成功。
    int exportedCount = 0;  // 已写入的图片数量。

    for (int i = 0; i < imageDataList.size(); ++i) {
        if (imageDataList[i].isEmpty()) {
            continue;
        }

        QString filename = QString("%1_%2.jpg").arg(safeName).arg(i);  // 当前图片文件名。
        QString filePath = QDir(imagesDir).filePath(filename);  // 当前图片完整路径。

        QFile file(filePath);  // 当前图片输出文件。
        if (file.open(QIODevice::WriteOnly)) {
            file.write(imageDataList[i]);
            file.close();
            exportedCount++;
        } else {
            allSuccess = false;
        }
    }

    return allSuccess;
}

/**
 * @brief 将内存中的数据手册写入输出目录。
 * @param datasheetData 数据手册二进制数据。
 * @param outputPath 输出根目录。
 * @param componentName 用于生成文件名的元器件名称。
 * @param format 数据手册格式。
 * @return 数据手册写入成功时返回 true。
 */
bool PipelineCompletionHandler::exportDatasheetFromMemory(const QByteArray& datasheetData,
                                                          const QString& outputPath,
                                                          const QString& componentName,
                                                          const QString& format) {
    QDir outputDir(outputPath);  // 导出根目录对象。
    QString datasheetsDir = outputDir.filePath("datasheets");  // 数据手册输出目录。
    if (!outputDir.exists(datasheetsDir)) {
        if (!outputDir.mkpath(datasheetsDir)) {
            qWarning() << "Failed to create datasheets directory:" << datasheetsDir;
            return false;
        }
    }

    QString safeName = componentName;  // 替换非法字符后的文件名前缀。
    safeName.replace(INVALID_FILENAME_CHARS, "_");

    QString extension = format.toLower();  // 规范化后的文件扩展名。
    if (extension != "html") {
        extension = "pdf";
    }

    QString filename = QString("%1.%2").arg(safeName).arg(extension);  // 当前数据手册文件名。
    QString filePath = QDir(datasheetsDir).filePath(filename);  // 当前数据手册完整路径。

    QFile file(filePath);  // 当前数据手册输出文件。
    if (file.open(QIODevice::WriteOnly)) {
        file.write(datasheetData);
        file.close();
        return true;
    }
    return false;
}

/**
 * @brief 从视图模型加载并导出预览图片。
 * @param componentListViewModel 提供预加载数据的元器件列表视图模型。
 * @param progressViewModel 接收单个元器件导出状态的进度视图模型。
 * @param options 导出选项。
 */
void PipelineCompletionHandler::exportPreviewImagesFromViewModel(ComponentListViewModel* componentListViewModel,
                                                                 ExportProgressViewModel* progressViewModel,
                                                                 const ExportOptions& options) {
    if (!options.exportPreviewImages || !componentListViewModel) {
        return;
    }

    QMap<QString, QSharedPointer<ComponentData>> allData =  // 从 ViewModel 获取最新的预加载数据。
        componentListViewModel->getAllPreloadedData();
    if (allData.isEmpty()) {
        qDebug() << "No preloaded data available from ViewModel for preview images export";
        emit previewImagesExported(0);
        return;
    }

    int successCount = 0;  // 成功导出预览图片的元器件数量。
    for (auto it = allData.constBegin(); it != allData.constEnd(); ++it) {
        const QSharedPointer<ComponentData>& componentData = it.value();  // 当前元器件数据。
        QString componentId = componentData->lcscId();  // 当前元器件编号。

        QList<QByteArray> previewImageDataList = componentData->previewImageData();  // 当前预览图片数据。

        // 1. 如果内存中有数据，直接使用
        // 2. 如果内存中没有但有 URL，尝试从磁盘缓存加载
        if (previewImageDataList.isEmpty() && !componentData->previewImages().isEmpty()) {
            const QStringList& imageUrls = componentData->previewImages();  // 当前元器件的预览图片地址。
            for (int i = 0; i < imageUrls.size() && i < 3; ++i) {
                QByteArray imageData =  // 当前预览图片的缓存或下载数据。
                    ComponentCacheService::instance()->loadPreviewImage(componentId, i);
                if (!imageData.isEmpty()) {
                    previewImageDataList.append(imageData);
                    qDebug() << "Preview image loaded from disk cache for component:" << componentId << "index:" << i;
                } else {
                    // 3. 磁盘缓存也没有，从 URL 下载并缓存
                    qDebug() << "Preview image not in cache, downloading for component:" << componentId
                             << "index:" << i;
                    imageData = ComponentCacheService::instance()->downloadPreviewImage(
                        componentId, imageUrls[i], i, nullptr, nullptr, options.weakNetworkSupport);
                    if (!imageData.isEmpty()) {
                        previewImageDataList.append(imageData);
                    }
                }
            }
        }

        bool exportSuccess = false;  // 当前元器件的预览图片是否导出成功。
        if (!previewImageDataList.isEmpty()) {
            // 用于生成文件名的元器件名称。
            QString componentName = componentData->name().isEmpty() ? componentId : componentData->name();

            if (exportPreviewImagesFromMemory(previewImageDataList, options.outputPath, componentName)) {
                successCount++;
                exportSuccess = true;
            } else {
                qWarning() << "Failed to export preview images for component:" << componentId;
            }
        } else {
            // 没有预览图数据但不需要导出时，也标记为成功（没有预览图不代表失败）
            if (!options.exportPreviewImages || componentData->previewImages().isEmpty()) {
                exportSuccess = true;
            }
        }

        // 更新预览图导出状态。
        componentListViewModel->updateExportStatus(componentId, exportSuccess ? 1 : 0, -1);
        if (progressViewModel) {
            progressViewModel->updateComponentExportStatus(componentId, exportSuccess ? 1 : 0, -1);
        }
    }
    qDebug() << "Preview images export from ViewModel completed:" << successCount << "components";
    emit previewImagesExported(successCount);
}

/**
 * @brief 从视图模型加载并导出数据手册。
 * @param componentListViewModel 提供预加载数据的元器件列表视图模型。
 * @param progressViewModel 接收单个元器件导出状态的进度视图模型。
 * @param options 导出选项。
 */
void PipelineCompletionHandler::exportDatasheetsFromViewModel(ComponentListViewModel* componentListViewModel,
                                                              ExportProgressViewModel* progressViewModel,
                                                              const ExportOptions& options) {
    if (!options.exportDatasheet || !componentListViewModel) {
        return;
    }

    QMap<QString, QSharedPointer<ComponentData>> allData =  // 从 ViewModel 获取最新的预加载数据。
        componentListViewModel->getAllPreloadedData();
    if (allData.isEmpty()) {
        qDebug() << "No preloaded data available from ViewModel for datasheet export";
        emit datasheetsExported(0);
        return;
    }

    int successCount = 0;  // 成功导出数据手册的元器件数量。
    for (auto it = allData.constBegin(); it != allData.constEnd(); ++it) {
        const QSharedPointer<ComponentData>& componentData = it.value();  // 当前元器件数据。
        QString componentId = componentData->lcscId();  // 当前元器件编号。

        QByteArray datasheetData = componentData->datasheetData();  // 当前数据手册二进制数据。
        QString datasheetFormat = componentData->datasheetFormat();  // 当前数据手册格式。

        // 1. 如果内存中有数据，直接使用
        // 2. 如果内存中没有但有 URL，尝试从磁盘缓存加载
        if (datasheetData.isEmpty() && !componentData->datasheet().isEmpty()) {
            datasheetData = ComponentCacheService::instance()->loadDatasheet(componentId);  // 从缓存加载的数据。
            if (!datasheetData.isEmpty()) {
                qDebug() << "Datasheet loaded from disk cache for component:" << componentId;
            } else {
                // 3. 磁盘缓存也没有，从 URL 下载并缓存
                qDebug() << "Datasheet not in cache, downloading for component:" << componentId;
                datasheetData = ComponentCacheService::instance()->downloadDatasheet(componentId,
                                                                                     componentData->datasheet(),
                                                                                     &datasheetFormat,
                                                                                     nullptr,
                                                                                     nullptr,
                                                                                     options.weakNetworkSupport);
            }
        }

        bool exportSuccess = false;  // 当前元器件的数据手册是否导出成功。
        if (!datasheetData.isEmpty()) {
            // 用于生成文件名的元器件名称。
            QString componentName = componentData->name().isEmpty() ? componentId : componentData->name();

            if (exportDatasheetFromMemory(datasheetData, options.outputPath, componentName, datasheetFormat)) {
                successCount++;
                exportSuccess = true;
            }
        } else {
            // 没有手册数据但不需要导出时，也标记为成功（没有手册不代表失败）
            if (!options.exportDatasheet || componentData->datasheet().isEmpty()) {
                exportSuccess = true;
            }
        }

        // 更新手册导出状态。
        componentListViewModel->updateExportStatus(componentId, -1, exportSuccess ? 1 : 0);
        if (progressViewModel) {
            progressViewModel->updateComponentExportStatus(componentId, -1, exportSuccess ? 1 : 0);
        }
    }
    qDebug() << "Datasheets export from ViewModel completed:" << successCount << "components";
    emit datasheetsExported(successCount);
}

}  // namespace EasyKiConverter
