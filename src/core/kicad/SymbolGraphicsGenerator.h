#ifndef SYMBOLGRAPHICSGENERATOR_H
#define SYMBOLGRAPHICSGENERATOR_H

#include "core/ir/SymbolIR.h"

#include <QString>

namespace EasyKiConverter {

/**
 * @brief 符号图形元素生成器
 *
 * 从 ExporterSymbol 中提取的图形生成函数，
 * 负责将各类符号图形元素转换为 KiCad 格式文本。
 */
class SymbolGraphicsGenerator {
public:
    SymbolGraphicsGenerator() = default;

    /**
     * @brief 设置当前原点偏移（用于坐标计算）
     *
     * @param originX 原点 X 坐标（mm）
     * @param originY 原点 Y 坐标（mm）
     */
    void setCurrentOrigin(double originX, double originY) {
        m_originX = originX;
        m_originY = originY;
    }

    /**
     * @brief 返回当前原点 X 坐标。
     * @return 原点 X 坐标，单位为毫米。
     */
    double originX() const {
        return m_originX;
    }

    /**
     * @brief 返回当前原点 Y 坐标。
     * @return 原点 Y 坐标，单位为毫米。
     */
    double originY() const {
        return m_originY;
    }

    // 批量生成函数
    QString generateDrawings(const IR::SymbolComponentIR& data) const;
    QString generatePins(const QList<IR::SymbolPinIR>& pins) const;

    // 单个图形元素生成函数
    QString generatePin(const IR::SymbolPinIR& pin) const;
    QString generateRectangle(const IR::SymbolRectangleIR& rect) const;
    QString generateCircle(const IR::SymbolCircleIR& circle) const;
    QString generateArc(const IR::SymbolArcIR& arc) const;
    QString generateEllipse(const IR::SymbolEllipseIR& ellipse) const;
    QString generatePolygon(const IR::SymbolPolygonIR& polygon) const;
    QString generatePolyline(const IR::SymbolPolylineIR& polyline) const;
    QString generatePath(const IR::SymbolPathIR& path) const;
    QString generateText(const IR::SymbolTextIR& text) const;

private:
    double m_originX = 0.0;  ///< 当前原点 X（mm）
    double m_originY = 0.0;  ///< 当前原点 Y（mm）
};

}  // namespace EasyKiConverter

#endif  // SYMBOLGRAPHICSGENERATOR_H
