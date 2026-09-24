#include "PreviewImageDownloadService.h"

#include "CacheDataValidator.h"
#include "CacheSafety.h"
#include "ComponentCacheService.h"
#include "core/network/NetworkClient.h"
#include "utils/logging/LogMacros.h"

#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QMutexLocker>
#include <QUrl>

namespace EasyKiConverter {

/** @brief 保存缓存服务引用。 */
PreviewImageDownloadService::PreviewImageDownloadService(ComponentCacheService& owner) : m_owner(owner) {}

/**
 * @brief 执行预览图缓存读取、网络下载和持久化流程。
 * @details 目录迁移与缓存读取共用磁盘锁，避免切换目录时使用不一致的路径。
 */
QByteArray PreviewImageDownloadService::download(const QString& componentId,
                                                 const QString& imageUrl,
                                                 int imageIndex,
                                                 ComponentExportStatus::NetworkDiagnostics* diag,
                                                 QAtomicInt* cancelled,
                                                 bool weakNetwork,
                                                 uint64_t expectedGeneration) {
    if (imageUrl.isEmpty() || imageIndex < 0 || imageIndex >= 3) {
        return QByteArray();
    }
    const uint64_t generation = expectedGeneration != 0 ? expectedGeneration : m_owner.currentGeneration();

    QElapsedTimer timer;
    timer.start();

    // 缓存目录路径、文件检查和读取必须与目录迁移串行化。
    {
        QMutexLocker diskLocker(&m_owner.m_diskWriteMutex);
        const QString cachedPath = m_owner.previewImagePath(componentId, imageIndex);
        if (QFileInfo::exists(cachedPath)) {
            QFile file(cachedPath);
            if (file.open(QIODevice::ReadOnly)) {
                const QByteArray data = file.readAll();
                file.close();
                if (CacheDataValidator::isValidPreviewImage(data)) {
                    LOG_DEBUG(LogModule::Core, "Preview image loaded from disk cache: {}", cachedPath);
                    if (diag) {
                        diag->url = imageUrl;
                        diag->statusCode = 200;
                        diag->errorString.clear();
                        diag->responseContentType.clear();
                        diag->retryAfter.clear();
                        diag->rateLimitRemaining.clear();
                        diag->rateLimitReset.clear();
                        diag->responseSummary.clear();
                        diag->retryCount = 0;
                        diag->latencyMs = timer.elapsed();
                        diag->wasRateLimited = false;
                        diag->hasRateLimitHint = false;
                    }
                    return data;
                }
                QString trashError;
                if (CacheSafety::isOwnedComponentFile(m_owner.cacheDir(), cachedPath))
                    CacheSafety::moveToTrash(cachedPath, &trashError);
            }
        }
    }

    // 在发起网络请求前检查取消标志，避免取消后仍访问外部服务。
    if (cancelled && cancelled->loadRelaxed()) {
        LOG_DEBUG(LogModule::Core, "Preview image download cancelled for {} before start", componentId);
        return QByteArray();
    }

    const RetryPolicy policy = RetryPolicy::fromProfile(RequestProfiles::previewImage(), weakNetwork);
    const NetworkResult result = NetworkClient::instance().get(QUrl(imageUrl), ResourceType::PreviewImage, policy);

    QByteArray data;
    QString errorString;
    if (cancelled && cancelled->loadRelaxed()) {
        errorString = QStringLiteral("Cancelled");
    } else if (result.wasCancelled) {
        errorString = QStringLiteral("Cancelled");
    } else if (result.success) {
        data = result.data;
        if (!CacheDataValidator::isValidPreviewImage(data)) {
            data.clear();
            errorString = QStringLiteral("Invalid preview image data");
        }
    } else {
        errorString = result.error;
    }

    // 将网络层诊断信息完整传递给调用方，便于导出报告分析失败原因。
    if (diag) {
        diag->url = imageUrl;
        diag->statusCode = result.statusCode;
        diag->errorString = errorString;
        diag->responseContentType = result.diagnostic.responseContentType;
        diag->retryAfter = result.diagnostic.retryAfter;
        diag->rateLimitRemaining = result.diagnostic.rateLimitRemaining;
        diag->rateLimitReset = result.diagnostic.rateLimitReset;
        diag->responseSummary = result.diagnostic.responseSummary;
        diag->retryCount = result.retryCount;
        diag->latencyMs = timer.elapsed();
        diag->wasRateLimited = result.diagnostic.wasRateLimited;
        diag->hasRateLimitHint = result.diagnostic.hasRateLimitHint;
    }

    if (errorString.isEmpty() && !data.isEmpty()) {
        m_owner.savePreviewImage(componentId, data, imageIndex, generation);
    } else if (!errorString.isEmpty() && errorString != QStringLiteral("Cancelled")) {
        LOG_WARN(LogModule::Core, "Preview image download failed for {}: {}", componentId, errorString);
    }
    return data;
}

}  // namespace EasyKiConverter
