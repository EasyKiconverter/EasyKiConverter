#include "CadstarModel.h"

#include "EncodingDetector.h"

#include <QRegularExpression>
#include <QSet>

namespace EasyKiConverter::Parser {

namespace {

// 为 Cadstar 分段节点提供简短别名，减少解析代码噪声。
using Node = DelimitedSectionNode;

/**
 * @brief 返回节点的第一个参数，缺失时返回空字符串。
 * @param node 待读取参数的分段节点。
 * @return 第一个参数或空字符串。
 */
QString firstArgument(const Node& node) {
    return node.arguments.value(0);
}

/**
 * @brief 解析 Cadstar 的形状名称。
 * @param value 源文件中的形状名称。
 * @param diagnostics 接收未知形状诊断的对象。
 * @param line 形状字段所在的源文件行号。
 * @return 对应的标准焊盘形状。
 */
CadstarPadShape parsePadShape(const QString& value, ParseDiagnostics* diagnostics, int line) {
    // 规范化大小写，兼容不同 Cadstar 导出器的命名风格。
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

/**
 * @brief 严格解析 Cadstar 数值字段。
 * @param value 待读取的数值文本。
 * @param diagnostics 接收解析诊断的对象。
 * @param field 数值字段的诊断路径。
 * @param line 数值字段所在的源文件行号。
 * @return 解析后的数值。
 */
double number(const QString& value, ParseDiagnostics* diagnostics, const QString& field, int line) {
    return StrictNumberParser::parseDouble(value, diagnostics, field, line);
}

/**
 * @brief 从一个或两个参数中读取坐标，并报告非法坐标。
 * @param values 坐标参数列表。
 * @param start 坐标起始参数索引。
 * @param diagnostics 接收解析诊断的对象。
 * @param field 坐标字段的诊断路径。
 * @param line 坐标字段所在的源文件行号。
 * @return 解析后的坐标，格式错误时返回原点。
 */
QPointF point(const QStringList& values, int start, ParseDiagnostics* diagnostics, const QString& field, int line) {
    if (start >= values.size()) {
        if (diagnostics)
            diagnostics->add(
                ParseSeverity::Error, ParseScope::Field, QStringLiteral("Cadstar 坐标字段缺失"), field, line);
        return {};
    }

    // 读取并清理首个坐标令牌，支持括号坐标和分离坐标两种写法。
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

/**
 * @brief 从节点参数中读取全部括号坐标。
 * @param values 节点参数列表。
 * @param diagnostics 接收解析诊断的对象。
 * @param field 坐标字段的诊断路径。
 * @param line 坐标字段所在的源文件行号。
 * @return 按源文件顺序解析出的坐标列表。
 */
QList<QPointF> points(const QStringList& values, ParseDiagnostics* diagnostics, const QString& field, int line) {
    // 收集参数中以左括号开始的坐标项。
    QList<QPointF> result;
    for (int index = 0; index < values.size(); ++index) {
        if (!values.at(index).startsWith(QChar('(')))
            continue;
        result.append(point(values, index, diagnostics, field, line));
    }
    return result;
}

/**
 * @brief 找到形如 WIDTH 0.2 的键值参数。
 * @param values 待搜索的键值参数列表。
 * @param name 要查找的键名。
 * @param diagnostics 接收数值解析诊断的对象。
 * @param field 数值字段的诊断路径。
 * @param line 字段所在的源文件行号。
 * @return 找到的数值，缺失时返回 0。
 */
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

/**
 * @brief 展开 Cadstar 的连续编号范围。
 * @param value 编号范围或逗号分隔编号文本。
 * @param diagnostics 接收范围解析诊断的对象。
 * @param line 编号字段所在的源文件行号。
 * @return 展开后的编号列表。
 */
QStringList expandNumbers(const QString& value, ParseDiagnostics* diagnostics, int line) {
    // 匹配 [起始:结束:步长] 形式的编号范围。
    static const QRegularExpression range(QStringLiteral(R"(^\[(\d+)\s*:\s*(\d+)(?:\s*:\s*(\d+))?\]$)"));
    // 保存范围匹配结果。
    const QRegularExpressionMatch match = range.match(value.trimmed());
    if (!match.hasMatch())
        return value.split(QRegularExpression(QStringLiteral("\\s*,\\s*")), Qt::SkipEmptyParts);

    // 记录范围各部分的整数转换结果。
    bool startOk = false;  // 记录起始编号的整数转换结果。
    bool endOk = false;  // 记录结束编号的整数转换结果。
    bool stepOk = true;  // 记录步长的整数转换结果。
    // 解析范围起点、终点和步长。
    const int start = match.captured(1).toInt(&startOk);  // 编号范围起点。
    const int end = match.captured(2).toInt(&endOk);  // 编号范围终点。
    const int step = match.captured(3).isEmpty() ? 1 : match.captured(3).toInt(&stepOk);  // 编号步长。
    if (!startOk || !endOk || !stepOk || step <= 0 || start > end) {
        if (diagnostics)
            diagnostics->add(ParseSeverity::Error,
                             ParseScope::Field,
                             QStringLiteral("Cadstar 引脚编号范围非法：%1").arg(value),
                             QStringLiteral("NUMBERS"),
                             line);
        return {};
    }

    // 按步长展开闭区间编号。
    QStringList result;
    for (int numberValue = start; numberValue <= end; numberValue += step)
        result.append(QString::number(numberValue));
    return result;
}

/**
 * @brief 解析图形节点的类型和基础几何数据。
 * @param node 待读取的图形节点。
 * @param graphic 接收解析结果的图形对象。
 * @param diagnostics 接收几何诊断的对象。
 * @return 图形类型和基础数据均可识别时返回 true。
 */
bool parseGraphic(const Node& node, CadstarGraphic& graphic, ParseDiagnostics* diagnostics) {
    // 统一图形关键字大小写后进行类型匹配。
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

/**
 * @brief 在重名定义上报告错误，避免关联结果静默覆盖。
 * @param definitions 同类定义集合。
 * @param name 待检查的定义名称。
 * @param diagnostics 接收重复名称诊断的对象。
 * @param type 定义类型名称。
 * @param line 定义所在的源文件行号。
 */
template <typename T>
void reportDuplicate(const QList<T>& definitions,
                     const QString& name,
                     ParseDiagnostics* diagnostics,
                     const QString& type,
                     int line) {
    // 统计同名定义数量。
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

// Cadstar 分段解析器中必须视为叶节点的关键字集合。
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

/**
 * @brief 根据已使用名称生成跨文件合并时稳定的唯一名称。
 * @param name 原始定义名称。
 * @param usedNames 已占用的名称集合。
 * @return 未占用的原名或带稳定后缀的唯一名称。
 */
QString mergedName(const QString& name, const QSet<QString>& usedNames) {
    if (!usedNames.contains(name))
        return name;
    // 从 2 开始递增，保持同一输入的重命名结果稳定。
    int suffix = 2;
    // 保存当前尝试的候选名称。
    QString candidate;
    do {
        candidate = QStringLiteral("%1_%2").arg(name).arg(suffix++);
    } while (usedNames.contains(candidate));
    return candidate;
}

/**
 * @brief 从源文件名称候选表反向找出定义的原始名称。
 * @param variants 原始名称到合并后候选名称的索引。
 * @param uniqueNameValue 合并后的唯一名称。
 * @return 对应的原始名称，找不到时返回输入名称。
 */
QString originalName(const QMap<QString, QStringList>& variants, const QString& uniqueNameValue) {
    for (auto iterator = variants.cbegin(); iterator != variants.cend(); ++iterator) {
        if (iterator.value().contains(uniqueNameValue))
            return iterator.key();
    }
    return uniqueNameValue;
}

/**
 * @brief 合并一种 Cadstar 定义并更新原始名称候选索引。
 * @param source 待合并的源定义。
 * @param target 接收合并定义的目标集合。
 * @param variants 原始名称到合并名称的索引。
 * @param usedNames 已占用的目标名称集合。
 * @param sourceVariants 源库的原始名称索引。
 * @param diagnostics 接收重命名诊断的对象。
 * @param scope 诊断所属范围。
 * @param type 定义类型名称。
 */
template <typename Definition>
void appendMergedDefinition(const Definition& source,
                            QList<Definition>& target,
                            QMap<QString, QStringList>& variants,
                            QSet<QString>& usedNames,
                            const QMap<QString, QStringList>& sourceVariants,
                            ParseDiagnostics* diagnostics,
                            ParseScope scope,
                            const QString& type) {
    // 复制定义后在目标库命名空间中调整名称。
    Definition merged = source;
    // 保留跨文件合并前的原始名称，供诊断和关联使用。
    const QString rawName = originalName(sourceVariants, source.name);
    merged.name = mergedName(source.name, usedNames);
    if (merged.name != source.name && diagnostics)
        diagnostics->add(ParseSeverity::Warning,
                         scope,
                         QStringLiteral("跨文件%1名称重复，已重命名：%2 -> %3").arg(type, source.name, merged.name),
                         rawName);
    usedNames.insert(merged.name);
    target.append(merged);
    variants[rawName].append(merged.name);
}

/**
 * @brief 对合并后的候选索引报告无法安全自动选择的原始名称。
 * @param variants 原始名称到合并名称候选的索引。
 * @param diagnostics 接收歧义诊断的对象。
 * @param scope 诊断所属范围。
 * @param type 定义类型名称。
 */
void reportAmbiguousNames(const QMap<QString, QStringList>& variants,
                          ParseDiagnostics* diagnostics,
                          ParseScope scope,
                          const QString& type) {
    for (auto iterator = variants.cbegin(); iterator != variants.cend(); ++iterator) {
        if (iterator.value().size() > 1 && diagnostics)
            diagnostics->add(ParseSeverity::Error,
                             scope,
                             QStringLiteral("跨文件%1名称存在歧义：%2").arg(type, iterator.key()),
                             iterator.key());
    }
}

}  // namespace

/**
 * @brief 判断 Cadstar 库模型是否包含可转换定义。
 * @return 至少包含一种可转换定义时返回 true。
 */
bool CadstarLibrary::isRecognized() const {
    return !pads.isEmpty() || !packages.isEmpty() || !components.isEmpty() || !parts.isEmpty();
}

/**
 * @brief 判断焊盘原始名称是否对应多个定义。
 * @param name 待检查的原始焊盘名称。
 * @return 名称对应多个定义时返回 true。
 */
bool CadstarLibrary::isPadAmbiguous(const QString& name) const {
    return padNameVariants.value(name).size() > 1;
}

/**
 * @brief 判断封装原始名称是否对应多个定义。
 * @param name 待检查的原始封装名称。
 * @return 名称对应多个定义时返回 true。
 */
bool CadstarLibrary::isPackageAmbiguous(const QString& name) const {
    return packageNameVariants.value(name).size() > 1;
}

/**
 * @brief 判断符号原始名称是否对应多个定义。
 * @param name 待检查的原始符号名称。
 * @return 名称对应多个定义时返回 true。
 */
bool CadstarLibrary::isComponentAmbiguous(const QString& name) const {
    return componentNameVariants.value(name).size() > 1;
}

/**
 * @brief 判断器件原始名称是否对应多个定义。
 * @param name 待检查的原始器件名称。
 * @return 名称对应多个定义时返回 true。
 */
bool CadstarLibrary::isPartAmbiguous(const QString& name) const {
    return partNameVariants.value(name).size() > 1;
}

/**
 * @brief 解析 Cadstar 分段树并构建格式专用模型。
 * @param content Cadstar 文本内容。
 * @param filePath 源文件路径，用于诊断定位。
 * @return 解析得到的 Cadstar 库模型。
 */
CadstarLibrary CadstarParser::parse(const QString& content, const QString& filePath) {
    // 保存当前文件的所有定义、索引和解析诊断。
    CadstarLibrary library;
    library.diagnostics.setFilePath(filePath);
    // 将文本解析为 Cadstar 顶层分段节点。
    const QList<DelimitedSectionNode> sections =
        DelimitedSectionParser::parse(content, kCadstarLeafKeywords, &library.diagnostics);

    for (const Node& root : sections) {
        if (root.keyword == QStringLiteral("UNITS")) {
            library.unit = UnitConverter::parseUnit(firstArgument(root), &library.diagnostics);
            continue;
        }
        if (root.keyword == QStringLiteral("PAD")) {
            // 保存当前焊盘定义及其几何、钻孔参数。
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
                    // 读取钻孔形状，用于区分圆孔和槽孔。
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
            library.padNameVariants[pad.name].append(pad.name);
            reportDuplicate(library.pads, pad.name, &library.diagnostics, QStringLiteral("焊盘"), root.line);
            continue;
        }
        if (root.keyword == QStringLiteral("PACKAGE")) {
            // 保存当前封装定义及其引脚和图形。
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
                    // 保存当前封装引脚及其焊盘关联。
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
                    // 尝试将未被专门处理的子节点解析为封装图形。
                    CadstarGraphic graphic;
                    if (parseGraphic(child, graphic, &library.diagnostics))
                        packageModel.graphics.append(graphic);
                }
            }
            if (packageModel.name.isEmpty())
                library.diagnostics.add(
                    ParseSeverity::Error, ParseScope::Footprint, QStringLiteral("Cadstar 封装缺少名称"), {}, root.line);
            library.packages.append(packageModel);
            library.packageNameVariants[packageModel.name].append(packageModel.name);
            reportDuplicate(
                library.packages, packageModel.name, &library.diagnostics, QStringLiteral("封装"), root.line);
            continue;
        }
        if (root.keyword == QStringLiteral("COMPONENT")) {
            // 保存当前符号定义及其引脚和图形。
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
                    // 保存当前符号引脚及其编号、类型和标签。
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
                    // 尝试将未被专门处理的子节点解析为符号图形。
                    CadstarGraphic graphic;
                    if (parseGraphic(child, graphic, &library.diagnostics))
                        component.graphics.append(graphic);
                }
            }
            if (component.name.isEmpty())
                library.diagnostics.add(
                    ParseSeverity::Error, ParseScope::Symbol, QStringLiteral("Cadstar 符号缺少名称"), {}, root.line);
            library.components.append(component);
            library.componentNameVariants[component.name].append(component.name);
            reportDuplicate(
                library.components, component.name, &library.diagnostics, QStringLiteral("符号"), root.line);
            continue;
        }
        if (root.keyword == QStringLiteral("PART")) {
            // 保存当前器件定义及其符号、封装关联。
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
            library.partNameVariants[part.name].append(part.name);
        }
    }

    if (!library.isRecognized())
        library.diagnostics.add(ParseSeverity::Error, ParseScope::File, QStringLiteral("未识别的 Cadstar ASCII 库"));
    return library;
}

/**
 * @brief 先检测文本编码，再复用统一字符串解析流程。
 * @param data 源文件原始字节。
 * @param filePath 源文件路径，用于编码和诊断定位。
 * @return 解析得到的 Cadstar 库模型。
 */
CadstarLibrary CadstarParser::parseBytes(const QByteArray& data, const QString& filePath) {
    // 保存编码检测阶段产生的诊断。
    ParseDiagnostics decodingDiagnostics;
    decodingDiagnostics.setFilePath(filePath);
    // 按文件特征解码为统一 UTF-16 字符串。
    const QString content = EncodingDetector::decode(data, &decodingDiagnostics, filePath);
    // 复用字符串解析流程构建库模型。
    CadstarLibrary library = parse(content, filePath);
    library.diagnostics.append(decodingDiagnostics);
    return library;
}

/**
 * @brief 合并 Cadstar 库并校验单位、重名和跨文件器件关联。
 * @param libraries 待合并的 Cadstar 库列表。
 * @param filePath 合并结果的逻辑路径，用于诊断定位。
 * @return 合并后的 Cadstar 库模型。
 */
CadstarLibrary CadstarMerger::merge(const QList<CadstarLibrary>& libraries, const QString& filePath) {
    // 保存合并后的定义、名称索引和诊断。
    CadstarLibrary merged;
    merged.diagnostics.setFilePath(filePath);
    if (libraries.isEmpty()) {
        merged.diagnostics.add(ParseSeverity::Error, ParseScope::File, QStringLiteral("没有可合并的 Cadstar 库"));
        return merged;
    }

    // 分别记录各类定义已占用的目标名称。
    QSet<QString> padNames;  // 已占用的焊盘名称。
    QSet<QString> packageNames;  // 已占用的封装名称。
    QSet<QString> componentNames;  // 已占用的符号名称。
    QSet<QString> partNames;  // 已占用的器件名称。
    for (const CadstarLibrary& library : libraries) {
        merged.diagnostics.append(library.diagnostics);
        if (merged.unit != LengthUnit::Unknown && library.unit != LengthUnit::Unknown && merged.unit != library.unit)
            merged.diagnostics.add(
                ParseSeverity::Error, ParseScope::File, QStringLiteral("Cadstar 库单位不一致，无法安全合并"), filePath);
        else if (merged.unit == LengthUnit::Unknown)
            merged.unit = library.unit;

        for (const CadstarPad& pad : library.pads)
            appendMergedDefinition(pad,
                                   merged.pads,
                                   merged.padNameVariants,
                                   padNames,
                                   library.padNameVariants,
                                   &merged.diagnostics,
                                   ParseScope::Footprint,
                                   QStringLiteral("焊盘"));
        for (const CadstarPackage& packageModel : library.packages)
            appendMergedDefinition(packageModel,
                                   merged.packages,
                                   merged.packageNameVariants,
                                   packageNames,
                                   library.packageNameVariants,
                                   &merged.diagnostics,
                                   ParseScope::Footprint,
                                   QStringLiteral("封装"));
        for (const CadstarComponent& component : library.components)
            appendMergedDefinition(component,
                                   merged.components,
                                   merged.componentNameVariants,
                                   componentNames,
                                   library.componentNameVariants,
                                   &merged.diagnostics,
                                   ParseScope::Symbol,
                                   QStringLiteral("符号"));
        for (const CadstarPart& part : library.parts) {
            const QString rawName = originalName(library.partNameVariants, part.name);  // 合并前的器件名称。
            CadstarPart mergedPart = part;  // 当前器件的合并副本。
            mergedPart.name = mergedName(part.name, partNames);
            if (mergedPart.name != part.name)
                merged.diagnostics.add(
                    ParseSeverity::Warning,
                    ParseScope::Component,
                    QStringLiteral("跨文件器件名称重复，已重命名：%1 -> %2").arg(part.name, mergedPart.name),
                    rawName);
            partNames.insert(mergedPart.name);
            merged.parts.append(mergedPart);
            merged.partNameVariants[rawName].append(mergedPart.name);
        }
    }
    reportAmbiguousNames(merged.padNameVariants, &merged.diagnostics, ParseScope::Footprint, QStringLiteral("焊盘"));
    reportAmbiguousNames(
        merged.packageNameVariants, &merged.diagnostics, ParseScope::Footprint, QStringLiteral("封装"));
    reportAmbiguousNames(merged.componentNameVariants, &merged.diagnostics, ParseScope::Symbol, QStringLiteral("符号"));
    reportAmbiguousNames(merged.partNameVariants, &merged.diagnostics, ParseScope::Component, QStringLiteral("器件"));
    if (!merged.isRecognized())
        merged.diagnostics.add(ParseSeverity::Error, ParseScope::File, QStringLiteral("合并结果没有 Cadstar 定义"));
    return merged;
}

}  // namespace EasyKiConverter::Parser
