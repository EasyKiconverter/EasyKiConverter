#include "DatasheetDownloadService.h"

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

/** @brief 保存所属缓存服务引用。 */
DatasheetDownloadService::DatasheetDownloadService(ComponentCacheService& owner) : m_owner(owner) {}

/**
 * @brief 执行数据手册缓存读取、网络下载和持久化流程。
 * @details 缓存目录读取与目录迁移共用磁盘锁，避免切换缓存目录时读取到不一致路径。
 */
QByteArray DatasheetDownloadService::download(const QString& componentId,
                                              const QString& datasheetUrl,
                                              QString* format,
                                              ComponentExportStatus::NetworkDiagnostics* diag,
                                              QAtomicInt* cancelled,
                                              bool weakNetwork,
                                              uint64_t expectedGeneration) {
    if (datasheetUrl.isEmpty()) {
        return QByteArray();
    }
    const uint64_t generation = expectedGeneration != 0 ? expectedGeneration : m_owner.currentGeneration();

    QElapsedTimer timer;
    timer.start();

    // 根据 URL 初步判断格式，收到内容后仍会校验并允许降级为 HTML。
    QString extension =
        datasheetUrl.toLower().contains(QStringLiteral(".html")) ? QStringLiteral("html") : QStringLiteral("pdf");
    if (format) {
        *format = extension;
    }

    // 缓存目录路径、文件检查和读取必须与目录迁移串行化。
    {
        QMutexLocker diskLocker(&m_owner.m_diskWriteMutex);
        const QString fullPath = m_owner.resolveDatasheetPath(componentId, extension, false);
        if (QFileInfo::exists(fullPath)) {
            QFile file(fullPath);
            if (file.open(QIODevice::ReadOnly)) {
                const QByteArray cachedData = file.readAll();
                file.close();
                const QString cachedFormat = fullPath.endsWith(QStringLiteral(".pdf"), Qt::CaseInsensitive)
                                                 ? QStringLiteral("pdf")
                                                 : QStringLiteral("html");
                if (CacheDataValidator::isValidDatasheet(cachedData, cachedFormat)) {
                    LOG_DEBUG(LogModule::Core, "Datasheet loaded from disk cache: {}", fullPath);
                    if (diag) {
                        diag->url = datasheetUrl;
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
                    if (format) {
                        *format = cachedFormat;
                    }
                    return cachedData;
                }
                QString trashError;
                CacheSafety::moveToTrash(fullPath, &trashError);
            }
        }
    }

    // 在发起网络请求前检查取消标志，避免不必要的外部请求。
    if (cancelled && cancelled->loadRelaxed()) {
        LOG_DEBUG(LogModule::Core, "Datasheet download cancelled for {} before start", componentId);
        return QByteArray();
    }

    const RetryPolicy policy = RetryPolicy::fromProfile(RequestProfiles::datasheet(), weakNetwork);
    const NetworkResult result = NetworkClient::instance().get(QUrl(datasheetUrl), ResourceType::Datasheet, policy);

    QByteArray data;
    const int statusCode = result.statusCode;
    QString errorString;
    const int retryCount = result.retryCount;
    const bool wasRateLimited = result.diagnostic.wasRateLimited;

    // 取消优先于响应成功状态，防止取消后的响应继续写入缓存。
    if (cancelled && cancelled->loadRelaxed()) {
        errorString = QStringLiteral("Cancelled");
    } else if (result.wasCancelled) {
        errorString = QStringLiteral("Cancelled");
    } else if (result.success) {
        data = result.data;
        if (format && extension == QStringLiteral("pdf") && data.size() >= 5 && !data.startsWith("%PDF-")) {
            extension = QStringLiteral("html");
            *format = extension;
        }
        if (!CacheDataValidator::isValidDatasheet(data, extension)) {
            data.clear();
            errorString = QStringLiteral("Invalid datasheet data");
        }
    } else {
        errorString = result.error;
    }

    // 将网络层诊断信息完整传递给调用方，便于导出报告分析失败原因。
    if (diag) {
        diag->url = datasheetUrl;
        diag->statusCode = statusCode;
        diag->errorString = errorString;
        diag->responseContentType = result.diagnostic.responseContentType;
        diag->retryAfter = result.diagnostic.retryAfter;
        diag->rateLimitRemaining = result.diagnostic.rateLimitRemaining;
        diag->rateLimitReset = result.diagnostic.rateLimitReset;
        diag->responseSummary = result.diagnostic.responseSummary;
        diag->retryCount = retryCount;
        diag->latencyMs = timer.elapsed();
        diag->wasRateLimited = wasRateLimited;
        diag->hasRateLimitHint = result.diagnostic.hasRateLimitHint;
    }

    // 只有有效数据且未被取消时才写入缓存，使用请求创建时的代次阻止旧任务污染新目录。
    if (errorString.isEmpty() && !data.isEmpty()) {
        m_owner.saveDatasheet(componentId, data, extension, generation);
    } else if (!errorString.isEmpty() && errorString != QStringLiteral("Cancelled")) {
        LOG_WARN(LogModule::Core, "Datasheet download failed for {}: {}", componentId, errorString);
    }

    return data;
}

}  // namespace EasyKiConverter
