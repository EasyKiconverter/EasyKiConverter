#pragma once

namespace EasyKiConverter {

class ComponentListViewModel;

/**
 * @brief 协调组件列表视图模型与服务层的信号连接。
 * @details 统一连接验证结果、组件数据和预览图回调，保持 ViewModel 生命周期内的连接一致性。
 */
class ComponentListServiceConnectionCoordinator final {
public:
    /**
     * @brief 创建组件列表所需的全部服务信号连接。
     * @param owner 所属组件列表视图模型。
     */
    static void initialize(ComponentListViewModel& owner);
};

}  // namespace EasyKiConverter
