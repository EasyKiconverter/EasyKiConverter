#ifndef COMPONENTLISTRETRYCOORDINATOR_H
#define COMPONENTLISTRETRYCOORDINATOR_H

namespace EasyKiConverter {

class ComponentListViewModel;

/**
 * @brief 协调元件列表中可重试失败项的重新请求。
 *
 * 该类只负责筛选可重试项、重置其验证状态和启动服务请求，不改变视图模型对外接口。
 */
class ComponentListRetryCoordinator final {
public:
    /**
     * @brief 重新请求所有当前未处理且允许重试的失败元件。
     * @param owner 所属组件列表视图模型。
     */
    static void retryAllInvalid(ComponentListViewModel& owner);
};

}  // namespace EasyKiConverter

#endif  // COMPONENTLISTRETRYCOORDINATOR_H
