#ifndef LCSCIMAGESERVICE_H
#define LCSCIMAGESERVICE_H

#include "ComponentCacheService.h"
#include "core/network/AsyncNetworkRequest.h"

#include <QAtomicInt>
#include <QFutureWatcher>
#include <QMap>
#include <QObject>
#include <QPointer>
#include <QSet>
#include <QString>
#include <QStringList>

namespace EasyKiConverter {

/**
 * @brief LCSC 图片服务类
 *
 * 专门负责抓取 LCSC 元件预览图和数据手册
 * 下载后直接写磁盘，不在内存中缓存
 */
class LcscImageService : public QObject {
    Q_OBJECT

public:
    explicit LcscImageService(QObject* parent = nullptr);

    /**
     * @brief 请求获取元件预览图（支持多张）
     *
     * @param componentId 元件 ID
     */
    void fetchPreviewImages(const QString& componentId);

    /**
     * @brief 批量获取多个元件的预览图
     *
     * @param componentIds 元件 ID 列表
     */
    void fetchBatchPreviewImages(const QStringList& componentIds);

    /**
     * @brief 清空所有缓存数据
     *
     * 当清空元器件列表时调用此方法，以便重新添加相同元器件时能够重新请求
     */
    void clearCache();

    /**
     * @brief 取消所有正在进行的预览图获取操作
     *
     * 当用户点击开始导出时调用，以确保取消所有未完成的预览图获取工作
     */
    void cancelAll();

    /**
     * @brief 取消特定元件的预览图获取操作
     *
     * 当删除单个元器件时调用，以中断该元器件的预览图获取工作
     * @param componentId 元件ID
     */
    void cancelRequestForComponent(const QString& componentId);

    /**
     * @brief 下载数据手册
     *
     * 当 ComponentService 接收到 LCSC API 返回的数据手册 URL 后调用此方法触发下载
     *
     * @param componentId 元件 ID
     * @param datasheetUrl 数据手册 URL
     */
    void fetchDatasheet(const QString& componentId, const QString& datasheetUrl);

signals:
    /**
     * @brief 单张图片下载成功信号
     *
     * @param componentId 元件 ID
     * @param imageData 图片数据
     * @param imageIndex 图片索引（0-2）
     */
    void imageReady(const QString& componentId, const QByteArray& imageData, int imageIndex);

    /**
     * @brief 所有图片下载完成信号
     *
     * @param componentId 元件 ID
     * @param imagePaths 图片路径列表
     */
    void allImagesReady(const QString& componentId, const QStringList& imagePaths);

    /**
     * @brief LCSC API 数据获取成功信号（包含数据手册和预览图 URL）
     *
     * @param componentId 元件 ID
     * @param manufacturerPart 制造商部件号
     * @param datasheetUrl 数据手册 URL
     * @param imageUrls 预览图 URL 列表
     */
    void lcscDataReady(const QString& componentId,
                       const QString& manufacturerPart,
                       const QString& datasheetUrl,
                       const QStringList& imageUrls);

    /**
     * @brief 数据手册下载成功信号
     *
     * @param componentId 元件 ID
     * @param datasheetData 数据手册数据（从磁盘读取）
     */
    void datasheetReady(const QString& componentId, const QByteArray& datasheetData);

    /**
     * @brief 错误信号
     * @param componentId 元器件编号。
     * @param errorMessage 输出错误信息的指针。
     */
    void error(const QString& componentId, const QString& errorMessage);

private:
    bool tryLoadCachedPreviewImages(const QString& componentId);
    void loadCachedPreviewImagesAsync(const QString& componentId, ComponentCacheService* cache, quint64 requestToken);
    void startPreviewImageDownloads(const QString& componentId, const QStringList& imageUrls, quint64 requestToken);
    void performApiSearch(const QString& componentId, quint64 requestToken);
    void performDownload(const QString& componentId, const QString& imageUrl, int imageIndex, quint64 requestToken);
    void performDatasheetDownload(const QString& componentId, const QString& datasheetUrl, quint64 requestToken);
    void checkDownloadCompletion(const QString& componentId, quint64 requestToken);
    void emitAllImagesReady(const QString& componentId, quint64 requestToken);
    quint64 beginRequest(const QString& componentId);
    bool isCurrentRequest(const QString& componentId, quint64 requestToken) const;
    quint64 beginDatasheetRequest(const QString& componentId);
    bool isCurrentDatasheetRequest(const QString& componentId, quint64 requestToken) const;
    void trackAsyncRequest(AsyncNetworkRequest* request);
    void untrackAsyncRequest(AsyncNetworkRequest* request);

    QThreadPool* m_cacheThreadPool;  // 缓存加载专用线程池
    QSet<QString> m_requestedComponents;  // 已经请求过的组件（防止重复请求）
    QMap<QString, quint64> m_requestTokens;  // componentId -> 当前请求令牌
    QMap<QString, quint64> m_datasheetTokens;  // componentId -> 当前数据手册请求令牌
    quint64 m_nextRequestToken = 0;  // 单调递增的请求令牌

    // 跟踪下载状态（只存计数，不存实际数据）
    QMap<QString, int> m_downloadCounts;  // componentId -> downloaded count
    QMap<QString, int> m_expectedCounts;  // componentId -> expected count

    // 跟踪异步加载的 watcher，用于非阻塞预览图加载
    QList<QFutureWatcher<QByteArray>*> m_pendingImageWatchers;
    QList<QPointer<AsyncNetworkRequest>> m_activeAsyncRequests;  ///< 活动异步请求列表。

    QAtomicInt m_isCancelled;  // 取消标志，防止 cancelAll() 后 pending 回调继续执行
    static const int MAX_CONCURRENT_REQUESTS = 10;  ///< 缓存加载的最大并发下载数。
    static const int MAX_IMAGES_PER_COMPONENT = 3;  ///< 每个组件允许下载的预览图数量上限。
    static const int MAX_RETRY_COUNT = 3;  ///< 预览图下载失败时的最大重试次数。
};

}  // namespace EasyKiConverter

#endif  // LCSCIMAGESERVICE_H
