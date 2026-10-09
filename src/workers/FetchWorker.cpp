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

// 初始化元器件获取任务及其可选的网络客户端。
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

// 执行元器件、CAD 和可选三维模型的获取流程。
void FetchWorker::run() {
    QElapsedTimer fetchTimer;
    fetchTimer.start();

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
        QDateTime now = QDateTime::currentDateTime();
        qint64 elapsedSinceLimit = s_lastRateLimitTime.msecsTo(now);
        if (elapsedSinceLimit < s_backoffMs * 2) {
            int delay = qMin(s_backoffMs, 5000);
            status->addDebugLog(QString("Rate limit backoff: delaying %1ms").arg(delay));
            QThread::msleep(static_cast<unsigned long>(delay));
        } else {
            s_backoffMs = 0;
        }
    }

    s_activeRequests.fetchAndAddRelaxed(1);

    bool hasError = false;
    QString errorMessage;

    if (m_fetch3DOnly) {
        if (!m_existing3DUuid.isEmpty()) {
            status->addDebugLog(QString("Using existing 3D model UUID: %1, skipping CAD fetch").arg(m_existing3DUuid));

            QString objUrl = QString("https://modules.easyeda.com/3dmodel/%1").arg(m_existing3DUuid);
            status->addDebugLog(QString("Downloading 3D model from: %1").arg(objUrl));

            QByteArray objData = httpGet(objUrl, MODEL_3D_TIMEOUT_MS, status, ResourceType::Model3DObj);
            if (objData.isEmpty()) {
                hasError = true;
                errorMessage = "Failed to download 3D model (empty response)";
                status->addDebugLog(QString("ERROR: %1").arg(errorMessage));
            } else {
                QByteArray actualObjData = objData;
                if (objData.size() >= 2 && objData[0] == 0x50 && objData[1] == 0x4B) {
                    status->addDebugLog("3D model data is ZIP compressed, decompressing...");
                    actualObjData = decompressZip(objData);
                }
                if (actualObjData.size() >= 2 && static_cast<unsigned char>(actualObjData[0]) == 0x1f &&
                    static_cast<unsigned char>(actualObjData[1]) == 0x8b) {
                    status->addDebugLog("3D model data is gzip compressed, decompressing...");
                    GzipUtils::DecompressResult decompResult = GzipUtils::decompress(actualObjData);
                    if (decompResult.success) {
                        actualObjData = decompResult.data;
                    } else {
                        status->addDebugLog("ERROR: Gzip decompression failed for 3D model data");
                    }
                }

                status->model3DObjRaw = actualObjData;
                status->addDebugLog(QString("3D model OBJ data ready: %1 bytes").arg(actualObjData.size()));

                QString stepUrl =
                    QString("https://modules.easyeda.com/qAxj6KHrDKw4blvCG8QJPs7Y/%1").arg(m_existing3DUuid);
                status->addDebugLog(QString("Downloading STEP model from: %1").arg(stepUrl));
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
        QString componentInfoUrl =
            QString("https://easyeda.com/api/products/%1/components?version=6.5.51").arg(m_componentId);
        status->addDebugLog(QString("Fetching component info from: %1").arg(componentInfoUrl));

        QByteArray componentInfoData =
            httpGet(componentInfoUrl, COMPONENT_INFO_TIMEOUT_MS, status, ResourceType::ComponentInfo);

        if (componentInfoData.isEmpty()) {
            hasError = true;
            errorMessage = "Failed to fetch component info (empty response)";
            status->addDebugLog(QString("ERROR: %1").arg(errorMessage));
        } else {
            QJsonParseError parseError;
            QJsonDocument doc = QJsonDocument::fromJson(componentInfoData, &parseError);
            if (parseError.error != QJsonParseError::NoError) {
                hasError = true;
                errorMessage = "Invalid JSON response: " + parseError.errorString();
                status->addDebugLog(QString("ERROR: %1").arg(errorMessage));
            } else {
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

    RequestProfile profile = RequestProfiles::fromType(resourceType);
    profile.connectTimeoutMs = timeoutMs;
    profile.readTimeoutMs = qMax(profile.readTimeoutMs, timeoutMs);
    bool weakNetworkSupport = false;
    if (!m_networkClient) {
        weakNetworkSupport = ConfigService::instance()->getWeakNetworkSupport();
    }
    RetryPolicy policy = RetryPolicy::fromProfile(profile, weakNetworkSupport);

    auto finishResult = [this, url, status](const NetworkResult& result) -> QByteArray {
        if (result.wasCancelled && m_isAborted.loadRelaxed()) {
            return QByteArray();
        }

        if (status) {
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

    AsyncNetworkRequest* request = NetworkClient::instance().getAsync(QUrl(url), resourceType, policy);
    if (!request) {
        if (status) {
            ComponentExportStatus::NetworkDiagnostics diag;
            diag.url = url;
            diag.errorString = QStringLiteral("Failed to create network request");
            status->networkDiagnostics.append(diag);
        }
        return QByteArray();
    }

    // 白名单拒绝等场景：request 已完成，直接返回结果，避免阻塞等待
    if (request->isFinished()) {
        const NetworkResult result = request->result();
        delete request;
        return finishResult(result);
    }

    {
        QMutexLocker locker(&m_replyMutex);
        m_currentRequest = request;
    }

    auto context = std::make_shared<BlockingRequestContext>();
    QObject::connect(
        request,
        &AsyncNetworkRequest::finished,
        request,
        [context](const NetworkResult& result) { context->complete(result); },
        Qt::DirectConnection);

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

// 解压获取到的 ZIP 模型数据并合并其中的内容。
QByteArray FetchWorker::decompressZip(const QByteArray& zipData) {
    if (zipData.size() < 30) {
        qWarning() << "Invalid ZIP data size";
        return QByteArray();
    }

    QByteArray result;
    int pos = 0;

    while (pos < zipData.size() - 30) {
        if (zipData[pos] != 0x50 || zipData[pos + 1] != 0x03 || zipData[pos + 2] != 0x04 || zipData[pos + 3] != 0x4b) {
            pos++;
            continue;
        }

        std::uint16_t compressedSize = ((std::uint8_t)zipData[pos + 18] | ((std::uint8_t)zipData[pos + 19] << 8));
        std::uint16_t uncompressedSize = ((std::uint8_t)zipData[pos + 22] | ((std::uint8_t)zipData[pos + 23] << 8));
        std::uint16_t fileNameLen = ((std::uint8_t)zipData[pos + 26] | ((std::uint8_t)zipData[pos + 27] << 8));
        std::uint16_t extraFieldLen = ((std::uint8_t)zipData[pos + 28] | ((std::uint8_t)zipData[pos + 29] << 8));
        int dataStart = pos + 30 + fileNameLen + extraFieldLen;
        std::uint16_t compressionMethod = ((std::uint8_t)zipData[pos + 8] | ((std::uint8_t)zipData[pos + 9] << 8));
        if (compressionMethod == 8) {
            QByteArray compressedData = zipData.mid(dataStart, compressedSize);
            GzipUtils::DecompressResult decompResult = GzipUtils::decompress(compressedData);
            if (decompResult.success) {
                result.append(decompResult.data);
            } else {
                qWarning() << "Failed to decompress ZIP entry at pos" << pos;
            }
        } else if (compressionMethod == 0) {
            QByteArray uncompressedData = zipData.mid(dataStart, uncompressedSize);
            result.append(uncompressedData);
        } else {
            qWarning() << "Unsupported ZIP compression method:" << compressionMethod;
        }

        pos = dataStart + compressedSize;
    }

    return result;
}

// 从 CAD JSON 中提取三维模型标识并获取模型文件。
bool FetchWorker::fetch3DModelData(QSharedPointer<ComponentExportStatus> status) {
    QString uuid;

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(status->cadDataRaw, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        QString msg = "Failed to parse CAD data for 3D model UUID";
        status->addDebugLog(msg);
        qWarning() << msg;
        return false;
    }

    QJsonObject rootObj = doc.object();
    QJsonObject obj;
    if (rootObj.contains("result") && rootObj["result"].isObject()) {
        obj = rootObj["result"].toObject();
    } else {
        obj = rootObj;
    }

    if (obj.contains("packageDetail") && obj["packageDetail"].isObject()) {
        QJsonObject packageDetail = obj["packageDetail"].toObject();
        if (packageDetail.contains("dataStr")) {
            QJsonObject dataObj;
            if (packageDetail["dataStr"].isObject()) {
                dataObj = packageDetail["dataStr"].toObject();
            } else if (packageDetail["dataStr"].isString()) {
                QString dataStrRaw = packageDetail["dataStr"].toString();
                if (!dataStrRaw.isEmpty()) {
                    QJsonDocument dataDoc = QJsonDocument::fromJson(dataStrRaw.toUtf8());
                    dataObj = dataDoc.object();
                }
            }

            if (!dataObj.isEmpty() && dataObj.contains("shape") && dataObj["shape"].isArray()) {
                QJsonArray shapes = dataObj["shape"].toArray();
                for (const QJsonValue& shapeVal : shapes) {
                    QString shapeStr = shapeVal.toString();
                    if (!shapeStr.startsWith("SVGNODE~")) {
                        continue;
                    }
                    int tildeIndex = shapeStr.indexOf('~');
                    if (tildeIndex == -1) {
                        continue;
                    }
                    QString jsonStr = shapeStr.mid(tildeIndex + 1);
                    QJsonDocument nodeDoc = QJsonDocument::fromJson(jsonStr.toUtf8());
                    QJsonObject nodeObj = nodeDoc.object();
                    if (!nodeObj.contains("attrs") || !nodeObj["attrs"].isObject()) {
                        continue;
                    }
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
        QString msg = "No 3D model UUID found in CAD data";
        status->addDebugLog(msg);
        qDebug() << msg;
        return false;
    }

    status->addDebugLog(QString("Fetching 3D model for UUID: %1").arg(uuid));

    QString objUrl = QString("https://modules.easyeda.com/3dmodel/%1").arg(uuid);
    status->addDebugLog(QString("Downloading 3D model from: %1").arg(objUrl));
    QByteArray objData = httpGet(objUrl, MODEL_3D_TIMEOUT_MS, status, ResourceType::Model3DObj);
    if (objData.isEmpty()) {
        QString msg = QString("ERROR: Failed to download 3D model from: %1").arg(objUrl);
        status->addDebugLog(msg);
        qWarning() << msg;
        return false;
    }

    QByteArray actualObjData = objData;
    if (objData.size() >= 2 && objData[0] == 0x50 && objData[1] == 0x4B) {
        status->addDebugLog("3D model data is ZIP compressed, decompressing...");
        actualObjData = decompressZip(objData);
    }

    if (actualObjData.size() >= 2 && static_cast<unsigned char>(actualObjData[0]) == 0x1f &&
        static_cast<unsigned char>(actualObjData[1]) == 0x8b) {
        status->addDebugLog("3D model data is gzip compressed, decompressing...");
        GzipUtils::DecompressResult decompResult = GzipUtils::decompress(actualObjData);
        if (decompResult.success) {
            actualObjData = decompResult.data;
        } else {
            status->addDebugLog("ERROR: Gzip decompression failed for 3D model data");
        }
    }

    if (actualObjData.isEmpty()) {
        QString msg = "ERROR: Failed to decompress 3D model data";
        status->addDebugLog(msg);
        qWarning() << msg;
        return false;
    }

    status->model3DObjRaw = actualObjData;
    status->addDebugLog(QString("3D model (OBJ) data fetched successfully for: %1").arg(status->componentId));

    QString stepUrl = QString("https://modules.easyeda.com/qAxj6KHrDKw4blvCG8QJPs7Y/%1").arg(uuid);
    status->addDebugLog(QString("Downloading STEP model from: %1").arg(stepUrl));
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

// 终止当前任务并取消尚未完成的网络请求。
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

// 根据重试次数计算带抖动的等待时间。
int FetchWorker::calculateRetryDelay(int retryCount) {
    int baseDelay;
    if (retryCount >= 0 && retryCount < static_cast<int>(sizeof(RETRY_DELAYS_MS) / sizeof(RETRY_DELAYS_MS[0]))) {
        baseDelay = RETRY_DELAYS_MS[retryCount];
    } else {
        baseDelay = RETRY_DELAYS_MS[sizeof(RETRY_DELAYS_MS) / sizeof(RETRY_DELAYS_MS[0]) - 1];
    }

    int jitter = static_cast<int>(baseDelay * 0.2);
    int randomOffset = QRandomGenerator::global()->bounded(-jitter, jitter + 1);
    return qMax(100, baseDelay + randomOffset);
}

}  // namespace EasyKiConverter
