#include "PcadModel.h"

#include "EncodingDetector.h"

#include <QRegularExpression>
#include <QSet>

namespace EasyKiConverter::Parser {
namespace {

// 为 P-CAD 读取器统一使用通用 S-expression 节点别名。
using Node = SExpressionNode;

/**
 * @brief 返回节点名称的大写形式，便于匹配不区分大小写的 P-CAD 关键字。
 * @param node 待读取名称的 S-expression 节点。
 * @return 节点原子的大写文本。
 */
QString nameOf(const Node& node) {
    return node.atom.toUpper();
}

/**
 * @brief 返回节点中指定名称的直接子列表。
 * @param node 待搜索的父节点。
 * @param name 子列表名称。
 * @return 匹配的直接子列表。
 */
QList<Node> lists(const Node& node, const QString& name) {
    QList<Node> result;  // 保存名称匹配的直接子列表。
    for (const Node& child : node.children) {
        if (child.isList() && nameOf(child) == name.toUpper())
            result.append(child);
    }
    return result;
}

/**
 * @brief 返回节点中指定名称的第一个直接子列表。
 * @param node 待搜索的父节点。
 * @param name 子列表名称。
 * @return 第一个匹配的子列表，缺失时返回空节点。
 */
Node firstList(const Node& node, const QString& name) {
    const QList<Node> matches = lists(node, name);  // 名称匹配的子列表。
    return matches.isEmpty() ? Node() : matches.first();
}

/**
 * @brief 提取节点的直接标量参数，保留源文件顺序。
 * @param node 待读取参数的节点。
 * @return 按源文件顺序排列的标量参数。
 */
QStringList scalarValues(const Node& node) {
    QStringList result;  // 保存节点的直接标量参数。
    for (const Node& child : node.children) {
        if (!child.isList())
            result.append(child.atom);
    }
    return result;
}

/**
 * @brief 返回节点的第一个标量参数。
 * @param node 待读取参数的节点。
 * @return 第一个标量参数，缺失时返回空字符串。
 */
QString firstScalar(const Node& node) {
    const QStringList values = scalarValues(node);  // 节点的全部直接标量参数。
    return values.isEmpty() ? QString() : values.first();
}

/**
 * @brief 严格解析 P-CAD 长度并统一转换为毫米。
 * @param text 长度文本。
 * @param diagnostics 接收数值解析诊断的对象。
 * @param field 长度字段的诊断路径。
 * @param line 长度字段所在的源文件行号。
 * @param defaultUnit 未声明单位时使用的默认单位。
 * @return 转换为毫米后的长度。
 */
double number(const QString& text,
              ParseDiagnostics* diagnostics,
              const QString& field,
              int line,
              LengthUnit defaultUnit = LengthUnit::Mil) {
    QString value = text.trimmed();  // 去除长度文本两侧空白。
    LengthUnit unit = defaultUnit;  // 当前长度使用的单位。
    const QString lower = value.toLower();  // 用于不区分大小写的单位匹配。
    if (lower.endsWith(QStringLiteral("mm"))) {
        unit = LengthUnit::Millimeter;
        value.chop(2);
    } else if (lower.endsWith(QStringLiteral("mil"))) {
        value.chop(3);
    } else if (lower.endsWith(QStringLiteral("in"))) {
        unit = LengthUnit::Inch;
        value.chop(2);
    }
    const double parsed = StrictNumberParser::parseDouble(value, diagnostics, field, line);  // 解析数值部分。
    return UnitConverter::toMillimeters(parsed, unit);
}

/**
 * @brief 解析 P-CAD 坐标并转换到 IR 使用的向下为正坐标系。
 * @param node 坐标节点。
 * @param diagnostics 接收坐标解析诊断的对象。
 * @param field 坐标字段的诊断路径。
 * @param defaultUnit 未声明单位时使用的默认单位。
 * @return 转换后的坐标。
 */
QPointF point(const Node& node, ParseDiagnostics* diagnostics, const QString& field, LengthUnit defaultUnit) {
    const QStringList values = scalarValues(node);  // 坐标节点的 X、Y 标量参数。
    if (values.size() < 2) {
        if (diagnostics)
            diagnostics->add(
                ParseSeverity::Error, ParseScope::Field, QStringLiteral("P-CAD 坐标缺少分量"), field, node.line);
        return {};
    }
    return QPointF(number(values.at(0), diagnostics, field + QStringLiteral(".X"), node.line, defaultUnit),
                   -number(values.at(1), diagnostics, field + QStringLiteral(".Y"), node.line, defaultUnit));
}

/**
 * @brief 将 P-CAD Pad Shape 关键字映射为格式模型枚举。
 * @param node Pad Shape 节点。
 * @param diagnostics 接收未知形状诊断的对象。
 * @return P-CAD 焊盘形状枚举。
 */
PcadPadShape padShape(const Node& node, ParseDiagnostics* diagnostics) {
    const QString type = firstScalar(node).toUpper();  // 规范化后的形状关键字。
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
                         QStringLiteral("P-CAD 未知焊盘形状，无法映射到统一 IR"),
                         type,
                         node.line);
    return PcadPadShape::Unknown;
}

/**
 * @brief 解析一个 Pad Style 及其孔和形状参数。
 * @param node Pad Style 节点。
 * @param diagnostics 接收解析诊断的对象。
 * @param defaultUnit 未声明尺寸单位时使用的默认单位。
 * @return 解析后的 Pad Style。
 */
PcadPadStyle parsePadStyle(const Node& node, ParseDiagnostics* diagnostics, LengthUnit defaultUnit) {
    PcadPadStyle style;  // 保存当前 Pad Style 定义。
    style.name = firstScalar(node);
    const Node shape = firstList(node, QStringLiteral("PADSHAPE"));  // Pad 形状节点。
    if (!shape.atom.isEmpty()) {
        style.shape = padShape(shape, diagnostics);
        const Node shapeWidth = firstList(shape, QStringLiteral("SHAPEWIDTH"));  // Pad 宽度节点。
        const Node shapeHeight = firstList(shape, QStringLiteral("SHAPEHEIGHT"));  // Pad 高度节点。
        style.width =
            number(firstScalar(shapeWidth), diagnostics, QStringLiteral("PADSTYLE.WIDTH"), shape.line, defaultUnit);
        style.height =
            number(firstScalar(shapeHeight), diagnostics, QStringLiteral("PADSTYLE.HEIGHT"), shape.line, defaultUnit);
    }
    const Node hole = firstList(node, QStringLiteral("HOLEDIAM"));  // 孔径节点。
    if (!hole.atom.isEmpty())
        style.holeDiameter =
            number(firstScalar(hole), diagnostics, QStringLiteral("PADSTYLE.HOLEDIAM"), hole.line, defaultUnit);
    const Node plated = firstList(node, QStringLiteral("ISHOLEPLATED"));  // 镀孔标志节点。
    if (!plated.atom.isEmpty())
        style.holePlated = firstScalar(plated).compare(QStringLiteral("FALSE"), Qt::CaseInsensitive) != 0;
    if (style.name.isEmpty())
        diagnostics->add(
            ParseSeverity::Error, ParseScope::Field, QStringLiteral("P-CAD Pad Style 缺少名称"), {}, node.line);
    return style;
}

/**
 * @brief 解析 Pattern 中的焊盘实例和旋转。
 * @param node Pattern 焊盘节点。
 * @param diagnostics 接收解析诊断的对象。
 * @param defaultUnit 未声明坐标单位时使用的默认单位。
 * @return 解析后的 Pattern 焊盘。
 */
PcadPatternPad parsePatternPad(const Node& node, ParseDiagnostics* diagnostics, LengthUnit defaultUnit) {
    PcadPatternPad pad;  // 保存当前 Pattern 焊盘实例。
    const Node numberNode = firstList(node, QStringLiteral("PADNUM"));  // 焊盘编号节点。
    pad.number = firstScalar(numberNode);
    const Node styleNode = firstList(node, QStringLiteral("PADSTYLEREF"));  // Pad Style 引用节点。
    pad.padStyleName = firstScalar(styleNode);
    const Node position = firstList(node, QStringLiteral("PT"));  // 焊盘位置节点。
    if (!position.atom.isEmpty())
        pad.position = point(position, diagnostics, QStringLiteral("PATTERN.PAD.PT"), defaultUnit);
    const Node rotation = firstList(node, QStringLiteral("ROTATION"));  // 焊盘旋转节点。
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

/**
 * @brief 解析 P-CAD 线、弧、圆、多边形和文本图元。
 * @param node 图元节点。
 * @param diagnostics 接收解析诊断的对象。
 * @param defaultUnit 未声明尺寸单位时使用的默认单位。
 * @return 解析后的 P-CAD 图元。
 */
PcadGraphic parseGraphic(const Node& node, ParseDiagnostics* diagnostics, LengthUnit defaultUnit) {
    PcadGraphic graphic;  // 保存当前图元的类型、几何和文本属性。
    const QString kind = nameOf(node);  // 图元关键字。
    if (kind == QStringLiteral("LINE"))
        graphic.type = PcadGraphicType::Line;
    else if (kind == QStringLiteral("ARC") || kind == QStringLiteral("TRIPLEPOINTARC"))
        graphic.type = PcadGraphicType::Arc;
    else if (kind == QStringLiteral("CIRCLE"))
        graphic.type = PcadGraphicType::Circle;
    else if (kind == QStringLiteral("TEXT"))
        graphic.type = PcadGraphicType::Text;
    else if (kind == QStringLiteral("POLY") || kind == QStringLiteral("POLYGON") || kind == QStringLiteral("PCBPOLY"))
        graphic.type = PcadGraphicType::Polygon;
    else {
        graphic.type = PcadGraphicType::Unknown;
        diagnostics->add(
            ParseSeverity::Skipped, ParseScope::Footprint, QStringLiteral("跳过不支持的 P-CAD 图元"), kind, node.line);
        return graphic;
    }
    const Node layer = firstList(node, QStringLiteral("LAYERNUMREF"));  // 图元层引用节点。
    if (!layer.atom.isEmpty())
        graphic.layerNumber = StrictNumberParser::parseInteger(
            firstScalar(layer), diagnostics, QStringLiteral("GRAPHIC.LAYER"), layer.line);
    const Node width = firstList(node, QStringLiteral("WIDTH"));  // 图元线宽节点。
    if (!width.atom.isEmpty())
        graphic.width =
            number(firstScalar(width), diagnostics, QStringLiteral("GRAPHIC.WIDTH"), width.line, defaultUnit);
    for (const Node& child : node.children) {
        if (!child.isList()) {
            if (graphic.type == PcadGraphicType::Text && graphic.text.isEmpty())
                graphic.text = child.atom;
            continue;
        }
        const QString childName = nameOf(child);  // 当前图元属性名称。
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

/**
 * @brief 解析 Pattern 的焊盘和封装图形。
 * @param node Pattern 定义节点。
 * @param diagnostics 接收解析诊断的对象。
 * @param defaultUnit 未声明尺寸单位时使用的默认单位。
 * @return 解析后的 Pattern。
 */
PcadPattern parsePattern(const Node& node, ParseDiagnostics* diagnostics, LengthUnit defaultUnit) {
    PcadPattern pattern;  // 保存当前 Pattern 的焊盘和图形。
    pattern.name = firstScalar(node);
    if (pattern.name.isEmpty())
        diagnostics->add(
            ParseSeverity::Error, ParseScope::Footprint, QStringLiteral("P-CAD Pattern 缺少名称"), {}, node.line);
    for (const Node& child : node.children) {
        if (!child.isList())
            continue;
        const QString kind = nameOf(child);  // 当前 Pattern 子节点类型。
        if (kind == QStringLiteral("PAD"))
            pattern.pads.append(parsePatternPad(child, diagnostics, defaultUnit));
        // 标准 P-CAD Pattern 可能把焊盘放在 MULTILAYER 容器中。
        else if (kind == QStringLiteral("MULTILAYER")) {
            for (const Node& padNode : child.children) {
                if (padNode.isList() && nameOf(padNode) == QStringLiteral("PAD"))
                    pattern.pads.append(parsePatternPad(padNode, diagnostics, defaultUnit));
            }
        }
        // Pattern 图形共用同一套图元解析和单位转换逻辑。
        else if (kind == QStringLiteral("PATTERNGRAPHICS") || kind == QStringLiteral("GRAPHICS")) {
            for (const Node& graphicNode : child.children) {
                if (graphicNode.isList())
                    pattern.graphics.append(parseGraphic(graphicNode, diagnostics, defaultUnit));
            }
        } else if (kind == QStringLiteral("LAYERCONTENTS")) {
            const Node layerNode = firstList(child, QStringLiteral("LAYERNUMREF"));  // 容器层引用节点。
            const int layerNumber =  // 容器默认层号。
                layerNode.atom.isEmpty()
                    ? 0
                    : StrictNumberParser::parseInteger(
                          firstScalar(layerNode), diagnostics, QStringLiteral("GRAPHICS.LAYER"), layerNode.line);
            for (const Node& graphicNode : child.children) {
                if (!graphicNode.isList() || nameOf(graphicNode) == QStringLiteral("LAYERNUMREF"))
                    continue;
                PcadGraphic graphic = parseGraphic(graphicNode, diagnostics, defaultUnit);  // 当前图元。
                if (graphic.layerNumber == 0)
                    graphic.layerNumber = layerNumber;
                pattern.graphics.append(graphic);
            }
        }
    }
    return pattern;
}

/**
 * @brief 报告 P-CAD 库中重复的命名定义，阻止后续引用产生隐式优先级。
 * @param definitions 同类定义集合。
 * @param name 待检查的定义名称。
 * @param diagnostics 接收重复名称诊断的对象。
 * @param type 定义类型名称。
 * @param line 定义所在的源文件行号。
 */
template <typename Definition>
void reportDuplicateDefinition(const QList<Definition>& definitions,
                               const QString& name,
                               ParseDiagnostics* diagnostics,
                               const QString& type,
                               int line) {
    if (name.isEmpty() || !diagnostics)
        return;
    int matches = 0;  // 统计同名定义数量。
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

/**
 * @brief 解析板级 Pattern 放置、旋转和镜像状态。
 * @param node 器件放置节点。
 * @param diagnostics 接收解析诊断的对象。
 * @param defaultUnit 未声明坐标单位时使用的默认单位。
 * @return 解析后的器件放置。
 */
PcadPlacement parsePlacement(const Node& node, ParseDiagnostics* diagnostics, LengthUnit defaultUnit) {
    PcadPlacement placement;  // 保存当前器件放置及其关联。
    const Node pattern = firstList(node, QStringLiteral("PATTERNREF"));  // Pattern 引用节点。
    placement.patternName = firstScalar(pattern);
    const Node reference = firstList(node, QStringLiteral("REFDESREF"));  // 参考标号节点。
    placement.reference = firstScalar(reference);
    const Node position = firstList(node, QStringLiteral("PT"));  // 放置坐标节点。
    if (!position.atom.isEmpty())
        placement.position = point(position, diagnostics, QStringLiteral("PLACEMENT.PT"), defaultUnit);
    const Node rotation = firstList(node, QStringLiteral("ROTATION"));  // 放置旋转节点。
    if (!rotation.atom.isEmpty())
        placement.rotation =
            StrictNumberParser::parseDouble(
                firstScalar(rotation), diagnostics, QStringLiteral("PLACEMENT.ROTATION"), rotation.line) /
            10.0;
    const Node flipped = firstList(node, QStringLiteral("ISFLIPPED"));  // 镜像标志节点。
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

/**
 * @brief 判断 P-CAD 模型是否包含可转换内容。
 * @return 至少包含一种可转换定义时返回 true。
 */
bool PcadBoard::isRecognized() const {
    return !padStyles.isEmpty() || !patterns.isEmpty() || !placements.isEmpty() || !graphics.isEmpty();
}

/**
 * @brief 解析 P-CAD 根节点并构建格式专用模型。
 * @param content P-CAD 文本内容。
 * @param filePath 源文件路径，用于诊断定位。
 * @return 解析得到的 P-CAD 板模型。
 */
PcadBoard PcadParser::parse(const QString& content, const QString& filePath) {
    PcadBoard board;  // 保存解析得到的 P-CAD 板模型和诊断。
    board.diagnostics.setFilePath(filePath);
    const QList<SExpressionNode> roots = SExpressionParser::parse(content, &board.diagnostics);  // 根节点列表。
    for (const Node& root : roots) {
        if (nameOf(root) != QStringLiteral("ACCEL_ASCII"))
            continue;
        board.name = firstScalar(root);
        // 先读取文件级单位，确保后续库定义不受节点顺序影响。
        for (const Node& rootChild : root.children) {
            if (rootChild.isList() &&
                (nameOf(rootChild) == QStringLiteral("UNITS") || nameOf(rootChild) == QStringLiteral("ASCIIHEADER"))) {
                const Node unitNode = nameOf(rootChild) == QStringLiteral("ASCIIHEADER")  // 文件级单位节点。
                                          ? firstList(rootChild, QStringLiteral("FILEUNITS"))
                                          : rootChild;
                board.unit = UnitConverter::parseUnit(firstScalar(unitNode), &board.diagnostics);
                break;
            }
        }
        for (const Node& child : root.children) {
            if (!child.isList())
                continue;
            const QString kind = nameOf(child);  // 当前根子节点类型。
            if (kind == QStringLiteral("UNITS")) {
                board.unit = UnitConverter::parseUnit(firstScalar(child), &board.diagnostics);
            } else if (kind == QStringLiteral("ASCIIHEADER")) {
                const Node unitNode = firstList(child, QStringLiteral("FILEUNITS"));  // 文件单位节点。
                if (!unitNode.atom.isEmpty())
                    board.unit = UnitConverter::parseUnit(firstScalar(unitNode), &board.diagnostics);
            } else if (kind == QStringLiteral("LAYER") || kind == QStringLiteral("LAYERDEF")) {
                const QStringList values = scalarValues(child);  // 层定义的标量参数。
                PcadLayer layer;  // 当前层定义。
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
                    const QString definitionName = nameOf(definition);  // 库定义类型。
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
        const PcadPadStyle& style = board.padStyles.at(index);  // 当前待检查的 Pad Style。
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
        const PcadPattern& pattern = board.patterns.at(index);  // 当前待检查的 Pattern。
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

/**
 * @brief 先检测文本编码，再复用统一字符串解析流程。
 * @param data 源文件原始字节。
 * @param filePath 源文件路径，用于编码和诊断定位。
 * @return 解析得到的 P-CAD 板模型。
 */
PcadBoard PcadParser::parseBytes(const QByteArray& data, const QString& filePath) {
    ParseDiagnostics decodingDiagnostics;  // 保存编码检测阶段的诊断。
    decodingDiagnostics.setFilePath(filePath);
    const QString content = EncodingDetector::decode(data, &decodingDiagnostics, filePath);  // 解码后的文本。
    PcadBoard board = parse(content, filePath);  // 复用文本解析流程构建模型。
    board.diagnostics.append(decodingDiagnostics);
    return board;
}

}  // namespace EasyKiConverter::Parser
