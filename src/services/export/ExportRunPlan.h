#pragma once

#include "ExportProgress.h"

#include <QMap>
#include <QSharedPointer>
#include <QStringList>

namespace EasyKiConverter {

class ComponentData;

/**
 * @brief 描述一次导出运行所需的阶段和输入数据。
 *
 * 该对象只保存计划计算结果，不持有线程、QObject 或文件资源，便于在启动
 * Stage 前验证目标格式、缓存完整性和进度统计范围。
 */
struct ExportRunPlan {
    /** @brief 是否启用符号导出阶段。 */
    bool enableSymbol{false};
    /** @brief 是否启用封装导出阶段。 */
    bool enableFootprint{false};
    /** @brief 组合库目标是否正在执行仅符号导出。 */
    bool symbolOnlyCombinedLibrary{false};
    /** @brief 是否启用三维模型统计项。 */
    bool enableModel3D{false};
    /** @brief 是否启动独立的三维模型导出阶段。 */
    bool runExternalModel3DStage{false};
    /** @brief 是否启用预览图导出阶段。 */
    bool enablePreview{false};
    /** @brief 是否启用数据手册导出阶段。 */
    bool enableDatasheet{false};
    /** @brief 通过预加载完整性检查、可以交给 Stage 的元件编号。 */
    QStringList exportableComponentIds;
    /** @brief 缺少完整预加载数据、需要标记失败的元件编号。 */
    QStringList missingDataComponentIds;

    /** @brief 返回实际需要启动的外部 Stage 数量。 */
    int runningStageCount() const;

    /** @brief 返回需要初始化进度表的导出类型名称。 */
    QStringList progressTypeNames() const;
};

/**
 * @brief 根据导出选项和缓存数据计算导出运行计划。
 * @param options 当前导出选项
 * @param componentIds 用户选择的元件编号
 * @param cachedData 预加载得到的元件数据
 * @return 不包含副作用的导出计划
 */
ExportRunPlan buildExportRunPlan(const ExportOptions& options,
                                 const QStringList& componentIds,
                                 const QMap<QString, QSharedPointer<ComponentData>>& cachedData);

}  // namespace EasyKiConverter
