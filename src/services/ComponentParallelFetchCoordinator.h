#ifndef COMPONENTPARALLELFETCHCOORDINATOR_H
#define COMPONENTPARALLELFETCHCOORDINATOR_H

#include "models/ComponentData.h"

#include <QString>

namespace EasyKiConverter {

class ComponentService;

/**
 * @brief 协调 ComponentService 的并行元件请求状态。
 *
 * 该协作者只处理 ParallelFetchContext、ComponentQueueManager 和批量完成信号
 * 之间的状态同步，不改变 ComponentService 对外暴露的请求接口。
 */
class ComponentParallelFetchCoordinator final {
public:
    /**
     * @brief 记录成功结果并释放一个并行请求槽位。
     * @param service 组件服务实例。
     * @param componentId 元器件编号。
     * @param data 组件数据。
     */
    static void handleDataCollected(ComponentService& service, const QString& componentId, const ComponentData& data);

    /**
     * @brief 记录失败结果并释放一个并行请求槽位。
     * @param service 组件服务实例。
     * @param componentId 元器件编号。
     * @param error 错误信息。
     */
    static void handleFetchError(ComponentService& service, const QString& componentId, const QString& error);

    /**
     * @brief 处理批量请求超时并发布已经收集的数据。
     * @param service 组件服务实例。
     */
    static void handleQueueTimeout(ComponentService& service);

    /**
     * @brief 停止队列并清理并行请求上下文。
     * @param service 组件服务实例。
     */
    static void reset(ComponentService& service);
};

}  // namespace EasyKiConverter

#endif  // COMPONENTPARALLELFETCHCOORDINATOR_H
