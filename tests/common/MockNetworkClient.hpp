#ifndef MOCKNETWORKCLIENT_HPP
#define MOCKNETWORKCLIENT_HPP

#include "core/network/AsyncNetworkRequest.h"
#include "core/network/INetworkClient.h"

#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <QTimer>

namespace EasyKiConverter::Test {

class MockNetworkClient : public INetworkClient {
public:
    struct ResponseSpec {
        NetworkResult result;
    };

    // 注册一个可按 URL 查询的 JSON 响应。
    void addJsonResponse(const QString& url, const QJsonObject& object, int statusCode = 200) {
        NetworkResult result;
        result.success = true;
        result.statusCode = statusCode;
        result.data = QJsonDocument(object).toJson(QJsonDocument::Compact);
        result.diagnostic.url = url;
        m_getResponses.insert(url, result);
    }

    // 注册一个可按 URL 查询的原始响应。
    void addResponse(const QString& url, const QByteArray& data, int statusCode = 200) {
        NetworkResult result;
        result.success = true;
        result.statusCode = statusCode;
        result.data = data;
        result.diagnostic.url = url;
        m_getResponses.insert(url, result);
    }

    // 注册一个失败响应并同步设置可识别的 HTTP 错误分类。
    void addErrorResponse(const QString& url, const QString& error, int statusCode = 0) {
        NetworkResult result;
        result.success = false;
        result.statusCode = statusCode;
        result.error = error;
        result.diagnostic.url = url;
        if (statusCode == 403) {
            result.diagnostic.errorType = NetworkErrorType::Forbidden;
        } else if (statusCode == 429) {
            result.diagnostic.errorType = NetworkErrorType::RateLimited;
            result.diagnostic.wasRateLimited = true;
        }
        m_getResponses.insert(url, result);
    }

    // 返回最近一次被 Mock 客户端访问的 URL。
    QString lastUrl() const {
        return m_lastUrl;
    }

    NetworkResult get(const QUrl& url, const RetryPolicy& policy = {}) override {
        return get(url, ResourceType::Unknown, policy);
    }

    NetworkResult get(const QUrl& url, ResourceType resourceType, const RetryPolicy& policy = {}) override {
        Q_UNUSED(policy);
        m_lastUrl = url.toString();
        NetworkResult result = m_getResponses.value(m_lastUrl);
        result.diagnostic.url = m_lastUrl;
        result.diagnostic.host = url.host();
        result.diagnostic.resourceType = resourceType;
        result.diagnostic.profileName = RequestProfiles::fromType(resourceType).name;
        return result;
    }

    NetworkResult post(const QUrl& url, const QByteArray& body, const RetryPolicy& policy = {}) override {
        return post(url, body, ResourceType::Unknown, policy);
    }

    NetworkResult post(const QUrl& url,
                       const QByteArray& body,
                       ResourceType resourceType,
                       const RetryPolicy& policy = {}) override {
        Q_UNUSED(body);
        Q_UNUSED(policy);
        m_lastUrl = url.toString();
        NetworkResult result = m_postResponses.value(m_lastUrl);
        result.diagnostic.url = m_lastUrl;
        result.diagnostic.host = url.host();
        result.diagnostic.resourceType = resourceType;
        result.diagnostic.profileName = RequestProfiles::fromType(resourceType).name;
        return result;
    }

    AsyncNetworkRequest* getAsync(const QUrl& url, ResourceType resourceType, const RetryPolicy& policy = {}) override {
        Q_UNUSED(policy);
        // 使用延迟完成避免信号在连接建立前发出
        // createFinished 会同步发出 finished 信号，
        // 如果不延迟，EasyedaApi 还没 connect 就收不到信号了
        auto* request = new AsyncNetworkRequest(QUrl(), nullptr, resourceType, RetryPolicy(), QByteArray(), nullptr);
        request->completeWithResultDelayed(get(url, resourceType, RetryPolicy()), 0);
        return request;
    }

    AsyncNetworkRequest* postAsync(const QUrl& url,
                                   const QByteArray& body,
                                   ResourceType resourceType,
                                   const RetryPolicy& policy = {}) override {
        Q_UNUSED(policy);
        return AsyncNetworkRequest::createFinished(post(url, body, resourceType, policy));
    }

    NetworkRuntimeStats runtimeStats() const override {
        return {};
    }

    QString formatRuntimeStats() const override {
        return QStringLiteral("MockNetworkClient runtime stats unavailable");
    }

private:
    QHash<QString, NetworkResult> m_getResponses;
    QHash<QString, NetworkResult> m_postResponses;
    QString m_lastUrl;
};

}  // namespace EasyKiConverter::Test

#endif  // MOCKNETWORKCLIENT_HPP
