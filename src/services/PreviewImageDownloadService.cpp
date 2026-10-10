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

/**
 * @brief 创建预览图下载服务。
 * @param owner 所属缓存服务，不由本对象管理其生命周期
 */
PreviewImageDownloadService::PreviewImageDownloadService(ComponentCacheService& owner) : m_owner(owner) {}

/**
 * @brief 执行预览图缓存读取、网络下载和持久化流程。
 * @details 目录迁移与缓存读取共用磁盘锁，避免切换目录时使用不一致的路径。
 * @param componentId 元器件编号
 * @param imageUrl 预览图地址
 * @param imageIndex 预览图序号
 * @param diag 网络诊断信息输出指针，可为空
 * @param cancelled 可选取消标志
 * @param weakNetwork 是否启用弱网络重试策略
 * @param expectedGeneration 请求创建时捕获的缓存代次，零表示使用当前代次
 * @return 有效图片数据，失败或取消时为空
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
    // 固定本次请求允许写入的缓存代次，避免目录切换后旧请求污染新目录。
    const uint64_t generation = expectedGeneration != 0 ? expectedGeneration : m_owner.currentGeneration();

    // 记录从缓存读取或网络请求开始到返回结果的耗时。
    QElapsedTimer timer;
    timer.start();

    // 缓存目录路径、文件检查和读取必须与目录迁移串行化。
    {
        // 串行化缓存读取与目录迁移，确保路径和文件操作处于同一锁保护下。
        QMutexLocker diskLocker(&m_owner.m_diskWriteMutex);
        // 根据元器件编号和预览图序号解析缓存文件路径。
        const QString cachedPath = m_owner.previewImagePath(componentId, imageIndex);
        if (QFileInfo::exists(cachedPath)) {
            // 读取已存在的缓存文件，后续仍需经过图片内容校验。
            QFile file(cachedPath);
            if (file.open(QIODevice::ReadOnly)) {
                // 保存缓存文件的原始内容，供校验和调用方返回。
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
                // 记录无效缓存移入回收站时的诊断信息，不影响后续网络下载。
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

    // 根据网络配置生成预览图请求的重试策略。
    const RetryPolicy policy = RetryPolicy::fromProfile(RequestProfiles::previewImage(), weakNetwork);
    // 执行预览图网络请求并收集响应、取消和诊断结果。
    const NetworkResult result = NetworkClient::instance().get(QUrl(imageUrl), ResourceType::PreviewImage, policy);

    // 保存经过校验、准备写入缓存并返回给调用方的内容。
    QByteArray data;
    // 保存取消、校验或网络失败的可读错误信息。
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
