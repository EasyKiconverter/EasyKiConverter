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

/** @brief Xpedition 符号引脚及其显示属性。 */
struct XpeditionSymbolPin {
    int id = 0;
    QPointF start;
    QPointF end;
    int rotation = 0;
    bool inverted = false;
    QString pinType;
    QStringList numbers;
    QString name;
    QPointF namePosition;
    double nameSize = 0.0;
    double nameRotation = 0.0;
    bool nameVisible = true;
    bool numbersVisible = false;
    int partIndex = 0;
};

/** @brief Xpedition 符号折线或多边形。 */
struct XpeditionSymbolPolyline {
    QList<QPointF> points;
    bool closed = false;
    int partIndex = 0;
};

/** @brief Xpedition 符号矩形。 */
struct XpeditionSymbolRectangle {
    QPointF start;
    QPointF end;
    int partIndex = 0;
};

/** @brief Xpedition 符号圆形。 */
struct XpeditionSymbolCircle {
    QPointF center;
    double radius = 0.0;
    int partIndex = 0;
};

/** @brief Xpedition 符号圆弧的三点表示。 */
struct XpeditionSymbolArc {
    QPointF start;
    QPointF center;
    QPointF end;
    int partIndex = 0;
};

/** @brief Xpedition 符号文本。 */
struct XpeditionSymbolText {
    QPointF position;
    double size = 0.0;
    double rotation = 0.0;
    int origin = 0;
    QString text;
    int partIndex = 0;
};

/**
 * @brief 解析后的 Xpedition V54 符号模型。
 * @details 坐标暂以 Xpedition 的千分之一英寸单位保存，Adapter 负责转换为毫米。
 */
struct XpeditionSymbolModel {
    QString name;
    int version = 0;
    int symbolType = 0;
    QRectF bounds;
    double zoomLevel = 1.0;
    int partCount = 1;
    QString footprint;
    QStringList heterogeneousParts;
    QMap<QString, QString> properties;
    QList<XpeditionSymbolPin> pins;
    QList<XpeditionSymbolPolyline> polylines;
    QList<XpeditionSymbolRectangle> rectangles;
    QList<XpeditionSymbolCircle> circles;
    QList<XpeditionSymbolArc> arcs;
    QList<XpeditionSymbolText> texts;
};

/** @brief Xpedition 符号文本解析结果。 */
struct XpeditionSymbolDocument {
    XpeditionSymbolModel model;
    ParseDiagnostics diagnostics;

    /** @brief 判断文本是否包含可识别的 V54 符号头。 */
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
