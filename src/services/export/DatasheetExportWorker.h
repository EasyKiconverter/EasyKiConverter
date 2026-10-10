#pragma once

#include "ExportProgress.h"
#include "IExportWorker.h"

#include <QObject>
#include <QRunnable>
#include <QSharedPointer>
#include <QString>

#include <atomic>
#include <cstdint>

namespace EasyKiConverter {

class ComponentData;

/**
 * @brief 数据手册导出Worker
 *
 * 在线程池中执行单个元器件的数据手册导出任务。
 * 负责将元器件的数据手册PDF复制到输出目录。
 */
class DatasheetExportWorker : public QObject, public QRunnable, public IExportWorker {
    Q_OBJECT

public:
    DatasheetExportWorker();

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
     * @brief 设置临时文件路径
     * @param tempPath 临时文件路径（写入位置）
     */
    void setTempPath(const QString& tempPath) {
        m_tempPath = tempPath;
    }

    /**
     * @brief 执行数据手册导出任务
     *
     * 在线程池中被调用，执行实际的导出逻辑:
     * 1. 检查数据手册数据是否可用
     * 2. 创建输出目录
     * 3. 复制数据手册PDF到目标位置
     * 4. 发射completed信号
     */
    void run() override;

    /**
     * @brief 请求取消导出任务
     */
    void cancel() override;

    /**
     * @brief 获取元器件ID
     */
    QString componentId() const override {
        return m_componentId;
    }

    /**
     * @brief 设置导出选项
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
    QString m_tempPath;  ///< 临时文件路径
    QSharedPointer<ComponentData> m_data;  ///< 预加载的元器件数据
    ExportOptions m_options;  ///< 导出选项
    uint64_t m_cacheGeneration = 0;  ///< 接收任务数据时捕获的缓存代次
    std::atomic<bool> m_cancelled{false};  ///< 取消标志
};

}  // namespace EasyKiConverter
