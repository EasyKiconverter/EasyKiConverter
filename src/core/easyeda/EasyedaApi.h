#ifndef EASYEDAAPI_H
#define EASYEDAAPI_H

#include "core/network/INetworkClient.h"

#include <QJsonObject>
#include <QMutex>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QVector>

namespace EasyKiConverter {

class AsyncNetworkRequest;

class EasyedaApi : public QObject {
    Q_OBJECT

public:
    explicit EasyedaApi(QObject* parent = nullptr);
    explicit EasyedaApi(INetworkClient* networkClient, QObject* parent = nullptr);
    ~EasyedaApi() override;

    void setWeakNetworkSupport(bool enabled);
    bool weakNetworkSupport() const;

    void fetchComponentInfo(const QString& lcscId);
    void fetchCadData(const QString& lcscId);
    void fetch3DModelObj(const QString& uuid);
    void fetch3DModelStep(const QString& uuid);
    void cancelRequest();
    /**
     * @brief 取消指定元件编号或模型 UUID 的活动请求。
     * @param id 元件编号或模型 UUID。
     */
    void cancelRequestForId(const QString& id);

signals:
    void componentInfoFetched(const QString& lcscId, const QJsonObject& data);
    void cadDataFetched(const QString& lcscId, const QJsonObject& data);
    void model3DFetched(const QString& uuid, const QByteArray& data);
    void fetchError(const QString& errorMessage);
    void fetchError(const QString& id, const QString& errorMessage);

private:
    void handleComponentInfoResponse(const QString& lcscId, const QJsonObject& data);
    void handleCadDataResponse(const QString& lcscId, const QJsonObject& data);
    void handleAsyncRequestFinished(AsyncNetworkRequest* request,
                                    const QString& id,
                                    ResourceType resourceType,
                                    bool isBinary);
    void fetchWithNetworkClient(const QString& id, const QUrl& url, ResourceType resourceType, bool isBinary);
    void resetRequestState();
    QString buildComponentApiUrl(const QString& lcscId) const;
    QString build3DModelObjUrl(const QString& uuid) const;
    QString build3DModelStepUrl(const QString& uuid) const;
    bool validateLcscId(const QString& lcscId) const;

private:
    INetworkClient* m_networkClient;  ///< 网络请求客户端，不拥有其生命周期。
    QString m_currentLcscId;  ///< 当前正在获取基础信息的元件编号。
    QString m_currentUuid;  ///< 当前正在获取三维模型的 UUID。
    bool m_isFetching;  ///< 是否存在正在获取的元件基础信息请求。
    bool m_weakNetworkSupport;  ///< 是否启用弱网重试和超时策略。

    struct ActiveRequest {
        QPointer<AsyncNetworkRequest> request;  ///< 活动网络请求的弱指针。
        QString id;  ///< 请求关联的元件编号或模型 UUID。
    };

    QVector<ActiveRequest> m_activeRequests;  ///< 当前登记的活动请求列表。
    QMutex m_requestsMutex;  ///< 保护活动请求列表的互斥锁。
};

}  // namespace EasyKiConverter

#endif  // EASYEDAAPI_H
