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
 * @brief 3D模型导出Worker
 *
 * 在线程池中执行单个元器件的3D模型导出任务。
 * 负责使用预加载阶段缓存的UUID构建API请求获取3D模型CAD数据，
 * 然后转换并写入输出目录。
 *
 * 工作流程:
 * 1. 从预加载数据中获取3D模型UUID
 * 2. 使用UUID通过Exporter3DModel获取3D模型数据(OBJ格式)
 * 3. Exporter3DModel自动处理下载、转换和写入
 * 4. 发射completed信号
 */
class Model3DExportWorker : public QObject, public QRunnable, public IExportWorker {
    Q_OBJECT

public:
    struct OutputPaths {
        QString wrlTempPath;  ///< WRL 临时文件路径。
        QString stepTempPath;  ///< STEP 临时文件路径。
    };

    /**
     * @brief 构造函数
     * @param parent 父对象
     */
    explicit Model3DExportWorker(QObject* parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~Model3DExportWorker() override;

    /**
     * @brief 设置导出所需的数据
     * @param componentId 元器件ID
     * @param data 预加载的元器件数据（包含3D模型UUID）
     * @param options 导出选项
     */
    void setData(const QString& componentId,
                 const QSharedPointer<ComponentData>& data,
                 const ExportOptions& options) override;

    /**
     * @brief 设置临时文件路径
     * @param tempPath 临时文件路径（写入位置）
     * @param paths 待处理的路径列表。
     */
    void setOutputPaths(const OutputPaths& paths) {
        m_outputPaths = paths;
    }

    /**
     * @brief 执行3D模型导出任务
     *
     * 在线程池中被调用，执行实际的导出逻辑:
     * 1. 检查3D模型UUID是否可用
     * 2. 创建输出目录
     * 3. 使用UUID构建API请求获取OBJ数据
     * 4. 将OBJ转换为WRL格式
     * 5. 写入.wrl文件
     * 6. 发射completed信号
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

private slots:
    /**
     * @brief 处理下载错误
     * @param error 错误信息
     */
    void onDownloadError(const QString& error);

private:
    QString m_componentId;  ///< 元器件ID
    OutputPaths m_outputPaths;  ///< 导出文件路径
    QSharedPointer<ComponentData> m_data;  ///< 预加载的元器件数据
    ExportOptions m_options;  ///< 导出选项
    uint64_t m_cacheGeneration = 0;  ///< 接收任务数据时捕获的缓存代次
    std::atomic<bool> m_cancelled{false};  ///< 取消标志
};

}  // namespace EasyKiConverter
