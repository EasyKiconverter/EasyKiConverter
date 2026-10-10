#pragma once

#include "ExportProgress.h"
#include "IExportWorker.h"

#include <QFileInfo>
#include <QObject>
#include <QRunnable>
#include <QSharedPointer>
#include <QString>

#include <atomic>
#include <cstdint>

namespace EasyKiConverter {

class ComponentData;

/**
 * @brief 预览图导出Worker
 *
 * 在线程池中执行单个元器件的预览图导出任务。
 * 负责将元器件的预览图导出为PNG/JPEG格式。
 */
class PreviewImagesExportWorker : public QObject, public QRunnable, public IExportWorker {
    Q_OBJECT

public:
    PreviewImagesExportWorker();

    /**
     * @brief 设置导出所需的数据
     * @param componentId 元器件ID
     * @param data 预加载的元器件数据
     * @param options 导出选项
     */
    void setData(const QString& componentId,
                 const QSharedPointer<ComponentData>& data,
                 const ExportOptions& options) override;

    /**
     * @brief 设置临时文件路径映射
     * @param tempPaths 文件名到临时路径的映射
     */
    void setTempPaths(const QMap<QString, QString>& tempPaths) {
        m_tempPaths = tempPaths;
        // 从 tempPaths 提取输出目录并更新 m_options.outputPath
        if (!m_tempPaths.isEmpty()) {
            const QString firstPath = m_tempPaths.first();  ///< 第一项临时文件路径，用于推导输出目录。
            m_options.outputPath = QFileInfo(firstPath).absolutePath();
        }
    }

    /**
     * @brief 执行预览图导出任务
     *
     * 在线程池中被调用，执行实际的导出逻辑:
     * 1. 检查预览图数据是否可用
     * 2. 创建输出目录
     * 3. 导出预览图为PNG格式
     * 4. 发射completed信号
     */
    void run() override;

    /**
     * @brief 请求取消导出任务
     */
    void cancel() override;

    /**
     * @brief 获取元器件 ID。
     * @return 元器件编号。
     */
    QString componentId() const override {
        return m_componentId;
    }

    /**
     * @brief 设置导出选项。
     * @param options 导出选项。
     */
    void setOptions(const ExportOptions& options);

signals:
    /**
     * @brief 导出完成信号
     * @param componentId 元器件ID
     * @param success 是否成功
     * @param error 错误信息（成功时为空）
     */
    void completed(const QString& componentId, bool success, const QString& error);

private:
    QString m_componentId;  ///< 元器件ID
    QMap<QString, QString> m_tempPaths;  ///< 文件名到临时路径的映射
    QSharedPointer<ComponentData> m_data;  ///< 预加载的元器件数据
    ExportOptions m_options;  ///< 导出选项
    uint64_t m_cacheGeneration = 0;  ///< 接收任务数据时捕获的缓存代次
    std::atomic<bool> m_cancelled{false};  ///< 取消标志
};

}  // namespace EasyKiConverter
