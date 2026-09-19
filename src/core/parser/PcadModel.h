#pragma once

/**
 * @file PcadModel.h
 * @brief P-CAD ASCII PCB 的格式专用模型和解析入口。
 */

#include "GeometryTransforms.h"
#include "ParseDiagnostics.h"
#include "TextParsers.h"

#include <QList>
#include <QPointF>
#include <QString>

namespace EasyKiConverter::Parser {

/** @brief P-CAD 焊盘形状。 */
enum class PcadPadShape { Round, Rectangle, Oval, RoundRectangle, Unknown };

/** @brief P-CAD 图形类型。 */
enum class PcadGraphicType { Line, Arc, Circle, Polygon, Text, Unknown };

/** @brief P-CAD 层的统一来源描述。 */
struct PcadLayer {
    int number = 0;
    QString name;
};

/** @brief P-CAD Pad Style 定义。 */
struct PcadPadStyle {
    QString name;
    PcadPadShape shape = PcadPadShape::Unknown;
    double width = 0.0;
    double height = 0.0;
    double holeDiameter = 0.0;
    bool holePlated = true;
};

/** @brief P-CAD Pattern 中的焊盘实例。 */
struct PcadPatternPad {
    QString number;
    QString padStyleName;
    QPointF position;
    double rotation = 0.0;
};

/** @brief P-CAD Pattern 或板级图形。 */
struct PcadGraphic {
    PcadGraphicType type = PcadGraphicType::Unknown;
    QList<QPointF> points;
    QPointF center;
    double radius = 0.0;
    double width = 0.0;
    double rotation = 0.0;
    double textHeight = 0.0;
    double startAngle = 0.0;
    double sweepAngle = 0.0;
    QString text;
    int layerNumber = 0;
};

/** @brief P-CAD 封装 Pattern。 */
struct PcadPattern {
    QString name;
    QList<PcadPatternPad> pads;
    QList<PcadGraphic> graphics;
};

/** @brief P-CAD PCB 中的器件放置实例。 */
struct PcadPlacement {
    QString reference;
    QString patternName;
    QPointF position;
    double rotation = 0.0;
    bool flipped = false;
};

/** @brief P-CAD ASCII PCB 的完整格式模型。 */
struct PcadBoard {
    QString name;
    LengthUnit unit = LengthUnit::Mil;
    QList<PcadLayer> layers;
    QList<PcadPadStyle> padStyles;
    QList<PcadPattern> patterns;
    QList<PcadPlacement> placements;
    QList<PcadGraphic> graphics;
    ParseDiagnostics diagnostics;

    /** @brief 判断输入是否包含可转换的 P-CAD PCB 定义。 */
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
};

}  // namespace EasyKiConverter::Parser
