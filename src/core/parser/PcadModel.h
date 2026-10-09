#pragma once

/**
 * @file PcadModel.h
 * @brief P-CAD ASCII PCB 的格式专用模型和解析入口。
 */

#include "GeometryTransforms.h"
#include "ParseDiagnostics.h"
#include "TextParsers.h"

#include <QByteArray>
#include <QList>
#include <QPointF>
#include <QString>

namespace EasyKiConverter::Parser {

/**
 * @brief P-CAD 焊盘形状。
 */
enum class PcadPadShape { Round, Rectangle, Oval, RoundRectangle, Unknown };

/**
 * @brief P-CAD 图形类型。
 */
enum class PcadGraphicType { Line, Arc, Circle, Polygon, Text, Unknown };

/**
 * @brief P-CAD 层的统一来源描述。
 */
struct PcadLayer {
    int number = 0;  ///< 层编号。
    QString name;  ///< 层名称。
};

/**
 * @brief P-CAD Pad Style 定义。
 */
struct PcadPadStyle {
    QString name;  ///< Pad Style 名称。
    PcadPadShape shape = PcadPadShape::Unknown;  ///< Pad 形状。
    double width = 0.0;  ///< Pad 宽度。
    double height = 0.0;  ///< Pad 高度。
    double holeDiameter = 0.0;  ///< 孔直径。
    bool holePlated = true;  ///< 是否镀孔。
};

/**
 * @brief P-CAD Pattern 中的焊盘实例。
 */
struct PcadPatternPad {
    QString number;  ///< 焊盘编号。
    QString padStyleName;  ///< 关联 Pad Style 名称。
    QPointF position;  ///< 焊盘位置。
    double rotation = 0.0;  ///< 焊盘旋转角度。
};

/**
 * @brief P-CAD Pattern 或板级图形。
 */
struct PcadGraphic {
    PcadGraphicType type = PcadGraphicType::Unknown;  ///< 图形类型。
    QList<QPointF> points;  ///< 图形顶点。
    QPointF center;  ///< 圆弧、圆或文本中心。
    double radius = 0.0;  ///< 半径。
    double width = 0.0;  ///< 线宽。
    double rotation = 0.0;  ///< 图形旋转角度。
    double textHeight = 0.0;  ///< 文本高度。
    double startAngle = 0.0;  ///< 起始角度。
    double sweepAngle = 0.0;  ///< 扫掠角度。
    QString text;  ///< 文本内容。
    int layerNumber = 0;  ///< 所在层编号。
};

/**
 * @brief P-CAD 封装 Pattern。
 */
struct PcadPattern {
    QString name;  ///< Pattern 名称。
    QList<PcadPatternPad> pads;  ///< Pattern 焊盘实例。
    QList<PcadGraphic> graphics;  ///< Pattern 图形。
};

/**
 * @brief P-CAD PCB 中的器件放置实例。
 */
struct PcadPlacement {
    QString reference;  ///< 器件参考标号。
    QString patternName;  ///< 关联 Pattern 名称。
    QPointF position;  ///< 放置位置。
    double rotation = 0.0;  ///< 放置旋转角度。
    bool flipped = false;  ///< 是否镜像放置。
};

/**
 * @brief P-CAD ASCII PCB 的完整格式模型。
 */
struct PcadBoard {
    QString name;  ///< 板级名称。
    LengthUnit unit = LengthUnit::Mil;  ///< 板级长度单位。
    QList<PcadLayer> layers;  ///< 层定义。
    QList<PcadPadStyle> padStyles;  ///< Pad Style 定义。
    QList<PcadPattern> patterns;  ///< Pattern 定义。
    QList<PcadPlacement> placements;  ///< 器件放置实例。
    QList<PcadGraphic> graphics;  ///< 板级图形。
    ParseDiagnostics diagnostics;  ///< 解析诊断。

    /**
     * @brief 判断输入是否包含可转换的 P-CAD PCB 定义。
     */
    bool isRecognized() const;
};

/**
 * @brief 解析 P-CAD ASCII PCB 的 Pad Style、Pattern、图形和器件放置。
 * @details 解析结果只使用 P-CAD 专用模型，统一 IR 转换由独立 Adapter 完成。
 */
class PcadParser {
public:
    /**
     * @brief 从内存文本解析 P-CAD ASCII PCB。
     * @param content UTF-8 或已完成编码转换的文本。
     * @param filePath 可选源文件路径。
     * @return P-CAD 格式模型和结构化诊断。
     */
    static PcadBoard parse(const QString& content, const QString& filePath = QString());

    /**
     * @brief 从原始字节检测编码后解析 P-CAD ASCII PCB。
     * @param data 原始文件字节。
     * @param filePath 可选源文件路径。
     * @return P-CAD 格式模型和编码、语法诊断。
     */
    static PcadBoard parseBytes(const QByteArray& data, const QString& filePath = QString());
};

}  // namespace EasyKiConverter::Parser
