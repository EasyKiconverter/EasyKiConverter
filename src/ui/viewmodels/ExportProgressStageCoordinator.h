#ifndef EXPORTPROGRESSSTAGECOORDINATOR_H
#define EXPORTPROGRESSSTAGECOORDINATOR_H

#include "services/export/ExportProgress.h"

namespace EasyKiConverter {

class ExportProgressViewModel;

/**
 * @brief 协调导出预加载和阶段进度状态。
 * @details 集中维护阶段权重、启用类型判断和前后阶段依赖，避免 ViewModel 同时承担进度算法。
 */
class ExportProgressStageCoordinator final {
public:
    /**
     * @brief 处理预加载进度并更新抓取阶段。
     * @param viewModel 导出进度视图模型。
     * @param progress 预加载阶段进度。
     */
    static void handlePreloadProgress(ExportProgressViewModel& viewModel, const PreloadProgress& progress);

    /**
     * @brief 处理整体进度并更新三段式进度条。
     * @param viewModel 导出进度视图模型。
     * @param progress 整体导出进度。
     */
    static void handleProgress(ExportProgressViewModel& viewModel, const ExportOverallProgress& progress);

private:
    static bool isTypeEnabled(const ExportProgressViewModel& viewModel, const QString& typeName);
    static int averageTypeProgress(const ExportOverallProgress& progress, const QStringList& typeNames);
    static int stageTypeProgress(const ExportProgressViewModel& viewModel,
                                 const ExportOverallProgress& progress,
                                 const QStringList& typeNames);
    static int weightedOverallProgress(const ExportProgressViewModel& viewModel);
};

}  // namespace EasyKiConverter

#endif  // EXPORTPROGRESSSTAGECOORDINATOR_H
