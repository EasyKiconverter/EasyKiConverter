#include "ExportProgressRetryCoordinator.h"

#include "ExportProgressViewModel.h"

#include <QDebug>

namespace EasyKiConverter {

/** @brief 保存导出进度视图模型的协作者引用并执行失败项重试。 */

/** @brief 重试指定组件的失败导出。 */
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

    int index = owner.m_idToIndexMap[componentId];
    QVariantMap result = owner.m_resultsList[index].toMap();

    owner.resetItemForRetry(result);
    owner.m_resultsList[index] = result;

    // Update counts and notify UI
    owner.markResultsDirty();
    owner.flushPendingUpdates();

    // Re-trigger export if not currently exporting
    if (!owner.m_isExporting && owner.m_exportService) {
        QStringList idsToRetry = {componentId};
        ExportOptions opts = owner.m_exportService->options();
        opts.retryMode = true;
        owner.m_exportService->setOptions(opts);
        owner.beginExportRun(idsToRetry, QStringLiteral("Preloading component data..."));
        owner.m_exportService->startPreload(idsToRetry);
    }
}

/** @brief 重试所有失败组件。 */
void ExportProgressRetryCoordinator::retryFailedComponents(ExportProgressViewModel& owner) {
    qDebug() << "Retry requested for all failed components";

    if (!owner.m_retrySupported) {
        qWarning() << "Retry is not supported for the selected target format";
        return;
    }

    // Collect all failed component IDs
    QStringList failedIds;
    for (const auto& item : owner.m_resultsList) {
        QVariantMap map = item.toMap();
        if (map.value("status") == "failed") {
            QString id = map.value("componentId").toString();
            failedIds.append(id);

            // Reset status to pending
            if (owner.m_idToIndexMap.contains(id)) {
                int index = owner.m_idToIndexMap[id];
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
        ExportOptions opts = owner.m_exportService->options();
        opts.retryMode = true;
        owner.m_exportService->setOptions(opts);
        owner.beginExportRun(failedIds, QStringLiteral("Preloading component data..."));
        owner.m_exportService->startPreload(failedIds);
    }
}

}  // namespace EasyKiConverter
