#include "NetworkWorker.h"

#include "core/network/AsyncNetworkRequest.h"
#include "core/network/BlockingRequestContext.h"
#include "core/network/NetworkClient.h"
#include "core/utils/GzipUtils.h"
#include "services/ConfigService.h"

#include <QAtomicInt>
#include <QCoreApplication>
#include <QDebug>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutexLocker>
#include <QWaitCondition>

namespace EasyKiConverter {

/**
 * @brief 创建指定网络任务类型的后台 Worker。
 * @param componentId 元器件编号
 * @param taskType 网络任务类型
 * @param uuid 三维模型 UUID，可为空
 * @param parent Qt 对象父级，可为空
 */
NetworkWorker::NetworkWorker(const QString& componentId, TaskType taskType, const QString& uuid, QObject* parent)
    // 保存任务参数并等待 run() 在工作线程中执行。
    : m_componentId(componentId), m_taskType(taskType), m_uuid(uuid), m_currentRequest(nullptr) {
    Q_UNUSED(parent);
}

/**
 * @brief 记录 Worker 销毁并释放其请求关联。
 */
NetworkWorker::~NetworkWorker() {
    qDebug() << "NetworkWorker destroyed for:" << m_componentId;
}

/**
 * @brief 根据任务类型执行对应的网络获取流程。
 */
void NetworkWorker::run() {
    qDebug() << "NetworkWorker started for:" << m_componentId << "- TaskType:" << static_cast<int>(m_taskType);

    // 保存当前任务的执行结果，用于失败日志判断。
    bool success = false;

    // 根据任务类型选择对应的请求和结果处理流程。
    switch (m_taskType) {
        case TaskType::FetchComponentInfo:
            success = fetchComponentInfo();
            break;
        case TaskType::FetchCadData:
            success = fetchCadData();
            break;
        case TaskType::Fetch3DModelObj:
            success = fetch3DModelObj();
            break;
        case TaskType::Fetch3DModelMtl:
            success = fetch3DModelMtl();
            break;
        default:
            qWarning() << "Unknown task type:" << static_cast<int>(m_taskType);
            emit fetchError(m_componentId, "Unknown task type");
            return;
    }

    if (!success) {
        qWarning() << "Network request failed for:" << m_componentId;
    }
}

/**
 * @brief 获取元器件基础信息。
 */
bool NetworkWorker::fetchComponentInfo() {
    // 保存网络响应的 JSON 字节内容。
    QByteArray responseData;
    // 保存请求或解析失败的可读错误信息。
    QString errorMsg;

    // 构造 EasyEDA 元器件信息接口地址。
    const QUrl url(QString("https://easyeda.com/api/products/%1/components?version=6.5.51").arg(m_componentId));
    if (!executeRequest(url, ResourceType::ComponentInfo, DEFAULT_TIMEOUT_MS, MAX_RETRIES, responseData, errorMsg)) {
        emit fetchError(m_componentId, errorMsg);
        return false;
    }

    // 保存解析后的元器件信息 JSON 文档。
    QJsonDocument doc = QJsonDocument::fromJson(responseData);
    if (doc.isNull() || !doc.isObject()) {
        // 保存 JSON 格式无效时的错误信息。
        QString errorMessage = "Invalid JSON response";
        qWarning() << "JSON parse error in fetchComponentInfo:" << errorMessage;
        emit fetchError(m_componentId, errorMessage);
        return false;
    }

    emit componentInfoFetched(m_componentId, doc.object());
    qDebug() << "Component info fetched successfully for:" << m_componentId;
    return true;
}

/**
 * @brief 获取元器件 CAD 数据。
 */
bool NetworkWorker::fetchCadData() {
    // 保存网络响应的 JSON 字节内容。
    QByteArray responseData;
    // 保存请求或解析失败的可读错误信息。
    QString errorMsg;

    // 构造 EasyEDA CAD 数据接口地址。
    const QUrl url(QString("https://easyeda.com/api/products/%1/components?version=6.5.51").arg(m_componentId));
    if (!executeRequest(url, ResourceType::CadData, DEFAULT_TIMEOUT_MS, MAX_RETRIES, responseData, errorMsg)) {
        emit fetchError(m_componentId, errorMsg);
        return false;
    }

    // Note: AsyncNetworkRequest already handles gzip decompression internally.
    // responseData is already decompressed if it was gzip-compressed.
    // 保存解析后的 CAD JSON 文档。
    QJsonDocument doc = QJsonDocument::fromJson(responseData);
    if (doc.isNull() || !doc.isObject()) {
        // 保存 JSON 格式无效时的错误信息。
        QString errorMessage = "Invalid JSON response";
        qWarning() << "JSON parse error in fetchCadData:" << errorMessage;
        emit fetchError(m_componentId, errorMessage);
        return false;
    }

    emit cadDataFetched(m_componentId, doc.object());
    qDebug() << "CAD data fetched successfully for:" << m_componentId;
    return true;
}

/**
 * @brief 获取元器件 OBJ 三维模型。
 */
bool NetworkWorker::fetch3DModelObj() {
    // 保存网络响应的模型文件内容。
    QByteArray responseData;
    // 保存请求失败的可读错误信息。
    QString errorMsg;

    // 构造 OBJ 模型下载地址。
    const QUrl url(QString("https://modules.easyeda.com/3dmodel/%1").arg(m_uuid));
    if (!executeRequest(url, ResourceType::Model3DObj, MODEL_TIMEOUT_MS, MAX_RETRIES, responseData, errorMsg)) {
        emit fetchError(m_componentId, errorMsg);
        return false;
    }

    emit model3DFetched(m_componentId, m_uuid, responseData);
    qDebug() << "3D model OBJ data fetched successfully for:" << m_componentId;
    return true;
}

/**
 * @brief 获取元器件 MTL 三维材质文件。
 */
bool NetworkWorker::fetch3DModelMtl() {
    // 保存网络响应的材质文件内容。
    QByteArray responseData;
    // 保存请求失败的可读错误信息。
    QString errorMsg;

    // 构造 MTL 材质文件下载地址。
    const QUrl url(QString("https://modules.easyeda.com/qAxj6KHrDKw4blvCG8QJPs7Y/%1").arg(m_uuid));
    if (!executeRequest(url, ResourceType::Model3DStep, MODEL_TIMEOUT_MS, MAX_RETRIES, responseData, errorMsg)) {
        emit fetchError(m_componentId, errorMsg);
        return false;
    }

    emit model3DFetched(m_componentId, m_uuid, responseData);
    qDebug() << "3D model MTL data fetched successfully for:" << m_componentId;
    return true;
}

/**
 * @brief 执行一次带重试策略的同步等待请求。
 * @param url 请求 URL
 * @param resourceType 请求资源类型
 * @param timeoutMs 连接和读取超时时间（毫秒）
 * @param maxRetries 最大重试次数
 * @param outData 输出响应数据
 * @param errorMsg 输出错误信息
 * @return 请求成功时返回 true
 */
bool NetworkWorker::executeRequest(const QUrl& url,
                                   ResourceType resourceType,
                                   int timeoutMs,
                                   int maxRetries,
                                   QByteArray& outData,
                                   QString& errorMsg) {
    // 根据资源类型获取默认请求配置。
    RequestProfile profile = RequestProfiles::fromType(resourceType);
    profile.connectTimeoutMs = timeoutMs;
    profile.readTimeoutMs = qMax(profile.readTimeoutMs, timeoutMs);
    profile.maxRetries = maxRetries;
    // 将默认请求配置转换为包含弱网络策略的重试策略。
    RetryPolicy policy = RetryPolicy::fromProfile(profile, ConfigService::instance()->getWeakNetworkSupport());

    // 创建异步请求对象，随后通过阻塞上下文同步等待完成。
    AsyncNetworkRequest* request = NetworkClient::instance().getAsync(url, resourceType, policy);
    if (!request) {
        errorMsg = QStringLiteral("Failed to create network request");
        return false;
    }

    // 白名单拒绝等场景：request 已完成，直接返回结果，避免阻塞等待
    if (request->isFinished()) {
        // 读取已经完成的请求结果，避免进入等待流程。
        const NetworkResult doneResult = request->result();
        delete request;
        if (doneResult.wasCancelled) {
            errorMsg = "Request cancelled";
            return false;
        }
        if (!doneResult.success) {
            errorMsg =
                doneResult.error.isEmpty()
                    ? QString("Network error for %1 after %2 retries").arg(url.toString()).arg(doneResult.retryCount)
                    : doneResult.error;
            qWarning() << "NetworkWorker:" << errorMsg << "for:" << m_componentId;
            return false;
        }
        outData = doneResult.data;
        return true;
    }

    {
        QMutexLocker locker(&m_mutex);
        m_currentRequest = request;
    }

    // 创建跨线程完成通知的阻塞等待上下文。
    auto context = std::make_shared<BlockingRequestContext>();
    QObject::connect(
        request,
        &AsyncNetworkRequest::finished,
        request,
        [context](const NetworkResult& result) { context->complete(result); },
        Qt::DirectConnection);

    // 等待请求完成并取得最终网络结果。
    const NetworkResult result = context->wait();

    {
        QMutexLocker locker(&m_mutex);
        if (m_currentRequest == request) {
            m_currentRequest = nullptr;
        }
    }
    QMetaObject::invokeMethod(request, &QObject::deleteLater, Qt::QueuedConnection);

    if (result.wasCancelled) {
        errorMsg = "Request cancelled";
        return false;
    }

    if (!result.success) {
        errorMsg = result.error.isEmpty()
                       ? QString("Network error for %1 after %2 retries").arg(url.toString()).arg(result.retryCount)
                       : result.error;
        qWarning() << "NetworkWorker:" << errorMsg << "for:" << m_componentId;
        return false;
    }

    outData = result.data;
    return true;
}

/**
 * @brief 取消当前网络请求并标记 Worker 终止。
 */
void NetworkWorker::abort() {
    QMutexLocker locker(&m_mutex);
    if (m_currentRequest) {
        qDebug() << "Aborting network request for component:" << m_componentId;
        m_currentRequest->cancel();
        m_currentRequest = nullptr;
    }
}

}  // namespace EasyKiConverter
