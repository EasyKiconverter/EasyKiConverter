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

constexpr double MM_TO_MIL = 39.37007874015748;  ///< 毫米到 mil 的换算系数。

/**
 * @brief 将封装名称清洗为 PADS PCB Decal 可接受的文件和记录标识符。
 * @param value 待清洗的封装名称。
 * @return 仅包含安全字符的 PADS 名称。
 */
QString safeDecalName(const QString& value) {
    QString result;  ///< 清洗后的 PADS 名称。
    result.reserve(value.size());
    for (const QChar character : value) {
        const ushort code = character.unicode();  ///< 当前字符的 Unicode 编码。
        if ((code >= 'A' && code <= 'Z') || (code >= 'a' && code <= 'z') || (code >= '0' && code <= '9') ||
            character == QChar('_') || character == QChar('-'))
            result.append(character);
        else
            result.append(QChar('_'));
    }
    return result.left(40);
}

/**
 * @brief 将毫米尺寸转换为 PADS 的 mil 文本。
 * @param value 待转换的毫米数值。
 * @return 格式化后的 mil 文本。
 */
QString number(double value) {
    return QString::number(value * MM_TO_MIL, 'f', 6);
}

/**
 * @brief 序列化 PADS 支持的角度字段。
 * @param value 待序列化的角度值。
 * @return 格式化后的角度文本。
 */
QString angle(double value) {
    return QString::number(value, 'f', 3);
}

/**
 * @brief 检查文本是否仅包含 ASCII 字符。
 * @param value 待检查的文本。
 * @return 文本全部为 ASCII 字符时返回 true。
 */
bool isAsciiText(const QString& value) {
    const QByteArray bytes = value.toUtf8();  ///< UTF-8 编码后的字节序列。
    for (const unsigned char byte : bytes) {
        if (byte >= 0x80)
            return false;
    }
    return true;
}

/**
 * @brief 将 IR 焊盘形状映射为 PADS Pad Stack 形状代码。
 * @param pad 待转换的统一 IR 焊盘。
 * @param diagnostics 用于追加不支持原因的诊断列表。
 * @return PADS 形状代码；无法表达时返回空字符串。
 */
QString padShape(const IR::FootprintPadIR& pad, QStringList& diagnostics) {
    // 只返回规范中有明确制造语义的形状代码。
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

/**
 * @brief 写入 PADS PCB Decal 图元头部。
 * @param stream 目标文本流。
 * @param type PADS 图元类型。
 * @param count 图元顶点或记录数量。
 * @param width 图元线宽。
 * @param layer PADS 层编号。
 * @return 写入成功时返回 true。
 */
bool writePieceHeader(QTextStream& stream, const QString& type, int count, double width, int layer) {
    stream << type << ' ' << count << ' ' << number(width) << ' ' << layer << " -1\n";
    return stream.status() == QTextStream::Ok;
}

/**
 * @brief 将单个封装 IR 写入一个 PADS PCB Decal 文件。
 * @param footprint 待导出的封装。
 * @param path 输出文件路径。
 * @param diagnostics 用于追加导出诊断的列表。
 * @return 导出成功时返回 true。
 */
bool writeFootprint(const IR::FootprintComponentIR& footprint, const QString& path, QStringList& diagnostics) {
    const QString name = safeDecalName(footprint.name);  ///< 清洗后的封装名称。
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
        diagnostics.append(QStringLiteral("PADS: PCB Decal 不包含三维模型关联，将由独立三维阶段输出 %1 个模型")
                               .arg(footprint.models3d.size()));

    QList<QString> padShapes;  ///< 与封装焊盘顺序对应的 PADS 形状代码。
    for (const IR::FootprintPadIR& pad : footprint.pads) {
        const QString shape = padShape(pad, diagnostics);  ///< 当前焊盘的 PADS 形状代码。
        if (shape.isEmpty())
            return false;
        padShapes.append(shape);
    }

    // PADS 图元数量。
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
    // 可见文本数量。
    const int textCount = std::count_if(footprint.texts.cbegin(), footprint.texts.cend(), [](const auto& text) {
        return text.isDisplayed && !text.text.isEmpty();
    });
    const QDateTime timestamp = QDateTime::currentDateTimeUtc();  ///< 输出文件时间戳。

    QFile file(path);  ///< PADS Decal 输出文件。
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        diagnostics.append(QStringLiteral("PADS: 无法写入文件 %1").arg(path));
        return false;
    }
    QTextStream stream(&file);  ///< PADS ASCII 输出流。
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
        const QRectF bounds = rectangle.bounds;  ///< 矩形边界。
        const QList<QPointF> corners = {///< 闭合矩形的顶点序列。
                                        bounds.topLeft(),
                                        bounds.topRight(),
                                        bounds.bottomRight(),
                                        bounds.bottomLeft(),
                                        bounds.topLeft()};
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
        const QPointF& first = region.vertices.first();  ///< 区域的首个顶点。
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
        const IR::FootprintTextIR& text = footprint.texts.at(index);  ///< 当前待写入的文本。
        if (!text.isDisplayed || text.text.isEmpty())
            continue;
        stream << number(text.position.x()) << ' ' << number(text.position.y()) << ' ' << angle(text.rotation)
               << " 0 1 " << number(text.fontSize > 0.0 ? text.fontSize : 1.0) << ' '
               << number(text.strokeWidth > 0.0 ? text.strokeWidth : 0.15) << " 0 0 0 0 Default Font\n";
        QString textValue = text.text;  ///< 已规范化换行的文本内容。
        stream << textValue.replace(QChar('\n'), QChar(' ')) << '\n';
    }
    for (int index = 0; index < footprint.pads.size(); ++index) {
        const IR::FootprintPadIR& pad = footprint.pads.at(index);  ///< 当前待写入的焊盘。
        const QString pin = pad.number;  ///< PADS 焊盘编号。
        stream << "T " << number(pad.position.x()) << ' ' << number(pad.position.y()) << ' ' << number(pad.position.x())
               << ' ' << number(pad.position.y()) << ' ' << pin << '\n';
        const QString plated = pad.isPlated ? QStringLiteral("P") : QStringLiteral("N");  ///< 镀层标记。
        const double drill = pad.isThroughHole() ? pad.holeSize : 0.0;  ///< 钻孔直径。
        const int layerCount = pad.isThroughHole() ? 2 : 1;  ///< PADS 层数量。
        stream << "PAD " << pin << ' ' << layerCount << ' ' << plated << ' ' << number(drill);
        if (pad.isThroughHole() && pad.holeLength > 0.0)
            stream << " 0 " << number(pad.holeLength) << " 0";
        stream << '\n';
        const QString shape = padShapes.at(index);  ///< 当前焊盘的 PADS 形状代码。
        const double width = qMax(pad.size.width(), pad.size.height());  ///< PADS 主尺寸。
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

/**
 * @brief 返回 PADS PCB Decal 目录后缀。
 * @return PADS 封装库目录后缀。
 */
QString ExporterPadsFootprint::libraryFileExtension() const {
    return QStringLiteral("_PADS");
}

/**
 * @brief 判断 PADS 封装输出是否采用目录形式。
 * @return PADS 输出始终为目录时返回 true。
 */
bool ExporterPadsFootprint::isDirectoryOutput() const {
    return true;
}

/**
 * @brief 导出单个 PADS PCB Decal，并保留目标格式诊断。
 * @param footprint 待导出的封装。
 * @param filePath 输出文件路径。
 * @param model3DPath 未使用的三维模型路径参数。
 * @return 导出成功时返回 true。
 */
bool ExporterPadsFootprint::exportFootprint(const IR::FootprintComponentIR& footprint,
                                            const QString& filePath,
                                            const QString&) {
    m_diagnostics.clear();
    return writeFootprint(footprint, filePath, m_diagnostics);
}

/**
 * @brief 导出目录形式的 PADS PCB Decal 库并检查名称冲突。
 * @param footprints 待导出的封装列表。
 * @param libName 目标库名称。
 * @param filePath 输出目录路径。
 * @param preferWrl 是否优先使用 WRL 模型。
 * @param exportStep 是否导出 STEP 模型。
 * @param libraryDescription 库描述文本。
 * @param libraryKeywords 库关键词文本。
 * @param useAbsolutePaths 是否使用绝对模型路径。
 * @param model3DBaseDir 三维模型基础目录。
 * @return 全部封装导出成功时返回 true。
 */
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
    QSet<QString> names;  ///< 已占用的清洗后封装名称集合。
    for (const IR::FootprintComponentIR& footprint : footprints) {
        const QString name = safeDecalName(footprint.name);  ///< 当前封装的输出名称。
        if (name.isEmpty() || names.contains(name)) {
            m_diagnostics.append(QStringLiteral("PADS: 封装名称清洗后冲突或为空：%1").arg(footprint.name));
            return false;
        }
        names.insert(name);
        const QString path = QDir(filePath).filePath(name + QStringLiteral(".d"));  ///< 当前封装的输出文件路径。
        if (!writeFootprint(footprint, path, m_diagnostics))
            return false;
    }
    return true;
}

/**
 * @brief 返回最近一次封装导出的诊断列表。
 * @return 最近一次导出的诊断信息列表。
 */
QStringList ExporterPadsFootprint::diagnostics() const {
    return m_diagnostics;
}

}  // namespace EasyKiConverter
