#pragma once

namespace EasyKiConverter {

class ComponentListViewModel;

/**
 * @brief 协调组件列表视图模型的定时器初始化。
 * @details 统一创建预览图、批处理和列表刷新定时器，保持各类防抖回调的生命周期一致。
 */
class ComponentListTimerCoordinator final {
public:
    /**
     * @brief 创建并连接组件列表所需的全部定时器。
     * @param owner 所属组件列表视图模型。
     */
    static void initialize(ComponentListViewModel& owner);
};

}  // namespace EasyKiConverter
