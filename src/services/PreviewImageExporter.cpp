#include "PreviewImageExporter.h"

#include "core/network/NetworkClient.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace EasyKiConverter {

/**
 * @brief 创建预览图片导出器。
 * @param parent Qt 对象父级。
 */
PreviewImageExporter::PreviewImageExporter(QObject* parent) : QObject(parent) {}

/**
 * @brief 销毁预览图片导出器。
 */
PreviewImageExporter::~PreviewImageExporter() = default;

/**
 * @brief 设置预览图片导出选项。
 * @param options 预览图片导出选项。
 */
void PreviewImageExporter::setOptions(const ExportOptions& options) {
    m_options = options;
}

/**
 * @brief 从远程地址下载并导出预览图片。
 * @param imageUrls 预览图片地址列表。
 * @param outputPath 输出根目录。
 * @param componentName 用于生成文件名的元器件名称。
 * @return 所有图片均成功导出或没有待导出图片时返回 true。
 */
bool PreviewImageExporter::exportFromUrls(const QStringList& imageUrls,
                                          const QString& outputPath,
                                          const QString& componentName) {
    if (imageUrls.isEmpty()) {
        return true;
    }

    QString imagesDirPath =  // 预览图片输出目录。
        QString("%1/%2.preview").arg(outputPath, m_options.libName);
    if (!createPreviewDirectory(imagesDirPath)) {
        qWarning() << "Failed to create preview images directory:" << imagesDirPath;
        return false;
    }

    bool allSuccess = true;  // 所有图片是否均导出成功。
    for (int i = 0; i < imageUrls.size(); ++i) {
        QString imageUrl = imageUrls[i];  // 当前预览图片地址。
        QString fileExtension = QFileInfo(imageUrl).suffix();  // 当前图片文件扩展名。
        if (fileExtension.isEmpty()) {
            fileExtension = "jpg";
        }

        QString fileName = generateFileName(componentName, fileExtension, i + 1, imageUrls.size());  // 输出文件名。
        QString imagePath = QString("%1/%2").arg(imagesDirPath, fileName);  // 输出文件完整路径。

        if (!m_options.overwriteExistingFiles && QFile::exists(imagePath)) {
            qWarning() << "Preview image already exists:" << imagePath;
            continue;
        }

        // Use NetworkClient with default retry policy (3 retries, 30s timeout)
        NetworkResult result =  // 预览图片下载结果。
            NetworkClient::instance().get(QUrl(imageUrl), ResourceType::PreviewImage);

        if (result.success) {
            QFile file(imagePath);  // 当前预览图片输出文件。
            if (file.open(QIODevice::WriteOnly)) {
                file.write(result.data);
                file.close();
                qDebug() << "Preview image exported successfully:" << imagePath;
            } else {
                qWarning() << "Failed to open file for writing:" << imagePath;
                allSuccess = false;
            }
        } else {
            qWarning() << "Failed to download preview image:" << imageUrl << result.error;
            allSuccess = false;
        }
    }

    return allSuccess;
}

/**
 * @brief 从本地缓存复制并导出预览图片。
 * @param imagePaths 本地缓存图片路径列表。
 * @param outputPath 输出根目录。
 * @param componentName 用于生成文件名的元器件名称。
 * @return 所有图片均成功复制或没有待导出图片时返回 true。
 */
bool PreviewImageExporter::exportFromCache(const QStringList& imagePaths,
                                           const QString& outputPath,
                                           const QString& componentName) {
    if (imagePaths.isEmpty()) {
        return true;
    }

    // exportFromCache 只进行文件复制操作，不需要网络管理器
    QString imagesDirPath =  // 预览图片输出目录。
        QString("%1/%2.preview").arg(outputPath, m_options.libName);
    if (!createPreviewDirectory(imagesDirPath)) {
        qWarning() << "Failed to create preview images directory:" << imagesDirPath;
        return false;
    }

    bool allSuccess = true;  // 所有图片是否均复制成功。
    for (int i = 0; i < imagePaths.size(); ++i) {
        QString sourcePath = imagePaths[i];  // 当前缓存图片路径。

        if (!QFile::exists(sourcePath)) {
            qWarning() << "Source preview image file not found:" << sourcePath;
            allSuccess = false;
            continue;
        }

        QString fileExtension = QFileInfo(sourcePath).suffix();  // 当前图片文件扩展名。
        if (fileExtension.isEmpty()) {
            fileExtension = "jpg";
        }

        QString fileName = generateFileName(componentName, fileExtension, i + 1, imagePaths.size());  // 输出文件名。
        QString destPath = QString("%1/%2").arg(imagesDirPath, fileName);  // 输出文件完整路径。

        if (!m_options.overwriteExistingFiles && QFile::exists(destPath)) {
            qWarning() << "Preview image already exists:" << destPath;
            continue;
        }

        if (QFile::copy(sourcePath, destPath)) {
            qDebug() << "Preview image copied from cache:" << destPath;
        } else {
            qWarning() << "Failed to copy preview image from cache:" << sourcePath << "to" << destPath;
            allSuccess = false;
        }
    }

    return allSuccess;
}

/**
 * @brief 创建预览图片输出目录。
 * @param dirPath 预览图片目录路径。
 * @return 目录已存在或创建成功时返回 true。
 */
bool PreviewImageExporter::createPreviewDirectory(const QString& dirPath) {
    QDir dir;  // 用于创建目录的目录对象。
    if (dir.exists(dirPath)) {
        return true;
    }
    return dir.mkpath(dirPath);
}

/**
 * @brief 根据元器件名称和图片序号生成输出文件名。
 * @param componentName 元器件名称。
 * @param extension 图片文件扩展名。
 * @param index 当前图片序号，从 1 开始。
 * @param total 图片总数。
 * @return 生成的图片文件名。
 */
QString PreviewImageExporter::generateFileName(const QString& componentName,
                                               const QString& extension,
                                               int index,
                                               int total) {
    if (total == 1) {
        return QString("%1.%2").arg(componentName, extension);
    }
    return QString("%1_%2.%3").arg(componentName).arg(index).arg(extension);
}

}  // namespace EasyKiConverter
