#ifndef COMPONENTREQUESTCANCELLATIONCOORDINATOR_H
#define COMPONENTREQUESTCANCELLATIONCOORDINATOR_H

#include <QString>

namespace EasyKiConverter {

class ComponentService;

/**
 * @brief 协调元器件请求的全量取消和单器件取消。
 *
 * 该类集中维护 API、网络客户端、图片服务和正在获取状态之间的取消顺序，
 * ComponentService 仍保留原有的公开取消接口。
 */
class ComponentRequestCancellationCoordinator final {
public:
    /**
     * @brief 取消所有未完成的元器件和媒体请求。
     * @param service 组件服务实例。
     */
    static void cancelAll(ComponentService& service);

    /**
     * @brief 取消指定元器件的请求并清理其状态。
     * @param service 组件服务实例。
     * @param componentId 元器件编号。
     */
    static void cancelForComponent(ComponentService& service, const QString& componentId);
};

}  // namespace EasyKiConverter

#endif  // COMPONENTREQUESTCANCELLATIONCOORDINATOR_H
