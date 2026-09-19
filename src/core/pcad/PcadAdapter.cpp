#include "PcadAdapter.h"

#include <QtMath>

namespace EasyKiConverter {
namespace {

/** @brief 将 P-CAD 层号映射为 IR 层类型并记录不确定映射。 */
IR::LayerType layer(int number, Parser::ParseDiagnostics* diagnostics) {
    if (number == 1)
        return IR::LayerType::TopCopper;
    if (number == 2)
        return IR::LayerType::BottomCopper;
    if (number == 0)
        return IR::LayerType::TopSilk;
    if (diagnostics)
        diagnostics->add(Parser::ParseSeverity::Warning,
                         Parser::ParseScope::Footprint,
                         QStringLiteral("P-CAD 层号无法精确映射，已降级为用户层"),
                         QString::number(number));
    return IR::LayerType::UserDefined;
}

/** @brief 按名称查找 Pad Style，找不到时由调用方生成关联诊断。 */
const Parser::PcadPadStyle* findStyle(const Parser::PcadBoard& board, const QString& name) {
    for (const Parser::PcadPadStyle& style : board.padStyles) {
        if (style.name == name)
            return &style;
    }
    return nullptr;
}

/** @brief 将 P-CAD 封装图元映射到现有 Footprint IR 图元。 */
void appendGraphic(const Parser::PcadGraphic& source,
                   IR::FootprintComponentIR& result,
                   Parser::ParseDiagnostics* diagnostics) {
    const IR::LayerType graphicLayer = layer(source.layerNumber, diagnostics);
    // 图元类型映射只使用 IR 语义，不把 P-CAD 节点名称泄漏到 IR。
    switch (source.type) {
        case Parser::PcadGraphicType::Line: {
            if (source.points.size() < 2)
                break;
            IR::FootprintTrackIR track;
            track.points = source.points;
            track.width = source.width;
            track.layer = graphicLayer;
            result.tracks.append(track);
            break;
        }
        case Parser::PcadGraphicType::Arc: {
            IR::FootprintArcIR arc;
            arc.center = source.center;
            arc.radius = source.radius;
            arc.startAngle = source.startAngle;
            arc.endAngle = source.startAngle + source.sweepAngle;
            arc.width = source.width;
            arc.layer = graphicLayer;
            result.arcs.append(arc);
            break;
        }
        case Parser::PcadGraphicType::Circle: {
            IR::FootprintCircleIR circle;
            circle.center = source.center;
            circle.radius = source.radius;
            circle.strokeWidth = source.width;
            circle.layer = graphicLayer;
            result.circles.append(circle);
            break;
        }
        case Parser::PcadGraphicType::Polygon: {
            if (source.points.size() < 3)
                break;
            IR::FootprintRegionIR region;
            region.vertices = source.points;
            region.layer = graphicLayer;
            result.regions.append(region);
            break;
        }
        case Parser::PcadGraphicType::Text: {
            IR::FootprintTextIR text;
            text.text = source.text;
            text.position = source.center;
            text.fontSize = source.radius;
            text.strokeWidth = source.width;
            text.layer = graphicLayer;
            result.texts.append(text);
            break;
        }
        case Parser::PcadGraphicType::Unknown:
            if (diagnostics)
                diagnostics->add(Parser::ParseSeverity::Skipped,
                                 Parser::ParseScope::Footprint,
                                 QStringLiteral("跳过未支持的 P-CAD 图元"));
            break;
    }
}

}  // namespace

/** @brief 转换 P-CAD 全部 Pattern 并校验板级 Pattern 引用。 */
PcadConversionResult PcadAdapter::toIR(const Parser::PcadBoard& board, Parser::ParseDiagnostics* diagnostics) {
    PcadConversionResult result;
    for (const Parser::PcadPattern& pattern : board.patterns)
        result.footprints.append(toFootprint(board, pattern, diagnostics));
    result.placements = board.placements;
    result.boardGraphics = board.graphics;
    for (const Parser::PcadPlacement& placement : board.placements) {
        bool found = false;
        for (const Parser::PcadPattern& pattern : board.patterns) {
            if (pattern.name == placement.patternName) {
                found = true;
                break;
            }
        }
        if (!found && diagnostics)
            diagnostics->add(Parser::ParseSeverity::Error,
                             Parser::ParseScope::Component,
                             QStringLiteral("P-CAD 器件引用了不存在的 Pattern"),
                             placement.patternName);
    }
    return result;
}

/** @brief 将单个 P-CAD Pattern 映射为封装 IR。 */
IR::FootprintComponentIR PcadAdapter::toFootprint(const Parser::PcadBoard& board,
                                                  const Parser::PcadPattern& pattern,
                                                  Parser::ParseDiagnostics* diagnostics) {
    IR::FootprintComponentIR result;
    result.name = pattern.name;
    for (const Parser::PcadPatternPad& source : pattern.pads) {
        const Parser::PcadPadStyle* style = findStyle(board, source.padStyleName);
        if (!style) {
            if (diagnostics)
                diagnostics->add(Parser::ParseSeverity::Error,
                                 Parser::ParseScope::Footprint,
                                 QStringLiteral("P-CAD Pattern 引用了不存在的 Pad Style"),
                                 source.padStyleName);
            continue;
        }
        IR::FootprintPadIR pad;
        pad.number = source.number;
        pad.position = source.position;
        pad.rotation = source.rotation;
        pad.size = QSizeF(style->width, style->height);
        pad.layer = IR::LayerType::TopCopper;
        pad.isPlated = style->holePlated;
        if (style->holeDiameter > 0.0) {
            pad.padType = IR::PadType::ThroughHole;
            pad.layer = IR::LayerType::MultiLayer;
            pad.holeSize = style->holeDiameter;
        }
        // 将 Pad Style 形状映射为统一焊盘枚举。
        switch (style->shape) {
            case Parser::PcadPadShape::Round:
                pad.shape = IR::PadShape::Ellipse;
                break;
            case Parser::PcadPadShape::Oval:
                pad.shape = IR::PadShape::Oval;
                break;
            case Parser::PcadPadShape::RoundRectangle:
                pad.shape = IR::PadShape::RoundRect;
                break;
            case Parser::PcadPadShape::Rectangle:
                pad.shape = IR::PadShape::Rect;
                break;
            case Parser::PcadPadShape::Unknown:
                pad.shape = IR::PadShape::Rect;
                if (diagnostics)
                    diagnostics->add(Parser::ParseSeverity::Warning,
                                     Parser::ParseScope::Footprint,
                                     QStringLiteral("P-CAD 未知焊盘形状已降级为矩形"),
                                     style->name);
                break;
        }
        result.pads.append(pad);
    }
    for (const Parser::PcadGraphic& graphic : pattern.graphics)
        appendGraphic(graphic, result, diagnostics);
    return result;
}

}  // namespace EasyKiConverter
