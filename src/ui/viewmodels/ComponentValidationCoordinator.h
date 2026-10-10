#ifndef COMPONENTVALIDATIONCOORDINATOR_H
#define COMPONENTVALIDATIONCOORDINATOR_H

#include <QString>

namespace EasyKiConverter {

class ComponentListViewModel;

/**
 * @brief 协调组件列表的并发验证队列。
 *
 * 该协作者只负责向 ComponentService 调度验证请求、推进队列和处理完成状态，
 * BOM 导入模式的 UI 更新策略仍由 ComponentListViewModel 保留。
 */
class ComponentValidationCoordinator final {
public:
    /**
     * @brief 启动当前列表中尚未验证的元件请求。
     * @param viewModel 列表视图模型。
     */
    static void start(ComponentListViewModel& viewModel);

    /**
     * @brief 从待验证队列中调度下一个元件请求。
     * @param viewModel 列表视图模型。
     */
    static void processNext(ComponentListViewModel& viewModel);

    /**
     * @brief 完成单个元件验证并推进后续请求。
     * @param viewModel 列表视图模型。
     * @param componentId 元器件编号。
     */
    static void complete(ComponentListViewModel& viewModel, const QString& componentId);
};

}  // namespace EasyKiConverter

#endif  // COMPONENTVALIDATIONCOORDINATOR_H
