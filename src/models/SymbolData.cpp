#include "SymbolData.h"

#include "SymbolDataSerializer.h"

#include <QRegularExpression>

#include <cmath>

namespace EasyKiConverter {

/**
 * @brief 初始化符号元数据和边界框。
 */
SymbolData::SymbolData() : m_info(), m_bbox() {}

/**
 * @brief 将符号数据交给统一序列化器转换为 JSON。
 * @return 符号数据 JSON 对象。
 */
QJsonObject SymbolData::toJson() const {
    return SymbolDataSerializer::toJson(*this);
}

/**
 * @brief 从 JSON 恢复符号数据并返回序列化器的解析结果。
 * @param json 符号数据 JSON 对象。
 * @return 解析成功时返回 true。
 */
bool SymbolData::fromJson(const QJsonObject& json) {
    return SymbolDataSerializer::fromJson(*this, json);
}

/**
 * @brief 通过完整诊断列表判断符号数据是否有效。
 * @return 符号数据有效时返回 true。
 */
bool SymbolData::isValid() const {
    return validationErrors().isEmpty();
}

/**
 * @brief 返回符号诊断列表中的首个错误或空字符串。
 * @return 首个校验错误；无错误时返回空字符串。
 */
QString SymbolData::validate() const {
    const QStringList errors = validationErrors();  ///< 当前符号的校验错误列表。
    return errors.isEmpty() ? QString() : errors.first();
}

/**
 * @brief 收集符号元数据、引脚、图元和多部分结构的全部校验错误。
 * @return 校验错误列表；无错误时为空。
 */
QStringList SymbolData::validationErrors() const {
    QStringList errors;  ///< 当前符号的全部校验错误。
    const auto addError = [&](const QString& message) { errors.append(message); };  ///< 追加一条校验错误。
    const auto isFinite = [](double value) { return std::isfinite(value); };  ///< 判断数值是否为有限值。
    const auto validatePointList = [&](const QList<QPointF>& points, const QString& type, int index, int minimum) {
        if (points.size() < minimum) {
            addError(QString("%1 %2 has fewer than %3 points").arg(type).arg(index).arg(minimum));
            return;
        }
        for (const QPointF& point : points) {
            if (!isFinite(point.x()) || !isFinite(point.y())) {
                addError(QString("%1 %2 contains a non-finite point").arg(type).arg(index));
                break;
            }
        }
    };
    const auto validateFlatPointString = [&](const QString& value, const QString& type, int index, int minimum) {
        const QStringList parts =  ///< 拆分后的平面坐标文本。
            value.trimmed().split(QRegularExpression(QStringLiteral("[\\s,]+")), Qt::SkipEmptyParts);
        if (parts.size() < minimum * 2 || parts.size() % 2 != 0) {
            addError(QString("%1 %2 has an invalid point list").arg(type).arg(index));
            return;
        }
        for (const QString& part : parts) {
            bool ok = false;  ///< 坐标文本是否解析成功。
            const double coordinate = part.toDouble(&ok);  ///< 当前解析出的坐标。
            if (!ok || !isFinite(coordinate)) {
                addError(QString("%1 %2 contains an invalid coordinate").arg(type).arg(index));
                break;
            }
        }
    };
    const auto validatePath = [&](const QString& value, const QString& prefix, int index) {
        const QString path = value.trimmed();  ///< 去除空白后的路径文本。
        if (path.isEmpty()) {
            addError(QString("%1Path %2 has no commands").arg(prefix).arg(index));
            return;
        }

        bool hasMoveCommand = false;  ///< 是否包含起始移动命令。
        bool hasDrawableCommand = false;  ///< 是否包含可绘制命令。
        bool hasUnsupportedCommand = false;  ///< 是否包含不支持的命令。
        bool hasInvalidSyntax = false;  ///< 是否包含无效语法。
        bool hasNonFiniteNumber = false;  ///< 是否包含非有限数值。
        const QRegularExpression commandExpression(QStringLiteral("[A-Za-z]"));  ///< SVG 命令匹配表达式。
        auto matchIterator = commandExpression.globalMatch(path);  ///< 当前路径的命令匹配迭代器。
        while (matchIterator.hasNext()) {
            const auto match = matchIterator.next();  ///< 当前命令匹配结果。
            const int position = match.capturedStart();  ///< 当前命令在路径中的位置。
            const QChar command = match.captured(0).at(0);  ///< 当前 SVG 命令字符。
            const bool isExponentMarker =  ///< 当前字母是否只是科学计数法指数标记。
                (command == QLatin1Char('e') || command == QLatin1Char('E')) && position > 0 &&
                position + 1 < path.size() && path.at(position - 1).isDigit() &&
                (path.at(position + 1).isDigit() || path.at(position + 1) == QLatin1Char('.') ||
                 path.at(position + 1) == QLatin1Char('+') || path.at(position + 1) == QLatin1Char('-'));
            if (isExponentMarker)
                continue;
            if (!QStringLiteral("MmZzLlHhVvCcSsQqTtAa").contains(command)) {
                hasUnsupportedCommand = true;
                continue;
            }
            if (command == QLatin1Char('M') || command == QLatin1Char('m'))
                hasMoveCommand = true;
            else
                hasDrawableCommand = true;
        }

        // 逐 token 校验，避免解析器静默跳过非法字符或不完整的参数组。
        const QRegularExpression tokenExpression(  ///< SVG 路径 token 匹配表达式。
            QStringLiteral(R"(\s+|,|[MmZzLlHhVvCcSsQqTtAa]|[-+]?(?:(?:\d+(?:\.\d*)?)|(?:\.\d+))(?:[eE][-+]?\d+)?)"));
        QStringList tokens;  ///< 解析出的 SVG 路径 token 列表。
        int tokenPosition = 0;  ///< 下一次 token 匹配的起始位置。
        while (tokenPosition < path.size()) {
            const auto tokenMatch = tokenExpression.match(path, tokenPosition);  ///< 当前 token 匹配结果。
            if (!tokenMatch.hasMatch() || tokenMatch.capturedStart() != tokenPosition) {
                hasInvalidSyntax = true;
                break;
            }
            const QString token = tokenMatch.captured(0);  ///< 当前路径 token。
            tokenPosition = tokenMatch.capturedEnd();
            if (!token.trimmed().isEmpty() && token != QStringLiteral(",")) {
                tokens.append(token);
                if (!token.at(0).isLetter()) {
                    bool ok = false;  ///< 数值 token 是否解析成功。
                    const double number = token.toDouble(&ok);  ///< 当前 token 的数值。
                    if (!ok)
                        hasNonFiniteNumber = true;
                    else if (!isFinite(number))
                        hasNonFiniteNumber = true;
                }
            }
        }

        const auto isCommandToken = [](const QString& token) { return token.size() == 1 && token.at(0).isLetter(); };
        const auto parameterCount = [](QChar command) {
            // 按 SVG 命令返回一组参数的固定数量，用于检查路径命令完整性。
            switch (command.toUpper().unicode()) {
                case 'M':
                case 'L':
                case 'T':
                    return 2;
                case 'H':
                case 'V':
                    return 1;
                case 'S':
                case 'Q':
                    return 4;
                case 'C':
                    return 6;
                case 'A':
                    return 7;
                default:
                    return 0;
            }
        };
        int tokenIndex = 0;  ///< 当前待处理 token 索引。
        while (!hasInvalidSyntax && tokenIndex < tokens.size()) {
            if (!isCommandToken(tokens.at(tokenIndex))) {
                hasInvalidSyntax = true;
                break;
            }
            const QChar command = tokens.at(tokenIndex++).at(0);  ///< 当前路径命令。
            const int groupSize = parameterCount(command);  ///< 当前命令所需的参数数量。
            if (command.toUpper() == QLatin1Char('Z'))
                continue;

            int groupCount = 0;  ///< 当前命令已解析的参数组数量。
            while (tokenIndex < tokens.size() && !isCommandToken(tokens.at(tokenIndex))) {
                if (tokenIndex + groupSize > tokens.size()) {
                    hasInvalidSyntax = true;
                    break;
                }
                for (int parameterIndex = 0; parameterIndex < groupSize; ++parameterIndex) {
                    if (isCommandToken(tokens.at(tokenIndex + parameterIndex))) {
                        hasInvalidSyntax = true;
                        break;
                    }
                }
                if (hasInvalidSyntax)
                    break;
                tokenIndex += groupSize;
                ++groupCount;
            }
            if (command.toUpper() == QLatin1Char('M') && groupCount > 1)
                hasDrawableCommand = true;
            if (groupCount == 0)
                hasInvalidSyntax = true;
        }

        if (hasUnsupportedCommand)
            addError(QString("%1Path %2 contains an unsupported command").arg(prefix).arg(index));
        if (hasInvalidSyntax)
            addError(QString("%1Path %2 has invalid command parameters").arg(prefix).arg(index));
        if (hasNonFiniteNumber)
            addError(QString("%1Path %2 contains a non-finite numeric parameter").arg(prefix).arg(index));
        if (!hasMoveCommand)
            addError(QString("%1Path %2 has no initial move command").arg(prefix).arg(index));
        if (!hasDrawableCommand)
            addError(QString("%1Path %2 has no drawable commands").arg(prefix).arg(index));
    };
    const auto validateText = [&](const SymbolText& text, const QString& prefix, int index) {
        if (!isFinite(text.posX) || !isFinite(text.posY) || !isFinite(text.rotation))
            addError(QString("%1Text %2 has a non-finite position or rotation").arg(prefix).arg(index));
        if (!text.text.trimmed().isEmpty() && text.visible && (!isFinite(text.textSize) || text.textSize <= 0.0))
            addError(QString("%1Text %2 has a non-positive font size").arg(prefix).arg(index));
        if (text.text.trimmed().isEmpty())
            addError(QString("%1Text %2 is empty").arg(prefix).arg(index));
        const QString anchor = text.anchor.trimmed().toLower();  ///< 标准化后的文本锚点。
        if (!anchor.isEmpty() &&
            !QStringList{QStringLiteral("start"), QStringLiteral("middle"), QStringLiteral("end")}.contains(anchor))
            addError(QString("%1Text %2 has an unsupported anchor %3").arg(prefix).arg(index).arg(text.anchor));
    };
    const auto validateImage = [&](const SymbolImage& image, const QString& prefix, int index) {
        if (!isFinite(image.posX) || !isFinite(image.posY) || !isFinite(image.width) || !isFinite(image.height) ||
            !isFinite(image.rotation) || image.width <= 0.0 || image.height <= 0.0)
            addError(QString("%1Image %2 has invalid bounds or rotation").arg(prefix).arg(index));
        if (image.source.trimmed().isEmpty() && image.fileName.trimmed().isEmpty() && image.data.isEmpty())
            addError(QString("%1Image %2 has no resource").arg(prefix).arg(index));
    };
    const auto validateGraphicOrder =
        [&](const QList<SymbolGraphicOrder>& order, const QString& prefix, const auto& countForType) {
            QStringList seen;  ///< 已出现的图元引用键。
            for (int i = 0; i < order.size(); ++i) {
                const SymbolGraphicOrder& reference = order.at(i);  ///< 当前图元顺序引用。
                const int typeCount = countForType(reference.type);  ///< 当前图元类型的实际数量。
                if (typeCount < 0) {
                    addError(QString("%1Graphic order %2 has unknown type %3").arg(prefix).arg(i).arg(reference.type));
                    continue;
                }
                if (reference.index < 0 || reference.index >= typeCount) {
                    addError(QString("%1Graphic order %2 has out-of-range %3 index %4")
                                 .arg(prefix)
                                 .arg(i)
                                 .arg(reference.type)
                                 .arg(reference.index));
                    continue;
                }
                const QString key = reference.type + QChar(':') + QString::number(reference.index);  ///< 引用去重键。
                if (seen.contains(key))
                    addError(QString("%1Graphic order %2 duplicates %3 index %4")
                                 .arg(prefix)
                                 .arg(i)
                                 .arg(reference.type)
                                 .arg(reference.index));
                else
                    seen.append(key);
            }
        };
    const auto validatePart = [&](const SymbolPart& part, int partIndex) {
        const QString prefix = QStringLiteral("Part %1 ").arg(partIndex);  ///< 当前部件诊断前缀。
        if (!isFinite(part.originX) || !isFinite(part.originY))
            addError(QString("%1origin contains a non-finite value").arg(prefix));
        const auto validatePin = [&](const SymbolPin& pin, int pinIndex) {
            if (!isFinite(pin.settings.posX) || !isFinite(pin.settings.posY))
                addError(QString("%1Pin %2 has a non-finite position").arg(prefix).arg(pinIndex));
            if (pin.name.isDisplayed && (!isFinite(pin.name.posX) || !isFinite(pin.name.posY) ||
                                         !isFinite(pin.name.fontSize) || pin.name.fontSize <= 0.0))
                addError(QString("%1Pin %2 has invalid name geometry").arg(prefix).arg(pinIndex));
            if (pin.number.isDisplayed && (!isFinite(pin.number.posX) || !isFinite(pin.number.posY) ||
                                           !isFinite(pin.number.fontSize) || pin.number.fontSize <= 0.0))
                addError(QString("%1Pin %2 has invalid number geometry").arg(prefix).arg(pinIndex));
        };
        for (int i = 0; i < part.pins.size(); ++i) {
            if (part.pins[i].settings.spicePinNumber.trimmed().isEmpty())
                addError(QString("%1Pin %2 has empty number").arg(prefix).arg(i));
            validatePin(part.pins[i], i);
        }
        for (int i = 0; i < part.rectangles.size(); ++i) {
            const SymbolRectangle& rectangle = part.rectangles[i];  ///< 当前部件矩形图元。
            if (!isFinite(rectangle.width) || !isFinite(rectangle.height) || rectangle.width <= 0.0 ||
                rectangle.height <= 0.0)
                addError(QString("%1Rectangle %2 has a non-positive size").arg(prefix).arg(i));
            if (!isFinite(rectangle.rx) || !isFinite(rectangle.ry) || rectangle.rx < 0.0 || rectangle.ry < 0.0)
                addError(QString("%1Rectangle %2 has a negative or non-finite corner radius").arg(prefix).arg(i));
        }
        for (int i = 0; i < part.circles.size(); ++i)
            if (!isFinite(part.circles[i].radius) || part.circles[i].radius <= 0.0)
                addError(QString("%1Circle %2 has a non-positive radius").arg(prefix).arg(i));
        for (int i = 0; i < part.ellipses.size(); ++i)
            if (!isFinite(part.ellipses[i].radiusX) || !isFinite(part.ellipses[i].radiusY) ||
                part.ellipses[i].radiusX <= 0.0 || part.ellipses[i].radiusY <= 0.0)
                addError(QString("%1Ellipse %2 has a non-positive radius").arg(prefix).arg(i));
        for (int i = 0; i < part.arcs.size(); ++i)
            validatePointList(part.arcs[i].path, prefix + QStringLiteral("Arc"), i, 3);
        for (int i = 0; i < part.polylines.size(); ++i)
            validateFlatPointString(part.polylines[i].points, prefix + QStringLiteral("Polyline"), i, 2);
        for (int i = 0; i < part.polygons.size(); ++i)
            validateFlatPointString(part.polygons[i].points, prefix + QStringLiteral("Polygon"), i, 3);
        for (int i = 0; i < part.paths.size(); ++i)
            validatePath(part.paths[i].paths, prefix, i);
        for (int i = 0; i < part.texts.size(); ++i)
            validateText(part.texts[i], prefix, i);
        for (int i = 0; i < part.images.size(); ++i)
            validateImage(part.images[i], prefix, i);
        validateGraphicOrder(part.graphicOrder, prefix, [&](const QString& type) -> int {
            if (type == QStringLiteral("P"))
                return part.pins.size();
            if (type == QStringLiteral("R"))
                return part.rectangles.size();
            if (type == QStringLiteral("C"))
                return part.circles.size();
            if (type == QStringLiteral("A"))
                return part.arcs.size();
            if (type == QStringLiteral("E"))
                return part.ellipses.size();
            if (type == QStringLiteral("PL"))
                return part.polylines.size();
            if (type == QStringLiteral("PG"))
                return part.polygons.size();
            if (type == QStringLiteral("PT"))
                return part.paths.size();
            if (type == QStringLiteral("T"))
                return part.texts.size();
            if (type == QStringLiteral("I"))
                return part.images.size();
            return -1;
        });
    };

    if (m_info.name.trimmed().isEmpty())
        addError(QStringLiteral("Symbol name is empty"));
    if (!isFinite(m_bbox.x) || !isFinite(m_bbox.y) || !isFinite(m_bbox.width) || !isFinite(m_bbox.height))
        addError(QStringLiteral("Symbol bbox contains a non-finite value"));
    else if (m_bbox.width <= 0.0 || m_bbox.height <= 0.0)
        addError(QStringLiteral("Symbol bbox is empty"));

    const auto validatePin = [&](const SymbolPin& pin, int pinIndex) {
        if (!isFinite(pin.settings.posX) || !isFinite(pin.settings.posY))
            addError(QString("Pin %1 has a non-finite position").arg(pinIndex));
        if (pin.name.isDisplayed && (!isFinite(pin.name.posX) || !isFinite(pin.name.posY) ||
                                     !isFinite(pin.name.fontSize) || pin.name.fontSize <= 0.0))
            addError(QString("Pin %1 has invalid name geometry").arg(pinIndex));
        if (pin.number.isDisplayed && (!isFinite(pin.number.posX) || !isFinite(pin.number.posY) ||
                                       !isFinite(pin.number.fontSize) || pin.number.fontSize <= 0.0))
            addError(QString("Pin %1 has invalid number geometry").arg(pinIndex));
    };
    for (int i = 0; i < m_pins.size(); ++i) {
        if (m_pins[i].settings.spicePinNumber.trimmed().isEmpty())
            addError(QString("Pin %1 has empty number").arg(i));
        validatePin(m_pins[i], i);
    }
    for (int i = 0; i < m_rectangles.size(); ++i) {
        const SymbolRectangle& rectangle = m_rectangles[i];  ///< 当前符号矩形图元。
        if (!isFinite(rectangle.width) || !isFinite(rectangle.height) || rectangle.width <= 0.0 ||
            rectangle.height <= 0.0)
            addError(QString("Rectangle %1 has a non-positive size").arg(i));
        if (!isFinite(rectangle.rx) || !isFinite(rectangle.ry) || rectangle.rx < 0.0 || rectangle.ry < 0.0)
            addError(QString("Rectangle %1 has a negative or non-finite corner radius").arg(i));
    }
    for (int i = 0; i < m_circles.size(); ++i) {
        if (!isFinite(m_circles[i].radius) || m_circles[i].radius <= 0.0)
            addError(QString("Circle %1 has a non-positive radius").arg(i));
    }
    for (int i = 0; i < m_ellipses.size(); ++i) {
        if (!isFinite(m_ellipses[i].radiusX) || !isFinite(m_ellipses[i].radiusY) || m_ellipses[i].radiusX <= 0.0 ||
            m_ellipses[i].radiusY <= 0.0)
            addError(QString("Ellipse %1 has a non-positive radius").arg(i));
    }
    for (int i = 0; i < m_arcs.size(); ++i)
        validatePointList(m_arcs[i].path, QStringLiteral("Arc"), i, 3);
    for (int i = 0; i < m_polylines.size(); ++i)
        validateFlatPointString(m_polylines[i].points, QStringLiteral("Polyline"), i, 2);
    for (int i = 0; i < m_polygons.size(); ++i)
        validateFlatPointString(m_polygons[i].points, QStringLiteral("Polygon"), i, 3);
    for (int i = 0; i < m_paths.size(); ++i)
        validatePath(m_paths[i].paths, QString(), i);
    for (int i = 0; i < m_texts.size(); ++i)
        validateText(m_texts[i], QString(), i);
    for (int i = 0; i < m_images.size(); ++i)
        validateImage(m_images[i], QString(), i);
    validateGraphicOrder(m_graphicOrder, QString(), [&](const QString& type) -> int {
        if (type == QStringLiteral("P"))
            return m_pins.size();
        if (type == QStringLiteral("R"))
            return m_rectangles.size();
        if (type == QStringLiteral("C"))
            return m_circles.size();
        if (type == QStringLiteral("A"))
            return m_arcs.size();
        if (type == QStringLiteral("E"))
            return m_ellipses.size();
        if (type == QStringLiteral("PL"))
            return m_polylines.size();
        if (type == QStringLiteral("PG"))
            return m_polygons.size();
        if (type == QStringLiteral("PT"))
            return m_paths.size();
        if (type == QStringLiteral("T"))
            return m_texts.size();
        if (type == QStringLiteral("I"))
            return m_images.size();
        return -1;
    });
    for (int i = 0; i < m_parts.size(); ++i)
        validatePart(m_parts[i], i);

    return errors;
}

/**
 * @brief 清空符号元数据、所有图元、绘制顺序和多部分数据。
 */
void SymbolData::clear() {
    m_info = SymbolInfo();
    m_bbox = SymbolBBox();
    m_pins.clear();
    m_rectangles.clear();
    m_circles.clear();
    m_arcs.clear();
    m_ellipses.clear();
    m_polylines.clear();
    m_polygons.clear();
    m_paths.clear();
    m_images.clear();
    m_texts.clear();
    m_graphicOrder.clear();
    m_parts.clear();
}

}  // namespace EasyKiConverter
