#include "ExportProgressAggregator.h"

#include "ExportWorkerHelpers.h"

namespace EasyKiConverter {

/**
 * @brief 合并同一元器件已有的导出状态和新增状态。
 * @param progress 导出类型进度对象。
 * @param componentId 元器件编号。
 * @param status 新的导出状态。
 */
void ExportProgressAggregator::mergeItemStatus(ExportTypeProgress& progress,
                                               const QString& componentId,
                                               const ExportItemStatus& status) {
    ExportItemStatus mergedStatus = status;  ///< 合并后的元器件导出状态。
    const auto previousStatus = progress.itemStatus.constFind(componentId);  ///< 已记录的元器件状态迭代器。
    if (previousStatus != progress.itemStatus.cend()) {
        for (const QString& diagnostic : previousStatus->diagnostics) {  ///< 当前待合并的诊断信息。
            if (!mergedStatus.diagnostics.contains(diagnostic)) {
                mergedStatus.diagnostics.append(diagnostic);
            }
        }
    }
    progress.itemStatus[componentId] = mergedStatus;
    ExportWorkerHelpers::recomputeTypeProgressCounts(progress);
}

/**
 * @brief 完成导出类型的总量初始化并重新计算计数。
 * @param progress 导出类型进度对象。
 * @param typeName 导出类型名称。
 * @param totalCount 该类型待处理的元器件数量。
 */
void ExportProgressAggregator::finalizeTypeProgress(ExportTypeProgress& progress,
                                                    const QString& typeName,
                                                    int totalCount) {
    progress.typeName = typeName;
    progress.totalCount = totalCount;
    progress.inProgressCount = 0;
    ExportWorkerHelpers::recomputeTypeProgressCounts(progress);
}

/**
 * @brief 汇总已经结束的元器件成功和失败数量。
 * @param componentIds 待统计的元器件编号列表。
 * @param typeProgress 各导出类型的进度对象。
 * @return 元器件完成数量汇总。
 */
ExportCompletionTotals ExportProgressAggregator::countCompletedComponents(
    const QStringList& componentIds,
    const QMap<QString, ExportTypeProgress>& typeProgress) {
    ExportCompletionTotals totals;  ///< 汇总后的完成、成功和失败数量。
    for (const QString& componentId : componentIds) {  ///< 当前统计的元器件编号。
        bool anyFailed = false;  ///< 当前元器件是否至少有一个阶段失败。
        bool allDone = true;  ///< 当前元器件的所有阶段是否已结束。

        for (auto it = typeProgress.cbegin(); it != typeProgress.cend(); ++it) {  ///< 当前导出类型。
            const ExportItemStatus itemStatus =
                it.value().itemStatus.value(componentId, ExportItemStatus{ExportItemStatus::Status::Pending});
            if (itemStatus.status == ExportItemStatus::Status::Failed) {
                anyFailed = true;
            }
            if (itemStatus.status != ExportItemStatus::Status::Success &&
                itemStatus.status != ExportItemStatus::Status::Failed &&
                itemStatus.status != ExportItemStatus::Status::Skipped) {
                allDone = false;
            }
        }

        if (!allDone) {
            continue;
        }
        if (anyFailed) {
            totals.failedCount++;
        } else {
            totals.successCount++;
        }
    }
    return totals;
}

}  // namespace EasyKiConverter
