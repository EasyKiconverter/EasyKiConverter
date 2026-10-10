#pragma once

#include <QJsonObject>
#include <QString>

namespace EasyKiConverter {

class ComponentService;

/**
 * @brief 协调 EasyEDA API 返回的元器件回调。
 * @details 负责基础信息响应和请求错误的代次校验、状态清理与信号转发。
 */
class ComponentApiCallbackCoordinator final {
public:
    /**
     * @brief 处理元器件基础信息响应。
     * @param owner 所属元器件服务。
     * @param componentId 元器件编号。
     * @param data API 响应数据。
     */
    static void handleComponentInfoFetched(ComponentService& owner,
                                           const QString& componentId,
                                           const QJsonObject& data);

    /**
     * @brief 处理未携带元器件编号的请求错误。
     * @param owner 所属元器件服务。
     * @param errorMessage 错误信息。
     */
    static void handleFetchError(ComponentService& owner, const QString& errorMessage);

    /**
     * @brief 处理携带元器件编号或模型 UUID 的请求错误。
     * @param owner 所属元器件服务。
     * @param idOrUuid 元器件编号或模型 UUID。
     * @param error 错误信息。
     */
    static void handleFetchErrorWithId(ComponentService& owner, const QString& idOrUuid, const QString& error);

    /**
     * @brief 清理失败请求状态并发送错误信号。
     * @param owner 所属元器件服务。
     * @param componentId 元器件编号。
     * @param error 错误信息。
     * @param expectedGeneration 调用方捕获的缓存代次。
     */
    static void emitFetchErrorAndClearState(ComponentService& owner,
                                            const QString& componentId,
                                            const QString& error,
                                            uint64_t expectedGeneration = 0);
};

}  // namespace EasyKiConverter
