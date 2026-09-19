#include "XpeditionHkpModel.h"

#include "TextParsers.h"

#include <QRegularExpression>

namespace EasyKiConverter::Parser {

namespace {

QString upper(const QString& value) {
    return value.trimmed().toUpper();
}

QString unquote(QString value) {
    value = value.trimmed();
    if (value.size() >= 2 && value.startsWith(QChar('"')) && value.endsWith(QChar('"')))
        return value.mid(1, value.size() - 2);
    return value;
}

QString valueOf(const SectionNode& node) {
    return unquote(node.value);
}

const SectionNode* child(const SectionNode& node, const QString& keyword) {
    for (const SectionNode& item : node.children) {
        if (upper(item.keyword) == upper(keyword))
            return &item;
    }
    return nullptr;
}

QList<const SectionNode*> childrenOf(const SectionNode& node, const QString& keyword) {
    QList<const SectionNode*> result;
    for (const SectionNode& item : node.children) {
        if (upper(item.keyword) == upper(keyword))
            result.append(&item);
    }
    return result;
}

double scale(double value, LengthUnit unit) {
    return UnitConverter::toMillimeters(value, unit);
}

bool parsePair(const QString& text,
               LengthUnit unit,
               ParseDiagnostics* diagnostics,
               const QString& field,
               int line,
               QPointF& result) {
    static const QRegularExpression pattern(QStringLiteral(R"(^\s*\(?\s*([^,\s]+)\s*,\s*([^\)\s]+)\s*\)?\s*$)"));
    const QRegularExpressionMatch match = pattern.match(text);
    if (!match.hasMatch()) {
        if (diagnostics)
            diagnostics->add(
                ParseSeverity::Error, ParseScope::Field, QStringLiteral("坐标字段格式错误：%1").arg(text), field, line);
        return false;
    }
    result = QPointF(scale(StrictNumberParser::parseDouble(match.captured(1), diagnostics, field, line), unit),
                     scale(StrictNumberParser::parseDouble(match.captured(2), diagnostics, field, line), unit));
    return true;
}

XpeditionPadShape shapeOf(const QString& value) {
    const QString type = upper(value);
    if (type == QStringLiteral("ROUND"))
        return XpeditionPadShape::Round;
    if (type == QStringLiteral("RECTANGLE"))
        return XpeditionPadShape::Rectangle;
    if (type == QStringLiteral("OBLONG"))
        return XpeditionPadShape::Oblong;
    if (type == QStringLiteral("SQUARE"))
        return XpeditionPadShape::Square;
    if (type == QStringLiteral("POLYGON"))
        return XpeditionPadShape::Polygon;
    return XpeditionPadShape::Unknown;
}

// 解析 Xpedition 中常见的布尔字段，未知值必须进入诊断而不是默认为真。
bool parseBool(const QString& value, ParseDiagnostics* diagnostics, const QString& field, int line) {
    const QString normalized = upper(value);
    if (normalized == QStringLiteral("TRUE") || normalized == QStringLiteral("YES") ||
        normalized == QStringLiteral("ON") || normalized == QStringLiteral("1"))
        return true;
    if (normalized == QStringLiteral("FALSE") || normalized == QStringLiteral("NO") ||
        normalized == QStringLiteral("OFF") || normalized == QStringLiteral("0"))
        return false;
    if (diagnostics)
        diagnostics->add(
            ParseSeverity::Error, ParseScope::Field, QStringLiteral("字段不是合法布尔值：%1").arg(value), field, line);
    return false;
}

void duplicateWarning(ParseDiagnostics* diagnostics, ParseScope scope, const QString& name, int line) {
    if (diagnostics)
        diagnostics->add(
            ParseSeverity::Warning, scope, QStringLiteral("重复名称将保留为独立定义：%1").arg(name), name, line);
}

QString uniqueName(const QString& name,
                   const QStringList& names,
                   ParseDiagnostics* diagnostics,
                   ParseScope scope,
                   int line) {
    if (!names.contains(name))
        return name;
    duplicateWarning(diagnostics, scope, name, line);
    int suffix = 2;
    QString candidate;
    do {
        candidate = QStringLiteral("%1_%2").arg(name).arg(suffix++);
    } while (names.contains(candidate));
    return candidate;
}

void parseSize(const SectionNode& node, LengthUnit unit, ParseDiagnostics* diagnostics, QSizeF& size) {
    if (upper(node.keyword) == QStringLiteral("ROUND") || upper(node.keyword) == QStringLiteral("SQUARE")) {
        if (const SectionNode* diameter = child(node, QStringLiteral("DIAMETER"))) {
            const double value = scale(StrictNumberParser::parseDouble(
                                           valueOf(*diameter), diagnostics, QStringLiteral("DIAMETER"), diameter->line),
                                       unit);
            size = QSizeF(value, value);
        }
        return;
    }
    if (const SectionNode* width = child(node, QStringLiteral("WIDTH")))
        size.setWidth(scale(
            StrictNumberParser::parseDouble(valueOf(*width), diagnostics, QStringLiteral("WIDTH"), width->line), unit));
    if (const SectionNode* height = child(node, QStringLiteral("HEIGHT")))
        size.setHeight(scale(
            StrictNumberParser::parseDouble(valueOf(*height), diagnostics, QStringLiteral("HEIGHT"), height->line),
            unit));
    if (size.height() == 0.0)
        size.setHeight(size.width());
}

void parsePad(const SectionNode& node, LengthUnit unit, XpeditionHkpModel& model, ParseDiagnostics* diagnostics) {
    const QString rawName = valueOf(node);
    XpeditionPadDefinition pad;
    pad.name = uniqueName(
        rawName,
        [&]() {
            QStringList names;
            for (const auto& item : model.pads)
                names.append(item.name);
            return names;
        }(),
        diagnostics,
        ParseScope::Footprint,
        node.line);
    pad.line = node.line;
    if (!node.children.isEmpty()) {
        pad.shape = shapeOf(node.children.first().keyword);
        if (pad.shape == XpeditionPadShape::Unknown && diagnostics)
            diagnostics->add(ParseSeverity::Skipped,
                             ParseScope::Footprint,
                             QStringLiteral("不支持的 Pad 图元：%1").arg(node.children.first().keyword),
                             pad.name,
                             node.children.first().line);
        parseSize(node.children.first(), unit, diagnostics, pad.size);
        if (const SectionNode* offset = child(node.children.first(), QStringLiteral("OFFSET")))
            parsePair(valueOf(*offset), unit, diagnostics, QStringLiteral("OFFSET"), offset->line, pad.offset);
        for (const SectionNode* xy : childrenOf(node.children.first(), QStringLiteral("XY"))) {
            QPointF point;
            if (parsePair(valueOf(*xy), unit, diagnostics, QStringLiteral("XY"), xy->line, point))
                pad.polygon.append(point);
        }
    } else {
        if (diagnostics)
            diagnostics->add(
                ParseSeverity::Warning, ParseScope::Footprint, QStringLiteral("Pad 没有几何定义"), pad.name, node.line);
    }
    model.pads.append(pad);
}

void parseHole(const SectionNode& node, LengthUnit unit, XpeditionHkpModel& model, ParseDiagnostics* diagnostics) {
    XpeditionHoleDefinition hole;
    hole.name = uniqueName(
        valueOf(node),
        [&]() {
            QStringList names;
            for (const auto& item : model.holes)
                names.append(item.name);
            return names;
        }(),
        diagnostics,
        ParseScope::Footprint,
        node.line);
    hole.line = node.line;
    if (!node.children.isEmpty()) {
        hole.shape = shapeOf(node.children.first().keyword);
        if (hole.shape == XpeditionPadShape::Unknown && diagnostics)
            diagnostics->add(ParseSeverity::Skipped,
                             ParseScope::Footprint,
                             QStringLiteral("不支持的孔图元：%1").arg(node.children.first().keyword),
                             hole.name,
                             node.children.first().line);
        parseSize(node.children.first(), unit, diagnostics, hole.size);
    }
    if (const SectionNode* options = child(node, QStringLiteral("HOLE_OPTIONS")))
        hole.plated = upper(valueOf(*options)).contains(QStringLiteral("PLATED"));
    model.holes.append(hole);
}

void parsePadstack(const SectionNode& node, XpeditionHkpModel& model, ParseDiagnostics* diagnostics) {
    XpeditionPadstackDefinition padstack;
    padstack.name = uniqueName(
        valueOf(node),
        [&]() {
            QStringList names;
            for (const auto& item : model.padstacks)
                names.append(item.name);
            return names;
        }(),
        diagnostics,
        ParseScope::Footprint,
        node.line);
    padstack.line = node.line;
    QList<const SectionNode*> fields;
    for (const SectionNode& item : node.children) {
        fields.append(&item);
        if (upper(item.keyword) == QStringLiteral("TECHNOLOGY")) {
            for (const SectionNode& technologyField : item.children)
                fields.append(&technologyField);
        }
    }
    for (const SectionNode* field : fields) {
        const SectionNode& item = *field;
        const QString keyword = upper(item.keyword);
        const QString value = valueOf(item);
        if (keyword == QStringLiteral("PADSTACK_TYPE"))
            padstack.technology = value;
        else if (keyword == QStringLiteral("TOP_PAD"))
            padstack.topPad = value;
        else if (keyword == QStringLiteral("BOTTOM_PAD"))
            padstack.bottomPad = value;
        else if (keyword == QStringLiteral("TOP_SOLDERMASK_PAD"))
            padstack.topSolderMaskPad = value;
        else if (keyword == QStringLiteral("BOTTOM_SOLDERMASK_PAD"))
            padstack.bottomSolderMaskPad = value;
        else if (keyword == QStringLiteral("TOP_SOLDERPASTE_PAD"))
            padstack.topSolderPastePad = value;
        else if (keyword == QStringLiteral("BOTTOM_SOLDERPASTE_PAD"))
            padstack.bottomSolderPastePad = value;
        else if (keyword == QStringLiteral("HOLE_NAME"))
            padstack.holeName = value;
    }
    model.padstacks.append(padstack);
}

void parseCell(const SectionNode& node, LengthUnit unit, XpeditionHkpModel& model, ParseDiagnostics* diagnostics) {
    XpeditionCellDefinition cell;
    cell.name = uniqueName(
        valueOf(node),
        [&]() {
            QStringList names;
            for (const auto& item : model.cells)
                names.append(item.name);
            return names;
        }(),
        diagnostics,
        ParseScope::Footprint,
        node.line);
    cell.line = node.line;
    for (const SectionNode& item : node.children) {
        const QString keyword = upper(item.keyword);
        if (keyword == QStringLiteral("PACKAGE_GROUP"))
            cell.packageGroup = valueOf(item);
        else if (keyword == QStringLiteral("MOUNT_TYPE"))
            cell.mountType = valueOf(item);
        else if (keyword == QStringLiteral("NUMBER_LAYERS"))
            cell.numberOfLayers = static_cast<int>(StrictNumberParser::parseInteger(
                valueOf(item), diagnostics, QStringLiteral("NUMBER_LAYERS"), item.line));
        else if (keyword == QStringLiteral("PIN")) {
            XpeditionCellPin pin;
            pin.number = valueOf(item);
            pin.line = item.line;
            if (const SectionNode* xy = child(item, QStringLiteral("XY")))
                parsePair(valueOf(*xy), unit, diagnostics, QStringLiteral("XY"), xy->line, pin.position);
            if (const SectionNode* padstack = child(item, QStringLiteral("PADSTACK")))
                pin.padstack = valueOf(*padstack);
            if (const SectionNode* rotation = child(item, QStringLiteral("ROTATION")))
                pin.rotation = StrictNumberParser::parseDouble(
                    valueOf(*rotation), diagnostics, QStringLiteral("ROTATION"), rotation->line);
            if (const SectionNode* mirror = child(item, QStringLiteral("MIRROR")))
                pin.mirror = parseBool(valueOf(*mirror), diagnostics, QStringLiteral("MIRROR"), mirror->line);
            cell.pins.append(pin);
        } else if (keyword.endsWith(QStringLiteral("OUTLINE"))) {
            XpeditionCellOutline outline;
            outline.layer = item.keyword;
            outline.line = item.line;
            for (const SectionNode& shape : item.children) {
                for (const SectionNode* xy : childrenOf(shape, QStringLiteral("XY"))) {
                    QPointF point;
                    if (parsePair(valueOf(*xy), unit, diagnostics, QStringLiteral("XY"), xy->line, point))
                        outline.points.append(point);
                }
            }
            if (!outline.points.isEmpty())
                cell.outlines.append(outline);
        }
    }
    if (cell.pins.isEmpty() && diagnostics)
        diagnostics->add(
            ParseSeverity::Skipped, ParseScope::Footprint, QStringLiteral("空 Cell 被跳过"), cell.name, cell.line);
    model.cells.append(cell);
}

void parsePart(const SectionNode& node, XpeditionHkpModel& model, ParseDiagnostics* diagnostics) {
    XpeditionPartDefinition part;
    part.number = valueOf(node);
    part.name = part.number;
    part.line = node.line;
    for (const SectionNode& item : node.children) {
        const QString keyword = upper(item.keyword);
        const QString value = valueOf(item);
        if (keyword == QStringLiteral("NAME"))
            part.name = value;
        else if (keyword == QStringLiteral("DESC"))
            part.description = value;
        else if (keyword == QStringLiteral("REFPREFIX"))
            part.referencePrefix = value;
        else if (keyword == QStringLiteral("TOPCELL"))
            part.topCell = value;
        else if (keyword == QStringLiteral("BOTTOMCELL"))
            part.bottomCell = value;
        else if (keyword == QStringLiteral("SYMBOL"))
            part.symbol = value;
        else if (keyword == QStringLiteral("PROP")) {
            const QStringList values = item.value.split(QRegularExpression(QStringLiteral("\\s*,\\s*")));
            if (values.size() >= 2)
                part.properties.insert(unquote(values.at(0)), unquote(values.at(1)));
            else if (diagnostics)
                diagnostics->add(ParseSeverity::Warning,
                                 ParseScope::Component,
                                 QStringLiteral("器件属性缺少名称或值"),
                                 part.number,
                                 item.line);
        }
    }
    if (part.topCell.isEmpty() && part.bottomCell.isEmpty() && diagnostics)
        diagnostics->add(ParseSeverity::Warning,
                         ParseScope::Component,
                         QStringLiteral("器件没有封装 Cell 关联"),
                         part.number,
                         part.line);
    model.parts.append(part);
}

}  // namespace

const XpeditionPadstackDefinition* XpeditionHkpModel::findPadstack(const QString& name) const {
    for (const auto& item : padstacks) {
        if (item.name == name)
            return &item;
    }
    return nullptr;
}

const XpeditionCellDefinition* XpeditionHkpModel::findCell(const QString& name) const {
    for (const auto& item : cells) {
        if (item.name == name)
            return &item;
    }
    return nullptr;
}

XpeditionHkpModel XpeditionHkpModelParser::parse(const QList<SectionNode>& sections,
                                                 LengthUnit unit,
                                                 ParseDiagnostics* diagnostics) {
    XpeditionHkpModel model;
    for (const SectionNode& node : sections) {
        const QString keyword = upper(node.keyword);
        if (keyword == QStringLiteral("PAD"))
            parsePad(node, unit, model, diagnostics);
        else if (keyword == QStringLiteral("HOLE"))
            parseHole(node, unit, model, diagnostics);
        else if (keyword == QStringLiteral("PADSTACK"))
            parsePadstack(node, model, diagnostics);
        else if (keyword == QStringLiteral("PACKAGE_CELL"))
            parseCell(node, unit, model, diagnostics);
        else if (keyword == QStringLiteral("NUMBER"))
            parsePart(node, model, diagnostics);
    }

    for (const auto& padstack : model.padstacks) {
        const QStringList padNames = {padstack.topPad,
                                      padstack.bottomPad,
                                      padstack.topSolderMaskPad,
                                      padstack.bottomSolderMaskPad,
                                      padstack.topSolderPastePad,
                                      padstack.bottomSolderPastePad};
        for (const QString& name : padNames) {
            if (!name.isEmpty() &&
                std::none_of(
                    model.pads.cbegin(), model.pads.cend(), [&](const auto& pad) { return pad.name == name; }) &&
                diagnostics)
                diagnostics->add(ParseSeverity::Warning,
                                 ParseScope::Footprint,
                                 QStringLiteral("Padstack 引用了不存在的 Pad：%1").arg(name),
                                 padstack.name,
                                 padstack.line);
        }
        if (!padstack.holeName.isEmpty() &&
            std::none_of(model.holes.cbegin(),
                         model.holes.cend(),
                         [&](const auto& hole) { return hole.name == padstack.holeName; }) &&
            diagnostics)
            diagnostics->add(ParseSeverity::Warning,
                             ParseScope::Footprint,
                             QStringLiteral("Padstack 引用了不存在的孔：%1").arg(padstack.holeName),
                             padstack.name,
                             padstack.line);
    }
    for (const auto& cell : model.cells) {
        for (const auto& pin : cell.pins) {
            if (!pin.padstack.isEmpty() && !model.findPadstack(pin.padstack) && diagnostics)
                diagnostics->add(ParseSeverity::Warning,
                                 ParseScope::Footprint,
                                 QStringLiteral("引脚引用了不存在的 Padstack：%1").arg(pin.padstack),
                                 cell.name,
                                 pin.line);
        }
    }
    for (const auto& part : model.parts) {
        for (const QString& cellName : {part.topCell, part.bottomCell}) {
            if (!cellName.isEmpty() && !model.findCell(cellName) && diagnostics)
                diagnostics->add(ParseSeverity::Warning,
                                 ParseScope::Component,
                                 QStringLiteral("器件引用了不存在的 Cell：%1").arg(cellName),
                                 part.number,
                                 part.line);
        }
    }
    return model;
}

}  // namespace EasyKiConverter::Parser
