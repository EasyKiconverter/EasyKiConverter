#include "ExporterPadsSymbol.h"

#include <QDateTime>
#include <QFile>
#include <QRegularExpression>
#include <QTextStream>

#include <algorithm>
#include <cmath>

namespace EasyKiConverter {
namespace {

constexpr double kMilPerMm = 39.37007874015748;

// 将用户名称清洗为 PADS 允许的 ASCII Decal 标识符。
QString safeName(const QString& value) {
    QString result;
    for (const QChar character : value) {
        const ushort code = character.unicode();
        if ((code >= 'A' && code <= 'Z') || (code >= 'a' && code <= 'z') || (code >= '0' && code <= '9') ||
            character == QChar('_') || character == QChar('-')) {
            result.append(character);
        } else {
            result.append(QChar('_'));
        }
    }
    return result.left(40);
}

// 将统一 IR 的毫米坐标转换为 PADS 规范使用的 mil 文本。
QString number(double value) {
    return QString::number(value * kMilPerMm, 'f', 6);
}

// 将符号文本压缩为单行，避免破坏 PADS 的两行文本记录结构。
QString textValue(const QString& value) {
    return value.simplified().replace(QChar('\n'), QChar(' '));
}

// 检查坐标是否可以安全写入目标格式。
bool finitePoint(const QPointF& point) {
    return std::isfinite(point.x()) && std::isfinite(point.y());
}

// 检查线宽是否为目标格式可接受的有限非负数。
bool finiteWidth(double width) {
    return std::isfinite(width) && width >= 0.0;
}

// 写入 PADS Schematic Decal 的通用折线或闭合图元。
bool writePiece(QTextStream& stream, const QString& type, const QList<QPointF>& points, double width) {
    if (points.isEmpty() || !finiteWidth(width))
        return false;
    stream << type << ' ' << points.size() << ' ' << number(width) << " 0 -1\n";
    for (const QPointF& point : points)
        stream << number(point.x()) << ' ' << number(-point.y()) << '\n';
    return stream.status() == QTextStream::Ok;
}

// 在写入部件前校验目标格式无法表达的图元和字段。
bool validatePart(const IR::SymbolComponentIR& symbol, int part, QStringList& diagnostics) {
    if (safeName(symbol.name).isEmpty()) {
        diagnostics.append(QStringLiteral("PADS: 符号名称清洗后为空: %1").arg(symbol.name));
        return false;
    }
    if (part < 0 || part >= qMax(1, symbol.partCount)) {
        diagnostics.append(QStringLiteral("PADS: 符号 %1 的部件索引无效: %2").arg(symbol.name).arg(part));
        return false;
    }
    if (!symbol.arcs.isEmpty() || !symbol.ellipses.isEmpty() || !symbol.pies.isEmpty() ||
        !symbol.ellipticalArcs.isEmpty() || !symbol.paths.isEmpty() || !symbol.beziers.isEmpty() ||
        !symbol.images.isEmpty() || !symbol.textFrames.isEmpty()) {
        diagnostics.append(
            QStringLiteral("PADS: 符号 %1 含当前 Schematic Decal writer 无法无损表达的图元").arg(symbol.name));
        return false;
    }
    for (const IR::SymbolPinIR& pin : symbol.pins) {
        if (pin.partIndex != part && !pin.commonToAllParts)
            continue;
        if (pin.designator.isEmpty() || pin.designator.contains(QRegularExpression(QStringLiteral("[\\s\\\"]"))) ||
            !finitePoint(pin.position) || !std::isfinite(pin.length) || pin.length < 0.0) {
            diagnostics.append(
                QStringLiteral("PADS: 符号 %1 的引脚编号、坐标或长度无效: %2").arg(symbol.name).arg(pin.designator));
            return false;
        }
        if (pin.name.contains(QRegularExpression(QStringLiteral("[\\s\\\"]")))) {
            diagnostics.append(
                QStringLiteral("PADS: 符号 %1 的引脚名称不能包含空格或引号: %2").arg(symbol.name).arg(pin.name));
            return false;
        }
    }
    for (const IR::SymbolTextIR& text : symbol.texts) {
        if (text.partIndex != part || !text.visible)
            continue;
        if (!finitePoint(text.position) || !std::isfinite(text.rotation) || !std::isfinite(text.fontSizeMm) ||
            text.fontSizeMm < 0.0) {
            diagnostics.append(QStringLiteral("PADS: 符号 %1 的文本几何无效").arg(symbol.name));
            return false;
        }
    }
    return true;
}

// 统计指定部件的 PADS 图元数量，用于生成符号头部。
int pieceCount(const IR::SymbolComponentIR& symbol, int part) {
    int count = 0;
    for (const auto& rectangle : symbol.rectangles)
        count += rectangle.partIndex == part ? 1 : 0;
    for (const auto& circle : symbol.circles)
        count += circle.partIndex == part ? 1 : 0;
    for (const auto& polyline : symbol.polylines)
        count += polyline.partIndex == part && polyline.points.size() >= 2 ? 1 : 0;
    for (const auto& polygon : symbol.polygons)
        count += polygon.partIndex == part && polygon.points.size() >= 3 ? 1 : 0;
    return count;
}

// 统计指定部件的可见文本数量。
int textCount(const IR::SymbolComponentIR& symbol, int part) {
    int count = 0;
    for (const auto& text : symbol.texts)
        count += text.partIndex == part && text.visible && !text.text.isEmpty() ? 1 : 0;
    return count;
}

// 统计指定部件及公共部件的引脚数量。
int pinCount(const IR::SymbolComponentIR& symbol, int part) {
    int count = 0;
    for (const auto& pin : symbol.pins)
        count += pin.partIndex == part || pin.commonToAllParts ? 1 : 0;
    return count;
}

// 将一个 IR 部件写成独立的 PADS Schematic Decal 记录。
bool writePart(QTextStream& stream, const IR::SymbolComponentIR& symbol, int part, QStringList& diagnostics) {
    const int pieces = pieceCount(symbol, part);
    const int texts = textCount(symbol, part);
    const int pins = pinCount(symbol, part);
    const QString baseName = safeName(symbol.name);
    const QString partName = part == 0 ? baseName : baseName + QStringLiteral("_P%1").arg(part + 1);
    stream << partName << " 0 0 50 5 50 5 2 " << pieces << ' ' << texts << ' ' << pins << " 0\n";
    stream << "TIMESTAMP " << QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyy.MM.dd.hh.mm.ss")) << "\n";
    stream << "Default Font\nDefault Font\n";
    stream << "0 0 0 12 50 5 Default Font\nREF-DES\n";
    stream << "0 0 0 12 50 5 Default Font\nPARTTYPE\n";

    for (const auto& rectangle : symbol.rectangles) {
        if (rectangle.partIndex != part)
            continue;
        if (!finiteWidth(rectangle.strokeWidth) || !std::isfinite(rectangle.x0) || !std::isfinite(rectangle.y0) ||
            !std::isfinite(rectangle.x1) || !std::isfinite(rectangle.y1) || rectangle.cornerRadiusX > 0.0 ||
            rectangle.cornerRadiusY > 0.0) {
            diagnostics.append(QStringLiteral("PADS: 符号 %1 的矩形包含非法值或圆角，无法无损写入").arg(symbol.name));
            return false;
        }
        const QList<QPointF> points = {{rectangle.x0, rectangle.y0},
                                       {rectangle.x1, rectangle.y0},
                                       {rectangle.x1, rectangle.y1},
                                       {rectangle.x0, rectangle.y1},
                                       {rectangle.x0, rectangle.y0}};
        if (!writePiece(stream, QStringLiteral("CLOSED"), points, rectangle.strokeWidth))
            return false;
    }
    for (const auto& circle : symbol.circles) {
        if (circle.partIndex != part || !finitePoint(circle.center) || !std::isfinite(circle.radius) ||
            circle.radius <= 0.0 || !finiteWidth(circle.strokeWidth)) {
            if (circle.partIndex == part)
                diagnostics.append(QStringLiteral("PADS: 符号 %1 的圆图元无效").arg(symbol.name));
            if (circle.partIndex == part)
                return false;
            continue;
        }
        const QList<QPointF> points = {{circle.center.x() - circle.radius, circle.center.y() - circle.radius},
                                       {circle.center.x() + circle.radius, circle.center.y() + circle.radius}};
        if (!writePiece(stream, QStringLiteral("CIRCLE"), points, circle.strokeWidth))
            return false;
    }
    for (const auto& polyline : symbol.polylines) {
        if (polyline.partIndex != part)
            continue;
        if (polyline.points.size() < 2 || !finiteWidth(polyline.strokeWidth) ||
            !std::all_of(polyline.points.cbegin(), polyline.points.cend(), finitePoint)) {
            diagnostics.append(QStringLiteral("PADS: 符号 %1 的折线图元无效").arg(symbol.name));
            return false;
        }
        if (!writePiece(stream, QStringLiteral("OPEN"), polyline.points, polyline.strokeWidth))
            return false;
    }
    for (const auto& polygon : symbol.polygons) {
        if (polygon.partIndex != part)
            continue;
        if (polygon.points.size() < 3 || !finiteWidth(polygon.strokeWidth) ||
            !std::all_of(polygon.points.cbegin(), polygon.points.cend(), finitePoint)) {
            diagnostics.append(QStringLiteral("PADS: 符号 %1 的多边形图元无效").arg(symbol.name));
            return false;
        }
        QList<QPointF> points = polygon.points;
        points.append(points.first());
        if (!writePiece(stream, QStringLiteral("CLOSED"), points, polygon.strokeWidth))
            return false;
    }
    for (const auto& text : symbol.texts) {
        if (text.partIndex != part || !text.visible || text.text.isEmpty())
            continue;
        const double font = text.fontSizeMm > 0.0 ? text.fontSizeMm : 1.27;
        stream << number(text.position.x()) << ' ' << number(-text.position.y()) << ' '
               << QString::number(text.rotation) << " 0 " << number(font) << " " << number(0.127)
               << " 0 12 0 0 Default Font\n"
               << textValue(text.text) << '\n';
    }
    for (const auto& pin : symbol.pins) {
        if (pin.partIndex != part && !pin.commonToAllParts)
            continue;
        stream << "T " << number(pin.position.x()) << ' ' << number(-pin.position.y()) << " 0 0 0 0 0 12 0 0 0 12 PIN\n"
               << "P 0 0 0 12 0 0 0 12 192\n";
    }
    return stream.status() == QTextStream::Ok;
}

}  // namespace

// 返回 PADS Schematic Decal 文件后缀，符号和封装输出保持可区分。
QString ExporterPadsSymbol::libraryFileExtension() const {
    return QStringLiteral("_PADS.c");
}

// 单符号导出复用库级 writer，确保头尾记录和多部件规则一致。
bool ExporterPadsSymbol::exportSymbol(const IR::SymbolComponentIR& symbol, const QString& filePath) {
    return exportSymbolLibrary({symbol}, safeName(symbol.name), filePath, false, false);
}

// 导出完整 Schematic Decal 文件，并拒绝未经实现的合并语义。
bool ExporterPadsSymbol::exportSymbolLibrary(const QList<IR::SymbolComponentIR>& symbols,
                                             const QString&,
                                             const QString& filePath,
                                             bool appendMode,
                                             bool updateMode,
                                             const QString&) {
    m_diagnostics.clear();
    if (appendMode || updateMode) {
        m_diagnostics.append(QStringLiteral("PADS Schematic Decal 当前只支持完整重写，不支持追加或更新模式"));
        return false;
    }
    if (symbols.isEmpty()) {
        m_diagnostics.append(QStringLiteral("PADS: 没有可导出的符号"));
        return false;
    }
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        m_diagnostics.append(QStringLiteral("PADS: 无法写入符号库 %1").arg(filePath));
        return false;
    }
    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    stream << "*PADS-LIBRARY-SCH-DECALS-V9*\n";
    for (const auto& symbol : symbols) {
        const int count = qMax(1, symbol.partCount);
        for (int part = 0; part < count; ++part) {
            if (!validatePart(symbol, part, m_diagnostics) || !writePart(stream, symbol, part, m_diagnostics))
                return false;
        }
    }
    stream << "*END*\n";
    stream.flush();
    if (stream.status() != QTextStream::Ok) {
        m_diagnostics.append(QStringLiteral("PADS: 写入符号库时发生 I/O 错误"));
        return false;
    }
    m_diagnostics.append(QStringLiteral("PADS: 当前输出为 Schematic Decal 符号库，不包含 Part Type 器件关联"));
    return true;
}

// 返回最近一次导出的结构限制和失败原因。
QStringList ExporterPadsSymbol::diagnostics() const {
    return m_diagnostics;
}

}  // namespace EasyKiConverter
