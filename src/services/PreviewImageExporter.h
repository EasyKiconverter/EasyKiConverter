#ifndef PREVIEWIMAGEEXPORTER_H
#define PREVIEWIMAGEEXPORTER_H

#include "export/ExportProgress.h"

#include <QDir>
#include <QObject>
#include <QString>
#include <QStringList>

namespace EasyKiConverter {

class PreviewImageExporter : public QObject {
    Q_OBJECT
public:
    /**
     * @brief 创建预览图导出器。
     * @param parent Qt 对象父级，可为空
     */
    explicit PreviewImageExporter(QObject* parent = nullptr);
    /**
     * @brief 销毁预览图导出器。
     */
    ~PreviewImageExporter() override;

    /**
     * @brief 设置预览图导出的运行选项。
     * @param options 导出选项
     */
    void setOptions(const ExportOptions& options);

    /**
     * @brief 从预览图地址下载并导出图片。
     * @param imageUrls 预览图地址列表
     * @param outputPath 输出目录
     * @param componentName 元器件名称
     * @return 导出成功返回 true，否则返回 false
     */
    bool exportFromUrls(const QStringList& imageUrls, const QString& outputPath, const QString& componentName);
    /**
     * @brief 从本地缓存文件导出预览图。
     * @param imagePaths 预览图文件路径列表
     * @param outputPath 输出目录
     * @param componentName 元器件名称
     * @return 导出成功返回 true，否则返回 false
     */
    bool exportFromCache(const QStringList& imagePaths, const QString& outputPath, const QString& componentName);

signals:
    // 预留接口：当前为同步导出，暂时不使用这些信号
    // 将来如需改为非阻塞异步导出，可启用这些信号进行进度/完成通知
    void exportCompleted(int successCount);
    void exportFailed(const QString& error);

private:
    /**
     * @brief 创建预览图输出目录。
     * @param dirPath 目标目录路径
     * @return 目录创建或已存在时返回 true，否则返回 false
     */
    bool createPreviewDirectory(const QString& dirPath);
    /**
     * @brief 为预览图生成稳定的输出文件名。
     * @param componentName 元器件名称
     * @param extension 文件扩展名
     * @param index 当前图片序号
     * @param total 图片总数
     * @return 生成的文件名
     */
    QString generateFileName(const QString& componentName, const QString& extension, int index, int total);

private:
    ExportOptions m_options;  ///< 当前预览图导出选项。
};

}  // namespace EasyKiConverter

#endif  // PREVIEWIMAGEEXPORTER_H
