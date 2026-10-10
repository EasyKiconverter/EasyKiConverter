#include "ComponentQueueManager.h"

#include <QDebug>

namespace EasyKiConverter {

/**
 * @brief 创建元器件请求队列管理器。
 * @param maxConcurrent 最大并发请求数。
 * @param parent Qt 对象父级。
 */
ComponentQueueManager::ComponentQueueManager(int maxConcurrent, QObject* parent)
    : QObject(parent)
    , m_maxConcurrentRequests(maxConcurrent)
    , m_checkTimer(new QTimer(this))
    , m_totalTimer(new QTimer(this))
    , m_isRunning(false) {
    m_checkTimer->setSingleShot(false);
    m_checkTimer->setInterval(QUEUE_CHECK_INTERVAL_MS);
    connect(m_checkTimer, &QTimer::timeout, this, &ComponentQueueManager::onCheckTimeout);

    m_totalTimer->setSingleShot(true);
    m_totalTimer->setInterval(TOTAL_TIMEOUT_MS);
    connect(m_totalTimer, &QTimer::timeout, this, &ComponentQueueManager::onTotalTimeout);
}

/**
 * @brief 销毁元器件请求队列管理器并停止队列。
 */
ComponentQueueManager::~ComponentQueueManager() {
    stop();
}

/**
 * @brief 启动元器件请求队列。
 * @param componentIds 待处理的元器件编号列表。
 */
void ComponentQueueManager::start(const QStringList& componentIds) {
    if (m_isRunning) {
        qWarning() << "ComponentQueueManager: Already running, ignoring new request";
        return;
    }

    qDebug() << "ComponentQueueManager: Starting with" << componentIds.size()
             << "components, max concurrent:" << m_maxConcurrentRequests;

    m_requestQueue = componentIds;
    m_pendingComponents.clear();
    m_isRunning = true;

    m_totalTimer->start();
    m_checkTimer->start();

    checkAndProcessNext();
}

/**
 * @brief 停止队列并清理未开始的请求。
 */
void ComponentQueueManager::stop() {
    if (!m_isRunning) {
        return;
    }

    qDebug() << "ComponentQueueManager: Stopping";

    m_checkTimer->stop();
    m_totalTimer->stop();

    m_requestQueue.clear();
    m_pendingComponents.clear();
    m_isRunning = false;
}

/**
 * @brief 设置最大并发请求数。
 * @param maxConcurrent 新的最大并发请求数。
 */
void ComponentQueueManager::setMaxConcurrentRequests(int maxConcurrent) {
    if (maxConcurrent <= 0) {
        return;
    }
    if (m_maxConcurrentRequests != maxConcurrent) {
        m_maxConcurrentRequests = maxConcurrent;
        qDebug() << "ComponentQueueManager: Max concurrent requests set to" << m_maxConcurrentRequests;
    }
}

/**
 * @brief 根据可用并发槽位发出下一批请求。
 */
void ComponentQueueManager::checkAndProcessNext() {
    if (!m_isRunning) {
        m_checkTimer->stop();
        return;
    }

    int activeCount = m_pendingComponents.size();  // 当前正在处理的请求数量。

    // 如果所有槽位都满了，启用定时器作为安全网（防止某些请求完成但信号未触发的情况）
    // 否则禁用定时器（正常情况下 requestCompleted 会直接触发处理）
    if (activeCount >= m_maxConcurrentRequests && !m_requestQueue.isEmpty()) {
        if (!m_checkTimer->isActive()) {
            m_checkTimer->start();
        }
    } else {
        m_checkTimer->stop();
    }

    while (activeCount < m_maxConcurrentRequests && !m_requestQueue.isEmpty()) {
        QString componentId = m_requestQueue.takeFirst();  // 从队首取出的元器件编号。

        if (m_pendingComponents.contains(componentId)) {
            qDebug() << "Component" << componentId << "already pending, skipping";
            continue;
        }

        m_pendingComponents.insert(componentId);
        activeCount++;

        qDebug() << "ComponentQueueManager: Request ready for:" << componentId << "(Active:" << activeCount << "/"
                 << m_maxConcurrentRequests << "Remaining:" << m_requestQueue.size() << ")";

        emit requestReady(componentId);
    }

    if (m_requestQueue.isEmpty() && activeCount == 0) {
        qDebug() << "ComponentQueueManager: All components processed. Queue empty.";
        m_checkTimer->stop();
        emit queueEmpty();
        stop();
    }
}

/**
 * @brief 重置队列状态。
 */
void ComponentQueueManager::reset() {
    stop();
}

/**
 * @brief 标记一个元器件请求已完成并继续调度队列。
 * @param componentId 已完成请求的元器件编号。
 */
void ComponentQueueManager::requestCompleted(const QString& componentId) {
    if (!m_isRunning) {
        return;
    }

    m_pendingComponents.remove(componentId);
    qDebug() << "ComponentQueueManager: Request completed for:" << componentId
             << "(Remaining active:" << m_pendingComponents.size() << ")";

    checkAndProcessNext();
}

/**
 * @brief 返回当前活动请求数量。
 * @return 当前正在处理的请求数量。
 */
int ComponentQueueManager::activeRequestCount() const {
    return m_pendingComponents.size();
}

/**
 * @brief 返回队列是否正在运行。
 * @return 队列正在运行时返回 true。
 */
bool ComponentQueueManager::isRunning() const {
    return m_isRunning;
}

/**
 * @brief 返回最大并发请求数。
 * @return 当前配置的最大并发请求数。
 */
int ComponentQueueManager::maxConcurrentRequests() const {
    return m_maxConcurrentRequests;
}

/**
 * @brief 处理队列检查定时器超时。
 */
void ComponentQueueManager::onCheckTimeout() {
    checkAndProcessNext();
}

/**
 * @brief 处理队列总超时并停止队列。
 */
void ComponentQueueManager::onTotalTimeout() {
    qWarning() << "ComponentQueueManager: Queue timeout reached after" << TOTAL_TIMEOUT_MS << "ms";
    qWarning() << "ComponentQueueManager: Pending:" << m_pendingComponents.size()
               << "Remaining in queue:" << m_requestQueue.size();

    emit timeout();
    stop();
}

}  // namespace EasyKiConverter
