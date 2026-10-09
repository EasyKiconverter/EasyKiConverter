#include "MediaFetchWorker.h"

#include "services/CacheRepository.h"
#include "services/ComponentCacheService.h"
#include "services/ConfigService.h"

#include <QDebug>

namespace EasyKiConverter {

MediaFetchWorker::MediaFetchWorker(const QString& componentId,
                                   const QStringList& previewImageUrls,
                                   const QString& datasheetUrl,
                                   QObject* parent)
    : QObject(parent)
    , m_componentId(componentId)
    , m_previewImageUrls(previewImageUrls)
    , m_datasheetUrl(datasheetUrl)
    , m_isAborted(0) {}

/**
 * @brief 销毁媒体获取工作对象。
 */
MediaFetchWorker::~MediaFetchWorker() = default;

/**
 * @brief 初始化并启动预览图与数据手册获取流程。
 */
void MediaFetchWorker::run() {
    if (m_isAborted.loadAcquire()) {
        return;
    }

    m_previewImageDataList.clear();
    m_datasheetData.clear();
    m_diagnostics.clear();
    m_pendingOperations = 0;

    ComponentCacheService* cache = ComponentCacheService::instance();  // 当前磁盘缓存服务。
    if (!cache) {
        qWarning() << "MediaFetchWorker: ComponentCacheService is null";
        emit fetchCompleted(m_componentId, m_previewImageDataList, m_datasheetData, m_diagnostics);
        return;
    }

    // 如果预览图为空，直接开始下载手册
    if (m_previewImageUrls.isEmpty()) {
        startDatasheetDownload(cache);
        return;
    }

    // 开始第一个预览图下载
    startPreviewDownload(0, cache);
}

/**
 * @brief 获取指定索引的预览图并继续处理后续媒体。
 * @param index 当前预览图索引。
 * @param cache 用于读取本地缓存的缓存服务。
 */
void MediaFetchWorker::startPreviewDownload(int index, ComponentCacheService* cache) {
    if (m_isAborted.loadAcquire()) {
        finishAllDownloads();
        return;
    }

    if (index >= m_previewImageUrls.size()) {
        // 所有预览图下载完成，开始下载手册
        startDatasheetDownload(cache);
        return;
    }

    m_pendingOperations++;
    const QString& url = m_previewImageUrls[index];  // 当前预览图的远程地址。

    if (url.isEmpty()) {
        // URL为空，尝试直接从缓存加载
        QElapsedTimer timer;  // 统计本地缓存读取耗时的计时器。
        timer.start();
        QByteArray imageData = cache->loadPreviewImage(m_componentId, index);  // 从缓存读取的预览图数据。

        ComponentExportStatus::NetworkDiagnostics diag;  // 本次缓存读取的诊断信息。
        diag.url = QString();
        diag.latencyMs = timer.elapsed();
        if (!imageData.isEmpty()) {
            diag.statusCode = 200;
            diag.errorString = "";
            diag.wasRateLimited = false;
            m_previewImageDataList.append(imageData);
        } else {
            diag.statusCode = 0;
            diag.errorString = "Not found in cache";
            diag.wasRateLimited = false;
        }
        diag.retryCount = 0;
        m_diagnostics.append(diag);

        m_pendingOperations--;
        // 继续下一个预览图
        startPreviewDownload(index + 1, cache);
        return;
    }

    // 使用异步下载
    CacheRepository::instance()->fetchPreviewImageAsync(
        m_componentId,
        url,
        index,
        &m_isAborted,
        ConfigService::instance()->getWeakNetworkSupport(),
        [this, index, cache](const QByteArray& imageData, const ComponentExportStatus::NetworkDiagnostics& diag) {
            if (m_isAborted.loadRelaxed()) {
                m_pendingOperations--;
                if (m_pendingOperations <= 0) {
                    finishAllDownloads();
                }
                return;
            }

            if (!imageData.isEmpty()) {
                m_previewImageDataList.append(imageData);
            }
            // 创建诊断信息的副本
            ComponentExportStatus::NetworkDiagnostics diagCopy = diag;  // 异步回调使用的诊断信息副本。
            diagCopy.latencyMs = diag.latencyMs;
            m_diagnostics.append(diagCopy);

            m_pendingOperations--;
            // 继续下一个预览图
            startPreviewDownload(index + 1, cache);
        });
}

/**
 * @brief 获取数据手册并在媒体流程完成后发出结果。
 * @param cache 用于读取本地缓存的缓存服务。
 */
void MediaFetchWorker::startDatasheetDownload(ComponentCacheService* cache) {
    if (m_isAborted.loadAcquire()) {
        finishAllDownloads();
        return;
    }

    if (m_datasheetUrl.isEmpty()) {
        // URL为空，尝试直接从缓存加载
        QElapsedTimer timer;  // 统计本地数据手册读取耗时的计时器。
        timer.start();
        m_datasheetData = cache->loadDatasheet(m_componentId);

        ComponentExportStatus::NetworkDiagnostics diag;  // 本次缓存读取的诊断信息。
        diag.url = QString();
        diag.latencyMs = timer.elapsed();
        if (!m_datasheetData.isEmpty()) {
            diag.statusCode = 200;
            diag.errorString = "";
            diag.wasRateLimited = false;
        } else {
            diag.statusCode = 0;
            diag.errorString = "Not found in cache";
            diag.wasRateLimited = false;
        }
        diag.retryCount = 0;
        m_diagnostics.append(diag);

        finishAllDownloads();
        return;
    }

    // 使用异步下载
    m_pendingOperations++;
    CacheRepository::instance()->fetchDatasheetAsync(
        m_componentId,
        m_datasheetUrl,
        &m_isAborted,
        ConfigService::instance()->getWeakNetworkSupport(),
        [this](const QByteArray& data, const ComponentExportStatus::NetworkDiagnostics& diag) {
            if (m_isAborted.loadRelaxed()) {
                m_pendingOperations--;
                if (m_pendingOperations <= 0) {
                    finishAllDownloads();
                }
                return;
            }

            m_datasheetData = data;
            ComponentExportStatus::NetworkDiagnostics diagCopy = diag;  // 异步回调使用的诊断信息副本。
            diagCopy.latencyMs = diag.latencyMs;
            m_diagnostics.append(diagCopy);

            m_pendingOperations--;
            finishAllDownloads();
        });
}

/**
 * @brief 在所有媒体操作完成后发出完成信号。
 */
void MediaFetchWorker::finishAllDownloads() {
    // 检查是否所有操作都已完成
    if (m_pendingOperations > 0) {
        return;
    }

    // 发送完成信号（包含诊断信息）
    emit fetchCompleted(m_componentId, m_previewImageDataList, m_datasheetData, m_diagnostics);
}

/**
 * @brief 请求终止当前媒体获取流程。
 */
void MediaFetchWorker::abort() {
    // 使用原子操作确保线程安全
    m_isAborted.storeRelease(1);
}

}  // namespace EasyKiConverter
