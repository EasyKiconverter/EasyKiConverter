#include "XpeditionSymbolModel.h"

#include <QRegularExpression>

namespace EasyKiConverter::Parser {

namespace {

/** @brief 按空白拆分 Xpedition 符号命令字段。 */
QStringList fields(const QString& line) {
    return line.trimmed().split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
}

/** @brief 解析符号数值并将非法输入交给统一诊断器。 */
double number(const QString& text, ParseDiagnostics* diagnostics, const QString& field, int line) {
    return StrictNumberParser::parseDouble(text, diagnostics, field, line);
}

/** @brief 从字段列表读取坐标，并在字段不足时记录错误。 */
QPointF point(const QStringList& values,
              int xIndex,
              int yIndex,
              ParseDiagnostics* diagnostics,
              const QString& field,
              int line) {
    if (xIndex < 0 || yIndex < 0 || xIndex >= values.size() || yIndex >= values.size()) {
        if (diagnostics)
            diagnostics->add(
                ParseSeverity::Error, ParseScope::Field, QStringLiteral("符号坐标字段数量不足"), field, line);
        return {};
    }
    return {number(values.at(xIndex), diagnostics, field, line), number(values.at(yIndex), diagnostics, field, line)};
}

/** @brief 解析整数属性并返回本次解析是否没有新增错误。 */
bool integer(const QString& text, ParseDiagnostics* diagnostics, const QString& field, int line, int& result) {
    const int before = diagnostics ? diagnostics->items().size() : 0;
    result = static_cast<int>(StrictNumberParser::parseInteger(text, diagnostics, field, line));
    return !diagnostics || diagnostics->items().size() == before;
}

/** @brief 从命令字段中提取第一个键值属性。 */
QString propertyValue(const QStringList& values) {
    const int separator = values.indexOf(QRegularExpression(QStringLiteral("^[^=]+=.*$")));
    if (separator < 0)
        return {};
    return values.mid(separator).join(QStringLiteral(" "));
}

/** @brief 为字段不足的符号命令记录结构化错误。 */
void diagnoseMalformed(ParseDiagnostics* diagnostics, const QString& command, int line) {
    if (diagnostics)
        diagnostics->add(ParseSeverity::Error,
                         ParseScope::Symbol,
                         QStringLiteral("Xpedition 符号命令字段不完整：%1").arg(command),
                         command,
                         line);
}

}  // namespace

/** @brief 判断解析结果是否包含可识别的符号内容。 */
bool XpeditionSymbolDocument::isRecognized() const {
    return model.version > 0 || !model.pins.isEmpty() || !model.polylines.isEmpty() || !model.rectangles.isEmpty() ||
           !model.circles.isEmpty() || !model.arcs.isEmpty() || !model.texts.isEmpty();
}

/** @brief 逐行解析 Xpedition V54 符号文本并保留不支持字段的诊断。 */
XpeditionSymbolDocument XpeditionSymbolParser::parse(const QString& content, const QString& filePath) {
    XpeditionSymbolDocument document;
    document.diagnostics.setFilePath(filePath);
    XpeditionSymbolPin* currentPin = nullptr;
    int currentPart = 0;
    bool closeNextPolyline = false;

    const QStringList lines = content.split(QRegularExpression(QStringLiteral("\\r?\\n")));
    for (int index = 0; index < lines.size(); ++index) {
        const int line = index + 1;
        const QString raw = lines.at(index).trimmed();
        if (raw.isEmpty())
            continue;
        if (raw.startsWith(QChar('+'))) {
            if (closeNextPolyline && !document.model.polylines.isEmpty())
                document.model.polylines.last().closed = true;
            closeNextPolyline = false;
            continue;
        }
        if (raw.startsWith(QChar('|')))
            continue;

        const QString command = raw.left(1);
        const QStringList values = fields(raw.mid(1));
        if (command == QStringLiteral("V")) {
            if (values.isEmpty() ||
                !integer(values.first(), &document.diagnostics, QStringLiteral("V"), line, document.model.version))
                diagnoseMalformed(&document.diagnostics, command, line);
        } else if (command == QStringLiteral("K")) {
            if (values.size() >= 2)
                document.model.name = values.mid(1).join(QStringLiteral("_"));
            else
                diagnoseMalformed(&document.diagnostics, command, line);
        } else if (command == QStringLiteral("F")) {
            if (!values.isEmpty())
                document.model.footprint = values.first();
        } else if (command == QStringLiteral("Y")) {
            if (values.isEmpty() ||
                !integer(values.first(), &document.diagnostics, QStringLiteral("Y"), line, document.model.symbolType))
                diagnoseMalformed(&document.diagnostics, command, line);
        } else if (command == QStringLiteral("D")) {
            if (values.size() < 4)
                diagnoseMalformed(&document.diagnostics, command, line);
            else
                document.model.bounds = QRectF(point(values, 0, 1, &document.diagnostics, QStringLiteral("D"), line),
                                               point(values, 2, 3, &document.diagnostics, QStringLiteral("D"), line));
        } else if (command == QStringLiteral("Z")) {
            if (values.isEmpty())
                diagnoseMalformed(&document.diagnostics, command, line);
            else
                document.model.zoomLevel = number(values.first(), &document.diagnostics, QStringLiteral("Z"), line);
        } else if (command == QStringLiteral("U")) {
            const QString property = propertyValue(values);
            const int equal = property.indexOf(QChar('='));
            if (equal > 0) {
                const QString key = property.left(equal);
                const QString value = property.mid(equal + 1);
                document.model.properties.insert(key, value);
                if (key == QStringLiteral("PARTS"))
                    integer(value, &document.diagnostics, key, line, document.model.partCount);
                else if (key == QStringLiteral("PART"))
                    integer(value, &document.diagnostics, key, line, currentPart);
                else if (key == QStringLiteral("HETERO"))
                    document.model.heterogeneousParts =
                        value.split(QRegularExpression(QStringLiteral("[,()]")), Qt::SkipEmptyParts);
            }
        } else if (command == QStringLiteral("P")) {
            if (values.size() < 8) {
                diagnoseMalformed(&document.diagnostics, command, line);
                currentPin = nullptr;
                continue;
            }
            XpeditionSymbolPin pin;
            pin.id = static_cast<int>(number(values.at(0), &document.diagnostics, QStringLiteral("P.ID"), line));
            pin.start = point(values, 1, 2, &document.diagnostics, QStringLiteral("P"), line);
            pin.end = point(values, 3, 4, &document.diagnostics, QStringLiteral("P"), line);
            integer(values.at(6), &document.diagnostics, QStringLiteral("P.ROTATION"), line, pin.rotation);
            int inverted = 0;
            integer(values.at(7), &document.diagnostics, QStringLiteral("P.INVERTED"), line, inverted);
            pin.inverted = inverted != 0;
            pin.partIndex = currentPart;
            document.model.pins.append(pin);
            currentPin = &document.model.pins.last();
        } else if (command == QStringLiteral("A")) {
            const QString property = propertyValue(values);
            const int equal = property.indexOf(QChar('='));
            if (currentPin != nullptr && equal > 0) {
                const QString key = property.left(equal);
                const QString value = property.mid(equal + 1);
                if (key == QStringLiteral("PINTYPE"))
                    currentPin->pinType = value;
                else if (key == QStringLiteral("#"))
                    currentPin->numbers = value.split(QRegularExpression(QStringLiteral(",")), Qt::SkipEmptyParts);
                else if (key == QStringLiteral("PART"))
                    integer(value, &document.diagnostics, key, line, currentPart);
            }
            if (currentPin != nullptr && values.size() > 5 && property.startsWith(QStringLiteral("#=")))
                currentPin->numbersVisible = values.at(5) != QStringLiteral("0");
        } else if (command == QStringLiteral("L")) {
            if (currentPin == nullptr || values.size() < 7) {
                diagnoseMalformed(&document.diagnostics, command, line);
                continue;
            }
            currentPin->namePosition = point(values, 0, 1, &document.diagnostics, QStringLiteral("L"), line);
            currentPin->nameSize = number(values.at(2), &document.diagnostics, QStringLiteral("L.SIZE"), line);
            currentPin->nameRotation = number(values.at(3), &document.diagnostics, QStringLiteral("L.ROTATION"), line);
            const int nameStart = values.size() >= 9 ? 8 : 6;
            const int visibilityIndex = values.size() >= 9 ? 6 : 5;
            currentPin->nameVisible = values.at(visibilityIndex) != QStringLiteral("0");
            currentPin->name = values.mid(nameStart).join(QStringLiteral(" "));
        } else if (command == QStringLiteral("l")) {
            if (values.size() < 5) {
                diagnoseMalformed(&document.diagnostics, command, line);
                continue;
            }
            int count = 0;
            integer(values.first(), &document.diagnostics, QStringLiteral("l.COUNT"), line, count);
            if (values.size() < 1 + count * 2) {
                diagnoseMalformed(&document.diagnostics, command, line);
                continue;
            }
            XpeditionSymbolPolyline polyline;
            polyline.partIndex = currentPart;
            for (int valueIndex = 1; valueIndex < 1 + count * 2; valueIndex += 2)
                polyline.points.append(
                    point(values, valueIndex, valueIndex + 1, &document.diagnostics, QStringLiteral("l"), line));
            document.model.polylines.append(polyline);
            closeNextPolyline = count >= 3;
        } else if (command == QStringLiteral("b")) {
            if (values.size() < 4) {
                diagnoseMalformed(&document.diagnostics, command, line);
                continue;
            }
            document.model.rectangles.append({point(values, 0, 1, &document.diagnostics, QStringLiteral("b"), line),
                                              point(values, 2, 3, &document.diagnostics, QStringLiteral("b"), line),
                                              currentPart});
        } else if (command == QStringLiteral("c")) {
            if (values.size() < 3) {
                diagnoseMalformed(&document.diagnostics, command, line);
                continue;
            }
            document.model.circles.append(
                {point(values, 0, 1, &document.diagnostics, QStringLiteral("c"), line),
                 number(values.at(2), &document.diagnostics, QStringLiteral("c.RADIUS"), line),
                 currentPart});
        } else if (command == QStringLiteral("a")) {
            if (values.size() < 6) {
                diagnoseMalformed(&document.diagnostics, command, line);
                continue;
            }
            document.model.arcs.append({point(values, 0, 1, &document.diagnostics, QStringLiteral("a"), line),
                                        point(values, 2, 3, &document.diagnostics, QStringLiteral("a"), line),
                                        point(values, 4, 5, &document.diagnostics, QStringLiteral("a"), line),
                                        currentPart});
        } else if (command == QStringLiteral("T")) {
            if (values.size() < 6) {
                diagnoseMalformed(&document.diagnostics, command, line);
                continue;
            }
            document.model.texts.append(
                {point(values, 0, 1, &document.diagnostics, QStringLiteral("T"), line),
                 number(values.at(2), &document.diagnostics, QStringLiteral("T.SIZE"), line),
                 number(values.at(3), &document.diagnostics, QStringLiteral("T.ROTATION"), line),
                 static_cast<int>(number(values.at(4), &document.diagnostics, QStringLiteral("T.ORIGIN"), line)),
                 values.mid(5).join(QStringLiteral(" ")),
                 currentPart});
        } else if (command == QStringLiteral("E")) {
            currentPin = nullptr;
        } else {
            document.diagnostics.add(ParseSeverity::Skipped,
                                     ParseScope::Symbol,
                                     QStringLiteral("跳过不支持的 Xpedition 符号命令：%1").arg(command),
                                     command,
                                     line);
        }
    }
    if (!document.isRecognized())
        document.diagnostics.add(ParseSeverity::Error, ParseScope::File, QStringLiteral("未识别的 Xpedition 符号文件"));
    return document;
}

}  // namespace EasyKiConverter::Parser
