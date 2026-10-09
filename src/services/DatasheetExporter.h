#ifndef DATASHEETEXPORTER_H
#define DATASHEETEXPORTER_H

#include "export/ExportProgress.h"

#include <QDir>
#include <QObject>
#include <QString>

namespace EasyKiConverter {

class DatasheetExporter : public QObject {
    Q_OBJECT
public:
    /**
     * @brief 创建数据手册导出器。
     * @param parent Qt 对象父级，可为空
     */
    explicit DatasheetExporter(QObject* parent = nullptr);
    /**
     * @brief 销毁数据手册导出器。
     */
    ~DatasheetExporter() override;

    /**
     * @brief 设置数据手册导出的运行选项。
     * @param options 导出选项
     */
    void setOptions(const ExportOptions& options);

    /**
     * @brief 下载并导出指定数据手册。
     * @param datasheetUrl 数据手册地址
     * @param outputPath 输出目录
     * @param componentName 元器件名称
     * @return 导出成功返回 true，否则返回 false
     */
    bool exportDatasheet(const QString& datasheetUrl, const QString& outputPath, const QString& componentName);

signals:
    // 预留接口：当前为同步导出，暂时不使用这些信号
    // 将来如需改为非阻塞异步导出，可启用这些信号进行进度/完成通知
    void exportCompleted(bool success);
    void exportFailed(const QString& error);

private:
    /**
     * @brief 创建数据手册输出目录。
     * @param dirPath 目标目录路径
     * @return 目录创建或已存在时返回 true，否则返回 false
     */
    bool createDatasheetDirectory(const QString& dirPath);

private:
    ExportOptions m_options;  ///< 当前数据手册导出选项。
};

}  // namespace EasyKiConverter

#endif  // DATASHEETEXPORTER_H
