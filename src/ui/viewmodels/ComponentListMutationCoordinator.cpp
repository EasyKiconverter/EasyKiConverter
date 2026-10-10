#include "ComponentListMutationCoordinator.h"

#include "ComponentListViewModel.h"
#include "ValidationStateManager.h"

#include <QDebug>
#include <QMutexLocker>

namespace EasyKiConverter {

/**
 * @brief 添加单个元件并启动其数据验证。
 * @param owner 元件列表视图模型。
 * @param componentId 元器件编号。
 */
void ComponentListMutationCoordinator::add(ComponentListViewModel& owner, const QString& componentId) {
    owner.clearAttentionHints();
    owner.m_bomImportComplete = false;
    // 保存去除首尾空白后的元器件编号。
    QString trimmedId = componentId.trimmed();

    if (trimmedId.isEmpty()) {
        qWarning() << "Component ID is empty";
        return;
    }

    // 统一转换为大写，支持用户输入小写 c。
    trimmedId = trimmedId.toUpper();

    // 验证元件 ID 格式。
    if (!owner.validateComponentId(trimmedId)) {
        qWarning() << "Invalid component ID format:" << trimmedId;

        // 尝试从文本中智能提取元件编号。
        // 保存从输入文本中提取出的元器件编号。
        const QStringList extractedIds = owner.extractComponentIdFromText(trimmedId);
        if (extractedIds.isEmpty()) {
            qWarning() << "Failed to extract component ID from text:" << trimmedId;
            emit owner.componentAdded(trimmedId, false, "Invalid LCSC component ID format");
            return;
        }

        owner.addComponentsBatch(extractedIds);
        return;
    }

    // 检查元件是否已存在（需要锁保护）。
    {
        QMutexLocker locker(&owner.m_listMutex);
        if (owner.m_componentIdIndex.contains(trimmedId)) {
            qWarning() << "Component already exists:" << trimmedId;
            emit owner.componentAdded(trimmedId, false, "Component already exists");
            return;
        }
    }

    // 创建新元件项，在锁外构造对象以避免扩大共享列表锁的范围。
    // 创建待加入列表的元件项。
    auto* item = new ComponentListItemData(trimmedId, &owner);
    item->setFetching(true);
    item->setValid(false);
    item->setValidationPhase("validating");

    // 保存新元件在列表中的插入位置。
    int insertIndex;
    {
        QMutexLocker locker(&owner.m_listMutex);
        insertIndex = owner.m_componentList.count();
    }

    owner.beginInsertRows(QModelIndex(), insertIndex, insertIndex);
    {
        QMutexLocker locker(&owner.m_listMutex);
        owner.m_componentList.append(item);
        owner.m_componentIdIndex.insert(trimmedId, owner.m_componentList.count() - 1);
    }
    owner.endInsertRows();

    owner.m_service->fetchComponentData(trimmedId, false);
    owner.m_validationStateManager->addValidation(1);
    owner.scheduleListUpdate();
    emit owner.componentAdded(trimmedId, true, "Component added");
}

/**
 * @brief 按列表位置删除元件并同步所有关联状态。
 * @param owner 元件列表视图模型。
 * @param index 待删除的列表索引。
 */
void ComponentListMutationCoordinator::remove(ComponentListViewModel& owner, int index) {
    owner.clearAttentionHints();

    // 保存当前列表项数量。
    int listCount;
    {
        QMutexLocker locker(&owner.m_listMutex);
        listCount = owner.m_componentList.count();
        if (index < 0 || index >= listCount) {
            return;
        }
    }

    // 删除最后一个元件时复用完整清空流程，确保缓存和服务状态同步清理。
    if (listCount == 1) {
        clear(owner);
        return;
    }

    // 保存待删除的列表项。
    ComponentListItemData* item = nullptr;
    // 保存被删除元件的编号。
    QString removedId;
    {
        QMutexLocker locker(&owner.m_listMutex);
        item = owner.m_componentList.takeAt(index);
        removedId = item->componentId();
        owner.m_componentIdIndex.remove(removedId);
        owner.m_validatedComponentIds.removeAll(removedId);
    }

    if (owner.m_service) {
        owner.m_service->cancelRequestForComponent(removedId);
    }

    // 记录被删除元件是否正在验证。
    const bool wasInFlight = owner.m_validationQueue.isInFlight(removedId);
    owner.m_validationQueue.remove(removedId);

    if (item->isFetching()) {
        owner.m_pendingValidationCount--;
        if (wasInFlight) {
            owner.m_validationPendingCount--;
        }
        owner.m_validationStateManager->cancelValidation(1);
    }

    owner.beginRemoveRows(QModelIndex(), index, index);
    delete item;
    owner.endRemoveRows();

    owner.rebuildComponentIdIndex();
    owner.updateHasInvalidComponents();
    owner.scheduleListUpdate();
    emit owner.componentRemoved(removedId);
}

/**
 * @brief 根据编号查找并删除元件。
 * @param owner 元件列表视图模型。
 * @param componentId 元器件编号。
 */
void ComponentListMutationCoordinator::removeById(ComponentListViewModel& owner, const QString& componentId) {
    // 保存待删除元件的列表索引。
    int indexToRemove = -1;
    {
        QMutexLocker locker(&owner.m_listMutex);
        indexToRemove = owner.m_componentIdIndex.indexOf(componentId);
    }
    if (indexToRemove >= 0) {
        remove(owner, indexToRemove);
    }
}

/**
 * @brief 清空列表、缓存、请求队列和验证统计。
 * @param owner 元件列表视图模型。
 */
void ComponentListMutationCoordinator::clear(ComponentListViewModel& owner) {
    owner.clearAttentionHints();
    owner.m_bomImportComplete = false;
    owner.m_listUpdatePending = false;
    owner.m_bomImportMode = false;
    owner.m_bomImportPendingUpdates = 0;
    owner.m_pendingPreviewFetchIds.clear();
    owner.m_previewUpdateBuffer.clear();

    // 保存清理前的列表项数量。
    int listCount;
    {
        QMutexLocker locker(&owner.m_listMutex);
        listCount = owner.m_componentList.count();
    }

    if (listCount <= 0) {
        return;
    }

    if (owner.m_service) {
        // 先取消所有进行中的请求，防止异步响应访问已删除的列表项。
        owner.m_service->cancelAllPendingRequests();
        owner.m_service->clearCache();
    }

    owner.beginResetModel();
    {
        QMutexLocker locker(&owner.m_listMutex);
        qDeleteAll(owner.m_componentList);
        owner.m_componentList.clear();
        owner.m_componentIdIndex.clear();
    }
    owner.endResetModel();

    owner.m_validationStateManager->reset();
    owner.recomputeStateCounters();
    owner.updateHasInvalidComponents();
    owner.scheduleListUpdate();
    emit owner.listCleared();
}

}  // namespace EasyKiConverter
