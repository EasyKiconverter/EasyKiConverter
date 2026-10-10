#ifndef PIPELINECOMPLETIONHANDLER_H
#define PIPELINECOMPLETIONHANDLER_H

#include "export/ExportProgress.h"

#include <QObject>
#include <QRegularExpression>
#include <QString>

namespace EasyKiConverter {

class ComponentData;
class ComponentListViewModel;

class PipelineCompletionHandler : public QObject {
    Q_OBJECT
public:
    /**
     * @brief 创建导出流水线完成处理器。
     * @param parent Qt 对象父级，可为空
     */
    explicit PipelineCompletionHandler(QObject* parent = nullptr);
    /**
     * @brief 销毁导出流水线完成处理器。
     */
    ~PipelineCompletionHandler() override;

    /**
     * @brief 从预加载数据导出预览图。
     * @param preloadedData 按元器件编号索引的预加载数据
     * @param options 导出选项
     */
    void exportPreviewImages(const QMap<QString, QSharedPointer<ComponentData>>& preloadedData,
                             const ExportOptions& options);

    /**
     * @brief 从预加载数据导出数据手册。
     * @param preloadedData 按元器件编号索引的预加载数据
     * @param options 导出选项
     */
    void exportDatasheets(const QMap<QString, QSharedPointer<ComponentData>>& preloadedData,
                          const ExportOptions& options);

    /**
     * @brief 使用组件列表视图模型导出预览图。
     * @param componentListViewModel 组件列表视图模型
     * @param progressViewModel 导出进度视图模型
     * @param options 导出选项
     */
    void exportPreviewImagesFromViewModel(ComponentListViewModel* componentListViewModel,
                                          class ExportProgressViewModel* progressViewModel,
                                          const ExportOptions& options);

    /**
     * @brief 使用组件列表视图模型导出数据手册。
     * @param componentListViewModel 组件列表视图模型
     * @param progressViewModel 导出进度视图模型
     * @param options 导出选项
     */
    void exportDatasheetsFromViewModel(ComponentListViewModel* componentListViewModel,
                                       class ExportProgressViewModel* progressViewModel,
                                       const ExportOptions& options);

signals:
    void previewImagesExported(int successCount);
    void datasheetsExported(int successCount);

private:
    /**
     * @brief 从内存中的预览图数据写出文件。
     * @param imageDataList 预览图数据列表
     * @param outputPath 输出目录
     * @param componentName 元器件名称
     * @return 全部写入成功返回 true，否则返回 false
     */
    bool exportPreviewImagesFromMemory(const QList<QByteArray>& imageDataList,
                                       const QString& outputPath,
                                       const QString& componentName);
    /**
     * @brief 从内存中的数据手册数据写出文件。
     * @param datasheetData 数据手册内容
     * @param outputPath 输出目录
     * @param componentName 元器件名称
     * @param format 数据手册格式
     * @return 写入成功返回 true，否则返回 false
     */
    bool exportDatasheetFromMemory(const QByteArray& datasheetData,
                                   const QString& outputPath,
                                   const QString& componentName,
                                   const QString& format);
    static const QRegularExpression INVALID_FILENAME_CHARS;  ///< 用于过滤输出文件名非法字符的正则表达式。
};

}  // namespace EasyKiConverter

#endif  // PIPELINECOMPLETIONHANDLER_H
