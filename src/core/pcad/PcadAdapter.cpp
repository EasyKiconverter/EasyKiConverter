#include "PcadAdapter.h"

#include <QtMath>

namespace EasyKiConverter {
namespace {

/**
 * @brief 将 P-CAD 层号映射为 IR 层类型并记录不确定映射。
 * @param number P-CAD 源层号。
 * @param diagnostics 接收无法精确映射诊断的对象。
 * @return 对应的 IR 层类型。
 */
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

/**
 * @brief 按名称查找唯一 Pad Style，重复定义时返回空指针并记录错误。
 * @param board P-CAD 板模型。
 * @param name 待查找的 Pad Style 名称。
 * @param diagnostics 接收引用歧义诊断的对象。
 * @return 唯一匹配的 Pad Style，未找到或重复时返回空指针。
 */
const Parser::PcadPadStyle* findStyle(const Parser::PcadBoard& board,
                                      const QString& name,
                                      Parser::ParseDiagnostics* diagnostics) {
    // 保存唯一匹配的 Pad Style，重复时清晰报告歧义。
    const Parser::PcadPadStyle* result = nullptr;
    for (const Parser::PcadPadStyle& style : board.padStyles) {
        if (style.name != name)
            continue;
        if (result != nullptr) {
            if (diagnostics)
                diagnostics->add(Parser::ParseSeverity::Error,
                                 Parser::ParseScope::Footprint,
                                 QStringLiteral("P-CAD Pad Style 引用存在歧义"),
                                 name);
            return nullptr;
        }
        result = &style;
    }
    return result;
}

/**
 * @brief 统计名称匹配的 Pattern 数量，避免放置静默绑定到首个定义。
 * @param board P-CAD 板模型。
 * @param name 待统计的 Pattern 名称。
 * @return 匹配的 Pattern 数量。
 */
int patternMatches(const Parser::PcadBoard& board, const QString& name) {
    // 统计具有相同名称的 Pattern 定义数量。
    int matches = 0;
    for (const Parser::PcadPattern& pattern : board.patterns) {
        if (pattern.name == name)
            ++matches;
    }
    return matches;
}

/**
 * @brief 将 P-CAD 封装图元映射到现有 Footprint IR 图元。
 * @param source P-CAD 源图元。
 * @param result 接收封装 IR 图元的对象。
 * @param diagnostics 接收不支持几何诊断的对象。
 */
void appendGraphic(const Parser::PcadGraphic& source,
                   IR::FootprintComponentIR& result,
                   Parser::ParseDiagnostics* diagnostics) {
    // 将源层号转换为统一 IR 层语义。
    const IR::LayerType graphicLayer = layer(source.layerNumber, diagnostics);
    // 图元类型映射只使用 IR 语义，不把 P-CAD 节点名称泄漏到 IR。
    switch (source.type) {
        case Parser::PcadGraphicType::Line: {
            if (source.points.size() < 2) {
                if (diagnostics)
                    diagnostics->add(Parser::ParseSeverity::Error,
                                     Parser::ParseScope::Footprint,
                                     QStringLiteral("P-CAD 封装线段缺少两个端点"));
                break;
            }
            IR::FootprintTrackIR track;  // 统一 IR 线段图元。
            track.points = source.points;
            track.width = source.width;
            track.layer = graphicLayer;
            result.tracks.append(track);
            break;
        }
        case Parser::PcadGraphicType::Arc: {
            IR::FootprintArcIR arc;  // 统一 IR 圆弧图元。
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
            IR::FootprintCircleIR circle;  // 统一 IR 圆图元。
            circle.center = source.center;
            circle.radius = source.radius;
            circle.strokeWidth = source.width;
            circle.layer = graphicLayer;
            result.circles.append(circle);
            break;
        }
        case Parser::PcadGraphicType::Polygon: {
            if (source.points.size() < 3) {
                if (diagnostics)
                    diagnostics->add(Parser::ParseSeverity::Error,
                                     Parser::ParseScope::Footprint,
                                     QStringLiteral("P-CAD 封装多边形缺少三个顶点"));
                break;
            }
            IR::FootprintRegionIR region;  // 统一 IR 区域图元。
            region.vertices = source.points;
            region.layer = graphicLayer;
            result.regions.append(region);
            break;
        }
        case Parser::PcadGraphicType::Text: {
            IR::FootprintTextIR text;  // 统一 IR 文本图元。
            text.text = source.text;
            text.position = source.center;
            text.fontSize = source.textHeight;
            text.rotation = source.rotation;
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

/**
 * @brief 将 P-CAD 板级图元转换为通用板级 IR。
 * @param source P-CAD 源图元。
 * @param result 接收板级 IR 图元的对象。
 * @param diagnostics 接收不支持几何诊断的对象。
 */
void appendBoardGraphic(const Parser::PcadGraphic& source, IR::BoardIR& result, Parser::ParseDiagnostics* diagnostics) {
    // 将源层号转换为统一 IR 层语义。
    const IR::LayerType graphicLayer = layer(source.layerNumber, diagnostics);
    // 按图元语义写入通用 IR，避免把来源格式的枚举和节点结构带出适配层。
    switch (source.type) {
        case Parser::PcadGraphicType::Line: {
            if (source.points.size() < 2) {
                if (diagnostics)
                    diagnostics->add(Parser::ParseSeverity::Error,
                                     Parser::ParseScope::File,
                                     QStringLiteral("P-CAD 板级线段缺少两个端点"));
                break;
            }
            IR::FootprintTrackIR track;  // 统一 IR 线段图元。
            track.points = source.points;
            track.width = source.width;
            track.layer = graphicLayer;
            result.tracks.append(track);
            break;
        }
        case Parser::PcadGraphicType::Arc: {
            IR::FootprintArcIR arc;  // 统一 IR 圆弧图元。
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
            IR::FootprintCircleIR circle;  // 统一 IR 圆图元。
            circle.center = source.center;
            circle.radius = source.radius;
            circle.strokeWidth = source.width;
            circle.layer = graphicLayer;
            result.circles.append(circle);
            break;
        }
        case Parser::PcadGraphicType::Polygon: {
            if (source.points.size() < 3) {
                if (diagnostics)
                    diagnostics->add(Parser::ParseSeverity::Error,
                                     Parser::ParseScope::File,
                                     QStringLiteral("P-CAD 板级多边形缺少三个顶点"));
                break;
            }
            IR::FootprintRegionIR region;  // 统一 IR 区域图元。
            region.vertices = source.points;
            region.layer = graphicLayer;
            result.regions.append(region);
            break;
        }
        case Parser::PcadGraphicType::Text: {
            IR::FootprintTextIR text;  // 统一 IR 文本图元。
            text.text = source.text;
            text.position = source.center;
            text.fontSize = source.textHeight;
            text.rotation = source.rotation;
            text.strokeWidth = source.width;
            text.layer = graphicLayer;
            result.texts.append(text);
            break;
        }
        case Parser::PcadGraphicType::Unknown:
            if (diagnostics)
                diagnostics->add(Parser::ParseSeverity::Skipped,
                                 Parser::ParseScope::Footprint,
                                 QStringLiteral("跳过未支持的 P-CAD 板级图元"));
            break;
    }
}

}  // namespace

/**
 * @brief 转换 P-CAD 全部 Pattern 并校验板级 Pattern 引用。
 * @param board P-CAD 板模型。
 * @param diagnostics 接收转换诊断的对象。
 * @return 封装、放置和板级图形的统一转换结果。
 */
PcadConversionResult PcadAdapter::toIR(const Parser::PcadBoard& board, Parser::ParseDiagnostics* diagnostics) {
    // 保存全部封装、放置和板级图形的转换结果。
    PcadConversionResult result;
    for (const Parser::PcadPattern& pattern : board.patterns)
        result.footprints.append(toFootprint(board, pattern, diagnostics));
    for (const Parser::PcadPlacement& sourcePlacement : board.placements) {
        IR::FootprintPlacementIR placement;  // 当前器件的统一放置信息。
        placement.reference = sourcePlacement.reference;
        placement.footprintName = sourcePlacement.patternName;
        placement.position = sourcePlacement.position;
        placement.rotation = sourcePlacement.rotation;
        placement.mirrored = sourcePlacement.flipped;
        result.placements.append(placement);
    }
    for (const Parser::PcadGraphic& graphic : board.graphics)
        appendBoardGraphic(graphic, result.board, diagnostics);
    for (const Parser::PcadPlacement& placement : board.placements) {
        bool found = false;  // 标记 Pattern 引用是否唯一有效。
        const int matches = patternMatches(board, placement.patternName);  // 统计匹配的 Pattern 数量。
        found = matches == 1;
        if (matches > 1 && diagnostics)
            diagnostics->add(Parser::ParseSeverity::Error,
                             Parser::ParseScope::Component,
                             QStringLiteral("P-CAD 器件引用的 Pattern 存在歧义"),
                             placement.patternName);
        if (!found && matches == 0 && diagnostics)
            diagnostics->add(Parser::ParseSeverity::Error,
                             Parser::ParseScope::Component,
                             QStringLiteral("P-CAD 器件引用了不存在的 Pattern"),
                             placement.patternName);
    }
    return result;
}

/**
 * @brief 将单个 P-CAD Pattern 映射为封装 IR。
 * @param board 用于解析 Pad Style 和层语义的 P-CAD 模型。
 * @param pattern P-CAD Pattern。
 * @param diagnostics 接收转换诊断的对象。
 * @return 统一封装 IR。
 */
IR::FootprintComponentIR PcadAdapter::toFootprint(const Parser::PcadBoard& board,
                                                  const Parser::PcadPattern& pattern,
                                                  Parser::ParseDiagnostics* diagnostics) {
    // 保存当前 Pattern 的统一封装 IR。
    IR::FootprintComponentIR result;
    result.name = pattern.name;
    for (const Parser::PcadPatternPad& source : pattern.pads) {
        // 解析焊盘引用的 Pad Style，无法唯一解析时拒绝该焊盘。
        const Parser::PcadPadStyle* style = findStyle(board, source.padStyleName, diagnostics);
        if (!style) {
            if (diagnostics)
                diagnostics->add(Parser::ParseSeverity::Error,
                                 Parser::ParseScope::Footprint,
                                 QStringLiteral("P-CAD Pattern 引用了不存在的 Pad Style"),
                                 source.padStyleName);
            continue;
        }
        IR::FootprintPadIR pad;  // 当前焊盘的统一 IR 描述。
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
        bool shapeSupported = true;  // 标记 Pad Style 形状是否可映射。
        // 未知形状不猜测替代几何，避免制造语义被静默改变。
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
                shapeSupported = false;
                if (diagnostics)
                    diagnostics->add(Parser::ParseSeverity::Error,
                                     Parser::ParseScope::Footprint,
                                     QStringLiteral("P-CAD 未知焊盘形状无法映射，已跳过焊盘"),
                                     style->name);
                break;
        }
        if (!shapeSupported)
            continue;
        result.pads.append(pad);
    }
    for (const Parser::PcadGraphic& graphic : pattern.graphics)
        appendGraphic(graphic, result, diagnostics);
    return result;
}

}  // namespace EasyKiConverter
