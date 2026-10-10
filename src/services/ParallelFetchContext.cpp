#include "ParallelFetchContext.h"

#include "ComponentData.h"

#include <QMutexLocker>

namespace EasyKiConverter {

/**
 * @brief 初始化并行获取上下文及其线程安全状态。
 * @param parent Qt 对象父级。
 */
ParallelFetchContext::ParallelFetchContext(QObject* parent)
    // 初始化计数器和完成状态。
    : QObject(parent), m_totalCount(0), m_completedCount(0), m_isAllDone(false) {}

/**
 * @brief 销毁并行获取上下文。
 */
ParallelFetchContext::~ParallelFetchContext() = default;

/**
 * @brief 开始新一轮批量获取并清理上一轮结果。
 * @param totalCount 本轮请求总数。
 */
void ParallelFetchContext::start(int totalCount) {
    bool shouldEmit = false;  // 是否需要立即发出完成信号。
    {
        QMutexLocker locker(&m_mutex);
        m_totalCount = totalCount;
        m_completedCount = 0;
        m_isAllDone = totalCount == 0;
        m_collectedData.clear();
        m_failedComponents.clear();
        m_finishedComponents.clear();
        shouldEmit = m_isAllDone;
    }
    if (shouldEmit) {
        /**
         * @brief 空批量请求立即通知完成。
         */
        Q_EMIT allCompleted({});
    }
}

/**
 * @brief 记录一个成功结果并检查批量任务是否完成。
 * @param componentId 元件编号。
 * @param data 元件数据。
 */
void ParallelFetchContext::markCompleted(const QString& componentId, const ComponentData& data) {
    const QString normalizedId = componentId.toUpper();  // 规范化元件编号。
    {
        QMutexLocker locker(&m_mutex);
        if (m_finishedComponents.contains(normalizedId)) {
            return;
        }
        m_finishedComponents.insert(normalizedId);
        m_collectedData.insert(normalizedId, data);
        ++m_completedCount;
    }
    checkCompletion();
}

/**
 * @brief 记录一个失败原因并检查批量任务是否完成。
 * @param componentId 元件编号。
 * @param error 失败原因。
 */
void ParallelFetchContext::markFailed(const QString& componentId, const QString& error) {
    const QString normalizedId = componentId.toUpper();  // 规范化元件编号。
    {
        QMutexLocker locker(&m_mutex);
        if (m_finishedComponents.contains(normalizedId)) {
            return;
        }
        m_finishedComponents.insert(normalizedId);
        m_failedComponents.insert(normalizedId, error);
        ++m_completedCount;
    }
    checkCompletion();
}

/**
 * @brief 返回批量获取是否已经发出完成信号。
 * @return 已发出完成信号时返回 true。
 */
bool ParallelFetchContext::isAllDone() const {
    QMutexLocker locker(&m_mutex);
    return m_isAllDone;
}

/**
 * @brief 返回已经结束的请求数量。
 * @return 已完成请求数量。
 */
int ParallelFetchContext::completedCount() const {
    QMutexLocker locker(&m_mutex);
    return m_completedCount;
}

/**
 * @brief 返回本轮批量获取的请求总数。
 * @return 请求总数。
 */
int ParallelFetchContext::totalCount() const {
    QMutexLocker locker(&m_mutex);
    return m_totalCount;
}

/**
 * @brief 返回当前已成功收集的元件数据。
 * @return 已收集元件数据列表。
 */
QList<ComponentData> ParallelFetchContext::collectedData() const {
    QMutexLocker locker(&m_mutex);
    return m_collectedData.values();
}

/**
 * @brief 返回当前失败元件及其错误信息。
 * @return 失败元件映射。
 */
QMap<QString, QString> ParallelFetchContext::failedComponents() const {
    QMutexLocker locker(&m_mutex);
    return m_failedComponents;
}

/**
 * @brief 在所有请求结束后发出一次批量完成信号。
 */
void ParallelFetchContext::checkCompletion() {
    QList<ComponentData> dataToEmit;  // 待发送的成功元件数据。
    bool shouldEmit = false;  // 是否需要发出完成信号。

    {
        QMutexLocker locker(&m_mutex);
        if (m_completedCount >= m_totalCount && !m_isAllDone) {
            m_isAllDone = true;
            shouldEmit = true;
            if (m_totalCount > 0) {
                dataToEmit = m_collectedData.values();
            }
        }
    }

    if (shouldEmit) {
        Q_EMIT allCompleted(dataToEmit);
    }
}

}  // namespace EasyKiConverter
