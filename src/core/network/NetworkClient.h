#ifndef NETWORKCLIENT_H
#define NETWORKCLIENT_H

#include "AsyncNetworkRequest.h"
#include "INetworkClient.h"
#include "utils/TestableSingleton.h"

#include <QHash>
#include <QMutex>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QObject>
#include <QPointer>
#include <QQueue>
#include <QRandomGenerator>
#include <QThread>
#include <QWaitCondition>

namespace EasyKiConverter {

/**
 * @brief 单例网络客户端，统一管理重试/退避行为
 */
class NetworkClient : public QObject, public INetworkClient, public TestableSingleton<NetworkClient> {
    Q_OBJECT
    friend class TestableSingleton<NetworkClient>;

public:
    /**
     * @brief 获取单例实例
     */
    using TestableSingleton<NetworkClient>::instance;

    /**
     * @brief 发送 HTTP GET 请求（同步，阻塞直到完成）
     *
     * @note 此方法使用 thread-local QNetworkAccessManager，必须在具有 Qt 事件循环的线程中调用。
     *       请勿在 bare std::thread 中调用。
     */
    /**
     * @brief 发送 HTTP GET 请求（同步，阻塞直到完成）。
     * @param url 请求 URL。
     * @param policy 重试策略。
     * @return 网络请求结果。
     */
    NetworkResult get(const QUrl& url, const RetryPolicy& policy = {}) override;

    /**
     * @brief 发送 HTTP GET 请求（带资源类型，用于配置Profile）
     *
     * @note 此方法使用 thread-local QNetworkAccessManager，必须在具有 Qt 事件循环的线程中调用。
     *       请勿在 bare std::thread 中调用。
     */
    /**
     * @brief 发送带资源类型的同步 HTTP GET 请求。
     * @param url 请求 URL。
     * @param resourceType 资源类型。
     * @param policy 重试策略。
     * @return 网络请求结果。
     */
    NetworkResult get(const QUrl& url, ResourceType resourceType, const RetryPolicy& policy = {}) override;

    /**
     * @brief 发送 HTTP POST 请求（同步，阻塞直到完成）
     *
     * @note 此方法使用 thread-local QNetworkAccessManager，必须在具有 Qt 事件循环的线程中调用。
     *       请勿在 bare std::thread 中调用。
     */
    /**
     * @brief 发送 HTTP POST 请求（同步，阻塞直到完成）。
     * @param url 请求 URL。
     * @param body 请求正文。
     * @param policy 重试策略。
     * @return 网络请求结果。
     */
    NetworkResult post(const QUrl& url, const QByteArray& body, const RetryPolicy& policy = {}) override;

    /**
     * @brief 发送 HTTP POST 请求（带资源类型，用于配置Profile）
     *
     * @note 此方法使用 thread-local QNetworkAccessManager，必须在具有 Qt 事件循环的线程中调用。
     *       请勿在 bare std::thread 中调用。
     */
    /**
     * @brief 发送带资源类型的同步 HTTP POST 请求。
     * @param url 请求 URL。
     * @param body 请求正文。
     * @param resourceType 资源类型。
     * @param policy 重试策略。
     * @return 网络请求结果。
     */
    NetworkResult post(const QUrl& url,
                       const QByteArray& body,
                       ResourceType resourceType,
                       const RetryPolicy& policy = {}) override;

    /**
     * @brief 发送 HTTP GET 请求（异步，非阻塞）
     *
     * 返回的 AsyncNetworkRequest 由调用方管理生命周期。
     * 调用方应连接 finished() 信号并在完成后删除请求。
     * 可通过 AsyncNetworkRequest::cancel() 随时取消请求。
     *
     * @note 此方法使用 thread-local QNetworkAccessManager，必须在具有 Qt 事件循环的线程中调用
     *       （如主线程或 QThread 子类）。请勿在 bare std::thread 中调用。
     *
     * @param url 请求 URL
     * @param resourceType 资源类型
     * @param policy 重试策略
     * @return AsyncNetworkRequest* - 调用方负责生命周期管理
     */
    Q_INVOKABLE AsyncNetworkRequest* getAsync(const QUrl& url,
                                              ResourceType resourceType = ResourceType::Unknown,
                                              const RetryPolicy& policy = {}) override;

    /**
     * @brief 发送 HTTP POST 请求（异步，非阻塞）
     *
     * @note 此方法使用 thread-local QNetworkAccessManager，必须在具有 Qt 事件循环的线程中调用
     *       （如主线程或 QThread 子类）。请勿在 bare std::thread 中调用。
     *
     * @param url 请求 URL
     * @param body 请求体
     * @param resourceType 资源类型
     * @param policy 重试策略
     * @return AsyncNetworkRequest* - 调用方负责生命周期管理
     */
    Q_INVOKABLE AsyncNetworkRequest* postAsync(const QUrl& url,
                                               const QByteArray& body,
                                               ResourceType resourceType = ResourceType::Unknown,
                                               const RetryPolicy& policy = {}) override;

    /**
     * @brief 获取网络运行统计。
     * @return 网络运行统计快照。
     */
    NetworkRuntimeStats runtimeStats() const override;

    /**
     * @brief 将网络运行统计格式化为诊断文本。
     * @return 可读的统计文本。
     */
    QString formatRuntimeStats() const override;

    /**
     * @brief 取消此客户端管理的所有排队和活动异步请求。
     *
     * 用于导出或应用退出时解除等待同步请求的调用方。
     */
    void cancelAllRequests();

    /**
     * @brief 检查数据是否为 Gzip 压缩格式。
     * @param data 待检查的数据。
     * @return 数据带有 Gzip 标识时返回 true。
     */
    static bool isGzipCompressed(const QByteArray& data);

    /**
     * @brief 解压 Gzip 数据。
     * @param data Gzip 压缩数据。
     * @return 解压后的数据；失败时返回空数据。
     */
    static QByteArray decompressGzip(const QByteArray& data);

    /**
     * @brief 销毁单例实例
     */
    using TestableSingleton<NetworkClient>::destroyInstance;

protected:
    NetworkClient();
    ~NetworkClient() override;

private:
    struct PendingAsyncRequest {
        QPointer<AsyncNetworkRequest> request;  ///< 排队中的异步请求对象。
        ResourceType resourceType = ResourceType::Unknown;  ///< 请求资源类型。
        int priority = 0;  ///< 调度优先级。
        quint64 sequence = 0;  ///< 入队序号。
        qint64 enqueuedAtMs = 0;  ///< 入队时间戳（毫秒）。
    };

    struct MutableResourceStats {
        NetworkResourceStats snapshot;  ///< 对外暴露的统计快照。
        qint64 totalQueueDelayMs = 0;  ///< 累计排队耗时（毫秒）。
        qint64 totalLatencyMs = 0;  ///< 累计网络延迟（毫秒）。
    };

    NetworkResult executeRequest(const QUrl& url,
                                 const QByteArray& body,
                                 ResourceType resourceType,
                                 const RetryPolicy& policy);
    AsyncNetworkRequest* enqueueAsyncRequest(const QUrl& url,
                                             const QByteArray& body,
                                             ResourceType resourceType,
                                             const RetryPolicy& policy);
    void scheduleAsyncPump();
    void pumpAsyncQueue();
    void onAsyncRequestFinished(ResourceType resourceType);
    void updateStatsForEnqueuedRequest(ResourceType resourceType);
    void updateStatsForDequeuedRequest(ResourceType resourceType, qint64 queueDelayMs);
    void updateStatsForStartedRequest(ResourceType resourceType);
    void updateStatsForCompletedRequest(ResourceType resourceType, const NetworkResult& result);
    void refreshDynamicStatsLocked(ResourceType resourceType);
    int calculateRetryDelay(int retryCount, const RetryPolicy& policy);
    bool shouldRetry(int statusCode, QNetworkReply::NetworkError error, int retryCount, const RetryPolicy& policy);
    void populateDiagnostic(NetworkDiagnostic& diag, const QUrl& url, ResourceType resourceType);

    QThread m_networkThread;  ///< 执行异步网络请求的专用线程。
    QNetworkAccessManager* m_asyncNetworkManager = nullptr;  ///< 异步请求使用的网络管理器。
    mutable QMutex m_asyncQueueMutex;  ///< 保护异步队列和运行统计的互斥锁。
    QList<PendingAsyncRequest> m_pendingAsyncRequests;  ///< 等待调度的异步请求。
    QList<QPointer<AsyncNetworkRequest>> m_liveAsyncRequests;  ///< 当前仍存活的异步请求。
    QHash<int, int> m_activeAsyncRequestsByType;  ///< 各资源类型的活动请求数。
    QHash<int, MutableResourceStats> m_resourceStats;  ///< 各资源类型的运行统计。
    quint64 m_asyncSequence = 0;  ///< 下一个异步请求的入队序号。
    bool m_asyncPumpScheduled = false;  ///< 是否已安排队列泵任务。
};

}  // namespace EasyKiConverter

#endif  // NETWORKCLIENT_H
