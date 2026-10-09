#ifndef INETWORKCLIENT_H
#define INETWORKCLIENT_H

#include <QByteArray>
#include <QString>
#include <QUrl>
#include <QVector>
#include <QtGlobal>

namespace EasyKiConverter {

class AsyncNetworkRequest;

/**
 * @brief 网络请求资源类型
 *
 * 用于选择合适的 RequestProfile 和诊断信息
 */
enum class ResourceType {
    ComponentInfo,  // EasyEDA API 组件元数据 JSON
    CadData,  // CAD 数据 JSON (符号、封装)
    ProductSearch,  // LCSC 产品搜索/详情 JSON
    PreviewImage,  // 组件预览图
    Datasheet,  // PDF 数据手册
    Model3DObj,  // 3D 模型 OBJ 文件
    Model3DStep,  // 3D 模型 STEP 文件
    UpdateCheck,  // GitHub 发布元数据
    WorkerRequest,  // 通用工作线程请求 (FetchWorker, NetworkWorker)
    Unknown
};

/**
 * @brief 请求配置结构体，包含特定资源类型的网络策略
 *
 * 每个 profile 定义针对特定资源类型优化的超时、重试、退避和并发设置
 */
struct RequestProfile {
    ResourceType type = ResourceType::Unknown;
    QString name;  // 用于诊断的可读名称

    int connectTimeoutMs = 30000;  // 连接超时
    int readTimeoutMs = 30000;  // 读取超时（大文件）
    int maxRetries = 3;
    double backoffBaseMs = 1000.0;  // 指数退避基础延迟
    double backoffMultiplier = 2.0;  // 每次重试的倍数
    double jitterFactor = 0.2;  // 抖动百分比 (0.0-1.0)

    int weakNetworkMaxRetries = 5;  // 弱网模式下更多重试
    int weakNetworkTimeoutMultiplier = 2;  // 弱网模式下双倍超时

    // 调度优先级（越低越高）
    int priority = 100;

    // 此资源类型的并发限制
    int maxConcurrent = 5;

    // 是否允许取消请求
    bool allowCancellation = true;

    // 此资源类型是否可缓存
    bool cacheable = false;

    // 此资源类型的最大响应字节数（防止恶意/异常响应导致内存耗尽）
    qint64 maxResponseBytes = 50 * 1024 * 1024;  // 默认 50MB
};

/**
 * @brief 预定义的请求配置
 */
struct RequestProfiles {
    static RequestProfile componentInfo() {
        RequestProfile profile;
        profile.type = ResourceType::ComponentInfo;
        profile.name = "ComponentInfo";
        profile.connectTimeoutMs = 15000;
        profile.readTimeoutMs = 30000;
        profile.maxRetries = 3;
        profile.backoffBaseMs = 1000.0;
        profile.backoffMultiplier = 2.0;
        profile.jitterFactor = 0.2;
        profile.weakNetworkMaxRetries = 5;
        profile.weakNetworkTimeoutMultiplier = 2;
        profile.priority = 10;
        profile.maxConcurrent = 10;
        profile.allowCancellation = true;
        profile.cacheable = true;
        profile.maxResponseBytes = 20 * 1024 * 1024;  // JSON 20MB
        return profile;
    }

    static RequestProfile cadData() {
        RequestProfile profile;
        profile.type = ResourceType::CadData;
        profile.name = "CadData";
        profile.connectTimeoutMs = 15000;
        profile.readTimeoutMs = 45000;
        profile.maxRetries = 3;
        profile.backoffBaseMs = 1000.0;
        profile.backoffMultiplier = 2.0;
        profile.jitterFactor = 0.2;
        profile.weakNetworkMaxRetries = 5;
        profile.weakNetworkTimeoutMultiplier = 2;
        profile.priority = 10;
        // Normal-mode validation historically runs at 10-way fan-out. Keeping
        // the scheduler cap at 5 halves BOM verification throughput even when
        // weak-network mode is disabled.
        profile.maxConcurrent = 10;
        profile.allowCancellation = true;
        profile.cacheable = true;
        profile.maxResponseBytes = 20 * 1024 * 1024;  // JSON 20MB
        return profile;
    }

    static RequestProfile previewImage() {
        RequestProfile profile;
        profile.type = ResourceType::PreviewImage;
        profile.name = "PreviewImage";
        profile.connectTimeoutMs = 20000;
        profile.readTimeoutMs = 40000;
        profile.maxRetries = 2;
        profile.backoffBaseMs = 2000.0;
        profile.backoffMultiplier = 2.0;
        profile.jitterFactor = 0.3;
        profile.weakNetworkMaxRetries = 3;
        profile.weakNetworkTimeoutMultiplier = 2;
        profile.priority = 50;
        profile.maxConcurrent = 5;
        profile.allowCancellation = true;
        profile.cacheable = true;
        profile.maxResponseBytes = 10 * 1024 * 1024;  // 图片 10MB
        return profile;
    }

    static RequestProfile productSearch() {
        RequestProfile profile;
        profile.type = ResourceType::ProductSearch;
        profile.name = "ProductSearch";
        profile.connectTimeoutMs = 15000;
        profile.readTimeoutMs = 30000;
        profile.maxRetries = 3;
        profile.backoffBaseMs = 1000.0;
        profile.backoffMultiplier = 2.0;
        profile.jitterFactor = 0.2;
        profile.weakNetworkMaxRetries = 5;
        profile.weakNetworkTimeoutMultiplier = 2;
        profile.priority = 20;
        profile.maxConcurrent = 5;
        profile.allowCancellation = true;
        profile.cacheable = true;
        profile.maxResponseBytes = 20 * 1024 * 1024;  // JSON 20MB
        return profile;
    }

    static RequestProfile datasheet() {
        RequestProfile profile;
        profile.type = ResourceType::Datasheet;
        profile.name = "Datasheet";
        profile.connectTimeoutMs = 20000;
        profile.readTimeoutMs = 120000;
        profile.maxRetries = 2;
        profile.backoffBaseMs = 2000.0;
        profile.backoffMultiplier = 2.0;
        profile.jitterFactor = 0.3;
        profile.weakNetworkMaxRetries = 3;
        profile.weakNetworkTimeoutMultiplier = 2;
        profile.priority = 60;
        profile.maxConcurrent = 3;
        profile.allowCancellation = true;
        profile.cacheable = true;
        profile.maxResponseBytes = 200 * 1024 * 1024;  // PDF/HTML 200MB
        return profile;
    }

    static RequestProfile model3DObj() {
        RequestProfile profile;
        profile.type = ResourceType::Model3DObj;
        profile.name = "Model3DObj";
        profile.connectTimeoutMs = 20000;
        profile.readTimeoutMs = 120000;
        profile.maxRetries = 2;
        profile.backoffBaseMs = 2000.0;
        profile.backoffMultiplier = 2.0;
        profile.jitterFactor = 0.3;
        profile.weakNetworkMaxRetries = 3;
        profile.weakNetworkTimeoutMultiplier = 2;
        profile.priority = 40;
        profile.maxConcurrent = 3;
        profile.allowCancellation = true;
        profile.cacheable = true;
        profile.maxResponseBytes = 200 * 1024 * 1024;  // OBJ 200MB
        return profile;
    }

    static RequestProfile model3DStep() {
        RequestProfile profile;
        profile.type = ResourceType::Model3DStep;
        profile.name = "Model3DStep";
        profile.connectTimeoutMs = 20000;
        profile.readTimeoutMs = 120000;
        profile.maxRetries = 2;
        profile.backoffBaseMs = 2000.0;
        profile.backoffMultiplier = 2.0;
        profile.jitterFactor = 0.3;
        profile.weakNetworkMaxRetries = 3;
        profile.weakNetworkTimeoutMultiplier = 2;
        profile.priority = 30;
        profile.maxConcurrent = 3;
        profile.allowCancellation = true;
        profile.cacheable = true;
        profile.maxResponseBytes = 200 * 1024 * 1024;  // STEP 200MB
        return profile;
    }

    static RequestProfile workerRequest() {
        RequestProfile profile;
        profile.type = ResourceType::WorkerRequest;
        profile.name = "WorkerRequest";
        profile.connectTimeoutMs = 30000;
        profile.readTimeoutMs = 45000;
        profile.maxRetries = 3;
        profile.backoffBaseMs = 3000.0;
        profile.backoffMultiplier = 1.7;
        profile.jitterFactor = 0.2;
        profile.weakNetworkMaxRetries = 5;
        profile.weakNetworkTimeoutMultiplier = 2;
        profile.priority = 20;
        profile.maxConcurrent = 5;
        profile.allowCancellation = true;
        profile.cacheable = false;
        profile.maxResponseBytes = 50 * 1024 * 1024;  // 通用 50MB
        return profile;
    }

    static RequestProfile updateCheck() {
        RequestProfile profile;
        profile.type = ResourceType::UpdateCheck;
        profile.name = "UpdateCheck";
        profile.connectTimeoutMs = 10000;
        profile.readTimeoutMs = 15000;
        profile.maxRetries = 2;
        profile.backoffBaseMs = 1500.0;
        profile.backoffMultiplier = 2.0;
        profile.jitterFactor = 0.2;
        profile.weakNetworkMaxRetries = 3;
        profile.weakNetworkTimeoutMultiplier = 2;
        profile.priority = 80;
        profile.maxConcurrent = 2;
        profile.allowCancellation = true;
        profile.cacheable = false;
        profile.maxResponseBytes = 5 * 1024 * 1024;  // GitHub API 5MB
        return profile;
    }

    /**
     * @brief 根据资源类型选择预定义请求配置。
     * @param type 资源类型。
     * @return 对应的请求配置；未知类型使用组件信息配置。
     */
    static RequestProfile fromType(ResourceType type) {
        /** @brief 分派资源类型到具体请求配置。 */
        // 将资源类型映射到对应的超时、并发和响应大小策略。
        switch (type) {
            case ResourceType::ComponentInfo:
                return componentInfo();
            case ResourceType::CadData:
                return cadData();
            case ResourceType::ProductSearch:
                return productSearch();
            case ResourceType::PreviewImage:
                return previewImage();
            case ResourceType::Datasheet:
                return datasheet();
            case ResourceType::Model3DObj:
                return model3DObj();
            case ResourceType::Model3DStep:
                return model3DStep();
            case ResourceType::UpdateCheck:
                return updateCheck();
            case ResourceType::WorkerRequest:
                return workerRequest();
            default:
                return componentInfo();
        }
    }
};

/**
 * @brief 错误类型分类（用于诊断）
 */
enum class NetworkErrorType {
    None,
    Timeout,
    ConnectionRefused,
    ConnectionReset,
    HostNotFound,
    SSLError,
    RateLimited,  // 429
    ServerError,  // 5xx
    NotFound,  // 404
    Forbidden,  // 403
    Canceled,
    DecompressionFailed,
    Other
};

/**
 * @brief 网络请求的统一诊断信息
 */
struct NetworkDiagnostic {
    QString url;
    QString host;
    ResourceType resourceType = ResourceType::Unknown;
    QString profileName;
    int statusCode = 0;
    NetworkErrorType errorType = NetworkErrorType::None;
    QString errorMessage;
    QString responseContentType;  ///< 错误响应的内容类型。
    QString retryAfter;  ///< 服务端建议的重试等待时间。
    QString rateLimitRemaining;  ///< 服务端返回的剩余请求数提示。
    QString rateLimitReset;  ///< 服务端返回的限流重置时间提示。
    QString responseSummary;  ///< 限长后的文本响应摘要。

    int retryCount = 0;
    qint64 latencyMs = 0;
    qint64 totalElapsedMs = 0;

    bool wasRateLimited = false;
    bool hasRateLimitHint = false;  ///< 是否出现限流相关响应头。
    bool wasCanceled = false;
    bool usedCache = false;

    // Weak network detection
    bool isWeakNetwork = false;
    double weakNetworkScore = 0.0;  // 0.0 = perfect, 1.0 = very poor

    // Timing breakdown
    qint64 dnsLookupMs = 0;
    qint64 connectMs = 0;
    qint64 tlsHandshakeMs = 0;
    qint64 waitMs = 0;  // 等待响应时间
    qint64 transferMs = 0;  // 数据传输时间

    /**
     * @brief 将错误类型转换为字符串（用于日志）
     */
    static QString errorTypeToString(NetworkErrorType type) {
        // 将诊断枚举转换为稳定的日志字段。
        switch (type) {
            case NetworkErrorType::None:
                return "None";
            case NetworkErrorType::Timeout:
                return "Timeout";
            case NetworkErrorType::ConnectionRefused:
                return "ConnectionRefused";
            case NetworkErrorType::ConnectionReset:
                return "ConnectionReset";
            case NetworkErrorType::HostNotFound:
                return "HostNotFound";
            case NetworkErrorType::SSLError:
                return "SSLError";
            case NetworkErrorType::RateLimited:
                return "RateLimited";
            case NetworkErrorType::ServerError:
                return "ServerError";
            case NetworkErrorType::NotFound:
                return "NotFound";
            case NetworkErrorType::Forbidden:
                return "Forbidden";
            case NetworkErrorType::Canceled:
                return "Canceled";
            case NetworkErrorType::DecompressionFailed:
                return "DecompressionFailed";
            default:
                return "Other";
        }
    }
};

/**
 * @brief 网络请求重试策略
 */
struct RetryPolicy {
    int maxRetries = 3;
    int baseTimeoutMs = 30000;
    double jitterFactor = 0.2;
    QVector<int> delays = {1000, 2000, 4000};  // Exponential backoff

    // Status codes that should trigger retry
    QVector<int> retryableStatusCodes = {429, 500, 502, 503, 504};

    /**
     * @brief 根据 RequestProfile 创建 RetryPolicy
     */
    static RetryPolicy fromProfile(const RequestProfile& profile, bool isWeakNetwork = false) {
        RetryPolicy policy;
        const int baseTimeout = qMax(profile.connectTimeoutMs, profile.readTimeoutMs);
        const int timeout = isWeakNetwork ? baseTimeout * profile.weakNetworkTimeoutMultiplier : baseTimeout;
        policy.baseTimeoutMs = timeout;
        policy.maxRetries = isWeakNetwork ? profile.weakNetworkMaxRetries : profile.maxRetries;
        policy.jitterFactor = profile.jitterFactor;

        // Generate backoff delays
        policy.delays.clear();
        double delay = profile.backoffBaseMs;
        for (int i = 0; i < policy.maxRetries; ++i) {
            policy.delays.append(static_cast<int>(delay));
            delay *= profile.backoffMultiplier;
        }

        return policy;
    }
};

/**
 * @brief 网络请求结果
 */
struct NetworkResult {
    QByteArray data;
    int statusCode = 0;
    QString error;
    int retryCount = 0;
    qint64 elapsedMs = 0;
    bool success = false;
    bool wasCancelled = false;  // true if request was cancelled via abort()
    NetworkDiagnostic diagnostic;  // 统一诊断信息
};

struct NetworkResourceStats {
    ResourceType resourceType = ResourceType::Unknown;
    QString profileName;
    int queuedRequests = 0;
    int activeRequests = 0;
    int peakQueuedRequests = 0;
    int peakActiveRequests = 0;
    int maxConcurrent = 0;
    quint64 backpressureEvents = 0;
    quint64 startedRequests = 0;
    quint64 completedRequests = 0;
    quint64 succeededRequests = 0;
    quint64 failedRequests = 0;
    quint64 cancelledRequests = 0;
    quint64 timeoutRequests = 0;
    quint64 rateLimitedRequests = 0;
    quint64 retryAttempts = 0;
    qint64 lastQueueDelayMs = 0;
    qint64 averageQueueDelayMs = 0;
    qint64 maxQueueDelayMs = 0;
    qint64 lastLatencyMs = 0;
    qint64 averageLatencyMs = 0;
    qint64 maxLatencyMs = 0;
};

struct NetworkRuntimeStats {
    QVector<NetworkResourceStats> resources;
    quint64 totalStartedRequests = 0;
    quint64 totalCompletedRequests = 0;
    quint64 totalSucceededRequests = 0;
    quint64 totalFailedRequests = 0;
    quint64 totalCancelledRequests = 0;
    quint64 totalTimeoutRequests = 0;
    quint64 totalRateLimitedRequests = 0;
    quint64 totalRetryAttempts = 0;
    quint64 totalBackpressureEvents = 0;
    int totalQueuedRequests = 0;
    int totalActiveRequests = 0;
};

/**
 * @brief 网络客户端接口
 *
 * 提供统一的网络请求接口，所有网络组件共享一致的超时/重试/退避行为
 */
class INetworkClient {
public:
    virtual ~INetworkClient() = default;

    /**
     * @brief 发送 HTTP GET 请求
     * @param url 请求 URL
     * @param policy 重试策略（默认：3 次重试，30s 超时）
     * @return NetworkResult 返回响应数据或错误
     */
    virtual NetworkResult get(const QUrl& url, const RetryPolicy& policy = {}) = 0;

    /**
     * @brief 发送 HTTP GET 请求（带资源类型，用于配置Profile）
     * @param url 请求 URL
     * @param resourceType 资源类型
     * @param policy 重试策略
     * @return NetworkResult 返回响应数据或错误
     */
    virtual NetworkResult get(const QUrl& url, ResourceType resourceType, const RetryPolicy& policy = {}) = 0;

    /**
     * @brief 发送 HTTP POST 请求
     * @param url 请求 URL
     * @param body 请求体
     * @param policy 重试策略
     * @return NetworkResult 返回响应数据或错误
     */
    virtual NetworkResult post(const QUrl& url, const QByteArray& body, const RetryPolicy& policy = {}) = 0;

    /**
     * @brief 发送 HTTP POST 请求（带资源类型，用于配置Profile）
     * @param url 请求 URL
     * @param body 请求体
     * @param resourceType 资源类型
     * @param policy 重试策略
     * @return NetworkResult 返回响应数据或错误
     */
    virtual NetworkResult post(const QUrl& url,
                               const QByteArray& body,
                               ResourceType resourceType,
                               const RetryPolicy& policy = {}) = 0;

    /**
     * @brief 异步发送 HTTP GET 请求
     */
    virtual AsyncNetworkRequest* getAsync(const QUrl& url,
                                          ResourceType resourceType,
                                          const RetryPolicy& policy = {}) = 0;

    /**
     * @brief 异步发送 HTTP POST 请求
     */
    virtual AsyncNetworkRequest* postAsync(const QUrl& url,
                                           const QByteArray& body,
                                           ResourceType resourceType,
                                           const RetryPolicy& policy = {}) = 0;

    virtual NetworkRuntimeStats runtimeStats() const = 0;
    virtual QString formatRuntimeStats() const = 0;
};

}  // namespace EasyKiConverter

#endif  // INETWORKCLIENT_H
