#ifndef CACHEREPOSITORY_H
#define CACHEREPOSITORY_H

#include "ComponentCacheService.h"

#include <QAtomicInt>
#include <QFile>
#include <QFileInfo>

#include <functional>

namespace EasyKiConverter {

class CacheRepository {
public:
    using MediaFetchCallback =
        std::function<void(const QByteArray&,
                           const ComponentExportStatus::NetworkDiagnostics&)>;  ///< 媒体下载完成回调。

    static CacheRepository* instance() {
        static CacheRepository repository;  // 进程级缓存仓库实例。
        return &repository;
    }

    /**
     * @brief 异步获取预览图并维护缓存。
     * @param lcscId LCSC 元件编号。
     * @param imageUrl 预览图 URL。
     * @param imageIndex 预览图索引。
     * @param cancelled 可选取消标志。
     * @param weakNetwork 是否使用弱网策略。
     * @param onComplete 完成回调。
     */
    void fetchPreviewImageAsync(const QString& lcscId,
                                const QString& imageUrl,
                                int imageIndex,
                                QAtomicInt* cancelled,
                                bool weakNetwork,
                                const MediaFetchCallback& onComplete) {
        if (imageIndex < 0 || imageIndex >= 3) {
            ComponentExportStatus::NetworkDiagnostics diag;  // 参数校验失败诊断。
            diag.url = imageUrl;
            diag.errorString = QStringLiteral("Invalid preview image index");
            onComplete(QByteArray(), diag);
            return;
        }

        ComponentCacheService* cache = ComponentCacheService::instance();  // 缓存服务实例。
        if (!cache) {
            ComponentExportStatus::NetworkDiagnostics diag;  // 缓存服务不可用诊断。
            diag.url = imageUrl;
            diag.errorString = QStringLiteral("Cache service unavailable");
            onComplete(QByteArray(), diag);
            return;
        }

        if (cancelled && cancelled->loadRelaxed()) {
            ComponentExportStatus::NetworkDiagnostics diag;  // 启动前取消诊断。
            diag.url = imageUrl;
            diag.errorString = QStringLiteral("Cancelled before start");
            onComplete(QByteArray(), diag);
            return;
        }

        const QByteArray cachedImage = cache->loadPreviewImage(lcscId, imageIndex);  // 已缓存预览图数据。
        if (!cachedImage.isEmpty()) {
            ComponentExportStatus::NetworkDiagnostics diag;  // 缓存命中诊断。
            diag.url = imageUrl;
            diag.statusCode = 200;
            diag.retryCount = 0;
            diag.wasRateLimited = false;
            onComplete(cachedImage, diag);
            return;
        }

        const RetryPolicy policy =
            RetryPolicy::fromProfile(RequestProfiles::previewImage(), weakNetwork);  // 下载重试策略。
        const uint64_t gen = cache->currentGeneration();  // 当前缓存代次。
        AsyncNetworkRequest* request =  // 异步网络请求对象。
            NetworkClient::instance().getAsync(QUrl(imageUrl), ResourceType::PreviewImage, policy);

        QObject::connect(request, &AsyncNetworkRequest::finished, request, [=](const NetworkResult& result) {
            ComponentExportStatus::NetworkDiagnostics diag;  // 网络响应诊断。
            diag.url = imageUrl;
            diag.statusCode = result.statusCode;
            diag.retryCount = result.retryCount;
            diag.wasRateLimited = result.diagnostic.wasRateLimited;
            diag.responseContentType = result.diagnostic.responseContentType;
            diag.retryAfter = result.diagnostic.retryAfter;
            diag.rateLimitRemaining = result.diagnostic.rateLimitRemaining;
            diag.rateLimitReset = result.diagnostic.rateLimitReset;
            diag.responseSummary = result.diagnostic.responseSummary;
            diag.hasRateLimitHint = result.diagnostic.hasRateLimitHint;
            diag.latencyMs = result.elapsedMs;

            if ((cancelled && cancelled->loadRelaxed()) || result.wasCancelled) {
                diag.errorString = QStringLiteral("Cancelled");
                onComplete(QByteArray(), diag);
            } else if (result.success) {
                if (ComponentCacheService::isValidPreviewImageData(result.data)) {
                    cache->savePreviewImage(lcscId, result.data, imageIndex, gen);
                    onComplete(result.data, diag);
                } else {
                    diag.errorString = QStringLiteral("Invalid preview image data");
                    onComplete(QByteArray(), diag);
                }
            } else {
                diag.errorString = result.error;
                onComplete(QByteArray(), diag);
            }

            request->deleteLater();
        });
    }

    /**
     * @brief 异步获取数据手册并维护缓存。
     * @param lcscId LCSC 元件编号。
     * @param datasheetUrl 数据手册 URL。
     * @param cancelled 可选取消标志。
     * @param weakNetwork 是否使用弱网策略。
     * @param onComplete 完成回调。
     */
    void fetchDatasheetAsync(const QString& lcscId,
                             const QString& datasheetUrl,
                             QAtomicInt* cancelled,
                             bool weakNetwork,
                             const MediaFetchCallback& onComplete) {
        ComponentCacheService* cache = ComponentCacheService::instance();  // 缓存服务实例。
        if (!cache) {
            ComponentExportStatus::NetworkDiagnostics diag;  // 缓存服务不可用诊断。
            diag.url = datasheetUrl;
            diag.errorString = QStringLiteral("Cache service unavailable");
            onComplete(QByteArray(), diag);
            return;
        }

        if (cancelled && cancelled->loadRelaxed()) {
            ComponentExportStatus::NetworkDiagnostics diag;  // 启动前取消诊断。
            diag.url = datasheetUrl;
            diag.errorString = QStringLiteral("Cancelled before start");
            onComplete(QByteArray(), diag);
            return;
        }

        QString ext =  // 根据 URL 推导的数据手册扩展名。
            datasheetUrl.toLower().contains(QStringLiteral(".html")) ? QStringLiteral("html") : QStringLiteral("pdf");
        const QByteArray cachedDatasheet = cache->loadDatasheet(lcscId);  // 已缓存数据手册。
        if (!cachedDatasheet.isEmpty()) {
            ComponentExportStatus::NetworkDiagnostics diag;  // 缓存命中诊断。
            diag.url = datasheetUrl;
            diag.statusCode = 200;
            diag.retryCount = 0;
            diag.wasRateLimited = false;
            onComplete(cachedDatasheet, diag);
            return;
        }

        const RetryPolicy policy =
            RetryPolicy::fromProfile(RequestProfiles::datasheet(), weakNetwork);  // 下载重试策略。
        const uint64_t gen = cache->currentGeneration();  // 当前缓存代次。
        AsyncNetworkRequest* request =
            NetworkClient::instance().getAsync(QUrl(datasheetUrl), ResourceType::Datasheet, policy);

        QObject::connect(request, &AsyncNetworkRequest::finished, request, [=](const NetworkResult& result) mutable {
            ComponentExportStatus::NetworkDiagnostics diag;  // 网络响应诊断。
            diag.url = datasheetUrl;
            diag.statusCode = result.statusCode;
            diag.retryCount = result.retryCount;
            diag.wasRateLimited = result.diagnostic.wasRateLimited;
            diag.responseContentType = result.diagnostic.responseContentType;
            diag.retryAfter = result.diagnostic.retryAfter;
            diag.rateLimitRemaining = result.diagnostic.rateLimitRemaining;
            diag.rateLimitReset = result.diagnostic.rateLimitReset;
            diag.responseSummary = result.diagnostic.responseSummary;
            diag.hasRateLimitHint = result.diagnostic.hasRateLimitHint;
            diag.latencyMs = result.elapsedMs;

            if ((cancelled && cancelled->loadRelaxed()) || result.wasCancelled) {
                diag.errorString = QStringLiteral("Cancelled");
                onComplete(QByteArray(), diag);
            } else if (result.success) {
                if (ext == QStringLiteral("pdf") && result.data.size() >= 5 && !result.data.startsWith("%PDF-")) {
                    ext = QStringLiteral("html");
                }
                if (ComponentCacheService::isValidDatasheetData(result.data, ext)) {
                    cache->saveDatasheet(lcscId, result.data, ext, gen);
                    onComplete(result.data, diag);
                } else {
                    diag.errorString = QStringLiteral("Invalid datasheet data");
                    onComplete(QByteArray(), diag);
                }
            } else {
                diag.errorString = result.error;
                onComplete(QByteArray(), diag);
            }

            request->deleteLater();
        });
    }

private:
    CacheRepository() = default;
};

}  // namespace EasyKiConverter

#endif  // CACHEREPOSITORY_H
