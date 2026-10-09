#include "ComponentListTimerCoordinator.h"

#include "ComponentListViewModel.h"

#include <QDebug>
#include <QPointer>
#include <QTimer>

namespace EasyKiConverter {

/**
 * @brief 创建并连接预览图、批处理和列表刷新定时器。
 * @param owner 所属组件列表视图模型。
 */
void ComponentListTimerCoordinator::initialize(ComponentListViewModel& owner) {
    // 缓存预览图使用固定时间窗批量更新，保证图片可以渐进显示。
    owner.m_cachePreviewImageTimer = new QTimer(&owner);
    owner.m_cachePreviewImageTimer->setSingleShot(true);
    owner.m_cachePreviewImageTimer->setInterval(120);
    QObject::connect(
        owner.m_cachePreviewImageTimer, &QTimer::timeout, &owner, &ComponentListViewModel::processCachePreviewImages);

    // 普通添加模式和 BOM 导入模式共用添加批处理定时器。
    owner.m_batchAddTimer = new QTimer(&owner);
    owner.m_batchAddTimer->setSingleShot(false);
    owner.m_batchAddTimer->setInterval(50);
    QObject::connect(owner.m_batchAddTimer, &QTimer::timeout, &owner, &ComponentListViewModel::processNextBatchAdd);

    // 聚合列表项属性通知，避免每个异步基础信息响应都刷新界面。
    owner.m_batchUpdateTimer = new QTimer(&owner);
    owner.m_batchUpdateTimer->setSingleShot(true);
    owner.m_batchUpdateTimer->setInterval(100);
    QObject::connect(owner.m_batchUpdateTimer, &QTimer::timeout, &owner, [&owner]() {
        const QList<QPointer<ComponentListItemData>> items =
            owner.m_batchUpdateItems.take();  // 待发送属性变更通知的列表项。
        for (const QPointer<ComponentListItemData>& item : items) {
            if (item) {
                emit item->dataChanged();
            }
        }
    });

    // 列表统计更新使用较短批处理窗口，减少批量操作时的界面抖动。
    owner.m_batchListUpdateTimer = new QTimer(&owner);
    owner.m_batchListUpdateTimer->setSingleShot(true);
    owner.m_batchListUpdateTimer->setInterval(150);
    QObject::connect(owner.m_batchListUpdateTimer, &QTimer::timeout, &owner, [&owner]() {
        owner.m_batchListUpdateMode = false;
        owner.m_listUpdatePending = false;
        owner.recomputeStateCounters();
        owner.updateHasInvalidComponents();
        emit owner.componentCountChanged();
        emit owner.filteredCountChanged();
    });

    // BOM 导入模式使用更长的窗口，集中处理累积的验证完成通知。
    owner.m_bomImportUpdateTimer = new QTimer(&owner);
    owner.m_bomImportUpdateTimer->setSingleShot(true);
    owner.m_bomImportUpdateTimer->setInterval(300);
    QObject::connect(owner.m_bomImportUpdateTimer, &QTimer::timeout, &owner, [&owner]() {
        owner.m_bomImportMode = false;
        owner.m_listUpdatePending = false;
        if (owner.m_bomImportPendingUpdates > 0) {
            owner.m_bomImportPendingUpdates = 0;
            owner.scheduleListUpdate();
        }
    });

    // 延迟获取预览图，给验证完成信号和列表状态更新留出时间。
    owner.m_delayedFetchPreviewTimer = new QTimer(&owner);
    owner.m_delayedFetchPreviewTimer->setSingleShot(true);
    owner.m_delayedFetchPreviewTimer->setInterval(100);
    QObject::connect(
        owner.m_delayedFetchPreviewTimer, &QTimer::timeout, &owner, &ComponentListViewModel::delayedFetchPreviewImages);
}

}  // namespace EasyKiConverter
