#include "ExporterPcadFootprint.h"

#include <QFile>
#include <QHash>
#include <QSet>
#include <QTextStream>

#include <cmath>
#include <optional>

namespace EasyKiConverter {
namespace {

constexpr double kMillimetersPerMil = 0.0254;  ///< 毫米到 mil 的换算系数。

/**
 * @brief 按 P-CAD ASCII 字符串规则转义并包裹字段。
 * @param value 原始字段值。
 * @return 转义后的带引号字段。
 */
QString quote(const QString& value) {
    QString escaped = value;  // 可修改的字段副本。
    escaped.replace(QChar('\\'), QStringLiteral("\\\\"));
    escaped.replace(QChar('"'), QStringLiteral("\\\""));
    return QStringLiteral("\"%1\"").arg(escaped);
}

/**
 * @brief 将统一 IR 的毫米尺寸转换为 P-CAD 使用的 mil 文本。
 * @param value 毫米尺寸。
 * @return mil 文本。
 */
QString number(double value) {
    return QString::number(value / kMillimetersPerMil, 'f', 6);
}

/**
 * @brief 将名称清洗为 P-CAD 可接受的 ASCII 标识符。
 * @param value 原始名称。
 * @return 清洗后的名称。
 */
QString safeName(const QString& value) {
    QString result;  // 清洗后的名称。
    for (const QChar character : value) {  // 逐字符处理原始名称。
        const ushort code = character.unicode();  // 当前字符编码。
        if ((code >= 'A' && code <= 'Z') || (code >= 'a' && code <= 'z') || (code >= '0' && code <= '9') ||
            character == QChar('_') || character == QChar('-'))
            result.append(character);
        else
            result.append(QChar('_'));
    }
    return result.left(64);
}

/**
 * @brief 检查字段是否可以安全写入当前 P-CAD ASCII 编码。
 * @param value 待检查字段。
 * @return 字段仅包含 ASCII 字符时返回 true。
 */
bool isAscii(const QString& value) {
    for (const unsigned char byte : value.toUtf8()) {
        if (byte >= 0x80)
            return false;
    }
    return true;
}

/**
 * @brief 将 IR 图层映射到 P-CAD 的默认层编号。
 * @param layer IR 图层。
 * @return P-CAD 层编号；无法表达时返回空值。
 */
std::optional<int> layerNumber(IR::LayerType layer) {
    // 这些编号来自 P-CAD ASCII 的默认层映射，而不是 KiCad 层编号。
    switch (layer) {
        case IR::LayerType::TopCopper:
            return 1;
        case IR::LayerType::BottomCopper:
            return 2;
        case IR::LayerType::EdgeCuts:
            return 3;
        case IR::LayerType::TopMask:
            return 4;
        case IR::LayerType::BottomMask:
            return 5;
        case IR::LayerType::TopSilk:
            return 6;
        case IR::LayerType::BottomSilk:
            return 7;
        case IR::LayerType::TopPaste:
            return 8;
        case IR::LayerType::BottomPaste:
            return 9;
        case IR::LayerType::TopAssembly:
            return 10;
        case IR::LayerType::BottomAssembly:
            return 11;
        default:
            return std::nullopt;
    }
}

/**
 * @brief 将 IR 焊盘形状映射为 P-CAD Pad Style 类型。
 * @param pad 待映射焊盘。
 * @param diagnostics 诊断信息输出列表。
 * @return P-CAD Pad Style 类型文本；无法表达时返回空字符串。
 */
QString padShape(const IR::FootprintPadIR& pad, QStringList& diagnostics) {
    // 仅接受 P-CAD Pad Style 能表达且不会改变制造语义的形状。
    switch (pad.shape) {
        case IR::PadShape::Ellipse:
            return QStringLiteral("Ellipse");
        case IR::PadShape::Oval:
            return QStringLiteral("Oval");
        case IR::PadShape::Rect:
            return QStringLiteral("Rect");
        case IR::PadShape::RoundRect:
            return QStringLiteral("RndRect");
        case IR::PadShape::Polygon:
        case IR::PadShape::Trapezoid:
            diagnostics.append(QStringLiteral("P-CAD: 焊盘 %1 的异形几何当前无法无损写入 Pad Style").arg(pad.number));
            return {};
    }
    diagnostics.append(QStringLiteral("P-CAD: 焊盘 %1 使用未知形状").arg(pad.number));
    return {};
}

/**
 * @brief 生成包含制造语义的稳定 Pad Style 去重键。
 * @param pad 焊盘定义。
 * @param shape 已映射的焊盘形状。
 * @return 稳定去重键。
 */
QString padKey(const IR::FootprintPadIR& pad, const QString& shape) {
    return QStringLiteral("%1|%2|%3|%4|%5|%6|%7")
        .arg(shape)
        .arg(QString::number(pad.size.width(), 'g', 17))
        .arg(QString::number(pad.size.height(), 'g', 17))
        .arg(QString::number(pad.holeSize, 'g', 17))
        .arg(QString::number(pad.holeLength, 'g', 17))
        .arg(pad.isPlated ? QStringLiteral("1") : QStringLiteral("0"))
        .arg(static_cast<int>(pad.layer));
}

/**
 * @brief 写入一个 P-CAD Pad Style 的几何定义。
 * @param stream P-CAD 文本流。
 * @param pad 焊盘定义。
 * @param shape P-CAD 形状文本。
 * @param layer P-CAD 层编号。
 */
void writePadShape(QTextStream& stream, const IR::FootprintPadIR& pad, const QString& shape, int layer) {
    stream << "          (padShape\n"
           << "            (layerNumRef " << layer << ")\n"
           << "            (padShapeType " << shape << ")\n"
           << "            (shapeWidth " << number(pad.size.width()) << ")\n"
           << "            (shapeHeight " << number(pad.size.height()) << ")\n"
           << "          )\n";
}

/**
 * @brief 在写入前检查封装字段和当前格式的表达能力。
 * @param footprint 封装组件 IR。
 * @param diagnostics 诊断信息输出列表。
 * @return 封装可导出时返回 true。
 */
bool validateFootprint(const IR::FootprintComponentIR& footprint, QStringList& diagnostics) {
    if (safeName(footprint.name).isEmpty()) {
        diagnostics.append(QStringLiteral("P-CAD: 封装名称清洗后为空"));
        return false;
    }
    if (!isAscii(footprint.name)) {
        diagnostics.append(QStringLiteral("P-CAD: 封装名称必须是 ASCII，无法安全编码：%1").arg(footprint.name));
        return false;
    }
    if (!footprint.holes.isEmpty()) {
        diagnostics.append(
            QStringLiteral("P-CAD: 封装 %1 含独立安装孔，当前 Pattern 输出无法表达无编号机械孔").arg(footprint.name));
        return false;
    }
    if (!footprint.models3d.isEmpty())
        diagnostics.append(QStringLiteral("P-CAD: Pattern ASCII 不包含三维模型关联，将由独立三维阶段输出 %1 个模型")
                               .arg(footprint.models3d.size()));
    for (const IR::FootprintPadIR& pad : footprint.pads) {
        if (pad.number.isEmpty() || !isAscii(pad.number)) {
            diagnostics.append(QStringLiteral("P-CAD: 焊盘编号必须为非空 ASCII 文本：%1").arg(pad.number));
            return false;
        }
        if (!std::isfinite(pad.position.x()) || !std::isfinite(pad.position.y()) || !std::isfinite(pad.size.width()) ||
            !std::isfinite(pad.size.height()) || !std::isfinite(pad.holeSize) || !std::isfinite(pad.holeLength)) {
            diagnostics.append(QStringLiteral("P-CAD: 焊盘 %1 包含非法坐标、尺寸或孔参数").arg(pad.number));
            return false;
        }
        if (pad.holeLength > 0.0) {
            diagnostics.append(
                QStringLiteral("P-CAD: 焊盘 %1 的槽孔当前没有可靠的 ASCII Pad Style 映射").arg(pad.number));
            return false;
        }
        if (pad.isSmd() && pad.layer != IR::LayerType::TopCopper && pad.layer != IR::LayerType::BottomCopper) {
            diagnostics.append(QStringLiteral("P-CAD: SMD 焊盘 %1 使用无法映射的铜层").arg(pad.number));
            return false;
        }
        if (padShape(pad, diagnostics).isEmpty())
            return false;
    }
    for (const IR::FootprintTextIR& text : footprint.texts) {
        if (text.isDisplayed && !isAscii(text.text)) {
            diagnostics.append(QStringLiteral("P-CAD: 文本必须是 ASCII，无法安全编码：%1").arg(text.text));
            return false;
        }
    }
    return true;
}

/**
 * @brief 写入经过 P-CAD 坐标系变换后的连续点列表。
 * @param stream P-CAD 文本流。
 * @param points 连续点列表。
 */
void writePoints(QTextStream& stream, const QList<QPointF>& points) {
    for (const QPointF& point : points)
        stream << " (pt " << number(point.x()) << ' ' << number(-point.y()) << ')';
}

/**
 * @brief 将 IR 图元写入 Pattern 的 Layer Contents 节点。
 * @param stream P-CAD 文本流。
 * @param footprint 封装组件 IR。
 * @param diagnostics 诊断信息输出列表。
 * @return 写入成功时返回 true。
 */
bool writeGraphics(QTextStream& stream, const IR::FootprintComponentIR& footprint, QStringList& diagnostics) {
    const auto layerOf = [&diagnostics](IR::LayerType layer) -> std::optional<int> {
        const std::optional<int> numberValue = layerNumber(layer);  // P-CAD 图层编号。
        if (!numberValue.has_value())
            diagnostics.append(QStringLiteral("P-CAD: 图元使用无法映射的 IR 图层"));
        return numberValue;
    };

    for (const IR::FootprintTrackIR& track : footprint.tracks) {
        if (track.points.size() < 2)
            continue;
        const std::optional<int> layer = layerOf(track.layer);  // 走线目标图层。
        if (!layer.has_value())
            return false;
        stream << "      (layerContents (layerNumRef " << *layer << ')';
        for (int index = 0; index + 1 < track.points.size(); ++index) {
            stream << "\n        (line";
            writePoints(stream, {track.points.at(index), track.points.at(index + 1)});
            stream << " (width " << number(track.width) << "))";
        }
        stream << "\n      )\n";
    }
    for (const IR::FootprintCircleIR& circle : footprint.circles) {
        const std::optional<int> layer = layerOf(circle.layer);  // 圆形图元目标图层。
        if (!layer.has_value())
            return false;
        stream << "      (layerContents (layerNumRef " << *layer << ")\n        (circle";
        writePoints(stream, {circle.center});
        stream << " (radius " << number(circle.radius) << ") (width " << number(circle.strokeWidth) << "))\n"
               << "      )\n";
    }
    for (const IR::FootprintRectangleIR& rectangle : footprint.rectangles) {
        const std::optional<int> layer = layerOf(rectangle.layer);  // 矩形图元目标图层。
        if (!layer.has_value())
            return false;
        const QRectF bounds = rectangle.bounds;  // 矩形边界。
        const QList<QPointF> corners = {// 矩形闭合顶点。
                                        bounds.topLeft(),
                                        bounds.topRight(),
                                        bounds.bottomRight(),
                                        bounds.bottomLeft(),
                                        bounds.topLeft()};
        stream << "      (layerContents (layerNumRef " << *layer << ')';
        for (int index = 0; index + 1 < corners.size(); ++index) {
            stream << "\n        (line";
            writePoints(stream, {corners.at(index), corners.at(index + 1)});
            stream << " (width " << number(rectangle.strokeWidth) << "))";
        }
        stream << "\n      )\n";
    }
    for (const IR::FootprintRegionIR& region : footprint.regions) {
        if (region.vertices.size() < 3)
            continue;
        const std::optional<int> layer = layerOf(region.layer);  // 区域目标图层。
        if (!layer.has_value())
            return false;
        stream << "      (layerContents (layerNumRef " << *layer << ")\n        (pcbPoly";
        writePoints(stream, region.vertices);
        stream << ")\n      )\n";
    }
    for (const IR::FootprintOutlineIR& outline : footprint.outlines) {
        if (outline.points.size() < 2)
            continue;
        const std::optional<int> layer = layerOf(outline.layer);  // 轮廓目标图层。
        if (!layer.has_value())
            return false;
        stream << "      (layerContents (layerNumRef " << *layer << ')';
        for (int index = 0; index + 1 < outline.points.size(); ++index) {
            stream << "\n        (line";
            writePoints(stream, {outline.points.at(index), outline.points.at(index + 1)});
            stream << " (width " << number(outline.strokeWidth) << "))";
        }
        stream << "\n      )\n";
    }
    for (const IR::FootprintTextIR& text : footprint.texts) {
        if (!text.isDisplayed || text.text.isEmpty())
            continue;
        const std::optional<int> layer = layerOf(text.layer);  // 文本目标图层。
        if (!layer.has_value())
            return false;
        stream << "      (layerContents (layerNumRef " << *layer << ")\n"
               << "        (text " << quote(text.text) << "\n"
               << "          (pt " << number(text.position.x()) << ' ' << number(-text.position.y()) << ")\n"
               << "          (rotation " << QString::number(text.rotation * 10.0, 'f', 3) << ")\n"
               << "          (isFlipped " << (text.mirror ? "True" : "False") << ")\n"
               << "        )\n      )\n";
    }
    return true;
}

/**
 * @brief 将封装列表写入 P-CAD ASCII 库文件。
 * @param footprints 封装列表。
 * @param libName 库名称。
 * @param path 输出文件路径。
 * @param diagnostics 诊断信息输出列表。
 * @return 写入成功时返回 true。
 */
bool writeLibrary(const QList<IR::FootprintComponentIR>& footprints,
                  const QString& libName,
                  const QString& path,
                  QStringList& diagnostics) {
    if (footprints.isEmpty()) {
        diagnostics.append(QStringLiteral("P-CAD: 没有可导出的封装"));
        return false;
    }
    QSet<QString> names;  // 已使用的封装名称。
    QHash<QString, QString> padStyles;  // Pad Style 去重键到名称的映射。
    QList<const IR::FootprintPadIR*> stylePads;  // 每种 Pad Style 的代表焊盘。
    for (const IR::FootprintComponentIR& footprint : footprints) {
        const QString name = safeName(footprint.name);  // 当前封装名称。
        if (!validateFootprint(footprint, diagnostics))
            return false;
        if (names.contains(name)) {
            diagnostics.append(QStringLiteral("P-CAD: 封装名称清洗后冲突：%1").arg(name));
            return false;
        }
        names.insert(name);
        for (const IR::FootprintPadIR& pad : footprint.pads) {
            const QString shape = padShape(pad, diagnostics);  // 当前焊盘形状文本。
            if (shape.isEmpty())
                return false;
            const QString key = padKey(pad, shape);  // 当前焊盘去重键。
            if (!padStyles.contains(key)) {
                padStyles.insert(key, QStringLiteral("EK_%1").arg(padStyles.size() + 1));
                stylePads.append(&pad);
            }
        }
    }

    QFile file(path);  // P-CAD 库输出文件。
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        diagnostics.append(QStringLiteral("P-CAD: 无法写入文件：%1").arg(path));
        return false;
    }
    QTextStream stream(&file);  // UTF-8 P-CAD 输出流。
    stream.setEncoding(QStringConverter::Utf8);
    stream << "(ACCEL_ASCII " << quote(libName.isEmpty() ? QStringLiteral("EasyKiConverter") : libName) << "\n"
           << "  (asciiHeader\n    (asciiVersion 4 0)\n    (fileUnits MM)\n  )\n"
           << "  (library\n";
    for (const IR::FootprintPadIR* pad : stylePads) {
        const QString shape = padShape(*pad, diagnostics);  // Pad Style 形状文本。
        stream << "    (padStyleDef " << quote(padStyles.value(padKey(*pad, shape))) << "\n"
               << "      (holeDiam " << number(pad->isThroughHole() ? pad->holeSize : 0.0) << ")\n"
               << "      (isHolePlated " << (pad->isPlated ? "True" : "False") << ")\n";
        if (pad->isThroughHole()) {
            writePadShape(stream, *pad, shape, 1);
            writePadShape(stream, *pad, shape, 2);
        } else {
            writePadShape(stream, *pad, shape, pad->layer == IR::LayerType::BottomCopper ? 2 : 1);
        }
        stream << "    )\n";
    }
    for (const IR::FootprintComponentIR& footprint : footprints) {
        const QString name = safeName(footprint.name);  // 当前封装名称。
        stream << "    (patternDef " << quote(name) << "\n"
               << "      (originalName " << quote(name) << ")\n"
               << "      (multiLayer\n";
        for (const IR::FootprintPadIR& pad : footprint.pads) {
            const QString shape = padShape(pad, diagnostics);  // 当前焊盘形状文本。
            stream << "        (pad (padNum " << quote(pad.number) << ") (padStyleRef "
                   << quote(padStyles.value(padKey(pad, shape))) << ") (pt " << number(pad.position.x()) << ' '
                   << number(-pad.position.y()) << ") (rotation " << QString::number(pad.rotation * 10.0, 'f', 3)
                   << "))\n";
        }
        stream << "      )\n";
        if (!writeGraphics(stream, footprint, diagnostics))
            return false;
        stream << "    )\n";
    }
    stream << "  )\n)\n";
    stream.flush();
    if (stream.status() != QTextStream::Ok) {
        diagnostics.append(QStringLiteral("P-CAD: 写入文件时发生 I/O 错误：%1").arg(path));
        return false;
    }
    return true;
}

}  // namespace

/**
 * @brief 返回 P-CAD 封装库文件扩展名。
 * @return P-CAD 封装库扩展名。
 */
QString ExporterPcadFootprint::libraryFileExtension() const {
    return QStringLiteral(".lia");
}

/**
 * @brief 判断 P-CAD 封装库是否使用目录输出。
 * @return P-CAD 使用单文件输出，因此返回 false。
 */
bool ExporterPcadFootprint::isDirectoryOutput() const {
    return false;
}

/**
 * @brief 导出单个封装并保留本次导出的诊断信息。
 * @param footprint 封装组件 IR。
 * @param filePath 输出文件路径。
 * @param model3DPath 可选的 3D 模型路径。
 * @return 导出成功时返回 true。
 */
bool ExporterPcadFootprint::exportFootprint(const IR::FootprintComponentIR& footprint,
                                            const QString& filePath,
                                            const QString&) {
    m_diagnostics.clear();
    return writeLibrary({footprint}, footprint.name, filePath, m_diagnostics);
}

/**
 * @brief 导出多个封装并统一写入一个 P-CAD ASCII 库文件。
 * @param footprints 封装列表。
 * @param libName 库名称。
 * @param filePath 输出文件路径。
 * @param appendMode 是否追加。
 * @param updateMode 是否更新。
 * @param libraryDescription 库描述。
 * @param libraryKeywords 库关键词。
 * @param useAbsolutePaths 是否使用绝对路径。
 * @param model3DBaseDir 3D 模型基础目录。
 * @return 导出成功时返回 true。
 */
bool ExporterPcadFootprint::exportFootprintLibrary(const QList<IR::FootprintComponentIR>& footprints,
                                                   const QString& libName,
                                                   const QString& filePath,
                                                   bool,
                                                   bool,
                                                   const QString&,
                                                   const QString&,
                                                   bool,
                                                   const QString&) {
    m_diagnostics.clear();
    return writeLibrary(footprints, libName, filePath, m_diagnostics);
}

/**
 * @brief 返回最近一次导出的格式限制、降级和错误诊断。
 * @return 诊断信息列表。
 */
QStringList ExporterPcadFootprint::diagnostics() const {
    return m_diagnostics;
}

}  // namespace EasyKiConverter
