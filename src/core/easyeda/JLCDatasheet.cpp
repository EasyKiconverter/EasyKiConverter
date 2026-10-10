#include "JLCDatasheet.h"

#include "core/network/NetworkClient.h"

#include <QDebug>
#include <QFile>

namespace EasyKiConverter {

JLCDatasheet::JLCDatasheet(QObject* parent)
    : QObject(parent), m_activeRequest(nullptr), m_isDownloading(false), m_weakNetworkSupport(false) {}

JLCDatasheet::~JLCDatasheet() {
    cancel();
}

/**
 * @brief 设置弱网重试支持开关。
 * @param enabled 是否启用弱网支持。
 */
void JLCDatasheet::setWeakNetworkSupport(bool enabled) {
    m_weakNetworkSupport = enabled;
}

/**
 * @brief 返回弱网重试支持状态。
 * @return 启用时返回 true。
 */
bool JLCDatasheet::weakNetworkSupport() const {
    return m_weakNetworkSupport;
}

/**
 * @brief 异步下载数据手册并保存到指定路径。
 * @param datasheetUrl 数据手册 URL。
 * @param savePath 本地保存路径。
 */
void JLCDatasheet::downloadDatasheet(const QString& datasheetUrl, const QString& savePath) {
    if (m_isDownloading) {
        qWarning() << "Already downloading datasheet";
        return;
    }

    if (datasheetUrl.isEmpty()) {
        const QString errorMsg = "Datasheet URL is empty";  // 缺少下载地址时返回的错误信息。
        qWarning() << errorMsg;
        emit downloadError(errorMsg);
        return;
    }

    m_datasheetUrl = datasheetUrl;
    m_savePath = savePath;
    m_isDownloading = true;

    RequestProfile profile = RequestProfiles::datasheet();  // 数据手册请求的网络策略。
    m_activeRequest = NetworkClient::instance().getAsync(
        QUrl(datasheetUrl), ResourceType::Datasheet, RetryPolicy::fromProfile(profile, m_weakNetworkSupport));

    connect(m_activeRequest, &AsyncNetworkRequest::downloadProgress, this, &JLCDatasheet::downloadProgress);
    connect(m_activeRequest, &AsyncNetworkRequest::finished, this, [this](const NetworkResult& result) {
        AsyncNetworkRequest* finishedRequest = m_activeRequest;  // 已完成请求，用于延迟释放。
        m_activeRequest = nullptr;
        m_isDownloading = false;

        if (result.wasCancelled) {
            if (finishedRequest) {
                finishedRequest->deleteLater();
            }
            return;
        }

        if (!result.success) {
            qWarning() << "Datasheet download error:" << result.error;
            emit downloadError(result.error);
            if (finishedRequest) {
                finishedRequest->deleteLater();
            }
            return;
        }

        QFile file(m_savePath);
        if (!file.open(QIODevice::WriteOnly)) {
            emit downloadError(QString("Failed to save datasheet: %1").arg(file.errorString()));
            if (finishedRequest) {
                finishedRequest->deleteLater();
            }
            return;
        }

        file.write(result.data);
        file.close();

        emit downloadSuccess(m_savePath);

        if (finishedRequest) {
            finishedRequest->deleteLater();
        }
    });
}

/**
 * @brief 取消当前数据手册下载。
 */
void JLCDatasheet::cancel() {
    if (m_activeRequest && !m_activeRequest->isFinished()) {
        m_activeRequest->cancel();
        m_activeRequest = nullptr;
    }
    m_isDownloading = false;
}

}  // namespace EasyKiConverter
