#pragma once

#include "ExportProgress.h"

#include <QMap>
#include <QStringList>

namespace EasyKiConverter {

/**
 * @brief 汇总一次导出运行的最终元件结果。
 */
struct ExportCompletionTotals {
    int successCount{0};  ///< 所有导出类型均成功的元件数量。
    int failedCount{0};  ///< 至少一个导出类型失败的元件数量。
};

/**
 * @brief 集中处理导出阶段的状态合并和最终统计。
 *
 * 该类不持有 QObject 或线程状态，只负责维护 ExportTypeProgress 中的逐项
 * 状态与计数之间的一致性，服务层可以继续负责信号和生命周期管理。
 */
class ExportProgressAggregator {
public:
    /**
     * @brief 合并一个元件的阶段状态并重新计算计数。
     * @param progress 需要更新的阶段进度
     * @param componentId 元件编号
     * @param status 新到达的状态
     */
    static void mergeItemStatus(ExportTypeProgress& progress,
                                const QString& componentId,
                                const ExportItemStatus& status);

    /**
     * @brief 结束阶段进度并根据逐项状态重新计算统计。
     * @param progress 需要结束的阶段进度
     * @param typeName 阶段名称
     * @param totalCount 本次导出的元件总数
     */
    static void finalizeTypeProgress(ExportTypeProgress& progress, const QString& typeName, int totalCount);

    /**
     * @brief 根据所有阶段的逐项状态统计最终成功和失败元件数。
     * @param componentIds 本次导出的元件编号
     * @param typeProgress 各导出类型的进度
     * @return 最终成功和失败数量
     */
    static ExportCompletionTotals countCompletedComponents(const QStringList& componentIds,
                                                           const QMap<QString, ExportTypeProgress>& typeProgress);
};

}  // namespace EasyKiConverter
