#include "ComponentApiCallbackCoordinator.h"

#include "ComponentCacheService.h"
#include "ComponentParallelFetchCoordinator.h"
#include "ComponentService.h"
#include "services/ComponentInfoParser.h"

#include <QDebug>
#include <QMutexLocker>

namespace EasyKiConverter {

/**
 * @brief 校验响应代次并发布元器件基础信息。
 * @param owner 所属元器件服务。
 * @param componentId 元器件编号。
 * @param data API 响应数据。
 */
void ComponentApiCallbackCoordinator::handleComponentInfoFetched(ComponentService& owner,
                                                                 const QString& componentId,
                                                                 const QJsonObject& data) {
    const QString normalizedId = componentId.toUpper();  // 规范化后的元器件编号。
    {
        QMutexLocker locker(&owner.m_fetchingComponentsMutex);
        const auto it = owner.m_fetchingComponents.find(normalizedId);  // 当前请求状态。
        if (it == owner.m_fetchingComponents.end() ||
            it->cacheGeneration != ComponentCacheService::instance()->currentGeneration()) {
            qDebug() << "ComponentService: Discarding stale component info callback for" << componentId;
            return;
        }
    }

    const ComponentData componentData = ComponentInfoParser::parse(normalizedId, data);  // 解析后的基础信息。
    emit owner.componentInfoReady(normalizedId, componentData);
}

/**
 * @brief 使用当前组件编号清理未标识的 API 错误。
 * @param owner 所属元器件服务。
 * @param errorMessage 错误信息。
 */
void ComponentApiCallbackCoordinator::handleFetchError(ComponentService& owner, const QString& errorMessage) {
    emitFetchErrorAndClearState(owner, owner.m_currentComponentId, errorMessage);
}

/**
 * @brief 将模型 UUID 解析为组件编号后清理 API 错误。
 * @param owner 所属元器件服务。
 * @param idOrUuid 元器件编号或模型 UUID。
 * @param error 错误信息。
 */
void ComponentApiCallbackCoordinator::handleFetchErrorWithId(ComponentService& owner,
                                                             const QString& idOrUuid,
                                                             const QString& error) {
    QString componentId = idOrUuid;  // 当前错误对应的元器件编号。

    // 并行模式下，CAD 请求错误可能只携带模型 UUID，需要从活动状态反查组件编号。
    if (owner.m_parallelContext != nullptr) {
        QMutexLocker locker(&owner.m_fetchingComponentsMutex);
        for (auto it = owner.m_fetchingComponents.begin(); it != owner.m_fetchingComponents.end();
             ++it) {  // 反查活动请求。
            if (it.value().data.model3DData() && it.value().data.model3DData()->uuid() == idOrUuid) {
                componentId = it.key();
                qDebug() << "Resolved UUID" << idOrUuid << "to component ID" << componentId;
                break;
            }
        }
    }

    emitFetchErrorAndClearState(owner, componentId, error);
}

/**
 * @brief 按请求代次清理状态、更新缓存并转发失败事件。
 * @param owner 所属元器件服务。
 * @param componentId 元器件编号。
 * @param error 错误信息。
 * @param expectedGeneration 调用方捕获的缓存代次。
 */
void ComponentApiCallbackCoordinator::emitFetchErrorAndClearState(ComponentService& owner,
                                                                  const QString& componentId,
                                                                  const QString& error,
                                                                  uint64_t expectedGeneration) {
    qWarning() << "Fetch error for component" << componentId << ":" << error;

    {
        QMutexLocker locker(&owner.m_fetchingComponentsMutex);
        const auto it = owner.m_fetchingComponents.find(componentId.toUpper());  // 当前请求状态。
        if (it == owner.m_fetchingComponents.end() ||
            (expectedGeneration != 0 && it->cacheGeneration != expectedGeneration)) {
            if (expectedGeneration != 0) {
                qDebug() << "ComponentService: Discarding stale error for" << componentId;
            }
            return;
        }
        owner.m_fetchingComponents.erase(it);
    }

    owner.m_componentCache.removeIfInvalid(componentId.toUpper());
    ComponentParallelFetchCoordinator::handleFetchError(owner, componentId, error);

    // fetchError 必须最后发送，因为连接的槽函数可能会删除服务对象。
    emit owner.fetchError(componentId, error);
}

}  // namespace EasyKiConverter
