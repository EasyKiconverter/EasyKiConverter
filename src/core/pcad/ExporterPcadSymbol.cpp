#include "ExporterPcadSymbol.h"

#include <QFile>
#include <QSet>
#include <QTextStream>

#include <cmath>

namespace EasyKiConverter {
namespace {

constexpr double kMillimetersPerMil = 0.0254;  ///< 毫米到 mil 的换算系数。

/**
 * @brief 按 P-CAD ASCII 字符串规则转义字段。
 * @param value 原始字段值。
 * @return 转义后的带引号字段。
 */
QString quote(const QString& value) {
    QString escaped = value;  // 可修改的字段副本。
    escaped.replace(QChar('\\'), QStringLiteral("\\\\"));
    escaped.replace(QChar('"'), QStringLiteral("\\\""));
    return QStringLiteral("\"%1\"").arg(escaped);
}

/**
 * @brief 将统一 IR 的毫米坐标转换为 P-CAD 使用的 mil 文本。
 * @param value 毫米数值。
 * @return mil 文本。
 */
QString number(double value) {
    return QString::number(value / kMillimetersPerMil, 'f', 6);
}

/**
 * @brief 将符号名称清洗为稳定的 ASCII 标识符。
 * @param value 原始名称。
 * @return 清洗后的 ASCII 名称。
 */
QString safeName(const QString& value) {
    QString result;  // 清洗后的名称。
    for (const QChar character : value) {  // 逐字符处理原始名称。
        const ushort code = character.unicode();  // 当前字符编码。
        if ((code >= 'A' && code <= 'Z') || (code >= 'a' && code <= 'z') || (code >= '0' && code <= '9') ||
            character == QChar('_') || character == QChar('-'))
            result.append(character);
        else
            result.append(QChar('_'));
    }
    return result.left(64);
}

/**
 * @brief 检查文本是否仅包含 ASCII 字符。
 * @param value 待检查文本。
 * @return 仅含 ASCII 字符时返回 true。
 */
bool isAscii(const QString& value) {
    for (const unsigned char byte : value.toUtf8()) {
        if (byte >= 0x80)
            return false;
    }
    return true;
}

/**
 * @brief 将统一符号方向转换为 P-CAD 的角度约定。
 * @param direction 统一 IR 引脚方向。
 * @return P-CAD 旋转角度。
 */
double pinRotation(IR::PinDirection direction) {
    // 方向映射保持引脚连接端朝向，不能用绝对坐标重新猜测旋转。
    switch (direction) {
        case IR::PinDirection::Right:
            return 0.0;
        case IR::PinDirection::Up:
            return 90.0;
        case IR::PinDirection::Left:
            return 180.0;
        case IR::PinDirection::Down:
            return 270.0;
    }
    return 0.0;
}

/**
 * @brief 将电气类型转换为 P-CAD compPin 可识别的文本枚举。
 * @param type 统一 IR 电气类型。
 * @return P-CAD 引脚类型文本。
 */
QString pinType(IR::PinElectricalType type) {
    // 未指定类型按无源处理，避免把不确定语义误报为输入或输出。
    switch (type) {
        case IR::PinElectricalType::Input:
            return QStringLiteral("Input");
        case IR::PinElectricalType::Output:
            return QStringLiteral("Output");
        case IR::PinElectricalType::Bidirectional:
            return QStringLiteral("Bidirectional");
        case IR::PinElectricalType::Power:
            return QStringLiteral("Power");
        case IR::PinElectricalType::OpenCollector:
            return QStringLiteral("OpenH");
        case IR::PinElectricalType::OpenEmitter:
            return QStringLiteral("OpenL");
        case IR::PinElectricalType::Passive:
        case IR::PinElectricalType::Unspecified:
        default:
            return QStringLiteral("Passive");
    }
}

/**
 * @brief 检查坐标是否为有限数。
 * @param point 待检查坐标。
 * @return 坐标有限时返回 true。
 */
bool finitePoint(const QPointF& point) {
    return std::isfinite(point.x()) && std::isfinite(point.y());
}

/**
 * @brief 写入 P-CAD 的 poly 图元。
 * @param stream P-CAD 文本流。
 * @param points 图元顶点列表。
 * @return 写入成功时返回 true。
 */
bool writePoly(QTextStream& stream, const QList<QPointF>& points) {
    if (points.size() < 2)
        return false;
    stream << "        (poly\n";
    for (const QPointF& point : points)
        stream << "          (pt " << number(point.x()) << ' ' << number(-point.y()) << ")\n";
    stream << "        )\n";
    return stream.status() == QTextStream::Ok;
}

/**
 * @brief 检查 P-CAD ASCII 无法表达的符号字段和图元。
 * @param symbol 待检查符号。
 * @param diagnostics 诊断信息输出列表。
 * @return 符号可导出时返回 true。
 */
bool validateSymbol(const IR::SymbolComponentIR& symbol, QStringList& diagnostics) {
    if (safeName(symbol.name).isEmpty()) {
        diagnostics.append(QStringLiteral("P-CAD: 符号名称清洗后为空：%1").arg(symbol.name));
        return false;
    }
    if (!isAscii(symbol.name) || !isAscii(symbol.designatorPrefix)) {
        diagnostics.append(QStringLiteral("P-CAD: 符号名称和位号前缀必须是 ASCII：%1").arg(symbol.name));
        return false;
    }
    if (symbol.partCount < 1) {
        diagnostics.append(QStringLiteral("P-CAD: 符号 %1 的部件数量无效").arg(symbol.name));
        return false;
    }
    if (!symbol.arcs.isEmpty() || !symbol.circles.isEmpty() || !symbol.ellipses.isEmpty() || !symbol.pies.isEmpty() ||
        !symbol.ellipticalArcs.isEmpty() || !symbol.paths.isEmpty() || !symbol.beziers.isEmpty() ||
        !symbol.ieeeSymbols.isEmpty() || !symbol.texts.isEmpty() || !symbol.textFrames.isEmpty() ||
        !symbol.images.isEmpty()) {
        diagnostics.append(QStringLiteral("P-CAD: 符号 %1 含当前 ASCII writer 无法无损表达的图元").arg(symbol.name));
        return false;
    }
    for (const IR::SymbolPinIR& pin : symbol.pins) {
        if (pin.designator.isEmpty() || !isAscii(pin.designator) || !isAscii(pin.name) || !finitePoint(pin.position) ||
            !std::isfinite(pin.length) || pin.length < 0.0 || pin.partIndex < 0 || pin.partIndex >= symbol.partCount) {
            diagnostics.append(QStringLiteral("P-CAD: 符号 %1 的引脚字段无效：%2").arg(symbol.name, pin.designator));
            return false;
        }
    }
    return true;
}

/**
 * @brief 写入 P-CAD 符号定义。
 * @param stream P-CAD 文本流。
 * @param symbol 符号 IR。
 * @param name 当前部件名称。
 * @param part 部件索引。
 * @param diagnostics 诊断信息输出列表。
 * @return 写入成功时返回 true。
 */
bool writeSymbolDef(QTextStream& stream,
                    const IR::SymbolComponentIR& symbol,
                    const QString& name,
                    int part,
                    QStringList& diagnostics) {
    stream << "    (symbolDef " << quote(name) << "\n"
           << "      (originalName " << quote(symbol.name) << ")\n";
    for (const IR::SymbolPinIR& pin : symbol.pins) {
        if (pin.partIndex != part && !pin.commonToAllParts)
            continue;
        stream << "      (pin " << quote(pin.designator) << "\n"
               << "        (pt " << number(pin.position.x()) << ' ' << number(-pin.position.y()) << ")\n"
               << "        (rotation " << QString::number(pinRotation(pin.direction), 'f', 3) << ")\n"
               << "        (pinLength " << number(pin.length) << ")\n"
               << "        (pinDisplay (dispPinDes " << (pin.display.showDesignator ? "True" : "False")
               << ") (dispPinName " << (pin.display.showName ? "True" : "False") << "))\n"
               << "      )\n";
    }
    for (const IR::SymbolRectangleIR& rectangle : symbol.rectangles) {
        if (rectangle.partIndex != part)
            continue;
        if (!finitePoint(QPointF(rectangle.x0, rectangle.y0)) || !finitePoint(QPointF(rectangle.x1, rectangle.y1)) ||
            !std::isfinite(rectangle.strokeWidth) || rectangle.strokeWidth < 0.0 || rectangle.cornerRadiusX != 0.0 ||
            rectangle.cornerRadiusY != 0.0) {
            diagnostics.append(QStringLiteral("P-CAD: 符号 %1 的矩形包含无法无损写入的值").arg(symbol.name));
            return false;
        }
        if (!writePoly(stream,
                       {{rectangle.x0, rectangle.y0},
                        {rectangle.x1, rectangle.y0},
                        {rectangle.x1, rectangle.y1},
                        {rectangle.x0, rectangle.y1},
                        {rectangle.x0, rectangle.y0}}))
            return false;
    }
    for (const IR::SymbolPolylineIR& polyline : symbol.polylines) {
        if (polyline.partIndex == part && !writePoly(stream, polyline.points)) {
            diagnostics.append(QStringLiteral("P-CAD: 符号 %1 的折线点数不足").arg(symbol.name));
            return false;
        }
    }
    for (const IR::SymbolPolygonIR& polygon : symbol.polygons) {
        if (polygon.partIndex == part && (polygon.points.size() < 3 || !writePoly(stream, polygon.points))) {
            diagnostics.append(QStringLiteral("P-CAD: 符号 %1 的多边形点数不足").arg(symbol.name));
            return false;
        }
    }
    stream << "    )\n";
    return stream.status() == QTextStream::Ok;
}

/**
 * @brief 写入 P-CAD 组件定义及部件关联。
 * @param stream P-CAD 文本流。
 * @param symbol 符号 IR。
 * @param componentName 组件名称。
 * @param partNames 部件名称列表。
 * @param diagnostics 诊断信息输出列表。
 * @return 写入成功时返回 true。
 */
bool writeComponentDef(QTextStream& stream,
                       const IR::SymbolComponentIR& symbol,
                       const QString& componentName,
                       const QList<QString>& partNames,
                       QStringList& diagnostics) {
    QSet<QString> pinNumbers;  // 已出现的引脚编号集合。
    stream << "    (compDef " << quote(componentName) << "\n"
           << "      (originalName " << quote(symbol.name) << ")\n"
           << "      (compHeader (compType Normal) (numPins " << symbol.pins.size() << ") (numParts "
           << symbol.partCount << ") (composition " << (symbol.partCount > 1 ? "Heterogeneous" : "Homogeneous")
           << ") (refDesPrefix "
           << quote(symbol.designatorPrefix.isEmpty() ? QStringLiteral("U") : symbol.designatorPrefix)
           << ") (numType Alpha))\n";
    for (const IR::SymbolPinIR& pin : symbol.pins) {
        if (pinNumbers.contains(pin.designator.toLower())) {
            diagnostics.append(QStringLiteral("P-CAD: 符号 %1 存在重复引脚编号：%2").arg(symbol.name, pin.designator));
            return false;
        }
        pinNumbers.insert(pin.designator.toLower());
        stream << "      (compPin " << quote(pin.designator) << " " << quote(pin.name) << " (partNum "
               << (pin.commonToAllParts ? 0 : pin.partIndex + 1) << ") (symPinNum " << quote(pin.designator)
               << ") (gateEq 0) (pinEq 0) (pinType " << pinType(pin.electricalType) << "))\n";
    }
    for (int part = 0; part < partNames.size(); ++part)
        stream << "      (attachedSymbol (partNum " << part + 1 << ") (altType Normal) (symbolName "
               << quote(partNames.at(part)) << "))\n";
    const QString footprintName = safeName(symbol.footprintName);  // 清洗后的封装名称。
    if (!symbol.footprintName.isEmpty() && !footprintName.isEmpty())
        stream << "      (attachedPattern (patternNum 1) (patternName " << quote(footprintName) << "))\n";
    else
        diagnostics.append(QStringLiteral("P-CAD: 符号 %1 没有封装关联，已输出独立符号").arg(symbol.name));
    stream << "    )\n";
    return stream.status() == QTextStream::Ok;
}

}  // namespace

/**
 * @brief 返回独立的原理图库后缀。
 * @return P-CAD 原理图库扩展名。
 */
QString ExporterPcadSymbol::libraryFileExtension() const {
    return QStringLiteral("_PCAD_SCH.lia");
}

/**
 * @brief 导出单个 P-CAD 符号。
 * @param symbol 符号 IR。
 * @param filePath 输出文件路径。
 * @return 导出成功时返回 true。
 */
bool ExporterPcadSymbol::exportSymbol(const IR::SymbolComponentIR& symbol, const QString& filePath) {
    return exportSymbolLibrary({symbol}, symbol.name, filePath, false, false);
}

/**
 * @brief 完整重写 P-CAD 原理图库并写入 Component/Part 关联。
 * @param symbols 符号列表。
 * @param libName 库名称。
 * @param filePath 输出文件路径。
 * @param appendMode 是否追加。
 * @param updateMode 是否更新。
 * @param libraryDescription 库描述。
 * @return 导出成功时返回 true。
 */
bool ExporterPcadSymbol::exportSymbolLibrary(const QList<IR::SymbolComponentIR>& symbols,
                                             const QString& libName,
                                             const QString& filePath,
                                             bool appendMode,
                                             bool updateMode,
                                             const QString&) {
    m_diagnostics.clear();
    if (appendMode || updateMode) {
        m_diagnostics.append(QStringLiteral("P-CAD 原理图库当前只支持完整重写，不支持追加或更新模式"));
        return false;
    }
    if (symbols.isEmpty()) {
        m_diagnostics.append(QStringLiteral("P-CAD: 没有可导出的符号"));
        return false;
    }
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        m_diagnostics.append(QStringLiteral("P-CAD: 无法写入符号库：%1").arg(filePath));
        return false;
    }
    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    stream << "(ACCEL_ASCII " << quote(libName.isEmpty() ? QStringLiteral("EasyKiConverter") : libName) << "\n"
           << "  (asciiHeader\n    (asciiVersion 4 0)\n    (fileUnits MM)\n  )\n"
           << "  (library\n";
    QSet<QString> usedNames;  // 已使用的组件名称。
    for (const IR::SymbolComponentIR& symbol : symbols) {
        if (!validateSymbol(symbol, m_diagnostics))
            return false;
        const QString componentName = safeName(symbol.name);  // 当前清洗后的组件名称。
        if (usedNames.contains(componentName.toLower())) {
            m_diagnostics.append(QStringLiteral("P-CAD: 符号名称清洗后冲突：%1").arg(symbol.name));
            return false;
        }
        usedNames.insert(componentName.toLower());
        QList<QString> partNames;  // 当前组件部件名称列表。
        for (int part = 0; part < symbol.partCount; ++part) {
            const QString partName =  // 当前部件名称。
                symbol.partCount == 1 ? componentName : componentName + QStringLiteral("_P%1").arg(part + 1);
            partNames.append(partName);
            if (!writeSymbolDef(stream, symbol, partName, part, m_diagnostics))
                return false;
        }
        if (!writeComponentDef(stream, symbol, componentName, partNames, m_diagnostics))
            return false;
    }
    stream << "  )\n)\n";
    stream.flush();
    if (stream.status() != QTextStream::Ok) {
        m_diagnostics.append(QStringLiteral("P-CAD: 写入符号库时发生 I/O 错误"));
        return false;
    }
    m_diagnostics.append(
        QStringLiteral("P-CAD: 已输出 %1 个符号及其器件关联；封装和 3D 文件由独立阶段生成").arg(symbols.size()));
    return true;
}

/**
 * @brief 返回最近一次写入过程中收集的诊断信息。
 * @return 诊断信息列表。
 */
QStringList ExporterPcadSymbol::diagnostics() const {
    return m_diagnostics;
}

}  // namespace EasyKiConverter
