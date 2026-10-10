#include "ExportProgressRetryCoordinator.h"

#include "ExportProgressViewModel.h"

#include <QDebug>

namespace EasyKiConverter {

/**
 * @brief 重试指定组件的失败导出。
 * @param owner 导出进度视图模型。
 * @param componentId 元器件编号。
 */
void ExportProgressRetryCoordinator::retryComponent(ExportProgressViewModel& owner, const QString& componentId) {
    qDebug() << "Retry requested for component:" << componentId;

    if (!owner.m_retrySupported) {
        qWarning() << "Retry is not supported for the selected target format";
        return;
    }

    if (!owner.m_idToIndexMap.contains(componentId)) {
        qWarning() << "Component not found in results:" << componentId;
        return;
    }

    // 获取失败元件在结果列表中的索引。
    int index = owner.m_idToIndexMap[componentId];
    // 保存待重试元件的结果数据。
    QVariantMap result = owner.m_resultsList[index].toMap();

    owner.resetItemForRetry(result);
    owner.m_resultsList[index] = result;

    // Update counts and notify UI
    owner.markResultsDirty();
    owner.flushPendingUpdates();

    // Re-trigger export if not currently exporting
    if (!owner.m_isExporting && owner.m_exportService) {
        // 保存需要重新预加载的元器件编号。
        QStringList idsToRetry = {componentId};
        // 复制当前导出选项并启用重试模式。
        ExportOptions opts = owner.m_exportService->options();
        opts.retryMode = true;
        owner.m_exportService->setOptions(opts);
        owner.beginExportRun(idsToRetry, QStringLiteral("Preloading component data..."));
        owner.m_exportService->startPreload(idsToRetry);
    }
}

/**
 * @brief 重试所有失败组件。
 * @param owner 导出进度视图模型。
 */
void ExportProgressRetryCoordinator::retryFailedComponents(ExportProgressViewModel& owner) {
    qDebug() << "Retry requested for all failed components";

    if (!owner.m_retrySupported) {
        qWarning() << "Retry is not supported for the selected target format";
        return;
    }

    // Collect all failed component IDs
    // 保存所有失败元器件的编号。
    QStringList failedIds;
    for (const auto& item : owner.m_resultsList) {
        // 读取单个导出结果。
        QVariantMap map = item.toMap();
        if (map.value("status") == "failed") {
            // 获取失败结果对应的元器件编号。
            QString id = map.value("componentId").toString();
            failedIds.append(id);

            // Reset status to pending
            if (owner.m_idToIndexMap.contains(id)) {
                // 获取失败元件在结果列表中的索引。
                int index = owner.m_idToIndexMap[id];
                // 保存待重试元件的结果数据。
                QVariantMap result = owner.m_resultsList[index].toMap();
                owner.resetItemForRetry(result);
                owner.m_resultsList[index] = result;
            }
        }
    }

    if (failedIds.isEmpty()) {
        qDebug() << "No failed components to retry";
        return;
    }

    qDebug() << "Retrying" << failedIds.size() << "failed components";

    // Update counts and notify UI
    owner.markResultsDirty();
    owner.flushPendingUpdates();

    // Re-trigger export if not currently exporting
    if (!owner.m_isExporting && owner.m_exportService) {
        // 复制当前导出选项并启用重试模式。
        ExportOptions opts = owner.m_exportService->options();
        opts.retryMode = true;
        owner.m_exportService->setOptions(opts);
        owner.beginExportRun(failedIds, QStringLiteral("Preloading component data..."));
        owner.m_exportService->startPreload(failedIds);
    }
}

}  // namespace EasyKiConverter
