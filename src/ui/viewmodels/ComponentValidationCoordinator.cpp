#include "ComponentValidationCoordinator.h"

#include "ComponentListViewModel.h"
#include "services/ConfigService.h"

namespace EasyKiConverter {

/**
 * @brief 将列表中尚未完成验证的元件加入队列并启动并发请求。
 * @param viewModel 组件列表视图模型。
 */
void ComponentValidationCoordinator::start(ComponentListViewModel& viewModel) {
    // 读取配置的验证并发数上限。
    const int concurrentWorkers = ConfigService::instance()->getValidationConcurrentCount();

    // 只调度仍处于验证状态的元件，避免延迟完成回调导致重复请求。
    for (ComponentListItemData* item : viewModel.m_componentList) {
        if (item && item->isFetching() && !item->isValid()) {
            viewModel.m_validationQueue.enqueue(item->componentId());
        }
    }

    if (viewModel.m_validationQueue.isEmpty()) {
        return;
    }

    // 首次启动时记录队列总数，后续追加元件由 ValidationStateManager 单独维护。
    if (viewModel.m_validationTotalCount == 0) {
        viewModel.m_validationTotalCount = viewModel.m_validationQueue.size();
    }

    // 计算本轮首次启动的验证请求数量。
    const int initialCount = qMin(concurrentWorkers, viewModel.m_validationQueue.size());
    for (int i = 0; i < initialCount; ++i) {
        // 取出下一个待验证的元器件编号。
        const QString componentId = viewModel.m_validationQueue.takeNext();
        viewModel.m_service->fetchComponentData(componentId, false);
        ++viewModel.m_validationPendingCount;
    }
}

/**
 * @brief 从待处理队列取出一个元件并发起验证请求。
 * @param viewModel 组件列表视图模型。
 */
void ComponentValidationCoordinator::processNext(ComponentListViewModel& viewModel) {
    if (viewModel.m_validationQueue.isEmpty()) {
        return;
    }

    // 取出队列中下一个待验证的元器件编号。
    const QString componentId = viewModel.m_validationQueue.takeNext();
    viewModel.m_service->fetchComponentData(componentId, false);
    ++viewModel.m_validationPendingCount;
}

/**
 * @brief 更新验证计数并根据 BOM 模式推进队列。
 * @param viewModel 组件列表视图模型。
 * @param componentId 元器件编号。
 */
void ComponentValidationCoordinator::complete(ComponentListViewModel& viewModel, const QString& componentId) {
    ++viewModel.m_validationCompletedCount;
    viewModel.m_validationQueue.complete(componentId);

    if (viewModel.m_validationPendingCount > 0) {
        --viewModel.m_validationPendingCount;
    }

    // BOM 导入完成后不触发额外的列表刷新，只继续消费验证队列。
    if (viewModel.m_bomImportComplete) {
        if (!viewModel.m_validationQueue.isEmpty()) {
            processNext(viewModel);
        } else if (viewModel.m_validationPendingCount == 0) {
            viewModel.m_bomImportComplete = false;
        }
        return;
    }

    // BOM 导入过程中累积更新次数，普通模式下立即安排一次批量刷新。
    if (!viewModel.m_bomImportMode) {
        viewModel.scheduleListUpdate();
    } else {
        ++viewModel.m_bomImportPendingUpdates;
    }

    if (!viewModel.m_validationQueue.isEmpty()) {
        processNext(viewModel);
    } else {
        // 验证期间可能有新元件加入，队列为空时重新扫描列表。
        viewModel.startValidationQueue();
    }
}

}  // namespace EasyKiConverter
