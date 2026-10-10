#include "ExporterCadstarLibrary.h"

#include "core/ir/ComponentIR.h"

#include <QFile>
#include <QRegularExpression>
#include <QSet>
#include <QTextStream>

namespace EasyKiConverter {

namespace {

/**
 * @brief 转义 CADSTAR ASCII 的带引号字段。
 * @param value 原始字段值。
 * @return 转义后的带引号字段。
 */
QString quote(const QString& value) {
    QString escaped = value;  // 需要进行 CADSTAR 转义的字段副本。
    escaped.replace('\\', QStringLiteral("\\\\"));
    escaped.replace('"', QStringLiteral("\\\""));
    return QStringLiteral("\"%1\"").arg(escaped);
}

/**
 * @brief 使用固定精度输出毫米坐标。
 * @param value 毫米数值。
 * @return 格式化后的数值文本。
 */
QString number(double value) {
    return QString::number(value, 'g', 12);
}

/**
 * @brief 输出 CADSTAR 点。
 * @param value 点坐标。
 * @return CADSTAR 点文本。
 */
QString point(const QPointF& value) {
    return QStringLiteral("(%1 %2)").arg(number(value.x()), number(value.y()));
}

/**
 * @brief 为同一封装中的焊盘生成稳定且唯一的定义名称。
 * @param footprint 所属封装。
 * @param pad 焊盘数据。
 * @param index 焊盘在封装中的索引。
 * @return 稳定的焊盘定义名称。
 */
QString padName(const IR::FootprintComponentIR& footprint, const IR::FootprintPadIR& pad, int index) {
    QString base = footprint.name.trimmed();  // 清理后的封装基础名称。
    base.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9_]+")), QStringLiteral("_"));
    if (base.isEmpty())
        base = QStringLiteral("PACKAGE");
    QString pin = pad.number.trimmed();  // 清理后的焊盘编号。
    pin.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9_]+")), QStringLiteral("_"));
    if (pin.isEmpty())
        pin = QString::number(index + 1);
    return QStringLiteral("%1_PAD_%2").arg(base, pin);
}

/**
 * @brief 判断图元是否可以表达为 CADSTAR ASCII 的基础图形。
 * @param stream 输出流。
 * @param footprint 封装数据。
 * @param diagnostics 诊断列表。
 * @return 图元全部可表达时返回 true。
 */
bool writeGraphic(QTextStream& stream, const IR::FootprintComponentIR& footprint, QStringList& diagnostics) {
    for (const auto& rectangle : footprint.rectangles) {
        const QRectF bounds = rectangle.bounds.normalized();  // 归一化后的矩形边界。
        stream << "RECT " << point(bounds.topLeft()) << ' ' << point(bounds.bottomRight()) << " WIDTH "
               << number(rectangle.strokeWidth) << '\n';
    }
    for (const auto& circle : footprint.circles) {
        stream << "CIRCLE " << point(circle.center) << " RADIUS " << number(circle.radius) << " WIDTH "
               << number(circle.strokeWidth) << '\n';
    }
    for (const auto& track : footprint.tracks) {
        if (track.points.size() < 2) {
            diagnostics.append(QStringLiteral("CADSTAR: 封装折线至少需要两个点"));
            return false;
        }
        stream << "POLYLINE";
        for (const QPointF& item : track.points)
            stream << ' ' << point(item);
        stream << " WIDTH " << number(track.width) << '\n';
    }
    for (const auto& region : footprint.regions) {
        if (region.vertices.size() < 3) {
            diagnostics.append(QStringLiteral("CADSTAR: 封装多边形至少需要三个点"));
            return false;
        }
        stream << "POLYGON";
        for (const QPointF& item : region.vertices)
            stream << ' ' << point(item);
        stream << '\n';
    }
    for (const auto& arc : footprint.arcs) {
        diagnostics.append(QStringLiteral("CADSTAR: FootprintArc 暂不支持直接写入，已拒绝导出"));
        Q_UNUSED(arc);
        return false;
    }
    for (const auto& text : footprint.texts) {
        diagnostics.append(QStringLiteral("CADSTAR: 封装文本暂不支持独立字体属性，已拒绝导出：%1").arg(text.text));
        return false;
    }
    return true;
}

/**
 * @brief 将统一焊盘映射为 CADSTAR Pad 定义。
 * @param stream 输出流。
 * @param name 焊盘定义名称。
 * @param pad 焊盘数据。
 * @param diagnostics 诊断列表。
 * @return 写入成功返回 true。
 */
bool writePad(QTextStream& stream, const QString& name, const IR::FootprintPadIR& pad, QStringList& diagnostics) {
    stream << "PAD " << quote(name) << '\n';
    // 按统一 IR 焊盘形状选择 CADSTAR 可以准确表达的定义。
    switch (pad.shape) {
        case IR::PadShape::Ellipse:
            if (qFuzzyCompare(pad.size.width(), pad.size.height())) {
                stream << "SHAPE ROUND\nDIAMETER " << number(pad.size.width()) << '\n';
            } else {
                stream << "SHAPE OBLONG\nWIDTH " << number(pad.size.width()) << "\nHEIGHT " << number(pad.size.height())
                       << '\n';
            }
            break;
        case IR::PadShape::Rect:
            stream << "SHAPE " << (qFuzzyCompare(pad.size.width(), pad.size.height()) ? "SQUARE" : "RECTANGLE") << '\n';
            stream << "WIDTH " << number(pad.size.width()) << "\nHEIGHT " << number(pad.size.height()) << '\n';
            break;
        case IR::PadShape::Oval:
            stream << "SHAPE OBLONG\nWIDTH " << number(pad.size.width()) << "\nHEIGHT " << number(pad.size.height())
                   << '\n';
            break;
        case IR::PadShape::Polygon:
            if (pad.customShapePoints.size() < 3) {
                diagnostics.append(QStringLiteral("CADSTAR: 多边形焊盘缺少三个以上顶点：%1").arg(name));
                return false;
            }
            stream << "SHAPE CUSTOM\nPOLYGON";
            for (const QPointF& item : pad.customShapePoints)
                stream << ' ' << point(item);
            stream << '\n';
            break;
        case IR::PadShape::RoundRect:
        case IR::PadShape::Trapezoid:
            diagnostics.append(QStringLiteral("CADSTAR: 不支持的焊盘形状，禁止静默降级：%1").arg(name));
            return false;
    }
    if (pad.isThroughHole()) {
        if (pad.holeLength > 0.0)
            stream << "HOLE SLOT " << number(pad.holeSize) << ' ' << number(pad.holeLength) << '\n';
        else if (pad.holeSize > 0.0)
            stream << "HOLE ROUND " << number(pad.holeSize) << '\n';
        else {
            diagnostics.append(QStringLiteral("CADSTAR: 通孔焊盘缺少孔径：%1").arg(name));
            return false;
        }
    }
    stream << "ENDPAD\n";
    return true;
}

/**
 * @brief 在 Package 定义之前输出该库所引用的全局 Pad 定义。
 * @param stream 输出流。
 * @param components 组件列表。
 * @param diagnostics 诊断列表。
 * @param padNames 已写入的焊盘名称集合。
 * @return 写入成功返回 true。
 */
bool writePadDefinitions(QTextStream& stream,
                         const QList<IR::ComponentIR>& components,
                         QStringList& diagnostics,
                         QSet<QString>& padNames) {
    for (const auto& component : components) {
        for (int index = 0; index < component.footprint.pads.size(); ++index) {
            const auto& pad = component.footprint.pads.at(index);  // 当前组件焊盘。
            const QString name = padName(component.footprint, pad, index);  // 稳定焊盘定义名称。
            if (padNames.contains(name))
                continue;
            if (!writePad(stream, name, pad, diagnostics))
                return false;
            padNames.insert(name);
        }
    }
    return true;
}

/**
 * @brief 将统一符号写为 CADSTAR Component 定义。
 * @param stream 输出流。
 * @param symbol 符号数据。
 * @param diagnostics 诊断列表。
 * @return 写入成功返回 true。
 */
bool writeComponent(QTextStream& stream, const IR::SymbolComponentIR& symbol, QStringList& diagnostics) {
    if (symbol.name.isEmpty()) {
        diagnostics.append(QStringLiteral("CADSTAR: 符号名称为空"));
        return false;
    }
    stream << "COMPONENT " << quote(symbol.name) << "\nVERSION 54\n";
    if (!symbol.designatorPrefix.isEmpty())
        stream << "PROPERTY REFDES " << quote(symbol.designatorPrefix) << '\n';
    for (const auto& pin : symbol.pins) {
        if (pin.designator.isEmpty()) {
            diagnostics.append(QStringLiteral("CADSTAR: 符号 %1 存在空引脚编号").arg(symbol.name));
            return false;
        }
        stream << "PIN " << quote(pin.designator) << '\n';
        const QPointF end = pin.position;  // 引脚终点坐标。
        QPointF start = end;  // 根据方向反推的引脚起点。
        // 根据统一 IR 的引脚方向反推 CADSTAR 引脚起点。
        switch (pin.direction) {
            case IR::PinDirection::Left:
                start.rx() += pin.length;
                break;
            case IR::PinDirection::Right:
                start.rx() -= pin.length;
                break;
            case IR::PinDirection::Up:
                start.ry() += pin.length;
                break;
            case IR::PinDirection::Down:
                start.ry() -= pin.length;
                break;
            default:
                break;
        }
        stream << "START " << point(start) << "\nEND " << point(end) << '\n';
        stream << "PINTYPE " << (pin.electricalType == IR::PinElectricalType::Input ? "INPUT" : "PASSIVE") << '\n';
        stream << "NUMBERS [" << quote(pin.designator) << ":" << quote(pin.designator) << "]\n";
        if (!pin.name.isEmpty())
            stream << "LABEL " << quote(pin.name) << ' ' << point(pin.hasNamePosition ? pin.namePosition : end)
                   << " 1\n";
        stream << "ENDPIN\n";
    }
    for (const auto& rectangle : symbol.rectangles)
        stream << "RECT " << point(QPointF(rectangle.x0, rectangle.y0)) << ' '
               << point(QPointF(rectangle.x1, rectangle.y1)) << '\n';
    for (const auto& circle : symbol.circles)
        stream << "CIRCLE " << point(circle.center) << " RADIUS " << number(circle.radius) << '\n';
    for (const auto& polygon : symbol.polygons) {
        if (polygon.points.size() < 3) {
            diagnostics.append(QStringLiteral("CADSTAR: 符号多边形至少需要三个点：%1").arg(symbol.name));
            return false;
        }
        stream << "POLYGON";
        for (const QPointF& item : polygon.points)
            stream << ' ' << point(item);
        stream << '\n';
    }
    if (!symbol.arcs.isEmpty() || !symbol.ellipses.isEmpty() || !symbol.paths.isEmpty() || !symbol.beziers.isEmpty()) {
        diagnostics.append(
            QStringLiteral("CADSTAR: 符号包含当前 ASCII writer 尚未表达的曲线图元：%1").arg(symbol.name));
        return false;
    }
    stream << "ENDCOMPONENT\n";
    return true;
}

/**
 * @brief 将一个完整组件写为 CADSTAR Package 和 Part。
 * @param stream 输出流。
 * @param component 完整组件数据。
 * @param diagnostics 诊断列表。
 * @param packageNames 已写入的封装名称集合。
 * @param includePart 是否同时写入 Part 定义。
 * @return 写入成功返回 true。
 */
bool writePackageAndPart(QTextStream& stream,
                         const IR::ComponentIR& component,
                         QStringList& diagnostics,
                         QSet<QString>& packageNames,
                         bool includePart) {
    if (component.name.isEmpty() || component.footprint.name.isEmpty()) {
        diagnostics.append(QStringLiteral("CADSTAR: 组件缺少名称或封装名称"));
        return false;
    }
    QString packageName = component.footprint.name;  // 当前组件封装名称。
    if (!packageNames.contains(packageName)) {
        stream << "PACKAGE " << quote(packageName) << '\n';
        if (!component.footprint.description.isEmpty())
            stream << "DESCRIPTION " << quote(component.footprint.description) << '\n';
        for (int index = 0; index < component.footprint.pads.size(); ++index) {
            const auto& pad = component.footprint.pads.at(index);  // 当前组件焊盘。
            const QString name = padName(component.footprint, pad, index);  // 稳定焊盘定义名称。
            stream << "PIN " << quote(pad.number) << "\nPOSITION " << point(pad.position) << "\nPAD " << quote(name)
                   << "\nENDPIN\n";
        }
        if (!writeGraphic(stream, component.footprint, diagnostics))
            return false;
        stream << "ENDPACKAGE\n";
        packageNames.insert(packageName);
    }
    if (includePart) {
        QSet<QString> padNumbers;  // 当前封装中可用的焊盘编号集合。
        for (const IR::FootprintPadIR& pad : component.footprint.pads)
            padNumbers.insert(pad.number);
        for (const IR::SymbolPinIR& pin : component.symbol.pins) {
            if (pin.designator.trimmed().isEmpty() || !padNumbers.contains(pin.designator)) {
                diagnostics.append(QStringLiteral("CADSTAR: 组件 %1 的符号引脚 %2 找不到对应封装焊盘")
                                       .arg(component.name, pin.designator));
                return false;
            }
        }
        const QString symbolName =
            component.symbol.name.isEmpty() ? component.name : component.symbol.name;  // 完整组件符号名称。
        stream << "PART " << quote(component.name) << "\nCOMPONENT " << quote(symbolName) << "\nPACKAGE "
               << quote(packageName) << '\n';
        if (!component.description.isEmpty())
            stream << "DESCRIPTION " << quote(component.description) << '\n';
        stream << "ENDPART\n";
    }
    return true;
}

}  // namespace

/**
 * @brief 返回 CADSTAR ASCII 库文件扩展名。
 * @return 库文件扩展名。
 */
QString ExporterCadstarLibrary::libraryFileExtension() const {
    return QStringLiteral(".lib");
}

/**
 * @brief 声明 CADSTAR 导出为单个文件而非目录。
 * @return 始终返回 false。
 */
bool ExporterCadstarLibrary::isDirectoryOutput() const {
    return false;
}

/**
 * @brief 导出单个 CADSTAR 封装。
 * @param footprint 封装数据。
 * @param filePath 输出文件路径。
 * @param libraryDescription 库描述，当前接口不使用。
 * @return 导出成功返回 true。
 */
bool ExporterCadstarLibrary::exportFootprint(const IR::FootprintComponentIR& footprint,
                                             const QString& filePath,
                                             const QString&) {
    return exportFootprintLibrary({footprint}, footprint.name, filePath);
}

/**
 * @brief 导出 CADSTAR 封装库。
 * @param footprints 封装列表。
 * @param libName 库名称。
 * @param filePath 输出文件路径。
 * @param appendMode 是否追加，当前格式不使用。
 * @param updateMode 是否更新，当前格式不使用。
 * @param libraryDescription 库描述。
 * @param libraryVersion 库版本，当前格式不使用。
 * @param export3D 是否导出三维模型，当前格式不使用。
 * @param model3DBasePath 三维模型基础路径，当前格式不使用。
 * @return 导出成功返回 true。
 */
bool ExporterCadstarLibrary::exportFootprintLibrary(const QList<IR::FootprintComponentIR>& footprints,
                                                    const QString& libName,
                                                    const QString& filePath,
                                                    bool,
                                                    bool,
                                                    const QString& libraryDescription,
                                                    const QString&,
                                                    bool,
                                                    const QString&) {
    m_diagnostics.clear();
    Q_UNUSED(libName);
    if (footprints.isEmpty()) {
        m_diagnostics.append(QStringLiteral("CADSTAR: 没有可导出的封装"));
        return false;
    }
    QFile file(filePath);  // 封装库输出文件。
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        m_diagnostics.append(QStringLiteral("CADSTAR: 无法写入 ASCII 库：%1").arg(filePath));
        return false;
    }
    QTextStream stream(&file);  // CADSTAR ASCII 输出流。
    stream.setEncoding(QStringConverter::Utf8);
    stream << "UNITS MM\n";
    if (!libraryDescription.isEmpty())
        stream << "PROPERTY DESCRIPTION " << quote(libraryDescription) << '\n';
    QSet<QString> padNames;  // 已写入的焊盘定义名称。
    QSet<QString> packageNames;  // 已写入的封装名称。
    QList<IR::ComponentIR> components;  // 由封装转换得到的组件列表。
    for (const auto& footprint : footprints) {
        IR::ComponentIR component;  // 当前封装对应的临时组件。
        component.name = footprint.name;
        component.footprint = footprint;
        components.append(component);
    }
    if (!writePadDefinitions(stream, components, m_diagnostics, padNames))
        return false;
    for (const auto& footprint : footprints) {
        IR::ComponentIR component;  // 当前封装对应的临时组件。
        component.name = footprint.name;
        component.footprint = footprint;
        if (!writePackageAndPart(stream, component, m_diagnostics, packageNames, false))
            return false;
    }
    return true;
}

/**
 * @brief 导出 CADSTAR 符号库。
 * @param symbols 符号列表。
 * @param libName 库名称。
 * @param filePath 输出文件路径。
 * @return 导出成功返回 true。
 */
bool ExporterCadstarLibrary::exportSymbolLibrary(const QList<IR::SymbolComponentIR>& symbols,
                                                 const QString& libName,
                                                 const QString& filePath) {
    m_diagnostics.clear();
    if (symbols.isEmpty()) {
        m_diagnostics.append(QStringLiteral("CADSTAR: 没有可导出的符号"));
        return false;
    }

    QFile file(filePath);  // 符号库输出文件。
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        m_diagnostics.append(QStringLiteral("CADSTAR: 无法写入符号库：%1").arg(filePath));
        return false;
    }
    QTextStream stream(&file);  // CADSTAR ASCII 输出流。
    stream.setEncoding(QStringConverter::Utf8);
    stream << "UNITS MM\n";
    if (!libName.isEmpty())
        stream << "PROPERTY DESCRIPTION " << quote(libName) << '\n';

    QSet<QString> symbolNames;  // 已写入的符号名称集合。
    for (const IR::SymbolComponentIR& symbol : symbols) {
        if (symbol.name.isEmpty() || symbolNames.contains(symbol.name)) {
            m_diagnostics.append(QStringLiteral("CADSTAR: 符号名称为空或重复：%1").arg(symbol.name));
            return false;
        }
        if (!writeComponent(stream, symbol, m_diagnostics))
            return false;
        symbolNames.insert(symbol.name);
    }
    return true;
}

/**
 * @brief 通过通用符号接口复用 CADSTAR 的符号库写入器。
 * @param symbols 符号列表。
 * @param libName 库名称。
 * @param filePath 输出文件路径。
 * @param appendMode 是否追加。
 * @param updateMode 是否更新。
 * @param libraryDescription 库描述。
 * @return 导出成功返回 true。
 */
bool ExporterCadstarLibrary::exportSymbolLibrary(const QList<IR::SymbolComponentIR>& symbols,
                                                 const QString& libName,
                                                 const QString& filePath,
                                                 bool appendMode,
                                                 bool updateMode,
                                                 const QString& libraryDescription) {
    Q_UNUSED(libraryDescription)
    if (appendMode || updateMode) {
        m_diagnostics = {QStringLiteral("CADSTAR ASCII 符号库暂不支持追加或更新模式")};
        return false;
    }
    return exportSymbolLibrary(symbols, libName, filePath);
}

/**
 * @brief 通过通用符号接口导出单个 CADSTAR 符号。
 * @param symbol 符号数据。
 * @param filePath 输出文件路径。
 * @return 导出成功返回 true。
 */
bool ExporterCadstarLibrary::exportSymbol(const IR::SymbolComponentIR& symbol, const QString& filePath) {
    return exportSymbolLibrary({symbol}, symbol.name, filePath, false, false);
}

/**
 * @brief 导出 CADSTAR 完整组件库。
 * @param components 完整组件列表。
 * @param libName 库名称。
 * @param filePath 输出文件路径。
 * @param exportModel3D 是否导出三维模型，当前格式不使用。
 * @param model3DBasePath 三维模型基础路径，当前格式不使用。
 * @return 导出成功返回 true。
 */
bool ExporterCadstarLibrary::exportComponentLibrary(const QList<IR::ComponentIR>& components,
                                                    const QString& libName,
                                                    const QString& filePath,
                                                    bool exportModel3D,
                                                    const QString&) {
    m_diagnostics.clear();
    if (components.isEmpty()) {
        m_diagnostics.append(QStringLiteral("CADSTAR: 没有可导出的完整组件"));
        return false;
    }
    QFile file(filePath);  // 完整组件库输出文件。
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        m_diagnostics.append(QStringLiteral("CADSTAR: 无法写入完整 ASCII 库：%1").arg(filePath));
        return false;
    }
    QTextStream stream(&file);  // CADSTAR ASCII 输出流。
    stream.setEncoding(QStringConverter::Utf8);
    stream << "UNITS MM\nPROPERTY DESCRIPTION " << quote(libName) << '\n';
    QSet<QString> symbols;  // 已写入的符号名称集合。
    QSet<QString> padNames;  // 已写入的焊盘定义名称。
    QSet<QString> packageNames;  // 已写入的封装名称。
    if (!writePadDefinitions(stream, components, m_diagnostics, padNames))
        return false;
    for (const auto& component : components) {
        const QString symbolName =
            component.symbol.name.isEmpty() ? component.name : component.symbol.name;  // 当前组件符号名称。
        if (symbols.contains(symbolName)) {
            m_diagnostics.append(QStringLiteral("CADSTAR: 符号名称冲突：%1").arg(symbolName));
            return false;
        }
        symbols.insert(symbolName);
        if (!writeComponent(stream, component.symbol, m_diagnostics) ||
            !writePackageAndPart(stream, component, m_diagnostics, packageNames, true))
            return false;
        if (exportModel3D && component.hasModel3D())
            m_diagnostics.append(QStringLiteral("CADSTAR: 组件 %1 的 3D 模型由独立阶段输出，ASCII Part 不写入模型关联")
                                     .arg(component.name));
    }
    return true;
}

/**
 * @brief 返回最近一次导出的诊断信息。
 * @return 诊断信息列表。
 */
QStringList ExporterCadstarLibrary::diagnostics() const {
    return m_diagnostics;
}

}  // namespace EasyKiConverter
