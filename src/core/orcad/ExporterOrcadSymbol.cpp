#include "ExporterOrcadSymbol.h"

#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QSet>
#include <QXmlStreamWriter>

#include <cmath>

namespace EasyKiConverter {

namespace {

constexpr double kMilPerMm = 1.0 / 0.0254;

/** @brief 将毫米坐标转换为 OrCAD XML 样本使用的整数 mil。 */
QString mil(double millimeters) {
    return QString::number(qRound64(millimeters * kMilPerMm));
}

/** @brief 判断点和长度是否为可序列化的有限数值。 */
bool finitePoint(const QPointF& point) {
    return std::isfinite(point.x()) && std::isfinite(point.y());
}

/** @brief 将统一符号引脚电气类型映射为 Capture XML type 值。 */
int orcadPinType(IR::PinElectricalType type) {
    // Capture XML 使用固定整数表示电气类型，未知类型按无源引脚处理并由调用方保留原始诊断。
    switch (type) {
        case IR::PinElectricalType::Input:
            return 0;
        case IR::PinElectricalType::Output:
            return 1;
        case IR::PinElectricalType::Bidirectional:
            return 2;
        case IR::PinElectricalType::OpenCollector:
            return 3;
        case IR::PinElectricalType::Passive:
            return 4;
        case IR::PinElectricalType::Power:
            return 5;
        case IR::PinElectricalType::OpenEmitter:
            return 6;
        case IR::PinElectricalType::Unspecified:
            return 4;
    }
    return 4;
}

/** @brief 返回 OrCAD XML 样本使用的引脚方向标志。 */
QPair<int, int> pinVector(IR::PinDirection direction, double length) {
    // Capture 的 hot point 位于引脚末端，因此这里把统一 IR 的方向和长度转换为 mil 偏移。
    switch (direction) {
        case IR::PinDirection::Left:
            return {-qRound64(length * kMilPerMm), 0};
        case IR::PinDirection::Up:
            return {0, qRound64(length * kMilPerMm)};
        case IR::PinDirection::Down:
            return {0, -qRound64(length * kMilPerMm)};
        case IR::PinDirection::Right:
            return {qRound64(length * kMilPerMm), 0};
    }
    return {0, 0};
}

/** @brief 将统一符号名称约束为一个安全的 Capture XML 定义名。 */
QString definitionName(const QString& name) {
    return name.trimmed();
}

/** @brief 计算符号图形和引脚端点的边界框。 */
QRectF symbolBounds(const IR::SymbolComponentIR& symbol) {
    QRectF bounds;
    bool initialized = false;
    const auto include = [&](const QPointF& point) {
        if (!finitePoint(point))
            return;
        if (!initialized) {
            bounds = QRectF(point, QSizeF(0, 0));
            initialized = true;
        } else {
            bounds = bounds.united(QRectF(point, QSizeF(0, 0)));
        }
    };
    for (const auto& rectangle : symbol.rectangles) {
        include({rectangle.x0, rectangle.y0});
        include({rectangle.x1, rectangle.y1});
    }
    for (const auto& pin : symbol.pins) {
        include(pin.position);
        const auto vector = pinVector(pin.direction, pin.length);
        include({pin.position.x() + vector.first / kMilPerMm, pin.position.y() + vector.second / kMilPerMm});
    }
    for (const auto& polyline : symbol.polylines)
        for (const QPointF& point : polyline.points)
            include(point);
    for (const auto& polygon : symbol.polygons)
        for (const QPointF& point : polygon.points)
            include(point);
    return initialized ? bounds : QRectF(-1.27, -1.27, 2.54, 2.54);
}

/** @brief 写入 Capture XML 的最小默认字体定义。 */
void writeDefaultValues(QXmlStreamWriter& xml) {
    xml.writeStartElement(QStringLiteral("DefaultValues"));
    xml.writeEmptyElement(QStringLiteral("Defn"));
    xml.writeStartElement(QStringLiteral("DefaultFont"));
    xml.writeEmptyElement(QStringLiteral("Defn"));
    xml.writeAttribute(QStringLiteral("escapement"), QStringLiteral("0"));
    xml.writeAttribute(QStringLiteral("height"), QStringLiteral("-9"));
    xml.writeAttribute(QStringLiteral("index"), QStringLiteral("0"));
    xml.writeAttribute(QStringLiteral("italic"), QStringLiteral("0"));
    xml.writeAttribute(QStringLiteral("name"), QStringLiteral("Arial"));
    xml.writeAttribute(QStringLiteral("orientation"), QStringLiteral("0"));
    xml.writeAttribute(QStringLiteral("weight"), QStringLiteral("400"));
    xml.writeAttribute(QStringLiteral("width"), QStringLiteral("4"));
    xml.writeEndElement();
    xml.writeStartElement(QStringLiteral("DefaultPartFieldMapping"));
    xml.writeEmptyElement(QStringLiteral("Defn"));
    xml.writeAttribute(QStringLiteral("index"), QStringLiteral("8"));
    xml.writeAttribute(QStringLiteral("val"), QStringLiteral("PCB Footprint"));
    xml.writeEndElement();
    xml.writeEndElement();
}

/** @brief 写入 Capture XML 的一个符号显示属性。 */
void writeDisplayProperty(QXmlStreamWriter& xml, const QString& name, qint64 x, qint64 y) {
    xml.writeStartElement(QStringLiteral("SymbolDisplayProp"));
    xml.writeEmptyElement(QStringLiteral("Defn"));
    xml.writeAttribute(QStringLiteral("locX"), QString::number(x));
    xml.writeAttribute(QStringLiteral("locY"), QString::number(y));
    xml.writeAttribute(QStringLiteral("name"), name);
    xml.writeAttribute(QStringLiteral("rotation"), QStringLiteral("0"));
    xml.writeAttribute(QStringLiteral("textJustification"), QStringLiteral("1"));
    xml.writeStartElement(QStringLiteral("PropFont"));
    xml.writeEmptyElement(QStringLiteral("Defn"));
    xml.writeAttribute(QStringLiteral("escapement"), QStringLiteral("0"));
    xml.writeAttribute(QStringLiteral("height"), QStringLiteral("-9"));
    xml.writeAttribute(QStringLiteral("index"), QStringLiteral("0"));
    xml.writeAttribute(QStringLiteral("italic"), QStringLiteral("0"));
    xml.writeAttribute(QStringLiteral("name"), QStringLiteral("Arial"));
    xml.writeAttribute(QStringLiteral("orientation"), QStringLiteral("0"));
    xml.writeAttribute(QStringLiteral("weight"), QStringLiteral("400"));
    xml.writeAttribute(QStringLiteral("width"), QStringLiteral("4"));
    xml.writeEndElement();
    xml.writeStartElement(QStringLiteral("PropColor"));
    xml.writeEmptyElement(QStringLiteral("Defn"));
    xml.writeAttribute(QStringLiteral("val"), QStringLiteral("48"));
    xml.writeEndElement();
    xml.writeStartElement(QStringLiteral("PropDispType"));
    xml.writeEmptyElement(QStringLiteral("Defn"));
    xml.writeAttribute(QStringLiteral("ValueIfValueExist"), QStringLiteral("0"));
    xml.writeAttribute(QStringLiteral("val"), QStringLiteral("1"));
    xml.writeEndElement();
    xml.writeEndElement();
}

/** @brief 写入一个矩形的四条 Capture XML 线段。 */
void writeRectangle(QXmlStreamWriter& xml, const IR::SymbolRectangleIR& rectangle) {
    const auto line = [&](double x1, double y1, double x2, double y2) {
        xml.writeStartElement(QStringLiteral("Line"));
        xml.writeEmptyElement(QStringLiteral("Defn"));
        xml.writeAttribute(QStringLiteral("lineStyle"), QStringLiteral("0"));
        xml.writeAttribute(QStringLiteral("lineWidth"), QStringLiteral("0"));
        xml.writeAttribute(QStringLiteral("x1"), mil(x1));
        xml.writeAttribute(QStringLiteral("x2"), mil(x2));
        xml.writeAttribute(QStringLiteral("y1"), mil(y1));
        xml.writeAttribute(QStringLiteral("y2"), mil(y2));
        xml.writeEndElement();
    };
    line(rectangle.x0, rectangle.y0, rectangle.x1, rectangle.y0);
    line(rectangle.x1, rectangle.y0, rectangle.x1, rectangle.y1);
    line(rectangle.x1, rectangle.y1, rectangle.x0, rectangle.y1);
    line(rectangle.x0, rectangle.y1, rectangle.x0, rectangle.y0);
}

/** @brief 写入一个 Capture XML 符号引脚及其物理编号。 */
void writePin(QXmlStreamWriter& xml, const IR::SymbolPinIR& pin, int position) {
    const auto vector = pinVector(pin.direction, pin.length);
    const qint64 startX = qRound64(pin.position.x() * kMilPerMm);
    const qint64 startY = qRound64(pin.position.y() * kMilPerMm);
    const qint64 hotX = startX + vector.first;
    const qint64 hotY = startY + vector.second;
    xml.writeStartElement(QStringLiteral("SymbolPinScalar"));
    xml.writeEmptyElement(QStringLiteral("Defn"));
    xml.writeAttribute(QStringLiteral("hotptX"), QString::number(hotX));
    xml.writeAttribute(QStringLiteral("hotptY"), QString::number(hotY));
    xml.writeAttribute(QStringLiteral("name"), pin.name);
    xml.writeAttribute(QStringLiteral("position"), QString::number(position));
    xml.writeAttribute(QStringLiteral("startX"), QString::number(startX));
    xml.writeAttribute(QStringLiteral("startY"), QString::number(startY));
    xml.writeAttribute(QStringLiteral("type"), QString::number(orcadPinType(pin.electricalType)));
    xml.writeAttribute(QStringLiteral("visible"), pin.display.showName ? QStringLiteral("1") : QStringLiteral("0"));
    xml.writeEndElement();
    const auto flag = [&](const QString& name, bool value) {
        xml.writeStartElement(name);
        xml.writeEmptyElement(QStringLiteral("Defn"));
        xml.writeAttribute(QStringLiteral("val"), value ? QStringLiteral("1") : QStringLiteral("0"));
        xml.writeEndElement();
    };
    flag(QStringLiteral("IsLong"), false);
    flag(QStringLiteral("IsClock"), pin.style.clock || pin.hasClock);
    flag(QStringLiteral("IsDot"), pin.style.inverted || pin.hasDot);
    flag(QStringLiteral("IsLeftPointing"), pin.direction == IR::PinDirection::Left);
    flag(QStringLiteral("IsRightPointing"), pin.direction == IR::PinDirection::Right);
    flag(QStringLiteral("IsNetStyle"), false);
    flag(QStringLiteral("IsNoConnect"), pin.style.decoration == IR::PinDecoration::NoConnect);
    flag(QStringLiteral("IsGlobal"), false);
    flag(QStringLiteral("IsNumberVisible"), pin.display.showDesignator);
}

}  // namespace

/** @brief 返回 OrCAD Capture XML 符号库的文件扩展名。 */
QString ExporterOrcadSymbol::libraryFileExtension() const {
    return QStringLiteral(".xml");
}

/** @brief 将单个符号包装为一个 OrCAD Capture XML 库。 */
bool ExporterOrcadSymbol::exportSymbol(const IR::SymbolComponentIR& symbol, const QString& filePath) {
    return exportSymbolLibrary({symbol}, symbol.name, filePath, false, false);
}

/** @brief 校验并写入 OrCAD Capture XML 符号库。 */
bool ExporterOrcadSymbol::exportSymbolLibrary(const QList<IR::SymbolComponentIR>& symbols,
                                              const QString& libName,
                                              const QString& filePath,
                                              bool appendMode,
                                              bool updateMode,
                                              const QString& libraryDescription) {
    m_diagnostics.clear();
    // XML writer 没有安全的原生 OLB 合并语义，任何追加或更新请求都必须显式拒绝。
    if (appendMode || updateMode) {
        m_diagnostics.append(
            QStringLiteral("OrCAD Capture XML 当前只支持完整重写，不支持追加或更新：%1").arg(filePath));
        return false;
    }
    if (symbols.isEmpty()) {
        m_diagnostics.append(QStringLiteral("OrCAD Capture XML 没有可导出的符号"));
        return false;
    }

    QSet<QString> names;
    for (const auto& symbol : symbols) {
        const QString name = definitionName(symbol.name);
        if (name.isEmpty()) {
            m_diagnostics.append(QStringLiteral("OrCAD Capture XML 符号名称为空"));
            return false;
        }
        const QString key = name.toCaseFolded();
        if (names.contains(key)) {
            m_diagnostics.append(QStringLiteral("OrCAD Capture XML 符号名称冲突：%1").arg(name));
            return false;
        }
        names.insert(key);
        for (const auto& pin : symbol.pins) {
            if (!finitePoint(pin.position) || !std::isfinite(pin.length) || pin.length < 0.0 || pin.name.isEmpty() ||
                pin.designator.isEmpty()) {
                m_diagnostics.append(
                    QStringLiteral("OrCAD Capture XML 符号 %1 存在非法引脚字段：%2").arg(name, pin.name));
                return false;
            }
        }
        if (!symbol.ellipses.isEmpty() || !symbol.arcs.isEmpty() || !symbol.pies.isEmpty() ||
            !symbol.ellipticalArcs.isEmpty() || !symbol.paths.isEmpty() || !symbol.beziers.isEmpty() ||
            !symbol.textFrames.isEmpty() || !symbol.images.isEmpty() || !symbol.circles.isEmpty() ||
            !symbol.texts.isEmpty()) {
            m_diagnostics.append(
                QStringLiteral("OrCAD Capture XML 符号 %1 含当前 writer 未实现的图元，已拒绝静默丢失").arg(name));
            return false;
        }
        for (const auto& polyline : symbol.polylines) {
            if (polyline.points.size() < 2) {
                m_diagnostics.append(QStringLiteral("OrCAD Capture XML 符号 %1 的折线点数不足").arg(name));
                return false;
            }
            m_diagnostics.append(QStringLiteral("OrCAD Capture XML 符号 %1 的折线当前降级为线段").arg(name));
        }
        if (!symbol.polygons.isEmpty()) {
            m_diagnostics.append(QStringLiteral("OrCAD Capture XML 暂不写入多边形填充，符号 %1 已拒绝导出").arg(name));
            return false;
        }
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        m_diagnostics.append(QStringLiteral("OrCAD Capture XML 无法写入文件：%1").arg(filePath));
        return false;
    }
    QXmlStreamWriter xml(&file);
    xml.setAutoFormatting(true);
    xml.writeStartDocument(QStringLiteral("1.0"), true);
    xml.writeStartElement(QStringLiteral("Lib"));
    xml.writeAttribute(QStringLiteral("xmlns:xsd"), QStringLiteral("http://www.w3.org/2001/XMLSchema"));
    xml.writeAttribute(QStringLiteral("xmlns:xsi"), QStringLiteral("http://www.w3.org/2001/XMLSchema-instance"));
    xml.writeAttribute(QStringLiteral("xsi:noNamespaceSchemaLocation"), QStringLiteral("olb.xsd"));
    xml.writeStartElement(QStringLiteral("Defn"));
    xml.writeAttribute(QStringLiteral("name"), libraryDescription.isEmpty() ? libName : libraryDescription);
    xml.writeEndElement();
    writeDefaultValues(xml);

    for (const auto& symbol : symbols) {
        const QString name = definitionName(symbol.name);
        const QRectF bounds = symbolBounds(symbol);
        xml.writeStartElement(QStringLiteral("Package"));
        xml.writeStartElement(QStringLiteral("Defn"));
        xml.writeAttribute(QStringLiteral("alphabeticNumbering"), QStringLiteral("1"));
        xml.writeAttribute(QStringLiteral("isHomogeneous"),
                           symbol.partCount > 1 ? QStringLiteral("0") : QStringLiteral("1"));
        xml.writeAttribute(QStringLiteral("name"), name);
        xml.writeAttribute(QStringLiteral("pcbFootprint"), symbol.footprintName);
        xml.writeAttribute(QStringLiteral("pcbLib"), QString());
        xml.writeAttribute(QStringLiteral("refdesPrefix"), symbol.designatorPrefix);
        xml.writeEndElement();
        xml.writeStartElement(QStringLiteral("LibPart"));
        xml.writeStartElement(QStringLiteral("Defn"));
        xml.writeAttribute(QStringLiteral("CellName"), name);
        xml.writeEndElement();
        xml.writeStartElement(QStringLiteral("NormalView"));
        xml.writeStartElement(QStringLiteral("Defn"));
        xml.writeAttribute(QStringLiteral("suffix"), QStringLiteral(".Normal"));
        xml.writeEndElement();
        writeDisplayProperty(xml,
                             QStringLiteral("Part Reference"),
                             qRound64(bounds.center().x() * kMilPerMm),
                             qRound64((bounds.bottom() + 2.54) * kMilPerMm));
        writeDisplayProperty(xml,
                             QStringLiteral("Value"),
                             qRound64(bounds.center().x() * kMilPerMm),
                             qRound64((bounds.top() - 2.54) * kMilPerMm));
        xml.writeStartElement(QStringLiteral("SymbolBBox"));
        xml.writeEmptyElement(QStringLiteral("Defn"));
        xml.writeAttribute(QStringLiteral("x1"), mil(bounds.left()));
        xml.writeAttribute(QStringLiteral("x2"), mil(bounds.right()));
        xml.writeAttribute(QStringLiteral("y1"), mil(bounds.top()));
        xml.writeAttribute(QStringLiteral("y2"), mil(bounds.bottom()));
        xml.writeEndElement();
        xml.writeStartElement(QStringLiteral("IsPinNumbersVisible"));
        xml.writeEmptyElement(QStringLiteral("Defn"));
        xml.writeAttribute(QStringLiteral("val"), QStringLiteral("1"));
        xml.writeEndElement();
        xml.writeStartElement(QStringLiteral("IsPinNamesVisible"));
        xml.writeEmptyElement(QStringLiteral("Defn"));
        xml.writeAttribute(QStringLiteral("val"), QStringLiteral("1"));
        xml.writeEndElement();
        xml.writeStartElement(QStringLiteral("PartValue"));
        xml.writeEmptyElement(QStringLiteral("Defn"));
        xml.writeAttribute(QStringLiteral("name"), name);
        xml.writeEndElement();
        xml.writeStartElement(QStringLiteral("Reference"));
        xml.writeEmptyElement(QStringLiteral("Defn"));
        xml.writeAttribute(QStringLiteral("name"), symbol.designatorPrefix);
        xml.writeEndElement();
        for (const auto& rectangle : symbol.rectangles)
            writeRectangle(xml, rectangle);
        for (const auto& polyline : symbol.polylines) {
            for (int i = 1; i < polyline.points.size(); ++i) {
                const QPointF& first = polyline.points.at(i - 1);
                const QPointF& second = polyline.points.at(i);
                xml.writeStartElement(QStringLiteral("Line"));
                xml.writeEmptyElement(QStringLiteral("Defn"));
                xml.writeAttribute(QStringLiteral("lineStyle"), QStringLiteral("0"));
                xml.writeAttribute(QStringLiteral("lineWidth"), QStringLiteral("0"));
                xml.writeAttribute(QStringLiteral("x1"), mil(first.x()));
                xml.writeAttribute(QStringLiteral("x2"), mil(second.x()));
                xml.writeAttribute(QStringLiteral("y1"), mil(first.y()));
                xml.writeAttribute(QStringLiteral("y2"), mil(second.y()));
                xml.writeEndElement();
            }
        }
        for (int i = 0; i < symbol.pins.size(); ++i)
            writePin(xml, symbol.pins.at(i), i);
        xml.writeEndElement();
        xml.writeStartElement(QStringLiteral("PhysicalPart"));
        xml.writeEmptyElement(QStringLiteral("Defn"));
        for (int i = 0; i < symbol.pins.size(); ++i) {
            xml.writeStartElement(QStringLiteral("PinNumber"));
            xml.writeEmptyElement(QStringLiteral("Defn"));
            xml.writeAttribute(QStringLiteral("number"), symbol.pins.at(i).designator);
            xml.writeAttribute(QStringLiteral("position"), QString::number(i));
            xml.writeEndElement();
        }
        xml.writeEndElement();
        xml.writeEndElement();
        xml.writeEndElement();
    }
    xml.writeEndElement();
    xml.writeEndDocument();
    file.close();
    if (xml.hasError()) {
        m_diagnostics.append(QStringLiteral("OrCAD Capture XML 写入过程中发生错误：%1").arg(filePath));
        return false;
    }
    return true;
}

/** @brief 返回导出过程中收集的诊断信息。 */
QStringList ExporterOrcadSymbol::diagnostics() const {
    return m_diagnostics;
}

}  // namespace EasyKiConverter
