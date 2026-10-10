#include "ExportStageLaunchCoordinator.h"

#include "ComponentService.h"
#include "DatasheetExportStage.h"
#include "ExportRunPlan.h"
#include "FootprintExportStage.h"
#include "Model3DExportStage.h"
#include "ParallelExportService.h"
#include "PreviewImagesExportStage.h"
#include "SymbolExportStage.h"
#include "models/ComponentData.h"
#include "services/ComponentCacheService.h"

#include <QDebug>

namespace EasyKiConverter {

/**
 * @brief 根据预加载结果初始化并启动所有需要的导出阶段。
 * @param owner 并行导出服务所有者。
 */
void ExportStageLaunchCoordinator::start(ParallelExportService& owner) {
    if (owner.m_progress.currentStage == ExportOverallProgress::Stage::Exporting) {
        qWarning() << "ParallelExportService: Export already in progress";
        return;
    }

    if (!owner.m_preloadCompleted) {
        qWarning() << "ParallelExportService: startExport called before preload completed";
        emit owner.failed(QStringLiteral("Preload has not completed"));
        return;
    }

    if (owner.m_componentIds.isEmpty()) {
        qWarning() << "ParallelExportService: No components to export";
        emit owner.failed(QStringLiteral("No components to export"));
        return;
    }

    owner.cleanupExportStages();
    owner.m_cancelRequested = false;
    // 预加载完成，旧回调已全部处理，解除全局 tombstone。
    ComponentCacheService::instance()->clearGlobalTombstone();

    // 从 ComponentService 刷新最新的组件数据，确保用户在 UI 中的编辑生效。
    if (owner.m_componentService) {
        for (auto it = owner.m_cachedData.begin(); it != owner.m_cachedData.end(); ++it) {
            ComponentData freshData =
                owner.m_componentService->getComponentData(it.key());  ///< 当前组件的最新编辑数据。
            if (freshData.symbolData() && it.value() && it.value()->symbolData()) {
                it.value()->symbolData()->setInfo(freshData.symbolData()->info());
            }
            if (freshData.footprintData() && it.value() && it.value()->footprintData()) {
                it.value()->footprintData()->setInfo(freshData.footprintData()->info());
            }
        }
    }
    const quint64 runGeneration = owner.m_activeRunGeneration;  ///< 当前导出运行代次。

    qDebug() << "ParallelExportService: Starting export for" << owner.m_componentIds.size() << "components"
             << (owner.m_options.retryMode ? "(retry mode)" : "");

    owner.m_progress.currentStage = ExportOverallProgress::Stage::Exporting;
    owner.m_progress.startTime = QDateTime::currentDateTime();
    owner.m_progress.exportTypeProgress.clear();
    owner.m_runningExportStages = 0;

    // Altium PcbLib 将 STEP 直接嵌入 Library/Models，不生成 KiCad 风格的
    // 外部 .3dmodels 目录；封装阶段仍会按需获取 STEP 并交给写入器。
    // Model3D 统计项仍需保留，Altium 下由封装阶段镜像完成状态。
    const ExportRunPlan plan =
        buildExportRunPlan(owner.m_options, owner.m_componentIds, owner.m_cachedData);  ///< 当前导出阶段计划。
    const bool enableSymbol = plan.enableSymbol;  ///< 是否启动符号导出阶段。
    const bool enableFootprint = plan.enableFootprint;  ///< 是否启动封装导出阶段。
    const bool runExternalModel3DStage = plan.runExternalModel3DStage;  ///< 是否启动独立三维模型阶段。
    const bool enablePreview = plan.enablePreview;  ///< 是否启动预览图阶段。
    const bool enableDatasheet = plan.enableDatasheet;  ///< 是否启动数据手册阶段。
    if (!plan.missingDataComponentIds.isEmpty()) {
        qWarning() << "ParallelExportService: Missing preloaded component data for"
                   << plan.missingDataComponentIds.size() << "components:" << plan.missingDataComponentIds;
    }

    const auto initTypeProgress = [&owner](const QString& typeName) {  ///< 初始化一种导出类型的进度对象。
        ExportTypeProgress progress;  ///< 当前导出类型进度。
        progress.typeName = typeName;
        progress.totalCount = owner.m_componentIds.size();
        owner.m_progress.exportTypeProgress[typeName] = progress;
    };

    for (const QString& typeName : plan.progressTypeNames()) {
        initTypeProgress(typeName);
    }

    const auto markMissingDataFailures = [&owner, &plan](const QString& typeName) {  ///< 标记缺失缓存数据的失败项。
        for (const QString& componentId : plan.missingDataComponentIds) {  ///< 当前缺失数据的元器件编号。
            ExportItemStatus status;  ///< 当前元器件的失败状态。
            status.status = ExportItemStatus::Status::Failed;
            status.errorMessage = QStringLiteral("Component preload data missing");
            owner.onExportItemStatusChanged(componentId, typeName, status);
        }
    };

    if (!plan.missingDataComponentIds.isEmpty()) {
        for (const QString& typeName : plan.progressTypeNames()) {
            markMissingDataFailures(typeName);
        }
    }

    owner.m_runningExportStages = plan.runningStageCount();
    owner.logNetworkRuntimeStats(QStringLiteral("export-start"));

    if (plan.exportableComponentIds.isEmpty()) {
        qWarning() << "ParallelExportService: No exportable components after preload";
        owner.m_progress.currentStage = ExportOverallProgress::Stage::Failed;
        owner.m_progress.endTime = QDateTime::currentDateTime();
        owner.writeExportDetailedReport(QStringLiteral("export-failed-no-exportable-components"));
        emit owner.failed(QStringLiteral("No exportable components after preload"));
        owner.cleanupExportStages();
        return;
    }

    // 创建并启动各导出类型的 Stage。
    if (enableSymbol) {
        auto* stage = new SymbolExportStage(&owner);  ///< 新建的符号导出阶段。
        stage->setOptions(owner.m_options);
        owner.registerStageAndStart(stage, QStringLiteral("Symbol"), plan.exportableComponentIds, runGeneration);
    }

    if (enableFootprint) {
        auto* stage = new FootprintExportStage(&owner);  ///< 新建的封装导出阶段。
        stage->setOptions(owner.m_options);
        owner.registerStageAndStart(
            stage,
            plan.symbolOnlyCombinedLibrary ? QStringLiteral("Symbol") : QStringLiteral("Footprint"),
            plan.exportableComponentIds,
            runGeneration);
    }

    if (runExternalModel3DStage) {
        auto* stage = new Model3DExportStage(&owner);  ///< 新建的独立三维模型导出阶段。
        stage->setOptions(owner.m_options);
        owner.registerStageAndStart(stage, QStringLiteral("Model3D"), plan.exportableComponentIds, runGeneration);
    }

    if (enablePreview) {
        auto* stage = new PreviewImagesExportStage(&owner);  ///< 新建的预览图导出阶段。
        stage->setOptions(owner.m_options);
        owner.registerStageAndStart(stage, QStringLiteral("PreviewImages"), plan.exportableComponentIds, runGeneration);
    }

    if (enableDatasheet) {
        auto* stage = new DatasheetExportStage(&owner);  ///< 新建的数据手册导出阶段。
        stage->setOptions(owner.m_options);
        owner.registerStageAndStart(stage, QStringLiteral("Datasheet"), plan.exportableComponentIds, runGeneration);
    }

    // 重试模式仅对本次导出生效，避免影响后续正常导出。
    owner.m_options.retryMode = false;

    if (owner.m_runningExportStages == 0) {
        qWarning() << "ParallelExportService: No export types enabled";
        owner.m_progress.currentStage = ExportOverallProgress::Stage::Completed;
        owner.m_progress.endTime = QDateTime::currentDateTime();
        owner.logNetworkRuntimeStats(QStringLiteral("export-no-types-enabled"));
        owner.writeExportDetailedReport(QStringLiteral("export-no-types-enabled"));
        emit owner.completed(0, 0);
    }
}

}  // namespace EasyKiConverter
