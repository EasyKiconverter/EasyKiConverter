#include "ExporterEagleFootprint.h"

#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QTextStream>
#include <QXmlStreamWriter>

#include <cmath>
#include <optional>

namespace EasyKiConverter {
namespace {

QString safeName(const QString& value) {
    QString result;
    for (const QChar character : value) {
        const ushort code = character.unicode();
        if ((code >= 'A' && code <= 'Z') || (code >= 'a' && code <= 'z') || (code >= '0' && code <= '9') ||
            character == QChar('_') || character == QChar('-'))
            result.append(character);
        else
            result.append(QChar('_'));
    }
    return result.left(64);
}

QString number(double value) {
    return QString::number(value, 'f', 6);
}

QString rotation(double value, bool mirror = false) {
    return QStringLiteral("%1%2").arg(mirror ? QStringLiteral("MR") : QStringLiteral("R"), number(value));
}

std::optional<int> layerNumber(IR::LayerType layer) {
    switch (layer) {
        case IR::LayerType::TopCopper:
            return 1;
        case IR::LayerType::BottomCopper:
            return 16;
        case IR::LayerType::TopSilk:
            return 21;
        case IR::LayerType::BottomSilk:
            return 22;
        case IR::LayerType::TopMask:
            return 29;
        case IR::LayerType::BottomMask:
            return 30;
        case IR::LayerType::TopPaste:
            return 31;
        case IR::LayerType::BottomPaste:
            return 32;
        case IR::LayerType::TopAssembly:
            return 51;
        case IR::LayerType::BottomAssembly:
            return 52;
        case IR::LayerType::KeepOut:
            return 39;
        case IR::LayerType::EdgeCuts:
            return 46;
        default:
            return std::nullopt;
    }
}

bool writePackage(QXmlStreamWriter& xml, const IR::FootprintComponentIR& footprint, QStringList& diagnostics) {
    const QString name = safeName(footprint.name);
    if (name.isEmpty()) {
        diagnostics.append(QStringLiteral("Eagle: 封装名称清洗后为空"));
        return false;
    }
    for (const IR::FootprintPadIR& pad : footprint.pads) {
        if (pad.number.isEmpty()) {
            diagnostics.append(QStringLiteral("Eagle: 封装 %1 存在空焊盘编号").arg(name));
            return false;
        }
        if (!std::isfinite(pad.position.x()) || !std::isfinite(pad.position.y()) || !std::isfinite(pad.size.width()) ||
            !std::isfinite(pad.size.height())) {
            diagnostics.append(QStringLiteral("Eagle: 焊盘 %1 包含非法坐标或尺寸").arg(pad.number));
            return false;
        }
        if (pad.shape == IR::PadShape::Polygon || pad.shape == IR::PadShape::Trapezoid || pad.holeLength > 0.0) {
            diagnostics.append(QStringLiteral("Eagle: 焊盘 %1 的异形或槽孔语义当前无法无损表达").arg(pad.number));
            return false;
        }
        if (pad.isSmd() && pad.layer != IR::LayerType::TopCopper && pad.layer != IR::LayerType::BottomCopper) {
            diagnostics.append(QStringLiteral("Eagle: SMD 焊盘 %1 使用无法映射到 Eagle 铜层的图层").arg(pad.number));
            return false;
        }
        if (pad.isThroughHole() && pad.shape != IR::PadShape::Rect && pad.shape != IR::PadShape::Ellipse &&
            pad.shape != IR::PadShape::Oval) {
            diagnostics.append(QStringLiteral("Eagle: 通孔焊盘 %1 的形状无法无损表达").arg(pad.number));
            return false;
        }
    }
    for (const IR::FootprintTrackIR& track : footprint.tracks) {
        if (!layerNumber(track.layer).has_value()) {
            diagnostics.append(QStringLiteral("Eagle: 走线使用无法映射的图层"));
            return false;
        }
    }
    if (!footprint.arcs.isEmpty()) {
        diagnostics.append(QStringLiteral("Eagle: 当前 XML 封装导出尚未实现圆弧图元"));
        return false;
    }
    if (!footprint.models3d.isEmpty())
        diagnostics.append(
            QStringLiteral("Eagle: package XML 不包含三维模型关联，已跳过 %1 个模型").arg(footprint.models3d.size()));

    xml.writeStartElement(QStringLiteral("package"));
    xml.writeAttribute(QStringLiteral("name"), name);
    for (const IR::FootprintCircleIR& circle : footprint.circles) {
        const auto layer = layerNumber(circle.layer);
        if (!layer.has_value()) {
            diagnostics.append(QStringLiteral("Eagle: 圆形图元使用无法映射的图层"));
            return false;
        }
        xml.writeStartElement(QStringLiteral("circle"));
        xml.writeAttribute(QStringLiteral("x"), number(circle.center.x()));
        xml.writeAttribute(QStringLiteral("y"), number(circle.center.y()));
        xml.writeAttribute(QStringLiteral("radius"), number(circle.radius));
        xml.writeAttribute(QStringLiteral("width"), number(circle.strokeWidth));
        xml.writeAttribute(QStringLiteral("layer"), QString::number(*layer));
        xml.writeEndElement();
    }
    for (const IR::FootprintRectangleIR& rectangle : footprint.rectangles) {
        const auto layer = layerNumber(rectangle.layer);
        if (!layer.has_value()) {
            diagnostics.append(QStringLiteral("Eagle: 矩形图元使用无法映射的图层"));
            return false;
        }
        const QRectF bounds = rectangle.bounds;
        const QList<QPointF> corners = {
            bounds.topLeft(), bounds.topRight(), bounds.bottomRight(), bounds.bottomLeft(), bounds.topLeft()};
        for (int index = 0; index < corners.size() - 1; ++index) {
            xml.writeStartElement(QStringLiteral("wire"));
            xml.writeAttribute(QStringLiteral("x1"), number(corners.at(index).x()));
            xml.writeAttribute(QStringLiteral("y1"), number(corners.at(index).y()));
            xml.writeAttribute(QStringLiteral("x2"), number(corners.at(index + 1).x()));
            xml.writeAttribute(QStringLiteral("y2"), number(corners.at(index + 1).y()));
            xml.writeAttribute(QStringLiteral("width"), number(rectangle.strokeWidth));
            xml.writeAttribute(QStringLiteral("layer"), QString::number(*layer));
            xml.writeEndElement();
        }
    }
    for (const IR::FootprintTrackIR& track : footprint.tracks) {
        if (track.points.size() < 2)
            continue;
        const int layer = *layerNumber(track.layer);
        for (int index = 0; index < track.points.size() - 1; ++index) {
            const QPointF& first = track.points.at(index);
            const QPointF& second = track.points.at(index + 1);
            xml.writeStartElement(QStringLiteral("wire"));
            xml.writeAttribute(QStringLiteral("x1"), number(first.x()));
            xml.writeAttribute(QStringLiteral("y1"), number(first.y()));
            xml.writeAttribute(QStringLiteral("x2"), number(second.x()));
            xml.writeAttribute(QStringLiteral("y2"), number(second.y()));
            xml.writeAttribute(QStringLiteral("width"), number(track.width));
            xml.writeAttribute(QStringLiteral("layer"), QString::number(layer));
            xml.writeEndElement();
        }
    }
    for (const IR::FootprintRegionIR& region : footprint.regions) {
        if (region.vertices.size() < 3)
            continue;
        const auto layer = layerNumber(region.layer);
        if (!layer.has_value()) {
            diagnostics.append(QStringLiteral("Eagle: 区域使用无法映射的图层"));
            return false;
        }
        for (int index = 0; index < region.vertices.size(); ++index) {
            const QPointF& first = region.vertices.at(index);
            const QPointF& second = region.vertices.at((index + 1) % region.vertices.size());
            xml.writeStartElement(QStringLiteral("wire"));
            xml.writeAttribute(QStringLiteral("x1"), number(first.x()));
            xml.writeAttribute(QStringLiteral("y1"), number(first.y()));
            xml.writeAttribute(QStringLiteral("x2"), number(second.x()));
            xml.writeAttribute(QStringLiteral("y2"), number(second.y()));
            xml.writeAttribute(QStringLiteral("width"), QStringLiteral("0"));
            xml.writeAttribute(QStringLiteral("layer"), QString::number(*layer));
            xml.writeEndElement();
        }
    }
    for (const IR::FootprintTextIR& text : footprint.texts) {
        if (!text.isDisplayed || text.text.isEmpty())
            continue;
        const auto layer = layerNumber(text.layer);
        if (!layer.has_value()) {
            diagnostics.append(QStringLiteral("Eagle: 文本使用无法映射的图层"));
            return false;
        }
        xml.writeStartElement(QStringLiteral("text"));
        xml.writeAttribute(QStringLiteral("x"), number(text.position.x()));
        xml.writeAttribute(QStringLiteral("y"), number(text.position.y()));
        xml.writeAttribute(QStringLiteral("size"), number(text.fontSize > 0.0 ? text.fontSize : 1.0));
        xml.writeAttribute(QStringLiteral("layer"), QString::number(*layer));
        xml.writeAttribute(QStringLiteral("rot"), rotation(text.rotation, text.mirror));
        xml.writeCharacters(text.text);
        xml.writeEndElement();
    }
    for (const IR::FootprintHoleIR& hole : footprint.holes) {
        xml.writeStartElement(QStringLiteral("hole"));
        xml.writeAttribute(QStringLiteral("x"), number(hole.center.x()));
        xml.writeAttribute(QStringLiteral("y"), number(hole.center.y()));
        xml.writeAttribute(QStringLiteral("drill"), number(hole.radius * 2.0));
        xml.writeEndElement();
    }
    for (const IR::FootprintPadIR& pad : footprint.pads) {
        xml.writeStartElement(pad.isThroughHole() ? QStringLiteral("pad") : QStringLiteral("smd"));
        xml.writeAttribute(QStringLiteral("name"), pad.number);
        xml.writeAttribute(QStringLiteral("x"), number(pad.position.x()));
        xml.writeAttribute(QStringLiteral("y"), number(pad.position.y()));
        xml.writeAttribute(QStringLiteral("rot"), rotation(pad.rotation));
        if (pad.isThroughHole()) {
            xml.writeAttribute(QStringLiteral("drill"), number(pad.holeSize));
            xml.writeAttribute(QStringLiteral("diameter"), number(qMax(pad.size.width(), pad.size.height())));
            xml.writeAttribute(QStringLiteral("shape"),
                               pad.shape == IR::PadShape::Rect ? QStringLiteral("square") : QStringLiteral("round"));
        } else {
            xml.writeAttribute(QStringLiteral("dx"), number(pad.size.width()));
            xml.writeAttribute(QStringLiteral("dy"), number(pad.size.height()));
            xml.writeAttribute(QStringLiteral("layer"),
                               pad.layer == IR::LayerType::BottomCopper ? QStringLiteral("16") : QStringLiteral("1"));
            if (pad.shape == IR::PadShape::RoundRect)
                xml.writeAttribute(QStringLiteral("roundness"), QStringLiteral("100"));
            else if (pad.shape == IR::PadShape::Ellipse || pad.shape == IR::PadShape::Oval)
                xml.writeAttribute(QStringLiteral("roundness"), QStringLiteral("100"));
            else if (pad.shape != IR::PadShape::Rect)
                xml.writeAttribute(QStringLiteral("roundness"), QStringLiteral("0"));
        }
        xml.writeEndElement();
    }
    xml.writeEndElement();
    return !xml.hasError();
}

bool writeLibrary(const QList<IR::FootprintComponentIR>& footprints,
                  const QString& filePath,
                  const QString& description,
                  QStringList& diagnostics) {
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        diagnostics.append(QStringLiteral("Eagle: 无法写入文件 %1").arg(filePath));
        return false;
    }
    QXmlStreamWriter xml(&file);
    xml.setAutoFormatting(true);
    xml.writeStartDocument();
    xml.writeStartElement(QStringLiteral("eagle"));
    xml.writeAttribute(QStringLiteral("version"), QStringLiteral("9.6.2"));
    xml.writeStartElement(QStringLiteral("drawing"));
    xml.writeStartElement(QStringLiteral("settings"));
    xml.writeEmptyElement(QStringLiteral("setting"));
    xml.writeAttribute(QStringLiteral("alwaysvectorfont"), QStringLiteral("no"));
    xml.writeEndElement();
    xml.writeStartElement(QStringLiteral("layers"));
    const QList<QPair<int, QString>> layers = {{1, "Top"},
                                               {16, "Bottom"},
                                               {21, "tPlace"},
                                               {22, "bPlace"},
                                               {29, "tStop"},
                                               {30, "bStop"},
                                               {31, "tCream"},
                                               {32, "bCream"},
                                               {39, "tKeepout"},
                                               {46, "Milling"},
                                               {51, "tDocu"},
                                               {52, "bDocu"}};
    for (const auto& layer : layers) {
        xml.writeEmptyElement(QStringLiteral("layer"));
        xml.writeAttribute(QStringLiteral("number"), QString::number(layer.first));
        xml.writeAttribute(QStringLiteral("name"), layer.second);
        xml.writeAttribute(QStringLiteral("color"), QStringLiteral("4"));
        xml.writeAttribute(QStringLiteral("fill"), QStringLiteral("1"));
        xml.writeAttribute(QStringLiteral("visible"), QStringLiteral("yes"));
        xml.writeAttribute(QStringLiteral("active"), QStringLiteral("yes"));
    }
    xml.writeEndElement();
    xml.writeStartElement(QStringLiteral("library"));
    xml.writeStartElement(QStringLiteral("description"));
    xml.writeCharacters(description);
    xml.writeEndElement();
    xml.writeStartElement(QStringLiteral("packages"));
    for (const auto& footprint : footprints) {
        if (!writePackage(xml, footprint, diagnostics))
            return false;
    }
    xml.writeEndElement();
    xml.writeEndElement();
    xml.writeEndElement();
    xml.writeEndElement();
    xml.writeEndDocument();
    return !xml.hasError();
}

}  // namespace

QString ExporterEagleFootprint::libraryFileExtension() const {
    return QStringLiteral(".lbr");
}

bool ExporterEagleFootprint::isDirectoryOutput() const {
    return false;
}

bool ExporterEagleFootprint::exportFootprint(const IR::FootprintComponentIR& footprint,
                                             const QString& filePath,
                                             const QString&) {
    m_diagnostics.clear();
    return writeLibrary({footprint}, filePath, footprint.description, m_diagnostics);
}

bool ExporterEagleFootprint::exportFootprintLibrary(const QList<IR::FootprintComponentIR>& footprints,
                                                    const QString&,
                                                    const QString& filePath,
                                                    bool,
                                                    bool,
                                                    const QString& libraryDescription,
                                                    const QString&,
                                                    bool,
                                                    const QString&) {
    m_diagnostics.clear();
    if (footprints.isEmpty()) {
        m_diagnostics.append(QStringLiteral("Eagle: 没有可导出的封装"));
        return false;
    }
    QSet<QString> names;
    for (const auto& footprint : footprints) {
        const QString name = safeName(footprint.name);
        if (name.isEmpty() || names.contains(name)) {
            m_diagnostics.append(QStringLiteral("Eagle: 封装名称清洗后冲突或为空：%1").arg(footprint.name));
            return false;
        }
        names.insert(name);
    }
    return writeLibrary(footprints, filePath, libraryDescription, m_diagnostics);
}

QStringList ExporterEagleFootprint::diagnostics() const {
    return m_diagnostics;
}

}  // namespace EasyKiConverter
