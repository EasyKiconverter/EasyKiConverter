#include "ExportProgressStageCoordinator.h"

#include "ExportProgressViewModel.h"

#include <QtGlobal>

namespace EasyKiConverter {

namespace {

constexpr int kFetchWeight = 30;  ///< 抓取阶段在总体进度中的权重。
constexpr int kProcessWeight = 50;  ///< 处理阶段在总体进度中的权重。
constexpr int kWriteWeight = 20;  ///< 写入阶段在总体进度中的权重。

const QString kSymbolType = QStringLiteral("Symbol");  ///< 符号导出类型名称。
const QString kFootprintType = QStringLiteral("Footprint");  ///< 封装导出类型名称。
const QString kModel3DType = QStringLiteral("Model3D");  ///< 三维模型导出类型名称。
const QString kPreviewType = QStringLiteral("PreviewImages");  ///< 预览图导出类型名称。
const QString kDatasheetType = QStringLiteral("Datasheet");  ///< 数据手册导出类型名称。

}  // namespace

/**
 * @brief 判断指定导出类型是否已在当前选项中启用。
 * @param viewModel 导出进度视图模型。
 * @param typeName 导出类型名称。
 * @return 类型已启用时返回 true。
 */
bool ExportProgressStageCoordinator::isTypeEnabled(const ExportProgressViewModel& viewModel, const QString& typeName) {
    if (typeName == kSymbolType)
        return viewModel.m_exportSymbolEnabled;
    if (typeName == kFootprintType)
        return viewModel.m_exportFootprintEnabled;
    if (typeName == kModel3DType)
        return viewModel.m_exportModel3DEnabled;
    if (typeName == kPreviewType)
        return viewModel.m_exportPreviewEnabled;
    if (typeName == kDatasheetType)
        return viewModel.m_exportDatasheetEnabled;
    return false;
}

/**
 * @brief 计算一组导出类型的平均进度。
 * @param progress 总体导出进度。
 * @param typeNames 待计算的导出类型名称。
 * @return 平均进度百分比。
 */
int ExportProgressStageCoordinator::averageTypeProgress(const ExportOverallProgress& progress,
                                                        const QStringList& typeNames) {
    int sum = 0;  // 已找到类型的进度总和。
    int count = 0;  // 已找到类型数量。
    for (const QString& typeName : typeNames) {  // 遍历请求的导出类型。
        const auto it = progress.exportTypeProgress.constFind(typeName);  // 当前类型的进度迭代器。
        if (it == progress.exportTypeProgress.constEnd())
            continue;
        sum += it.value().percentage();
        ++count;
    }
    return count == 0 ? 0 : qBound(0, sum / count, 100);
}

/**
 * @brief 计算启用导出类型的阶段进度，未启用时视为已完成。
 * @param viewModel 导出进度视图模型。
 * @param progress 总体导出进度。
 * @param typeNames 阶段涉及的导出类型名称。
 * @return 阶段进度百分比。
 */
int ExportProgressStageCoordinator::stageTypeProgress(const ExportProgressViewModel& viewModel,
                                                      const ExportOverallProgress& progress,
                                                      const QStringList& typeNames) {
    for (const QString& typeName : typeNames) {
        if (isTypeEnabled(viewModel, typeName))
            return averageTypeProgress(progress, typeNames);
    }
    return 100;
}

/**
 * @brief 按抓取、处理和写入三个阶段的权重合并总体进度。
 * @param viewModel 导出进度视图模型。
 * @return 加权后的总体进度百分比。
 */
int ExportProgressStageCoordinator::weightedOverallProgress(const ExportProgressViewModel& viewModel) {
    // 按三个阶段权重计算未裁剪的总体进度值。
    const int weighted = (viewModel.m_fetchProgress * kFetchWeight) + (viewModel.m_processProgress * kProcessWeight) +
                         (viewModel.m_writeProgress * kWriteWeight);
    return qBound(0, weighted / 100, 100);
}

/**
 * @brief 处理预加载阶段进度并更新视图模型。
 * @param viewModel 导出进度视图模型。
 * @param progress 预加载进度。
 */
void ExportProgressStageCoordinator::handlePreloadProgress(ExportProgressViewModel& viewModel,
                                                           const PreloadProgress& progress) {
    QString statusText =
        QString("Preloading... %1/%2").arg(progress.completedCount).arg(progress.totalCount);  // 预加载状态文本。
    if (!progress.currentComponentId.isEmpty())
        statusText += QString(" (%1)").arg(progress.currentComponentId);
    viewModel.setStatus(statusText);
    viewModel.m_fetchProgress = qBound(0, progress.percentage(), 100);
    viewModel.m_processProgress = 0;
    viewModel.m_writeProgress = 0;
    viewModel.setProgress(weightedOverallProgress(viewModel));
    emit viewModel.stageProgressChanged();
}

/**
 * @brief 根据当前导出阶段更新抓取、处理、写入进度和状态文本。
 * @param viewModel 导出进度视图模型。
 * @param progress 总体导出进度。
 */
void ExportProgressStageCoordinator::handleProgress(ExportProgressViewModel& viewModel,
                                                    const ExportOverallProgress& progress) {
    // 按当前阶段更新三段式进度，并确保后续阶段不超过前置阶段。
    switch (progress.currentStage) {
        case ExportOverallProgress::Stage::Preloading:
            viewModel.setStatus("Preloading components...");
            viewModel.m_fetchProgress = progress.preloadProgress.percentage();
            viewModel.m_processProgress = 0;
            viewModel.m_writeProgress = 0;
            viewModel.setProgress(weightedOverallProgress(viewModel));
            break;
        case ExportOverallProgress::Stage::Exporting: {
            viewModel.setStatus("Exporting components...");
            viewModel.m_fetchProgress =
                stageTypeProgress(viewModel, progress, {kPreviewType, kDatasheetType, kModel3DType});
            const int rawProcessProgress =
                stageTypeProgress(viewModel, progress, {kSymbolType, kFootprintType, kModel3DType});
            const int rawWriteProgress = stageTypeProgress(  // 未限制前的写入阶段进度。
                viewModel,
                progress,
                {kSymbolType, kFootprintType, kModel3DType, kPreviewType, kDatasheetType});
            viewModel.m_processProgress = qMin(rawProcessProgress, viewModel.m_fetchProgress);
            viewModel.m_writeProgress = qMin(rawWriteProgress, viewModel.m_processProgress);
            viewModel.setProgress(weightedOverallProgress(viewModel));
            break;
        }
        case ExportOverallProgress::Stage::Completed:
            viewModel.m_fetchProgress = 100;
            viewModel.m_processProgress = 100;
            viewModel.m_writeProgress = 100;
            viewModel.setProgress(100);
            viewModel.setStatus("Export completed");
            break;
        case ExportOverallProgress::Stage::Cancelled:
            viewModel.m_fetchProgress = 0;
            viewModel.m_processProgress = 0;
            viewModel.m_writeProgress = 0;
            viewModel.setProgress(0);
            viewModel.setStatus("Export cancelled");
            break;
        case ExportOverallProgress::Stage::Failed:
            viewModel.m_fetchProgress = 0;
            viewModel.m_processProgress = 0;
            viewModel.m_writeProgress = 0;
            viewModel.setStatus("Export failed");
            break;
        default:
            break;
    }
    emit viewModel.stageProgressChanged();
}

}  // namespace EasyKiConverter
