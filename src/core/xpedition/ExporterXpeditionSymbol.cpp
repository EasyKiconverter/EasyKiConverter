#include "ExporterXpeditionSymbol.h"

#include "XpeditionZipWriter.h"

#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>
#include <QtMath>

#include <algorithm>

namespace EasyKiConverter {

namespace {

constexpr double kThousandthInchMm = 0.0254;

/** @brief 将毫米转换为目标格式的千分之一英寸单位。 */
double toTh(double valueMm) {
    return valueMm / kThousandthInchMm;
}

/** @brief 清理符号名称，使其可作为 ZIP 条目名称。 */
QString safeName(QString name) {
    name = name.trimmed();
    name.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9_.-]")), QStringLiteral("_"));
    return name.isEmpty() ? QStringLiteral("unnamed") : name;
}

/**
 * @brief 转义 Xpedition 符号文本字段中的特殊字符。
 * @param text 原始文本。
 * @return 可安全写入单行字段的文本。
 */
QString escapedText(QString text) {
    text.replace('\\', QStringLiteral("\\\\"));
    text.replace('"', QStringLiteral("\\\""));
    text.replace('\n', QStringLiteral(" "));
    text.replace('\r', QStringLiteral(" "));
    return text;
}

/** @brief 使用稳定精度输出几何数值。 */
QString fmt(double value) {
    return QString::number(value, 'f', 4);
}

/**
 * @brief 将 IR 引脚电气类型转换为符号文件中的 PINTYPE 值。
 * @param type IR 中已经归一化的引脚电气类型。
 * @return 目标格式类型字符串；未指定类型返回空字符串。
 */
QString pinElectricalTypeName(IR::PinElectricalType type) {
    // 未指定类型不写入属性，避免用猜测覆盖目标工具的默认语义。
    switch (type) {
        case IR::PinElectricalType::Input:
            return QStringLiteral("IN");
        case IR::PinElectricalType::Output:
            return QStringLiteral("OUT");
        case IR::PinElectricalType::Bidirectional:
            return QStringLiteral("BI");
        case IR::PinElectricalType::Passive:
            return QStringLiteral("PASSIVE");
        case IR::PinElectricalType::Power:
            return QStringLiteral("PWR");
        case IR::PinElectricalType::OpenCollector:
            return QStringLiteral("OCL");
        case IR::PinElectricalType::OpenEmitter:
            return QStringLiteral("OEM");
        case IR::PinElectricalType::Unspecified:
        default:
            return {};
    }
}

/** @brief 将引脚方向转换为单位方向向量。 */
QPointF directionVector(IR::PinDirection direction) {
    // 方向枚举到单位向量的转换决定引脚内外端点的几何关系。
    switch (direction) {
        case IR::PinDirection::Left:
            return {-1.0, 0.0};
        case IR::PinDirection::Up:
            return {0.0, 1.0};
        case IR::PinDirection::Down:
            return {0.0, -1.0};
        case IR::PinDirection::Right:
        default:
            return {1.0, 0.0};
    }
}

/**
 * @brief 获取引脚名称或编号的可靠显示位置。
 * @param position IR 中记录的文本位置。
 * @param hasPosition 是否确认该文本位置来自有效的源数据。
 * @param pinPosition 引脚连接点位置。
 * @return 有效文本位置；源位置缺失时回退到引脚连接点。
 */
QPointF pinDisplayPosition(const QPointF& position, bool hasPosition, const QPointF& pinPosition) {
    return hasPosition ? position : pinPosition;
}

/** @brief 写入一个符号直线图元及其默认图形样式。 */
void appendLine(QString& output, double x1, double y1, double x2, double y2) {
    output += QStringLiteral("l 2 %1 %2 %3 %4\n|GRPHSTL -1 0 0 1\n")
                  .arg(fmt(toTh(x1)), fmt(toTh(y1)), fmt(toTh(x2)), fmt(toTh(y2)));
}

/** @brief 将点列拆分为目标格式可识别的连续线段。 */
void appendPolyline(QString& output, const QList<QPointF>& points) {
    for (int i = 1; i < points.size(); ++i)
        appendLine(output, points[i - 1].x(), points[i - 1].y(), points[i].x(), points[i].y());
}

/**
 * @brief 计算一个符号部件的几何包围盒。
 * @param symbol 符号统一表示。
 * @param partIndex 当前部件索引。
 * @return 包含引脚和已支持图元的包围盒，单位为毫米。
 */
QRectF symbolBounds(const IR::SymbolComponentIR& symbol, int partIndex) {
    QRectF bounds;
    bool initialized = false;
    const auto addPoint = [&](const QPointF& point) {
        if (!initialized) {
            bounds = QRectF(point, QSizeF(0.0, 0.0));
            initialized = true;
            return;
        }
        bounds.setLeft(qMin(bounds.left(), point.x()));
        bounds.setTop(qMin(bounds.top(), point.y()));
        bounds.setRight(qMax(bounds.right(), point.x()));
        bounds.setBottom(qMax(bounds.bottom(), point.y()));
    };
    for (const IR::SymbolPinIR& pin : symbol.pins) {
        if (pin.partIndex != partIndex && !pin.commonToAllParts)
            continue;
        addPoint(pin.position);
        addPoint(pin.position - directionVector(pin.direction) * pin.length);
    }
    for (const IR::SymbolRectangleIR& rect : symbol.rectangles) {
        if (rect.partIndex != partIndex)
            continue;
        addPoint(QPointF(rect.x0, rect.y0));
        addPoint(QPointF(rect.x1, rect.y1));
    }
    const auto addPointList = [&](const QList<QPointF>& points, int elementPartIndex) {
        if (elementPartIndex != partIndex)
            return;
        for (const QPointF& point : points)
            addPoint(point);
    };
    for (const IR::SymbolPolylineIR& polyline : symbol.polylines)
        addPointList(polyline.points, polyline.partIndex);
    for (const IR::SymbolPolygonIR& polygon : symbol.polygons)
        addPointList(polygon.points, polygon.partIndex);
    for (const IR::SymbolCircleIR& circle : symbol.circles) {
        if (circle.partIndex != partIndex)
            continue;
        addPoint(circle.center - QPointF(circle.radius, circle.radius));
        addPoint(circle.center + QPointF(circle.radius, circle.radius));
    }
    for (const IR::SymbolArcIR& arc : symbol.arcs) {
        if (arc.partIndex != partIndex)
            continue;
        addPoint(arc.startPoint);
        addPoint(arc.midPoint);
        addPoint(arc.endPoint);
    }
    return initialized ? bounds.normalized() : QRectF(-1.0, -1.0, 2.0, 2.0);
}

/**
 * @brief 写入一个符号引脚及可选的名称、编号文本。
 * @param output 输出缓冲区。
 * @param pin 统一表示中的引脚。
 * @param index 目标文件中的引脚序号。
 */
void appendPin(QString& output, const IR::SymbolPinIR& pin, int index) {
    const QPointF direction = directionVector(pin.direction);
    const QPointF outer = pin.position;
    const QPointF inner = outer - direction * pin.length;
    const int side = pin.direction == IR::PinDirection::Left   ? 3
                     : pin.direction == IR::PinDirection::Up   ? 0
                     : pin.direction == IR::PinDirection::Down ? 1
                                                               : 2;
    output += QStringLiteral("P %1 %2 %3 %4 %5 0 %6 0\n")
                  .arg(index)
                  .arg(fmt(toTh(outer.x())))
                  .arg(fmt(toTh(outer.y())))
                  .arg(fmt(toTh(inner.x())))
                  .arg(fmt(toTh(inner.y())))
                  .arg(side);
    const QPointF namePosition = pinDisplayPosition(pin.namePosition, pin.hasNamePosition, outer);
    const QPointF numberPosition = pinDisplayPosition(pin.numberPosition, pin.hasNumberPosition, outer);
    if (pin.display.showName && !pin.name.isEmpty())
        output += QStringLiteral("L %1 %2 8 0 2 0 1 0 %3\n")
                      .arg(fmt(toTh(namePosition.x())))
                      .arg(fmt(toTh(namePosition.y())))
                      .arg(escapedText(pin.name));
    if (pin.display.showDesignator && !pin.designator.isEmpty())
        output += QStringLiteral("A %1 %2 8 0 3 3 #=%4\n")
                      .arg(fmt(toTh(numberPosition.x())))
                      .arg(fmt(toTh(numberPosition.y())))
                      .arg(escapedText(pin.designator));
    const QString electricalType = pinElectricalTypeName(pin.electricalType);
    if (!electricalType.isEmpty())
        output += QStringLiteral("A %1 %2 10 0 2 0 PINTYPE=%3\n")
                      .arg(fmt(toTh(outer.x())))
                      .arg(fmt(toTh(outer.y())))
                      .arg(electricalType);
}

}  // namespace

// 符号归档中的每个条目都是 ASCII 部件文件，统一使用符号扩展名。
QString ExporterXpeditionSymbol::libraryFileExtension() const {
    // 符号库同样使用 ZIP，但条目内容是 ASCII 符号部件文件。
    return QStringLiteral("_Symbols.zip");
}

// 诊断信息由符号批量入口生成，读取操作不改变导出器状态。
QStringList ExporterXpeditionSymbol::diagnostics() const {
    // 诊断信息描述未支持图元和文件写入失败，供 CLI 与 GUI 共用。
    return m_diagnostics;
}

// 多单元符号通过“名称.部件序号”形成稳定的库条目名称。
QString ExporterXpeditionSymbol::symbolFileName(const QString& outputName, int partIndex) const {
    // 部件序号从 1 开始，符合目标符号库的多单元命名约定。
    return QStringLiteral("%1.%2").arg(outputName).arg(partIndex + 1);
}

// 符号部件正文按照头信息、引脚、图元和结束记录的顺序拼接。
QByteArray ExporterXpeditionSymbol::symbolFile(const IR::SymbolComponentIR& symbol,
                                               int partIndex,
                                               const QString& outputName) const {
    // 每个部件独立成文件，同时保留 commonToAllParts 引脚，避免多单元符号丢失公共引脚。
    QString body;
    body += QStringLiteral("V 50\nK 1000000000 %1\nY 1\nZ 0\ni 0\n").arg(outputName);
    body += QStringLiteral("U 0 0 10 0 5 0 %1\n").arg(outputName);
    body += QStringLiteral("U 0 0 5 0 5 0 REFDES=%1\n").arg(escapedText(symbol.designatorPrefix));
    body += QStringLiteral("U 0 0 5 0 5 0 VALUE=%1\n").arg(escapedText(symbol.name));
    const QRectF bounds = symbolBounds(symbol, partIndex);
    body += QStringLiteral("b %1 %2 %3 %4\n")
                .arg(fmt(toTh(bounds.left())))
                .arg(fmt(toTh(bounds.top())))
                .arg(fmt(toTh(bounds.right())))
                .arg(fmt(toTh(bounds.bottom())));

    int pinIndex = 1;
    for (const IR::SymbolPinIR& pin : symbol.pins) {
        if (pin.partIndex == partIndex || pin.commonToAllParts)
            appendPin(body, pin, pinIndex++);
    }
    for (const IR::SymbolRectangleIR& rect : symbol.rectangles) {
        if (rect.partIndex != partIndex)
            continue;
        appendLine(body, rect.x0, rect.y0, rect.x1, rect.y0);
        appendLine(body, rect.x1, rect.y0, rect.x1, rect.y1);
        appendLine(body, rect.x1, rect.y1, rect.x0, rect.y1);
        appendLine(body, rect.x0, rect.y1, rect.x0, rect.y0);
    }
    for (const IR::SymbolPolylineIR& polyline : symbol.polylines) {
        if (polyline.partIndex == partIndex)
            appendPolyline(body, polyline.points);
    }
    for (const IR::SymbolPolygonIR& polygon : symbol.polygons) {
        if (polygon.partIndex == partIndex)
            appendPolyline(body, polygon.points);
    }
    for (const IR::SymbolCircleIR& circle : symbol.circles) {
        if (circle.partIndex != partIndex)
            continue;
        body += QStringLiteral("c %1 %2 %3\n|GRPHSTL_EXT01 255 -1 0 1 1\n")
                    .arg(fmt(toTh(circle.center.x())))
                    .arg(fmt(toTh(circle.center.y())))
                    .arg(fmt(toTh(circle.radius)));
    }
    for (const IR::SymbolArcIR& arc : symbol.arcs) {
        if (arc.partIndex != partIndex)
            continue;
        body += QStringLiteral("a %1 %2 %3 %4 %5 %6\n|GRPHSTL_EXT01 255 -1 0 1 1\n")
                    .arg(fmt(toTh(arc.startPoint.x())))
                    .arg(fmt(toTh(arc.startPoint.y())))
                    .arg(fmt(toTh(arc.midPoint.x())))
                    .arg(fmt(toTh(arc.midPoint.y())))
                    .arg(fmt(toTh(arc.endPoint.x())))
                    .arg(fmt(toTh(arc.endPoint.y())));
    }
    body += QStringLiteral("E\n");
    return body.toUtf8();
}

// 单符号入口委托批量入口，避免两条路径产生不同的归档结构。
bool ExporterXpeditionSymbol::exportSymbol(const IR::SymbolComponentIR& symbol, const QString& filePath) {
    // 单符号入口委托批量入口，避免两条路径产生不同的 ZIP 结构。
    return exportSymbolLibrary({symbol}, symbol.name, filePath, false, false);
}

// 批量入口负责清理诊断、展开多单元并写入符号 ZIP。
bool ExporterXpeditionSymbol::exportSymbolLibrary(const QList<IR::SymbolComponentIR>& symbols,
                                                  const QString&,
                                                  const QString& filePath,
                                                  bool,
                                                  bool,
                                                  const QString&) {
    // 清理任务级诊断后再写入各个部件，保证调用者读取到的是本次结果。
    m_diagnostics.clear();
    XpeditionZipWriter archive;
    QSet<QString> usedNames;
    for (const IR::SymbolComponentIR& symbol : symbols) {
        if (symbol.name.trimmed().isEmpty()) {
            m_diagnostics.append(QStringLiteral("跳过名称为空的 Xpedition 符号"));
            continue;
        }
        const QString baseName = safeName(symbol.name);
        QString outputName = baseName;
        int suffix = 2;
        while (usedNames.contains(outputName))
            outputName = QStringLiteral("%1_%2").arg(baseName).arg(suffix++);
        if (outputName != baseName)
            m_diagnostics.append(
                QStringLiteral("Xpedition 符号名称重复，已重命名：%1 -> %2").arg(symbol.name, outputName));
        usedNames.insert(outputName);
        const int partCount = qMax(1, symbol.partCount);
        if (!symbol.ellipses.isEmpty() || !symbol.pies.isEmpty() || !symbol.ellipticalArcs.isEmpty() ||
            !symbol.paths.isEmpty() || !symbol.beziers.isEmpty() || !symbol.ieeeSymbols.isEmpty() ||
            !symbol.texts.isEmpty() || !symbol.textFrames.isEmpty() || !symbol.images.isEmpty()) {
            m_diagnostics.append(QStringLiteral("Xpedition 符号 %1 包含当前未写入的 IR 图元").arg(symbol.name));
        }
        if (std::any_of(symbol.pins.cbegin(), symbol.pins.cend(), [](const IR::SymbolPinIR& pin) {
                return pin.style.hasAny();
            })) {
            m_diagnostics.append(QStringLiteral("Xpedition 符号 %1 包含当前未写入的引脚装饰").arg(symbol.name));
        }
        for (int partIndex = 0; partIndex < partCount; ++partIndex) {
            if (!archive.addFile(symbolFileName(outputName, partIndex), symbolFile(symbol, partIndex, outputName))) {
                m_diagnostics.append(QStringLiteral("Xpedition 符号文件名重复：%1").arg(symbol.name));
                return false;
            }
        }
    }
    if (archive.write(filePath))
        return true;
    m_diagnostics.append(QStringLiteral("无法写入 Xpedition 符号 ZIP：%1").arg(QFileInfo(filePath).fileName()));
    return false;
}

}  // namespace EasyKiConverter
