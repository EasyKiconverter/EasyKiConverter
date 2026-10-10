#include "AltiumSchGraphicOrderWriter.h"

#include "AltiumSchLibWriter.h"
#include "utils/AltiumBinaryWriter.h"

#include <QSet>

#include <algorithm>

namespace EasyKiConverter {

/**
 * @brief 创建 SchLib 图元顺序调度器。
 * @param owner 所属的 SchLib 主写入器。
 */
AltiumSchGraphicOrderWriter::AltiumSchGraphicOrderWriter(AltiumSchLibWriter& owner) : m_owner(owner) {}

/**
 * @brief 查找来源图元并委托给对应的记录写入方法。
 * @param writer 当前组件 Data 流写入器。
 * @param component 待写入的原理图元件。
 * @param order 来源图元顺序信息。
 */
void AltiumSchGraphicOrderWriter::write(AltiumBinaryWriter& writer,
                                        const AltiumSchComponent& component,
                                        const AltiumSchGraphicOrder& order) {
    const auto matchesPart = [&order](int sourcePartIndex) { return sourcePartIndex == order.partIndex; };
    if (order.type == QStringLiteral("P")) {
        int localIndex = 0;  // 当前部件范围内的引脚局部索引。
        for (const AltiumSchPin& pin : component.pins) {
            const bool matchesSourcePart =
                order.partIndex < 0 ? pin.ownerPartId == -1 : pin.sourcePartIndex == order.partIndex;
            if (!matchesSourcePart)
                continue;
            if (localIndex == order.index) {
                m_owner.writePinRecord(writer, pin);
                return;
            }
            ++localIndex;
        }
        return;
    }
    if (order.type == QStringLiteral("R")) {
        for (const AltiumSchRoundRectangle& rect : component.roundRectangles) {
            if (rect.sourceGraphicIndex == order.index && matchesPart(rect.sourcePartIndex)) {
                m_owner.writeRoundRectangleRecord(writer, rect);
                return;
            }
        }
        for (const AltiumSchRectangle& rect : component.rectangles) {
            if (rect.sourceGraphicIndex == order.index && matchesPart(rect.sourcePartIndex)) {
                m_owner.writeRectangleRecord(writer, rect);
                return;
            }
        }
        return;
    }
    if (order.type == QStringLiteral("C") || order.type == QStringLiteral("E")) {
        for (const AltiumSchEllipse& ellipse : component.ellipses) {
            if (ellipse.sourceGraphicType == order.type && ellipse.sourceGraphicIndex == order.index &&
                matchesPart(ellipse.sourcePartIndex)) {
                m_owner.writeEllipseRecord(writer, ellipse);
                return;
            }
        }
        return;
    }
    if (order.type == QStringLiteral("A")) {
        for (const AltiumSchArc& arc : component.arcs) {
            if (arc.sourceGraphicType == order.type && arc.sourceGraphicIndex == order.index &&
                matchesPart(arc.sourcePartIndex)) {
                m_owner.writeArcRecord(writer, arc);
                return;
            }
        }
        return;
    }
    if (order.type == QStringLiteral("PL")) {
        for (const AltiumSchPolyline& polyline : component.polylines) {
            if (polyline.sourceGraphicIndex == order.index && matchesPart(polyline.sourcePartIndex)) {
                m_owner.writePolylineRecord(writer, polyline);
                return;
            }
        }
        return;
    }
    if (order.type == QStringLiteral("PG")) {
        for (const AltiumSchPolygon& polygon : component.polygons) {
            if (polygon.sourceGraphicIndex == order.index && matchesPart(polygon.sourcePartIndex)) {
                m_owner.writePolygonRecord(writer, polygon);
                return;
            }
        }
        return;
    }
    if (order.type == QStringLiteral("T")) {
        for (const AltiumSchText& text : component.texts) {
            if (!text.isPinLabel && text.sourceGraphicIndex == order.index && matchesPart(text.sourcePartIndex)) {
                m_owner.writeTextRecord(writer, text);
                return;
            }
        }
        return;
    }
    if (order.type == QStringLiteral("I")) {
        for (const AltiumSchImage& image : component.images) {
            if (image.sourceGraphicIndex == order.index && matchesPart(image.sourcePartIndex)) {
                m_owner.writeImageRecord(writer, image);
                return;
            }
        }
        return;
    }
    if (order.type != QStringLiteral("PT"))
        return;

    for (const AltiumSchPath& path : component.paths) {
        if (path.sourceGraphicType == order.type && path.sourceGraphicIndex == order.index &&
            matchesPart(path.sourcePartIndex) && path.sourceSegmentIndex < 0) {
            m_owner.writePathRecord(writer, path);
            return;
        }
    }

    QSet<int> segmentIndexSet;  // 当前路径中已发现的片段索引集合。
    const auto collectSegmentIndices = [&order, &matchesPart, &segmentIndexSet](const auto& graphics) {
        for (const auto& graphic : graphics) {
            if (graphic.sourceGraphicType == order.type && graphic.sourceGraphicIndex == order.index &&
                matchesPart(graphic.sourcePartIndex) && graphic.sourceSegmentIndex >= 0) {
                segmentIndexSet.insert(graphic.sourceSegmentIndex);
            }
        }
    };
    collectSegmentIndices(component.paths);
    collectSegmentIndices(component.beziers);
    collectSegmentIndices(component.arcs);
    collectSegmentIndices(component.ellipticalArcs);

    QList<int> segmentIndices = segmentIndexSet.values();  // 按序写入的路径片段索引列表。
    std::sort(segmentIndices.begin(), segmentIndices.end());
    for (const int segmentIndex : segmentIndices) {
        for (const AltiumSchPath& path : component.paths) {
            if (path.sourceGraphicType == order.type && path.sourceGraphicIndex == order.index &&
                matchesPart(path.sourcePartIndex) && path.sourceSegmentIndex == segmentIndex) {
                m_owner.writePathRecord(writer, path);
                break;
            }
        }
        for (const AltiumSchBezier& bezier : component.beziers) {
            if (bezier.sourceGraphicType == order.type && bezier.sourceGraphicIndex == order.index &&
                matchesPart(bezier.sourcePartIndex) && bezier.sourceSegmentIndex == segmentIndex) {
                m_owner.writeBezierRecord(writer, bezier);
                break;
            }
        }
        for (const AltiumSchArc& arc : component.arcs) {
            if (arc.sourceGraphicType == order.type && arc.sourceGraphicIndex == order.index &&
                matchesPart(arc.sourcePartIndex) && arc.sourceSegmentIndex == segmentIndex) {
                m_owner.writeArcRecord(writer, arc);
                break;
            }
        }
        for (const AltiumSchEllipticalArc& arc : component.ellipticalArcs) {
            if (arc.sourceGraphicType == order.type && arc.sourceGraphicIndex == order.index &&
                matchesPart(arc.sourcePartIndex) && arc.sourceSegmentIndex == segmentIndex) {
                m_owner.writeEllipticalArcRecord(writer, arc);
                break;
            }
        }
    }
}

}  // namespace EasyKiConverter
