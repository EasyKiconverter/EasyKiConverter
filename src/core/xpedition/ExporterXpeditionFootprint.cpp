#include "ExporterXpeditionFootprint.h"

#include "XpeditionZipWriter.h"

#include <QCryptographicHash>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>
#include <QtMath>

#include <cmath>

namespace EasyKiConverter {

namespace {

constexpr double kThousandthInchMm = 0.0254;
constexpr double kSolderMaskExpansionTh = 8.0;

/**
 * @brief 将毫米转换为 Xpedition 使用的千分之一英寸单位。
 * @param valueMm 输入的毫米数。
 * @return 转换后的 TH 数值。
 */
double toTh(double valueMm) {
    return valueMm / kThousandthInchMm;
}

/**
 * @brief 使用固定精度格式化几何数值。
 * @param value 待格式化的数值。
 * @return 适合写入 HKP 文本的十进制字符串。
 */
QString fmt(double value) {
    return QString::number(value, 'f', 4);
}

/**
 * @brief 将统一表示中的焊盘形状映射到目标格式名称。
 * @param shape 统一表示中的焊盘形状。
 * @return Xpedition Pad 定义使用的形状名称。
 */
QString padShape(const IR::FootprintPadIR& pad) {
    // 目标格式可以直接表达圆、长圆、矩形和自定义多边形；其余形状必须先有可用顶点。
    switch (pad.shape) {
        case IR::PadShape::Ellipse:
            return qFuzzyCompare(pad.size.width(), pad.size.height()) ? QStringLiteral("ROUND")
                                                                      : QStringLiteral("OBLONG");
        case IR::PadShape::Oval:
            return QStringLiteral("OBLONG");
        case IR::PadShape::Polygon:
        case IR::PadShape::RoundRect:
        case IR::PadShape::Trapezoid:
            return pad.customShapePoints.size() >= 3 ? QStringLiteral("CUSTOM") : QString();
        case IR::PadShape::Rect:
            return QStringLiteral("RECTANGLE");
        default:
            return QString();
    }
}

/**
 * @brief 判断焊盘是否已经具备目标格式所需的自定义顶点。
 * @param pad 待检查的焊盘。
 * @return 可以写成自定义多边形时返回 true。
 */
bool hasCustomPadGeometry(const IR::FootprintPadIR& pad) {
    return (pad.shape == IR::PadShape::Polygon || pad.shape == IR::PadShape::RoundRect ||
            pad.shape == IR::PadShape::Trapezoid) &&
           pad.customShapePoints.size() >= 3;
}

/**
 * @brief 根据焊盘形状和尺寸生成可复用的 Pad 名称。
 * @param pad 待命名的焊盘。
 * @return 稳定且可去重的 Pad 名称。
 */
QString padName(const IR::FootprintPadIR& pad) {
    QString geometry = QStringLiteral("%1_%2x%3")
                           .arg(padShape(pad))
                           .arg(fmt(toTh(pad.size.width())))
                           .arg(fmt(toTh(pad.size.height())));
    if (pad.shape == IR::PadShape::Polygon && !pad.customShapePoints.isEmpty()) {
        // 自定义多边形顶点参与名称计算，避免相同外接尺寸的异形焊盘错误复用。
        QByteArray points;
        for (const QPointF& point : pad.customShapePoints)
            points += QByteArray::number(point.x(), 'f', 6) + ',' + QByteArray::number(point.y(), 'f', 6) + ';';
        geometry += QStringLiteral("_P%1").arg(
            QString::fromLatin1(QCryptographicHash::hash(points, QCryptographicHash::Sha1).toHex().left(10)));
    }
    if (pad.isThroughHole()) {
        // 通孔几何参数参与名称计算，避免不同孔径或槽长共享同一个钻孔定义。
        geometry += QStringLiteral("_H%1_L%2").arg(fmt(toTh(pad.holeSize))).arg(fmt(toTh(pad.holeLength)));
        // 镀层属性决定孔定义的工艺选项，必须参与名称计算以避免错误复用。
        if (!pad.isPlated)
            geometry += QStringLiteral("_NONPLATED");
    }
    return QStringLiteral("PAD_") + geometry;
}

/**
 * @brief 生成同一焊盘的阻焊层定义名称。
 * @param baseName 铜焊盘定义名称。
 * @return 可复用的阻焊焊盘名称。
 */
QString solderMaskPadName(const QString& baseName) {
    return baseName + QStringLiteral("_MASK");
}

struct LayerSection {
    QString name;
    QString side;
};

/**
 * @brief 生成考虑旋转角度的矩形闭合点列。
 * @param rectangle IR 中的矩形图元。
 * @return 按顺时针方向排列并重复首点的矩形顶点。
 */
QList<QPointF> rectanglePoints(const IR::FootprintRectangleIR& rectangle) {
    const QPointF center = rectangle.bounds.center();
    const double angle = qDegreesToRadians(rectangle.rotation);
    const double cosine = std::cos(angle);
    const double sine = std::sin(angle);
    const QList<QPointF> corners = {rectangle.bounds.topLeft(),
                                    rectangle.bounds.topRight(),
                                    rectangle.bounds.bottomRight(),
                                    rectangle.bounds.bottomLeft()};
    QList<QPointF> points;
    points.reserve(corners.size() + 1);
    for (const QPointF& corner : corners) {
        const QPointF relative = corner - center;
        points.append(
            center + QPointF(relative.x() * cosine - relative.y() * sine, relative.x() * sine + relative.y() * cosine));
    }
    points.append(points.first());
    return points;
}

/**
 * @brief 将统一表示的图层映射为目标格式的段名和安装面。
 * @param layer 统一表示中的图层。
 * @return 目标格式段名与面向信息；不支持的图层返回空值。
 */
LayerSection layerSection(IR::LayerType layer) {
    // 每组图层同时决定目标段名和安装面，未知图层返回空值由调用方跳过。
    switch (layer) {
        case IR::LayerType::TopSilk:
        case IR::LayerType::BottomSilk:
        case IR::LayerType::TopOverlay:
        case IR::LayerType::BottomOverlay:
            return {QStringLiteral("SILKSCREEN_OUTLINE"),
                    layer == IR::LayerType::TopSilk || layer == IR::LayerType::TopOverlay ? QStringLiteral("MNT_SIDE")
                                                                                          : QStringLiteral("OPP_SIDE")};
        case IR::LayerType::TopAssembly:
        case IR::LayerType::BottomAssembly:
            return {QStringLiteral("ASSEMBLY_OUTLINE"),
                    layer == IR::LayerType::TopAssembly ? QStringLiteral("MNT_SIDE") : QStringLiteral("OPP_SIDE")};
        case IR::LayerType::TopPaste:
        case IR::LayerType::BottomPaste:
            return {QStringLiteral("SOLDER_PASTE"),
                    layer == IR::LayerType::TopPaste ? QStringLiteral("MNT_SIDE") : QStringLiteral("OPP_SIDE")};
        case IR::LayerType::TopMask:
        case IR::LayerType::BottomMask:
            return {QStringLiteral("SOLDER_MASK"),
                    layer == IR::LayerType::TopMask ? QStringLiteral("MNT_SIDE") : QStringLiteral("OPP_SIDE")};
        case IR::LayerType::TopCopper:
        case IR::LayerType::BottomCopper:
            return {QStringLiteral("GRAPHIC"),
                    layer == IR::LayerType::TopCopper ? QStringLiteral("MNT_SIDE") : QStringLiteral("OPP_SIDE")};
        case IR::LayerType::EdgeCuts:
            return {QStringLiteral("ASSEMBLY_OUTLINE"), QStringLiteral("MNT_SIDE")};
        default:
            return {};
    }
}

/**
 * @brief 转义 HKP 文本字段中的特殊字符。
 * @param text 原始文本。
 * @return 可安全放入双引号字段的文本。
 */
QString escapedText(QString text) {
    text.replace('\\', QStringLiteral("\\\\"));
    text.replace('"', QStringLiteral("\\\""));
    text.replace('\n', QStringLiteral(" "));
    text.replace('\r', QStringLiteral(" "));
    return text;
}

/**
 * @brief 根据钻孔直径生成共享的钻孔定义名称。
 * @param diameterMm 钻孔直径，单位为毫米。
 * @param slotLengthMm 槽孔总长度，单位为毫米；不大于孔径时按圆孔处理。
 * @return 稳定的 Hole 名称。
 */
QString holeName(double diameterMm, double slotLengthMm = 0.0, bool isPlated = true) {
    QString name;
    if (slotLengthMm > diameterMm)
        name = QStringLiteral("HOLE_%1x%2").arg(fmt(toTh(diameterMm))).arg(fmt(toTh(slotLengthMm)));
    else
        name = QStringLiteral("HOLE_%1").arg(fmt(toTh(diameterMm)));
    if (!isPlated)
        name += QStringLiteral("_NONPLATED");
    return name;
}

/**
 * @brief 根据独立安装孔直径生成对应的焊盘名称。
 * @param diameterMm 安装孔直径，单位为毫米。
 * @return 稳定的孔焊盘名称。
 */
QString holePadName(double diameterMm) {
    return QStringLiteral("HOLE_PAD_%1").arg(fmt(toTh(diameterMm)));
}

/**
 * @brief 生成独立安装孔在 Padstack 和 Cell 中共用的引用名称。
 * @param diameterMm 安装孔直径，单位为毫米。
 * @return 与 Padstack 定义完全一致的安装孔 Padstack 名称。
 */
QString standaloneHoleStackName(double diameterMm) {
    return holeName(diameterMm, 0.0, false) + QStringLiteral("_TH");
}

/**
 * @brief 根据表贴焊盘所在面生成 Padstack 名称后缀。
 * @param pad 待处理的焊盘。
 * @return 表贴焊盘的安装面后缀；通孔焊盘返回空字符串。
 */
QString padstackSideSuffix(const IR::FootprintPadIR& pad) {
    if (pad.isThroughHole())
        return {};
    return pad.layer == IR::LayerType::BottomCopper ? QStringLiteral("_BOTTOM_SMD") : QStringLiteral("_TOP_SMD");
}

/**
 * @brief 计算封装所有几何元素的包围盒。
 * @param footprint 待计算的封装。
 * @return 包含焊盘、线条、圆、孔和轮廓的包围盒。
 */
QRectF footprintBounds(const IR::FootprintComponentIR& footprint) {
    double minX = 0.0;
    double minY = 0.0;
    double maxX = 0.0;
    double maxY = 0.0;
    bool initialized = false;
    const auto addPoint = [&](const QPointF& point) {
        if (!initialized) {
            minX = maxX = point.x();
            minY = maxY = point.y();
            initialized = true;
            return;
        }
        minX = qMin(minX, point.x());
        minY = qMin(minY, point.y());
        maxX = qMax(maxX, point.x());
        maxY = qMax(maxY, point.y());
    };
    for (const auto& pad : footprint.pads) {
        addPoint(pad.position - QPointF(pad.size.width() / 2.0, pad.size.height() / 2.0));
        addPoint(pad.position + QPointF(pad.size.width() / 2.0, pad.size.height() / 2.0));
    }
    for (const auto& circle : footprint.circles) {
        addPoint(circle.center - QPointF(circle.radius, circle.radius));
        addPoint(circle.center + QPointF(circle.radius, circle.radius));
    }
    for (const auto& rect : footprint.rectangles) {
        for (const QPointF& point : rectanglePoints(rect))
            addPoint(point);
    }
    for (const auto& arc : footprint.arcs) {
        // 用完整圆弧外接框参与原点计算，保证任意圆弧不会被遗漏；不会改变实际写出的弧段。
        addPoint(arc.center - QPointF(arc.radius, arc.radius));
        addPoint(arc.center + QPointF(arc.radius, arc.radius));
    }
    for (const auto& region : footprint.regions) {
        for (const QPointF& point : region.vertices)
            addPoint(point);
    }
    for (const auto& text : footprint.texts)
        if (text.isDisplayed && !text.text.isEmpty())
            addPoint(text.position);
    for (const auto& outline : footprint.outlines) {
        for (const auto& point : outline.points)
            addPoint(point);
    }
    for (const auto& track : footprint.tracks) {
        for (const auto& point : track.points)
            addPoint(point);
    }
    for (const auto& hole : footprint.holes) {
        addPoint(hole.center - QPointF(hole.radius, hole.radius));
        addPoint(hole.center + QPointF(hole.radius, hole.radius));
    }
    return initialized ? QRectF(QPointF(minX, minY), QPointF(maxX, maxY)) : QRectF(-1, -1, 2, 2);
}

/**
 * @brief 将点列写成目标格式的折线或填充多边形。
 * @param kind 目标格式图元类型。
 * @param points 已经完成原点归一化的点列。
 * @param width 线宽，单位为毫米。
 * @param filled 是否标记为填充区域。
 * @return HKP 图元文本；点列为空时返回空字符串。
 */
QString shapeBlock(const QString& kind, const QList<QPointF>& points, double width = 0.0, bool filled = false) {
    if (points.isEmpty())
        return {};
    QString output = QStringLiteral(" ...%1\n  ....WIDTH %2\n  ....XY (%3, %4)")
                         .arg(kind)
                         .arg(fmt(toTh(width)))
                         .arg(fmt(toTh(points.first().x())))
                         .arg(fmt(toTh(points.first().y())));
    for (int index = 1; index < points.size(); ++index)
        output +=
            QStringLiteral("\n   ...XY (%1, %2)").arg(fmt(toTh(points[index].x()))).arg(fmt(toTh(points[index].y())));
    if (filled)
        output += QStringLiteral("\n  ....SHAPE_OPTIONS FILLED");
    output += QLatin1Char('\n');
    return output;
}

}  // namespace

// 封装归档由 Padstack 和 Cell 两类 HKP 文件组成，使用专用扩展名标识。
QString ExporterXpeditionFootprint::libraryFileExtension() const {
    // 封装导出结果由多个 HKP 文件组成，因此使用专用 ZIP 扩展名区分符号库。
    return QStringLiteral("_Footprints.zip");
}

// 目录输出能力由导出接口统一查询，当前归档实现始终返回单文件结果。
bool ExporterXpeditionFootprint::isDirectoryOutput() const {
    // 写入器会在内存中组装归档，外部接口看到的是单个文件而不是目录。
    return false;
}

// 诊断接口不改写内部列表，保证调用方可以区分多个封装的处理结果。
QStringList ExporterXpeditionFootprint::diagnostics() const {
    // 返回值按导出顺序保留，调用方可以直接展示降级和跳过原因。
    return m_diagnostics;
}

// 清理名称后再同时用于 ZIP 条目和 HKP 逻辑名称，避免两处命名不一致。
QString ExporterXpeditionFootprint::safeName(QString name) const {
    // 文件名必须同时适合 ZIP 条目和 HKP 逻辑名称，因此在导出边界统一清理。
    name = name.trimmed();
    name.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9_.-]")), QStringLiteral("_"));
    return name.isEmpty() ? QStringLiteral("unnamed") : name;
}

// Padstack 文件集中声明可复用焊盘、钻孔以及两者的技术组合。
QByteArray ExporterXpeditionFootprint::padstackFile(const IR::FootprintComponentIR& footprint) const {
    // Pad 与 Padstack 分开去重，保证相同尺寸的焊盘只生成一次定义。
    QString output = QStringLiteral(".FILETYPE PADSTACK_LIBRARY\n.VERSION \"VB99.0\"\n.SCHEMA_VERSION 13\n") +
                     QStringLiteral(".CREATOR \"EasyKiConverter\"\n\n.UNITS TH\n\n");
    QSet<QString> writtenPads;
    QSet<QString> writtenStacks;

    // IR 没有独立的阻焊扩展字段，使用明确的 8 TH 默认扩展生成工艺层焊盘。
    const auto appendPad = [&](const QString& name, const IR::FootprintPadIR& pad, double expansionTh) {
        if (writtenPads.contains(name))
            return;
        const double width = toTh(pad.size.width()) + expansionTh;
        const double height = toTh(pad.size.height()) + expansionTh;
        output += QStringLiteral(".PAD \"%1\"\n..PAD_OPTIONS USER_GENERATED_NAME\n..OFFSET (0, 0)\n..%2\n")
                      .arg(name, padShape(pad));
        if (pad.shape == IR::PadShape::Ellipse && qFuzzyCompare(pad.size.width(), pad.size.height())) {
            output += QStringLiteral("...DIAMETER %1\n").arg(fmt(width));
        } else if (hasCustomPadGeometry(pad)) {
            // 自定义焊盘按 X/Y 尺寸比例扩展，保持多边形的相对形状。
            const double scaleX = pad.size.width() > 0.0 ? width / toTh(pad.size.width()) : 1.0;
            const double scaleY = pad.size.height() > 0.0 ? height / toTh(pad.size.height()) : 1.0;
            output += QStringLiteral("...POLYLINE_SHAPE\n....XY");
            for (const QPointF& point : pad.customShapePoints)
                output +=
                    QStringLiteral(" (%1, %2)").arg(fmt(toTh(point.x()) * scaleX)).arg(fmt(toTh(point.y()) * scaleY));
            output += QStringLiteral("\n....SHAPE_OPTIONS FILLED\n");
        } else {
            output += QStringLiteral("...WIDTH %1\n...HEIGHT %2\n").arg(fmt(width)).arg(fmt(height));
        }
        writtenPads.insert(name);
    };

    for (const auto& pad : footprint.pads) {
        const QString baseName = padName(pad);
        // 先写入铜焊盘，再写入供阻焊层引用的放大定义。
        appendPad(baseName, pad, 0.0);
        const QString maskName = solderMaskPadName(baseName);
        appendPad(maskName, pad, kSolderMaskExpansionTh);

        const QString stackName = baseName + (pad.isThroughHole() ? QStringLiteral("_TH") : padstackSideSuffix(pad));
        if (writtenStacks.contains(stackName))
            continue;
        output += QStringLiteral(".PADSTACK \"%1\"\n..PADSTACK_TYPE %2\n..TECHNOLOGY \"(Default)\"\n")
                      .arg(stackName, pad.isThroughHole() ? QStringLiteral("PIN_THROUGH") : QStringLiteral("PIN_SMD"));
        output += QStringLiteral("...TECHNOLOGY_OPTIONS NONE\n");
        if (pad.isThroughHole() || pad.layer == IR::LayerType::TopCopper)
            output += QStringLiteral("...TOP_PAD \"%1\"\n").arg(baseName);
        if (pad.isThroughHole() || pad.layer == IR::LayerType::BottomCopper)
            output += QStringLiteral("...BOTTOM_PAD \"%1\"\n").arg(baseName);
        if (pad.isThroughHole() || pad.layer == IR::LayerType::TopCopper)
            output += QStringLiteral("...TOP_SOLDERMASK_PAD \"%1\"\n").arg(maskName);
        if (pad.isThroughHole() || pad.layer == IR::LayerType::BottomCopper)
            output += QStringLiteral("...BOTTOM_SOLDERMASK_PAD \"%1\"\n").arg(maskName);
        if (!pad.isThroughHole() && pad.layer == IR::LayerType::TopCopper)
            output += QStringLiteral("...TOP_SOLDERPASTE_PAD \"%1\"\n").arg(baseName);
        if (!pad.isThroughHole() && pad.layer == IR::LayerType::BottomCopper)
            output += QStringLiteral("...BOTTOM_SOLDERPASTE_PAD \"%1\"\n").arg(baseName);
        if (pad.isThroughHole()) {
            // 通孔焊盘需要独立的 Hole 定义，孔径以实际直径参与命名和写入。
            const QString drillName = holeName(pad.holeSize, pad.holeLength, pad.isPlated);
            output += QStringLiteral("...INTERNAL_PAD \"%1\"\n...HOLE_NAME \"%2\"\n").arg(baseName, drillName);
            output += QStringLiteral(".Hole \"%1\"\n..POSITIVE_TOLERANCE 0\n..NEGATIVE_TOLERANCE 0\n").arg(drillName);
            output += QStringLiteral("..HOLE_OPTIONS %1 DRILLED USER_GENERATED_NAME\n")
                          .arg(pad.isPlated ? QStringLiteral("PLATED") : QStringLiteral("NON_PLATED"));
            if (pad.holeLength > pad.holeSize) {
                // 槽孔长度和孔径分别写入目标格式的宽高字段，方向由 Cell 引脚旋转控制。
                output += QStringLiteral("..SLOT\n...WIDTH %1\n...HEIGHT %2\n")
                              .arg(fmt(toTh(pad.holeLength)))
                              .arg(fmt(toTh(pad.holeSize)));
            } else {
                output += QStringLiteral("..ROUND\n...DIAMETER %1\n").arg(fmt(toTh(pad.holeSize)));
            }
        }
        writtenStacks.insert(stackName);
    }
    for (const auto& hole : footprint.holes) {
        // IR 中没有焊盘的独立孔仍需转换为可定位的安装孔引脚。
        if (hole.radius <= 0.0)
            continue;
        const double diameterMm = hole.radius * 2.0;
        const QString baseName = holePadName(diameterMm);
        const QString stackName = standaloneHoleStackName(diameterMm);
        if (!writtenPads.contains(baseName)) {
            output += QStringLiteral(
                          ".PAD \"%1\"\n..PAD_OPTIONS USER_GENERATED_NAME\n..OFFSET (0, 0)\n..ROUND\n...DIAMETER %2\n")
                          .arg(baseName)
                          .arg(fmt(toTh(diameterMm)));
            writtenPads.insert(baseName);
        }
        if (writtenStacks.contains(stackName))
            continue;
        const QString drillName = holeName(diameterMm, 0.0, false);
        output += QStringLiteral(".PADSTACK \"%1\"\n..PADSTACK_TYPE PIN_THROUGH\n..TECHNOLOGY \"(Default)\"\n")
                      .arg(stackName);
        output += QStringLiteral(
                      "...TECHNOLOGY_OPTIONS NONE\n...TOP_PAD \"%1\"\n...BOTTOM_PAD \"%1\"\n...INTERNAL_PAD "
                      "\"%1\"\n...HOLE_NAME \"%2\"\n")
                      .arg(baseName, drillName);
        output += QStringLiteral(".Hole \"%1\"\n..POSITIVE_TOLERANCE 0\n..NEGATIVE_TOLERANCE 0\n").arg(drillName);
        output += QStringLiteral("..HOLE_OPTIONS NON_PLATED DRILLED USER_GENERATED_NAME\n..ROUND\n...DIAMETER %1\n")
                      .arg(fmt(toTh(diameterMm)));
        writtenStacks.insert(stackName);
    }
    return output.toUtf8();
}

// Cell 文件保存引脚、图形、文本和安装孔的相对位置。
QByteArray ExporterXpeditionFootprint::cellFile(const IR::FootprintComponentIR& footprint) const {
    // Cell 的局部原点取包围盒中心，能让没有明确原点的封装保持几何居中。
    const QRectF bounds = footprintBounds(footprint);
    const QPointF origin = bounds.center();
    QString output =
        QStringLiteral(".FILETYPE CELL_LIBRARY\n.VERSION \"1.01.01\"\n.CREATOR \"EasyKiConverter\"\n\n.UNITS TH\n\n");
    output += QStringLiteral(".PACKAGE_CELL \"%1\"\n ..NUMBER_LAYERS 2\n ..PACKAGE_GROUP General\n ..MOUNT_TYPE %2\n")
                  .arg(safeName(footprint.name),
                       footprint.hasThroughHolePads() || !footprint.holes.isEmpty() ? QStringLiteral("THROUGH")
                                                                                    : QStringLiteral("SURFACE"));

    int pinIndex = 1;
    for (const auto& pad : footprint.pads) {
        const QString stackName =
            padName(pad) + (pad.isThroughHole() ? QStringLiteral("_TH") : padstackSideSuffix(pad));
        output +=
            QStringLiteral(
                " ..PIN \"%1\"\n  ...XY (%2, %3)\n  ...PADSTACK \"%4\"\n  ...ROTATION %5\n  ...PIN_OPTIONS NONE\n")
                .arg(pad.number.isEmpty() ? QString::number(pinIndex) : pad.number)
                .arg(fmt(toTh(pad.position.x() - origin.x())))
                .arg(fmt(-toTh(pad.position.y() - origin.y())))
                .arg(stackName)
                .arg(fmt(pad.rotation));
        ++pinIndex;
    }

    const auto appendPolyline =
        [&](const LayerSection& layer, const QList<QPointF>& points, double width, bool filled) {
            // 目标坐标系的 Y 轴方向与 IR 相反，这里集中完成平移和翻转。
            if (layer.name.isEmpty() || points.isEmpty())
                return;
            QList<QPointF> normalized;
            for (const QPointF& point : points)
                normalized.append(QPointF(point.x() - origin.x(), -(point.y() - origin.y())));
            output += QStringLiteral(" ..%1\n  ...SIDE %2\n").arg(layer.name, layer.side);
            output += shapeBlock(
                filled ? QStringLiteral("POLYLINE_SHAPE") : QStringLiteral("POLYLINE_PATH"), normalized, width, filled);
        };
    for (const auto& outline : footprint.outlines)
        appendPolyline(layerSection(outline.layer), outline.points, outline.strokeWidth, false);
    for (const auto& track : footprint.tracks)
        appendPolyline(layerSection(track.layer), track.points, track.width, false);
    for (const auto& rect : footprint.rectangles)
        appendPolyline(layerSection(rect.layer), rectanglePoints(rect), rect.strokeWidth, false);
    for (const auto& circle : footprint.circles) {
        const LayerSection layer = layerSection(circle.layer);
        if (layer.name.isEmpty())
            continue;
        output += QStringLiteral(
                      " ..%1\n  ...SIDE %2\n   ...CIRCLE_PATH\n   ....WIDTH %3\n   ....XY (%4, %5)\n   ....RADIUS %6\n")
                      .arg(layer.name, layer.side)
                      .arg(fmt(toTh(circle.strokeWidth)))
                      .arg(fmt(toTh(circle.center.x() - origin.x())))
                      .arg(fmt(-toTh(circle.center.y() - origin.y())))
                      .arg(fmt(toTh(circle.radius)));
    }
    for (const auto& arc : footprint.arcs) {
        // 目标格式的封装图元统一使用折线，因此按最多 15 度的弦段近似圆弧。
        const LayerSection layer = layerSection(arc.layer);
        if (layer.name.isEmpty() || arc.radius <= 0.0)
            continue;
        double sweep = arc.endAngle - arc.startAngle;
        if (qFuzzyIsNull(sweep))
            sweep = 360.0;
        const int segments = qMax(8, static_cast<int>(std::ceil(std::abs(sweep) / 15.0)));
        QList<QPointF> points;
        points.reserve(segments + 1);
        for (int index = 0; index <= segments; ++index) {
            const double angle = qDegreesToRadians(arc.startAngle + sweep * index / segments);
            points.append(
                QPointF(arc.center.x() + arc.radius * std::cos(angle), arc.center.y() + arc.radius * std::sin(angle)));
        }
        appendPolyline(layer, points, arc.width, false);
    }
    for (const auto& region : footprint.regions) {
        // 填充区域必须闭合，否则目标工具会把它解释为开放轮廓。
        const LayerSection layer = layerSection(region.layer);
        if (layer.name.isEmpty() || region.vertices.size() < 3)
            continue;
        QList<QPointF> points = region.vertices;
        if (points.first() != points.last())
            points.append(points.first());
        appendPolyline(layer, points, 0.0, true);
    }
    int holeIndex = 1;
    for (const auto& hole : footprint.holes) {
        // 独立孔通过 MH 前缀的无电气引脚表达，保持其位置可被 Cell 引用。
        if (hole.radius <= 0.0)
            continue;
        output +=
            QStringLiteral(
                " ..PIN \"MH%1\"\n  ...XY (%2, %3)\n  ...PADSTACK \"%4\"\n  ...ROTATION 0\n  ...PIN_OPTIONS NONE\n")
                .arg(holeIndex++)
                .arg(fmt(toTh(hole.center.x() - origin.x())))
                .arg(fmt(-toTh(hole.center.y() - origin.y())))
                .arg(standaloneHoleStackName(hole.radius * 2.0));
    }
    for (const auto& text : footprint.texts) {
        // 仅导出可见文本；隐藏文本不应改变目标封装的视觉结果。
        if (!text.isDisplayed || text.text.isEmpty())
            continue;
        const LayerSection layer = layerSection(text.layer);
        if (layer.name.isEmpty())
            continue;
        const double height = text.fontSize > 0.0 ? toTh(text.fontSize) : 50.0;
        const double stroke = text.strokeWidth > 0.0 ? toTh(text.strokeWidth) : 3.0;
        output += QStringLiteral(" ..TEXT \"%1\"\n  ...TEXT_TYPE USER_DEFINED\n   ...DISPLAY_ATTR\n")
                      .arg(escapedText(text.text));
        output += QStringLiteral("    ....XY (%1, %2)\n    ....TEXT_LYR %3\n    ....HORZ_JUST CENTER\n")
                      .arg(fmt(toTh(text.position.x() - origin.x())))
                      .arg(fmt(-toTh(text.position.y() - origin.y())))
                      .arg(layer.name == QStringLiteral("SILKSCREEN_OUTLINE")
                               ? (layer.side == QStringLiteral("MNT_SIDE") ? QStringLiteral("SILKSCREEN_MNT_SIDE")
                                                                           : QStringLiteral("SILKSCREEN_OPP_SIDE"))
                               : layer.name);
        output +=
            QStringLiteral("    ....VERT_JUST CENTER\n    ....HEIGHT %1\n    ....WIDTH 0\n    ....STROKE_WIDTH %2\n")
                .arg(fmt(height))
                .arg(fmt(stroke));
        output += QStringLiteral("    ....ROTATION %1\n    ....FONT \"vf_std\"\n    ....TEXT_OPTIONS NONE\n")
                      .arg(fmt(text.rotation));
    }
    return output.toUtf8();
}

// 单个封装入口沿用批量导出实现，以保持诊断和命名规则一致。
bool ExporterXpeditionFootprint::exportFootprint(const IR::FootprintComponentIR& footprint,
                                                 const QString& filePath,
                                                 const QString& model3DPath) {
    // 单个封装复用批量路径，确保命名、诊断和 ZIP 写入行为保持一致。
    const bool success = exportFootprintLibrary({footprint}, footprint.name, filePath);
    if (!model3DPath.trimmed().isEmpty() && footprint.models3d.isEmpty())
        m_diagnostics.append(QStringLiteral("Xpedition 封装 %1 未写入 3D 模型关联").arg(footprint.name));
    return success;
}

// 批量入口负责去重名称、收集诊断并提交最终 ZIP 文件。
bool ExporterXpeditionFootprint::exportFootprintLibrary(const QList<IR::FootprintComponentIR>& footprints,
                                                        const QString&,
                                                        const QString& filePath,
                                                        bool,
                                                        bool,
                                                        const QString&,
                                                        const QString&,
                                                        bool,
                                                        const QString&) {
    // 每次批量导出都清空旧诊断，避免上一次任务的消息污染当前结果。
    m_diagnostics.clear();
    XpeditionZipWriter archive;
    QSet<QString> usedNames;
    for (const auto& footprint : footprints) {
        if (footprint.name.trimmed().isEmpty()) {
            m_diagnostics.append(QStringLiteral("跳过名称为空的 Xpedition 封装"));
            continue;
        }
        const QString baseName = safeName(footprint.name);
        QString name = baseName;
        int suffix = 2;
        while (usedNames.contains(name))
            name = QStringLiteral("%1_%2").arg(baseName).arg(suffix++);
        if (name != baseName)
            m_diagnostics.append(
                QStringLiteral("Xpedition 封装名称重复，已重命名：%1 -> %2").arg(footprint.name, name));
        for (const auto& pad : footprint.pads) {
            if (padShape(pad).isEmpty()) {
                m_diagnostics.append(QStringLiteral("Xpedition 封装 %1 的焊盘 %2 形状无法表达，导出已拒绝")
                                         .arg(footprint.name, pad.number));
                return false;
            }
        }
        if (!footprint.arcs.isEmpty())
            m_diagnostics.append(QStringLiteral("Xpedition 封装 %1 的 %2 个圆弧已使用折线近似")
                                     .arg(footprint.name)
                                     .arg(footprint.arcs.size()));
        if (!footprint.texts.isEmpty())
            m_diagnostics.append(
                QStringLiteral("Xpedition 封装 %1 已写入 %2 个文本").arg(footprint.name).arg(footprint.texts.size()));
        if (!footprint.regions.isEmpty())
            m_diagnostics.append(QStringLiteral("Xpedition 封装 %1 已写入 %2 个填充区域")
                                     .arg(footprint.name)
                                     .arg(footprint.regions.size()));
        if (!footprint.holes.isEmpty())
            m_diagnostics.append(QStringLiteral("Xpedition 封装 %1 已将 %2 个独立孔写为安装孔引脚")
                                     .arg(footprint.name)
                                     .arg(footprint.holes.size()));
        bool hasUnsupportedLayer = false;
        for (const auto& track : footprint.tracks)
            hasUnsupportedLayer |= layerSection(track.layer).name.isEmpty();
        for (const auto& rectangle : footprint.rectangles)
            hasUnsupportedLayer |= layerSection(rectangle.layer).name.isEmpty();
        for (const auto& circle : footprint.circles)
            hasUnsupportedLayer |= layerSection(circle.layer).name.isEmpty();
        for (const auto& arc : footprint.arcs)
            hasUnsupportedLayer |= layerSection(arc.layer).name.isEmpty();
        for (const auto& region : footprint.regions)
            hasUnsupportedLayer |= layerSection(region.layer).name.isEmpty();
        for (const auto& outline : footprint.outlines)
            hasUnsupportedLayer |= layerSection(outline.layer).name.isEmpty();
        for (const auto& text : footprint.texts)
            hasUnsupportedLayer |= layerSection(text.layer).name.isEmpty();
        if (hasUnsupportedLayer)
            m_diagnostics.append(QStringLiteral("Xpedition 封装 %1 包含无法映射的图层图元").arg(footprint.name));
        if (std::any_of(footprint.texts.cbegin(), footprint.texts.cend(), [](const IR::FootprintTextIR& text) {
                return text.mirror || !text.textPathPoints.isEmpty();
            })) {
            m_diagnostics.append(QStringLiteral("Xpedition 封装 %1 的文本镜像或路径字形未写入").arg(footprint.name));
        }
        if (std::any_of(footprint.regions.cbegin(), footprint.regions.cend(), [](const IR::FootprintRegionIR& region) {
                return region.isKeepOut && region.layer != IR::LayerType::KeepOut;
            })) {
            m_diagnostics.append(QStringLiteral("Xpedition 封装 %1 的 KeepOut 标志与图层不一致").arg(footprint.name));
        }
        if (footprint.shouldGenerateCourtyard)
            m_diagnostics.append(QStringLiteral("Xpedition 封装 %1 未自动生成 courtyard").arg(footprint.name));
        usedNames.insert(name);
        if (!archive.addFile(name + QStringLiteral("_Pads.hkp"), padstackFile(footprint)) ||
            !archive.addFile(name + QStringLiteral("_Cell.hkp"), cellFile(footprint))) {
            m_diagnostics.append(QStringLiteral("Xpedition 封装文件名重复：%1").arg(footprint.name));
            return false;
        }
        if (!footprint.models3d.isEmpty())
            m_diagnostics.append(QStringLiteral("Xpedition 封装 %1 未写入 3D 模型关联").arg(footprint.name));
    }
    if (archive.write(filePath))
        return true;
    m_diagnostics.append(QStringLiteral("无法写入 Xpedition 封装 ZIP：%1").arg(QFileInfo(filePath).fileName()));
    return false;
}

}  // namespace EasyKiConverter
