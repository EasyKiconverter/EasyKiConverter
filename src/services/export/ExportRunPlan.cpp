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
        typeNames.append(symbolOnlyCombinedLibrary ? QStringLiteral("Symbol") : QStringLiteral("Footprint"));
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
    const bool combinedTarget = options.targetFormat == TargetEdaFormat::Eagle ||
                                options.targetFormat == TargetEdaFormat::Cadstar ||
                                options.targetFormat == TargetEdaFormat::Allegro;
    const bool embeddedModelTarget =
        options.targetFormat == TargetEdaFormat::Altium || options.targetFormat == TargetEdaFormat::Allegro;
    // 仅符号模式仍由组合库写入阶段负责，但进度需要展示为 Symbol。
    plan.symbolOnlyCombinedLibrary = combinedTarget && options.exportSymbol && !options.exportFootprint;
    // Eagle/CADSTAR 的组合库由封装阶段统一写入，符号单独导出也复用同一文件事务。
    plan.enableSymbol = options.exportSymbol && options.targetFormat != TargetEdaFormat::Allegro &&
                        options.targetFormat != TargetEdaFormat::Eagle &&
                        options.targetFormat != TargetEdaFormat::Cadstar;
    // OrCAD Capture XML 只承载符号和封装名称属性，PCB 封装几何由其他目标库负责。
    // 独立三维导出不应顺带生成封装库；只有目标格式需要把模型关联写入库时，才保留封装阶段。
    plan.enableFootprint = (options.exportFootprint && options.targetFormat != TargetEdaFormat::Orcad) ||
                           (combinedTarget && options.exportSymbol) || (embeddedModelTarget && options.exportModel3D);
    // 目标格式没有经过本项目验证的原生模型关联时，仍输出独立模型文件并保留诊断。
    plan.enableModel3D = options.exportModel3D;
    plan.runExternalModel3DStage = plan.enableModel3D && options.targetFormat != TargetEdaFormat::Altium &&
                                   options.targetFormat != TargetEdaFormat::Allegro;
    plan.enablePreview = options.exportPreviewImages;
    plan.enableDatasheet = options.exportDatasheet;

    // Eagle 和 CADSTAR 在同时选择符号、封装时才写入完整组合库；仅封装导出不应无条件要求符号缓存。
    const bool needsSymbolData = plan.enableSymbol || (combinedTarget && options.exportSymbol);
    // 组合目标的仅符号路径直接调用符号 writer，不应因为没有封装缓存而被预加载阶段拦截。
    const bool needsFootprintData = (options.exportFootprint && options.targetFormat != TargetEdaFormat::Orcad) ||
                                    (embeddedModelTarget && options.exportModel3D);
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
