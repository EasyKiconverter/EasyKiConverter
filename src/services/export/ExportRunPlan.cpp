#include "ExportRunPlan.h"

#include "models/ComponentData.h"

namespace EasyKiConverter {

/** @brief 计算需要等待完成的外部导出阶段数量。 */
int ExportRunPlan::runningStageCount() const {
    return (enableSymbol ? 1 : 0) + (enableFootprint ? 1 : 0) + (runExternalModel3DStage ? 1 : 0) +
           (enablePreview ? 1 : 0) + (enableDatasheet ? 1 : 0);
}

/** @brief 按照服务进度统计使用的固定顺序返回导出类型名称。 */
QStringList ExportRunPlan::progressTypeNames() const {
    QStringList typeNames;
    if (enableSymbol) {
        typeNames.append(QStringLiteral("Symbol"));
    }
    if (enableFootprint) {
        typeNames.append(QStringLiteral("Footprint"));
    }
    if (enableModel3D) {
        typeNames.append(QStringLiteral("Model3D"));
    }
    if (enablePreview) {
        typeNames.append(QStringLiteral("PreviewImages"));
    }
    if (enableDatasheet) {
        typeNames.append(QStringLiteral("Datasheet"));
    }
    return typeNames;
}

ExportRunPlan buildExportRunPlan(const ExportOptions& options,
                                 const QStringList& componentIds,
                                 const QMap<QString, QSharedPointer<ComponentData>>& cachedData) {
    ExportRunPlan plan;
    // Eagle 完整 XML 库由封装阶段一次性写入 Symbol、Package 和 DeviceSet，避免两个阶段争用同一 .lbr。
    plan.enableSymbol = options.exportSymbol && options.targetFormat != TargetEdaFormat::Eagle;
    plan.enableFootprint = options.exportFootprint;
    plan.enableModel3D = options.exportModel3D && options.targetFormat != TargetEdaFormat::Xpedition &&
                         options.targetFormat != TargetEdaFormat::Pads && options.targetFormat != TargetEdaFormat::Pcad;
    plan.enableModel3D = plan.enableModel3D && options.targetFormat != TargetEdaFormat::Pcad;
    plan.runExternalModel3DStage = plan.enableModel3D && options.targetFormat != TargetEdaFormat::Altium &&
                                   options.targetFormat != TargetEdaFormat::Allegro;
    plan.enablePreview = options.exportPreviewImages;
    plan.enableDatasheet = options.exportDatasheet;

    for (const QString& componentId : componentIds) {
        const auto it = cachedData.constFind(componentId);
        if (it != cachedData.cend() && it.value() && it.value()->isValid() && it.value()->symbolData() &&
            it.value()->footprintData()) {
            plan.exportableComponentIds.append(componentId);
        } else {
            plan.missingDataComponentIds.append(componentId);
        }
    }
    return plan;
}

}  // namespace EasyKiConverter
