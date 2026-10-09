#pragma once

#include "core/ir/SymbolIR.h"
#include "models/AltiumSchComponent.h"

namespace EasyKiConverter {

/**
 * @brief 将 IR 曲线图元转换为 Altium SchLib 记录。
 * @details 转换器不保存状态，仅负责坐标、线型、颜色和部件归属映射。
 */
class AltiumSymbolCurveConverter final {
public:
    /**
     * @brief 转换三点圆弧。
     * @param arc 待转换圆弧。
     * @return Altium 圆弧记录。
     */
    static AltiumSchArc convertArc(const IR::SymbolArcIR& arc);

    /**
     * @brief 转换多边形。
     * @param polygon 待转换多边形。
     * @return Altium 多边形记录。
     */
    static AltiumSchPolygon convertPolygon(const IR::SymbolPolygonIR& polygon);

    /**
     * @brief 转换折线。
     * @param polyline 待转换折线。
     * @return Altium 折线记录。
     */
    static AltiumSchPolyline convertPolyline(const IR::SymbolPolylineIR& polyline);

    /**
     * @brief 转换路径。
     * @param path 待转换路径。
     * @return Altium 路径记录。
     */
    static AltiumSchPath convertPath(const IR::SymbolPathIR& path);

    /**
     * @brief 转换 Bezier 曲线。
     * @param bezier 待转换曲线。
     * @return Altium Bezier 记录。
     */
    static AltiumSchBezier convertBezier(const IR::SymbolBezierIR& bezier);

    /**
     * @brief 转换椭圆。
     * @param ellipse 待转换椭圆。
     * @return Altium 椭圆记录。
     */
    static AltiumSchEllipse convertEllipse(const IR::SymbolEllipseIR& ellipse);

    /**
     * @brief 转换扇形。
     * @param pie 待转换扇形。
     * @return Altium 扇形记录。
     */
    static AltiumSchPie convertPie(const IR::SymbolPieIR& pie);

    /**
     * @brief 转换椭圆弧。
     * @param arc 待转换椭圆弧。
     * @return Altium 椭圆弧记录。
     */
    static AltiumSchEllipticalArc convertEllipticalArc(const IR::SymbolEllipticalArcIR& arc);
};

}  // namespace EasyKiConverter
