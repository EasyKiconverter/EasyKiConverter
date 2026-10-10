#ifndef COMPONENTCADFETCHCOORDINATOR_H
#define COMPONENTCADFETCHCOORDINATOR_H

#include "CadDataLoader.h"

#include <QJsonObject>
#include <QString>

#include <cstdint>

namespace EasyKiConverter {

class ComponentService;

/**
 * @brief 协调 CAD 响应的异步解析、代次校验和结果提交。
 *
 * 该协作者保持 ComponentService 的网络回调入口不变，只集中处理 CAD 数据
 * 从响应到缓存、信号和并行批处理状态的转换。
 */
class ComponentCadFetchCoordinator final {
public:
    /**
     * @brief 启动 CAD 响应的后台解析任务。
     * @param service 组件服务实例。
     * @param componentId 元器件编号。
     * @param data CAD 响应数据。
     */
    static void handleDataFetched(ComponentService& service, const QString& componentId, const QJsonObject& data);

    /**
     * @brief 校验代次并提交 CAD 解析结果。
     * @param service 组件服务实例。
     * @param result CAD 解析结果。
     * @param expectedGeneration 期望的缓存代次。
     */
    static void handleFetchResult(ComponentService& service,
                                  const CadFetchTaskResult& result,
                                  uint64_t expectedGeneration);
};

}  // namespace EasyKiConverter

#endif  // COMPONENTCADFETCHCOORDINATOR_H
