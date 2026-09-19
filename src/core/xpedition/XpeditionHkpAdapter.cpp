#include "XpeditionHkpAdapter.h"

namespace EasyKiConverter {
namespace {

/** @brief 按名称查找唯一 Pad 定义，重复名称不允许隐式绑定。 */
const Parser::XpeditionPadDefinition* findPad(const Parser::XpeditionHkpModel& model,
                                              const QString& name,
                                              Parser::ParseDiagnostics* diagnostics,
                                              const QString& owner) {
    if (name.isEmpty())
        return nullptr;
    if (model.isPadAmbiguous(name)) {
        if (diagnostics)
            diagnostics->add(Parser::ParseSeverity::Error,
                             Parser::ParseScope::Footprint,
                             QStringLiteral("Xpedition Pad 引用存在歧义：%1").arg(name),
                             owner);
        return nullptr;
    }
    for (const Parser::XpeditionPadDefinition& pad : model.pads) {
        if (pad.name == name)
            return &pad;
    }
    if (diagnostics)
        diagnostics->add(Parser::ParseSeverity::Error,
                         Parser::ParseScope::Footprint,
                         QStringLiteral("Xpedition Pad 不存在：%1").arg(name),
                         owner);
    return nullptr;
}

/** @brief 按名称查找唯一孔定义，缺失孔由调用方决定是否继续。 */
const Parser::XpeditionHoleDefinition* findHole(const Parser::XpeditionHkpModel& model,
                                                const QString& name,
                                                Parser::ParseDiagnostics* diagnostics,
                                                const QString& owner) {
    if (name.isEmpty())
        return nullptr;
    if (model.isHoleAmbiguous(name)) {
        if (diagnostics)
            diagnostics->add(Parser::ParseSeverity::Error,
                             Parser::ParseScope::Footprint,
                             QStringLiteral("Xpedition 孔引用存在歧义：%1").arg(name),
                             owner);
        return nullptr;
    }
    for (const Parser::XpeditionHoleDefinition& hole : model.holes) {
        if (hole.name == name)
            return &hole;
    }
    if (diagnostics)
        diagnostics->add(Parser::ParseSeverity::Error,
                         Parser::ParseScope::Footprint,
                         QStringLiteral("Xpedition 孔不存在：%1").arg(name),
                         owner);
    return nullptr;
}

/** @brief 将 Xpedition Pad 形状和尺寸映射到统一焊盘 IR。 */
void applyPadShape(const Parser::XpeditionPadDefinition& source, IR::FootprintPadIR& target) {
    target.size = source.size;
    // 统一形状枚举，无法表达的来源形状降级为矩形并由上层保留诊断上下文。
    switch (source.shape) {
        case Parser::XpeditionPadShape::Round:
            target.shape = IR::PadShape::Ellipse;
            break;
        case Parser::XpeditionPadShape::Oblong:
            target.shape = IR::PadShape::Oval;
            break;
        case Parser::XpeditionPadShape::Polygon:
            target.shape = IR::PadShape::Polygon;
            target.customShapePoints = source.polygon;
            break;
        case Parser::XpeditionPadShape::Rectangle:
        case Parser::XpeditionPadShape::Square:
            target.shape = IR::PadShape::Rect;
            break;
        case Parser::XpeditionPadShape::Unknown:
            target.shape = IR::PadShape::Rect;
            break;
    }
}

/** @brief 根据 HKP 轮廓层名称选择保守的通用 IR 层。 */
IR::LayerType outlineLayer(const QString& sourceLayer, Parser::ParseDiagnostics* diagnostics, const QString& cellName) {
    const QString layer = sourceLayer.toUpper();
    if (layer.contains(QStringLiteral("SILK")) || layer.contains(QStringLiteral("OVERLAY")))
        return IR::LayerType::TopSilk;
    if (layer.contains(QStringLiteral("ASSEMBLY")))
        return IR::LayerType::TopAssembly;
    if (layer.contains(QStringLiteral("COURTYARD")))
        return IR::LayerType::Mechanical1;
    if (diagnostics)
        diagnostics->add(Parser::ParseSeverity::Warning,
                         Parser::ParseScope::Footprint,
                         QStringLiteral("Xpedition 轮廓层无法精确映射，已降级为用户层：%1").arg(sourceLayer),
                         cellName);
    return IR::LayerType::UserDefined;
}

/** @brief 按名称查找唯一 Cell，并对缺失或歧义关联写入诊断。 */
const Parser::XpeditionCellDefinition* findCell(const Parser::XpeditionHkpModel& model,
                                                const QString& name,
                                                Parser::ParseDiagnostics* diagnostics,
                                                const QString& partNumber) {
    if (name.isEmpty())
        return nullptr;
    if (model.isCellAmbiguous(name)) {
        if (diagnostics)
            diagnostics->add(Parser::ParseSeverity::Error,
                             Parser::ParseScope::Component,
                             QStringLiteral("Xpedition 器件引用的 Cell 存在歧义：%1").arg(name),
                             partNumber);
        return nullptr;
    }
    const Parser::XpeditionCellDefinition* cell = model.findCell(name);
    if (cell == nullptr && diagnostics)
        diagnostics->add(Parser::ParseSeverity::Error,
                         Parser::ParseScope::Component,
                         QStringLiteral("Xpedition 器件引用了不存在的 Cell：%1").arg(name),
                         partNumber);
    return cell;
}

}  // namespace

IR::FootprintComponentIR XpeditionHkpAdapter::toFootprint(const Parser::XpeditionHkpModel& model,
                                                          const Parser::XpeditionCellDefinition& cell,
                                                          Parser::ParseDiagnostics* diagnostics) {
    IR::FootprintComponentIR result;
    result.name = cell.name;
    for (const Parser::XpeditionCellPin& sourcePin : cell.pins) {
        if (model.isPadstackAmbiguous(sourcePin.padstack)) {
            if (diagnostics)
                diagnostics->add(Parser::ParseSeverity::Error,
                                 Parser::ParseScope::Footprint,
                                 QStringLiteral("Xpedition 引脚引用的 Padstack 存在歧义：%1").arg(sourcePin.padstack),
                                 cell.name);
            continue;
        }
        const Parser::XpeditionPadstackDefinition* padstack = model.findPadstack(sourcePin.padstack);
        if (padstack == nullptr) {
            if (diagnostics)
                diagnostics->add(Parser::ParseSeverity::Error,
                                 Parser::ParseScope::Footprint,
                                 QStringLiteral("Xpedition 引脚引用了不存在的 Padstack：%1").arg(sourcePin.padstack),
                                 cell.name);
            continue;
        }
        const bool hasTop = !padstack->topPad.isEmpty();
        const bool hasBottom = !padstack->bottomPad.isEmpty();
        const QString padName = hasTop ? padstack->topPad : padstack->bottomPad;
        const Parser::XpeditionPadDefinition* sourcePad = findPad(model, padName, diagnostics, cell.name);
        if (sourcePad == nullptr)
            continue;

        IR::FootprintPadIR pad;
        pad.number = sourcePin.number;
        pad.position = sourcePin.position + sourcePad->offset;
        pad.rotation = sourcePin.rotation;
        pad.layer = hasTop && hasBottom ? IR::LayerType::MultiLayer
                                        : (hasBottom ? IR::LayerType::BottomCopper : IR::LayerType::TopCopper);
        applyPadShape(*sourcePad, pad);
        if (!padstack->holeName.isEmpty()) {
            const Parser::XpeditionHoleDefinition* hole = findHole(model, padstack->holeName, diagnostics, cell.name);
            if (hole != nullptr) {
                pad.padType = IR::PadType::ThroughHole;
                pad.layer = IR::LayerType::MultiLayer;
                pad.isPlated = hole->plated;
                pad.holeSize = hole->size.width();
                pad.holeLength = qMax(hole->size.width(), hole->size.height());
            }
        }
        result.pads.append(pad);
    }
    for (const Parser::XpeditionCellOutline& sourceOutline : cell.outlines) {
        if (sourceOutline.points.size() < 2)
            continue;
        IR::FootprintTrackIR track;
        track.points = sourceOutline.points;
        track.layer = outlineLayer(sourceOutline.layer, diagnostics, cell.name);
        result.tracks.append(track);
    }
    return result;
}

XpeditionHkpConversionResult XpeditionHkpAdapter::toIR(const Parser::XpeditionHkpModel& model,
                                                       const QMap<QString, IR::SymbolComponentIR>& symbols,
                                                       Parser::ParseDiagnostics* diagnostics) {
    XpeditionHkpConversionResult result;
    QMap<QString, IR::FootprintComponentIR> footprints;
    for (const Parser::XpeditionCellDefinition& cell : model.cells) {
        IR::FootprintComponentIR footprint = toFootprint(model, cell, diagnostics);
        footprints.insert(cell.name, footprint);
        result.footprints.append(footprint);
    }
    for (const Parser::XpeditionPartDefinition& part : model.parts) {
        const Parser::XpeditionCellDefinition* cell = findCell(model, part.topCell, diagnostics, part.number);
        if (cell == nullptr && !part.bottomCell.isEmpty())
            cell = findCell(model, part.bottomCell, diagnostics, part.number);
        if (cell == nullptr)
            continue;
        if (!symbols.contains(part.symbol)) {
            if (diagnostics)
                diagnostics->add(Parser::ParseSeverity::Error,
                                 Parser::ParseScope::Symbol,
                                 QStringLiteral("Xpedition 器件引用了不存在的符号：%1").arg(part.symbol),
                                 part.number);
            continue;
        }
        IR::ComponentIR component;
        component.name = part.name.isEmpty() ? part.number : part.name;
        component.description = part.description;
        component.prefix = part.referencePrefix;
        component.package = cell->name;
        component.symbol = symbols.value(part.symbol);
        component.footprint = footprints.value(cell->name);
        component.sourceMetadata.insert(QStringLiteral("xpeditionPartNumber"), part.number);
        component.sourceMetadata.insert(QStringLiteral("xpeditionSymbol"), part.symbol);
        component.sourceMetadata.insert(QStringLiteral("xpeditionTopCell"), part.topCell);
        component.sourceMetadata.insert(QStringLiteral("xpeditionBottomCell"), part.bottomCell);
        for (auto iterator = part.properties.cbegin(); iterator != part.properties.cend(); ++iterator)
            component.sourceMetadata.insert(iterator.key(), iterator.value());
        result.components.append(component);
    }
    return result;
}

}  // namespace EasyKiConverter
