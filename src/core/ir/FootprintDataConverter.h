#pragma once

/**
 * @file FootprintDataConverter.h
 * @brief FootprintData -> FootprintComponentIR 转换桥接
 *
 * 提供从旧模型 FootprintData 到 IR FootprintComponentIR 的转换函数。
 * 负责：
 * - 焊盘形状字符串 -> PadShape 枚举（EasyedaPadShapeMap）
 * - 层 ID 整数 -> LayerType 枚举（EasyedaLayerMap）
 * - 坐标字符串解析
 * - SVG 路径解析
 * - holeRadius > 0 -> PadType 枚举
 */

#include "EasyedaLayerMap.h"
#include "EasyedaPadShapeMap.h"
#include "FootprintIR.h"
#include "GeometryNormalizer.h"  // 复用 parseFlatPointString 等工具函数
#include "Model3DDataConverter.h"
#include "models/FootprintData.h"

#include <cmath>

namespace EasyKiConverter {
namespace IR {

// 从 GeometryNormalizer 引入坐标解析工具（向后兼容）
using GeometryNormalizer::parseCommaSeparatedPoints;
using GeometryNormalizer::parseFlatPointString;
using GeometryNormalizer::parseSimpleSvgPath;

/**
 * @brief SVG 弧线解析结果
 */
struct SvgArcResult {
    QPointF center;  ///< 圆心（已缩放）
    double radius = 0.0;  ///< 半径（已缩放）
    double startAngle = 0.0;  ///< 起始角度（度，0-360）
    double endAngle = 0.0;  ///< 终止角度（度，0-360）
};

/**
 * @brief 从 SVG 弧线路径字符串解析圆心和角度
 * @param pathStr SVG 路径字符串
 * @param scaleFactor 坐标缩放因子
 * @return 解析结果，失败时 center 为 (0,0) 且 radius=0
 */
inline SvgArcResult parseSvgArcPath(const QString& pathStr, double scaleFactor = EASYEDA_PX_TO_MM) {
    SvgArcResult result;
    if (pathStr.isEmpty())
        return result;

    // 提取所有 token（保留命令字母）
    QStringList tokens;
    QString current;
    for (const QChar& ch : pathStr) {
        if (ch.isLetter() || ch == '-') {
            if (!current.isEmpty()) {
                tokens.append(current.trimmed());
                current.clear();
            }
            if (ch == '-') {
                current = ch;
            } else {
                tokens.append(QString(ch));
            }
        } else if (ch == ',' || ch.isSpace()) {
            if (!current.isEmpty()) {
                tokens.append(current.trimmed());
                current.clear();
            }
        } else {
            current += ch;
        }
    }
    if (!current.isEmpty()) {
        tokens.append(current.trimmed());
    }

    // 找到 M 和 A 命令
    double startX = 0, startY = 0;
    double rx = 0, ry = 0, xRot = 0;
    int largeArc = 0, sweep = 0;
    double endX = 0, endY = 0;
    bool hasM = false, hasA = false;

    for (int i = 0; i < tokens.size(); ++i) {
        QChar cmd = tokens[i][0].toUpper();
        if (cmd == 'M' && i + 2 < tokens.size()) {
            startX = tokens[i + 1].toDouble();
            startY = tokens[i + 2].toDouble();
            hasM = true;
            i += 2;
        } else if (cmd == 'A' && i + 7 < tokens.size()) {
            rx = tokens[i + 1].toDouble();
            ry = tokens[i + 2].toDouble();
            xRot = tokens[i + 3].toDouble();
            largeArc = tokens[i + 4].toInt();
            sweep = tokens[i + 5].toInt();
            endX = tokens[i + 6].toDouble();
            endY = tokens[i + 7].toDouble();
            hasA = true;
            i += 7;
        }
    }

    if (!hasM || !hasA || rx <= 0 || ry <= 0)
        return result;

    // SVG 弧线 → 圆心转换
    double phi = xRot * M_PI / 180.0;
    double dx = (startX - endX) / 2.0;
    double dy = (startY - endY) / 2.0;

    double x1 = std::cos(phi) * dx + std::sin(phi) * dy;
    double y1 = -std::sin(phi) * dx + std::cos(phi) * dy;

    double rxSq = rx * rx;
    double rySq = ry * ry;
    double x1Sq = x1 * x1;
    double y1Sq = y1 * y1;

    double temp = (rxSq * rySq - rxSq * y1Sq - rySq * x1Sq) / (rxSq * y1Sq + rySq * x1Sq);
    if (temp < 0)
        temp = 0;
    temp = std::sqrt(temp);

    double factor = (largeArc == sweep) ? -1.0 : 1.0;
    double cx1 = factor * temp * rx * y1 / ry;
    double cy1 = -factor * temp * ry * x1 / rx;

    double cx = std::cos(phi) * cx1 - std::sin(phi) * cy1 + (startX + endX) / 2.0;
    double cy = std::sin(phi) * cx1 + std::cos(phi) * cy1 + (startY + endY) / 2.0;

    // 计算起始角度和终止角度
    auto getAngle = [](double ux, double uy, double vx, double vy) -> double {
        double dot = ux * vx + uy * vy;
        double mag = std::sqrt(ux * ux + uy * uy) * std::sqrt(vx * vx + vy * vy);
        if (mag < 1e-12)
            return 0.0;
        double cosAngle = std::clamp(dot / mag, -1.0, 1.0);
        double angle = std::acos(cosAngle);
        if (ux * vy - uy * vx < 0)
            angle = -angle;
        return angle;
    };

    double startAngle = getAngle(1.0, 0.0, (x1 - cx1) / rx, (y1 - cy1) / ry);
    double deltaAngle = getAngle((x1 - cx1) / rx, (y1 - cy1) / ry, (-x1 - cx1) / rx, (-y1 - cy1) / ry);

    // 规范化
    while (startAngle < 0)
        startAngle += 2 * M_PI;
    while (startAngle >= 2 * M_PI)
        startAngle -= 2 * M_PI;

    if (sweep) {
        if (deltaAngle < 0)
            deltaAngle += 2 * M_PI;
    } else {
        if (deltaAngle > 0)
            deltaAngle -= 2 * M_PI;
    }

    double startDeg = startAngle * 180.0 / M_PI;
    double endDeg = (startAngle + deltaAngle) * 180.0 / M_PI;

    result.center = QPointF(cx * scaleFactor, cy * scaleFactor);
    result.radius = rx * scaleFactor;  // 简化：使用 rx 作为半径
    result.startAngle = startDeg;
    result.endAngle = endDeg;

    return result;
}

/**
 * @brief FootprintData -> FootprintComponentIR 转换
 * @param data 旧模型的封装数据
 * @return IR 的封装组件数据
 */
inline FootprintComponentIR toFootprintIR(const FootprintData& data) {
    FootprintComponentIR ir;

    const auto layerExpansion = [&data](int layerId) {
        for (const LayerDefinition& layer : data.layers()) {
            if (layer.layerId == layerId)
                return layer.expansion;
        }
        return 0.0;
    };

    // 通用元数据
    ir.name = data.info().name;
    ir.description = data.info().description;

    // 转换焊盘
    for (const auto& pad : data.pads()) {
        FootprintPadIR pir;
        pir.number = pad.number;
        pir.position = QPointF(pad.centerX * EASYEDA_PX_TO_MM, pad.centerY * EASYEDA_PX_TO_MM);
        pir.shape = EasyedaPadShapeMap::toPadShape(pad.shape);
        pir.size = QSizeF(pad.width * EASYEDA_PX_TO_MM, pad.height * EASYEDA_PX_TO_MM);
        pir.layer = EasyedaLayerMap::toLayerType(pad.layerId);
        pir.rotation = pad.rotation;
        pir.holeSize = pad.holeRadius * 2.0 * EASYEDA_PX_TO_MM;  // radius -> diameter
        pir.holeLength = pad.holeLength * EASYEDA_PX_TO_MM;
        pir.netName = pad.net;
        pir.isPlated = pad.isPlated;
        pir.isLocked = pad.isLocked;
        pir.padType = (pad.holeRadius > 0) ? PadType::ThroughHole : PadType::Smd;
        if (pir.isSmd()) {
            const bool bottom = pir.layer == LayerType::BottomCopper;
            pir.solderMaskExpansionMm = layerExpansion(bottom ? 8 : 7);
            // EasyEDA layer records expose paste expansion separately when present;
            // absent values intentionally remain zero instead of being inferred.
            pir.pasteMaskContractionMm = layerExpansion(bottom ? 6 : 5);
        }

        // 异形焊盘自定义形状
        if (pir.shape == PadShape::Polygon && !pad.points.isEmpty()) {
            pir.customShapePoints = parseFlatPointString(pad.points);
            // 注意：自定义形状点相对于焊盘中心，但不翻转 Y（与旧代码一致）
            for (auto& pt : pir.customShapePoints) {
                pt.setX(pt.x() - pad.centerX * EASYEDA_PX_TO_MM);
                pt.setY(pt.y() - pad.centerY * EASYEDA_PX_TO_MM);
            }
        }

        ir.pads.append(pir);
    }

    // 转换走线
    for (const auto& track : data.tracks()) {
        FootprintTrackIR tir;
        tir.points = parseFlatPointString(track.points);
        tir.width = track.strokeWidth * EASYEDA_PX_TO_MM;
        tir.layer = EasyedaLayerMap::toLayerType(track.layerId);
        tir.netName = track.net;
        tir.isLocked = track.isLocked;
        ir.tracks.append(tir);
    }

    // 转换安装孔
    for (const auto& hole : data.holes()) {
        FootprintHoleIR hir;
        hir.center = QPointF(hole.centerX * EASYEDA_PX_TO_MM, hole.centerY * EASYEDA_PX_TO_MM);
        hir.radius = hole.radius * EASYEDA_PX_TO_MM;
        hir.isLocked = hole.isLocked;
        ir.holes.append(hir);
    }

    // 转换圆
    for (const auto& circle : data.circles()) {
        FootprintCircleIR cir;
        cir.center = QPointF(circle.cx * EASYEDA_PX_TO_MM, circle.cy * EASYEDA_PX_TO_MM);
        cir.radius = circle.radius * EASYEDA_PX_TO_MM;
        cir.strokeWidth = circle.strokeWidth * EASYEDA_PX_TO_MM;
        cir.layer = EasyedaLayerMap::toLayerType(circle.layerId);
        cir.isLocked = circle.isLocked;
        ir.circles.append(cir);
    }

    // 转换矩形
    for (const auto& rect : data.rectangles()) {
        FootprintRectangleIR rir;
        rir.bounds = QRectF(rect.x * EASYEDA_PX_TO_MM,
                            rect.y * EASYEDA_PX_TO_MM,
                            rect.width * EASYEDA_PX_TO_MM,
                            rect.height * EASYEDA_PX_TO_MM);
        rir.strokeWidth = rect.strokeWidth * EASYEDA_PX_TO_MM;
        rir.layer = EasyedaLayerMap::toLayerType(rect.layerId);
        rir.isLocked = rect.isLocked;
        ir.rectangles.append(rir);
    }

    // 转换圆弧
    for (const auto& arc : data.arcs()) {
        FootprintArcIR air;
        // 从 SVG 弧线路径解析圆心、半径和角度
        SvgArcResult arcResult = parseSvgArcPath(arc.path);
        if (arcResult.radius > 0) {
            air.center = arcResult.center;
            air.radius = arcResult.radius;
            air.startAngle = arcResult.startAngle;
            air.endAngle = arcResult.endAngle;
        } else {
            // 使用 parseSimpleSvgPath 提取坐标点
            const QList<QPointF> arcPoints = parseSimpleSvgPath(arc.path);
            if (arcPoints.size() >= 3) {
                air.center = arcPoints[2];
                const double dx = arcPoints[0].x() - air.center.x();
                const double dy = arcPoints[0].y() - air.center.y();
                air.radius = qSqrt(dx * dx + dy * dy);
            }
        }
        air.width = arc.strokeWidth * EASYEDA_PX_TO_MM;
        air.layer = EasyedaLayerMap::toLayerType(arc.layerId);
        air.netName = arc.net;
        air.isLocked = arc.isLocked;
        ir.arcs.append(air);
    }

    // 转换文本
    for (const auto& text : data.texts()) {
        FootprintTextIR tir;
        tir.text = text.text;
        tir.position = QPointF(text.centerX * EASYEDA_PX_TO_MM, text.centerY * EASYEDA_PX_TO_MM);
        tir.rotation = text.rotation;
        tir.mirror = (text.mirror == "1" || text.mirror.toLower() == "true");
        tir.strokeWidth = text.strokeWidth * EASYEDA_PX_TO_MM;
        tir.fontSize = text.fontSize * EASYEDA_PX_TO_MM;
        tir.layer = EasyedaLayerMap::toLayerType(text.layerId);
        tir.isDisplayed = text.isDisplayed;
        tir.isFabrication = (text.type == "N");
        tir.isLocked = text.isLocked;

        // 非 ASCII 文本路径
        if (!text.textPath.isEmpty()) {
            tir.textPathPoints = parseSimpleSvgPath(text.textPath);
        }

        ir.texts.append(tir);
    }

    // 转换实心区域
    for (const auto& region : data.solidRegions()) {
        FootprintRegionIR rir;
        rir.vertices = parseSimpleSvgPath(region.path);
        rir.layer = EasyedaLayerMap::toLayerType(region.layerId);
        rir.isKeepOut = region.isKeepOut || (region.layerId == 99);
        rir.isLocked = region.isLocked;
        ir.regions.append(rir);
    }

    // 转换板框轮廓
    for (const auto& outline : data.outlines()) {
        // EasyEDA 的 layer 19 是 SVGNODE outline3D 的模型辅助轮廓，
        // 不是封装的二维 PCB 图元。若将其转成 Track，Altium 中会在
        // 未映射层上显示成黑色矩形/大面积黑色轮廓；3D 模型边界由
        // ExporterAltiumFootprint::generateComponentBody() 单独处理。
        if (outline.layerId == 19)
            continue;
        FootprintOutlineIR oir;
        oir.points = parseSimpleSvgPath(outline.path);
        oir.strokeWidth = outline.strokeWidth * EASYEDA_PX_TO_MM;
        oir.layer = EasyedaLayerMap::toLayerType(outline.layerId);
        oir.isLocked = outline.isLocked;
        ir.outlines.append(oir);
    }

    // 仅转换带有模型标识的有效三维数据，避免空模型污染目标格式的诊断和统计。
    const Model3DData model3D = data.model3D();
    if (model3D.isValid())
        ir.models3d.append(toModel3DIR(model3D));

    // 设置 courtyard 生成标志：当原始 bbox 有有效尺寸时生成
    const auto& bbox = data.bbox();
    ir.shouldGenerateCourtyard = (bbox.width > 0 && bbox.height > 0);

    return ir;
}

}  // namespace IR
}  // namespace EasyKiConverter
