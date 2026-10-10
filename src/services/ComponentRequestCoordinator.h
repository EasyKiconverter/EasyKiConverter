#ifndef COMPONENTREQUESTCOORDINATOR_H
#define COMPONENTREQUESTCOORDINATOR_H

#include <QString>

namespace EasyKiConverter {

class ComponentService;

/**
 * @brief 协调单个元件请求的去重、缓存命中和 CAD 后台获取。
 *
 * 该协调器只负责请求启动阶段；网络回调、缓存解析结果和批量状态仍由
 * ComponentService 的其他协调器处理，避免把请求入口继续堆积在服务类中。
 */
class ComponentRequestCoordinator final {
public:
    /**
     * @brief 启动一个规范化后的元件请求。
     * @param service 所属元件服务。
     * @param componentId 原始元件编号。
     * @param fetch3DModel 是否同时获取三维模型。
     */
    static void start(ComponentService& service, const QString& componentId, bool fetch3DModel);
};

}  // namespace EasyKiConverter

#endif  // COMPONENTREQUESTCOORDINATOR_H
