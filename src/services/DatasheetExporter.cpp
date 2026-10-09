#include "DatasheetExporter.h"

#include "core/network/NetworkClient.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace EasyKiConverter {

/**
 * @brief 创建数据手册导出器。
 * @param parent Qt 对象父级。
 */
DatasheetExporter::DatasheetExporter(QObject* parent) : QObject(parent) {}

/** @brief 销毁数据手册导出器。 */
DatasheetExporter::~DatasheetExporter() = default;

/**
 * @brief 设置数据手册导出选项。
 * @param options 导出选项。
 */
void DatasheetExporter::setOptions(const ExportOptions& options) {
    m_options = options;
}

/**
 * @brief 下载并导出单个元器件数据手册。
 * @param datasheetUrl 数据手册 URL。
 * @param outputPath 导出根目录。
 * @param componentName 元器件名称。
 * @return 导出成功或数据手册为空时返回 true。
 */
bool DatasheetExporter::exportDatasheet(const QString& datasheetUrl,
                                        const QString& outputPath,
                                        const QString& componentName) {
    if (datasheetUrl.isEmpty()) {
        return true;
    }

    QString datasheetDirPath = QString("%1/%2.datasheet").arg(outputPath, m_options.libName);  // 数据手册目录。
    if (!createDatasheetDirectory(datasheetDirPath)) {
        qWarning() << "Failed to create datasheet directory:" << datasheetDirPath;
        return false;
    }

    QString fileExtension = "pdf";  // 默认文件扩展名。
    QFileInfo urlInfo(datasheetUrl);  // URL 路径信息。
    if (!urlInfo.suffix().isEmpty()) {
        fileExtension = urlInfo.suffix();
    }

    QString fileName = QString("%1.%2").arg(componentName, fileExtension);  // 输出文件名。
    QString datasheetPath = QString("%1/%2").arg(datasheetDirPath, fileName);  // 输出文件完整路径。

    if (!m_options.overwriteExistingFiles && QFile::exists(datasheetPath)) {
        qWarning() << "Datasheet already exists:" << datasheetPath;
        return true;
    }

    // 使用统一网络客户端的默认重试策略下载数据手册。
    NetworkResult result = NetworkClient::instance().get(QUrl(datasheetUrl), ResourceType::Datasheet);  // 下载结果。

    if (result.success) {
        QFile file(datasheetPath);  // 数据手册输出文件。
        if (file.open(QIODevice::WriteOnly)) {
            file.write(result.data);
            file.close();
            qDebug() << "Datasheet exported successfully:" << datasheetPath;
            return true;
        } else {
            qWarning() << "Failed to open file for writing:" << datasheetPath;
            return false;
        }
    } else {
        qWarning() << "Failed to download datasheet:" << datasheetUrl << result.error;
        return false;
    }
}

/**
 * @brief 创建数据手册输出目录。
 * @param dirPath 目标目录路径。
 * @return 目录已存在或创建成功时返回 true。
 */
bool DatasheetExporter::createDatasheetDirectory(const QString& dirPath) {
    QDir dir;  // 用于创建目录的目录对象。
    if (dir.exists(dirPath)) {
        return true;
    }
    return dir.mkpath(dirPath);
}

}  // namespace EasyKiConverter
