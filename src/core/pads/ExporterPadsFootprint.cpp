#include "ExporterPadsFootprint.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QSet>
#include <QTextStream>
#include <QtMath>

#include <algorithm>
#include <cmath>

namespace EasyKiConverter {
namespace {

constexpr double MM_TO_MIL = 39.37007874015748;

QString safeDecalName(const QString& value) {
    QString result;
    result.reserve(value.size());
    for (const QChar character : value) {
        const ushort code = character.unicode();
        if ((code >= 'A' && code <= 'Z') || (code >= 'a' && code <= 'z') || (code >= '0' && code <= '9') ||
            character == QChar('_') || character == QChar('-'))
            result.append(character);
        else
            result.append(QChar('_'));
    }
    return result.left(40);
}

QString number(double value) {
    return QString::number(value * MM_TO_MIL, 'f', 6);
}

QString angle(double value) {
    return QString::number(value, 'f', 3);
}

bool isAsciiText(const QString& value) {
    const QByteArray bytes = value.toUtf8();
    for (const unsigned char byte : bytes) {
        if (byte >= 0x80)
            return false;
    }
    return true;
}

QString padShape(const IR::FootprintPadIR& pad, QStringList& diagnostics) {
    switch (pad.shape) {
        case IR::PadShape::Ellipse:
            return qFuzzyCompare(pad.size.width(), pad.size.height()) ? QStringLiteral("R") : QStringLiteral("OF");
        case IR::PadShape::Rect:
            return qFuzzyCompare(pad.size.width(), pad.size.height()) ? QStringLiteral("S") : QStringLiteral("RF");
        case IR::PadShape::Oval:
            return QStringLiteral("OF");
        case IR::PadShape::RoundRect:
        case IR::PadShape::Polygon:
        case IR::PadShape::Trapezoid:
            diagnostics.append(
                QStringLiteral("PADS: 焊盘 %1 的形状无法由当前 ASCII Pad Stack 语义无损表达").arg(pad.number));
            return {};
    }
    diagnostics.append(QStringLiteral("PADS: 焊盘 %1 使用未知形状").arg(pad.number));
    return {};
}

bool writePieceHeader(QTextStream& stream, const QString& type, int count, double width, int layer) {
    stream << type << ' ' << count << ' ' << number(width) << ' ' << layer << " -1\n";
    return stream.status() == QTextStream::Ok;
}

bool writeFootprint(const IR::FootprintComponentIR& footprint, const QString& path, QStringList& diagnostics) {
    const QString name = safeDecalName(footprint.name);
    if (name.isEmpty()) {
        diagnostics.append(QStringLiteral("PADS: 封装名称清洗后为空"));
        return false;
    }

    if (!footprint.holes.isEmpty()) {
        diagnostics.append(
            QStringLiteral("PADS: 封装 %1 包含独立安装孔，当前 Decal 导出无法保持其机械孔语义").arg(name));
        return false;
    }
    if (!footprint.arcs.isEmpty()) {
        diagnostics.append(QStringLiteral("PADS: 封装 %1 包含圆弧，当前 Decal 导出尚未实现圆弧图元").arg(name));
        return false;
    }
    for (const IR::FootprintPadIR& pad : footprint.pads) {
        if (pad.number.isEmpty() || !isAsciiText(pad.number)) {
            diagnostics.append(QStringLiteral("PADS: 焊盘编号必须为非空 ASCII 文本：%1").arg(pad.number));
            return false;
        }
        if (!std::isfinite(pad.position.x()) || !std::isfinite(pad.position.y()) || !std::isfinite(pad.size.width()) ||
            !std::isfinite(pad.size.height())) {
            diagnostics.append(QStringLiteral("PADS: 焊盘 %1 包含非法坐标或尺寸").arg(pad.number));
            return false;
        }
    }
    for (const IR::FootprintTextIR& text : footprint.texts) {
        if (text.isDisplayed && !isAsciiText(text.text)) {
            diagnostics.append(
                QStringLiteral("PADS: 文本包含非 ASCII 字符，当前 Decal 文本编码未定义：%1").arg(text.text));
            return false;
        }
    }
    if (!footprint.models3d.isEmpty())
        diagnostics.append(
            QStringLiteral("PADS: 当前输出为 PCB Decal，已跳过 %1 个三维模型关联").arg(footprint.models3d.size()));

    QList<QString> padShapes;
    for (const IR::FootprintPadIR& pad : footprint.pads) {
        const QString shape = padShape(pad, diagnostics);
        if (shape.isEmpty())
            return false;
        padShapes.append(shape);
    }

    const int pieces = footprint.circles.size() + footprint.rectangles.size() +
                       std::count_if(footprint.tracks.cbegin(),
                                     footprint.tracks.cend(),
                                     [](const auto& track) { return track.points.size() >= 2; }) +
                       std::count_if(footprint.regions.cbegin(),
                                     footprint.regions.cend(),
                                     [](const auto& region) { return region.vertices.size() >= 3; }) +
                       std::count_if(footprint.outlines.cbegin(), footprint.outlines.cend(), [](const auto& outline) {
                           return outline.points.size() >= 2;
                       });
    const int textCount = std::count_if(footprint.texts.cbegin(), footprint.texts.cend(), [](const auto& text) {
        return text.isDisplayed && !text.text.isEmpty();
    });
    const QDateTime timestamp = QDateTime::currentDateTimeUtc();

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        diagnostics.append(QStringLiteral("PADS: 无法写入文件 %1").arg(path));
        return false;
    }
    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    stream << name << " I 0 0 0 0 " << pieces << ' ' << textCount << ' ' << footprint.pads.size() << ' '
           << footprint.pads.size() << " 0\n";
    stream << "TIMESTAMP " << timestamp.toString(QStringLiteral("yyyy.MM.dd.hh.mm.ss")) << "\n";

    for (const IR::FootprintCircleIR& circle : footprint.circles) {
        if (!writePieceHeader(stream, QStringLiteral("CIRCLE"), 2, circle.strokeWidth, 0))
            return false;
        stream << number(circle.center.x() - circle.radius) << ' ' << number(circle.center.y() - circle.radius) << '\n';
        stream << number(circle.center.x() + circle.radius) << ' ' << number(circle.center.y() + circle.radius) << '\n';
    }
    for (const IR::FootprintRectangleIR& rectangle : footprint.rectangles) {
        if (!writePieceHeader(stream, QStringLiteral("CLOSED"), 5, rectangle.strokeWidth, 0))
            return false;
        const QRectF bounds = rectangle.bounds;
        const QList<QPointF> corners = {
            bounds.topLeft(), bounds.topRight(), bounds.bottomRight(), bounds.bottomLeft(), bounds.topLeft()};
        for (const QPointF& point : corners)
            stream << number(point.x()) << ' ' << number(point.y()) << '\n';
    }
    for (const IR::FootprintTrackIR& track : footprint.tracks) {
        if (track.points.size() < 2)
            continue;
        if (!writePieceHeader(stream, QStringLiteral("OPEN"), track.points.size(), track.width, 0))
            return false;
        for (const QPointF& point : track.points)
            stream << number(point.x()) << ' ' << number(point.y()) << '\n';
    }
    for (const IR::FootprintRegionIR& region : footprint.regions) {
        if (region.vertices.size() < 3)
            continue;
        if (!writePieceHeader(stream, QStringLiteral("CLOSED"), region.vertices.size() + 1, 0.0, 0))
            return false;
        for (const QPointF& point : region.vertices)
            stream << number(point.x()) << ' ' << number(point.y()) << '\n';
        const QPointF& first = region.vertices.first();
        stream << number(first.x()) << ' ' << number(first.y()) << '\n';
    }
    for (const IR::FootprintOutlineIR& outline : footprint.outlines) {
        if (outline.points.size() < 2)
            continue;
        if (!writePieceHeader(stream, QStringLiteral("OPEN"), outline.points.size(), outline.strokeWidth, 0))
            return false;
        for (const QPointF& point : outline.points)
            stream << number(point.x()) << ' ' << number(point.y()) << '\n';
    }
    for (int index = 0; index < footprint.texts.size(); ++index) {
        const IR::FootprintTextIR& text = footprint.texts.at(index);
        if (!text.isDisplayed || text.text.isEmpty())
            continue;
        stream << number(text.position.x()) << ' ' << number(text.position.y()) << ' ' << angle(text.rotation)
               << " 0 1 " << number(text.fontSize > 0.0 ? text.fontSize : 1.0) << ' '
               << number(text.strokeWidth > 0.0 ? text.strokeWidth : 0.15) << " 0 0 0 0 Default Font\n";
        QString textValue = text.text;
        stream << textValue.replace(QChar('\n'), QChar(' ')) << '\n';
    }
    for (int index = 0; index < footprint.pads.size(); ++index) {
        const IR::FootprintPadIR& pad = footprint.pads.at(index);
        const QString pin = pad.number;
        stream << "T " << number(pad.position.x()) << ' ' << number(pad.position.y()) << ' ' << number(pad.position.x())
               << ' ' << number(pad.position.y()) << ' ' << pin << '\n';
        const QString plated = pad.isPlated ? QStringLiteral("P") : QStringLiteral("N");
        const double drill = pad.isThroughHole() ? pad.holeSize : 0.0;
        const int layerCount = pad.isThroughHole() ? 2 : 1;
        stream << "PAD " << pin << ' ' << layerCount << ' ' << plated << ' ' << number(drill);
        if (pad.isThroughHole() && pad.holeLength > 0.0)
            stream << " 0 " << number(pad.holeLength) << " 0";
        stream << '\n';
        const QString shape = padShapes.at(index);
        const double width = qMax(pad.size.width(), pad.size.height());
        stream << (pad.isThroughHole() ? "-2 " : "-2 ") << number(width) << ' ' << shape;
        if (shape == QStringLiteral("OF") || shape == QStringLiteral("RF"))
            stream << " 0 " << angle(pad.rotation) << ' ' << number(pad.size.height()) << " 0";
        stream << '\n';
        if (pad.isThroughHole()) {
            stream << "-0 " << number(width) << ' ' << shape;
            if (shape == QStringLiteral("OF") || shape == QStringLiteral("RF"))
                stream << " 0 " << angle(pad.rotation) << ' ' << number(pad.size.height()) << " 0";
            stream << '\n';
        }
    }
    stream.flush();
    if (stream.status() != QTextStream::Ok) {
        diagnostics.append(QStringLiteral("PADS: 写入文件时发生 I/O 错误 %1").arg(path));
        return false;
    }
    return true;
}

}  // namespace

QString ExporterPadsFootprint::libraryFileExtension() const {
    return QStringLiteral("_PADS");
}

bool ExporterPadsFootprint::isDirectoryOutput() const {
    return true;
}

bool ExporterPadsFootprint::exportFootprint(const IR::FootprintComponentIR& footprint,
                                            const QString& filePath,
                                            const QString&) {
    m_diagnostics.clear();
    return writeFootprint(footprint, filePath, m_diagnostics);
}

bool ExporterPadsFootprint::exportFootprintLibrary(const QList<IR::FootprintComponentIR>& footprints,
                                                   const QString&,
                                                   const QString& filePath,
                                                   bool,
                                                   bool,
                                                   const QString&,
                                                   const QString&,
                                                   bool,
                                                   const QString&) {
    m_diagnostics.clear();
    if (footprints.isEmpty()) {
        m_diagnostics.append(QStringLiteral("PADS: 没有可导出的封装"));
        return false;
    }
    if (!QDir().mkpath(filePath)) {
        m_diagnostics.append(QStringLiteral("PADS: 无法创建输出目录 %1").arg(filePath));
        return false;
    }
    QSet<QString> names;
    for (const IR::FootprintComponentIR& footprint : footprints) {
        const QString name = safeDecalName(footprint.name);
        if (name.isEmpty() || names.contains(name)) {
            m_diagnostics.append(QStringLiteral("PADS: 封装名称清洗后冲突或为空：%1").arg(footprint.name));
            return false;
        }
        names.insert(name);
        const QString path = QDir(filePath).filePath(name + QStringLiteral(".d"));
        if (!writeFootprint(footprint, path, m_diagnostics))
            return false;
    }
    return true;
}

QStringList ExporterPadsFootprint::diagnostics() const {
    return m_diagnostics;
}

}  // namespace EasyKiConverter
