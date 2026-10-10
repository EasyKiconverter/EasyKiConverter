#ifndef ASYNCNETWORKREQUEST_H
#define ASYNCNETWORKREQUEST_H

#include "core/network/INetworkClient.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>

// Forward declaration for friend class
namespace EasyKiConverter::Test {
class MockNetworkClient;
}

#include <QAtomicInt>
#include <QByteArray>
#include <QList>
#include <QMutex>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QObject>
#include <QRandomGenerator>
#include <QTimer>
#include <QUrl>

namespace EasyKiConverter {

/**
 * @brief 异步网络请求，支持取消
 *
 * 支持：
 * - 通过 cancel() 取消请求
 * - 进度跟踪
 * - 自动重试与指数退避
 * - Gzip 解压
 * - 带请求体的 POST 请求
 *
 * 使用示例：
 *   auto* request = client->getAsync(url, policy);
 *   connect(request, &AsyncNetworkRequest::finished, this, [](const NetworkResult& result) {
 *       if (result.wasCancelled) { ... }
 *   });
 *   // 取消请求：request->cancel();
 */
class AsyncNetworkRequest : public QObject {
    Q_OBJECT

public:
    /**
     * @brief 创建已完成的异步请求对象。
     * @param result 已完成的网络结果。
     * @param parent Qt 父对象。
     * @return 已保存结果并将异步发射完成信号的请求对象。
     */
    static AsyncNetworkRequest* createFinished(const NetworkResult& result, QObject* parent = nullptr);

    /**
     * @brief 延迟完成请求 - 用于测试场景
     * @param result 要发送的结果
     * @param delayMs 延迟毫秒数。
     *
     * 此方法用于模拟异步网络请求，避免信号在槽连接建立前发出
     */
    Q_INVOKABLE void completeWithResultDelayed(const NetworkResult& result, int delayMs = 0);

    /**
     * @brief 析构函数，确保清理
     */
    ~AsyncNetworkRequest() override;

    /**
     * @brief 取消请求
     *
     * Can be called from any thread. The request will abort as soon as possible.
     * The finished signal will be emitted with wasCancelled=true.
     */
    Q_INVOKABLE void cancel();

    /**
     * @brief 检查请求是否已取消
     */
    bool isCancelled() const;

    /**
     * @brief 检查请求是否已完成 (successfully or with error)
     */
    bool isFinished() const;

    /**
     * @brief 获取结果
     *
     * Only valid after finished signal is emitted.
     */
    NetworkResult result() const;

    /**
     * @brief 设置超时时间（毫秒）
     *
     * Default is 30000ms (30 seconds).
     * Set to 0 to disable timeout.
     */
    /**
     * @brief 设置超时时间（毫秒）。
     * @param ms 超时时间；传入 0 表示禁用超时。
     */
    void setTimeoutMs(int ms);

    /**
     * @brief 获取当前重试次数
     */
    int currentRetryCount() const;

signals:
    /**
     * @brief 请求完成时发射（成功、错误或取消） (success, error, or cancelled)
     * @param result The result of the request
     */
    void finished(const NetworkResult& result);

    /**
     * @brief 下载过程中发射以报告进度 to report progress
     * @param bytesReceived Bytes received so far
     * @param bytesTotal Total bytes to receive (-1 if unknown)
     */
    void downloadProgress(qint64 bytesReceived, qint64 bytesTotal);

    /**
     * @brief 请求超时时发射
     */
    void timeout();

    /**
     * @brief 请求重试时发射
     * @param retryCount Current retry count (0-based)
     * @param delayMs Delay before retry
     */
    void retrying(int retryCount, int delayMs);

private:
    friend class NetworkClient;
    friend class Test::MockNetworkClient;

    /**
     * @brief 构造函数；私有，使用 NetworkClient::getAsync() 或 postAsync() 创建请求。
     * @param url 请求目标 URL。
     * @param networkManager 所属线程的网络管理器。
     * @param resourceType 请求资源类型。
     * @param policy 请求重试、超时和响应大小策略。
     * @param body POST 请求正文；为空时执行 GET 请求。
     * @param parent Qt 父对象。
     */
    AsyncNetworkRequest(const QUrl& url,
                        QNetworkAccessManager* networkManager,
                        ResourceType resourceType,
                        const RetryPolicy& policy,
                        const QByteArray& body = QByteArray(),
                        QObject* parent = nullptr);

    /**
     * @brief 启动请求
     */
    void start();

    /**
     * @brief 启动单次尝试（重试时会多次调用）。
     * @param attemptNumber 当前尝试序号。
     */
    void startAttempt(int attemptNumber);

    /**
     * @brief 处理尝试完成
     */
    void handleAttemptFinished();

    /**
     * @brief 处理超时
     */
    void handleTimeout();

    /**
     * @brief 处理下载进度，检查响应大小限制。
     * @param bytesReceived 已接收字节数。
     * @param bytesTotal 预期总字节数；未知时为 -1。
     */
    void handleDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);

    /**
     * @brief 处理响应数据。
     * @param reply 已完成的网络响应。
     */
    void processResponse(QNetworkReply* reply);

    /**
     * @brief 调度重试。
     * @param retryCount 当前重试次数。
     */
    void scheduleRetry(int retryCount);

    /**
     * @brief 计算带抖动的重试延迟。
     * @param retryCount 当前重试次数。
     */
    int calculateRetryDelay(int retryCount) const;

    /**
     * @brief 检查是否应该根据状态码重试。
     * @param statusCode HTTP 状态码。
     * @param error Qt 网络错误码。
     * @param retryCount 当前重试次数。
     */
    bool shouldRetryInternal(int statusCode, QNetworkReply::NetworkError error, int retryCount) const;

    /**
     * @brief 清理当前尝试
     */
    void cleanupAttempt();
    bool completeWithResult(const NetworkResult& result);
    NetworkResult cancelledResult(const QString& errorMessage = QStringLiteral("Cancelled")) const;
    NetworkResult timeoutResult() const;
    bool hasActiveReply() const;

    QUrl m_url;  ///< 请求目标 URL。
    QNetworkAccessManager* m_networkManager;  ///< 所属线程的网络管理器，不由请求对象拥有。
    ResourceType m_resourceType;  ///< 请求资源类型。
    RetryPolicy m_policy;  ///< 当前请求的重试和超时策略。
    QByteArray m_postBody;  ///< Request body for POST requests
    bool m_isPostRequest;  ///< 是否为 POST 请求。
    QAtomicInt m_cancelled;  ///< 取消状态原子标志。
    QAtomicInt m_finished;  ///< 完成状态原子标志。
    mutable QMutex m_resultMutex;  ///< 保护最终结果的互斥锁。
    NetworkResult m_result;  ///< 当前请求的最终结果。

    QNetworkReply* m_currentReply;  ///< 当前网络响应对象，不拥有其生命周期。
    QTimer* m_timeoutTimer;  ///< 当前请求的超时计时器。
    int m_currentRetryCount;  ///< 已执行的重试次数。
    int m_timeoutMs;  ///< 当前尝试使用的超时时间（毫秒）。
    bool m_gzipUsed;  ///< 当前响应是否使用了 Gzip 解压。
    QAtomicInt m_currentAttemptTimedOut;  ///< 当前尝试是否已因超时结束。

    QList<QNetworkReply*> m_pendingReplies;  ///< 等待析构清理的网络响应列表。
    mutable QMutex m_repliesMutex;  ///< 保护当前和待清理响应列表的互斥锁。

    static const QList<QNetworkReply*> s_emptyReplies;  ///< 空响应列表的静态占位对象。
};

}  // namespace EasyKiConverter

#endif  // ASYNCNETWORKREQUEST_H
