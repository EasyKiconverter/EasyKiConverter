#include "XpeditionHkpAdapter.h"

#include "XpeditionSymbolAdapter.h"

namespace EasyKiConverter {
namespace {

/**
 * @brief 按名称查找唯一 Pad 定义。
 * @param model Xpedition HKP 模型。
 * @param name Pad 名称。
 * @param diagnostics 可选的诊断输出。
 * @param owner 当前引用所属对象。
 * @return 唯一匹配的 Pad 定义；缺失或歧义时返回 nullptr。
 */
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

/**
 * @brief 按名称查找唯一孔定义。
 * @param model Xpedition HKP 模型。
 * @param name 孔名称。
 * @param diagnostics 可选的诊断输出。
 * @param owner 当前引用所属对象。
 * @return 唯一匹配的孔定义；缺失或歧义时返回 nullptr。
 */
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

/**
 * @brief 将 Xpedition Pad 形状和尺寸映射到统一焊盘 IR。
 * @param source Xpedition Pad 定义。
 * @param target 接收结果的焊盘 IR。
 */
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

/**
 * @brief 根据 HKP 轮廓层名称选择保守的通用 IR 层。
 * @param sourceLayer Xpedition 来源层名称。
 * @param diagnostics 可选的诊断输出。
 * @param cellName 当前 Cell 名称。
 * @return 映射后的 IR 层。
 */
IR::LayerType outlineLayer(const QString& sourceLayer, Parser::ParseDiagnostics* diagnostics, const QString& cellName) {
    const QString layer = sourceLayer.toUpper();  // 规范化后的来源层名称。
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

/**
 * @brief 按名称查找唯一 Cell，并对缺失或歧义关联写入诊断。
 * @param model Xpedition HKP 模型。
 * @param name Cell 名称。
 * @param diagnostics 可选的诊断输出。
 * @param partNumber 当前器件编号。
 * @param reportFailure 是否报告缺失或歧义错误。
 * @return 唯一匹配的 Cell 定义；缺失或歧义时返回 nullptr。
 */
const Parser::XpeditionCellDefinition* findCell(const Parser::XpeditionHkpModel& model,
                                                const QString& name,
                                                Parser::ParseDiagnostics* diagnostics,
                                                const QString& partNumber,
                                                bool reportFailure = true) {
    if (name.isEmpty())
        return nullptr;
    if (model.isCellAmbiguous(name)) {
        if (diagnostics && reportFailure)
            diagnostics->add(Parser::ParseSeverity::Error,
                             Parser::ParseScope::Component,
                             QStringLiteral("Xpedition 器件引用的 Cell 存在歧义：%1").arg(name),
                             partNumber);
        return nullptr;
    }
    const Parser::XpeditionCellDefinition* cell = model.findCell(name);  // 查找到的 Cell 定义。
    if (cell == nullptr && diagnostics && reportFailure)
        diagnostics->add(Parser::ParseSeverity::Error,
                         Parser::ParseScope::Component,
                         QStringLiteral("Xpedition 器件引用了不存在的 Cell：%1").arg(name),
                         partNumber);
    return cell;
}

}  // namespace

/**
 * @brief 将 Xpedition Cell 转换为封装组件 IR。
 * @param model Xpedition HKP 模型。
 * @param cell 待转换的 Cell 定义。
 * @param diagnostics 可选的诊断输出。
 * @return 转换后的封装组件 IR。
 */
IR::FootprintComponentIR XpeditionHkpAdapter::toFootprint(const Parser::XpeditionHkpModel& model,
                                                          const Parser::XpeditionCellDefinition& cell,
                                                          Parser::ParseDiagnostics* diagnostics) {
    IR::FootprintComponentIR result;  // 输出封装组件 IR。
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
        const Parser::XpeditionPadstackDefinition* padstack =
            model.findPadstack(sourcePin.padstack);  // 来源 Padstack。
        if (padstack == nullptr) {
            if (diagnostics)
                diagnostics->add(Parser::ParseSeverity::Error,
                                 Parser::ParseScope::Footprint,
                                 QStringLiteral("Xpedition 引脚引用了不存在的 Padstack：%1").arg(sourcePin.padstack),
                                 cell.name);
            continue;
        }
        const bool hasTop = !padstack->topPad.isEmpty();  // 是否存在顶层 Pad。
        const bool hasBottom = !padstack->bottomPad.isEmpty();  // 是否存在底层 Pad。
        const QString padName = hasTop ? padstack->topPad : padstack->bottomPad;  // 实际使用的 Pad 名称。
        const Parser::XpeditionPadDefinition* sourcePad =
            findPad(model, padName, diagnostics, cell.name);  // 来源 Pad。
        if (sourcePad == nullptr)
            continue;

        IR::FootprintPadIR pad;  // 当前焊盘 IR。
        pad.number = sourcePin.number;
        pad.position = sourcePin.position + sourcePad->offset;
        pad.rotation = sourcePin.rotation;
        pad.layer = hasTop && hasBottom ? IR::LayerType::MultiLayer
                                        : (hasBottom ? IR::LayerType::BottomCopper : IR::LayerType::TopCopper);
        applyPadShape(*sourcePad, pad);
        if (!padstack->holeName.isEmpty()) {
            const Parser::XpeditionHoleDefinition* hole =
                findHole(model, padstack->holeName, diagnostics, cell.name);  // 来源孔定义。
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
        IR::FootprintOutlineIR outline;  // 当前封装轮廓 IR。
        outline.points = sourceOutline.points;
        outline.layer = outlineLayer(sourceOutline.layer, diagnostics, cell.name);
        result.outlines.append(outline);
    }
    return result;
}

/**
 * @brief 将 Xpedition HKP 模型和符号转换为统一组件 IR。
 * @param model Xpedition HKP 模型。
 * @param symbols 已转换的符号集合。
 * @param diagnostics 可选的诊断输出。
 * @return 转换后的组件、封装和符号集合。
 */
XpeditionHkpConversionResult XpeditionHkpAdapter::toIR(const Parser::XpeditionHkpModel& model,
                                                       const QMap<QString, IR::SymbolComponentIR>& symbols,
                                                       Parser::ParseDiagnostics* diagnostics) {
    XpeditionHkpConversionResult result;  // 输出转换结果。
    QMap<QString, IR::FootprintComponentIR> footprints;  // 按 Cell 名称索引的封装。
    for (const Parser::XpeditionCellDefinition& cell : model.cells) {
        IR::FootprintComponentIR footprint = toFootprint(model, cell, diagnostics);  // 当前 Cell 的封装 IR。
        footprints.insert(cell.name, footprint);
        result.footprints.append(footprint);
    }
    for (const Parser::XpeditionPartDefinition& part : model.parts) {
        const bool hasFallbackCell =
            !part.bottomCell.isEmpty() && part.bottomCell != part.topCell;  // 是否需要底层 Cell 回退。
        const Parser::XpeditionCellDefinition* cell =
            findCell(model, part.topCell, diagnostics, part.number, !hasFallbackCell);
        if (cell == nullptr && !part.bottomCell.isEmpty())
            cell = findCell(model, part.bottomCell, diagnostics, part.number, true);
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
        IR::ComponentIR component;  // 当前器件 IR。
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

/**
 * @brief 将 Xpedition HKP 文档及其符号文档转换为统一组件 IR。
 * @param document Xpedition HKP 文档。
 * @param symbolDocuments 关联的符号文档列表。
 * @param diagnostics 可选的诊断输出。
 * @return 转换后的组件、封装和符号集合。
 */
XpeditionHkpConversionResult XpeditionHkpAdapter::toIR(const Parser::XpeditionHkpDocument& document,
                                                       const QList<Parser::XpeditionSymbolDocument>& symbolDocuments,
                                                       Parser::ParseDiagnostics* diagnostics) {
    if (diagnostics)
        diagnostics->append(document.diagnostics);

    QMap<QString, IR::SymbolComponentIR> symbols;  // 按符号名称索引的符号 IR。
    for (const Parser::XpeditionSymbolDocument& symbolDocument : symbolDocuments) {
        if (diagnostics)
            diagnostics->append(symbolDocument.diagnostics);
        if (!symbolDocument.isRecognized())
            continue;
        IR::SymbolComponentIR symbol = XpeditionSymbolAdapter::toIR(symbolDocument, diagnostics);  // 当前符号 IR。
        if (symbol.name.isEmpty())
            continue;
        if (symbols.contains(symbol.name)) {
            if (diagnostics)
                diagnostics->add(Parser::ParseSeverity::Error,
                                 Parser::ParseScope::Symbol,
                                 QStringLiteral("Xpedition 符号跨文件重名：%1").arg(symbol.name),
                                 symbol.name);
            continue;
        }
        symbols.insert(symbol.name, symbol);
    }
    return toIR(document.model, symbols, diagnostics);
}

}  // namespace EasyKiConverter
