#include "GeometryTransforms.h"

#include <cmath>

namespace EasyKiConverter::Parser {

/**
 * @brief 将来源单位的数值统一换算为 IR 使用的毫米。
 * @param value 来源数值。
 * @param unit 来源长度单位。
 * @return 换算后的毫米数值。
 */
double UnitConverter::toMillimeters(double value, LengthUnit unit) {
    // 按格式声明选择精确的单位比例，未知单位保持原值并由上层诊断。
    switch (unit) {
        case LengthUnit::Millimeter:
            return value;
        case LengthUnit::Mil:
            return value * 0.0254;
        case LengthUnit::Inch:
            return value * 25.4;
        case LengthUnit::Unknown:
            return value;
    }
    return value;
}

/**
 * @brief 解析格式文件中的单位名称。
 * @param text 单位名称文本。
 * @param diagnostics 可选的诊断接收器。
 * @return 解析出的长度单位。
 */
LengthUnit UnitConverter::parseUnit(const QString& text, ParseDiagnostics* diagnostics) {
    const QString normalized = text.trimmed().toLower();  // 统一大小写和首尾空白后的单位名称。
    if (normalized == QStringLiteral("mm") || normalized == QStringLiteral("millimeter") ||
        normalized == QStringLiteral("millimeters"))
        return LengthUnit::Millimeter;
    if (normalized == QStringLiteral("mil") || normalized == QStringLiteral("mils") ||
        normalized == QStringLiteral("th"))
        return LengthUnit::Mil;
    if (normalized == QStringLiteral("in") || normalized == QStringLiteral("inch") ||
        normalized == QStringLiteral("inches"))
        return LengthUnit::Inch;
    if (diagnostics)
        diagnostics->add(ParseSeverity::Warning,
                         ParseScope::Field,
                         QStringLiteral("未知长度单位，保留原始数值：%1").arg(text),
                         QStringLiteral("units"));
    return LengthUnit::Unknown;
}

/**
 * @brief 对坐标应用平移、镜像和旋转变换。
 * @param point 待变换坐标。
 * @param origin 旋转和镜像中心。
 * @param rotationDegrees 旋转角度，单位为度。
 * @param mirrorX 是否沿 X 轴镜像。
 * @param mirrorY 是否沿 Y 轴镜像。
 * @return 变换后的坐标。
 */
QPointF CoordinateTransform::apply(const QPointF& point,
                                   const QPointF& origin,
                                   double rotationDegrees,
                                   bool mirrorX,
                                   bool mirrorY) {
    QPointF translated = point - origin;  // 相对变换中心的坐标。
    if (mirrorX)
        translated.setX(-translated.x());
    if (mirrorY)
        translated.setY(-translated.y());
    constexpr double pi = 3.14159265358979323846;  // 圆周率常量。
    const double radians = rotationDegrees * pi / 180.0;  // 旋转角度的弧度值。
    const double cosine = std::cos(radians);  // 旋转矩阵余弦项。
    const double sine = std::sin(radians);  // 旋转矩阵正弦项。
    return QPointF(translated.x() * cosine - translated.y() * sine + origin.x(),
                   translated.x() * sine + translated.y() * cosine + origin.y());
}

}  // namespace EasyKiConverter::Parser
