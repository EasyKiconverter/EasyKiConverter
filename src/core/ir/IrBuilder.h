#pragma once

/**
 * @file IrBuilder.h
 * @brief SymbolData -> SymbolComponentIR 的 IR 构建器
 *
 * 从 SymbolDataConverter.h 中提取的 IR 构建逻辑。
 * 负责：
 * - 引脚类型/方向映射（EasyedaPinTypeMap）
 * - 引脚样式/显示字段填充（SymbolPinStyle / SymbolPinDisplay）
 * - 图元坐标变换和 IR 构建
 * - EasyEDA 特有字段 -> sourceMetadata
 * - 多部件符号原点计算
 *
 * 不包含坐标解析工具函数（这些在 GeometryNormalizer.h 中）。
 */

#include "EasyedaPinTypeMap.h"
#include "GeometryNormalizer.h"
#include "SymbolIR.h"
#include "models/SymbolData.h"

#include <QRegularExpression>

#include <algorithm>
#include <cmath>
#include <limits>

namespace EasyKiConverter {
namespace IR {

/**
 * @brief SymbolData -> SymbolComponentIR 转换
 * @param data 旧模型的符号数据
 * @return IR 的符号组件数据
 */
inline SymbolComponentIR toSymbolIR(const SymbolData& data) {
    SymbolComponentIR ir;  // 待填充的符号组件 IR。

    // 通用元数据
    ir.name = data.info().name;
    ir.description = data.info().description;
    ir.designatorPrefix = data.info().prefix;
    ir.aliases = data.info().aliases;

    // 封装关联
    ir.footprintName = data.info().package;
    if (!ir.footprintName.isEmpty())
        ir.footprintNames.append(ir.footprintName);

    // EasyEDA 特有字段 -> sourceMetadata
    if (!data.info().lcscId.isEmpty())
        ir.sourceMetadata["lcscId"] = data.info().lcscId;
    if (!data.info().manufacturer.isEmpty())
        ir.sourceMetadata["manufacturer"] = data.info().manufacturer;
    if (!data.info().datasheet.isEmpty())
        ir.sourceMetadata["datasheet"] = data.info().datasheet;
    if (!data.info().jlcId.isEmpty())
        ir.sourceMetadata["jlcId"] = data.info().jlcId;
    if (!data.info().uuid.isEmpty())
        ir.sourceMetadata["uuid"] = data.info().uuid;
    if (!data.info().docType.isEmpty())
        ir.sourceMetadata["docType"] = data.info().docType;
    if (!data.info().datastrid.isEmpty())
        ir.sourceMetadata["datastrid"] = data.info().datastrid;
    if (!data.info().supplierPart.isEmpty())
        ir.sourceMetadata["supplierPart"] = data.info().supplierPart;
    if (!data.info().supplier.isEmpty())
        ir.sourceMetadata["supplier"] = data.info().supplier;
    if (!data.info().manufacturerPart.isEmpty())
        ir.sourceMetadata["manufacturerPart"] = data.info().manufacturerPart;
    if (!data.info().jlcpcbPartClass.isEmpty())
        ir.sourceMetadata["jlcpcbPartClass"] = data.info().jlcpcbPartClass;

    // 转换辅助 lambda：处理单个 part 的引脚
    auto convertPins = [&](const QList<SymbolPin>& pins,
                           double originX,
                           double originY,
                           int partIdx = 0,
                           bool commonToAllParts = false) {
        for (const auto& pin : pins) {
            SymbolPinIR pir;  // 当前引脚的符号 IR。
            pir.name = pin.name.text;
            pir.designator = pin.settings.spicePinNumber;
            pir.position = GeometryNormalizer::transformPoint(pin.settings.posX, pin.settings.posY, originX, originY);
            // 只有显示且坐标有限的名称文本才使用独立位置。
            const bool hasNamePosition = pin.name.isDisplayed &&
                                         !pin.name.text.isEmpty() &&  // 是否有有效的引脚名称坐标。
                                         std::isfinite(pin.name.posX) && std::isfinite(pin.name.posY);
            pir.namePosition = hasNamePosition
                                   ? GeometryNormalizer::transformPoint(pin.name.posX, pin.name.posY, originX, originY)
                                   : pir.position;
            pir.nameRotation = pin.name.rotation;
            pir.nameAnchor = pin.name.textAnchor;
            pir.nameFontSizeMm = pin.name.fontSize * EASYEDA_PX_TO_MM;
            pir.hasNamePosition = hasNamePosition;
            // 只有显示且坐标有限的编号文本才使用独立位置。
            const bool hasNumberPosition = pin.number.isDisplayed &&
                                           !pin.number.text.isEmpty() &&  // 是否有有效的引脚编号坐标。
                                           std::isfinite(pin.number.posX) && std::isfinite(pin.number.posY);
            pir.numberPosition = hasNumberPosition ? GeometryNormalizer::transformPoint(
                                                         pin.number.posX, pin.number.posY, originX, originY)
                                                   : pir.position;
            pir.numberRotation = pin.number.rotation;
            pir.numberAnchor = pin.number.textAnchor;
            pir.numberFontSizeMm = pin.number.fontSize * EASYEDA_PX_TO_MM;
            pir.hasNumberPosition = hasNumberPosition;

            // 从 pinPath SVG 解析引脚长度
            const QString pathStr = pin.pinPath.path;  // 引脚 SVG 路径文本。
            static const QRegularExpression lengthExpression(
                QStringLiteral(R"([hHvV]\s*([-+]?(?:\d+\.?\d*|\.\d+)))"));  // 引脚长度匹配表达式。
            const QRegularExpressionMatch lengthMatch = lengthExpression.match(pathStr);  // 匹配到的长度字段。
            if (lengthMatch.hasMatch()) {
                bool ok = false;  // 长度字段是否解析成功。
                const double len = lengthMatch.captured(1).toDouble(&ok);  // SVG 路径中的原始长度。
                if (ok)
                    pir.length = qAbs(len) * EASYEDA_PX_TO_MM;
            }

            pir.direction = EasyedaPinTypeMap::toPinDirection(pin.settings.rotation);
            pir.electricalType = EasyedaPinTypeMap::toPinElectricalType(static_cast<int>(pin.settings.type));

            // 显示控制
            pir.showName = pin.name.isDisplayed;
            pir.showDesignator = pin.settings.isDisplayed;
            pir.display.showName = pin.name.isDisplayed;
            pir.display.showDesignator = pin.settings.isDisplayed;

            // 引脚样式（语义层）
            pir.hasDot = pin.dot.isDisplayed;
            pir.hasClock = pin.clock.isDisplayed;
            pir.style.inverted = pin.dot.isDisplayed;
            pir.style.clock = pin.clock.isDisplayed;
            if (pin.dot.isDisplayed && pin.clock.isDisplayed) {
                pir.style.decoration = PinDecoration::InvertedClock;
            } else if (pin.dot.isDisplayed) {
                pir.style.decoration = PinDecoration::Dot;
            } else if (pin.clock.isDisplayed) {
                pir.style.decoration = PinDecoration::Clock;
            }

            pir.partIndex = commonToAllParts ? -1 : partIdx;
            pir.commonToAllParts = commonToAllParts;
            ir.pins.append(pir);
        }
    };

    // 转换辅助 lambda：处理矩形
    auto convertRectangles = [&](const QList<SymbolRectangle>& rects, double originX, double originY, int partIdx = 0) {
        for (const auto& rect : rects) {
            SymbolRectangleIR rir;  // 当前矩形的符号 IR。
            rir.x0 = (rect.posX - originX) * EASYEDA_PX_TO_MM;
            rir.y0 = -(rect.posY - originY) * EASYEDA_PX_TO_MM;
            rir.x1 = (rect.posX + rect.width - originX) * EASYEDA_PX_TO_MM;
            rir.y1 = -(rect.posY + rect.height - originY) * EASYEDA_PX_TO_MM;
            rir.cornerRadiusX = rect.rx * EASYEDA_PX_TO_MM;
            rir.cornerRadiusY = rect.ry * EASYEDA_PX_TO_MM;
            rir.strokeColor = parseColor(rect.strokeColor);
            rir.strokeWidth = rect.strokeWidth * EASYEDA_PX_TO_MM;
            rir.strokeStyle = GeometryNormalizer::parseStrokeStyle(rect.strokeStyle);
            rir.isFilled = !rect.fillColor.isEmpty() && rect.fillColor != "none";
            if (rir.isFilled)
                rir.fillColor = parseColor(rect.fillColor);
            rir.partIndex = partIdx;
            ir.rectangles.append(rir);
        }
    };

    // 转换辅助 lambda：处理圆形
    auto convertCircles = [&](const QList<SymbolCircle>& circles, double originX, double originY, int partIdx = 0) {
        for (const auto& circle : circles) {
            SymbolCircleIR cir;  // 当前圆形的符号 IR。
            cir.center =
                QPointF((circle.centerX - originX) * EASYEDA_PX_TO_MM, -(circle.centerY - originY) * EASYEDA_PX_TO_MM);
            cir.radius = circle.radius * EASYEDA_PX_TO_MM;
            cir.strokeColor = parseColor(circle.strokeColor);
            cir.strokeWidth = circle.strokeWidth * EASYEDA_PX_TO_MM;
            cir.strokeStyle = GeometryNormalizer::parseStrokeStyle(circle.strokeStyle);
            cir.isFilled = circle.fillColor;
            cir.partIndex = partIdx;
            ir.circles.append(cir);
        }
    };

    // 转换辅助 lambda：处理椭圆
    auto convertEllipses = [&](const QList<SymbolEllipse>& ellipses, double originX, double originY, int partIdx = 0) {
        for (const auto& ellipse : ellipses) {
            SymbolEllipseIR eir;  // 当前椭圆的符号 IR。
            eir.center = QPointF((ellipse.centerX - originX) * EASYEDA_PX_TO_MM,
                                 -(ellipse.centerY - originY) * EASYEDA_PX_TO_MM);
            eir.radiusX = ellipse.radiusX * EASYEDA_PX_TO_MM;
            eir.radiusY = ellipse.radiusY * EASYEDA_PX_TO_MM;
            eir.strokeColor = parseColor(ellipse.strokeColor);
            eir.strokeWidth = ellipse.strokeWidth * EASYEDA_PX_TO_MM;
            eir.strokeStyle = GeometryNormalizer::parseStrokeStyle(ellipse.strokeStyle);
            eir.isFilled = ellipse.fillColor;
            eir.partIndex = partIdx;
            ir.ellipses.append(eir);
        }
    };

    // 转换辅助 lambda：处理折线
    auto convertPolylines =
        [&](const QList<SymbolPolyline>& polylines, double originX, double originY, int partIdx = 0) {
            for (const auto& pl : polylines) {
                SymbolPolylineIR plir;  // 当前折线的符号 IR。
                plir.points = GeometryNormalizer::parseFlatPointString(pl.points);
                QPointF originMm(originX * EASYEDA_PX_TO_MM, originY * EASYEDA_PX_TO_MM);  // 已缩放的局部原点。
                plir.points = GeometryNormalizer::transformPoints(plir.points, originMm);
                plir.strokeColor = parseColor(pl.strokeColor);
                plir.strokeWidth = pl.strokeWidth * EASYEDA_PX_TO_MM;
                plir.strokeStyle = GeometryNormalizer::parseStrokeStyle(pl.strokeStyle);
                plir.isFilled = pl.fillColor;
                plir.partIndex = partIdx;
                ir.polylines.append(plir);
            }
        };

    // 转换辅助 lambda：处理多边形
    auto convertPolygons = [&](const QList<SymbolPolygon>& polygons, double originX, double originY, int partIdx = 0) {
        for (const auto& pg : polygons) {
            SymbolPolygonIR pgir;  // 当前多边形的符号 IR。
            pgir.points = GeometryNormalizer::parseFlatPointString(pg.points);
            QPointF originMm(originX * EASYEDA_PX_TO_MM, originY * EASYEDA_PX_TO_MM);  // 已缩放的局部原点。
            pgir.points = GeometryNormalizer::transformPoints(pgir.points, originMm);
            pgir.strokeColor = parseColor(pg.strokeColor);
            pgir.strokeWidth = pg.strokeWidth * EASYEDA_PX_TO_MM;
            pgir.strokeStyle = GeometryNormalizer::parseStrokeStyle(pg.strokeStyle);
            pgir.isFilled = pg.fillColor;
            pgir.partIndex = partIdx;
            ir.polygons.append(pgir);
        }
    };

    // 转换辅助 lambda：处理路径
    auto convertPaths = [&](const QList<SymbolPath>& paths, double originX, double originY, int partIdx = 0) {
        for (const auto& path : paths) {
            SymbolPathIR pathIR;  // 当前路径的符号 IR。
            pathIR.points = GeometryNormalizer::parseSimpleSvgPath(path.paths);
            QPointF originMm(originX * EASYEDA_PX_TO_MM, originY * EASYEDA_PX_TO_MM);  // 已缩放的局部原点。
            pathIR.points = GeometryNormalizer::transformPoints(pathIR.points, originMm);
            const auto transformSegmentPoint = [&](const QPointF& point) {
                return GeometryNormalizer::transformPoints({point * EASYEDA_PX_TO_MM}, originMm).value(0);
            };
            for (const SvgPathSegment& sourceSegment : SvgPathParser::parseSegments(path.paths)) {
                SymbolPathSegmentIR segment;  // 当前路径片段的符号 IR。
                if (sourceSegment.type == SvgPathSegment::Type::QuadraticBezier)
                    segment.type = SymbolPathSegmentIR::Type::QuadraticBezier;
                else if (sourceSegment.type == SvgPathSegment::Type::CubicBezier)
                    segment.type = SymbolPathSegmentIR::Type::CubicBezier;
                else if (sourceSegment.type == SvgPathSegment::Type::CircularArc)
                    segment.type = SymbolPathSegmentIR::Type::CircularArc;
                else if (sourceSegment.type == SvgPathSegment::Type::EllipticalArc)
                    segment.type = SymbolPathSegmentIR::Type::EllipticalArc;
                else
                    segment.type = SymbolPathSegmentIR::Type::Line;
                segment.start = transformSegmentPoint(sourceSegment.start);
                segment.end = transformSegmentPoint(sourceSegment.end);
                if (segment.type == SymbolPathSegmentIR::Type::QuadraticBezier ||
                    segment.type == SymbolPathSegmentIR::Type::CubicBezier) {
                    segment.control1 = transformSegmentPoint(sourceSegment.control1);
                }
                if (segment.type == SymbolPathSegmentIR::Type::CubicBezier) {
                    segment.control2 = transformSegmentPoint(sourceSegment.control2);
                } else if (segment.type == SymbolPathSegmentIR::Type::CircularArc) {
                    segment.arcMid = transformSegmentPoint(sourceSegment.arcMid);
                } else if (segment.type == SymbolPathSegmentIR::Type::EllipticalArc) {
                    segment.arcCenter = transformSegmentPoint(sourceSegment.arcCenter);
                    segment.radiusX = sourceSegment.radiusX * EASYEDA_PX_TO_MM;
                    segment.radiusY = sourceSegment.radiusY * EASYEDA_PX_TO_MM;
                    segment.arcStartAngle = -sourceSegment.arcStartAngle;
                    segment.arcEndAngle = -sourceSegment.arcEndAngle;
                }
                pathIR.segments.append(segment);
            }
            // SVG Z 命令闭合路径
            if (path.paths.contains('Z', Qt::CaseInsensitive) && pathIR.points.size() >= 2) {
                if (pathIR.points.first() != pathIR.points.last()) {
                    pathIR.points.append(pathIR.points.first());
                }
            }
            pathIR.strokeColor = parseColor(path.strokeColor);
            pathIR.strokeWidth = path.strokeWidth * EASYEDA_PX_TO_MM;
            pathIR.strokeStyle = GeometryNormalizer::parseStrokeStyle(path.strokeStyle);
            pathIR.isFilled = path.fillColor;
            pathIR.partIndex = partIdx;
            ir.paths.append(pathIR);
        }
    };

    // 转换图片。嵌入 data 保留原始字节，外部资源保留源 URL/文件名。
    auto convertImages = [&](const QList<SymbolImage>& images, double originX, double originY, int partIdx = 0) {
        for (const auto& image : images) {
            SymbolImageIR imageIR;  // 当前图片的符号 IR。
            imageIR.x0 = (image.posX - originX) * EASYEDA_PX_TO_MM;
            imageIR.y0 = -(image.posY - originY) * EASYEDA_PX_TO_MM;
            imageIR.x1 = (image.posX + image.width - originX) * EASYEDA_PX_TO_MM;
            imageIR.y1 = -(image.posY + image.height - originY) * EASYEDA_PX_TO_MM;
            imageIR.rotation = image.rotation;
            imageIR.fileName = image.data.isEmpty() ? image.source : image.fileName;
            imageIR.data = image.data;
            imageIR.partIndex = partIdx;
            ir.images.append(imageIR);
        }
    };

    // 转换辅助 lambda：处理文本
    auto convertTexts = [&](const QList<SymbolText>& texts, double originX, double originY, int partIdx = 0) {
        for (const auto& text : texts) {
            SymbolTextIR tir;  // 当前文本的符号 IR。
            tir.text = text.text;
            tir.position = QPointF((text.posX - originX) * EASYEDA_PX_TO_MM, -(text.posY - originY) * EASYEDA_PX_TO_MM);
            tir.rotation = text.rotation;
            tir.color = parseColor(text.color);
            tir.fontFamily = text.font;
            tir.fontSizeMm = text.textSize * EASYEDA_PT_TO_MM;
            tir.bold = text.bold;
            tir.italic = (text.italic == "1" || text.italic == "Italic" || text.italic == "italic");
            tir.visible = text.visible;
            tir.anchor = text.anchor.trimmed().isEmpty() ? QStringLiteral("middle") : text.anchor.trimmed();
            tir.partIndex = partIdx;
            ir.texts.append(tir);
        }
    };

    // 转换辅助 lambda：处理圆弧
    auto convertArcs = [&](const QList<SymbolArc>& arcs, double originX, double originY, int partIdx = 0) {
        for (const auto& arc : arcs) {
            SymbolArcIR air;  // 当前圆弧的符号 IR。
            if (arc.path.size() >= 3) {
                int midIdx = arc.path.size() / 2;  // 三点圆弧的中间点索引。
                air.startPoint = QPointF((arc.path.first().x() - originX) * EASYEDA_PX_TO_MM,
                                         -(arc.path.first().y() - originY) * EASYEDA_PX_TO_MM);
                air.midPoint = QPointF((arc.path[midIdx].x() - originX) * EASYEDA_PX_TO_MM,
                                       -(arc.path[midIdx].y() - originY) * EASYEDA_PX_TO_MM);
                air.endPoint = QPointF((arc.path.last().x() - originX) * EASYEDA_PX_TO_MM,
                                       -(arc.path.last().y() - originY) * EASYEDA_PX_TO_MM);
            } else if (arc.path.size() == 2) {
                air.startPoint = QPointF((arc.path.first().x() - originX) * EASYEDA_PX_TO_MM,
                                         -(arc.path.first().y() - originY) * EASYEDA_PX_TO_MM);
                QPointF mid = (arc.path.first() + arc.path.last()) / 2.0;  // 两点弧线的几何中点。
                air.midPoint = QPointF((mid.x() - originX) * EASYEDA_PX_TO_MM, -(mid.y() - originY) * EASYEDA_PX_TO_MM);
                air.endPoint = QPointF((arc.path.last().x() - originX) * EASYEDA_PX_TO_MM,
                                       -(arc.path.last().y() - originY) * EASYEDA_PX_TO_MM);
            }
            air.strokeColor = parseColor(arc.strokeColor);
            air.strokeWidth = arc.strokeWidth * EASYEDA_PX_TO_MM;
            air.strokeStyle = GeometryNormalizer::parseStrokeStyle(arc.strokeStyle);
            air.isFilled = arc.fillColor;
            air.partIndex = partIdx;
            ir.arcs.append(air);
        }
    };

    // 处理单部件或多部件符号
    if (data.isMultiPart()) {
        ir.partCount = qMax(
            1, static_cast<int>(std::count_if(data.parts().cbegin(), data.parts().cend(), [](const SymbolPart& part) {
                return !part.commonToAllParts;
            })));
        int partIdx = 0;  // 非公共部件的递增索引。
        for (const auto& part : data.parts()) {
            // 图形使用 part 的显式原点（通常为 0,0）
            const double gox = part.originX;  // 当前部件图形原点 X。
            const double goy = part.originY;  // 当前部件图形原点 Y。

            // 引脚使用 part 的几何边界框原点（与旧代码的 calculatePartBBox 一致）
            double pox = part.originX;  // 当前部件引脚原点 X。
            double poy = part.originY;  // 当前部件引脚原点 Y。
            {
                double minX = part.originX;  // 引脚/图形边界最小 X。
                double minY = part.originY;  // 引脚/图形边界最小 Y。
                for (const auto& pin : part.pins) {
                    minX = qMin(minX, pin.settings.posX);
                    minY = qMin(minY, pin.settings.posY);
                }
                for (const auto& rect : part.rectangles) {
                    minX = qMin(minX, rect.posX);
                    minY = qMin(minY, rect.posY);
                }
                for (const auto& circle : part.circles) {
                    minX = qMin(minX, circle.centerX - circle.radius);
                    minY = qMin(minY, circle.centerY - circle.radius);
                }
                for (const auto& pg : part.polygons) {
                    QStringList pts = pg.points.split(" ");  // 多边形原始坐标字段。
                    pts.removeAll("");
                    for (int i = 0; i + 1 < pts.size(); i += 2) {
                        minX = qMin(minX, pts[i].toDouble());
                        minY = qMin(minY, pts[i + 1].toDouble());
                    }
                }
                for (const auto& pl : part.polylines) {
                    QStringList pts = pl.points.split(" ");  // 折线原始坐标字段。
                    pts.removeAll("");
                    for (int i = 0; i + 1 < pts.size(); i += 2) {
                        minX = qMin(minX, pts[i].toDouble());
                        minY = qMin(minY, pts[i + 1].toDouble());
                    }
                }
                for (const auto& text : part.texts) {
                    minX = qMin(minX, text.posX);
                    minY = qMin(minY, text.posY);
                }
                if (minX < 1e30)
                    pox = minX;
                if (minY < 1e30)
                    poy = minY;
            }

            const int irPartIndex = part.commonToAllParts ? -1 : partIdx;  // IR 中的部件索引。

            // 引脚使用几何边界框原点；公共部件进入 Part Zero。
            convertPins(part.pins, pox, poy, partIdx, part.commonToAllParts);
            // 图形使用 part 显式原点
            convertRectangles(part.rectangles, gox, goy, irPartIndex);
            convertCircles(part.circles, gox, goy, irPartIndex);
            convertEllipses(part.ellipses, gox, goy, irPartIndex);
            convertArcs(part.arcs, gox, goy, irPartIndex);
            convertPolylines(part.polylines, gox, goy, irPartIndex);
            convertPolygons(part.polygons, gox, goy, irPartIndex);
            convertPaths(part.paths, gox, goy, irPartIndex);
            convertImages(part.images, gox, goy, irPartIndex);
            convertTexts(part.texts, gox, goy, irPartIndex);
            for (const SymbolGraphicOrder& order : part.graphicOrder)
                ir.graphicOrder.append({order.type, order.index, irPartIndex});
            if (!part.commonToAllParts)
                ++partIdx;
        }
    } else {
        const SymbolBBox bbox = data.bbox();  // 单部件符号的原始边界框。
        double ox = bbox.x;  // 当前使用的原点 X。
        double oy = bbox.y;  // 当前使用的原点 Y。
        bool usesLogicalOrigin = false;  // 是否采用了数据中的逻辑原点。
        const double bboxCenterX = bbox.x + bbox.width / 2.0;  // 边界框中心 X。
        const double bboxCenterY = bbox.y + bbox.height / 2.0;  // 边界框中心 Y。
        const double headTolerance = 0.75 * qMax(bbox.width, bbox.height);  // 逻辑原点允许偏离边界框中心的容差。
        // 判断逻辑原点是否位于合理的边界框范围内。
        const bool headInsideBbox = bbox.width > 0.0 && bbox.height > 0.0 &&
                                    std::hypot(bbox.headX - bboxCenterX, bbox.headY - bboxCenterY) <= headTolerance;
        if (bbox.hasHeadCenter && std::isfinite(bbox.headX) && std::isfinite(bbox.headY) && headInsideBbox) {
            ox = bbox.headX;
            oy = bbox.headY;
            usesLogicalOrigin = true;
        } else if (!bbox.hasHeadCenter) {
            // 兼容旧缓存：没有 head 中心时保留原有 BBox 原点语义。
        } else {
            double minX = std::numeric_limits<double>::max();  // 几何集合最小 X。
            double minY = std::numeric_limits<double>::max();  // 几何集合最小 Y。
            double maxX = std::numeric_limits<double>::lowest();  // 几何集合最大 X。
            double maxY = std::numeric_limits<double>::lowest();  // 几何集合最大 Y。
            auto include = [&](double x, double y) {
                minX = qMin(minX, x);
                minY = qMin(minY, y);
                maxX = qMax(maxX, x);
                maxY = qMax(maxY, y);
            };
            for (const auto& pin : data.pins())
                include(pin.settings.posX, pin.settings.posY);
            for (const auto& rect : data.rectangles()) {
                include(rect.posX, rect.posY);
                include(rect.posX + rect.width, rect.posY + rect.height);
            }
            if (minX <= maxX && minY <= maxY) {
                ox = (minX + maxX) / 2.0;
                oy = (minY + maxY) / 2.0;
            }
        }
        convertPins(data.pins(), ox, oy);
        convertRectangles(data.rectangles(), ox, oy);
        convertCircles(data.circles(), ox, oy);
        convertEllipses(data.ellipses(), ox, oy);
        convertArcs(data.arcs(), ox, oy);
        convertPolylines(data.polylines(), ox, oy);
        convertPolygons(data.polygons(), ox, oy);
        convertPaths(data.paths(), ox, oy);
        convertImages(data.images(), ox, oy);
        convertTexts(data.texts(), ox, oy);
        for (const SymbolGraphicOrder& order : data.graphicOrder())
            ir.graphicOrder.append({order.type, order.index, 0});
        ir.preserveLogicalOrigin = usesLogicalOrigin;
    }

    return ir;
}

}  // namespace IR
}  // namespace EasyKiConverter
