#include "FetchWorker.h"

#include "BaseWorker.h"
#include "core/network/BlockingRequestContext.h"
#include "core/network/NetworkClient.h"
#include "core/utils/GzipUtils.h"
#include "services/ConfigService.h"

#include <QAtomicInt>
#include <QCoreApplication>
#include <QDateTime>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaObject>
#include <QMutex>
#include <QThread>
#include <QTimer>
#include <QWaitCondition>

#include <zlib.h>

namespace EasyKiConverter {

QAtomicInt FetchWorker::s_activeRequests = 0;
QMutex FetchWorker::s_rateLimitMutex;
QDateTime FetchWorker::s_lastRateLimitTime;
int FetchWorker::s_backoffMs = 0;

/**
 * @brief 初始化元器件获取任务及其可选的网络客户端。
 * @param componentId 元器件编号。
 * @param need3DModel 是否需要获取三维模型。
 * @param fetch3DOnly 是否仅获取三维模型。
 * @param existing3DUuid 已知的三维模型标识。
 * @param networkClient 可选的网络客户端。
 * @param parent 父对象。
 */
FetchWorker::FetchWorker(const QString& componentId,
                         bool need3DModel,
                         bool fetch3DOnly,
                         const QString& existing3DUuid,
                         INetworkClient* networkClient,
                         QObject* parent)
    : m_componentId(componentId)
    , m_need3DModel(need3DModel)
    , m_fetch3DOnly(fetch3DOnly)
    , m_existing3DUuid(existing3DUuid)
    , m_networkClient(networkClient)
    , m_currentRequest(nullptr)
    , m_isAborted(0) {
    Q_UNUSED(parent);
}

FetchWorker::~FetchWorker() = default;

/**
 * @brief 执行元器件、CAD 和可选三维模型的获取流程。
 */
void FetchWorker::run() {
    // 记录本次获取任务的耗时。
    QElapsedTimer fetchTimer;
    fetchTimer.start();

    // 保存元器件获取结果及诊断信息。
    QSharedPointer<ComponentExportStatus> status = QSharedPointer<ComponentExportStatus>::create();
    status->componentId = m_componentId;
    status->need3DModel = m_need3DModel;
    status->fetch3DOnly = m_fetch3DOnly;

    if (m_isAborted.loadRelaxed()) {
        status->fetchSuccess = false;
        status->fetchMessage = "Export cancelled";
        status->isCancelled = true;
        status->addDebugLog(QString("FetchWorker cancelled early for component: %1").arg(m_componentId));
        emit fetchCompleted(status);
        return;
    }

    if (m_fetch3DOnly) {
        status->addDebugLog(QString("FetchWorker started for 3D model only: %1").arg(m_componentId));
    } else {
        status->addDebugLog(QString("FetchWorker started for component: %1").arg(m_componentId));
    }

    if (s_backoffMs > 0) {
        QMutexLocker locker(&s_rateLimitMutex);
        // 获取当前时间，用于计算距离上次限流的间隔。
        QDateTime now = QDateTime::currentDateTime();
        // 计算上次限流发生至今经过的毫秒数。
        qint64 elapsedSinceLimit = s_lastRateLimitTime.msecsTo(now);
        if (elapsedSinceLimit < s_backoffMs * 2) {
            // 限制单次退避等待时间，避免任务长时间阻塞。
            int delay = qMin(s_backoffMs, 5000);
            status->addDebugLog(QString("Rate limit backoff: delaying %1ms").arg(delay));
            QThread::msleep(static_cast<unsigned long>(delay));
        } else {
            s_backoffMs = 0;
        }
    }

    s_activeRequests.fetchAndAddRelaxed(1);

    // 标记获取流程是否发生错误。
    bool hasError = false;
    // 保存最终返回给调用方的错误信息。
    QString errorMessage;

    if (m_fetch3DOnly) {
        if (!m_existing3DUuid.isEmpty()) {
            status->addDebugLog(QString("Using existing 3D model UUID: %1, skipping CAD fetch").arg(m_existing3DUuid));

            // 构造 OBJ 三维模型下载地址。
            QString objUrl = QString("https://modules.easyeda.com/3dmodel/%1").arg(m_existing3DUuid);
            status->addDebugLog(QString("Downloading 3D model from: %1").arg(objUrl));

            // 获取原始 OBJ 响应数据。
            QByteArray objData = httpGet(objUrl, MODEL_3D_TIMEOUT_MS, status, ResourceType::Model3DObj);
            if (objData.isEmpty()) {
                hasError = true;
                errorMessage = "Failed to download 3D model (empty response)";
                status->addDebugLog(QString("ERROR: %1").arg(errorMessage));
            } else {
                // 保存解压后的实际 OBJ 数据。
                QByteArray actualObjData = objData;
                if (objData.size() >= 2 && objData[0] == 0x50 && objData[1] == 0x4B) {
                    status->addDebugLog("3D model data is ZIP compressed, decompressing...");
                    actualObjData = decompressZip(objData);
                }
                if (actualObjData.size() >= 2 && static_cast<unsigned char>(actualObjData[0]) == 0x1f &&
                    static_cast<unsigned char>(actualObjData[1]) == 0x8b) {
                    status->addDebugLog("3D model data is gzip compressed, decompressing...");
                    // 保存 gzip 解压结果及其状态。
                    GzipUtils::DecompressResult decompResult = GzipUtils::decompress(actualObjData);
                    if (decompResult.success) {
                        actualObjData = decompResult.data;
                    } else {
                        status->addDebugLog("ERROR: Gzip decompression failed for 3D model data");
                    }
                }

                status->model3DObjRaw = actualObjData;
                status->addDebugLog(QString("3D model OBJ data ready: %1 bytes").arg(actualObjData.size()));

                // 构造 STEP 三维模型下载地址。
                QString stepUrl =
                    QString("https://modules.easyeda.com/qAxj6KHrDKw4blvCG8QJPs7Y/%1").arg(m_existing3DUuid);
                status->addDebugLog(QString("Downloading STEP model from: %1").arg(stepUrl));
                // 获取原始 STEP 响应数据。
                QByteArray stepData = httpGet(stepUrl, MODEL_3D_TIMEOUT_MS, status, ResourceType::Model3DStep);
                if (!stepData.isEmpty()) {
                    status->model3DStepRaw = stepData;
                    status->addDebugLog(QString("STEP model data ready: %1 bytes").arg(stepData.size()));
                } else {
                    status->addDebugLog("No STEP model data available (this is optional)");
                }
            }
        } else {
            QString cadUrl =
                QString("https://easyeda.com/api/products/%1/components?version=6.5.51").arg(m_componentId);
            status->addDebugLog(QString("Fetching CAD data for 3D UUID: %1").arg(cadUrl));

            // 获取用于提取三维模型标识的 CAD 数据。
            QByteArray cadData = httpGet(cadUrl, COMPONENT_INFO_TIMEOUT_MS, status, ResourceType::CadData);
            if (cadData.isEmpty()) {
                hasError = true;
                errorMessage = "Failed to fetch CAD data for 3D model UUID (empty response)";
                status->addDebugLog(QString("ERROR: %1").arg(errorMessage));
            } else {
                status->cadDataRaw = cadData;
                status->cadJsonRaw = cadData;
                if (!fetch3DModelData(status)) {
                    hasError = true;
                    errorMessage = "Failed to fetch 3D model data";
                    status->addDebugLog(QString("ERROR: %1").arg(errorMessage));
                }
            }
        }
    } else {
        // 构造元器件信息接口地址。
        QString componentInfoUrl =
            QString("https://easyeda.com/api/products/%1/components?version=6.5.51").arg(m_componentId);
        status->addDebugLog(QString("Fetching component info from: %1").arg(componentInfoUrl));

        // 获取元器件信息及其 CAD 数据。
        QByteArray componentInfoData =
            httpGet(componentInfoUrl, COMPONENT_INFO_TIMEOUT_MS, status, ResourceType::ComponentInfo);

        if (componentInfoData.isEmpty()) {
            hasError = true;
            errorMessage = "Failed to fetch component info (empty response)";
            status->addDebugLog(QString("ERROR: %1").arg(errorMessage));
        } else {
            // 保存元器件信息 JSON 的解析状态。
            QJsonParseError parseError;
            // 解析元器件信息响应。
            QJsonDocument doc = QJsonDocument::fromJson(componentInfoData, &parseError);
            if (parseError.error != QJsonParseError::NoError) {
                hasError = true;
                errorMessage = "Invalid JSON response: " + parseError.errorString();
                status->addDebugLog(QString("ERROR: %1").arg(errorMessage));
            } else {
                // 获取接口返回的顶层 JSON 对象。
                QJsonObject rootObj = doc.object();
                if (rootObj.contains("success") && !rootObj["success"].toBool()) {
                    hasError = true;
                    errorMessage = "API returned error: " +
                                   (rootObj.contains("message") ? rootObj["message"].toString() : "Unknown error");
                    status->addDebugLog(QString("ERROR: %1").arg(errorMessage));
                } else if (!rootObj.contains("result") || rootObj["result"].isNull()) {
                    hasError = true;
                    errorMessage = "Component not found (null result)";
                    status->addDebugLog(QString("ERROR: %1").arg(errorMessage));
                } else if (rootObj["result"].isObject() && rootObj["result"].toObject().isEmpty()) {
                    hasError = true;
                    errorMessage = "Component data is empty";
                    status->addDebugLog(QString("ERROR: %1").arg(errorMessage));
                } else {
                    status->componentInfoRaw = componentInfoData;
                    status->cinfoJsonRaw = componentInfoData;
                    status->cadDataRaw = componentInfoData;
                    status->cadJsonRaw = componentInfoData;
                    status->addDebugLog(QString("Component info (including CAD) fetched for: %1, Size: %2 bytes")
                                            .arg(m_componentId)
                                            .arg(componentInfoData.size()));

                    if (m_need3DModel && !hasError) {
                        status->addDebugLog("Parsing CAD data to extract 3D model UUID...");
                        if (!fetch3DModelData(status)) {
                        }
                    }
                }
            }
        }
    }

    s_activeRequests.fetchAndSubRelaxed(1);
    status->fetchDurationMs = fetchTimer.elapsed();

    if (hasError) {
        status->fetchSuccess = false;
        status->fetchMessage = errorMessage;
        status->addDebugLog(QString("FetchWorker failed for component: %1, Error: %2, Duration: %3ms")
                                .arg(m_componentId)
                                .arg(errorMessage)
                                .arg(status->fetchDurationMs));
    } else {
        status->fetchSuccess = true;
        status->fetchMessage = "Fetch completed successfully";
        status->addDebugLog(QString("FetchWorker completed successfully for component: %1, Duration: %2ms")
                                .arg(m_componentId)
                                .arg(status->fetchDurationMs));
    }

    emit fetchCompleted(status);
}

/**
 * @brief 发起带超时、重试和取消支持的 HTTP GET 请求。
 * @param url 请求 URL。
 * @param timeoutMs 请求超时时间，单位为毫秒。
 * @param status 用于记录请求状态的对象。
 * @param resourceType 请求资源类型。
 * @return 响应正文；请求失败或被取消时返回空数组。
 */
QByteArray FetchWorker::httpGet(const QString& url,
                                int timeoutMs,
                                QSharedPointer<ComponentExportStatus> status,
                                ResourceType resourceType) {
    if (m_isAborted.loadRelaxed()) {
        return QByteArray();
    }

    // 根据资源类型获取网络请求配置。
    RequestProfile profile = RequestProfiles::fromType(resourceType);
    profile.connectTimeoutMs = timeoutMs;
    profile.readTimeoutMs = qMax(profile.readTimeoutMs, timeoutMs);
    // 标记是否启用弱网兼容策略。
    bool weakNetworkSupport = false;
    if (!m_networkClient) {
        weakNetworkSupport = ConfigService::instance()->getWeakNetworkSupport();
    }
    // 根据请求配置和网络环境生成重试策略。
    RetryPolicy policy = RetryPolicy::fromProfile(profile, weakNetworkSupport);

    auto finishResult = [this, url, status](const NetworkResult& result) -> QByteArray {
        if (result.wasCancelled && m_isAborted.loadRelaxed()) {
            return QByteArray();
        }

        if (status) {
            // 记录当前请求的网络诊断信息。
            ComponentExportStatus::NetworkDiagnostics diag;
            diag.url = result.diagnostic.url.isEmpty() ? url : result.diagnostic.url;
            diag.statusCode = result.statusCode;
            diag.errorString = result.success ? QString() : result.error;
            diag.responseContentType = result.diagnostic.responseContentType;
            diag.retryAfter = result.diagnostic.retryAfter;
            diag.rateLimitRemaining = result.diagnostic.rateLimitRemaining;
            diag.rateLimitReset = result.diagnostic.rateLimitReset;
            diag.responseSummary = result.diagnostic.responseSummary;
            diag.retryCount = result.retryCount;
            diag.latencyMs = result.elapsedMs;
            diag.wasRateLimited = result.diagnostic.wasRateLimited;
            diag.hasRateLimitHint = result.diagnostic.hasRateLimitHint;
            status->networkDiagnostics.append(diag);
        }

        if (!result.success) {
            if (result.statusCode == 429) {
                QMutexLocker locker(&s_rateLimitMutex);
                s_lastRateLimitTime = QDateTime::currentDateTime();
                s_backoffMs = qMin(s_backoffMs == 0 ? 1000 : s_backoffMs * 2, 8000);
            }
            return QByteArray();
        }

        // Note: AsyncNetworkRequest already handles gzip decompression internally
        // based on Content-Encoding header or magic bytes. result.data is already decompressed.
        return result.data;
    };

    if (m_networkClient) {
        return finishResult(m_networkClient->get(QUrl(url), resourceType, policy));
    }

    // 创建异步请求，以便在无注入网络客户端时复用全局网络服务。
    AsyncNetworkRequest* request = NetworkClient::instance().getAsync(QUrl(url), resourceType, policy);
    if (!request) {
        if (status) {
            // 记录创建异步请求失败的诊断信息。
            ComponentExportStatus::NetworkDiagnostics diag;
            diag.url = url;
            diag.errorString = QStringLiteral("Failed to create network request");
            status->networkDiagnostics.append(diag);
        }
        return QByteArray();
    }

    // 白名单拒绝等场景：request 已完成，直接返回结果，避免阻塞等待
    if (request->isFinished()) {
        // 读取已完成请求的结果并立即释放请求对象。
        const NetworkResult result = request->result();
        delete request;
        return finishResult(result);
    }

    {
        QMutexLocker locker(&m_replyMutex);
        m_currentRequest = request;
    }

    // 创建阻塞等待上下文，将异步请求结果同步返回给当前工作线程。
    auto context = std::make_shared<BlockingRequestContext>();
    QObject::connect(
        request,
        &AsyncNetworkRequest::finished,
        request,
        [context](const NetworkResult& result) { context->complete(result); },
        Qt::DirectConnection);

    // 等待网络请求完成或被取消。
    const NetworkResult result = context->wait();

    {
        QMutexLocker locker(&m_replyMutex);
        if (m_currentRequest == request) {
            m_currentRequest = nullptr;
        }
    }
    QMetaObject::invokeMethod(request, &QObject::deleteLater, Qt::QueuedConnection);

    return finishResult(result);
}

/**
 * @brief 解压获取到的 ZIP 模型数据并合并其中的内容。
 * @param zipData ZIP 压缩数据。
 * @return 合并后的模型数据；输入无效或没有可解压条目时返回空数组。
 */
QByteArray FetchWorker::decompressZip(const QByteArray& zipData) {
    if (zipData.size() < 30) {
        qWarning() << "Invalid ZIP data size";
        return QByteArray();
    }

    // 保存所有 ZIP 条目解压后合并得到的模型数据。
    QByteArray result;
    // 当前扫描到的 ZIP 本地文件头偏移。
    int pos = 0;

    while (pos < zipData.size() - 30) {
        if (zipData[pos] != 0x50 || zipData[pos + 1] != 0x03 || zipData[pos + 2] != 0x04 || zipData[pos + 3] != 0x4b) {
            pos++;
            continue;
        }

        // 读取当前 ZIP 条目的压缩数据长度。
        std::uint16_t compressedSize = ((std::uint8_t)zipData[pos + 18] | ((std::uint8_t)zipData[pos + 19] << 8));
        // 读取当前 ZIP 条目解压后的数据长度。
        std::uint16_t uncompressedSize = ((std::uint8_t)zipData[pos + 22] | ((std::uint8_t)zipData[pos + 23] << 8));
        // 读取当前 ZIP 条目的文件名长度。
        std::uint16_t fileNameLen = ((std::uint8_t)zipData[pos + 26] | ((std::uint8_t)zipData[pos + 27] << 8));
        // 读取当前 ZIP 条目的扩展字段长度。
        std::uint16_t extraFieldLen = ((std::uint8_t)zipData[pos + 28] | ((std::uint8_t)zipData[pos + 29] << 8));
        // 计算条目数据在压缩包中的起始偏移。
        int dataStart = pos + 30 + fileNameLen + extraFieldLen;
        // 读取当前 ZIP 条目的压缩方法。
        std::uint16_t compressionMethod = ((std::uint8_t)zipData[pos + 8] | ((std::uint8_t)zipData[pos + 9] << 8));
        if (compressionMethod == 8) {
            // 提取使用 deflate 压缩的条目数据。
            QByteArray compressedData = zipData.mid(dataStart, compressedSize);
            // 保存 deflate 解压结果及其状态。
            GzipUtils::DecompressResult decompResult = GzipUtils::decompress(compressedData);
            if (decompResult.success) {
                result.append(decompResult.data);
            } else {
                qWarning() << "Failed to decompress ZIP entry at pos" << pos;
            }
        } else if (compressionMethod == 0) {
            // 提取未压缩存储的条目数据。
            QByteArray uncompressedData = zipData.mid(dataStart, uncompressedSize);
            result.append(uncompressedData);
        } else {
            qWarning() << "Unsupported ZIP compression method:" << compressionMethod;
        }

        pos = dataStart + compressedSize;
    }

    return result;
}

/**
 * @brief 从 CAD JSON 中提取三维模型标识并获取模型文件。
 * @param status 保存 CAD 数据、模型文件和诊断信息的状态对象。
 * @return 成功获取 OBJ 模型数据时返回 true，否则返回 false。
 */
bool FetchWorker::fetch3DModelData(QSharedPointer<ComponentExportStatus> status) {
    // 保存从 CAD 轮廓中提取出的三维模型标识。
    QString uuid;

    // 保存 CAD JSON 的解析状态。
    QJsonParseError parseError;
    // 解析 CAD 原始数据。
    QJsonDocument doc = QJsonDocument::fromJson(status->cadDataRaw, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        // 保存 CAD 数据解析失败的诊断信息。
        QString msg = "Failed to parse CAD data for 3D model UUID";
        status->addDebugLog(msg);
        qWarning() << msg;
        return false;
    }

    // 获取 CAD 响应的顶层对象。
    QJsonObject rootObj = doc.object();
    // 保存实际包含元器件数据的 JSON 对象。
    QJsonObject obj;
    if (rootObj.contains("result") && rootObj["result"].isObject()) {
        obj = rootObj["result"].toObject();
    } else {
        obj = rootObj;
    }

    if (obj.contains("packageDetail") && obj["packageDetail"].isObject()) {
        // 读取封装详情对象。
        QJsonObject packageDetail = obj["packageDetail"].toObject();
        if (packageDetail.contains("dataStr")) {
            // 保存封装详情中的形状数据对象。
            QJsonObject dataObj;
            if (packageDetail["dataStr"].isObject()) {
                dataObj = packageDetail["dataStr"].toObject();
            } else if (packageDetail["dataStr"].isString()) {
                // 保存字符串形式的形状数据。
                QString dataStrRaw = packageDetail["dataStr"].toString();
                if (!dataStrRaw.isEmpty()) {
                    // 解析字符串形式的形状数据。
                    QJsonDocument dataDoc = QJsonDocument::fromJson(dataStrRaw.toUtf8());
                    dataObj = dataDoc.object();
                }
            }

            if (!dataObj.isEmpty() && dataObj.contains("shape") && dataObj["shape"].isArray()) {
                // 获取所有封装形状节点。
                QJsonArray shapes = dataObj["shape"].toArray();
                for (const QJsonValue& shapeVal : shapes) {
                    // 读取单个形状节点的序列化内容。
                    QString shapeStr = shapeVal.toString();
                    if (!shapeStr.startsWith("SVGNODE~")) {
                        continue;
                    }
                    // 定位 SVGNODE 前缀与 JSON 内容之间的分隔符。
                    int tildeIndex = shapeStr.indexOf('~');
                    if (tildeIndex == -1) {
                        continue;
                    }
                    // 提取形状节点中的 JSON 文本。
                    QString jsonStr = shapeStr.mid(tildeIndex + 1);
                    // 解析形状节点 JSON。
                    QJsonDocument nodeDoc = QJsonDocument::fromJson(jsonStr.toUtf8());
                    // 获取形状节点对象。
                    QJsonObject nodeObj = nodeDoc.object();
                    if (!nodeObj.contains("attrs") || !nodeObj["attrs"].isObject()) {
                        continue;
                    }
                    // 读取形状节点属性。
                    QJsonObject attrs = nodeObj["attrs"].toObject();
                    if (attrs.contains("c_etype") && attrs["c_etype"].toString() == "outline3D" &&
                        attrs.contains("uuid") && !attrs["uuid"].toString().isEmpty()) {
                        uuid = attrs["uuid"].toString();
                        qDebug() << "Found 3D model UUID from outline3D SVGNODE:" << uuid;
                        break;
                    }
                }
            }
        }
    }

    if (uuid.isEmpty()) {
        // 保存未找到三维模型标识的诊断信息。
        QString msg = "No 3D model UUID found in CAD data";
        status->addDebugLog(msg);
        qDebug() << msg;
        return false;
    }

    status->addDebugLog(QString("Fetching 3D model for UUID: %1").arg(uuid));

    // 构造 OBJ 三维模型下载地址。
    QString objUrl = QString("https://modules.easyeda.com/3dmodel/%1").arg(uuid);
    status->addDebugLog(QString("Downloading 3D model from: %1").arg(objUrl));
    // 获取原始 OBJ 响应数据。
    QByteArray objData = httpGet(objUrl, MODEL_3D_TIMEOUT_MS, status, ResourceType::Model3DObj);
    if (objData.isEmpty()) {
        // 保存 OBJ 下载失败的诊断信息。
        QString msg = QString("ERROR: Failed to download 3D model from: %1").arg(objUrl);
        status->addDebugLog(msg);
        qWarning() << msg;
        return false;
    }

    // 保存解压后的实际 OBJ 数据。
    QByteArray actualObjData = objData;
    if (objData.size() >= 2 && objData[0] == 0x50 && objData[1] == 0x4B) {
        status->addDebugLog("3D model data is ZIP compressed, decompressing...");
        actualObjData = decompressZip(objData);
    }

    if (actualObjData.size() >= 2 && static_cast<unsigned char>(actualObjData[0]) == 0x1f &&
        static_cast<unsigned char>(actualObjData[1]) == 0x8b) {
        status->addDebugLog("3D model data is gzip compressed, decompressing...");
        // 保存 gzip 解压结果及其状态。
        GzipUtils::DecompressResult decompResult = GzipUtils::decompress(actualObjData);
        if (decompResult.success) {
            actualObjData = decompResult.data;
        } else {
            status->addDebugLog("ERROR: Gzip decompression failed for 3D model data");
        }
    }

    if (actualObjData.isEmpty()) {
        // 保存三维模型解压失败的诊断信息。
        QString msg = "ERROR: Failed to decompress 3D model data";
        status->addDebugLog(msg);
        qWarning() << msg;
        return false;
    }

    status->model3DObjRaw = actualObjData;
    status->addDebugLog(QString("3D model (OBJ) data fetched successfully for: %1").arg(status->componentId));

    // 构造 STEP 三维模型下载地址。
    QString stepUrl = QString("https://modules.easyeda.com/qAxj6KHrDKw4blvCG8QJPs7Y/%1").arg(uuid);
    status->addDebugLog(QString("Downloading STEP model from: %1").arg(stepUrl));
    // 获取原始 STEP 响应数据。
    QByteArray stepData = httpGet(stepUrl, MODEL_3D_TIMEOUT_MS, status, ResourceType::Model3DStep);
    if (!stepData.isEmpty() && stepData.size() > 100) {
        status->model3DStepRaw = stepData;
        status->addDebugLog(QString("3D model (STEP) data fetched successfully for: %1, Size: %2 bytes")
                                .arg(status->componentId)
                                .arg(stepData.size()));
    } else {
        status->addDebugLog(
            QString("WARNING: Failed to download STEP model or STEP data too small for: %1").arg(status->componentId));
    }

    return true;
}

/**
 * @brief 终止当前任务并取消尚未完成的网络请求。
 */
void FetchWorker::abort() {
    if (m_isAborted.testAndSetRelaxed(0, 1)) {
        qDebug() << "FetchWorker abort requested for:" << m_componentId;
        QMutexLocker locker(&m_replyMutex);
        if (m_currentRequest && !m_currentRequest->isFinished()) {
            m_currentRequest->cancel();
        }
        m_currentRequest = nullptr;
    }
}

/**
 * @brief 根据重试次数计算带抖动的等待时间。
 * @param retryCount 当前重试次数。
 * @return 包含随机抖动且不低于最小值的等待毫秒数。
 */
int FetchWorker::calculateRetryDelay(int retryCount) {
    // 根据重试次数选择基础退避时长。
    int baseDelay;
    if (retryCount >= 0 && retryCount < static_cast<int>(sizeof(RETRY_DELAYS_MS) / sizeof(RETRY_DELAYS_MS[0]))) {
        baseDelay = RETRY_DELAYS_MS[retryCount];
    } else {
        baseDelay = RETRY_DELAYS_MS[sizeof(RETRY_DELAYS_MS) / sizeof(RETRY_DELAYS_MS[0]) - 1];
    }

    // 计算基础退避时长的随机抖动范围。
    int jitter = static_cast<int>(baseDelay * 0.2);
    // 生成本次退避时长的随机偏移量。
    int randomOffset = QRandomGenerator::global()->bounded(-jitter, jitter + 1);
    return qMax(100, baseDelay + randomOffset);
}

}  // namespace EasyKiConverter
