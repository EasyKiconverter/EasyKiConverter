#ifndef NETWORKWORKER_H
#define NETWORKWORKER_H

#include "BaseWorker.h"
#include "core/network/INetworkClient.h"

#include <QJsonObject>
#include <QMutex>

namespace EasyKiConverter {

class AsyncNetworkRequest;

/**
 * @brief 网络工作线程
 *
 * 用于在后台线程中执行网络请求任务
 */
class NetworkWorker : public BaseWorker {
    Q_OBJECT

public:
    enum class TaskType { FetchComponentInfo, FetchCadData, Fetch3DModelObj, Fetch3DModelMtl };

    explicit NetworkWorker(const QString& componentId,
                           TaskType taskType,
                           // 标识符。
                           const QString& uuid = QString(),
                           QObject* parent = nullptr);

    ~NetworkWorker() override;

    void run() override;

signals:
    void componentInfoFetched(const QString& componentId, const QJsonObject& data);
    void cadDataFetched(const QString& componentId, const QJsonObject& data);
    void model3DFetched(const QString& componentId, const QString& uuid, const QByteArray& data);
    void fetchError(const QString& componentId, const QString& errorMessage);
    void requestProgress(const QString& componentId, int progress);

private:
    bool fetchComponentInfo();
    bool fetchCadData();
    bool fetch3DModelObj();
    bool fetch3DModelMtl();

public slots:
    void abort();

    bool executeRequest(const QUrl& url,
                        ResourceType resourceType,
                        int timeoutMs,
                        int maxRetries,
                        QByteArray& outData,
                        QString& errorMsg);

private:
    QString m_componentId;  ///< 标识符。
    TaskType m_taskType;  ///< 类型。
    QString m_uuid;  ///< 标识符。
    AsyncNetworkRequest* m_currentRequest;  ///< 请求。
    QMutex m_mutex;  ///< 对象状态互斥量。

    static const int DEFAULT_TIMEOUT_MS = 30000;  ///< 普通网络请求超时时间（毫秒）。
    static const int MODEL_TIMEOUT_MS = 45000;  ///< 三维模型请求超时时间（毫秒）。
    static const int MAX_RETRIES = 3;  ///< 网络请求最大重试次数。
};

}  // namespace EasyKiConverter

#endif  // NETWORKWORKER_H
