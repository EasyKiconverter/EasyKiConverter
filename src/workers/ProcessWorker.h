#ifndef PROCESSWORKER_H
#define PROCESSWORKER_H

#include "models/ComponentExportStatus.h"

#include <QObject>
#include <QRunnable>

namespace EasyKiConverter {

/**
 * @brief 处理工作线程
 *
 * 负责解析和转换数据（CPU密集型任务）
 * 纯CPU密集型，不包含任何网络I/O操作
 */
class ProcessWorker : public QObject, public QRunnable {
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param status 导出状态（使用 QSharedPointer 避免拷贝）
     * @param parent 父对象
         */
    explicit ProcessWorker(QSharedPointer<ComponentExportStatus> status, QObject* parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~ProcessWorker() override;

    /**
     * @brief 执行处理任务
     */
    void run() override;

    /**
     * @brief 取消处理任务 (v3.0.5+)
     */
    void abort();

signals:
    /**
     * @brief 处理完成信号
     * @param status 导出状态（使用 QSharedPointer 避免拷贝）
     */
    void processCompleted(QSharedPointer<ComponentExportStatus> status);

private:
    /**
     * @brief 解析组件信息
     * @param status 导出状态
         * @return bool 是否成功
     */
    bool parseComponentInfo(ComponentExportStatus& status);

    /**
     * @brief 解析CAD数据
     * @param status 导出状态
         * @return bool 是否成功
     */
    bool parseCadData(ComponentExportStatus& status);

    /**
     * @brief 解析3D模型数据
     * @param status 导出状态
         * @return bool 是否成功
     */
    bool parse3DModelData(ComponentExportStatus& status);

private:
    QSharedPointer<ComponentExportStatus> m_status;  ///< 线程处理所使用的共享导出状态。
    QAtomicInt m_isAborted;  ///< 取消标志（v3.0.5+）。
};

}  // namespace EasyKiConverter

#endif  // PROCESSWORKER_H
