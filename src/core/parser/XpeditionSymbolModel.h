#pragma once

/**
 * @file XpeditionSymbolModel.h
 * @brief Xpedition V54 ASCII 符号的格式专用模型。
 */

#include "ParseDiagnostics.h"

#include <QByteArray>
#include <QList>
#include <QMap>
#include <QPointF>
#include <QRectF>
#include <QStringList>

namespace EasyKiConverter::Parser {

/**
 * @brief Xpedition 符号引脚及其显示属性。
 */
struct XpeditionSymbolPin {
    int id = 0;  ///< 源格式引脚标识。
    QPointF start;  ///< 引脚图形起点。
    QPointF end;  ///< 引脚图形终点。
    int rotation = 0;  ///< 引脚方向编码。
    bool inverted = false;  ///< 是否显示反相标记。
    QString pinType;  ///< 源格式引脚类型。
    QStringList numbers;  ///< 引脚编号列表。
    QString name;  ///< 引脚名称。
    QPointF namePosition;  ///< 引脚名称位置。
    double nameSize = 0.0;  ///< 引脚名称字号。
    double nameRotation = 0.0;  ///< 引脚名称旋转角度。
    bool nameVisible = true;  ///< 引脚名称是否可见。
    bool numbersVisible = false;  ///< 引脚编号是否可见。
    int partIndex = 0;  ///< 所属器件单元索引。
};

/**
 * @brief Xpedition 符号折线或多边形。
 */
struct XpeditionSymbolPolyline {
    QList<QPointF> points;  ///< 折线顶点。
    bool closed = false;  ///< 是否闭合为多边形。
    int partIndex = 0;  ///< 所属器件单元索引。
};

/**
 * @brief Xpedition 符号矩形。
 */
struct XpeditionSymbolRectangle {
    QPointF start;  ///< 矩形起点。
    QPointF end;  ///< 矩形对角点。
    int partIndex = 0;  ///< 所属器件单元索引。
};

/**
 * @brief Xpedition 符号圆形。
 */
struct XpeditionSymbolCircle {
    QPointF center;  ///< 圆心。
    double radius = 0.0;  ///< 半径。
    int partIndex = 0;  ///< 所属器件单元索引。
};

/**
 * @brief Xpedition 符号圆弧的三点表示。
 */
struct XpeditionSymbolArc {
    QPointF start;  ///< 圆弧起点。
    QPointF center;  ///< 圆弧中心。
    QPointF end;  ///< 圆弧终点。
    int partIndex = 0;  ///< 所属器件单元索引。
};

/**
 * @brief Xpedition 符号文本。
 */
struct XpeditionSymbolText {
    QPointF position;  ///< 文本基准位置。
    double size = 0.0;  ///< 文本字号。
    double rotation = 0.0;  ///< 文本旋转角度。
    int origin = 0;  ///< 文本对齐原点编码。
    QString text;  ///< 文本内容。
    int partIndex = 0;  ///< 所属器件单元索引。
};

/**
 * @brief 解析后的 Xpedition V54 符号模型。
 * @details 坐标暂以 Xpedition 的千分之一英寸单位保存，Adapter 负责转换为毫米。
 */
struct XpeditionSymbolModel {
    QString name;  ///< 符号名称。
    int version = 0;  ///< 源格式版本。
    int symbolType = 0;  ///< 符号类型编码。
    QRectF bounds;  ///< 符号边界。
    double zoomLevel = 1.0;  ///< 源格式显示缩放级别。
    int partCount = 1;  ///< 符号单元数量。
    QString footprint;  ///< 关联封装名称。
    QStringList heterogeneousParts;  ///< 异构单元名称列表。
    QMap<QString, QString> properties;  ///< 未建模的源格式属性。
    QList<XpeditionSymbolPin> pins;  ///< 符号引脚。
    QList<XpeditionSymbolPolyline> polylines;  ///< 折线图形。
    QList<XpeditionSymbolRectangle> rectangles;  ///< 矩形图形。
    QList<XpeditionSymbolCircle> circles;  ///< 圆形图形。
    QList<XpeditionSymbolArc> arcs;  ///< 圆弧图形。
    QList<XpeditionSymbolText> texts;  ///< 文本图形。
};

/**
 * @brief Xpedition 符号文本解析结果。
 */
struct XpeditionSymbolDocument {
    XpeditionSymbolModel model;  ///< 解析后的符号模型。
    ParseDiagnostics diagnostics;  ///< 解析过程中的结构化诊断。

    /**
     * @brief 判断文本是否包含可识别的 V54 符号头。
     */
    bool isRecognized() const;
};

/**
 * @brief 解析 Xpedition V54 ASCII 符号文本。
 */
class XpeditionSymbolParser {
public:
    /**
     * @brief 从内存文本解析符号。
     * @param content UTF-8 或 ASCII 符号文本。
     * @param filePath 可选源文件路径，用于诊断。
     * @return 格式专用模型和诊断集合。
     */
    static XpeditionSymbolDocument parse(const QString& content, const QString& filePath = QString());

    /**
     * @brief 从原始字节检测编码后解析 Xpedition V54 符号。
     * @param data 原始文件字节。
     * @param filePath 可选源文件路径。
     * @return 格式专用模型和编码、语法诊断。
     */
    static XpeditionSymbolDocument parseBytes(const QByteArray& data, const QString& filePath = QString());
};

}  // namespace EasyKiConverter::Parser
