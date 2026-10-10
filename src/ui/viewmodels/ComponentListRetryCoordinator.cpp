#include "ComponentListRetryCoordinator.h"

#include "ComponentListViewModel.h"
#include "ValidationStateManager.h"

#include <QMutexLocker>

namespace EasyKiConverter {

/**
 * @brief 收集可重试项并重新发起组件数据请求。
 * @param owner 所属组件列表视图模型。
 */
void ComponentListRetryCoordinator::retryAllInvalid(ComponentListViewModel& owner) {
    owner.clearAttentionHints();

    // 先收集需要重试的组件 ID，避免在持有列表锁时触发网络请求。
    QStringList idsToRetry;  // 可安全重新请求的元器件编号。
    {
        QMutexLocker locker(&owner.m_listMutex);
        for (const auto* item : owner.m_componentList) {
            if (item && !item->isValid() && !item->isFetching() && item->retryable()) {
                idsToRetry.append(item->componentId());
            }
        }
    }

    // 在锁外重置列表项状态并执行重试操作。
    for (const QString& id : idsToRetry) {
        auto* item = owner.findItemData(id);  // 待重试的列表项。
        if (item) {
            owner.m_bomImportComplete = false;
            item->setFetching(true);
            item->setValid(false);
            item->setRetryable(true);
            item->setValidationPhase("validating");
            item->setErrorMessage("");
            owner.m_service->fetchComponentData(id, false);
        }
    }

    if (!idsToRetry.isEmpty()) {
        owner.m_validationStateManager->startValidation(idsToRetry.count());
    }

    emit owner.filteredCountChanged();
}

}  // namespace EasyKiConverter
