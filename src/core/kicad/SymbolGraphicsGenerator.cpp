/**
 * @file SymbolGraphicsGenerator.cpp
 * @brief SymbolGraphicsGenerator 的实现。
 */
#include "SymbolGraphicsGenerator.h"

#include "KiCadTypeMap.h"

#include <QDebug>
#include <QRegularExpression>

#include <cmath>

namespace EasyKiConverter {

/**
 * @brief 生成符号的全部图形文本。
 * @param data 符号图形数据。
 * @return KiCad 图形文本。
 */
QString SymbolGraphicsGenerator::generateDrawings(const IR::SymbolComponentIR& data) const {
    QString content;  // 全部图形的序列化文本。
    for (const IR::SymbolRectangleIR& rect : data.rectangles) {
        content += generateRectangle(rect);
    }
    for (const IR::SymbolCircleIR& circle : data.circles) {
        content += generateCircle(circle);
    }
    for (const IR::SymbolArcIR& arc : data.arcs) {
        content += generateArc(arc);
    }
    for (const IR::SymbolEllipseIR& ellipse : data.ellipses) {
        content += generateEllipse(ellipse);
    }
    for (const IR::SymbolPolygonIR& polygon : data.polygons) {
        content += generatePolygon(polygon);
    }
    for (const IR::SymbolPolylineIR& polyline : data.polylines) {
        content += generatePolyline(polyline);
    }
    for (const IR::SymbolPathIR& path : data.paths) {
        content += generatePath(path);
    }
    for (const IR::SymbolTextIR& text : data.texts) {
        content += generateText(text);
    }
    return content;
}

/**
 * @brief 生成符号的全部引脚文本。
 * @param pins 引脚列表。
 * @return KiCad 引脚文本。
 */
QString SymbolGraphicsGenerator::generatePins(const QList<IR::SymbolPinIR>& pins) const {
    QString content;  // 全部引脚的序列化文本。
    for (const IR::SymbolPinIR& pin : pins) {
        content += generatePin(pin);
    }
    return content;
}

/**
 * @brief 生成单个引脚及其文本属性。
 * @param pin 引脚数据。
 * @return KiCad 引脚文本。
 */
QString SymbolGraphicsGenerator::generatePin(const IR::SymbolPinIR& pin) const {
    QString content;  // 单个引脚的序列化文本。

    // 坐标已为 mm，直接计算相对位置
    double x = pin.position.x() - m_originX;  // 引脚相对原点的 X 坐标。
    double y = pin.position.y() - m_originY;  // 引脚相对原点的 Y 坐标。

    // 引脚长度（已为 mm）
    double length = pin.length;

    // 确保引脚长度为正数（KiCad 引脚长度必须是正数）
    length = std::abs(length);

    // 确保引脚长度不为 0
    if (length < 0.01) {
        length = 2.54;  // 默认引脚长度（100mil）
    }

    // 使用 IR 映射获取 KiCad 引脚电气类型
    const char* kicadPinType = KiCadTypeMap::toKicadPinType(pin.electricalType);

    // 引脚样式：从 IR 语义层获取
    QString kicadPinStyle = "line";
    if (pin.style.inverted && pin.style.clock) {
        kicadPinStyle = "inverted_clock";
    } else if (pin.style.inverted) {
        kicadPinStyle = "inverted";
    } else if (pin.style.clock) {
        kicadPinStyle = "clock";
    }

    // 处理引脚名称和编号
    QString pinName = pin.name;
    pinName.replace(" ", "");
    QString pinNumber = pin.designator;  // 去除空格前的引脚编号。
    pinNumber.replace(" ", "");

    // 如果引脚名称为空，使用引脚编号
    if (pinName.isEmpty()) {
        pinName = pinNumber;
    }

    const bool useOriginalNamePosition = pin.hasNamePosition && !pinName.isEmpty();  // 是否保留原始名称位置。
    const QString nameHide = useOriginalNamePosition ? QStringLiteral(" hide") : QString();  // 名称字段隐藏标记。

    // 使用 IR 方向枚举获取 KiCad 角度
    double kicadOrientation = KiCadTypeMap::toKicadAngle(pin.direction);

    content += QString("    (pin %1 %2\n").arg(kicadPinType, kicadPinStyle);
    content += QString("      (at %1 %2 %3)\n").arg(x, 0, 'f', 2).arg(y, 0, 'f', 2).arg(kicadOrientation, 0, 'f', 0);
    content += QString("      (length %1)\n").arg(length, 0, 'f', 2);
    if (useOriginalNamePosition) {
        content +=
            QString("      (name \"%1\" (effects (font (size 1.27 1.27) (thickness 0))%2))\n").arg(pinName, nameHide);
    } else {
        content += QString("      (name \"%1\" (effects (font (size 1.27 1.27) (thickness 0) )))\n").arg(pinName);
    }
    // 引脚编号使用 KiCad 原生字段，避免把原始编号再绘制成倾斜的普通文本图元。
    content += QString("      (number \"%1\" (effects (font (size 1.27 1.27) (thickness 0) )))\n").arg(pinNumber);
    content += "    )\n";

    auto kicadJustification = [](const QString& anchor) {
        const QString normalized = anchor.trimmed().toLower();  // 统一后的文本锚点名称。
        if (normalized == QStringLiteral("start") || normalized == QStringLiteral("left"))
            return QStringLiteral(" (justify left)");
        if (normalized == QStringLiteral("end") || normalized == QStringLiteral("right"))
            return QStringLiteral(" (justify right)");
        if (normalized == QStringLiteral("top"))
            return QStringLiteral(" (justify top)");
        if (normalized == QStringLiteral("bottom"))
            return QStringLiteral(" (justify bottom)");
        return QString();
    };
    auto appendOriginalText =
        [&](const QString& text, const QPointF& position, double rotation, const QString& anchor, double fontSizeMm) {
            double textRotation = rotation;  // KiCad 文本旋转角度。
            if (textRotation != 0.0)
                textRotation = 360.0 - textRotation;
            const double size = fontSizeMm > 0.0 ? fontSizeMm : 1.27;  // 文本字号，单位为毫米。
            content += "    (text\n";
            content += QString("      \"%1\"\n").arg(text);
            content += QString("      (at %1 %2 %3)\n")
                           .arg(position.x() - m_originX, 0, 'f', 2)
                           .arg(position.y() - m_originY, 0, 'f', 2)
                           .arg(textRotation, 0, 'f', 0);
            content += QString("      (effects (font (size %1 %1) (thickness 0.1))%2)\n")
                           .arg(size, 0, 'f', 2)
                           .arg(kicadJustification(anchor));
            content += "    )\n";
        };
    if (useOriginalNamePosition)
        appendOriginalText(pinName, pin.namePosition, pin.nameRotation, pin.nameAnchor, pin.nameFontSizeMm);
    return content;
}

/**
 * @brief 生成矩形图形文本。
 * @param rect 矩形数据。
 * @return KiCad 矩形文本。
 */
QString SymbolGraphicsGenerator::generateRectangle(const IR::SymbolRectangleIR& rect) const {
    QString content;  // 矩形的序列化文本。

    // 坐标已为 mm，计算相对位置
    double x0 = rect.x0 - m_originX;  // 矩形起点 X 坐标。
    double y0 = rect.y0 - m_originY;  // 矩形起点 Y 坐标。
    double x1 = rect.x1 - m_originX;  // 矩形终点 X 坐标。
    double y1 = rect.y1 - m_originY;  // 矩形终点 Y 坐标。
    double strokeWidth = rect.strokeWidth;  // 矩形描边宽度。

    content += "    (rectangle\n";
    content += QString("      (start %1 %2)\n").arg(x0, 0, 'f', 2).arg(y0, 0, 'f', 2);
    content += QString("      (end %1 %2)\n").arg(x1, 0, 'f', 2).arg(y1, 0, 'f', 2);
    content += QString("      (stroke (width %1) (type default))\n").arg(strokeWidth, 0, 'f', 3);
    content += "      (fill (type none))\n";
    content += "    )\n";

    return content;
}

/**
 * @brief 生成圆形图形文本。
 * @param circle 圆形数据。
 * @return KiCad 圆形文本。
 */
QString SymbolGraphicsGenerator::generateCircle(const IR::SymbolCircleIR& circle) const {
    QString content;  // 圆形的序列化文本。

    // 坐标已为 mm，计算相对位置
    double cx = circle.center.x() - m_originX;  // 圆心相对原点的 X 坐标。
    double cy = circle.center.y() - m_originY;  // 圆心相对原点的 Y 坐标。
    double radius = circle.radius;  // 圆半径。
    double strokeWidth = circle.strokeWidth;  // 圆描边宽度。

    content += "    (circle\n";
    content += QString("      (center %1 %2)\n").arg(cx, 0, 'f', 2).arg(cy, 0, 'f', 2);
    content += QString("      (radius %1)\n").arg(radius, 0, 'f', 2);
    content += QString("      (stroke (width %1) (type default))\n").arg(strokeWidth, 0, 'f', 3);
    content += "      (fill (type none))\n";
    content += "    )\n";

    return content;
}

/**
 * @brief 生成圆弧图形文本。
 * @param arc 圆弧数据。
 * @return KiCad 圆弧文本。
 */
QString SymbolGraphicsGenerator::generateArc(const IR::SymbolArcIR& arc) const {
    QString content;  // 圆弧的序列化文本。
    double strokeWidth = arc.strokeWidth;  // 圆弧描边宽度。

    // 使用三点法生成 KiCad 圆弧（与旧代码一致）
    double startX = arc.startPoint.x() - m_originX;  // 圆弧起点 X 坐标。
    double startY = arc.startPoint.y() - m_originY;  // 圆弧起点 Y 坐标。
    double midX = arc.midPoint.x() - m_originX;  // 圆弧中点 X 坐标。
    double midY = arc.midPoint.y() - m_originY;  // 圆弧中点 Y 坐标。
    double endX = arc.endPoint.x() - m_originX;  // 圆弧终点 X 坐标。
    double endY = arc.endPoint.y() - m_originY;  // 圆弧终点 Y 坐标。

    content += "    (arc\n";
    content += QString("      (start %1 %2)\n").arg(startX, 0, 'f', 2).arg(startY, 0, 'f', 2);
    content += QString("      (mid %1 %2)\n").arg(midX, 0, 'f', 2).arg(midY, 0, 'f', 2);
    content += QString("      (end %1 %2)\n").arg(endX, 0, 'f', 2).arg(endY, 0, 'f', 2);
    content += QString("      (stroke (width %1) (type default))\n").arg(strokeWidth, 0, 'f', 3);

    if (arc.isFilled) {
        content += "      (fill (type background))\n";
    } else {
        content += "      (fill (type none))\n";
    }

    content += "    )\n";

    return content;
}

/**
 * @brief 生成椭圆图形文本。
 * @param ellipse 椭圆数据。
 * @return KiCad 椭圆文本。
 */
QString SymbolGraphicsGenerator::generateEllipse(const IR::SymbolEllipseIR& ellipse) const {
    QString content;  // 椭圆的序列化文本。

    // 坐标已为 mm，计算相对位置
    double cx = ellipse.center.x() - m_originX;  // 椭圆中心 X 坐标。
    double cy = ellipse.center.y() - m_originY;  // 椭圆中心 Y 坐标。
    double radiusX = ellipse.radiusX;  // 椭圆 X 方向半径。
    double radiusY = ellipse.radiusY;  // 椭圆 Y 方向半径。
    double strokeWidth = ellipse.strokeWidth;  // 椭圆描边宽度。

    // 如果是圆形（radiusX 等于 radiusY），使用 circle 元素
    if (qAbs(radiusX - radiusY) < 0.01) {
        content += "    (circle\n";
        content += QString("      (center %1 %2)\n").arg(cx, 0, 'f', 2).arg(cy, 0, 'f', 2);
        content += QString("      (radius %1)\n").arg(radiusX, 0, 'f', 2);
        content += QString("      (stroke (width %1) (type default))\n").arg(strokeWidth, 0, 'f', 3);
        content += "      (fill (type none))\n";
        content += "    )\n";
    } else {
        // 椭圆：使用 32 段折线近似
        content += "    (polyline\n";
        content += "      (pts";

        const int segments = 32;  // 椭圆折线近似使用的分段数。
        for (int i = 0; i <= segments; ++i) {
            double angle = 2.0 * M_PI * i / segments;  // 当前近似点的角度。
            double x = cx + radiusX * std::cos(angle);  // 近似点 X 坐标。
            double y = cy + radiusY * std::sin(angle);  // 近似点 Y 坐标。
            content += QString(" (xy %1 %2)").arg(x, 0, 'f', 2).arg(y, 0, 'f', 2);
        }

        content += ")\n";
        content += QString("      (stroke (width %1) (type default))\n").arg(strokeWidth, 0, 'f', 3);

        if (ellipse.isFilled) {
            content += "      (fill (type background))\n";
        } else {
            content += "      (fill (type none))\n";
        }
        content += "    )\n";
    }

    return content;
}

/**
 * @brief 生成多边形图形文本。
 * @param polygon 多边形数据。
 * @return KiCad 多边形文本。
 */
QString SymbolGraphicsGenerator::generatePolygon(const IR::SymbolPolygonIR& polygon) const {
    QString content;  // 多边形的序列化文本。
    double strokeWidth = polygon.strokeWidth;  // 多边形描边宽度。

    // IR 中坐标已解析为 QList<QPointF>
    if (polygon.points.size() >= 2) {
        // KiCad V6 不支持 polygon 元素，使用 polyline 代替
        content += "    (polyline\n";
        content += "      (pts";
        QString firstPoint;  // 首个去重后的顶点文本。
        QString lastPoint;  // 最近一次写入的顶点文本。
        for (const QPointF& pt : polygon.points) {
            double x = pt.x() - m_originX;  // 顶点相对原点的 X 坐标。
            double y = pt.y() - m_originY;  // 顶点相对原点的 Y 坐标。
            QString point = QString(" (xy %1 %2)").arg(x, 0, 'f', 2).arg(y, 0, 'f', 2);  // KiCad 顶点文本。

            // 避免重复点
            if (point != lastPoint) {
                content += point;
                if (firstPoint.isEmpty()) {
                    firstPoint = point;
                }
                lastPoint = point;
            }
        }
        // 多边形总是重复第一个点以闭合
        if (!firstPoint.isEmpty() && firstPoint != lastPoint) {
            content += firstPoint;
        }
        content += ")\n";
        content += QString("      (stroke (width %1) (type default))\n").arg(strokeWidth, 0, 'f', 3);
        if (polygon.isFilled) {
            content += "      (fill (type background))\n";
        } else {
            content += "      (fill (type none))\n";
        }
        content += "    )\n";
    }

    return content;
}

/**
 * @brief 生成折线图形文本。
 * @param polyline 折线数据。
 * @return KiCad 折线文本。
 */
QString SymbolGraphicsGenerator::generatePolyline(const IR::SymbolPolylineIR& polyline) const {
    QString content;  // 折线的序列化文本。
    double strokeWidth = polyline.strokeWidth;  // 折线描边宽度。

    // IR 中坐标已解析为 QList<QPointF>
    if (polyline.points.size() >= 2) {
        content += "    (polyline\n";
        content += "      (pts";
        QString firstPoint;  // 首个去重后的顶点文本。
        QString lastPoint;  // 最近一次写入的顶点文本。
        for (const QPointF& pt : polyline.points) {
            double x = pt.x() - m_originX;  // 顶点相对原点的 X 坐标。
            double y = pt.y() - m_originY;  // 顶点相对原点的 Y 坐标。
            QString point = QString(" (xy %1 %2)").arg(x, 0, 'f', 2).arg(y, 0, 'f', 2);  // KiCad 顶点文本。

            // 避免重复点
            if (point != lastPoint) {
                content += point;
                if (firstPoint.isEmpty()) {
                    firstPoint = point;
                }
                lastPoint = point;
            }
        }
        // 只有 fillColor 为 true 时才重复第一个点
        if (polyline.isFilled && !firstPoint.isEmpty() && firstPoint != lastPoint) {
            content += firstPoint;
        }
        content += ")\n";
        content += QString("      (stroke (width %1) (type default))\n").arg(strokeWidth, 0, 'f', 3);
        if (polyline.isFilled) {
            content += "      (fill (type background))\n";
        } else {
            content += "      (fill (type none))\n";
        }
        content += "    )\n";
    }

    return content;
}

/**
 * @brief 生成路径图形文本。
 * @param path 路径数据。
 * @return KiCad 路径文本。
 */
QString SymbolGraphicsGenerator::generatePath(const IR::SymbolPathIR& path) const {
    QString content;  // 路径的序列化文本。

    // IR 中路径坐标已解析为 QList<QPointF>
    if (!path.points.isEmpty()) {
        content += "    (polyline\n";
        content += "      (pts";

        QString lastPoint;  // 最近一次写入的顶点文本。
        for (const QPointF& pt : path.points) {
            // 坐标已为 mm，计算相对位置
            double x = pt.x() - m_originX;  // 顶点相对原点的 X 坐标。
            double y = pt.y() - m_originY;  // 顶点相对原点的 Y 坐标。

            QString point = QString(" (xy %1 %2)").arg(x, 0, 'f', 2).arg(y, 0, 'f', 2);  // KiCad 顶点文本。

            // 避免重复点
            if (point != lastPoint) {
                content += point;
                lastPoint = point;
            }
        }

        content += ")\n";
        content += QString("      (stroke (width %1) (type default))\n").arg(path.strokeWidth, 0, 'f', 3);

        if (path.isFilled) {
            content += "      (fill (type background))\n";
        } else {
            content += "      (fill (type none))\n";
        }

        content += "    )\n";
    } else {
        // 如果没有有效点，生成占位
        content += "    (polyline (pts (xy 0 0))\n";
        content += "      (stroke (width 0.127) (type default))\n";
        content += "      (fill (type none))\n";
        content += "    )\n";
    }

    return content;
}

/**
 * @brief 生成文本图形文本。
 * @param text 文本数据。
 * @return KiCad 文本图元。
 */
QString SymbolGraphicsGenerator::generateText(const IR::SymbolTextIR& text) const {
    QString content;  // 文本图元的序列化文本。

    // 坐标已为 mm，计算相对位置
    double x = text.position.x() - m_originX;  // 文本相对原点的 X 坐标。
    double y = text.position.y() - m_originY;  // 文本相对原点的 Y 坐标。

    // IR 中 fontSizeMm 已从 pt 转换；部分 EasyEDA 文本记录未提供字号，
    // 使用与平台属性一致的默认字号，避免生成 0.00 的不可见文本。
    double fontSize = text.fontSizeMm > 0.0 ? text.fontSizeMm : 1.27;  // 文本字号，单位为毫米。

    // 处理粗体和斜体（IR 中 italic 已为 bool）
    QString fontStyle = "";
    if (text.bold) {
        fontStyle += "bold ";
    }
    if (text.italic) {
        fontStyle += "italic ";
    }
    if (fontStyle.isEmpty()) {
        fontStyle = "normal";
    } else {
        fontStyle = fontStyle.trimmed();
    }

    // 处理旋转角度
    double rotation = text.rotation;
    if (rotation != 0) {
        rotation = 360 - rotation;
    }

    // 处理可见性
    QString hide = text.visible ? "" : "hide";

    // 转义文本内容
    QString textContent = text.text;
    textContent.replace(" ", "");
    if (textContent.isEmpty()) {
        textContent = "~";
    }

    // 生成文本元素
    content += "    (text\n";
    content += QString("      \"%1\"\n").arg(textContent);
    content += QString("      (at %1 %2 %3)\n").arg(x, 0, 'f', 2).arg(y, 0, 'f', 2).arg(rotation, 0, 'f', 0);
    content += QString("      (effects (font (size %1 %2) (thickness 0.1) %3) %4)\n")
                   .arg(fontSize, 0, 'f', 2)
                   .arg(fontSize, 0, 'f', 2)
                   .arg(fontStyle)
                   .arg(hide);
    content += "    )\n";

    return content;
}

}  // namespace EasyKiConverter
