#include "PcadModel.h"

#include <QRegularExpression>
#include <QSet>

namespace EasyKiConverter::Parser {
namespace {

// 为 P-CAD 读取器统一使用通用 S-expression 节点别名。
using Node = SExpressionNode;

/** @brief 返回节点名称的大写形式，便于匹配不区分大小写的 P-CAD 关键字。 */
QString nameOf(const Node& node) {
    return node.atom.toUpper();
}

/** @brief 返回节点中指定名称的直接子列表。 */
QList<Node> lists(const Node& node, const QString& name) {
    QList<Node> result;
    for (const Node& child : node.children) {
        if (child.isList() && nameOf(child) == name.toUpper())
            result.append(child);
    }
    return result;
}

/** @brief 返回节点中指定名称的第一个直接子列表。 */
Node firstList(const Node& node, const QString& name) {
    const QList<Node> matches = lists(node, name);
    return matches.isEmpty() ? Node() : matches.first();
}

/** @brief 提取节点的直接标量参数，保留源文件顺序。 */
QStringList scalarValues(const Node& node) {
    QStringList result;
    for (const Node& child : node.children) {
        if (!child.isList())
            result.append(child.atom);
    }
    return result;
}

/** @brief 返回节点的第一个标量参数。 */
QString firstScalar(const Node& node) {
    const QStringList values = scalarValues(node);
    return values.isEmpty() ? QString() : values.first();
}

/** @brief 严格解析 P-CAD 长度并统一转换为毫米。 */
double number(const QString& text,
              ParseDiagnostics* diagnostics,
              const QString& field,
              int line,
              LengthUnit defaultUnit = LengthUnit::Mil) {
    QString value = text.trimmed();
    LengthUnit unit = defaultUnit;
    const QString lower = value.toLower();
    if (lower.endsWith(QStringLiteral("mm"))) {
        unit = LengthUnit::Millimeter;
        value.chop(2);
    } else if (lower.endsWith(QStringLiteral("mil"))) {
        value.chop(3);
    } else if (lower.endsWith(QStringLiteral("in"))) {
        unit = LengthUnit::Inch;
        value.chop(2);
    }
    const double parsed = StrictNumberParser::parseDouble(value, diagnostics, field, line);
    return UnitConverter::toMillimeters(parsed, unit);
}

/** @brief 解析 P-CAD 坐标并转换到 IR 使用的向下为正坐标系。 */
QPointF point(const Node& node, ParseDiagnostics* diagnostics, const QString& field, LengthUnit defaultUnit) {
    const QStringList values = scalarValues(node);
    if (values.size() < 2) {
        if (diagnostics)
            diagnostics->add(
                ParseSeverity::Error, ParseScope::Field, QStringLiteral("P-CAD 坐标缺少分量"), field, node.line);
        return {};
    }
    return QPointF(number(values.at(0), diagnostics, field + QStringLiteral(".X"), node.line, defaultUnit),
                   -number(values.at(1), diagnostics, field + QStringLiteral(".Y"), node.line, defaultUnit));
}

/** @brief 将 P-CAD Pad Shape 关键字映射为格式模型枚举。 */
PcadPadShape padShape(const Node& node, ParseDiagnostics* diagnostics) {
    const QString type = firstScalar(node).toUpper();
    if (type == QStringLiteral("ROUND") || type == QStringLiteral("CIRCLE"))
        return PcadPadShape::Round;
    if (type == QStringLiteral("RECT") || type == QStringLiteral("RECTANGLE"))
        return PcadPadShape::Rectangle;
    if (type == QStringLiteral("OVAL"))
        return PcadPadShape::Oval;
    if (type == QStringLiteral("RNDRECT") || type == QStringLiteral("ROUNDRECT"))
        return PcadPadShape::RoundRectangle;
    if (diagnostics)
        diagnostics->add(ParseSeverity::Warning,
                         ParseScope::Footprint,
                         QStringLiteral("P-CAD 未知焊盘形状，后续将降级"),
                         type,
                         node.line);
    return PcadPadShape::Unknown;
}

/** @brief 解析一个 Pad Style 及其孔和形状参数。 */
PcadPadStyle parsePadStyle(const Node& node, ParseDiagnostics* diagnostics, LengthUnit defaultUnit) {
    PcadPadStyle style;
    style.name = firstScalar(node);
    const Node shape = firstList(node, QStringLiteral("PADSHAPE"));
    if (!shape.atom.isEmpty()) {
        style.shape = padShape(shape, diagnostics);
        const Node shapeWidth = firstList(shape, QStringLiteral("SHAPEWIDTH"));
        const Node shapeHeight = firstList(shape, QStringLiteral("SHAPEHEIGHT"));
        style.width =
            number(firstScalar(shapeWidth), diagnostics, QStringLiteral("PADSTYLE.WIDTH"), shape.line, defaultUnit);
        style.height =
            number(firstScalar(shapeHeight), diagnostics, QStringLiteral("PADSTYLE.HEIGHT"), shape.line, defaultUnit);
    }
    const Node hole = firstList(node, QStringLiteral("HOLEDIAM"));
    if (!hole.atom.isEmpty())
        style.holeDiameter =
            number(firstScalar(hole), diagnostics, QStringLiteral("PADSTYLE.HOLEDIAM"), hole.line, defaultUnit);
    const Node plated = firstList(node, QStringLiteral("ISHOLEPLATED"));
    if (!plated.atom.isEmpty())
        style.holePlated = firstScalar(plated).compare(QStringLiteral("FALSE"), Qt::CaseInsensitive) != 0;
    if (style.name.isEmpty())
        diagnostics->add(
            ParseSeverity::Error, ParseScope::Field, QStringLiteral("P-CAD Pad Style 缺少名称"), {}, node.line);
    return style;
}

/** @brief 解析 Pattern 中的焊盘实例和旋转。 */
PcadPatternPad parsePatternPad(const Node& node, ParseDiagnostics* diagnostics, LengthUnit defaultUnit) {
    PcadPatternPad pad;
    const Node numberNode = firstList(node, QStringLiteral("PADNUM"));
    pad.number = firstScalar(numberNode);
    const Node styleNode = firstList(node, QStringLiteral("PADSTYLEREF"));
    pad.padStyleName = firstScalar(styleNode);
    const Node position = firstList(node, QStringLiteral("PT"));
    if (!position.atom.isEmpty())
        pad.position = point(position, diagnostics, QStringLiteral("PATTERN.PAD.PT"), defaultUnit);
    const Node rotation = firstList(node, QStringLiteral("ROTATION"));
    if (!rotation.atom.isEmpty())
        pad.rotation = StrictNumberParser::parseDouble(
                           firstScalar(rotation), diagnostics, QStringLiteral("PATTERN.PAD.ROTATION"), rotation.line) /
                       10.0;
    if (pad.number.isEmpty())
        diagnostics->add(
            ParseSeverity::Error, ParseScope::Footprint, QStringLiteral("P-CAD Pattern 焊盘缺少编号"), {}, node.line);
    if (pad.padStyleName.isEmpty())
        diagnostics->add(ParseSeverity::Error,
                         ParseScope::Footprint,
                         QStringLiteral("P-CAD Pattern 焊盘缺少 Pad Style 引用"),
                         pad.number,
                         node.line);
    return pad;
}

/** @brief 解析 P-CAD 线、弧、圆、多边形和文本图元。 */
PcadGraphic parseGraphic(const Node& node, ParseDiagnostics* diagnostics, LengthUnit defaultUnit) {
    PcadGraphic graphic;
    const QString kind = nameOf(node);
    if (kind == QStringLiteral("LINE"))
        graphic.type = PcadGraphicType::Line;
    else if (kind == QStringLiteral("ARC") || kind == QStringLiteral("TRIPLEPOINTARC"))
        graphic.type = PcadGraphicType::Arc;
    else if (kind == QStringLiteral("CIRCLE"))
        graphic.type = PcadGraphicType::Circle;
    else if (kind == QStringLiteral("TEXT"))
        graphic.type = PcadGraphicType::Text;
    else if (kind == QStringLiteral("POLY") || kind == QStringLiteral("POLYGON"))
        graphic.type = PcadGraphicType::Polygon;
    else {
        graphic.type = PcadGraphicType::Unknown;
        diagnostics->add(
            ParseSeverity::Skipped, ParseScope::Footprint, QStringLiteral("跳过不支持的 P-CAD 图元"), kind, node.line);
        return graphic;
    }
    const Node layer = firstList(node, QStringLiteral("LAYERNUMREF"));
    if (!layer.atom.isEmpty())
        graphic.layerNumber = StrictNumberParser::parseInteger(
            firstScalar(layer), diagnostics, QStringLiteral("GRAPHIC.LAYER"), layer.line);
    const Node width = firstList(node, QStringLiteral("WIDTH"));
    if (!width.atom.isEmpty())
        graphic.width =
            number(firstScalar(width), diagnostics, QStringLiteral("GRAPHIC.WIDTH"), width.line, defaultUnit);
    for (const Node& child : node.children) {
        if (!child.isList()) {
            if (graphic.type == PcadGraphicType::Text && graphic.text.isEmpty())
                graphic.text = child.atom;
            continue;
        }
        const QString childName = nameOf(child);
        if (childName == QStringLiteral("PT")) {
            if (graphic.type == PcadGraphicType::Arc || graphic.type == PcadGraphicType::Circle ||
                graphic.type == PcadGraphicType::Text)
                graphic.center = point(child, diagnostics, QStringLiteral("GRAPHIC.CENTER"), defaultUnit);
            else
                graphic.points.append(point(child, diagnostics, QStringLiteral("GRAPHIC.POINT"), defaultUnit));
        } else if (childName == QStringLiteral("RADIUS")) {
            graphic.radius =
                number(firstScalar(child), diagnostics, QStringLiteral("GRAPHIC.RADIUS"), child.line, defaultUnit);
        } else if (childName == QStringLiteral("STARTANGLE")) {
            graphic.startAngle =
                StrictNumberParser::parseDouble(
                    firstScalar(child), diagnostics, QStringLiteral("GRAPHIC.STARTANGLE"), child.line) /
                10.0;
        } else if (childName == QStringLiteral("SWEEPANGLE")) {
            graphic.sweepAngle =
                StrictNumberParser::parseDouble(
                    firstScalar(child), diagnostics, QStringLiteral("GRAPHIC.SWEEPANGLE"), child.line) /
                10.0;
        } else if (childName == QStringLiteral("HEIGHT") && graphic.type == PcadGraphicType::Text) {
            graphic.textHeight =
                number(firstScalar(child), diagnostics, QStringLiteral("GRAPHIC.TEXT.HEIGHT"), child.line, defaultUnit);
        } else if (childName == QStringLiteral("ROTATION")) {
            graphic.rotation = StrictNumberParser::parseDouble(
                                   firstScalar(child), diagnostics, QStringLiteral("GRAPHIC.ROTATION"), child.line) /
                               10.0;
        } else if (childName == QStringLiteral("WIDTH")) {
            graphic.width =
                number(firstScalar(child), diagnostics, QStringLiteral("GRAPHIC.WIDTH"), child.line, defaultUnit);
        }
    }
    return graphic;
}

/** @brief 解析 Pattern 的焊盘和封装图形。 */
PcadPattern parsePattern(const Node& node, ParseDiagnostics* diagnostics, LengthUnit defaultUnit) {
    PcadPattern pattern;
    pattern.name = firstScalar(node);
    if (pattern.name.isEmpty())
        diagnostics->add(
            ParseSeverity::Error, ParseScope::Footprint, QStringLiteral("P-CAD Pattern 缺少名称"), {}, node.line);
    for (const Node& child : node.children) {
        if (!child.isList())
            continue;
        const QString kind = nameOf(child);
        if (kind == QStringLiteral("PAD"))
            pattern.pads.append(parsePatternPad(child, diagnostics, defaultUnit));
        // Pattern 图形共用同一套图元解析和单位转换逻辑。
        else if (kind == QStringLiteral("PATTERNGRAPHICS") || kind == QStringLiteral("GRAPHICS")) {
            for (const Node& graphicNode : child.children) {
                if (graphicNode.isList())
                    pattern.graphics.append(parseGraphic(graphicNode, diagnostics, defaultUnit));
            }
        }
    }
    return pattern;
}

/** @brief 报告 P-CAD 库中重复的命名定义，阻止后续引用产生隐式优先级。 */
template <typename Definition>
void reportDuplicateDefinition(const QList<Definition>& definitions,
                               const QString& name,
                               ParseDiagnostics* diagnostics,
                               const QString& type,
                               int line) {
    if (name.isEmpty() || !diagnostics)
        return;
    int matches = 0;
    for (const Definition& definition : definitions) {
        if (definition.name == name)
            ++matches;
    }
    if (matches > 1)
        diagnostics->add(ParseSeverity::Error,
                         ParseScope::File,
                         QStringLiteral("P-CAD %1名称重复，引用将被视为歧义").arg(type),
                         name,
                         line);
}

/** @brief 解析板级 Pattern 放置、旋转和镜像状态。 */
PcadPlacement parsePlacement(const Node& node, ParseDiagnostics* diagnostics, LengthUnit defaultUnit) {
    PcadPlacement placement;
    const Node pattern = firstList(node, QStringLiteral("PATTERNREF"));
    placement.patternName = firstScalar(pattern);
    const Node reference = firstList(node, QStringLiteral("REFDESREF"));
    placement.reference = firstScalar(reference);
    const Node position = firstList(node, QStringLiteral("PT"));
    if (!position.atom.isEmpty())
        placement.position = point(position, diagnostics, QStringLiteral("PLACEMENT.PT"), defaultUnit);
    const Node rotation = firstList(node, QStringLiteral("ROTATION"));
    if (!rotation.atom.isEmpty())
        placement.rotation =
            StrictNumberParser::parseDouble(
                firstScalar(rotation), diagnostics, QStringLiteral("PLACEMENT.ROTATION"), rotation.line) /
            10.0;
    const Node flipped = firstList(node, QStringLiteral("ISFLIPPED"));
    placement.flipped = firstScalar(flipped).compare(QStringLiteral("TRUE"), Qt::CaseInsensitive) == 0;
    if (placement.patternName.isEmpty())
        diagnostics->add(ParseSeverity::Error,
                         ParseScope::Component,
                         QStringLiteral("P-CAD 器件放置缺少 Pattern 引用"),
                         placement.reference,
                         node.line);
    if (placement.reference.isEmpty())
        diagnostics->add(ParseSeverity::Warning,
                         ParseScope::Component,
                         QStringLiteral("P-CAD 器件放置缺少参考标识"),
                         placement.patternName,
                         node.line);
    return placement;
}

}  // namespace

/** @brief 判断 P-CAD 模型是否包含可转换内容。 */
bool PcadBoard::isRecognized() const {
    return !padStyles.isEmpty() || !patterns.isEmpty() || !placements.isEmpty() || !graphics.isEmpty();
}

/** @brief 解析 P-CAD 根节点并构建格式专用模型。 */
PcadBoard PcadParser::parse(const QString& content, const QString& filePath) {
    PcadBoard board;
    board.diagnostics.setFilePath(filePath);
    const QList<SExpressionNode> roots = SExpressionParser::parse(content, &board.diagnostics);
    for (const Node& root : roots) {
        if (nameOf(root) != QStringLiteral("ACCEL_ASCII"))
            continue;
        board.name = firstScalar(root);
        // 先读取文件级单位，确保后续库定义不受节点顺序影响。
        for (const Node& rootChild : root.children) {
            if (rootChild.isList() && nameOf(rootChild) == QStringLiteral("UNITS")) {
                board.unit = UnitConverter::parseUnit(firstScalar(rootChild), &board.diagnostics);
                break;
            }
        }
        for (const Node& child : root.children) {
            if (!child.isList())
                continue;
            const QString kind = nameOf(child);
            if (kind == QStringLiteral("UNITS")) {
                board.unit = UnitConverter::parseUnit(firstScalar(child), &board.diagnostics);
            } else if (kind == QStringLiteral("LAYER") || kind == QStringLiteral("LAYERDEF")) {
                const QStringList values = scalarValues(child);
                PcadLayer layer;
                if (!values.isEmpty())
                    layer.number = static_cast<int>(StrictNumberParser::parseInteger(
                        values.first(), &board.diagnostics, QStringLiteral("LAYER.NUMBER"), child.line));
                if (values.size() > 1)
                    layer.name = values.at(1);
                if (values.isEmpty())
                    board.diagnostics.add(ParseSeverity::Error,
                                          ParseScope::Field,
                                          QStringLiteral("P-CAD 层定义缺少编号"),
                                          {},
                                          child.line);
                board.layers.append(layer);
            } else if (kind == QStringLiteral("LIBRARY")) {
                for (const Node& definition : child.children) {
                    if (!definition.isList())
                        continue;
                    const QString definitionName = nameOf(definition);
                    if (definitionName == QStringLiteral("PADSTYLEDEF")) {
                        board.padStyles.append(parsePadStyle(definition, &board.diagnostics, board.unit));
                        reportDuplicateDefinition(board.padStyles,
                                                  board.padStyles.last().name,
                                                  &board.diagnostics,
                                                  QStringLiteral("Pad Style"),
                                                  definition.line);
                    } else if (definitionName == QStringLiteral("PATTERNDEF") ||
                               definitionName == QStringLiteral("PATTERNDEFEXTENDED")) {
                        board.patterns.append(parsePattern(definition, &board.diagnostics, board.unit));
                        reportDuplicateDefinition(board.patterns,
                                                  board.patterns.last().name,
                                                  &board.diagnostics,
                                                  QStringLiteral("Pattern"),
                                                  definition.line);
                    }
                }
            } else if (kind == QStringLiteral("PCBDESIGN")) {
                for (const Node& design : child.children) {
                    if (!design.isList())
                        continue;
                    if (nameOf(design) == QStringLiteral("MULTILAYER")) {
                        for (const Node& placement : design.children) {
                            if (placement.isList() && nameOf(placement) == QStringLiteral("PATTERN"))
                                board.placements.append(parsePlacement(placement, &board.diagnostics, board.unit));
                        }
                    } else if (nameOf(design) == QStringLiteral("LAYERCONTENTS")) {
                        for (const Node& graphic : design.children) {
                            if (graphic.isList())
                                board.graphics.append(parseGraphic(graphic, &board.diagnostics, board.unit));
                        }
                    }
                }
            }
        }
        break;
    }
    if (roots.isEmpty() || !board.isRecognized())
        board.diagnostics.add(
            ParseSeverity::Error, ParseScope::File, QStringLiteral("未识别到有效的 P-CAD ASCII PCB 定义"), filePath);
    for (int index = 0; index < board.padStyles.size(); ++index) {
        const PcadPadStyle& style = board.padStyles.at(index);
        for (int otherIndex = index + 1; otherIndex < board.padStyles.size(); ++otherIndex) {
            if (style.name == board.padStyles.at(otherIndex).name && !style.name.isEmpty()) {
                board.diagnostics.add(ParseSeverity::Error,
                                      ParseScope::Footprint,
                                      QStringLiteral("P-CAD Pad Style 名称重复"),
                                      style.name);
                break;
            }
        }
    }
    for (int index = 0; index < board.patterns.size(); ++index) {
        const PcadPattern& pattern = board.patterns.at(index);
        for (int otherIndex = index + 1; otherIndex < board.patterns.size(); ++otherIndex) {
            if (pattern.name == board.patterns.at(otherIndex).name && !pattern.name.isEmpty()) {
                board.diagnostics.add(ParseSeverity::Error,
                                      ParseScope::Footprint,
                                      QStringLiteral("P-CAD Pattern 名称重复"),
                                      pattern.name);
                break;
            }
        }
    }
    return board;
}

}  // namespace EasyKiConverter::Parser
