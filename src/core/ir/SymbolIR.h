#pragma once

/**
 * @file SymbolIR.h
 * @brief 原理图符号的通用中间表示
 *
 * 替代 SymbolData.h 中的结构体，移除所有 EasyEDA 特有格式：
 * - 坐标以 QList<QPointF> 存储（替代原始字符串）
 * - 引脚类型/方向使用枚举（替代整数）
 * - 文本属性使用标准类型（替代字符串编码）
 */

#include "IRTypes.h"

#include <QByteArray>
#include <QColor>
#include <QList>
#include <QMap>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QStringList>

namespace EasyKiConverter {
namespace IR {

/**
 * @brief 引脚样式（电气语义层）
 *
 * 描述引脚的电气语义特征，与具体 EDA 格式的图形表示解耦。
 * 各导出器根据自身能力将这些语义映射为图形装饰。
 */
struct SymbolPinStyle {
    bool inverted = false;  ///< 取反（低电平有效）
    bool clock = false;  ///< 时钟信号
    bool activeLow = false;  ///< 低电平有效（语义标记，与 inverted 不同）
    PinDecoration decoration = PinDecoration::None;  ///< IEEE 图形装饰

    /**
     * @brief 判断是否存在任一样式标记。
     * @return 存在取反、时钟、低有效或图形装饰时返回 true。
     */
    bool hasAny() const {
        return inverted || clock || activeLow || decoration != PinDecoration::None;
    }
};

/**
 * @brief 引脚显示控制
 *
 * 控制引脚名称和编号的显示方式，包括锚点和字体信息。
 * 适用于需要精确控制文本位置的格式（如 Altium）。
 */
struct SymbolPinDisplay {
    bool showName = true;  ///< 是否显示引脚名称
    bool showDesignator = true;  ///< 是否显示引脚编号
    TextAnchor nameAnchor = TextAnchor::Start;  ///< 名称文本锚点
    TextAnchor numberAnchor = TextAnchor::Start;  ///< 编号文本锚点
    TextOrientation nameOrientation = TextOrientation::Horizontal;  ///< 名称文本方向
    TextOrientation numberOrientation = TextOrientation::Horizontal;  ///< 编号文本方向
    double nameFontSizeMm = 0.0;  ///< 名称字体大小（mm，0 表示默认）
    double numberFontSizeMm = 0.0;  ///< 编号字体大小（mm，0 表示默认）
};

/**
 * @brief 通用符号引脚
 *
 * 替代 SymbolPin 及其子结构体（SymbolPinSettings, SymbolPinDot,
 * SymbolPinPath, SymbolPinName, SymbolPinDotBis, SymbolPinClock）。
 * 所有字段均为解析后的最终值，不再存储原始字符串。
 *
 * 结构分为三层：
 * - 几何：position, length, direction
 * - 电气语义：electricalType, style
 * - 显示控制：display
 */
struct SymbolPinIR {
    // === 几何 ===
    QString name;  ///< 引脚名称（如 "VCC", "A0"）
    QString designator;  ///< 引脚编号（如 "1", "2"）
    QPointF position;  ///< 引脚位置（已解析，单位 mm）
    QPointF namePosition;  ///< 引脚名称原始位置（已解析，单位 mm）
    double nameRotation = 0.0;  ///< 引脚名称旋转角度
    QString nameAnchor;  ///< 引脚名称原始文本锚点
    double nameFontSizeMm = 0.0;  ///< 引脚名称字体大小（mm，0 表示默认）
    bool hasNamePosition = false;  ///< 是否存在可靠的原始名称坐标
    QPointF numberPosition;  ///< 引脚编号原始位置（已解析，单位 mm）
    double numberRotation = 0.0;  ///< 引脚编号旋转角度
    QString numberAnchor;  ///< 引脚编号原始文本锚点
    double numberFontSizeMm = 0.0;  ///< 引脚编号字体大小（mm，0 表示默认）
    bool hasNumberPosition = false;  ///< 是否存在可靠的原始编号坐标
    double length = 0.0;  ///< 引脚长度（mm，替代 SVG path 解析）
    PinDirection direction = PinDirection::Right;  ///< 引脚方向

    // === 电气语义 ===
    PinElectricalType electricalType = PinElectricalType::Unspecified;  ///< 电气类型
    SymbolPinStyle style;  ///< 引脚样式（语义层）

    // === 显示控制 ===
    SymbolPinDisplay display;  ///< 引脚显示控制

    // === 兼容性字段（过渡期，新代码请使用 style/display） ===
    bool showName = true;  ///< @deprecated 使用 display.showName
    bool showDesignator = true;  ///< @deprecated 使用 display.showDesignator
    bool hasDot = false;  ///< @deprecated 使用 style.inverted
    bool hasClock = false;  ///< @deprecated 使用 style.clock

    // === 部件 ===
    int partIndex = 0;  ///< 所属部件索引（多部件符号使用）
    bool commonToAllParts = false;  ///< 是否属于多部件符号的公共 Part Zero
};

/**
 * @brief 通用符号矩形
 * @note 使用未归一化的起点/终点坐标，而非 QRectF（QRectF 会归一化负宽高导致坐标信息丢失）
 */
struct SymbolRectangleIR {
    double x0 = 0.0;  ///< 起点 X（mm，已转换）
    double y0 = 0.0;  ///< 起点 Y（mm，已转换，KiCad Y 翻转后）
    double x1 = 0.0;  ///< 终点 X（mm，已转换）
    double y1 = 0.0;  ///< 终点 Y（mm，已转换，KiCad Y 翻转后）
    double cornerRadiusX = 0.0;  ///< X 方向圆角半径（mm）
    double cornerRadiusY = 0.0;  ///< Y 方向圆角半径（mm）
    QColor strokeColor = Qt::black;  ///< 边框颜色
    double strokeWidth = 0.0;  ///< 边框宽度（mm）
    StrokeStyle strokeStyle = StrokeStyle::Solid;  ///< 线型
    QColor fillColor = Qt::transparent;  ///< 填充颜色
    bool isFilled = false;  ///< 是否填充
    int partIndex = 0;  ///< 所属部件索引（多部件符号使用）
};

/**
 * @brief 通用符号圆形
 */
struct SymbolCircleIR {
    QPointF center;  ///< 圆心（已解析，单位 mm）
    double radius = 0.0;  ///< 半径（mm）
    QColor strokeColor = Qt::black;  ///< 边框颜色。
    double strokeWidth = 0.0;  ///< 边框宽度，单位为毫米。
    StrokeStyle strokeStyle = StrokeStyle::Solid;  ///< 边框线型。
    QColor fillColor = Qt::transparent;  ///< 填充颜色。
    bool isFilled = false;  ///< 是否填充图形。
    int partIndex = 0;  ///< 所属部件索引（多部件符号使用）
};

/**
 * @brief 通用符号圆弧
 *
 * @note EasyEDA 的 SymbolArc.path 存储 3 个 QPointF（起点、中点、终点），
 *       保留原始三点表示以便直接生成 KiCad 三点式圆弧。
 */
struct SymbolArcIR {
    QPointF startPoint;  ///< 起点（已解析，单位 mm）
    QPointF midPoint;  ///< 中点（已解析，单位 mm）
    QPointF endPoint;  ///< 终点（已解析，单位 mm）
    QColor strokeColor = Qt::black;  ///< 边框颜色。
    double strokeWidth = 0.0;  ///< 边框宽度，单位为毫米。
    StrokeStyle strokeStyle = StrokeStyle::Solid;  ///< 边框线型。
    QColor fillColor = Qt::transparent;  ///< 填充颜色。
    bool isFilled = false;  ///< 是否填充图形。
    int partIndex = 0;  ///< 所属部件索引（多部件符号使用）
};

/**
 * @brief 通用符号椭圆
 */
struct SymbolEllipseIR {
    QPointF center;  ///< 中心点（已解析，单位 mm）
    double radiusX = 0.0;  ///< X 轴半径（mm）
    double radiusY = 0.0;  ///< Y 轴半径（mm）
    QColor strokeColor = Qt::black;  ///< 边框颜色。
    double strokeWidth = 0.0;  ///< 边框宽度，单位为毫米。
    StrokeStyle strokeStyle = StrokeStyle::Solid;  ///< 边框线型。
    QColor fillColor = Qt::transparent;  ///< 填充颜色。
    bool isFilled = false;  ///< 是否填充图形。
    int partIndex = 0;  ///< 所属部件索引（多部件符号使用）
};

/**
 * @brief 通用符号扇形
 * @details 写入 Altium RECORD=9，角度单位为度。
 */
struct SymbolPieIR {
    QPointF center;  ///< 圆心（mm）
    double radius = 0.0;  ///< 半径（mm）
    double startAngle = 0.0;  ///< 起始角度（度）
    double endAngle = 360.0;  ///< 结束角度（度）
    QColor strokeColor = Qt::black;  ///< 边框颜色。
    double strokeWidth = 0.0;  ///< 边框宽度，单位为毫米。
    StrokeStyle strokeStyle = StrokeStyle::Solid;  ///< 边框线型。
    QColor fillColor = Qt::transparent;  ///< 填充颜色。
    bool isFilled = false;  ///< 是否填充图形。
    int partIndex = 0;  ///< 所属部件索引
};

/**
 * @brief 通用符号椭圆弧
 * @details 写入 Altium RECORD=11，角度单位为度。
 */
struct SymbolEllipticalArcIR {
    QPointF center;  ///< 中心点（mm）
    double radiusX = 0.0;  ///< X 方向半径（mm）
    double radiusY = 0.0;  ///< Y 方向半径（mm）
    double startAngle = 0.0;  ///< 起始角度（度）
    double endAngle = 360.0;  ///< 结束角度（度）
    QColor strokeColor = Qt::black;  ///< 边框颜色。
    double strokeWidth = 0.0;  ///< 边框宽度，单位为毫米。
    StrokeStyle strokeStyle = StrokeStyle::Solid;  ///< 边框线型。
    QColor fillColor = Qt::transparent;  ///< 填充颜色。
    bool isFilled = false;  ///< 是否填充图形。
    int partIndex = 0;  ///< 所属部件索引
};

/**
 * @brief 通用符号折线
 * @note 替代 SymbolPolyline::points 原始字符串，坐标已解析
 */
struct SymbolPolylineIR {
    QList<QPointF> points;  ///< 顶点列表（已解析，单位 mm）
    QColor strokeColor = Qt::black;  ///< 边框颜色。
    double strokeWidth = 0.0;  ///< 边框宽度，单位为毫米。
    StrokeStyle strokeStyle = StrokeStyle::Solid;  ///< 边框线型。
    QColor fillColor = Qt::transparent;  ///< 填充颜色。
    bool isFilled = false;  ///< 是否填充图形。
    int partIndex = 0;  ///< 所属部件索引（多部件符号使用）
};

/**
 * @brief 通用符号多边形
 * @note 替代 SymbolPolygon::points 原始字符串，坐标已解析
 */
struct SymbolPolygonIR {
    QList<QPointF> points;  ///< 顶点列表（已解析，单位 mm）
    QColor strokeColor = Qt::black;  ///< 边框颜色。
    double strokeWidth = 0.0;  ///< 边框宽度，单位为毫米。
    StrokeStyle strokeStyle = StrokeStyle::Solid;  ///< 边框线型。
    QColor fillColor = Qt::transparent;  ///< 填充颜色。
    bool isFilled = false;  ///< 是否填充多边形。
    int partIndex = 0;  ///< 所属部件索引（多部件符号使用）
};

/**
 * @brief 通用符号路径段
 * @details 二次 Bézier、圆形弧和未旋转椭圆弧保留为原生段，旋转椭圆弧
 *          由解析器转换为分段三次 Bézier，以兼容目标格式的曲线记录。
 */
struct SymbolPathSegmentIR {
    enum class Type { Line, QuadraticBezier, CubicBezier, CircularArc, EllipticalArc };

    Type type = Type::Line;  ///< 路径段类型。
    QPointF start;  ///< 路径段起点。
    QPointF control1;  ///< 二次曲线控制点，或三次曲线第一个控制点
    QPointF control2;  ///< 三次曲线第二个控制点。
    QPointF arcMid;  ///< 圆弧中点。
    QPointF arcCenter;  ///< 圆弧中心。
    double radiusX = 0.0;  ///< 椭圆弧 X 轴半径，单位为毫米。
    double radiusY = 0.0;  ///< 椭圆弧 Y 轴半径，单位为毫米。
    double arcStartAngle = 0.0;  ///< 圆弧起始角度，单位为度。
    double arcEndAngle = 0.0;  ///< 圆弧终止角度，单位为度。
    QPointF end;  ///< 路径段终点。
};

/**
 * @brief 通用符号路径
 * @note 同时保留点列兼容 KiCad 等导出器，并提供段列供支持原生曲线的导出器使用。
 */
struct SymbolPathIR {
    QList<QPointF> points;  ///< 路径坐标序列（已解析，单位 mm）
    QList<SymbolPathSegmentIR> segments;  ///< 原生直线/二次与三次 Bézier/圆弧段（已解析，单位 mm）
    QColor strokeColor = Qt::black;  ///< 边框颜色。
    double strokeWidth = 0.0;  ///< 边框宽度，单位为毫米。
    StrokeStyle strokeStyle = StrokeStyle::Solid;  ///< 边框线型。
    QColor fillColor = Qt::transparent;  ///< 填充颜色。
    bool isFilled = false;  ///< 是否填充图形。
    int partIndex = 0;  ///< 所属部件索引（多部件符号使用）
};

/**
 * @brief 通用符号三次 Bézier 曲线
 * @details 按 Altium RECORD=5 的四个控制点保存，坐标单位为 mm。
 */
struct SymbolBezierIR {
    QList<QPointF> controlPoints;  ///< 起点、两个控制点和终点
    QColor strokeColor = Qt::black;  ///< 边框颜色。
    double strokeWidth = 0.0;  ///< 边框宽度，单位为毫米。
    StrokeStyle strokeStyle = StrokeStyle::Solid;  ///< 边框线型。
    int partIndex = 0;  ///< 所属部件索引（多部件符号使用）
};

/**
 * @brief 通用 Altium IEEE 图形
 * @details `symbol` 使用 Altium RECORD=3 的 TIeeeSymbol 编号，位置单位为 mm。
 */
struct SymbolIeeeIR {
    int symbol = 0;  ///< Altium IEEE 图形编号（0-34）
    QPointF position;  ///< 图形锚点（mm）
    int scaleFactor = 10;  ///< Altium 图形缩放因子
    int orientation = 0;  ///< 0-3，表示 0°/90°/180°/270°
    bool mirrored = false;  ///< 是否镜像
    QColor color = Qt::black;  ///< 图形颜色。
    int partIndex = 0;  ///< 所属部件索引（多部件符号使用）
};

/**
 * @brief 通用符号文本
 * @note 颜色、字体大小已标准化
 */
struct SymbolTextIR {
    QString text;  ///< 文本内容
    QPointF position;  ///< 位置（已解析，单位 mm）
    double rotation = 0.0;  ///< 旋转角度（度）
    QColor color = Qt::black;  ///< 文本颜色
    QString fontFamily;  ///< 字体名称
    double fontSizeMm = 0.0;  ///< 字体大小（mm，已从 pt 转换）
    bool bold = false;  ///< 是否粗体
    bool italic = false;  ///< 是否斜体（替代原始字符串 "1"/"Italic"）
    bool visible = true;  ///< 是否可见
    QString anchor = QStringLiteral("middle");  ///< 文本锚点（如 start、middle、end）
    int partIndex = 0;  ///< 所属部件索引（多部件符号使用）
};

/**
 * @brief 通用符号文本框
 * @details 写入 Altium RECORD=28，可同时表达边框、填充和多行文本。
 */
struct SymbolTextFrameIR {
    double x0 = 0.0, y0 = 0.0;  ///< 第一角点（mm）
    double x1 = 0.0, y1 = 0.0;  ///< 第二角点（mm）
    QString text;  ///< 多行文本
    QColor strokeColor = Qt::black;  ///< 边框颜色。
    QColor fillColor = Qt::transparent;  ///< 填充颜色。
    QColor textColor = Qt::black;  ///< 文本颜色。
    double strokeWidth = 0.0;  ///< 边框宽度，单位为毫米。
    StrokeStyle strokeStyle = StrokeStyle::Solid;  ///< 边框线型。
    double textMargin = 0.0;  ///< 文本边距（mm）
    QString fontFamily;  ///< 字体族；为空时使用 Altium 默认字体
    double fontSizeMm = 0.0;  ///< 字体大小（mm，0 表示使用字体编号默认值）
    bool bold = false;  ///< 是否粗体
    bool italic = false;  ///< 是否斜体
    int fontId = 0;  ///< 目标格式字体编号。
    int orientation = 0;  ///< 文本框方向编号。
    int alignment = 0;  ///< 文本对齐方式编号。
    bool isFilled = false;  ///< 是否填充文本框。
    bool showBorder = false;  ///< 是否显示文本框边框。
    bool wordWrap = false;  ///< 是否自动换行。
    bool clipToRect = false;  ///< 是否裁剪到文本框边界。
    bool transparent = false;  ///< 是否使用透明背景。
    int partIndex = 0;  ///< 所属部件索引。
};

/**
 * @brief 通用符号图片
 * @details 支持链接图片和写入 SchLib /Storage 的嵌入图片。
 */
struct SymbolImageIR {
    double x0 = 0.0, y0 = 0.0;  ///< 边界第一角点（mm）
    double x1 = 0.0, y1 = 0.0;  ///< 边界第二角点（mm）
    double rotation = 0.0;  ///< 旋转角度（度）
    QString fileName;  ///< 图片文件名或外部路径
    QByteArray data;  ///< 图片原始字节；非空时可嵌入
    QColor strokeColor = Qt::black;  ///< 图片边框颜色。
    QColor fillColor = Qt::transparent;  ///< 图片填充颜色。
    double strokeWidth = 0.0;  ///< 图片边框宽度，单位为毫米。
    StrokeStyle strokeStyle = StrokeStyle::Solid;  ///< 图片边框线型。
    bool isFilled = false;  ///< 是否填充图片区域。
    bool showBorder = false;  ///< 是否显示图片边框。
    bool keepAspect = true;  ///< 是否保持图片宽高比。
    bool transparent = false;  ///< 是否使用透明背景。
    int partIndex = 0;  ///< 所属部件索引。
};

/**
 * @brief 符号参数字段
 * @details 用于表达 Value、Datasheet 以及供应商自定义属性，参数名称和值
 *          与图形数据分离，便于各导出器按目标格式映射。
 */
struct SymbolParameterIR {
    QString name;  ///< 参数名称
    QString value;  ///< 符号属性对应的文本值。
    bool visible = false;  ///< 是否在符号图面显示
    bool readOnly = false;  ///< 是否禁止在目标 EDA 中编辑
    QPointF position;  ///< 参数位置（mm）
    double fontSizeMm = 0.0;  ///< 字体大小（mm，0 表示默认）
    QColor color = Qt::black;  ///< 参数颜色
    double rotation = 0.0;  ///< 旋转角度（度）
    int partIndex = 0;  ///< 所属部件索引
};

/**
 * @brief 符号关联模型
 * @details 用于封装、SPICE、STEP、VRML 等目标格式模型的统一描述。
 */
struct SymbolModelIR {
    QString name;  ///< 模型名称
    QString type = QStringLiteral("PCBLIB");  ///< 模型类型
    QString fileKind;  ///< 数据文件类型
    QString fileEntity;  ///< 数据文件实体或路径
    QMap<QString, QString> parameters;  ///< 模型参数
    QMap<QString, QString> pinMappings;  ///< 符号引脚到模型引脚的映射
};

/**
 * @brief IR 中的符号图元顺序引用
 * @details index 是对应图元类型在指定部件中的局部索引。
 */
struct SymbolGraphicOrderIR {
    QString type;  ///< 图元类型名称。
    int index = -1;  ///< 图元在对应列表中的局部索引。
    int partIndex = 0;  ///< 所属部件索引。
};

/**
 * @brief 通用符号组件
 *
 * 包含一个符号的所有图形原语和引脚。
 * 不包含任何 EDA 平台特有的元数据字段。
 */
struct SymbolComponentIR {
    QString name;  ///< 组件名称
    QString description;  ///< 组件描述
    QString designatorPrefix;  ///< 位号前缀（"U", "R", "C" 等）
    int partCount = 1;  ///< 部件数（多部件符号）
    double originX = 0.0;  ///< 原点 X 坐标（mm）
    double originY = 0.0;  ///< 原点 Y 坐标（mm）
    bool preserveLogicalOrigin = false;  ///< 坐标已按来源逻辑原点归一化，导出器不得再次按图形居中

    QList<SymbolPinIR> pins;  ///< 符号引脚列表。
    QList<SymbolRectangleIR> rectangles;  ///< 矩形图元列表。
    QList<SymbolCircleIR> circles;  ///< 圆形图元列表。
    QList<SymbolArcIR> arcs;  ///< 圆弧图元列表。
    QList<SymbolEllipseIR> ellipses;  ///< 椭圆图元列表。
    QList<SymbolPieIR> pies;  ///< 扇形图元列表。
    QList<SymbolEllipticalArcIR> ellipticalArcs;  ///< 椭圆弧图元列表。
    QList<SymbolPolylineIR> polylines;  ///< 折线图元列表。
    QList<SymbolPolygonIR> polygons;  ///< 多边形图元列表。
    QList<SymbolPathIR> paths;  ///< 路径图元列表。
    QList<SymbolBezierIR> beziers;  ///< 三次 Bézier 图元列表。
    QList<SymbolIeeeIR> ieeeSymbols;  ///< IEEE 图形列表。
    QList<SymbolTextIR> texts;  ///< 文本图元列表。
    QList<SymbolTextFrameIR> textFrames;  ///< 文本框图元列表。
    QList<SymbolImageIR> images;  ///< 图片图元列表。
    QList<SymbolParameterIR> parameters;  ///< 参数字段列表。
    QList<SymbolModelIR> models;  ///< 关联模型列表。
    QList<SymbolGraphicOrderIR> graphicOrder;  ///< 图元原始顺序列表。

    QString footprintName;  ///< 首选封装关联名称。
    QStringList footprintNames;  ///< 候选封装名称列表。
    QStringList aliases;  ///< 符号别名列表。
    QMap<QString, QString> sourceMetadata;  ///< 来源平台特有元数据。

    /**
     * @brief 判断是否为多部件符号。
     * @return 部件数量大于一时返回 true。
     */
    bool isMultiPart() const {
        return partCount > 1;
    }

    /**
     * @brief 判断是否包含任何图形数据。
     * @return 任一图元列表非空时返回 true。
     */
    bool hasGraphics() const {
        return !pins.isEmpty() || !rectangles.isEmpty() || !circles.isEmpty() || !arcs.isEmpty() ||
               !ellipses.isEmpty() || !pies.isEmpty() || !ellipticalArcs.isEmpty() || !polylines.isEmpty() ||
               !polygons.isEmpty() || !paths.isEmpty() || !beziers.isEmpty() || !ieeeSymbols.isEmpty() ||
               !texts.isEmpty() || !textFrames.isEmpty() || !images.isEmpty();
    }

    /**
     * @brief 清空符号组件的名称、几何数据和多单元状态。
     */
    void clear() {
        name.clear();
        description.clear();
        designatorPrefix.clear();
        partCount = 1;
        originX = originY = 0.0;
        preserveLogicalOrigin = false;
        pins.clear();
        rectangles.clear();
        circles.clear();
        arcs.clear();
        ellipses.clear();
        pies.clear();
        ellipticalArcs.clear();
        polylines.clear();
        polygons.clear();
        paths.clear();
        beziers.clear();
        ieeeSymbols.clear();
        texts.clear();
        textFrames.clear();
        images.clear();
        parameters.clear();
        models.clear();
        graphicOrder.clear();
        footprintName.clear();
        footprintNames.clear();
        aliases.clear();
        sourceMetadata.clear();
    }
};

}  // namespace IR
}  // namespace EasyKiConverter
