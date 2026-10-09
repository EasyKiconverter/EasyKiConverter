#include "NetworkClient.h"

#include "BlockingRequestContext.h"
#include "core/utils/GzipUtils.h"
#include "core/utils/UrlUtils.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QElapsedTimer>
#include <QMetaObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStringList>
#include <QTimer>

#include <limits>
#include <memory>

namespace EasyKiConverter {

NetworkClient::NetworkClient() {
    m_networkThread.setObjectName(QStringLiteral("EasyKiConverterNetworkThread"));
    m_networkThread.start();

    m_asyncNetworkManager = new QNetworkAccessManager();
    m_asyncNetworkManager->moveToThread(&m_networkThread);
}

NetworkClient::~NetworkClient() {
    cancelAllRequests();

    if (m_asyncNetworkManager) {
        QMetaObject::invokeMethod(m_asyncNetworkManager, &QObject::deleteLater, Qt::QueuedConnection);
        m_asyncNetworkManager = nullptr;
    }

    m_networkThread.quit();
    m_networkThread.wait();
}

/**
 * @brief 判断响应数据是否为 Gzip 压缩数据。
 * @param data 待检查的响应数据。
 * @return 数据带有 Gzip 标识时返回 true。
 */
bool NetworkClient::isGzipCompressed(const QByteArray& data) {
    return GzipUtils::isGzipped(data);
}

/**
 * @brief 解压 Gzip 响应数据。
 * @param data Gzip 压缩数据。
 * @return 解压后的数据，解压失败时返回空数据。
 */
QByteArray NetworkClient::decompressGzip(const QByteArray& data) {
    GzipUtils::DecompressResult decompResult = GzipUtils::decompress(data);  // Gzip 解压结果。
    if (!decompResult.success) {
        qWarning() << "Gzip decompression failed";
        return QByteArray();
    }
    return decompResult.data;
}

/**
 * @brief 发送使用默认资源类型的同步 GET 请求。
 * @param url 请求地址。
 * @param policy 重试策略。
 * @return 网络请求结果。
 */
NetworkResult NetworkClient::get(const QUrl& url, const RetryPolicy& policy) {
    return executeRequest(url, QByteArray(), ResourceType::Unknown, policy);
}

/**
 * @brief 发送带资源类型的同步 GET 请求。
 * @param url 请求地址。
 * @param resourceType 资源类型。
 * @param policy 重试策略。
 * @return 网络请求结果。
 */
NetworkResult NetworkClient::get(const QUrl& url, ResourceType resourceType, const RetryPolicy& policy) {
    return executeRequest(url, QByteArray(), resourceType, policy);
}

/**
 * @brief 发送使用默认资源类型的同步 POST 请求。
 * @param url 请求地址。
 * @param body POST 请求体。
 * @param policy 重试策略。
 * @return 网络请求结果。
 */
NetworkResult NetworkClient::post(const QUrl& url, const QByteArray& body, const RetryPolicy& policy) {
    return executeRequest(url, body, ResourceType::Unknown, policy);
}

/**
 * @brief 发送带资源类型的同步 POST 请求。
 * @param url 请求地址。
 * @param body POST 请求体。
 * @param resourceType 资源类型。
 * @param policy 重试策略。
 * @return 网络请求结果。
 */
NetworkResult NetworkClient::post(const QUrl& url,
                                  const QByteArray& body,
                                  ResourceType resourceType,
                                  const RetryPolicy& policy) {
    return executeRequest(url, body, resourceType, policy);
}

/**
 * @brief 获取当前网络请求运行统计。
 * @return 当前网络请求统计快照。
 */
NetworkRuntimeStats NetworkClient::runtimeStats() const {
    NetworkRuntimeStats stats;  // 汇总后的网络运行统计。

    QMutexLocker locker(&m_asyncQueueMutex);
    for (auto it = m_resourceStats.cbegin(); it != m_resourceStats.cend(); ++it) {
        const NetworkResourceStats& snapshot = it.value().snapshot;  // 当前资源统计快照。
        stats.totalQueuedRequests += snapshot.queuedRequests;
        stats.totalActiveRequests += snapshot.activeRequests;
        stats.totalStartedRequests += snapshot.startedRequests;
        stats.totalCompletedRequests += snapshot.completedRequests;
        stats.totalSucceededRequests += snapshot.succeededRequests;
        stats.totalFailedRequests += snapshot.failedRequests;
        stats.totalCancelledRequests += snapshot.cancelledRequests;
        stats.totalTimeoutRequests += snapshot.timeoutRequests;
        stats.totalRateLimitedRequests += snapshot.rateLimitedRequests;
        stats.totalRetryAttempts += snapshot.retryAttempts;
        stats.totalBackpressureEvents += snapshot.backpressureEvents;
        stats.resources.append(snapshot);
    }

    return stats;
}

/**
 * @brief 将网络请求运行统计格式化为诊断文本。
 * @return 可读的网络请求统计文本。
 */
QString NetworkClient::formatRuntimeStats() const {
    const NetworkRuntimeStats stats = runtimeStats();  // 当前网络运行统计。
    QStringList lines;  // 格式化后的诊断文本行。
    lines << QStringLiteral(
                 "NetworkRuntimeStats total{queued=%1 active=%2 started=%3 completed=%4 ok=%5 fail=%6 "
                 "cancelled=%7 timeout=%8 rateLimited=%9 retries=%10 backpressure=%11}")
                 .arg(stats.totalQueuedRequests)
                 .arg(stats.totalActiveRequests)
                 .arg(stats.totalStartedRequests)
                 .arg(stats.totalCompletedRequests)
                 .arg(stats.totalSucceededRequests)
                 .arg(stats.totalFailedRequests)
                 .arg(stats.totalCancelledRequests)
                 .arg(stats.totalTimeoutRequests)
                 .arg(stats.totalRateLimitedRequests)
                 .arg(stats.totalRetryAttempts)
                 .arg(stats.totalBackpressureEvents);

    for (const NetworkResourceStats& resource : stats.resources) {
        lines << QStringLiteral(
                     "  [%1] queued=%2/%3 active=%4/%5 backpressure=%6 started=%7 completed=%8 ok=%9 fail=%10 "
                     "cancelled=%11 timeout=%12 rateLimited=%13 retries=%14 queueDelay(last/avg/max)=%15/%16/%17ms "
                     "latency(last/avg/max)=%18/%19/%20ms")
                     .arg(resource.profileName)
                     .arg(resource.queuedRequests)
                     .arg(resource.peakQueuedRequests)
                     .arg(resource.activeRequests)
                     .arg(resource.peakActiveRequests)
                     .arg(resource.backpressureEvents)
                     .arg(resource.startedRequests)
                     .arg(resource.completedRequests)
                     .arg(resource.succeededRequests)
                     .arg(resource.failedRequests)
                     .arg(resource.cancelledRequests)
                     .arg(resource.timeoutRequests)
                     .arg(resource.rateLimitedRequests)
                     .arg(resource.retryAttempts)
                     .arg(resource.lastQueueDelayMs)
                     .arg(resource.averageQueueDelayMs)
                     .arg(resource.maxQueueDelayMs)
                     .arg(resource.lastLatencyMs)
                     .arg(resource.averageLatencyMs)
                     .arg(resource.maxLatencyMs);
    }

    return lines.join(QLatin1Char('\n'));
}

/**
 * @brief 创建异步 GET 请求并加入调度队列。
 * @param url 请求地址。
 * @param resourceType 资源类型。
 * @param policy 重试策略。
 * @return 创建的异步请求对象。
 */
AsyncNetworkRequest* NetworkClient::getAsync(const QUrl& url, ResourceType resourceType, const RetryPolicy& policy) {
    return enqueueAsyncRequest(url, QByteArray(), resourceType, policy);
}

/**
 * @brief 创建异步 POST 请求并加入调度队列。
 * @param url 请求地址。
 * @param body POST 请求体。
 * @param resourceType 资源类型。
 * @param policy 重试策略。
 * @return 创建的异步请求对象。
 */
AsyncNetworkRequest* NetworkClient::postAsync(const QUrl& url,
                                              const QByteArray& body,
                                              ResourceType resourceType,
                                              const RetryPolicy& policy) {
    return enqueueAsyncRequest(url, body, resourceType, policy);
}

/**
 * @brief 填充网络请求诊断中的 URL、主机和资源类型。
 * @param diag 待填充的诊断对象。
 * @param url 请求地址。
 * @param resourceType 资源类型。
 */
void NetworkClient::populateDiagnostic(NetworkDiagnostic& diag, const QUrl& url, ResourceType resourceType) {
    diag.url = url.toString();
    diag.host = url.host();
    diag.resourceType = resourceType;
    diag.profileName = RequestProfiles::fromType(resourceType).name;
}

/**
 * @brief 执行同步网络请求并等待异步结果。
 * @param url 请求地址。
 * @param body 请求体。
 * @param resourceType 资源类型。
 * @param policy 重试策略。
 * @return 网络请求结果。
 */
NetworkResult NetworkClient::executeRequest(const QUrl& url,
                                            const QByteArray& body,
                                            ResourceType resourceType,
                                            const RetryPolicy& policy) {
    // 同步路径：在创建 request 前做 URL 校验，避免白名单拒绝后死锁
    // （QWaitCondition::wait() 不处理事件循环，queued 完成信号无法到达）
    {
        QString errorMsg;  // URL 白名单校验错误。
        if (!UrlUtils::isAllowedUrl(url, resourceType, &errorMsg)) {
            qWarning() << "NetworkClient: URL rejected by whitelist (sync):" << errorMsg;
            NetworkResult result;  // URL 被拒绝时返回的失败结果。
            populateDiagnostic(result.diagnostic, url, resourceType);
            result.error = errorMsg;
            result.success = false;
            result.diagnostic.errorType = NetworkErrorType::Other;
            result.diagnostic.errorMessage = errorMsg;
            return result;
        }
    }

    auto* request = enqueueAsyncRequest(url, body, resourceType, policy);  // 创建的异步请求对象。
    if (!request) {
        NetworkResult result;  // 创建请求失败时返回的失败结果。
        populateDiagnostic(result.diagnostic, url, resourceType);
        result.error = QStringLiteral("Failed to create async request");
        result.success = false;
        result.diagnostic.errorType = NetworkErrorType::Other;
        result.diagnostic.errorMessage = result.error;
        return result;
    }

    auto context = std::make_shared<BlockingRequestContext>();  // 同步等待异步结果的上下文。
    QObject::connect(
        request,
        &AsyncNetworkRequest::finished,
        request,
        [context](const NetworkResult& result) { context->complete(result); },
        Qt::DirectConnection);

    const NetworkResult result = context->wait();  // 异步请求完成后的结果。
    QMetaObject::invokeMethod(request, &QObject::deleteLater, Qt::QueuedConnection);
    return result;
}

/**
 * @brief 校验并将异步请求加入调度队列。
 * @param url 请求地址。
 * @param body 请求体。
 * @param resourceType 资源类型。
 * @param policy 重试策略。
 * @return 排队的异步请求对象，拒绝时返回已完成的失败请求。
 */
AsyncNetworkRequest* NetworkClient::enqueueAsyncRequest(const QUrl& url,
                                                        const QByteArray& body,
                                                        ResourceType resourceType,
                                                        const RetryPolicy& policy) {
    if (!m_asyncNetworkManager) {
        return nullptr;
    }

    // URL 白名单校验：拒绝非预期的 scheme 或 host
    {
        QString errorMsg;  // URL 白名单校验错误。
        if (!UrlUtils::isAllowedUrl(url, resourceType, &errorMsg)) {
            qWarning() << "NetworkClient: URL rejected by whitelist:" << errorMsg;
            // 返回已完成的失败 request，避免调用方需处理 nullptr
            NetworkResult failResult;  // URL 被拒绝时返回的失败请求结果。
            failResult.success = false;
            failResult.wasCancelled = false;
            failResult.error = errorMsg;
            failResult.diagnostic.url = url.toString();
            failResult.diagnostic.host = url.host();
            failResult.diagnostic.resourceType = resourceType;
            failResult.diagnostic.profileName = RequestProfiles::fromType(resourceType).name;
            failResult.diagnostic.errorType = NetworkErrorType::Other;
            failResult.diagnostic.errorMessage = errorMsg;
            return AsyncNetworkRequest::createFinished(failResult);
        }
    }

    auto* request =  // 新建的异步网络请求对象。
        new AsyncNetworkRequest(url, m_asyncNetworkManager, resourceType, policy, body, nullptr);
    request->moveToThread(&m_networkThread);

    const RequestProfile profile = RequestProfiles::fromType(resourceType);  // 当前资源的调度配置。
    {
        QMutexLocker locker(&m_asyncQueueMutex);
        PendingAsyncRequest pending;  // 待调度请求记录。
        pending.request = request;
        pending.resourceType = resourceType;
        pending.priority = profile.priority;
        pending.sequence = m_asyncSequence++;
        pending.enqueuedAtMs = QDateTime::currentMSecsSinceEpoch();
        m_pendingAsyncRequests.append(pending);
        m_liveAsyncRequests.append(request);
        updateStatsForEnqueuedRequest(resourceType);
    }

    QObject::connect(
        request,
        &AsyncNetworkRequest::finished,
        this,
        [this, request, resourceType](const NetworkResult& result) {
            updateStatsForCompletedRequest(resourceType, result);
            {
                QMutexLocker locker(&m_asyncQueueMutex);
                m_liveAsyncRequests.removeAll(request);
            }
            onAsyncRequestFinished(resourceType);
        },
        Qt::QueuedConnection);

    scheduleAsyncPump();
    return request;
}

/**
 * @brief 安排异步请求队列的下一次调度。
 */
void NetworkClient::scheduleAsyncPump() {
    bool shouldSchedule = false;  // 是否需要向事件队列提交调度任务。
    {
        QMutexLocker locker(&m_asyncQueueMutex);
        if (!m_asyncPumpScheduled) {
            m_asyncPumpScheduled = true;
            shouldSchedule = true;
        }
    }

    if (shouldSchedule) {
        QMetaObject::invokeMethod(this, &NetworkClient::pumpAsyncQueue, Qt::QueuedConnection);
    }
}

/**
 * @brief 按并发和优先级限制启动等待中的异步请求。
 */
void NetworkClient::pumpAsyncQueue() {
    while (true) {
        QPointer<AsyncNetworkRequest> requestToStart;  // 本轮选中的异步请求。
        ResourceType resourceType = ResourceType::Unknown;  // 本轮请求的资源类型。

        {
            QMutexLocker locker(&m_asyncQueueMutex);
            int bestIndex = -1;  // 当前可启动请求在队列中的索引。
            int bestPriority = std::numeric_limits<int>::max();  // 当前候选请求的最低优先级值。
            quint64 bestSequence = std::numeric_limits<quint64>::max();  // 当前候选请求的最早入队序号。

            for (int i = m_pendingAsyncRequests.size() - 1; i >= 0; --i) {
                const PendingAsyncRequest& pending =  // 反向清理阶段的待调度请求记录。
                    m_pendingAsyncRequests.at(i);
                if (!pending.request || pending.request->isFinished()) {
                    m_pendingAsyncRequests.removeAt(i);
                }
            }

            for (int i = 0; i < m_pendingAsyncRequests.size(); ++i) {
                const PendingAsyncRequest& pending =  // 当前优先级选择阶段的待调度请求记录。
                    m_pendingAsyncRequests.at(i);
                if (!pending.request || pending.request->isFinished()) {
                    continue;
                }

                const RequestProfile profile = RequestProfiles::fromType(pending.resourceType);  // 候选请求调度配置。
                const int activeCount =  // 候选请求资源类型当前活动数量。
                    m_activeAsyncRequestsByType.value(static_cast<int>(pending.resourceType), 0);
                if (activeCount >= profile.maxConcurrent) {
                    continue;
                }

                if (pending.priority < bestPriority ||
                    (pending.priority == bestPriority && pending.sequence < bestSequence)) {
                    bestIndex = i;
                    bestPriority = pending.priority;
                    bestSequence = pending.sequence;
                }
            }

            if (bestIndex < 0) {
                m_asyncPumpScheduled = false;
                break;
            }

            PendingAsyncRequest pending = m_pendingAsyncRequests.takeAt(bestIndex);  // 取出的待调度请求记录。
            requestToStart = pending.request;
            resourceType = pending.resourceType;
            m_activeAsyncRequestsByType[static_cast<int>(resourceType)] =
                m_activeAsyncRequestsByType.value(static_cast<int>(resourceType), 0) + 1;
            const qint64 queueDelayMs =  // 请求从入队到启动前的等待时长。
                qMax<qint64>(0, QDateTime::currentMSecsSinceEpoch() - pending.enqueuedAtMs);
            updateStatsForDequeuedRequest(resourceType, queueDelayMs);
            updateStatsForStartedRequest(resourceType);
        }

        if (!requestToStart) {
            onAsyncRequestFinished(resourceType);
            continue;
        }

        QMetaObject::invokeMethod(
            requestToStart,
            [requestToStart]() {
                if (requestToStart) {
                    requestToStart->start();
                }
            },
            Qt::QueuedConnection);
    }
}

/**
 * @brief 取消队列中和运行中的全部异步请求。
 */
void NetworkClient::cancelAllRequests() {
    QList<QPointer<AsyncNetworkRequest>> requestsToCancel;  // 待取消的异步请求快照。
    {
        QMutexLocker locker(&m_asyncQueueMutex);
        requestsToCancel = m_liveAsyncRequests;
        m_pendingAsyncRequests.clear();
        m_asyncPumpScheduled = false;
    }

    for (const QPointer<AsyncNetworkRequest>& request : std::as_const(requestsToCancel)) {
        if (!request || request->isFinished()) {
            continue;
        }

        if (request->thread() == QThread::currentThread()) {
            request->cancel();
        } else if (QThread::currentThread() == &m_networkThread) {
            request->cancel();
        } else {
            const Qt::ConnectionType connectionType =
                m_networkThread.isRunning() ? Qt::BlockingQueuedConnection : Qt::QueuedConnection;
            QMetaObject::invokeMethod(request, &AsyncNetworkRequest::cancel, connectionType);
        }
    }
}

/**
 * @brief 收敛异步请求完成后的活动计数并继续调度。
 * @param resourceType 已完成请求的资源类型。
 */
void NetworkClient::onAsyncRequestFinished(ResourceType resourceType) {
    {
        QMutexLocker locker(&m_asyncQueueMutex);
        const int key = static_cast<int>(resourceType);  // 资源类型对应的统计键。
        const int activeCount = m_activeAsyncRequestsByType.value(key, 0);  // 当前资源活动请求数量。
        if (activeCount <= 1) {
            m_activeAsyncRequestsByType.remove(key);
        } else {
            m_activeAsyncRequestsByType[key] = activeCount - 1;
        }
        refreshDynamicStatsLocked(resourceType);
    }

    scheduleAsyncPump();
}

/**
 * @brief 记录请求入队后的资源统计。
 * @param resourceType 请求的资源类型。
 */
void NetworkClient::updateStatsForEnqueuedRequest(ResourceType resourceType) {
    const int key = static_cast<int>(resourceType);  // 资源类型对应的统计键。
    MutableResourceStats& stats = m_resourceStats[key];  // 当前资源可变统计对象。
    const RequestProfile profile = RequestProfiles::fromType(resourceType);  // 当前资源调度配置。
    stats.snapshot.resourceType = resourceType;
    stats.snapshot.profileName = profile.name;
    stats.snapshot.maxConcurrent = profile.maxConcurrent;
    if (m_activeAsyncRequestsByType.value(key, 0) >= profile.maxConcurrent) {
        ++stats.snapshot.backpressureEvents;
    }
    refreshDynamicStatsLocked(resourceType);
}

/**
 * @brief 记录请求离开等待队列时的延迟统计。
 * @param resourceType 请求的资源类型。
 * @param queueDelayMs 请求在队列中的等待时长。
 */
void NetworkClient::updateStatsForDequeuedRequest(ResourceType resourceType, qint64 queueDelayMs) {
    const int key = static_cast<int>(resourceType);  // 资源类型对应的统计键。
    MutableResourceStats& stats = m_resourceStats[key];  // 当前资源可变统计对象。
    const RequestProfile profile = RequestProfiles::fromType(resourceType);  // 当前资源调度配置。
    stats.snapshot.resourceType = resourceType;
    stats.snapshot.profileName = profile.name;
    stats.snapshot.maxConcurrent = profile.maxConcurrent;
    stats.snapshot.lastQueueDelayMs = queueDelayMs;
    stats.totalQueueDelayMs += queueDelayMs;
    stats.snapshot.averageQueueDelayMs =
        stats.snapshot.startedRequests > 0
            ? (stats.totalQueueDelayMs / static_cast<qint64>(stats.snapshot.startedRequests))
            : 0;
    stats.snapshot.maxQueueDelayMs = qMax(stats.snapshot.maxQueueDelayMs, queueDelayMs);
    refreshDynamicStatsLocked(resourceType);
}

/**
 * @brief 记录请求开始执行时的资源统计。
 * @param resourceType 请求的资源类型。
 */
void NetworkClient::updateStatsForStartedRequest(ResourceType resourceType) {
    const int key = static_cast<int>(resourceType);  // 资源类型对应的统计键。
    MutableResourceStats& stats = m_resourceStats[key];  // 当前资源可变统计对象。
    const RequestProfile profile = RequestProfiles::fromType(resourceType);  // 当前资源调度配置。
    stats.snapshot.resourceType = resourceType;
    stats.snapshot.profileName = profile.name;
    stats.snapshot.maxConcurrent = profile.maxConcurrent;
    ++stats.snapshot.startedRequests;
    stats.snapshot.averageQueueDelayMs =
        stats.snapshot.startedRequests > 0
            ? (stats.totalQueueDelayMs / static_cast<qint64>(stats.snapshot.startedRequests))
            : 0;
    refreshDynamicStatsLocked(resourceType);
}

/**
 * @brief 根据请求结果更新完成、失败和重试统计。
 * @param resourceType 请求的资源类型。
 * @param result 请求结果。
 */
void NetworkClient::updateStatsForCompletedRequest(ResourceType resourceType, const NetworkResult& result) {
    QMutexLocker locker(&m_asyncQueueMutex);  // 保护资源统计的互斥锁。
    const int key = static_cast<int>(resourceType);  // 资源类型对应的统计键。
    MutableResourceStats& stats = m_resourceStats[key];  // 当前资源可变统计对象。
    const RequestProfile profile = RequestProfiles::fromType(resourceType);  // 当前资源调度配置。
    stats.snapshot.resourceType = resourceType;
    stats.snapshot.profileName = profile.name;
    stats.snapshot.maxConcurrent = profile.maxConcurrent;
    ++stats.snapshot.completedRequests;
    stats.snapshot.retryAttempts += static_cast<quint64>(qMax(0, result.retryCount));
    stats.snapshot.lastLatencyMs = result.elapsedMs;
    stats.totalLatencyMs += result.elapsedMs;
    stats.snapshot.averageLatencyMs =
        stats.snapshot.completedRequests > 0
            ? (stats.totalLatencyMs / static_cast<qint64>(stats.snapshot.completedRequests))
            : 0;
    stats.snapshot.maxLatencyMs = qMax(stats.snapshot.maxLatencyMs, result.elapsedMs);

    if (result.wasCancelled || result.diagnostic.wasCanceled) {
        ++stats.snapshot.cancelledRequests;
    } else if (result.success) {
        ++stats.snapshot.succeededRequests;
    } else {
        ++stats.snapshot.failedRequests;
    }

    if (result.diagnostic.errorType == NetworkErrorType::Timeout) {
        ++stats.snapshot.timeoutRequests;
    }
    if (result.diagnostic.wasRateLimited || result.statusCode == 429) {
        ++stats.snapshot.rateLimitedRequests;
    }
    refreshDynamicStatsLocked(resourceType);
}

/**
 * @brief 在持有统计锁时刷新指定资源的动态计数。
 * @param resourceType 请求的资源类型。
 */
void NetworkClient::refreshDynamicStatsLocked(ResourceType resourceType) {
    const int key = static_cast<int>(resourceType);  // 资源类型对应的统计键。
    MutableResourceStats& stats = m_resourceStats[key];  // 当前资源的可变统计对象。
    int queuedCount = 0;  // 当前资源仍在等待队列中的请求数量。
    for (const PendingAsyncRequest& pending : std::as_const(m_pendingAsyncRequests)) {
        if (pending.request && !pending.request->isFinished() && pending.resourceType == resourceType) {
            ++queuedCount;
        }
    }

    stats.snapshot.queuedRequests = queuedCount;
    stats.snapshot.activeRequests = m_activeAsyncRequestsByType.value(key, 0);
    stats.snapshot.peakQueuedRequests = qMax(stats.snapshot.peakQueuedRequests, queuedCount);
    stats.snapshot.peakActiveRequests = qMax(stats.snapshot.peakActiveRequests, stats.snapshot.activeRequests);
}

/**
 * @brief 计算重试次数对应的退避时长。
 * @param retryCount 当前重试次数。
 * @param policy 重试策略。
 * @return 本次重试等待的毫秒数。
 */
int NetworkClient::calculateRetryDelay(int retryCount, const RetryPolicy& policy) {
    if (retryCount >= static_cast<int>(policy.delays.size())) {
        return policy.delays.back();
    }
    int baseDelay = policy.delays[retryCount];  // 当前重试次数对应的基础延迟。

    int jitterRange = static_cast<int>(baseDelay * policy.jitterFactor);  // 随机抖动范围。
    int jitter = QRandomGenerator::global()->bounded(-jitterRange, jitterRange + 1);  // 本次随机抖动。
    return baseDelay + jitter;
}

/**
 * @brief 判断请求失败是否应进入重试流程。
 * @param statusCode HTTP 状态码。
 * @param error Qt 网络错误类型。
 * @param retryCount 当前重试次数。
 * @param policy 重试策略。
 * @return 满足重试条件时返回 true。
 */
bool NetworkClient::shouldRetry(int statusCode,
                                QNetworkReply::NetworkError error,
                                int retryCount,
                                const RetryPolicy& policy) {
    if (retryCount >= policy.maxRetries) {
        return false;
    }

    if (policy.retryableStatusCodes.contains(statusCode)) {
        return true;
    }

    // 网络层临时错误允许进入重试流程。
    switch (error) {
        case QNetworkReply::TimeoutError:
        case QNetworkReply::TemporaryNetworkFailureError:
        case QNetworkReply::NetworkSessionFailedError:
        case QNetworkReply::UnknownNetworkError:
        case QNetworkReply::RemoteHostClosedError:
        case QNetworkReply::HostNotFoundError:
        case QNetworkReply::ConnectionRefusedError:
        case QNetworkReply::ProxyTimeoutError:
            return true;
        default:
            return false;
    }
}

}  // namespace EasyKiConverter
