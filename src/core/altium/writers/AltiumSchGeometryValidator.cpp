#include "AltiumSchGeometryValidator.h"

#include "AltiumSchLibWriter.h"

#include <QDebug>

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace EasyKiConverter {

/**
 * @brief 校验组件中所有图元的有限坐标、尺寸、角度和控制点数量。
 * @param writer 所属的 SchLib 主写入器，用于记录诊断信息。
 * @param component 待校验的原理图元件。
 * @return 所有图元均符合 Altium SchLib 约束时返回 true。
 */
bool AltiumSchGeometryValidator::validate(AltiumSchLibWriter& writer, const AltiumSchComponent& component) {
    const auto hasFinitePoints = [](const QList<QPointF>& points) {  // 检查点集合是否全部为有限坐标。
        // 拒绝包含 NaN 或无穷值的点，避免生成无法被 Altium 读取的记录。
        /**
         * @brief 判断单个点坐标是否为有限值。
         */
        return std::all_of(points.cbegin(), points.cend(), [](const QPointF& point) {
            return std::isfinite(point.x()) && std::isfinite(point.y());
        });
    };
    const auto reject = [&writer, &component](const QString& message) {  // 记录诊断并拒绝当前组件。
        writer.m_diagnostics.append(
            QStringLiteral("Altium SchLib 组件 %1 的%2，已拒绝写入").arg(component.name, message));
        qWarning() << "AltiumSchLibWriter:" << writer.m_diagnostics.constLast();
        return false;
    };
    const auto validateLineWidths = [&reject](const auto& objects, const QString& context) {  // 检查图元线宽索引。
        for (const auto& object : objects) {
            if (object.lineWidth < 0 || object.lineWidth > 3)
                return reject(QStringLiteral("%1线宽索引无效: %2").arg(context).arg(object.lineWidth));
        }
        return true;
    };
    const auto validateLineStyles = [&reject](const auto& objects, const QString& context) {  // 检查图元线型索引。
        for (const auto& object : objects) {
            if (object.lineStyle < 0 || object.lineStyle > 2)
                return reject(QStringLiteral("%1线型无效: %2").arg(context).arg(object.lineStyle));
        }
        return true;
    };
    const auto validateShortString = [&reject](const QString& value,
                                               const QString& context) {  // 检查短字符串编码约束。
        const QByteArray encoded = value.toLatin1();  // 按 Altium Latin-1 规则编码后的字节序列。
        if (encoded.size() > 255)
            return reject(QStringLiteral("%1超过 255 字节").arg(context));
        if (value.contains(QChar::Null))
            return reject(QStringLiteral("%1包含 NUL 字符").arg(context));
        if (QString::fromLatin1(encoded) != value)
            return reject(QStringLiteral("%1包含无法编码的字符").arg(context));
        return true;
    };

    for (int i = 0; i < component.pins.size(); ++i) {
        const AltiumSchPin& pin = component.pins.at(i);  // 当前待校验的引脚。
        if (!validateShortString(pin.name, QStringLiteral("引脚 %1 名称").arg(i)) ||
            !validateShortString(pin.designator, QStringLiteral("引脚 %1 编号").arg(i)))
            return false;
        if (static_cast<uint8_t>(pin.orientation) > 3)
            return reject(QStringLiteral("引脚 %1 方向无效: %2").arg(i).arg(static_cast<uint8_t>(pin.orientation)));
        if (static_cast<uint8_t>(pin.electricalType) > 7)
            return reject(
                QStringLiteral("引脚 %1 电气类型无效: %2").arg(i).arg(static_cast<uint8_t>(pin.electricalType)));
    }

    if (!validateLineWidths(component.rectangles, QStringLiteral("矩形图元")) ||
        !validateLineWidths(component.roundRectangles, QStringLiteral("圆角矩形图元")) ||
        !validateLineWidths(component.lines, QStringLiteral("线段图元")) ||
        !validateLineWidths(component.arcs, QStringLiteral("圆弧图元")) ||
        !validateLineWidths(component.polygons, QStringLiteral("多边形图元")) ||
        !validateLineWidths(component.ellipses, QStringLiteral("椭圆图元")) ||
        !validateLineWidths(component.pies, QStringLiteral("扇形图元")) ||
        !validateLineWidths(component.ellipticalArcs, QStringLiteral("椭圆弧图元")) ||
        !validateLineWidths(component.polylines, QStringLiteral("折线图元")) ||
        !validateLineWidths(component.paths, QStringLiteral("路径图元")) ||
        !validateLineWidths(component.beziers, QStringLiteral("Bézier 图元")) ||
        !validateLineWidths(component.ieeeSymbols, QStringLiteral("IEEE 图元")) ||
        !validateLineWidths(component.textFrames, QStringLiteral("文本框图元")) ||
        !validateLineWidths(component.images, QStringLiteral("图片图元")) ||
        !validateLineStyles(component.rectangles, QStringLiteral("矩形图元")) ||
        !validateLineStyles(component.roundRectangles, QStringLiteral("圆角矩形图元")) ||
        !validateLineStyles(component.lines, QStringLiteral("线段图元")) ||
        !validateLineStyles(component.arcs, QStringLiteral("圆弧图元")) ||
        !validateLineStyles(component.polygons, QStringLiteral("多边形图元")) ||
        !validateLineStyles(component.ellipses, QStringLiteral("椭圆图元")) ||
        !validateLineStyles(component.pies, QStringLiteral("扇形图元")) ||
        !validateLineStyles(component.ellipticalArcs, QStringLiteral("椭圆弧图元")) ||
        !validateLineStyles(component.polylines, QStringLiteral("折线图元")) ||
        !validateLineStyles(component.paths, QStringLiteral("路径图元")) ||
        !validateLineStyles(component.textFrames, QStringLiteral("文本框图元")) ||
        !validateLineStyles(component.images, QStringLiteral("图片图元"))) {
        return false;
    }
    const auto validateOrientation = [&reject](int orientation, const QString& context) {
        if (orientation < 0 || orientation > 3)
            return reject(QStringLiteral("%1方向无效: %2").arg(context).arg(orientation));
        return true;
    };

    for (const AltiumSchIeee& ieee : component.ieeeSymbols) {
        if (!validateOrientation(ieee.orientation, QStringLiteral("IEEE 图元")))
            return false;
        if (ieee.symbol < 0 || ieee.symbol > 34)
            return reject(QStringLiteral("IEEE 图元符号编号无效: %1").arg(ieee.symbol));
        if (ieee.scaleFactor < 1)
            return reject(QStringLiteral("IEEE 图元缩放因子无效: %1").arg(ieee.scaleFactor));
        if (ieee.lineWidth < 0 || ieee.lineWidth > 3)
            return reject(QStringLiteral("IEEE 图元线宽索引无效: %1").arg(ieee.lineWidth));
    }
    for (const AltiumSchText& text : component.texts) {
        if (!validateOrientation(text.orientation, QStringLiteral("文本图元")))
            return false;
    }
    for (const AltiumSchTextFrame& frame : component.textFrames) {
        if (!validateOrientation(frame.orientation, QStringLiteral("文本框图元")))
            return false;
    }
    for (const AltiumSchParameter& parameter : component.parameters) {
        if (!validateOrientation(parameter.orientation, QStringLiteral("参数")))
            return false;
    }

    for (const AltiumSchRoundRectangle& rect : component.roundRectangles) {
        if (rect.cornerXRadius < 0 || rect.cornerYRadius < 0)
            return reject(QStringLiteral("圆角矩形圆角半径无效"));
    }
    for (const AltiumSchArc& arc : component.arcs) {
        if (arc.radius <= 0)
            return reject(QStringLiteral("圆弧半径无效"));
    }
    for (const AltiumSchEllipse& ellipse : component.ellipses) {
        if (ellipse.radiusX <= 0 || ellipse.radiusY <= 0)
            return reject(QStringLiteral("椭圆半径无效"));
    }
    for (const AltiumSchPie& pie : component.pies) {
        if (pie.radius <= 0)
            return reject(QStringLiteral("扇形半径无效"));
    }
    for (const AltiumSchEllipticalArc& arc : component.ellipticalArcs) {
        if (arc.radiusX <= 0 || arc.radiusY <= 0)
            return reject(QStringLiteral("椭圆弧半径无效"));
    }
    for (const AltiumSchRectangle& rect : component.rectangles) {
        if (rect.locationX == rect.cornerX || rect.locationY == rect.cornerY)
            return reject(QStringLiteral("矩形图元边界尺寸无效"));
    }
    for (const AltiumSchRoundRectangle& rect : component.roundRectangles) {
        if (rect.locationX == rect.cornerX || rect.locationY == rect.cornerY)
            return reject(QStringLiteral("圆角矩形图元边界尺寸无效"));
    }
    for (const AltiumSchLine& line : component.lines) {
        if (line.locationX == line.cornerX && line.locationY == line.cornerY)
            return reject(QStringLiteral("线段图元长度无效"));
    }
    for (const AltiumSchTextFrame& frame : component.textFrames) {
        if (frame.locationX == frame.cornerX || frame.locationY == frame.cornerY)
            return reject(QStringLiteral("文本框边界尺寸无效"));
        if (frame.textMargin < 0)
            return reject(QStringLiteral("文本框文本边距无效: %1").arg(frame.textMargin));
    }
    for (const AltiumSchPolygon& polygon : component.polygons) {
        if (polygon.vertices.size() < 3)
            return reject(QStringLiteral("多边形顶点数量不足"));
        if (!hasFinitePoints(polygon.vertices))
            return reject(QStringLiteral("多边形顶点包含非有限坐标"));
    }
    for (const AltiumSchPolyline& polyline : component.polylines) {
        if (polyline.vertices.size() < 2)
            return reject(QStringLiteral("折线顶点数量不足"));
        if (!hasFinitePoints(polyline.vertices))
            return reject(QStringLiteral("折线顶点包含非有限坐标"));
    }
    for (const AltiumSchPath& path : component.paths) {
        if (path.vertices.size() < 2)
            return reject(QStringLiteral("路径顶点数量不足"));
        if (!hasFinitePoints(path.vertices))
            return reject(QStringLiteral("路径顶点包含非有限坐标"));
    }
    for (const AltiumSchBezier& bezier : component.beziers) {
        if (bezier.controlPoints.size() != 4)
            return reject(QStringLiteral("Bézier 控制点数量无效"));
        if (!hasFinitePoints(bezier.controlPoints))
            return reject(QStringLiteral("Bézier 控制点包含非有限坐标"));
    }
    for (const AltiumSchImage& image : component.images) {
        if (!std::isfinite(image.rotation))
            return reject(QStringLiteral("图片旋转角度无效"));
        if (image.locationX == image.cornerX || image.locationY == image.cornerY)
            return reject(QStringLiteral("图片边界尺寸无效"));
    }
    return true;
}

}  // namespace EasyKiConverter
