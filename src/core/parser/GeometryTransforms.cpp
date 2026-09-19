#include "GeometryTransforms.h"

#include <cmath>

namespace EasyKiConverter::Parser {

// 将来源单位的数值统一换算为 IR 使用的毫米。
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

// 解析格式文件中的单位名称并为未知值生成可观察告警。
LengthUnit UnitConverter::parseUnit(const QString& text, ParseDiagnostics* diagnostics) {
    const QString normalized = text.trimmed().toLower();
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

QPointF CoordinateTransform::apply(const QPointF& point,
                                   const QPointF& origin,
                                   double rotationDegrees,
                                   bool mirrorX,
                                   bool mirrorY) {
    QPointF translated = point - origin;
    if (mirrorX)
        translated.setX(-translated.x());
    if (mirrorY)
        translated.setY(-translated.y());
    constexpr double pi = 3.14159265358979323846;
    const double radians = rotationDegrees * pi / 180.0;
    const double cosine = std::cos(radians);
    const double sine = std::sin(radians);
    return QPointF(translated.x() * cosine - translated.y() * sine + origin.x(),
                   translated.x() * sine + translated.y() * cosine + origin.y());
}

}  // namespace EasyKiConverter::Parser
