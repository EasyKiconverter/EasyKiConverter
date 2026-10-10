#pragma once

#include "ExportProgress.h"

#include <QMap>
#include <QMutex>
#include <QObject>
#include <QQueue>
#include <QSet>
#include <QSharedPointer>
#include <QString>
#include <QStringList>
#include <QThread>
#include <QThreadPool>

#include <atomic>

namespace EasyKiConverter {

class TempFileManager;

class ComponentData;

/**
 * @brief 导出类型阶段基类
 *
 * 所有导出类型（符号、封装、3D模型、预览图、数据手册）都继承此类。
 * 该类管理一种导出类型的并行导出任务，使用QThreadPool实现线程池管理。
 *
 * 使用示例:
 * @code
 * class SymbolExportStage : public ExportTypeStage {
 * protected:
 *     QObject* createWorker() override {
 *         return new SymbolExportWorker();
 *     }
 *     void startWorker(QObject* worker, const QString& id,
 *                      const QSharedPointer<ComponentData>& data) override {
 *         auto* w = qobject_cast<SymbolExportWorker*>(worker);
 *         w->setData(id, data, m_options);
 *         m_threadPool.start(w);
 *     }
 * };
 * @endcode
 *
 * 线程安全说明:
 * - 进度数据通过QMutex保护
 * - 原子标志(m_isRunning, m_cancelled)保证线程安全
 * - 活跃worker列表通过QMutex保护
 */
class ExportTypeStage : public QObject {
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param typeName 导出类型名称（如 "Symbol", "Footprint"）
     * @param maxConcurrent 最大并发线程数
     * @param parent 父对象指针
     */
    ExportTypeStage(const QString& typeName, int maxConcurrent, QObject* parent = nullptr);

    /**
     * @brief 析构函数
     * @details 确保在销毁前取消所有任务并等待线程池完成
     */
    ~ExportTypeStage() override;

    /**
     * @brief 开始导出任务
     * @param componentIds 需要导出的元器件ID列表
     * @param cachedData 预加载并缓存的元器件数据
     *
     * 调用此方法将创建worker并提交到线程池执行。
     * 如果任务已在运行中，则忽略此次调用。
     */
    virtual void start(const QStringList& componentIds, const QMap<QString, QSharedPointer<ComponentData>>& cachedData);

    /**
     * @brief 取消导出任务
     *
     * 向所有活跃的worker发送取消信号，并等待线程池完成。
     * 安全的多次调用：若任务未运行则直接返回。
     */
    virtual void cancel();

    /**
     * @brief 等待已提交的后台任务结束
     * @param timeoutMs 最长等待时间（毫秒）
     * @return true 表示后台任务已结束，false 表示超时
     *
     * 仅用于上层服务关闭或回收已取消 stage 时收敛后台线程。
     * 普通取消流程应调用 cancel()，避免阻塞 UI。
     */
    virtual bool waitForFinished(int timeoutMs = 5000);

    /**
     * @brief 获取当前进度
     * @return 线程安全的进度快照
     */
    ExportTypeProgress getProgress() const;

    /**
     * @brief 检查导出任务是否正在运行
     * @return true 表示有任务正在执行
     */
    bool isRunning() const;

    /**
     * @brief 是否仍有已经提交、尚未回调完成的 worker
     */
    bool hasActiveWorkers() const;

    /**
     * @brief 获取导出类型名称。
     */
    QString typeName() const {
        return m_typeName;
    }

signals:
    /**
     * @brief 进度更新信号
     * @param progress 更新后的进度信息
     */
    void progressChanged(const ExportTypeProgress& progress);

    /**
     * @brief 单个元器件状态更新信号
     * @param componentId 元器件ID
     * @param status 更新后的状态
     */
    void itemStatusChanged(const QString& componentId, const ExportItemStatus& status);

    /**
     * @brief 该导出类型全部完成信号
     * @param successCount 成功导出的数量
     * @param failedCount 导出失败的数量
     * @param skippedCount 被跳过的数量
     */
    void completed(int successCount, int failedCount, int skippedCount);

protected:
    /**
     * @brief 创建导出worker实例
     * @return 新创建的worker对象指针，由子类实现具体的worker类型
     *
     * 子类必须实现此方法以创建对应类型的导出worker。
     */
    virtual QObject* createWorker() = 0;

    /**
     * @brief 启动worker执行导出任务
     * @param worker 由createWorker()创建的worker实例
     * @param componentId 要导出的元器件ID
     * @param data 该元器件的预加载数据
     *
     * 子类必须实现此方法以自定义worker的启动方式。
     */
    virtual void startWorker(QObject* worker,
                             const QString& componentId,
                             const QSharedPointer<ComponentData>& data) = 0;

    /**
     * @brief 启动下一个待处理的worker（按需创建）
     *
     * 当一个worker完成后自动调用，启动队列中的下一个worker。
     * 这实现了 worker 的按需创建，避免一次创建所有 worker。
     * 线程安全：由m_workerMutex保护。
     */
    void startNextWorker();

    /**
     * @brief 等待单个工作线程退出（供使用独立 QThread 的子类调用）
     * @param thread 子类的 m_workerThread 指针引用
     * @param timeoutMs 超时时间（毫秒），默认 5000
     * @return true 线程已退出，false 超时
     *
     * SymbolExportStage 和 FootprintExportStage 共用此逻辑：
     * 设置取消标志 → 请求线程退出 → 等待超时 → 置空指针。
     */
    bool waitForWorkerThread(QThread*& thread, int timeoutMs = 5000);

    /**
     * @brief 初始化单个元器件的进度状态
     * @param componentId 元器件ID
     *
     * 在start()方法中被调用，为每个元器件设置初始Pending状态。
     * 线程安全：由m_progressMutex保护。
     */
    void initItemProgress(const QString& componentId);

public:
    /**
     * @brief 完成单个元器件的进度更新
     * @param worker 完成时触发此方法的worker对象指针（用于从活跃列表移除）
     * @param componentId 元器件ID
     * @param success 导出是否成功
     * @param error 错误信息（当success为false时有效）
     *
     * 更新元器件状态、增加成功/失败计数、递减inProgressCount。
     * 当所有元器件都完成时发射completed()信号。
     * 线程安全：由m_progressMutex和m_workerMutex保护。
     *
     * 注意：显式传递worker指针而非使用sender()，因为跨线程信号sender()不可靠。
     */
    void completeItemProgress(QObject* worker,
                              const QString& componentId,
                              bool success,
                              const QString& error = QString());

    /**
     * @brief 将未满足导出前置条件的元器件标记为跳过。
     * @param worker 尚未执行或刚完成的导出 Worker。
     * @param componentId 元器件 ID。
     * @param reason 跳过原因。
     */
    void completeSkippedItemProgress(QObject* worker, const QString& componentId, const QString& reason);

    QString m_typeName;  ///< 导出类型名称
    QThreadPool m_threadPool;  ///< 线程池，用于管理并行导出任务
    std::atomic<bool> m_isRunning{false};  ///< 任务是否正在运行的原子标志
    std::atomic<bool> m_cancelled{false};  ///< 取消请求的原子标志
    std::atomic<bool> m_isExporting{false};  ///< 是否正在导出（子类可直接读写）
    QStringList m_componentIds;  ///< 需要导出的元器件ID列表
    QMap<QString, QSharedPointer<ComponentData>> m_cachedData;  ///< 预加载的元器件数据缓存
    mutable QMutex m_progressMutex;  ///< 保护m_progress的互斥量
    ExportTypeProgress m_progress;  ///< 当前进度信息
    mutable QMutex m_workerMutex;  ///< 保护m_activeWorkers和m_pendingComponents的互斥量
    QSet<QObject*> m_activeWorkers;  ///< 当前活跃的worker集合（O(1)插入/删除）
    QQueue<QString> m_pendingComponents;  ///< 待处理的组件ID队列（按需创建worker）
};

}  // namespace EasyKiConverter
