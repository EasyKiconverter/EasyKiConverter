#include "ExportTypeStage.h"

#include "IExportWorker.h"

#include <QDebug>
#include <QMutexLocker>
#include <QPointer>

namespace EasyKiConverter {

/**
 * @brief 初始化导出阶段的线程池、类型名称和完成状态回调。
 * @param typeName 导出类型名称。
 * @param maxConcurrent 最大并发 Worker 数量。
 * @param parent QObject 父对象。
 */
ExportTypeStage::ExportTypeStage(const QString& typeName, int maxConcurrent, QObject* parent)
    /**
     * @brief 初始化 QObject 基类和阶段名称成员。
     * @param typeName 类型名称。
     */
    : QObject(parent), m_typeName(typeName) {
    m_threadPool.setMaxThreadCount(maxConcurrent);
    m_threadPool.setExpiryTimeout(60000);
    m_progress.typeName = typeName;

    // completed 信号发射时自动清除 m_isExporting 标志（DirectConnection：原子写操作，跨线程安全）
    connect(this, &ExportTypeStage::completed, this, [this]() { m_isExporting.store(false); }, Qt::DirectConnection);
}

ExportTypeStage::~ExportTypeStage() {
    // Don't call cancel() or waitForDone() in destructor
    // as it can cause crashes during application shutdown

    // Clear the active workers list to prevent accessing deleted objects
    {
        QMutexLocker locker(&m_workerMutex);
        m_activeWorkers.clear();
    }
}

/**
 * @brief 初始化导出阶段并提交第一批 Worker。
 * @param componentIds 待导出的元器件编号列表。
 * @param cachedData 已预加载的元器件数据。
 */
void ExportTypeStage::start(const QStringList& componentIds,
                            const QMap<QString, QSharedPointer<ComponentData>>& cachedData) {
    if (m_isRunning.load()) {
        qWarning() << "ExportTypeStage:" << m_typeName << "already running";
        return;
    }

    m_componentIds = componentIds;
    m_cachedData = cachedData;
    m_cancelled.store(false);
    m_isRunning.store(true);

    {
        QMutexLocker locker(&m_progressMutex);
        m_progress = ExportTypeProgress();
        m_progress.typeName = m_typeName;
        m_progress.totalCount = componentIds.size();
        m_progress.completedCount = 0;
        m_progress.successCount = 0;
        m_progress.failedCount = 0;
        m_progress.skippedCount = 0;
        m_progress.inProgressCount = 0;
    }

    // 清空待处理队列和追踪集合
    {
        QMutexLocker locker(&m_workerMutex);
        m_pendingComponents.clear();
        m_activeWorkers.clear();
    }

    qDebug() << "ExportTypeStage:" << m_typeName << "starting for" << componentIds.size() << "components";

    // 如果没有组件需要处理，立即发送完成信号
    if (componentIds.isEmpty()) {
        qDebug() << "ExportTypeStage:" << m_typeName << "no components to process, completing immediately";
        m_isRunning.store(false);
        emit completed(0, 0, 0);
        return;
    }

    // 将所有组件加入待处理队列
    for (const QString& componentId : componentIds) {
        initItemProgress(componentId);
        m_pendingComponents.enqueue(componentId);
    }

    // 启动初始批次：不超过 maxConcurrent 个 worker
    int initialBatch = qMin(componentIds.size(), m_threadPool.maxThreadCount());
    for (int i = 0; i < initialBatch; ++i) {
        if (m_cancelled.load()) {
            break;
        }
        startNextWorker();
    }

    qDebug() << "ExportTypeStage:" << m_typeName << "initial batch started, pending:" << m_pendingComponents.size();
}

/**
 * @brief 从待处理队列取出一个元器件并创建对应的导出 Worker。
 */
void ExportTypeStage::startNextWorker() {
    if (m_cancelled.load()) {
        return;
    }

    QString componentId;  ///< 从待处理队列取出的元器件编号。
    {
        QMutexLocker locker(&m_workerMutex);
        if (m_pendingComponents.isEmpty()) {
            return;
        }
        componentId = m_pendingComponents.dequeue();
    }

    QSharedPointer<ComponentData> data = m_cachedData.value(componentId);  ///< 当前元器件的缓存数据。
    QObject* worker = createWorker();  ///< 当前元器件对应的导出 Worker。

    {
        QMutexLocker locker(&m_workerMutex);
        m_activeWorkers.insert(worker);  // 使用 QSet 而非 QList，O(1) 插入和删除
    }

    ExportItemStatus statusSnapshot;  ///< 当前元器件状态的线程安全快照。
    ExportTypeProgress progressSnapshot;  ///< 启动 Worker 后的阶段进度快照。
    {
        QMutexLocker locker(&m_progressMutex);
        auto it = m_progress.itemStatus.find(componentId);  ///< 当前元器件的状态记录迭代器。
        if (it != m_progress.itemStatus.end()) {
            it->status = ExportItemStatus::Status::InProgress;
            it->startTime = QDateTime::currentDateTime();
            statusSnapshot = it.value();
        }
        m_progress.inProgressCount++;
        progressSnapshot = m_progress;
    }

    qInfo() << "ExportTypeStage::startNextWorker: Emitting itemStatusChanged for" << componentId
            << "status:" << (int)statusSnapshot.status;
    emit itemStatusChanged(componentId, statusSnapshot);
    emit progressChanged(progressSnapshot);

    startWorker(worker, componentId, data);
}

/**
 * @brief 取消待处理任务并保留活跃 Worker 的收敛过程。
 */
void ExportTypeStage::cancel() {
    if (!m_isRunning.load()) {
        return;
    }

    qDebug() << "ExportTypeStage:" << m_typeName << "cancelling...";
    m_cancelled.store(true);

    bool hasActiveWorkers = false;  ///< 是否仍有 Worker 在执行。

    {
        QMutexLocker locker(&m_workerMutex);
        m_pendingComponents.clear();
        hasActiveWorkers = !m_activeWorkers.isEmpty();
        if (!hasActiveWorkers) {
            m_isRunning.store(false);
        }
    }

    qDebug() << "ExportTypeStage:" << m_typeName << "cancelled";
}

/**
 * @brief 等待线程池退出并清理阶段内部的任务状态。
 * @param timeoutMs 超时时间（毫秒）。
 */
bool ExportTypeStage::waitForFinished(int timeoutMs) {
    m_cancelled.store(true);
    m_threadPool.clear();

    if (!m_threadPool.waitForDone(timeoutMs)) {
        qWarning() << "ExportTypeStage:" << m_typeName << "thread pool did not finish in" << timeoutMs << "ms";
        return false;
    }

    {
        QMutexLocker locker(&m_workerMutex);
        m_pendingComponents.clear();
        m_activeWorkers.clear();
    }
    m_isRunning.store(false);
    m_isExporting.store(false);
    return true;
}

/**
 * @brief 等待库级导出线程退出，并在成功后清空线程指针。
 * @param thread 目标线程。
 * @param timeoutMs 超时时间（毫秒）。
 */
bool ExportTypeStage::waitForWorkerThread(QThread*& thread, int timeoutMs) {
    m_cancelled.store(true);

    if (!thread || !thread->isRunning()) {
        thread = nullptr;
        return true;
    }

    qDebug() << "ExportTypeStage:" << m_typeName << "waiting for worker thread to finish...";
    thread->quit();
    if (!thread->wait(timeoutMs)) {
        qWarning() << "ExportTypeStage:" << m_typeName << "thread did not finish in" << timeoutMs
                   << "ms, leaving it to finish asynchronously";
        return false;
    }

    thread = nullptr;
    return true;
}

/**
 * @brief 返回受互斥锁保护的进度快照。
 */
ExportTypeProgress ExportTypeStage::getProgress() const {
    QMutexLocker locker(&m_progressMutex);
    return m_progress;
}

/**
 * @brief 查询阶段是否仍有未完成的导出任务。
 */
bool ExportTypeStage::isRunning() const {
    return m_isRunning.load();
}

/**
 * @brief 查询是否仍有已提交但尚未回调完成的 Worker。
 */
bool ExportTypeStage::hasActiveWorkers() const {
    QMutexLocker locker(&m_workerMutex);
    return !m_activeWorkers.isEmpty();
}

/**
 * @brief 为一个元器件创建待处理状态记录。
 * @param componentId 元器件编号。
 */
void ExportTypeStage::initItemProgress(const QString& componentId) {
    QMutexLocker locker(&m_progressMutex);
    ExportItemStatus status;  ///< 新建元器件的初始待处理状态。
    status.status = ExportItemStatus::Status::Pending;
    m_progress.itemStatus[componentId] = status;
    qInfo() << "ExportTypeStage::initItemProgress:" << m_typeName << "componentId:" << componentId
            << "total items now:" << m_progress.itemStatus.size();
}

/**
 * @brief 收敛一个已完成 Worker 的进度和结果。
 * @param worker 已完成的 Worker。
 * @param componentId 已处理的元器件编号。
 * @param success 是否导出成功。
 * @param error 导出失败时的错误信息。
 */
void ExportTypeStage::completeItemProgress(QObject* worker,
                                           const QString& componentId,
                                           bool success,
                                           const QString& error) {
    // 1. 从活跃worker集合移除
    bool shouldStartNext = false;  ///< 是否还有待处理元器件需要启动。
    bool hasActiveWorkers = false;  ///< 是否仍有其他 Worker 在执行。
    {
        QMutexLocker workerLocker(&m_workerMutex);
        if (worker) {
            m_activeWorkers.remove(worker);
        }
        // 检查是否还有待处理的worker
        shouldStartNext = !m_pendingComponents.isEmpty();
        hasActiveWorkers = !m_activeWorkers.isEmpty();
    }

    ExportItemStatus statusSnapshot;  ///< 当前元器件完成后的状态快照。
    ExportTypeProgress progressSnapshot;  ///< 当前阶段完成后的进度快照。
    bool shouldComplete = false;  ///< 当前阶段是否已达到完成条件。

    // 2. 更新进度
    {
        QMutexLocker locker(&m_progressMutex);

        auto it = m_progress.itemStatus.find(componentId);  ///< 当前元器件的状态记录迭代器。
        if (it == m_progress.itemStatus.end()) {
            qInfo() << "ExportTypeStage::completeItemProgress: componentId not found:" << componentId;
            return;
        }

        ExportItemStatus& status = it.value();  ///< 当前元器件的可写状态记录。
        status.status = success ? ExportItemStatus::Status::Success : ExportItemStatus::Status::Failed;
        status.endTime = QDateTime::currentDateTime();
        if (!success) {
            status.errorMessage = error;
        }
        statusSnapshot = status;

        m_progress.completedCount++;
        if (success) {
            m_progress.successCount++;
        } else {
            m_progress.failedCount++;
        }
        m_progress.inProgressCount--;
        progressSnapshot = m_progress;

        const bool cancelledAndDrained =
            m_cancelled.load() && !hasActiveWorkers && !shouldStartNext;  ///< 取消后队列是否已排空。
        shouldComplete = m_progress.completedCount >= m_progress.totalCount || cancelledAndDrained;
    }

    emit itemStatusChanged(componentId, statusSnapshot);
    emit progressChanged(progressSnapshot);

    if (shouldComplete) {
        qInfo() << "ExportTypeStage::completeItemProgress: Emitting itemStatusChanged for" << componentId
                << "status:" << (int)statusSnapshot.status;
        qDebug() << "ExportTypeStage:" << m_typeName << "completed. Success:" << progressSnapshot.successCount
                 << "Failed:" << progressSnapshot.failedCount << "Skipped:" << progressSnapshot.skippedCount;
        m_isRunning.store(false);
        emit completed(progressSnapshot.successCount, progressSnapshot.failedCount, progressSnapshot.skippedCount);
        return;
    }

    // 3. 启动下一个待处理的worker（按需创建）
    if (shouldStartNext && !m_cancelled.load()) {
        startNextWorker();
    }
}

/**
 * @brief 完成一个无需执行导出的元器件，并将其计入跳过统计。
 * @param worker 工作对象。
 * @param componentId 元器件编号。
 * @param reason 失败原因。
 */
void ExportTypeStage::completeSkippedItemProgress(QObject* worker, const QString& componentId, const QString& reason) {
    bool shouldStartNext = false;  ///< 是否还有待处理元器件需要启动。
    bool hasActiveWorkers = false;  ///< 是否仍有其他 Worker 在执行。
    {
        QMutexLocker workerLocker(&m_workerMutex);
        if (worker)
            m_activeWorkers.remove(worker);
        shouldStartNext = !m_pendingComponents.isEmpty();
        hasActiveWorkers = !m_activeWorkers.isEmpty();
    }

    ExportItemStatus statusSnapshot;  ///< 被跳过元器件的状态快照。
    ExportTypeProgress progressSnapshot;  ///< 跳过当前元器件后的进度快照。
    bool shouldComplete = false;  ///< 当前阶段是否已达到完成条件。
    {
        QMutexLocker locker(&m_progressMutex);
        auto it = m_progress.itemStatus.find(componentId);  ///< 当前元器件的状态记录迭代器。
        if (it == m_progress.itemStatus.end())
            return;

        ExportItemStatus& status = it.value();  ///< 当前元器件的可写状态记录。
        status.status = ExportItemStatus::Status::Skipped;
        status.errorMessage = reason;
        status.endTime = QDateTime::currentDateTime();
        statusSnapshot = status;

        ++m_progress.completedCount;
        ++m_progress.skippedCount;
        --m_progress.inProgressCount;
        progressSnapshot = m_progress;

        const bool cancelledAndDrained =
            m_cancelled.load() && !hasActiveWorkers && !shouldStartNext;  ///< 取消后队列是否已排空。
        shouldComplete = m_progress.completedCount >= m_progress.totalCount || cancelledAndDrained;
    }

    emit itemStatusChanged(componentId, statusSnapshot);
    emit progressChanged(progressSnapshot);

    if (shouldComplete) {
        m_isRunning.store(false);
        emit completed(progressSnapshot.successCount, progressSnapshot.failedCount, progressSnapshot.skippedCount);
        return;
    }

    if (shouldStartNext && !m_cancelled.load())
        startNextWorker();
}

}  // namespace EasyKiConverter
