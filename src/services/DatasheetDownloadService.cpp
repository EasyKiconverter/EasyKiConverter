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

/**
 * @brief 创建数据手册下载服务。
 * @param owner 所属缓存服务，不由本对象管理其生命周期
 */
DatasheetDownloadService::DatasheetDownloadService(ComponentCacheService& owner) : m_owner(owner) {}

/**
 * @brief 执行数据手册缓存读取、网络下载和持久化流程。
 * @details 缓存目录读取与目录迁移共用磁盘锁，避免切换缓存目录时读取到不一致路径。
 * @param componentId 元器件编号
 * @param datasheetUrl 数据手册 URL
 * @param format 输出格式指针，可为空
 * @param diag 网络诊断信息输出指针，可为空
 * @param cancelled 可选取消标志
 * @param weakNetwork 是否启用弱网络重试策略
 * @param expectedGeneration 请求创建时捕获的缓存代次，零表示使用当前代次
 * @return 有效数据手册内容，失败或取消时为空
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
    // 固定本次请求允许写入的缓存代次，避免目录切换后旧请求污染新目录。
    const uint64_t generation = expectedGeneration != 0 ? expectedGeneration : m_owner.currentGeneration();

    // 记录从缓存读取或网络请求开始到返回结果的耗时。
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
        // 串行化缓存读取与目录迁移，确保路径和文件操作处于同一锁保护下。
        QMutexLocker diskLocker(&m_owner.m_diskWriteMutex);
        // 根据元器件编号和候选格式解析数据手册缓存文件路径。
        const QString fullPath = m_owner.resolveDatasheetPath(componentId, extension, false);
        if (QFileInfo::exists(fullPath)) {
            // 读取已存在的缓存文件，后续仍需经过内容校验。
            QFile file(fullPath);
            if (file.open(QIODevice::ReadOnly)) {
                // 保存缓存文件的原始内容，供格式校验和调用方返回。
                const QByteArray cachedData = file.readAll();
                file.close();
                // 根据缓存文件后缀确定实际数据手册格式。
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
                // 记录无效缓存移入回收站时的诊断信息，不影响后续网络下载。
                QString trashError;
                if (CacheSafety::isOwnedComponentFile(m_owner.cacheDir(), fullPath))
                    CacheSafety::moveToTrash(fullPath, &trashError);
            }
        }
    }

    // 在发起网络请求前检查取消标志，避免不必要的外部请求。
    if (cancelled && cancelled->loadRelaxed()) {
        LOG_DEBUG(LogModule::Core, "Datasheet download cancelled for {} before start", componentId);
        return QByteArray();
    }

    // 根据网络配置生成数据手册请求的重试策略。
    const RetryPolicy policy = RetryPolicy::fromProfile(RequestProfiles::datasheet(), weakNetwork);
    // 执行数据手册网络请求并收集响应、取消和诊断结果。
    const NetworkResult result = NetworkClient::instance().get(QUrl(datasheetUrl), ResourceType::Datasheet, policy);

    // 保存经过校验、准备写入缓存并返回给调用方的内容。
    QByteArray data;
    // 保存网络层返回的 HTTP 状态码。
    const int statusCode = result.statusCode;
    // 保存取消、校验或网络失败的可读错误信息。
    QString errorString;
    // 保存网络层实际使用的重试次数。
    const int retryCount = result.retryCount;
    // 保存网络层是否触发限流，以便回写诊断信息。
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
