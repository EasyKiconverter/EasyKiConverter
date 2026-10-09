#include "ComponentRequestCancellationCoordinator.h"

#include "ComponentService.h"
#include "core/easyeda/EasyedaApi.h"
#include "core/network/NetworkClient.h"

#include <QDebug>
#include <QMutexLocker>

namespace EasyKiConverter {

/**
 * @brief 取消全部网络、媒体请求并清理正在获取的元器件状态。
 * @param service 持有请求状态和媒体服务的组件服务。
 */
void ComponentRequestCancellationCoordinator::cancelAll(ComponentService& service) {
    qDebug() << "ComponentService: Cancelling all pending component data requests";

    // 先取消当前 API 请求，再清理全局网络队列，避免新任务继续占用请求配额。
    if (service.m_api) {
        service.m_api->cancelRequest();
    }
    NetworkClient::instance().cancelAllRequests();

    // 清空正在获取的组件记录，防止迟到响应更新已经取消的数据。
    {
        QMutexLocker locker(&service.m_fetchingComponentsMutex);
        service.m_fetchingComponents.clear();
    }

    // 图片服务拥有独立的下载队列，需要显式取消其全部任务。
    if (service.m_imageService) {
        service.m_imageService->cancelAll();
    }

    qDebug() << "ComponentService: All pending requests cancelled";
}

/**
 * @brief 清理指定元器件状态并取消对应 API 与媒体请求。
 * @param service 持有请求状态和媒体服务的组件服务。
 * @param componentId 待取消的元器件编号。
 */
void ComponentRequestCancellationCoordinator::cancelForComponent(ComponentService& service,
                                                                 const QString& componentId) {
    const QString normalizedId = componentId.toUpper();  // 规范化后的元器件编号。
    qDebug() << "ComponentService: Cancelling request for component" << normalizedId;

    // 先移除状态，避免取消操作期间到达的响应重新写入已删除的请求。
    {
        QMutexLocker locker(&service.m_fetchingComponentsMutex);
        service.m_fetchingComponents.remove(normalizedId);
    }

    if (service.m_api) {
        service.m_api->cancelRequestForId(normalizedId);
    }
    if (service.m_imageService) {
        service.m_imageService->cancelRequestForComponent(normalizedId);
    }

    qDebug() << "ComponentService: Request cancelled for component" << normalizedId;
}

}  // namespace EasyKiConverter
