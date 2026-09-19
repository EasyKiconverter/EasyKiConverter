#pragma once

/**
 * @file GeometryTransforms.h
 * @brief 格式解析阶段通用的单位和坐标变换。
 */

#include "ParseDiagnostics.h"

#include <QPointF>

namespace EasyKiConverter::Parser {

/** @brief 解析源文件中常见的长度单位。 */
enum class LengthUnit { Millimeter, Mil, Inch, Unknown };

/**
 * @brief 在源格式单位和 IR 毫米单位之间转换。
 */
class UnitConverter {
public:
    /** @brief 将指定单位的长度转换为毫米。 */
    static double toMillimeters(double value, LengthUnit unit);

    /**
     * @brief 解析单位名称。
     * @param text 单位文本，例如 mm、mil 或 inch。
     * @param diagnostics 可选诊断接收器。
     * @return 识别出的单位，未知文本返回 Unknown。
     */
    static LengthUnit parseUnit(const QString& text, ParseDiagnostics* diagnostics = nullptr);
};

/**
 * @brief 执行统一的平移、旋转和镜像坐标变换。
 */
class CoordinateTransform {
public:
    /**
     * @brief 将源坐标转换到目标坐标系。
     * @param point 源坐标。
     * @param origin 变换原点。
     * @param rotationDegrees 逆时针旋转角度。
     * @param mirrorX 是否沿 X 轴镜像。
     * @param mirrorY 是否沿 Y 轴镜像。
     * @return 变换后的坐标。
     */
    static QPointF apply(const QPointF& point,
                         const QPointF& origin,
                         double rotationDegrees,
                         bool mirrorX = false,
                         bool mirrorY = false);
};

}  // namespace EasyKiConverter::Parser
