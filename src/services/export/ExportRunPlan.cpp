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
    plan.enableSymbol = options.exportSymbol && options.targetFormat != TargetEdaFormat::Allegro &&
                        options.targetFormat != TargetEdaFormat::Eagle &&
                        options.targetFormat != TargetEdaFormat::Cadstar;
    // OrCAD Capture XML 只承载符号和封装名称属性，PCB 封装几何由其他目标库负责。
    plan.enableFootprint = options.exportFootprint && options.targetFormat != TargetEdaFormat::Orcad;
    // 目标格式没有经过本项目验证的原生模型关联时，仍输出独立模型文件并保留诊断。
    plan.enableModel3D = options.exportModel3D;
    plan.runExternalModel3DStage = plan.enableModel3D && options.targetFormat != TargetEdaFormat::Altium &&
                                   options.targetFormat != TargetEdaFormat::Allegro;
    plan.enablePreview = options.exportPreviewImages;
    plan.enableDatasheet = options.exportDatasheet;

    // Eagle 和 CADSTAR 在同时选择符号、封装时才写入完整组合库；仅封装导出不应无条件要求符号缓存。
    const bool writesCombinedLibrary =
        (options.targetFormat == TargetEdaFormat::Eagle || options.targetFormat == TargetEdaFormat::Cadstar) &&
        options.exportSymbol && options.exportFootprint;
    const bool needsSymbolData = plan.enableSymbol || writesCombinedLibrary;
    const bool needsFootprintData = plan.enableFootprint || writesCombinedLibrary;
    for (const QString& componentId : componentIds) {
        const auto it = cachedData.constFind(componentId);
        const auto& component = it == cachedData.cend() ? QSharedPointer<ComponentData>() : it.value();
        if (component && component->isValid() && (!needsSymbolData || component->symbolData()) &&
            (!needsFootprintData || component->footprintData())) {
            plan.exportableComponentIds.append(componentId);
        } else {
            plan.missingDataComponentIds.append(componentId);
        }
    }
    return plan;
}

}  // namespace EasyKiConverter
