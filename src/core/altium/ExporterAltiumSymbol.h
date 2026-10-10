#pragma once

#include "core/interfaces/ISymbolExporter.h"
#include "core/ir/SymbolIR.h"
#include "models/AltiumSchComponent.h"
#include "writers/AltiumSchLibWriter.h"

namespace EasyKiConverter {

/**
 * @brief Altium 符号导出器
 * @details 实现 ISymbolExporter 接口，将 SymbolComponentIR 转换为 Altium SchLib 格式
 */
class ExporterAltiumSymbol : public ISymbolExporter {
public:
    /**
     * @brief 获取符号库文件扩展名
     * @return ".SchLib"
     */
    QString libraryFileExtension() const override {
        return QStringLiteral(".SchLib");
    }

    /**
     * @brief 获取最近一次 SchLib 导出的非致命诊断
     */
    QStringList diagnostics() const override {
        return m_diagnostics;
    }

    /**
     * @brief 导出单个符号到 SchLib 文件。
     * @param symbol 待导出的符号 IR。
     * @param filePath 输出文件路径。
     * @return 导出是否成功。
     */
    bool exportSymbol(const IR::SymbolComponentIR& symbol, const QString& filePath) override;

    /**
     * @brief 导出符号库（多个符号合并到一个 SchLib 文件）。
     * @param symbols 待导出的符号列表。
     * @param libName 输出库名称。
     * @param filePath 输出文件路径。
     * @param appendMode 是否请求追加模式。
     * @param updateMode 是否请求更新模式。
     * @param libraryDescription 库描述。
     * @return 导出是否成功。
     */
    bool exportSymbolLibrary(const QList<IR::SymbolComponentIR>& symbols,
                             const QString& libName,
                             const QString& filePath,
                             bool appendMode = true,
                             bool updateMode = false,
                             const QString& libraryDescription = QString()) override;

private:
    /**
     * @brief 将 SymbolComponentIR 转换为 AltiumSchComponent。
     * @param data 符号组件 IR。
     * @return 转换后的 Altium 符号组件。
     */
    AltiumSchComponent convertSymbol(const IR::SymbolComponentIR& data);

    /**
     * @brief 将 SymbolPinIR 转换为 AltiumSchPin。
     * @param pin 符号引脚 IR。
     * @return 转换后的 Altium 引脚。
     */
    AltiumSchPin convertPin(const IR::SymbolPinIR& pin);

    /**
     * @brief 将 SymbolRectangleIR 转换为 AltiumSchRectangle。
     * @param rect 矩形图元 IR。
     * @return 转换后的 Altium 矩形。
     */
    AltiumSchRectangle convertRectangle(const IR::SymbolRectangleIR& rect);

    /**
     * @brief 将 SymbolRectangleIR 转换为 AltiumSchRoundRectangle。
     * @param rect 圆角矩形图元 IR。
     * @return 转换后的 Altium 圆角矩形。
     */
    AltiumSchRoundRectangle convertRoundRectangle(const IR::SymbolRectangleIR& rect);

    /**
     * @brief 将 SymbolCircleIR 转换为 AltiumSchEllipse。
     * @param circle 圆形图元 IR。
     * @return 转换后的 Altium 椭圆。
     */
    AltiumSchEllipse convertCircle(const IR::SymbolCircleIR& circle);

    /**
     * @brief 将 SymbolArcIR 转换为 AltiumSchArc。
     * @param arc 圆弧图元 IR。
     * @return 转换后的 Altium 圆弧。
     */
    AltiumSchArc convertArc(const IR::SymbolArcIR& arc);

    /**
     * @brief 将 SymbolPolygonIR 转换为 AltiumSchPolygon。
     * @param polygon 多边形图元 IR。
     * @return 转换后的 Altium 多边形。
     */
    AltiumSchPolygon convertPolygon(const IR::SymbolPolygonIR& polygon);

    /**
     * @brief 将 SymbolPolylineIR 转换为 AltiumSchPolyline。
     * @param polyline 折线图元 IR。
     * @return 转换后的 Altium 折线。
     */
    AltiumSchPolyline convertPolyline(const IR::SymbolPolylineIR& polyline);

    /**
     * @brief 将 SymbolPathIR 转换为 AltiumSchPath。
     * @param path 路径图元 IR。
     * @return 转换后的 Altium 路径。
     */
    AltiumSchPath convertPath(const IR::SymbolPathIR& path);

    /**
     * @brief 将 SymbolBezierIR 转换为 AltiumSchBezier。
     * @param bezier Bézier 图元 IR。
     * @return 转换后的 Altium Bézier 曲线。
     */
    AltiumSchBezier convertBezier(const IR::SymbolBezierIR& bezier);

    /**
     * @brief 将 SymbolIeeeIR 转换为 AltiumSchIeee。
     * @param ieee IEEE 图元 IR。
     * @return 转换后的 Altium IEEE 图元。
     */
    AltiumSchIeee convertIeee(const IR::SymbolIeeeIR& ieee);

    /**
     * @brief 将 SymbolTextIR 转换为 AltiumSchText。
     * @param text 文本图元 IR。
     * @return 转换后的 Altium 文本。
     */
    AltiumSchText convertText(const IR::SymbolTextIR& text);

    /**
     * @brief 将 SymbolTextFrameIR 转换为 AltiumSchTextFrame。
     * @param frame 文本框图元 IR。
     * @return 转换后的 Altium 文本框。
     */
    AltiumSchTextFrame convertTextFrame(const IR::SymbolTextFrameIR& frame);

    /**
     * @brief 将 SymbolImageIR 转换为 AltiumSchImage。
     * @param image 图片图元 IR。
     * @return 转换后的 Altium 图片。
     */
    AltiumSchImage convertImage(const IR::SymbolImageIR& image);

    /**
     * @brief 将 SymbolEllipseIR 转换为 AltiumSchEllipse。
     * @param ellipse 椭圆图元 IR。
     * @return 转换后的 Altium 椭圆。
     */
    AltiumSchEllipse convertEllipse(const IR::SymbolEllipseIR& ellipse);

    /**
     * @brief 将 SymbolPieIR 转换为 AltiumSchPie。
     * @param pie 扇形图元 IR。
     * @return 转换后的 Altium 扇形。
     */
    AltiumSchPie convertPie(const IR::SymbolPieIR& pie);

    /**
     * @brief 将 SymbolEllipticalArcIR 转换为 AltiumSchEllipticalArc。
     * @param arc 椭圆弧图元 IR。
     * @return 转换后的 Altium 椭圆弧。
     */
    AltiumSchEllipticalArc convertEllipticalArc(const IR::SymbolEllipticalArcIR& arc);

    /**
     * @brief 将符号图元坐标归一化到原点。
     * @param component 待归一化的 Altium 符号组件。
     */
    void centerComponent(AltiumSchComponent& component);

    AltiumSchLibWriter m_writer;  ///< Altium SchLib 文件写入器。
    QStringList m_diagnostics;  ///< 最近一次导出的非致命诊断信息。
};

}  // namespace EasyKiConverter
