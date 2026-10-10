#include "AsyncNetworkRequest.h"

#include "core/utils/GzipUtils.h"
#include "core/utils/UrlUtils.h"

#include <QDebug>
#include <QElapsedTimer>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QThread>
#include <QTimer>

namespace EasyKiConverter {

// 异步请求共享的空响应列表。
const QList<QNetworkReply*> AsyncNetworkRequest::s_emptyReplies;

namespace {

constexpr int kResponseSummaryLimit = 512;  ///< 错误响应摘要允许保留的最大字符数。

/**
 * @brief 读取并限制响应头长度，避免把控制字符或异常长值带入诊断日志。
 * @param reply 网络响应对象。
 * @param name 要读取的响应头名称。
 * @return 清理控制字符并限制长度后的响应头值。
 */
QString boundedHeaderValue(QNetworkReply* reply, const QByteArray& name) {
    if (!reply) {
        return QString();
    }
    QString value = QString::fromLatin1(reply->rawHeader(name)).trimmed();  ///< 去除首尾空白后的响应头文本。
    value.replace(QRegularExpression(QStringLiteral("[\\r\\n]")), QString());
    return value.left(256);
}

/**
 * @brief 从错误响应中提取访问策略和限流状态所需的非敏感摘要。
 * @param reply 发生错误的网络响应对象。
 * @param result 要补充诊断信息的网络结果。
 */
void captureErrorResponseDiagnostics(QNetworkReply* reply, NetworkResult& result) {
    if (!reply) {
        return;
    }

    result.diagnostic.responseContentType = boundedHeaderValue(reply, "Content-Type");
    result.diagnostic.retryAfter = boundedHeaderValue(reply, "Retry-After");
    result.diagnostic.rateLimitRemaining = boundedHeaderValue(reply, "X-RateLimit-Remaining");
    result.diagnostic.rateLimitReset = boundedHeaderValue(reply, "X-RateLimit-Reset");
    result.diagnostic.hasRateLimitHint = !result.diagnostic.retryAfter.isEmpty() ||
                                         !result.diagnostic.rateLimitRemaining.isEmpty() ||
                                         !result.diagnostic.rateLimitReset.isEmpty();

    const QString contentType = result.diagnostic.responseContentType.toLower();  ///< 响应的标准化内容类型。
    const bool isTextResponse = contentType.startsWith(QStringLiteral("text/")) ||  ///< 是否适合提取文本摘要。
                                contentType.contains(QStringLiteral("json")) ||
                                contentType.contains(QStringLiteral("xml"));
    if (!isTextResponse) {
        return;
    }

    const QByteArray body = reply->read(kResponseSummaryLimit);  ///< 错误响应正文的有限长度片段。
    QString summary = QString::fromUtf8(body).simplified();  ///< 清理空白后的错误响应摘要。
    if (summary.size() > kResponseSummaryLimit) {
        summary = summary.left(kResponseSummaryLimit) + QStringLiteral("...");
    }
    result.diagnostic.responseSummary = summary;
}

}  // namespace

/**
 * @brief 创建已经完成的请求对象，供同步结果和测试场景复用。
 * @param result 要保存并发送的网络结果。
 * @param parent Qt 父对象。
 * @return 已保存结果并异步发射完成信号的请求对象。
 */
AsyncNetworkRequest* AsyncNetworkRequest::createFinished(const NetworkResult& result, QObject* parent) {
    auto* request =
        new AsyncNetworkRequest(QUrl(), nullptr, result.diagnostic.resourceType, RetryPolicy(), QByteArray(), parent);
    if (result.wasCancelled) {
        request->m_cancelled.storeRelease(1);
    }
    // 设置已完成状态（同步），供调用方立即检查 isFinished() / result()
    {
        QMutexLocker locker(&request->m_resultMutex);
        request->m_result = result;
    }
    request->m_finished.storeRelease(1);

    // 延迟发射 finished 信号，确保调用方有机会先 connect
    // 使用 QTimer::singleShot(0, ...) 投递到当前线程的事件循环
    // 调用方必须在有事件循环的线程中使用（主线程、QThread 子类）
    QTimer::singleShot(0, request, [request, result]() { emit request->finished(result); });
    return request;
}

/**
 * @brief 初始化请求状态、网络策略和诊断上下文。
 * @param url 请求目标 URL。
 * @param networkManager 所属线程的网络管理器。
 * @param resourceType 请求资源类型。
 * @param policy 请求重试、超时和响应大小策略。
 * @param body POST 请求正文；为空时执行 GET 请求。
 * @param parent Qt 父对象。
 */
AsyncNetworkRequest::AsyncNetworkRequest(const QUrl& url,
                                         QNetworkAccessManager* networkManager,
                                         ResourceType resourceType,
                                         const RetryPolicy& policy,
                                         const QByteArray& body,
                                         QObject* parent)
    : QObject(parent)
    , m_url(url)
    , m_networkManager(networkManager)
    , m_resourceType(resourceType)
    , m_policy(policy)
    , m_postBody(body)
    , m_isPostRequest(!body.isEmpty())
    , m_cancelled(0)
    , m_finished(0)
    , m_currentReply(nullptr)
    , m_timeoutTimer(nullptr)
    , m_currentRetryCount(0)
    , m_timeoutMs(policy.baseTimeoutMs)
    , m_gzipUsed(false)
    , m_currentAttemptTimedOut(0) {
    m_result.diagnostic.url = m_url.toString();
    m_result.diagnostic.host = m_url.host();
    m_result.diagnostic.resourceType = m_resourceType;
    m_result.diagnostic.profileName = RequestProfiles::fromType(m_resourceType).name;
}

AsyncNetworkRequest::~AsyncNetworkRequest() {
    // Cancel any pending operations
    cancel();

    // Clean up any pending replies
    QMutexLocker locker(&m_repliesMutex);
    for (QNetworkReply* reply : m_pendingReplies) {
        if (reply && !reply->isFinished()) {
            reply->abort();
        }
        if (reply) {
            reply->deleteLater();
        }
    }
    m_pendingReplies.clear();

    // Clean up current reply if still active
    if (m_currentReply && !m_currentReply->isFinished()) {
        m_currentReply->abort();
    }
    if (m_currentReply) {
        m_currentReply->deleteLater();
        m_currentReply = nullptr;
    }
}

/**
 * @brief 在所属线程中中止当前请求并发布取消结果。
 */
void AsyncNetworkRequest::cancel() {
    if (thread() != QThread::currentThread()) {
        QMetaObject::invokeMethod(this, &AsyncNetworkRequest::cancel, Qt::QueuedConnection);
        return;
    }

    if (!m_cancelled.testAndSetRelaxed(0, 1)) {
        return;
    }

    qDebug() << "AsyncNetworkRequest: Cancelling request to" << m_url;

    if (m_timeoutTimer) {
        m_timeoutTimer->stop();
    }

    {
        QMutexLocker locker(&m_repliesMutex);
        if (m_currentReply && !m_currentReply->isFinished()) {
            m_currentReply->abort();
        }
    }

    completeWithResult(cancelledResult());
}

/**
 * @brief 返回请求是否已经进入取消状态。
 * @return 请求已取消时返回 true。
 */
bool AsyncNetworkRequest::isCancelled() const {
    return m_cancelled.loadRelaxed() != 0;
}

/**
 * @brief 返回请求是否已经产生最终结果。
 * @return 请求已完成时返回 true。
 */
bool AsyncNetworkRequest::isFinished() const {
    return m_finished.loadRelaxed() != 0;
}

/**
 * @brief 在线程安全地读取请求结果。
 * @return 当前保存的网络结果。
 */
NetworkResult AsyncNetworkRequest::result() const {
    QMutexLocker locker(&m_resultMutex);
    return m_result;
}

/**
 * @brief 设置后续请求尝试使用的超时时间。
 * @param ms 超时时间；传入 0 表示禁用超时。
 */
void AsyncNetworkRequest::setTimeoutMs(int ms) {
    m_timeoutMs = ms;
}

/**
 * @brief 返回已经执行的重试次数。
 * @return 已执行的重试次数。
 */
int AsyncNetworkRequest::currentRetryCount() const {
    return m_currentRetryCount;
}

/**
 * @brief 从第一次尝试开始执行请求生命周期。
 */
void AsyncNetworkRequest::start() {
    if (isCancelled() || isFinished()) {
        return;
    }

    qDebug() << "AsyncNetworkRequest: Starting request to" << m_url;

    // Call startAttempt directly since:
    // 1. We're using thread-local QNetworkAccessManager (same thread)
    // 2. Signals use Qt::DirectConnection (work in same thread)
    // 3. No event loop needed for QTimer in same thread
    startAttempt(0);
}

/**
 * @brief 启动一次网络请求，并安装超时、进度和完成处理器。
 * @param attemptNumber 当前尝试序号。
 */
void AsyncNetworkRequest::startAttempt(int attemptNumber) {
    if (isCancelled() || isFinished()) {
        return;
    }

    m_currentAttemptTimedOut.storeRelease(0);

    if (!m_networkManager) {
        NetworkResult result = m_result;  ///< 网络管理器缺失时要发布的失败结果。
        result.error = "Network manager is null";
        result.success = false;
        result.wasCancelled = false;
        result.diagnostic.errorType = NetworkErrorType::Other;
        result.diagnostic.errorMessage = result.error;
        completeWithResult(result);
        return;
    }

    QNetworkRequest request(m_url);
    request.setHeader(QNetworkRequest::UserAgentHeader, "EasyKiConverter/1.0");
    request.setRawHeader("X-Requested-With", "XMLHttpRequest");
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
    // 禁止不安全的重定向（HTTPS -> HTTP），防止降级攻击
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    // Note: We handle gzip manually in processResponse() since Qt doesn't auto-decompress

    // Create the reply - use POST if body is provided
    QNetworkReply* reply = nullptr;  ///< 当前尝试创建的网络响应对象。
    if (m_isPostRequest && !m_postBody.isEmpty()) {
        // Auto-detect Content-Type based on body format
        // If body contains = and &, it's likely URL-encoded form data
        if (m_postBody.contains('=') && m_postBody.contains('&')) {
            request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
        } else {
            request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        }
        reply = m_networkManager->post(request, m_postBody);
    } else {
        reply = m_networkManager->get(request);
    }

    {
        QMutexLocker locker(&m_repliesMutex);
        m_currentReply = reply;
        m_pendingReplies.append(reply);
    }

    // Set up timeout (no parent - created/destroyed in same thread pool thread)
    m_timeoutTimer = new QTimer();
    m_timeoutTimer->setSingleShot(true);
    connect(m_timeoutTimer, &QTimer::timeout, this, &AsyncNetworkRequest::handleTimeout, Qt::DirectConnection);
    if (m_timeoutMs > 0) {
        m_timeoutTimer->start(m_timeoutMs);
    }

    // Connect signals - use DirectConnection since we want immediate handling
    // The slot will run in this object's thread (main thread where AsyncNetworkRequest lives)
    connect(reply, &QNetworkReply::finished, this, &AsyncNetworkRequest::handleAttemptFinished, Qt::DirectConnection);
    connect(reply,
            &QNetworkReply::downloadProgress,
            this,
            &AsyncNetworkRequest::handleDownloadProgress,
            Qt::DirectConnection);

    qDebug() << "AsyncNetworkRequest: Attempt" << attemptNumber << "started for" << m_url;
}

/**
 * @brief 过滤过期回复并处理当前网络尝试的最终信号。
 */
void AsyncNetworkRequest::handleAttemptFinished() {
    if (isCancelled() || isFinished()) {
        return;
    }

    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());  ///< 发出完成信号的网络响应对象。
    if (!reply) {
        return;
    }

    // Stop timeout
    if (m_timeoutTimer) {
        m_timeoutTimer->stop();
    }

    // Check if this is our current reply
    {
        QMutexLocker locker(&m_repliesMutex);
        if (reply != m_currentReply) {
            // Stale reply, ignore
            return;
        }
    }

    // Process the response
    processResponse(reply);
}

/**
 * @brief 标记当前尝试超时并中止底层回复。
 */
void AsyncNetworkRequest::handleTimeout() {
    if (isCancelled() || isFinished()) {
        return;
    }

    qDebug() << "AsyncNetworkRequest: Timeout for" << m_url;
    emit timeout();
    m_currentAttemptTimedOut.storeRelease(1);

    {
        QMutexLocker locker(&m_repliesMutex);
        if (m_currentReply) {
            m_currentReply->abort();
        }
    }
}

/**
 * @brief 转发下载进度并在响应过大时提前终止请求。
 * @param bytesReceived 已接收的字节数。
 * @param bytesTotal 预期总字节数；未知时为负数。
 */
void AsyncNetworkRequest::handleDownloadProgress(qint64 bytesReceived, qint64 bytesTotal) {
    if (isCancelled() || isFinished()) {
        return;
    }

    // 转发进度信号
    emit downloadProgress(bytesReceived, bytesTotal);

    // 检查响应大小限制
    const RequestProfile profile = RequestProfiles::fromType(m_resourceType);  ///< 当前资源类型的请求策略。
    const qint64 maxBytes = profile.maxResponseBytes;  ///< 当前响应允许的最大字节数。

    if (maxBytes <= 0) {
        return;  // 无限制
    }

    // 优先使用 Content-Length（bytesTotal），其次用已接收字节数
    const qint64 checkSize = (bytesTotal > 0) ? bytesTotal : bytesReceived;

    if (checkSize > maxBytes) {
        qWarning() << "AsyncNetworkRequest: Response too large (" << checkSize << "bytes, limit" << maxBytes
                   << "), aborting:" << m_url;
        QMutexLocker locker(&m_repliesMutex);
        if (m_currentReply) {
            m_currentReply->abort();
        }
    }
}

/**
 * @brief 将网络回复转换为统一结果并提取错误诊断。
 * @param reply 已完成的网络响应。
 */
void AsyncNetworkRequest::processResponse(QNetworkReply* reply) {
    NetworkResult result;  ///< 从当前请求上下文派生并最终发布的网络结果。
    {
        QMutexLocker locker(&m_resultMutex);
        result = m_result;
    }

    result.statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    result.diagnostic.statusCode = result.statusCode;
    result.diagnostic.retryCount = m_currentRetryCount;

    // Check for error
    if (reply->error() != QNetworkReply::NoError) {
        QString errorString = reply->errorString();  ///< Qt 提供的底层网络错误文本。

        // Check if cancelled
        if (isCancelled()) {
            completeWithResult(cancelledResult());
            return;
        }

        if (m_currentAttemptTimedOut.loadAcquire() != 0) {
            if (shouldRetryInternal(result.statusCode, QNetworkReply::TimeoutError, m_currentRetryCount)) {
                scheduleRetry(m_currentRetryCount);
            } else {
                completeWithResult(timeoutResult());
            }
            return;
        }

        // Check if should retry
        if (shouldRetryInternal(result.statusCode, reply->error(), m_currentRetryCount)) {
            scheduleRetry(m_currentRetryCount);
            return;
        }

        // Final error
        captureErrorResponseDiagnostics(reply, result);
        result.error =
            result.statusCode > 0 ? QStringLiteral("HTTP %1: %2").arg(result.statusCode).arg(errorString) : errorString;
        result.success = false;
        result.wasCancelled = false;
        result.diagnostic.errorMessage = result.error;

        // 依据底层网络错误和 HTTP 状态码生成统一分类。
        switch (reply->error()) {
            case QNetworkReply::TimeoutError:
                result.diagnostic.errorType = NetworkErrorType::Timeout;
                break;
            case QNetworkReply::ConnectionRefusedError:
                result.diagnostic.errorType = NetworkErrorType::ConnectionRefused;
                break;
            case QNetworkReply::HostNotFoundError:
                result.diagnostic.errorType = NetworkErrorType::HostNotFound;
                break;
            case QNetworkReply::SslHandshakeFailedError:
                result.diagnostic.errorType = NetworkErrorType::SSLError;
                break;
            case QNetworkReply::OperationCanceledError:
                result.diagnostic.errorType = NetworkErrorType::Canceled;
                result.diagnostic.wasCanceled = true;
                break;
            default:
                if (result.statusCode == 429) {
                    result.diagnostic.errorType = NetworkErrorType::RateLimited;
                    result.diagnostic.wasRateLimited = true;
                } else if (result.statusCode >= 500) {
                    result.diagnostic.errorType = NetworkErrorType::ServerError;
                } else if (result.statusCode == 404) {
                    result.diagnostic.errorType = NetworkErrorType::NotFound;
                } else if (result.statusCode == 403) {
                    result.diagnostic.errorType = NetworkErrorType::Forbidden;
                } else {
                    result.diagnostic.errorType = NetworkErrorType::Other;
                }
                break;
        }
        completeWithResult(result);
        return;
    }

    // Success - read data
    QByteArray data = reply->readAll();  ///< 已读取且可能需要解压的响应正文。

    // 安全兜底：检查实际读取的数据大小（Content-Length 可能缺失或不准确）
    {
        const RequestProfile profile = RequestProfiles::fromType(m_resourceType);  ///< 当前资源的响应大小策略。
        if (profile.maxResponseBytes > 0 && data.size() > profile.maxResponseBytes) {
            result.error = QStringLiteral("Response too large: %1 bytes (limit %2)")
                               .arg(data.size())
                               .arg(profile.maxResponseBytes);
            result.success = false;
            result.wasCancelled = false;
            result.diagnostic.errorType = NetworkErrorType::Other;
            result.diagnostic.errorMessage = result.error;
            completeWithResult(result);
            return;
        }
    }

    // 校验重定向后的最终 URL 是否仍在白名单内
    {
        const QUrl finalUrl = reply->url();  ///< 重定向后服务器实际返回的最终 URL。
        if (finalUrl != m_url) {
            QString errorMsg;  ///< URL 白名单校验失败时的具体原因。
            if (!UrlUtils::isAllowedUrl(finalUrl, m_resourceType, &errorMsg)) {
                result.error = QStringLiteral("Redirect to disallowed URL: %1").arg(errorMsg);
                result.success = false;
                result.wasCancelled = false;
                result.diagnostic.errorType = NetworkErrorType::Other;
                result.diagnostic.errorMessage = result.error;
                completeWithResult(result);
                return;
            }
        }
    }

    // Check for gzip encoding and decompress if needed
    QVariant encodingVariant = reply->rawHeader("Content-Encoding");  ///< 响应声明的编码头。
    QString encoding = encodingVariant.toString().toLower();  ///< 标准化后的内容编码名称。
    m_gzipUsed = encoding.contains("gzip");

    if (m_gzipUsed || GzipUtils::isGzipped(data)) {
        GzipUtils::DecompressResult decompResult = GzipUtils::decompress(data);  ///< Gzip 解压结果。
        if (decompResult.success) {
            data = decompResult.data;

            // 解压后重新检查大小限制（压缩可能放大数据）
            const RequestProfile profile = RequestProfiles::fromType(m_resourceType);  ///< 解压后的响应大小策略。
            if (profile.maxResponseBytes > 0 && data.size() > profile.maxResponseBytes) {
                result.error = QStringLiteral("Decompressed response too large: %1 bytes (limit %2)")
                                   .arg(data.size())
                                   .arg(profile.maxResponseBytes);
                result.success = false;
                result.wasCancelled = false;
                result.diagnostic.errorType = NetworkErrorType::Other;
                result.diagnostic.errorMessage = result.error;
                completeWithResult(result);
                return;
            }
        } else {
            result.error = "Gzip decompression failed";
            result.success = false;
            result.wasCancelled = false;
            result.diagnostic.errorType = NetworkErrorType::DecompressionFailed;
            result.diagnostic.errorMessage = result.error;
            completeWithResult(result);
            return;
        }
    }

    result.data = data;
    result.success = true;
    result.retryCount = m_currentRetryCount;
    result.wasCancelled = false;
    result.diagnostic.errorType = NetworkErrorType::None;
    result.diagnostic.errorMessage.clear();

    completeWithResult(result);
}

/**
 * @brief 按退避策略安排下一次请求尝试。
 * @param retryCount 当前重试次数。
 */
void AsyncNetworkRequest::scheduleRetry(int retryCount) {
    if (isCancelled() || isFinished()) {
        return;
    }

    int delay = calculateRetryDelay(retryCount);  ///< 下一次尝试前等待的毫秒数。
    m_currentRetryCount = retryCount + 1;

    qDebug() << "AsyncNetworkRequest: Scheduling retry" << m_currentRetryCount << "for" << m_url << "after" << delay
             << "ms";

    emit retrying(m_currentRetryCount, delay);

    // Use a timer to delay retry - this allows cancellation to be checked
    QTimer::singleShot(delay, this, [this]() {
        if (isCancelled() || isFinished()) {
            return;
        }

        // Clean up old reply
        cleanupAttempt();

        // Start new attempt in main thread where QNetworkAccessManager lives
        QMetaObject::invokeMethod(this, [this]() { startAttempt(m_currentRetryCount); }, Qt::QueuedConnection);
    });
}

/**
 * @brief 计算带抖动的指数退避等待时间。
 * @param retryCount 当前重试次数。
 * @return 下一次重试前应等待的毫秒数。
 */
int AsyncNetworkRequest::calculateRetryDelay(int retryCount) const {
    if (retryCount >= static_cast<int>(m_policy.delays.size())) {
        return m_policy.delays.back();
    }
    int baseDelay = m_policy.delays[retryCount];  ///< 当前重试序号对应的基础等待时间。

    // Add jitter
    int jitterRange = static_cast<int>(baseDelay * m_policy.jitterFactor);  ///< 随机抖动的半范围。
    int jitter = QRandomGenerator::global()->bounded(-jitterRange, jitterRange + 1);  ///< 本次实际抖动值。
    return baseDelay + jitter;
}

/**
 * @brief 判断当前错误是否允许再次请求。
 * @param statusCode HTTP 状态码。
 * @param error Qt 网络错误码。
 * @param retryCount 当前重试次数。
 * @return 当前错误满足重试策略时返回 true。
 */
bool AsyncNetworkRequest::shouldRetryInternal(int statusCode, QNetworkReply::NetworkError error, int retryCount) const {
    if (retryCount >= m_policy.maxRetries) {
        return false;
    }

    if (m_policy.retryableStatusCodes.contains(statusCode)) {
        return true;
    }

    // 网络层瞬时错误可重试，明确的业务拒绝不重试。
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

/**
 * @brief 释放当前回复和计时器，为下一次尝试清理状态。
 */
void AsyncNetworkRequest::cleanupAttempt() {
    QMutexLocker locker(&m_repliesMutex);

    if (m_currentReply) {
        m_pendingReplies.removeAll(m_currentReply);
        if (!m_currentReply->isFinished()) {
            m_currentReply->abort();
        }
        m_currentReply->deleteLater();
        m_currentReply = nullptr;
    }

    if (m_timeoutTimer) {
        m_timeoutTimer->stop();
        m_timeoutTimer->deleteLater();
        m_timeoutTimer = nullptr;
    }

    m_currentAttemptTimedOut.storeRelease(0);
}

/**
 * @brief 只允许第一个完成结果写入状态并发出信号。
 * @param result 要保存并发送给调用方的网络结果。
 * @return 本次调用成功写入并发布结果时返回 true。
 */
bool AsyncNetworkRequest::completeWithResult(const NetworkResult& result) {
    if (!m_finished.testAndSetOrdered(0, 1)) {
        return false;
    }

    {
        QMutexLocker locker(&m_resultMutex);
        m_result = result;
    }

    emit finished(result);
    return true;
}

/**
 * @brief 构造保留原始诊断上下文的取消结果。
 * @param errorMessage 取消结果要携带的错误文本。
 * @return 标记为取消并保留原诊断上下文的结果。
 */
NetworkResult AsyncNetworkRequest::cancelledResult(const QString& errorMessage) const {
    NetworkResult result;  ///< 基于当前请求上下文构造的取消结果。
    {
        QMutexLocker locker(&m_resultMutex);
        result = m_result;
    }

    result.success = false;
    result.wasCancelled = true;
    result.error = errorMessage;
    result.diagnostic.wasCanceled = true;
    result.diagnostic.errorType = NetworkErrorType::Canceled;
    result.diagnostic.errorMessage = result.error;
    result.diagnostic.retryCount = m_currentRetryCount;
    return result;
}

/**
 * @brief 构造保留原始诊断上下文的超时结果。
 * @return 标记为超时并保留原诊断上下文的结果。
 */
NetworkResult AsyncNetworkRequest::timeoutResult() const {
    NetworkResult result;  ///< 基于当前请求上下文构造的超时结果。
    {
        QMutexLocker locker(&m_resultMutex);
        result = m_result;
    }

    result.success = false;
    result.wasCancelled = false;
    result.error = "Timeout";
    result.diagnostic.errorType = NetworkErrorType::Timeout;
    result.diagnostic.errorMessage = result.error;
    result.diagnostic.retryCount = m_currentRetryCount;
    return result;
}

/**
 * @brief 检查当前是否仍有未完成的网络回复。
 * @return 存在未完成回复时返回 true。
 */
bool AsyncNetworkRequest::hasActiveReply() const {
    QMutexLocker locker(&m_repliesMutex);
    return m_currentReply && !m_currentReply->isFinished();
}

/**
 * @brief 延迟发布结果，保证异步调用方有机会连接完成信号。
 * @param result 要保存并发布的网络结果。
 * @param delayMs 延迟毫秒数；不大于 0 时使用下一轮事件循环。
 */
void AsyncNetworkRequest::completeWithResultDelayed(const NetworkResult& result, int delayMs) {
    if (delayMs <= 0) {
        QTimer::singleShot(0, this, [this, result]() { completeWithResult(result); });
    } else {
        QTimer::singleShot(delayMs, this, [this, result]() { completeWithResult(result); });
    }
}

}  // namespace EasyKiConverter
