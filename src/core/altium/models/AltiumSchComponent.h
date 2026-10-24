#pragma once

#include "AltiumCommon.h"

#include <QByteArray>
#include <QList>
#include <QMap>
#include <QPointF>
#include <QString>
#include <QStringList>

namespace EasyKiConverter {

/**
 * @brief Altium 符号引脚
 */
struct AltiumSchPin {
    QString name;  ///< 显示名称
    QString designator;  ///< 引脚编号
    int locationX = 0;  ///< X 坐标（原始单位）
    int locationY = 0;  ///< Y 坐标（原始单位）
    int length = 100000;  ///< 引脚长度（原始单位，默认 10mil）
    AltiumModels::PinElectricalType electricalType = AltiumModels::PinElectricalType::Passive;  ///< 引脚电气类型。
    AltiumModels::PinOrientation orientation = AltiumModels::PinOrientation::Right;  ///< 引脚延伸方向。
    bool showName = true;  ///< 是否显示引脚名称。
    bool showDesignator = true;  ///< 是否显示引脚编号。
    bool isHidden = false;  ///< 是否隐藏引脚。
    uint32_t color = 0x000000;  ///< 颜色（0x00BBGGRR）
    uint8_t symbolInnerEdge = 0;  ///< 主体内侧装饰（0=None, 3=Clock）
    uint8_t symbolOuterEdge = 0;  ///< 引脚外侧装饰（1=Dot 等）
    uint8_t symbolInside = 0;  ///< 引脚内部装饰
    uint8_t symbolOutside = 0;  ///< 引脚外部装饰
    int ownerPartId = 1;  ///< 所属部件（Altium 使用 1-based 编号）。
    int sourcePartIndex = 0;  ///< 来源部件索引（用于恢复源图元的局部顺序）。
};

/**
 * @brief Altium 符号矩形
 */
struct AltiumSchRectangle {
    int locationX = 0, locationY = 0;  ///< 左上角（原始单位）
    int cornerX = 0, cornerY = 0;  ///< 右下角（原始单位）
    int lineWidth = 0;  ///< 线宽索引 (0-3)
    int lineStyle = 0;  ///< 线型（0 实线、1 虚线、2 点线）
    uint32_t color = 0x000000;  ///< 轮廓颜色。
    uint32_t areaColor = 0xFFFFFF;  ///< 填充色
    bool isSolid = true;  ///< 是否填充区域。
    int ownerPartId = 1;  ///< 所属部件编号。
    int sourceGraphicIndex = -1;  ///< 来源图元索引。
    int sourcePartIndex = 0;  ///< 来源部件中的局部顺序索引。
};

/**
 * @brief Altium 符号圆角矩形
 */
struct AltiumSchRoundRectangle {
    int locationX = 0, locationY = 0;  ///< 左上角坐标（原始单位）。
    int cornerX = 0, cornerY = 0;  ///< 右下角坐标（原始单位）。
    int cornerXRadius = 0, cornerYRadius = 0;  ///< X/Y 方向圆角半径。
    int lineWidth = 0;  ///< 轮廓线宽索引。
    int lineStyle = 0;  ///< 轮廓线型。
    uint32_t color = 0x000000;  ///< 轮廓颜色。
    uint32_t areaColor = 0xFFFFFF;  ///< 填充颜色。
    bool isSolid = true;  ///< 是否填充区域。
    int ownerPartId = 1;  ///< 所属部件编号。
    int sourceGraphicIndex = -1;  ///< 来源图元索引。
    int sourcePartIndex = 0;  ///< 来源部件中的局部顺序索引。
};

/**
 * @brief Altium 符号线段
 */
struct AltiumSchLine {
    int locationX = 0, locationY = 0;  ///< 起点
    int cornerX = 0, cornerY = 0;  ///< 终点
    int lineWidth = 0;  ///< 线宽索引。
    int lineStyle = 0;  ///< 线型编码。
    uint32_t color = 0x000000;  ///< 线段颜色。
    int ownerPartId = 1;  ///< 所属部件编号。
};

/**
 * @brief Altium 符号弧线
 */
struct AltiumSchArc {
    int centerX = 0, centerY = 0;  ///< 圆心（原始单位）
    int radius = 0;  ///< 半径（原始单位）
    double startAngle = 0.0;  ///< 起始角度
    double endAngle = 360.0;  ///< 结束角度
    int lineWidth = 0;  ///< 弧线宽索引。
    int lineStyle = 0;  ///< 弧线型编码。
    uint32_t color = 0x000000;  ///< 弧线颜色。
    int ownerPartId = 1;  ///< 所属部件编号。
    QString sourceGraphicType;  ///< 来源图元类型。
    int sourceGraphicIndex = -1;  ///< 来源图元索引。
    int sourceSegmentIndex = -1;  ///< 来源路径片段索引。
    int sourcePartIndex = 0;  ///< 来源部件中的局部顺序索引。
};

/**
 * @brief Altium 符号多边形
 */
struct AltiumSchPolygon {
    QList<QPointF> vertices;  ///< 顶点列表（Schematic Units）
    int lineWidth = 0;  ///< 轮廓线宽索引。
    int lineStyle = 0;  ///< 轮廓线型。
    uint32_t color = 0x000000;  ///< 轮廓颜色。
    uint32_t areaColor = 0xFFFFFF;  ///< 填充颜色。
    bool isSolid = true;  ///< 是否填充区域。
    int ownerPartId = 1;  ///< 所属部件编号。
    int sourceGraphicIndex = -1;  ///< 来源图元索引。
    int sourcePartIndex = 0;  ///< 来源部件中的局部顺序索引。
};

/**
 * @brief Altium 符号椭圆/圆
 */
struct AltiumSchEllipse {
    int centerX = 0, centerY = 0;  ///< 圆心坐标（原始单位）。
    int radiusX = 0, radiusY = 0;  ///< X/Y 半径（原始单位）
    int lineWidth = 0;  ///< 轮廓线宽索引。
    int lineStyle = 0;  ///< 轮廓线型。
    uint32_t color = 0x000000;  ///< 轮廓颜色。
    uint32_t areaColor = 0xFFFFFF;  ///< 填充颜色。
    bool isSolid = true;  ///< 是否填充区域。
    int ownerPartId = 1;  ///< 所属部件编号。
    QString sourceGraphicType;  ///< 来源图元类型。
    int sourceGraphicIndex = -1;  ///< 来源图元索引。
    int sourcePartIndex = 0;  ///< 来源部件中的局部顺序索引。
};

/**
 * @brief Altium 符号扇形
 */
struct AltiumSchPie {
    int centerX = 0, centerY = 0;  ///< 圆心坐标（原始单位）。
    int radius = 0;  ///< 半径（原始单位）。
    double startAngle = 0.0;  ///< 起始角度。
    double endAngle = 360.0;  ///< 结束角度。
    int lineWidth = 0;  ///< 轮廓线宽索引。
    int lineStyle = 0;  ///< 轮廓线型。
    uint32_t color = 0x000000;  ///< 轮廓颜色。
    uint32_t areaColor = 0xFFFFFF;  ///< 填充颜色。
    bool isSolid = true;  ///< 是否填充区域。
    int ownerPartId = 1;  ///< 所属部件编号。
};

/**
 * @brief Altium 符号椭圆弧
 */
struct AltiumSchEllipticalArc {
    int centerX = 0, centerY = 0;  ///< 圆心坐标（原始单位）。
    int radiusX = 0, radiusY = 0;  ///< X/Y 方向半径（原始单位）。
    double startAngle = 0.0;  ///< 起始角度。
    double endAngle = 360.0;  ///< 结束角度。
    int lineWidth = 0;  ///< 轮廓线宽索引。
    int lineStyle = 0;  ///< 轮廓线型。
    uint32_t color = 0x000000;  ///< 轮廓颜色。
    uint32_t areaColor = 0xFFFFFF;  ///< 填充颜色。
    int ownerPartId = 1;  ///< 所属部件编号。
    QString sourceGraphicType;  ///< 来源图元类型。
    int sourceGraphicIndex = -1;  ///< 来源图元索引。
    int sourceSegmentIndex = -1;  ///< 来源路径片段索引。
    int sourcePartIndex = 0;  ///< 来源部件中的局部顺序索引。
};

/**
 * @brief Altium 符号折线
 */
struct AltiumSchPolyline {
    QList<QPointF> vertices;  ///< 顶点列表（Schematic Units）
    int lineWidth = 0;  ///< 线宽索引。
    int lineStyle = 0;  ///< 线型编码。
    uint32_t color = 0x000000;  ///< 轮廓颜色。
    int ownerPartId = 1;  ///< 所属部件编号。
    int sourceGraphicIndex = -1;  ///< 来源图元索引。
    int sourcePartIndex = 0;  ///< 来源部件中的局部顺序索引。
};

/**
 * @brief Altium 符号路径
 */
struct AltiumSchPath {
    QList<QPointF> vertices;  ///< 路径顶点（Schematic Units）
    int lineWidth = 0;  ///< 线宽索引。
    int lineStyle = 0;  ///< 线型编码。
    uint32_t color = 0x000000;  ///< 轮廓颜色。
    int ownerPartId = 1;  ///< 所属部件编号。
    QString sourceGraphicType;  ///< 来源图元类型。
    int sourceGraphicIndex = -1;  ///< 来源图元索引。
    int sourceSegmentIndex = -1;  ///< 来源路径片段索引。
    int sourcePartIndex = 0;  ///< 来源部件中的局部顺序索引。
};

/**
 * @brief Altium 符号三次 Bézier 曲线
 * @details 四个控制点按 Schematic Units 写入 RECORD=5。
 */
struct AltiumSchBezier {
    QList<QPointF> controlPoints;  ///< 起点、两个控制点和终点
    int lineWidth = 0;  ///< 曲线线宽索引。
    uint32_t color = 0x000000;  ///< 曲线颜色。
    int ownerPartId = 1;  ///< 所属部件编号。
    QString sourceGraphicType;  ///< 来源图元类型。
    int sourceGraphicIndex = -1;  ///< 来源图元索引。
    int sourceSegmentIndex = -1;  ///< 来源路径片段索引。
    int sourcePartIndex = 0;  ///< 来源部件中的局部顺序索引。
};

/**
 * @brief Altium 符号 IEEE 图形
 * @details 写入 RECORD=3，支持独立逻辑和数学图形。
 */
struct AltiumSchIeee {
    int symbol = 0;  ///< TIeeeSymbol 编号
    int locationX = 0, locationY = 0;  ///< 锚点位置（原始单位）
    int scaleFactor = 10;  ///< IEEE 图形缩放因子。
    int orientation = 0;  ///< IEEE 图形方向编码。
    bool mirrored = false;  ///< 是否镜像 IEEE 图形。
    int lineWidth = 1;  ///< 图形线宽索引。
    uint32_t color = 0x000000;  ///< 图形颜色。
    int ownerPartId = 1;  ///< 所属部件编号。
};

/**
 * @brief Altium 符号文本
 */
struct AltiumSchText {
    int locationX = 0, locationY = 0;  ///< 文本锚点位置（原始单位）
    QString text;  ///< 文本内容
    int fontId = 1;  ///< Altium 字体编号
    double fontSizeMm = 0.0;  ///< 原始字体大小（mm，0 表示使用字体编号默认值）
    QString fontName;  ///< 字体族；为空时使用字体表默认字体
    bool bold = false;  ///< 是否粗体
    bool italic = false;  ///< 是否斜体
    QString anchor = QStringLiteral("middle");  ///< 原始文本锚点
    uint32_t color = 0x000000;  ///< 颜色（0x00BBGGRR）
    bool isDisplayed = true;  ///< 是否显示文本
    bool isHidden = false;  ///< 是否强制隐藏文本
    int orientation = 0;  ///< 0-3，表示 0°/90°/180°/270°
    int ownerPartId = 1;  ///< 所属部件编号。
    bool isPinLabel = false;  ///< 是否为引脚标签文本。
    bool isPinNumber = false;  ///< 是否为独立的引脚编号文本。
    bool isFallbackPinNumber = false;  ///< 是否需要跟随归一化后的引脚主体端
    int sourcePinIndex = -1;  ///< 来源引脚索引，用于同步归一化坐标
    int sourceGraphicIndex = -1;  ///< 来源图元索引。
    int sourcePartIndex = 0;  ///< 来源部件中的局部顺序索引。
};

/**
 * @brief Altium 符号图元顺序引用
 * @details index 指向来源图元类型在所属部件中的局部索引。
 */
struct AltiumSchGraphicOrder {
    QString type;  ///< 图元类型名称。
    int index = -1;  ///< 图元在所属类型列表中的索引。
    int partIndex = 0;  ///< 图元所属部件索引。
};

/**
 * @brief Altium 符号文本框
 */
struct AltiumSchTextFrame {
    int locationX = 0, locationY = 0;  ///< 左上角坐标（原始单位）。
    int cornerX = 0, cornerY = 0;  ///< 右下角坐标（原始单位）。
    int lineWidth = 0;  ///< 边框线宽索引。
    int lineStyle = 0;  ///< 边框线型编码。
    uint32_t color = 0x000000;  ///< 边框颜色。
    uint32_t areaColor = 0x000000;  ///< 背景填充颜色。
    uint32_t textColor = 0x000000;  ///< 文本颜色。
    QString fontName;  ///< 字体族；为空时使用字体表默认字体
    double fontSizeMm = 0.0;  ///< 字体大小（mm，0 表示使用字体编号默认值）
    bool bold = false;  ///< 是否粗体
    bool italic = false;  ///< 是否斜体
    int fontId = 0;  ///< Altium 字体编号。
    int orientation = 0;  ///< 文本方向编码。
    int alignment = 0;  ///< 文本对齐方式编码。
    int textMargin = 0;  ///< 文本边距（原始单位）。
    QString text;  ///< 文本内容。
    bool isSolid = false;  ///< 是否填充文本框。
    bool showBorder = false;  ///< 是否显示边框。
    bool wordWrap = false;  ///< 是否自动换行。
    bool clipToRect = false;  ///< 是否裁剪到文本框范围。
    bool transparent = false;  ///< 是否透明背景。
    int ownerPartId = 1;  ///< 所属部件编号。
};

/**
 * @brief Altium 符号图片
 */
struct AltiumSchImage {
    int locationX = 0, locationY = 0;  ///< 左上角坐标（原始单位）。
    int cornerX = 0, cornerY = 0;  ///< 右下角坐标（原始单位）。
    double rotation = 0.0;  ///< 图片旋转角度。
    int lineWidth = 0;  ///< 边框线宽索引。
    int lineStyle = 0;  ///< 边框线型编码。
    uint32_t color = 0x000000;  ///< 边框颜色。
    uint32_t areaColor = 0x000000;  ///< 背景颜色。
    QString fileName;  ///< 图片文件名。
    QByteArray data;  ///< 嵌入图片数据。
    bool isSolid = false;  ///< 是否填充图片区域。
    bool transparent = false;  ///< 是否使用透明背景。
    bool showBorder = false;  ///< 是否显示图片边框。
    bool keepAspect = true;  ///< 是否保持图片宽高比。
    bool embedImage = false;  ///< 是否将图片数据嵌入库文件。
    int ownerPartId = 1;  ///< 所属部件编号。
    int sourceGraphicIndex = -1;  ///< 来源图元索引。
    int sourcePartIndex = 0;  ///< 来源部件中的局部顺序索引。
};

/**
 * @brief Altium 符号参数字段
 * @details 参数使用 RECORD=41 写入 SchLib Data 流。
 */
struct AltiumSchParameter {
    QString name;  ///< 参数名称
    QString value;  ///< 原理图元件属性对应的文本值。
    int locationX = 0;  ///< 参数位置 X（原始单位）
    int locationY = 0;  ///< 参数位置 Y（原始单位）
    int fontId = 1;  ///< Altium 字体编号
    double fontSizeMm = 0.0;  ///< 参数字体大小（mm，0 表示默认）
    uint32_t color = 0x000000;  ///< 颜色（0x00BBGGRR）
    bool isHidden = true;  ///< 是否隐藏
    bool readOnly = false;  ///< 是否只读
    int orientation = 0;  ///< 0-3，表示 0°/90°/180°/270°
    int ownerPartId = -1;  ///< 所属部件，-1 表示所有部件
};

/**
 * @brief Altium 符号元件
 * @details IR 与 SchLib 二进制协议之间的强类型边界，包含图元、引脚及实现关系。
 */
struct AltiumSchComponent {
    QString name;  ///< 元件名称（LibReference）
    QString description;  ///< 元件描述
    QString designatorPrefix = "?";  ///< 位号前缀（如 "R", "C", "U"）
    int partCount = 1;  ///< 部件数量

    /**
     * @brief 符号参数（如制造商、料号、Datasheet 和来源元数据）
     */
    QMap<QString, QString> sourceMetadata;  ///< 来源元数据键值表。
    /**
     * @brief 显式参数字段
     */
    QList<AltiumSchParameter> parameters;  ///< 显式参数字段列表。
    /**
     * @brief 符号别名
     */
    QStringList aliases;  ///< 符号别名列表。

    QList<AltiumSchPin> pins;  ///< 符号引脚列表。
    QList<AltiumSchRectangle> rectangles;  ///< 矩形图元列表。
    QList<AltiumSchRoundRectangle> roundRectangles;  ///< 圆角矩形图元列表。
    QList<AltiumSchLine> lines;  ///< 线段图元列表。
    QList<AltiumSchArc> arcs;  ///< 圆弧图元列表。
    QList<AltiumSchPolygon> polygons;  ///< 多边形图元列表。
    QList<AltiumSchEllipse> ellipses;  ///< 椭圆图元列表。
    QList<AltiumSchPie> pies;  ///< 扇形图元列表。
    QList<AltiumSchEllipticalArc> ellipticalArcs;  ///< 椭圆弧图元列表。
    QList<AltiumSchPolyline> polylines;  ///< 折线图元列表。
    QList<AltiumSchPath> paths;  ///< 路径图元列表。
    QList<AltiumSchBezier> beziers;  ///< 三次 Bézier 曲线列表。
    QList<AltiumSchIeee> ieeeSymbols;  ///< IEEE 图形列表。
    QList<AltiumSchText> texts;  ///< 文本图元列表。
    QList<AltiumSchTextFrame> textFrames;  ///< 文本框图元列表。
    QList<AltiumSchImage> images;  ///< 图片图元列表。
    QList<AltiumSchGraphicOrder> graphicOrder;  ///< 原始图元绘制顺序。

    /**
     * @brief 封装链接（实现记录）
     */
    struct Implementation {
        QString modelName;  ///< 封装名称
        QString modelType = "PCBLIB";  ///< 模型类型
        QString dataFileKind = "PCBLib";  ///< 数据文件类型
        QString dataFileEntity;  ///< 数据文件实体或路径
        QMap<QString, QString> parameters;  ///< 模型参数
        QMap<QString, QString> pinMappings;  ///< 引脚映射
    };

    QList<Implementation> implementations;  ///< 封装等实现模型列表。
};

}  // namespace EasyKiConverter
