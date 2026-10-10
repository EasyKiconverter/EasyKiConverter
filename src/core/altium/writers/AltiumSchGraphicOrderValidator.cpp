#include "AltiumSchGraphicOrderValidator.h"

#include <QMap>
#include <QSet>

#include <algorithm>

namespace EasyKiConverter {

/**
 * @brief 检查来源图元顺序是否覆盖所有可排序图元。
 * @details 不完整的顺序会导致图元被静默跳过，并使 FileHeader 的 WEIGHT 与 Data 流不一致。
 * @param component 待校验来源顺序的原理图元件。
 * @return 来源顺序完整且无重复引用时返回 true。
 */
bool AltiumSchGraphicOrderValidator::validate(const AltiumSchComponent& component) {
    QSet<QString> expected;  // 按组件图元计算出的完整顺序键集合。
    QSet<QString> actual;  // 来源顺序中实际出现的顺序键集合。
    QMap<QString, QSet<int>> pathSegments;  // 按路径键收集的已出现片段索引。
    QSet<QString> unsplitPaths;  // 没有片段索引、需要整体写出的路径集合。
    const auto key = [](const QString& type, int index, int partIndex) {
        return QStringLiteral("%1:%2:%3").arg(type).arg(index).arg(partIndex);
    };

    QMap<int, int> partPinIndexes;  // 各部件普通引脚的局部索引计数。
    QMap<int, int> commonPinIndexes;  // 公共引脚的局部索引计数。
    for (const AltiumSchPin& pin : component.pins) {
        const int partIndex = pin.sourcePartIndex;  // 当前引脚的来源部件索引。
        expected.insert(key(QStringLiteral("P"), partPinIndexes[partIndex]++, partIndex));
        if (pin.ownerPartId == -1)
            expected.insert(key(QStringLiteral("P"), commonPinIndexes[-1]++, -1));
    }

    const auto addUniqueIndexed = [&expected, &key](const QString& type, int index, int partIndex) {
        if (index < 0)
            return true;
        const QString orderKey = key(type, index, partIndex);  // 当前图元的唯一顺序键。
        if (expected.contains(orderKey))
            return false;
        expected.insert(orderKey);
        return true;
    };
    const auto addUniqueNonPathIndexed = [&addUniqueIndexed, &expected, &key](
                                             const QString& type, int index, int partIndex) {
        if (type == QStringLiteral("PT")) {
            if (index >= 0)
                expected.insert(key(type, index, partIndex));
            return true;
        }
        return addUniqueIndexed(type, index, partIndex);
    };
    for (const AltiumSchRectangle& rect : component.rectangles) {
        if (!addUniqueIndexed(QStringLiteral("R"), rect.sourceGraphicIndex, rect.sourcePartIndex))
            return false;
    }
    for (const AltiumSchRoundRectangle& rect : component.roundRectangles) {
        if (!addUniqueIndexed(QStringLiteral("R"), rect.sourceGraphicIndex, rect.sourcePartIndex))
            return false;
    }
    for (const AltiumSchEllipse& ellipse : component.ellipses) {
        if (!addUniqueNonPathIndexed(ellipse.sourceGraphicType, ellipse.sourceGraphicIndex, ellipse.sourcePartIndex))
            return false;
    }
    for (const AltiumSchArc& arc : component.arcs) {
        if (!addUniqueNonPathIndexed(arc.sourceGraphicType, arc.sourceGraphicIndex, arc.sourcePartIndex))
            return false;
    }
    for (const AltiumSchPolyline& polyline : component.polylines) {
        if (!addUniqueIndexed(QStringLiteral("PL"), polyline.sourceGraphicIndex, polyline.sourcePartIndex))
            return false;
    }
    for (const AltiumSchPolygon& polygon : component.polygons) {
        if (!addUniqueIndexed(QStringLiteral("PG"), polygon.sourceGraphicIndex, polygon.sourcePartIndex))
            return false;
    }
    for (const AltiumSchText& text : component.texts) {
        if (!text.isPinLabel && !addUniqueIndexed(QStringLiteral("T"), text.sourceGraphicIndex, text.sourcePartIndex))
            return false;
    }
    const bool hasImageOrder = std::any_of(
        component.graphicOrder.cbegin(), component.graphicOrder.cend(), [](const AltiumSchGraphicOrder& order) {
            return order.type == QStringLiteral("I");
        });
    if (hasImageOrder) {
        for (const AltiumSchImage& image : component.images) {
            if (!addUniqueIndexed(QStringLiteral("I"), image.sourceGraphicIndex, image.sourcePartIndex))
                return false;
        }
    }
    const auto addPathSegment = [&pathSegments, &unsplitPaths, &key](
                                    const QString& type, int index, int segmentIndex, int partIndex) {
        if (type != QStringLiteral("PT") || index < 0)
            return true;
        const QString pathKey = key(type, index, partIndex);  // 当前路径的唯一顺序键。
        if (segmentIndex < 0)
            unsplitPaths.insert(pathKey);
        else {
            QSet<int>& segments = pathSegments[pathKey];  // 当前路径已登记的片段索引集合。
            if (segments.contains(segmentIndex))
                return false;
            segments.insert(segmentIndex);
        }
        return true;
    };
    for (const AltiumSchPath& path : component.paths) {
        if (path.sourceGraphicIndex >= 0)
            expected.insert(key(path.sourceGraphicType, path.sourceGraphicIndex, path.sourcePartIndex));
        if (!addPathSegment(
                path.sourceGraphicType, path.sourceGraphicIndex, path.sourceSegmentIndex, path.sourcePartIndex))
            return false;
    }
    for (const AltiumSchBezier& bezier : component.beziers) {
        if (bezier.controlPoints.size() == 4) {
            if (bezier.sourceGraphicIndex >= 0)
                expected.insert(key(bezier.sourceGraphicType, bezier.sourceGraphicIndex, bezier.sourcePartIndex));
            if (!addPathSegment(bezier.sourceGraphicType,
                                bezier.sourceGraphicIndex,
                                bezier.sourceSegmentIndex,
                                bezier.sourcePartIndex))
                return false;
        }
    }
    for (const AltiumSchEllipticalArc& arc : component.ellipticalArcs) {
        if (!addUniqueNonPathIndexed(arc.sourceGraphicType, arc.sourceGraphicIndex, arc.sourcePartIndex))
            return false;
        if (!addPathSegment(arc.sourceGraphicType, arc.sourceGraphicIndex, arc.sourceSegmentIndex, arc.sourcePartIndex))
            return false;
    }
    for (const AltiumSchArc& arc : component.arcs) {
        if (!addPathSegment(arc.sourceGraphicType, arc.sourceGraphicIndex, arc.sourceSegmentIndex, arc.sourcePartIndex))
            return false;
    }

    // 这些图元只有在来源类型和索引同时有效时才能由 AltiumSchGraphicOrderWriter 写出。
    // 否则应回退到默认顺序，避免来源类型非空但索引缺失的图元被静默丢弃。
    const auto hasUnresolvedOrderedGraphic = [](const auto& graphics) {
        for (const auto& graphic : graphics) {
            if (!graphic.sourceGraphicType.isEmpty() && graphic.sourceGraphicIndex < 0)
                return true;
        }
        return false;
    };
    if (hasUnresolvedOrderedGraphic(component.ellipses) || hasUnresolvedOrderedGraphic(component.arcs) ||
        hasUnresolvedOrderedGraphic(component.paths) || hasUnresolvedOrderedGraphic(component.beziers) ||
        hasUnresolvedOrderedGraphic(component.ellipticalArcs)) {
        return false;
    }

    for (const AltiumSchGraphicOrder& order : component.graphicOrder) {
        if (order.index < 0 || order.type.isEmpty())
            return false;
        const QString orderKey = key(order.type, order.index, order.partIndex);  // 当前来源顺序项的唯一键。
        if (actual.contains(orderKey))
            return false;
        actual.insert(orderKey);
    }

    QSet<int> emittedPins;  // 已由来源顺序覆盖的组件引脚索引。
    for (const AltiumSchGraphicOrder& order : component.graphicOrder) {
        if (order.type != QStringLiteral("P"))
            continue;
        int localIndex = 0;  // 当前部件范围内的引脚局部索引。
        for (int pinIndex = 0; pinIndex < component.pins.size(); ++pinIndex) {
            const AltiumSchPin& pin = component.pins.at(pinIndex);  // 当前候选引脚。
            const bool matchesPart =
                order.partIndex < 0 ? pin.ownerPartId == -1 : pin.sourcePartIndex == order.partIndex;
            if (!matchesPart)
                continue;
            if (localIndex == order.index) {
                if (emittedPins.contains(pinIndex))
                    return false;
                emittedPins.insert(pinIndex);
                break;
            }
            ++localIndex;
        }
    }
    if (emittedPins.size() != component.pins.size())
        return false;

    for (const QString& pathKey : unsplitPaths) {
        if (pathSegments.contains(pathKey))
            return false;
    }
    for (auto it = pathSegments.cbegin(); it != pathSegments.cend(); ++it) {
        // 段索引允许出现缺口：前面的来源段可能因几何量化或参数无效被跳过，
        // 但后续有效段仍应按实际索引写出，而不是迫使整个图元回退到分组顺序。
        if (it.value().isEmpty())
            return false;
    }

    for (const QString& expectedKey : expected) {
        if (expectedKey.startsWith(QStringLiteral("P:")))
            continue;
        if (!actual.contains(expectedKey))
            return false;
    }
    for (const QString& actualKey : actual) {
        if (!expected.contains(actualKey))
            return false;
    }
    return true;
}

}  // namespace EasyKiConverter
