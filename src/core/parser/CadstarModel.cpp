#include "CadstarModel.h"

#include <QRegularExpression>
#include <QSet>

namespace EasyKiConverter::Parser {

namespace {

using Node = DelimitedSectionNode;

/** @brief 返回节点的第一个参数，缺失时返回空字符串。 */
QString firstArgument(const Node& node) {
    return node.arguments.value(0);
}

/** @brief 解析 Cadstar 的形状名称。 */
CadstarPadShape parsePadShape(const QString& value, ParseDiagnostics* diagnostics, int line) {
    const QString shape = value.trimmed().toUpper();
    if (shape == QStringLiteral("ROUND") || shape == QStringLiteral("CIRCLE"))
        return CadstarPadShape::Round;
    if (shape == QStringLiteral("SQUARE"))
        return CadstarPadShape::Square;
    if (shape == QStringLiteral("RECT") || shape == QStringLiteral("RECTANGLE"))
        return CadstarPadShape::Rectangle;
    if (shape == QStringLiteral("OBLONG"))
        return CadstarPadShape::Oblong;
    if (shape == QStringLiteral("OCTAGON"))
        return CadstarPadShape::Octagon;
    if (shape == QStringLiteral("POLYGON") || shape == QStringLiteral("CUSTOM"))
        return CadstarPadShape::Custom;
    if (diagnostics)
        diagnostics->add(ParseSeverity::Warning,
                         ParseScope::Field,
                         QStringLiteral("未知 Cadstar 焊盘形状：%1").arg(value),
                         value,
                         line);
    return CadstarPadShape::Unknown;
}

/** @brief 严格解析 Cadstar 数值字段。 */
double number(const QString& value, ParseDiagnostics* diagnostics, const QString& field, int line) {
    return StrictNumberParser::parseDouble(value, diagnostics, field, line);
}

/** @brief 从一个或两个参数中读取坐标，并报告非法坐标。 */
QPointF point(const QStringList& values, int start, ParseDiagnostics* diagnostics, const QString& field, int line) {
    if (start >= values.size()) {
        if (diagnostics)
            diagnostics->add(
                ParseSeverity::Error, ParseScope::Field, QStringLiteral("Cadstar 坐标字段缺失"), field, line);
        return {};
    }

    const QString token = values.at(start).trimmed();
    if (token.startsWith(QChar('(')) && token.endsWith(QChar(')'))) {
        const QStringList pair =
            token.mid(1, token.size() - 2).split(QRegularExpression(QStringLiteral("\\s+|,")), Qt::SkipEmptyParts);
        if (pair.size() == 2)
            return {number(pair.at(0), diagnostics, field + QStringLiteral(".X"), line),
                    number(pair.at(1), diagnostics, field + QStringLiteral(".Y"), line)};
    } else if (start + 1 < values.size()) {
        return {number(values.at(start), diagnostics, field + QStringLiteral(".X"), line),
                number(values.at(start + 1), diagnostics, field + QStringLiteral(".Y"), line)};
    }

    if (diagnostics)
        diagnostics->add(ParseSeverity::Error,
                         ParseScope::Field,
                         QStringLiteral("Cadstar 坐标格式错误：%1").arg(token),
                         field,
                         line);
    return {};
}

/** @brief 从节点参数中读取全部括号坐标。 */
QList<QPointF> points(const QStringList& values, ParseDiagnostics* diagnostics, const QString& field, int line) {
    QList<QPointF> result;
    for (int index = 0; index < values.size(); ++index) {
        if (!values.at(index).startsWith(QChar('(')))
            continue;
        result.append(point(values, index, diagnostics, field, line));
    }
    return result;
}

/** @brief 找到形如 WIDTH 0.2 的键值参数。 */
double namedNumber(const QStringList& values,
                   const QString& name,
                   ParseDiagnostics* diagnostics,
                   const QString& field,
                   int line) {
    for (int index = 0; index + 1 < values.size(); ++index) {
        if (values.at(index).compare(name, Qt::CaseInsensitive) == 0)
            return number(values.at(index + 1), diagnostics, field, line);
    }
    return 0.0;
}

/** @brief 展开 Cadstar 的连续编号范围。 */
QStringList expandNumbers(const QString& value, ParseDiagnostics* diagnostics, int line) {
    static const QRegularExpression range(QStringLiteral(R"(^\[(\d+)\s*:\s*(\d+)(?:\s*:\s*(\d+))?\]$)"));
    const QRegularExpressionMatch match = range.match(value.trimmed());
    if (!match.hasMatch())
        return value.split(QRegularExpression(QStringLiteral("\\s*,\\s*")), Qt::SkipEmptyParts);

    bool startOk = false;
    bool endOk = false;
    bool stepOk = true;
    const int start = match.captured(1).toInt(&startOk);
    const int end = match.captured(2).toInt(&endOk);
    const int step = match.captured(3).isEmpty() ? 1 : match.captured(3).toInt(&stepOk);
    if (!startOk || !endOk || !stepOk || step <= 0 || start > end) {
        if (diagnostics)
            diagnostics->add(ParseSeverity::Error,
                             ParseScope::Field,
                             QStringLiteral("Cadstar 引脚编号范围非法：%1").arg(value),
                             QStringLiteral("NUMBERS"),
                             line);
        return {};
    }

    QStringList result;
    for (int numberValue = start; numberValue <= end; numberValue += step)
        result.append(QString::number(numberValue));
    return result;
}

/** @brief 解析图形节点的类型和基础几何数据。 */
bool parseGraphic(const Node& node, CadstarGraphic& graphic, ParseDiagnostics* diagnostics) {
    const QString keyword = node.keyword.toUpper();
    if (keyword == QStringLiteral("LINE"))
        graphic.type = CadstarGraphicType::Line;
    else if (keyword == QStringLiteral("RECT"))
        graphic.type = CadstarGraphicType::Rectangle;
    else if (keyword == QStringLiteral("CIRCLE"))
        graphic.type = CadstarGraphicType::Circle;
    else if (keyword == QStringLiteral("ARC"))
        graphic.type = CadstarGraphicType::Arc;
    else if (keyword == QStringLiteral("POLYLINE"))
        graphic.type = CadstarGraphicType::Polyline;
    else if (keyword == QStringLiteral("POLYGON"))
        graphic.type = CadstarGraphicType::Polygon;
    else
        return false;

    graphic.points = points(node.arguments, diagnostics, keyword, node.line);
    if ((graphic.type == CadstarGraphicType::Line || graphic.type == CadstarGraphicType::Rectangle ||
         graphic.type == CadstarGraphicType::Arc) &&
        graphic.points.size() < 2) {
        if (diagnostics)
            diagnostics->add(ParseSeverity::Error,
                             ParseScope::Field,
                             QStringLiteral("Cadstar 图形点数量不足：%1").arg(keyword),
                             keyword,
                             node.line);
        return false;
    }
    if ((graphic.type == CadstarGraphicType::Polyline || graphic.type == CadstarGraphicType::Polygon) &&
        graphic.points.size() < (graphic.type == CadstarGraphicType::Polygon ? 3 : 2)) {
        if (diagnostics)
            diagnostics->add(ParseSeverity::Error,
                             ParseScope::Field,
                             QStringLiteral("Cadstar 多点图形点数量不足：%1").arg(keyword),
                             keyword,
                             node.line);
        return false;
    }
    if (graphic.type == CadstarGraphicType::Circle)
        graphic.radius =
            namedNumber(node.arguments, QStringLiteral("RADIUS"), diagnostics, QStringLiteral("RADIUS"), node.line);
    graphic.width =
        namedNumber(node.arguments, QStringLiteral("WIDTH"), diagnostics, QStringLiteral("WIDTH"), node.line);
    return true;
}

/** @brief 在重名定义上报告错误，避免关联结果静默覆盖。 */
template <typename T>
void reportDuplicate(const QList<T>& definitions,
                     const QString& name,
                     ParseDiagnostics* diagnostics,
                     const QString& type,
                     int line) {
    int count = 0;
    for (const T& definition : definitions) {
        if (definition.name == name)
            ++count;
    }
    if (count > 1 && diagnostics)
        diagnostics->add(ParseSeverity::Error,
                         ParseScope::File,
                         QStringLiteral("Cadstar 存在重复%1名称：%2").arg(type, name),
                         name,
                         line);
}

const QStringList kCadstarLeafKeywords = {
    QStringLiteral("UNITS"),       QStringLiteral("SHAPE"),
    QStringLiteral("DIAMETER"),    QStringLiteral("WIDTH"),
    QStringLiteral("HEIGHT"),      QStringLiteral("OFFSET"),
    QStringLiteral("HOLE"),        QStringLiteral("POINTS"),
    QStringLiteral("POLYGON"),     QStringLiteral("POLYLINE"),
    QStringLiteral("LINE"),        QStringLiteral("RECT"),
    QStringLiteral("CIRCLE"),      QStringLiteral("ARC"),
    QStringLiteral("POSITION"),    QStringLiteral("ROTATION"),
    QStringLiteral("START"),       QStringLiteral("END"),
    QStringLiteral("INVERTED"),    QStringLiteral("PINTYPE"),
    QStringLiteral("NUMBERS"),     QStringLiteral("NUMBER_VISIBLE"),
    QStringLiteral("LABEL"),       QStringLiteral("PROPERTY"),
    QStringLiteral("DESCRIPTION"), QStringLiteral("VERSION"),
    QStringLiteral("PAD"),         QStringLiteral("PIN"),
    QStringLiteral("PART"),        QStringLiteral("COMPONENT"),
    QStringLiteral("PACKAGE"),
};

}  // namespace

/** @brief 判断 Cadstar 库模型是否包含可转换定义。 */
bool CadstarLibrary::isRecognized() const {
    return !pads.isEmpty() || !packages.isEmpty() || !components.isEmpty() || !parts.isEmpty();
}

/** @brief 解析 Cadstar 分段树并构建格式专用模型。 */
CadstarLibrary CadstarParser::parse(const QString& content, const QString& filePath) {
    CadstarLibrary library;
    library.diagnostics.setFilePath(filePath);
    const QList<DelimitedSectionNode> sections =
        DelimitedSectionParser::parse(content, kCadstarLeafKeywords, &library.diagnostics);

    for (const Node& root : sections) {
        if (root.keyword == QStringLiteral("UNITS")) {
            library.unit = UnitConverter::parseUnit(firstArgument(root), &library.diagnostics);
            continue;
        }
        if (root.keyword == QStringLiteral("PAD")) {
            CadstarPad pad;
            pad.name = firstArgument(root);
            for (const Node& child : root.children) {
                if (child.keyword == QStringLiteral("SHAPE"))
                    pad.shape = parsePadShape(firstArgument(child), &library.diagnostics, child.line);
                else if (child.keyword == QStringLiteral("DIAMETER"))
                    pad.diameter =
                        number(firstArgument(child), &library.diagnostics, QStringLiteral("PAD.DIAMETER"), child.line);
                else if (child.keyword == QStringLiteral("WIDTH"))
                    pad.width =
                        number(firstArgument(child), &library.diagnostics, QStringLiteral("PAD.WIDTH"), child.line);
                else if (child.keyword == QStringLiteral("HEIGHT"))
                    pad.height =
                        number(firstArgument(child), &library.diagnostics, QStringLiteral("PAD.HEIGHT"), child.line);
                // 读取焊盘中心偏移，保持来源坐标语义。
                else if (child.keyword == QStringLiteral("OFFSET")) {
                    pad.offsetX = number(
                        child.arguments.value(0), &library.diagnostics, QStringLiteral("PAD.OFFSET.X"), child.line);
                    pad.offsetY = number(
                        child.arguments.value(1), &library.diagnostics, QStringLiteral("PAD.OFFSET.Y"), child.line);
                } else if (child.keyword == QStringLiteral("POLYGON") || child.keyword == QStringLiteral("POINTS"))
                    pad.polygon =
                        points(child.arguments, &library.diagnostics, QStringLiteral("PAD.POLYGON"), child.line);
                // 读取圆孔或槽孔参数，非法数值由严格解析器诊断。
                else if (child.keyword == QStringLiteral("HOLE")) {
                    const QString shape = child.arguments.value(0).toUpper();
                    if (shape == QStringLiteral("SLOT")) {
                        pad.holeWidth = number(child.arguments.value(1),
                                               &library.diagnostics,
                                               QStringLiteral("PAD.HOLE.WIDTH"),
                                               child.line);
                        pad.holeHeight = number(child.arguments.value(2),
                                                &library.diagnostics,
                                                QStringLiteral("PAD.HOLE.HEIGHT"),
                                                child.line);
                    } else {
                        pad.holeDiameter = number(child.arguments.value(1),
                                                  &library.diagnostics,
                                                  QStringLiteral("PAD.HOLE.DIAMETER"),
                                                  child.line);
                    }
                }
            }
            if (pad.name.isEmpty())
                library.diagnostics.add(
                    ParseSeverity::Error, ParseScope::Field, QStringLiteral("Cadstar 焊盘缺少名称"), {}, root.line);
            library.pads.append(pad);
            reportDuplicate(library.pads, pad.name, &library.diagnostics, QStringLiteral("焊盘"), root.line);
            continue;
        }
        if (root.keyword == QStringLiteral("PACKAGE")) {
            CadstarPackage packageModel;
            packageModel.name = firstArgument(root);
            for (const Node& child : root.children) {
                if (child.keyword == QStringLiteral("DESCRIPTION"))
                    packageModel.description = child.arguments.join(QStringLiteral(" "));
                else if (child.keyword == QStringLiteral("PROPERTY"))
                    packageModel.properties.insert(child.arguments.value(0),
                                                   child.arguments.mid(1).join(QStringLiteral(" ")));
                // 解析封装引脚及其焊盘引用。
                else if (child.keyword == QStringLiteral("PIN")) {
                    CadstarPackagePin pin;
                    pin.number = firstArgument(child);
                    for (const Node& pinChild : child.children) {
                        if (pinChild.keyword == QStringLiteral("POSITION"))
                            pin.position = point(pinChild.arguments,
                                                 0,
                                                 &library.diagnostics,
                                                 QStringLiteral("PACKAGE.PIN.POSITION"),
                                                 pinChild.line);
                        else if (pinChild.keyword == QStringLiteral("ROTATION"))
                            pin.rotation = number(firstArgument(pinChild),
                                                  &library.diagnostics,
                                                  QStringLiteral("PACKAGE.PIN.ROTATION"),
                                                  pinChild.line);
                        else if (pinChild.keyword == QStringLiteral("PAD"))
                            pin.padName = firstArgument(pinChild);
                    }
                    if (pin.number.isEmpty())
                        library.diagnostics.add(ParseSeverity::Error,
                                                ParseScope::Footprint,
                                                QStringLiteral("Cadstar 封装引脚缺少编号"),
                                                {},
                                                child.line);
                    packageModel.pins.append(pin);
                } else {
                    CadstarGraphic graphic;
                    if (parseGraphic(child, graphic, &library.diagnostics))
                        packageModel.graphics.append(graphic);
                }
            }
            if (packageModel.name.isEmpty())
                library.diagnostics.add(
                    ParseSeverity::Error, ParseScope::Footprint, QStringLiteral("Cadstar 封装缺少名称"), {}, root.line);
            library.packages.append(packageModel);
            reportDuplicate(
                library.packages, packageModel.name, &library.diagnostics, QStringLiteral("封装"), root.line);
            continue;
        }
        if (root.keyword == QStringLiteral("COMPONENT")) {
            CadstarComponent component;
            component.name = firstArgument(root);
            for (const Node& child : root.children) {
                if (child.keyword == QStringLiteral("VERSION"))
                    component.version = static_cast<int>(number(
                        firstArgument(child), &library.diagnostics, QStringLiteral("COMPONENT.VERSION"), child.line));
                else if (child.keyword == QStringLiteral("PROPERTY"))
                    component.properties.insert(child.arguments.value(0),
                                                child.arguments.mid(1).join(QStringLiteral(" ")));
                // 解析符号引脚、编号范围和显示属性。
                else if (child.keyword == QStringLiteral("PIN")) {
                    CadstarComponentPin pin;
                    pin.id = firstArgument(child);
                    for (const Node& pinChild : child.children) {
                        if (pinChild.keyword == QStringLiteral("START"))
                            pin.start = point(pinChild.arguments,
                                              0,
                                              &library.diagnostics,
                                              QStringLiteral("COMPONENT.PIN.START"),
                                              pinChild.line);
                        else if (pinChild.keyword == QStringLiteral("END"))
                            pin.end = point(pinChild.arguments,
                                            0,
                                            &library.diagnostics,
                                            QStringLiteral("COMPONENT.PIN.END"),
                                            pinChild.line);
                        else if (pinChild.keyword == QStringLiteral("ROTATION"))
                            pin.rotation = number(firstArgument(pinChild),
                                                  &library.diagnostics,
                                                  QStringLiteral("COMPONENT.PIN.ROTATION"),
                                                  pinChild.line);
                        else if (pinChild.keyword == QStringLiteral("INVERTED"))
                            pin.inverted = true;
                        else if (pinChild.keyword == QStringLiteral("PINTYPE"))
                            pin.pinType = pinChild.arguments.join(QStringLiteral(" "));
                        else if (pinChild.keyword == QStringLiteral("NUMBERS"))
                            pin.numbers = expandNumbers(
                                pinChild.arguments.join(QStringLiteral(" ")), &library.diagnostics, pinChild.line);
                        // 保留引脚编号的显示状态。
                        else if (pinChild.keyword == QStringLiteral("NUMBER_VISIBLE"))
                            pin.numberVisible = number(firstArgument(pinChild),
                                                       &library.diagnostics,
                                                       QStringLiteral("COMPONENT.PIN.NUMBER_VISIBLE"),
                                                       pinChild.line) != 0.0;
                        // 读取引脚标签及其位置和可见性属性。
                        else if (pinChild.keyword == QStringLiteral("LABEL")) {
                            pin.label = firstArgument(pinChild);
                            pin.labelPosition = point(pinChild.arguments,
                                                      1,
                                                      &library.diagnostics,
                                                      QStringLiteral("COMPONENT.PIN.LABEL"),
                                                      pinChild.line);
                            if (pinChild.arguments.size() > 2)
                                pin.labelVisible = number(pinChild.arguments.at(2),
                                                          &library.diagnostics,
                                                          QStringLiteral("COMPONENT.PIN.LABEL_VISIBLE"),
                                                          pinChild.line) != 0.0;
                        }
                    }
                    component.pins.append(pin);
                } else {
                    CadstarGraphic graphic;
                    if (parseGraphic(child, graphic, &library.diagnostics))
                        component.graphics.append(graphic);
                }
            }
            if (component.name.isEmpty())
                library.diagnostics.add(
                    ParseSeverity::Error, ParseScope::Symbol, QStringLiteral("Cadstar 符号缺少名称"), {}, root.line);
            library.components.append(component);
            reportDuplicate(
                library.components, component.name, &library.diagnostics, QStringLiteral("符号"), root.line);
            continue;
        }
        if (root.keyword == QStringLiteral("PART")) {
            CadstarPart part;
            part.name = firstArgument(root);
            for (const Node& child : root.children) {
                if (child.keyword == QStringLiteral("COMPONENT"))
                    part.componentName = firstArgument(child);
                else if (child.keyword == QStringLiteral("PACKAGE"))
                    part.packageName = firstArgument(child);
                else if (child.keyword == QStringLiteral("DESCRIPTION"))
                    part.description = child.arguments.join(QStringLiteral(" "));
                else if (child.keyword == QStringLiteral("PROPERTY"))
                    part.properties.insert(child.arguments.value(0), child.arguments.mid(1).join(QStringLiteral(" ")));
            }
            if (part.name.isEmpty() || part.componentName.isEmpty() || part.packageName.isEmpty())
                library.diagnostics.add(ParseSeverity::Error,
                                        ParseScope::Component,
                                        QStringLiteral("Cadstar Part 缺少名称或关联"),
                                        part.name,
                                        root.line);
            library.parts.append(part);
        }
    }

    if (!library.isRecognized())
        library.diagnostics.add(ParseSeverity::Error, ParseScope::File, QStringLiteral("未识别的 Cadstar ASCII 库"));
    return library;
}

}  // namespace EasyKiConverter::Parser
