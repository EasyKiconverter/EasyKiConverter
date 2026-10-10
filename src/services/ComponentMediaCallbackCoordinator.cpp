#include "ComponentMediaCallbackCoordinator.h"

#include "ComponentCacheService.h"
#include "ComponentQueueManager.h"
#include "ComponentService.h"
#include "PreviewImageDataEncoder.h"

#include <QDebug>
#include <QFutureWatcher>
#include <QImage>
#include <QMutexLocker>
#include <QtConcurrent>

namespace EasyKiConverter {

/**
 * @brief 合并单张预览图数据并转发图像和原始数据事件。
 * @param owner 发起请求并接收回调信号的组件服务。
 * @param componentId 元器件编号。
 * @param imageData 下载得到的原始图像数据。
 * @param imageIndex 图像在预览图列表中的索引。
 */
void ComponentMediaCallbackCoordinator::handleImageReady(ComponentService& owner,
                                                         const QString& componentId,
                                                         const QByteArray& imageData,
                                                         int imageIndex) {
    // 将原始字节解码为 Qt 图像，供界面预览和缓存事件同时使用。
    QImage image = QImage::fromData(imageData);
    if (image.isNull()) {
        qWarning() << "Failed to load image from data for component:" << componentId << "index:" << imageIndex
                   << "data size:" << imageData.size();
        return;
    }

    // 使用大写编号统一访问正在获取的元器件状态。
    const QString normalizedId = componentId.toUpper();
    // 保存合并后的组件快照，供缓存替换使用。
    ComponentData updatedData;
    // 标记本次回调是否命中了有效的活动请求。
    bool hasValidUpdate = false;
    // 记录请求创建时的缓存代次，避免旧回调覆盖新缓存。
    uint64_t gen = 0;
    {
        QMutexLocker locker(&owner.m_fetchingComponentsMutex);
        // 在锁保护下查找当前元器件的活动请求。
        const auto it = owner.m_fetchingComponents.find(normalizedId);
        if (it == owner.m_fetchingComponents.end() ||
            it->cacheGeneration != ComponentCacheService::instance()->currentGeneration()) {
            qDebug() << "ComponentService: Discarding stale image callback for" << componentId;
            return;
        }
        it->data.addPreviewImageData(imageData, imageIndex);
        updatedData = it->data;
        gen = it->cacheGeneration;
        hasValidUpdate = true;
    }

    if (hasValidUpdate) {
        emit owner.previewImageReady(normalizedId, image, imageIndex);
        emit owner.previewImageDataReady(normalizedId, imageData, imageIndex);
        if (ComponentCacheService::instance()->currentGeneration() != gen)
            return;
        owner.m_componentCache.replaceIfPresent(normalizedId, updatedData);
    }
}

/**
 * @brief 合并 LCSC 元数据并异步持久化最新组件快照。
 * @param owner 发起请求并接收回调信号的组件服务。
 * @param componentId 元器件编号。
 * @param manufacturerPart 厂商料号。
 * @param datasheetUrl 数据手册地址。
 * @param imageUrls 预览图地址列表。
 */
void ComponentMediaCallbackCoordinator::handleLcscDataReady(ComponentService& owner,
                                                            const QString& componentId,
                                                            const QString& manufacturerPart,
                                                            const QString& datasheetUrl,
                                                            const QStringList& imageUrls) {
    qDebug() << "LCSC data ready for component:" << componentId
             << "Manufacturer Part:" << (manufacturerPart.isEmpty() ? "none" : manufacturerPart)
             << "Datasheet:" << (datasheetUrl.isEmpty() ? "none" : datasheetUrl) << "Images:" << imageUrls.size();

    // 保存合并后的组件快照，供异步元数据持久化使用。
    ComponentData updatedData;
    // 标记是否找到可更新的活动请求。
    bool hasValidUpdate = false;
    // 记录请求创建时的缓存代次。
    uint64_t gen = 0;
    {
        QMutexLocker locker(&owner.m_fetchingComponentsMutex);
        // 使用大写编号查找活动请求，兼容不同大小写来源。
        const QString normalizedId = componentId.toUpper();
        if (owner.m_fetchingComponents.contains(normalizedId)) {
            // 获取待合并的活动组件状态。
            ComponentService::FetchingComponent& fetchingComponent = owner.m_fetchingComponents[normalizedId];
            if (fetchingComponent.cacheGeneration != ComponentCacheService::instance()->currentGeneration()) {
                qDebug() << "ComponentService: Discarding stale LCSC data callback for" << componentId;
                return;
            }

            if (!manufacturerPart.isEmpty()) {
                fetchingComponent.data.setManufacturerPart(manufacturerPart);
                qDebug() << "Manufacturer part saved to ComponentData:" << manufacturerPart;
            }

            if (!datasheetUrl.isEmpty()) {
                fetchingComponent.data.setDatasheet(datasheetUrl);
                // 根据数据手册地址推断其缓存格式。
                QString format =
                    datasheetUrl.toLower().contains(".html") ? QStringLiteral("html") : QStringLiteral("pdf");
                fetchingComponent.data.setDatasheetFormat(format);
                qDebug() << "Datasheet saved to ComponentData:" << datasheetUrl << "format:" << format;
            }

            if (!imageUrls.isEmpty()) {
                fetchingComponent.data.setPreviewImages(imageUrls);
                qDebug() << "Preview images saved to ComponentData:" << imageUrls.size() << "images";
                // 为新预览图地址预分配与地址数量一致的图像数据槽位。
                QList<QByteArray> emptyImageDataList;
                emptyImageDataList.resize(imageUrls.size());
                fetchingComponent.data.setPreviewImageData(emptyImageDataList);
                qDebug() << "Pre-allocated" << imageUrls.size() << "empty image data slots";
            }

            updatedData = fetchingComponent.data;
            gen = fetchingComponent.cacheGeneration;
            hasValidUpdate = true;
        } else {
            qWarning() << "Component" << componentId << "not found in m_fetchingComponents, cannot update LCSC data";
        }
    }

    if (hasValidUpdate) {
        if (ComponentCacheService::instance()->currentGeneration() != gen) {
            qDebug() << "ComponentService: Discarding stale LCSC data update for" << componentId;
            return;
        }
        // 使用大写编号转发事件并更新缓存。
        const QString normalizedId = componentId.toUpper();
        emit owner.lcscDataUpdated(normalizedId, manufacturerPart, datasheetUrl, imageUrls);
        ComponentCacheService::instance()->saveComponentMetadataAsync(normalizedId, updatedData, gen);
        owner.updateComponentCache(normalizedId, updatedData);
    }
}

/**
 * @brief 合并数据手册、推进异步下载完成状态并转发结果。
 * @param owner 发起请求并接收回调信号的组件服务。
 * @param componentId 元器件编号。
 * @param datasheetData 下载得到的数据手册内容。
 */
void ComponentMediaCallbackCoordinator::handleDatasheetReady(ComponentService& owner,
                                                             const QString& componentId,
                                                             const QByteArray& datasheetData) {
    qDebug() << "Datasheet downloaded for component:" << componentId << "size:" << datasheetData.size() << "bytes";

    // 记录数据手册格式，并在 PDF 内容无效时切换到 HTML。
    QString format;
    // 标记当前回调是否命中了活动请求。
    bool hasValidUpdate = false;
    // 标记所有并行下载是否已完成。
    bool shouldMarkCompleted = false;
    // 保存完成时的组件快照，供并行获取上下文消费。
    ComponentData completedData;
    // 记录活动请求的缓存代次。
    uint64_t gen = 0;
    // 使用大写编号访问活动请求和并行获取上下文。
    const QString normalizedId = componentId.toUpper();
    {
        QMutexLocker locker(&owner.m_fetchingComponentsMutex);
        if (owner.m_fetchingComponents.contains(normalizedId)) {
            // 获取待更新的数据手册所属组件状态。
            ComponentService::FetchingComponent& fetchingComponent = owner.m_fetchingComponents[normalizedId];
            gen = fetchingComponent.cacheGeneration;
            if (gen != ComponentCacheService::instance()->currentGeneration()) {
                qDebug() << "ComponentService: Discarding stale datasheet callback for" << componentId;
                return;
            }
            fetchingComponent.data.setDatasheetData(datasheetData);
            format = fetchingComponent.data.datasheetFormat();
            if (format == "pdf" && !owner.isPDF(datasheetData)) {
                format = "html";
                fetchingComponent.data.setDatasheetFormat(format);
            }
            qDebug() << "Datasheet data saved to ComponentData, size:" << datasheetData.size() << "bytes"
                     << "format:" << format;
            if (fetchingComponent.pendingAsyncDownloads > 0) {
                fetchingComponent.pendingAsyncDownloads--;
                qDebug() << "Datasheet download complete, pending async downloads:"
                         << fetchingComponent.pendingAsyncDownloads;
                if (fetchingComponent.pendingAsyncDownloads == 0) {
                    shouldMarkCompleted = true;
                    completedData = fetchingComponent.data;
                }
            }
            hasValidUpdate = true;
        } else {
            qWarning() << "Component" << componentId
                       << "not found in m_fetchingComponents, datasheet data will be saved directly to cache";
        }
    }

    if (shouldMarkCompleted) {
        if (ComponentCacheService::instance()->currentGeneration() != gen) {
            qDebug() << "ComponentService: Discarding stale datasheet completion for" << componentId;
            return;
        }
        // 读取当前并行获取上下文，避免在无锁状态下访问共享指针。
        ParallelFetchContext* parallelContext = nullptr;
        {
            QMutexLocker locker(&owner.m_parallelContextMutex);
            parallelContext = owner.m_parallelContext;
        }
        if (parallelContext != nullptr)
            parallelContext->markCompleted(normalizedId, completedData);
        if (owner.m_queueManager != nullptr)
            owner.m_queueManager->requestCompleted(normalizedId);
    }

    if (hasValidUpdate) {
        // 复制带有最新数据手册的组件状态，避免锁外访问共享对象。
        ComponentData dataCopy;
        // 标记是否成功复制了仍属于当前代次的组件状态。
        bool hasDataCopy = false;
        {
            QMutexLocker fetchLocker(&owner.m_fetchingComponentsMutex);
            if (owner.m_fetchingComponents.contains(normalizedId) &&
                owner.m_fetchingComponents[normalizedId].cacheGeneration == gen) {
                dataCopy = owner.m_fetchingComponents[normalizedId].data;
                hasDataCopy = true;
            }
        }
        if (hasDataCopy && owner.m_componentCache.replaceIfPresent(normalizedId, dataCopy))
            qDebug() << "ComponentService: Updated cache with datasheet data for" << normalizedId;
    } else {
        qDebug() << "ComponentService: Discarding datasheet callback without active request for" << componentId;
    }

    if (hasValidUpdate)
        emit owner.datasheetReady(componentId, datasheetData);
}

/**
 * @brief 校验预览图错误回调代次并转发可用性错误。
 * @param owner 发起请求并接收回调信号的组件服务。
 * @param componentId 元器件编号。
 * @param error 下载失败原因。
 */
void ComponentMediaCallbackCoordinator::handlePreviewImageError(ComponentService& owner,
                                                                const QString& componentId,
                                                                const QString& error) {
    // 使用大写编号查找活动请求并转发错误信号。
    const QString normalizedId = componentId.toUpper();
    {
        QMutexLocker locker(&owner.m_fetchingComponentsMutex);
        // 在锁保护下确认错误仍属于当前活动请求。
        const auto it = owner.m_fetchingComponents.find(normalizedId);
        if (it == owner.m_fetchingComponents.end() ||
            it->cacheGeneration != ComponentCacheService::instance()->currentGeneration()) {
            qDebug() << "ComponentService: Discarding stale preview error for" << componentId;
            return;
        }
    }

    if (error == QLatin1String("Image not found") || error == QLatin1String("Preview image URL not found") ||
        error == QLatin1String("No images downloaded") || error == QLatin1String("No preview image URLs available"))
        qDebug() << "Preview image unavailable for component:" << componentId << "error:" << error;
    else
        qWarning() << "Preview image fetch error for component:" << componentId << "error:" << error;

    emit owner.previewImageFailed(normalizedId, error);
}

/**
 * @brief 后台编码全部预览图并合并结果。
 * @param owner 发起请求并接收回调信号的组件服务。
 * @param componentId 元器件编号。
 * @param imagePaths 已下载的预览图路径列表。
 */
void ComponentMediaCallbackCoordinator::handleAllImagesReady(ComponentService& owner,
                                                             const QString& componentId,
                                                             const QStringList& imagePaths) {
    qDebug() << "All images ready for component:" << componentId << "paths:" << imagePaths.size();
    // 记录活动请求的缓存代次，防止异步编码结果写回旧请求。
    uint64_t gen = 0;
    {
        QMutexLocker locker(&owner.m_fetchingComponentsMutex);
        // 在锁保护下确认元器件请求仍处于活动状态。
        const auto it = owner.m_fetchingComponents.find(componentId.toUpper());
        if (it == owner.m_fetchingComponents.end()) {
            qDebug() << "ComponentService: Discarding all-images callback without active request for" << componentId;
            return;
        }
        gen = it->cacheGeneration;
    }

    // 在线程池中编码所有预览图文件，避免阻塞组件服务线程。
    QFuture<PreviewImageDataResult> future =
        QtConcurrent::run([imagePaths]() { return PreviewImageDataEncoder::encodeFiles(imagePaths); });
    // 监听后台编码完成事件，并在组件服务线程中合并结果。
    auto* watcher = new QFutureWatcher<PreviewImageDataResult>(&owner);
    QObject::connect(watcher,
                     &QFutureWatcher<PreviewImageDataResult>::finished,
                     &owner,
                     [&owner, watcher, componentId, imagePaths, gen]() {
                         // 读取后台编码结果并释放一次性监听器。
                         const PreviewImageDataResult imageResult = watcher->result();
                         watcher->deleteLater();
                         if (ComponentCacheService::instance()->currentGeneration() != gen) {
                             qDebug() << "ComponentService: Discarding stale all-images result for" << componentId;
                             return;
                         }
                         // 使用大写编号查找仍然有效的活动请求。
                         const QString normalizedId = componentId.toUpper();
                         {
                             QMutexLocker locker(&owner.m_fetchingComponentsMutex);
                             // 在锁保护下确认请求代次没有被替换。
                             const auto it = owner.m_fetchingComponents.find(normalizedId);
                             if (it == owner.m_fetchingComponents.end() || it->cacheGeneration != gen) {
                                 qDebug() << "ComponentService: Discarding all-images result for replaced request"
                                          << componentId;
                                 return;
                             }
                             it->data.setPreviewImageData(imageResult.imageData);
                             qDebug() << "All image data updated in ComponentData for component:" << componentId
                                      << "count:" << imageResult.imageData.size();
                         }
                         emit owner.previewImagesReady(componentId, imageResult.encodedImages);
                         emit owner.allImagesReady(componentId, imagePaths);
                     });
    watcher->setFuture(future);
}

}  // namespace EasyKiConverter
