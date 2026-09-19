#pragma once

/**
 * @file CadstarModel.h
 * @brief Cadstar ASCII 库的格式专用模型和解析入口。
 */

#include "GeometryTransforms.h"
#include "ParseDiagnostics.h"
#include "TextParsers.h"

#include <QList>
#include <QMap>
#include <QPointF>
#include <QString>

namespace EasyKiConverter::Parser {

/** @brief Cadstar 焊盘形状。 */
enum class CadstarPadShape { Round, Square, Rectangle, Oblong, Octagon, Custom, Unknown };

/** @brief Cadstar 焊盘定义。 */
struct CadstarPad {
    QString name;
    CadstarPadShape shape = CadstarPadShape::Unknown;
    double diameter = 0.0;
    double width = 0.0;
    double height = 0.0;
    double offsetX = 0.0;
    double offsetY = 0.0;
    double holeDiameter = 0.0;
    double holeWidth = 0.0;
    double holeHeight = 0.0;
    QList<QPointF> polygon;
};

/** @brief Cadstar 封装引脚。 */
struct CadstarPackagePin {
    QString number;
    QPointF position;
    QString padName;
    double rotation = 0.0;
};

/** @brief Cadstar 封装图形类型。 */
enum class CadstarGraphicType { Line, Rectangle, Circle, Arc, Polyline, Polygon };

/** @brief Cadstar 封装或符号图形。 */
struct CadstarGraphic {
    CadstarGraphicType type = CadstarGraphicType::Line;
    QList<QPointF> points;
    double width = 0.0;
    double radius = 0.0;
};

/** @brief Cadstar 封装定义。 */
struct CadstarPackage {
    QString name;
    QString description;
    QList<CadstarPackagePin> pins;
    QList<CadstarGraphic> graphics;
    QMap<QString, QString> properties;
};

/** @brief Cadstar 符号引脚。 */
struct CadstarComponentPin {
    QString id;
    QPointF start;
    QPointF end;
    double rotation = 0.0;
    bool inverted = false;
    QString pinType;
    QStringList numbers;
    QString label;
    QPointF labelPosition;
    bool labelVisible = true;
    bool numberVisible = true;
};

/** @brief Cadstar 符号定义。 */
struct CadstarComponent {
    QString name;
    int version = 0;
    QList<CadstarComponentPin> pins;
    QList<CadstarGraphic> graphics;
    QMap<QString, QString> properties;
};

/** @brief Cadstar 器件与符号、封装之间的关联。 */
struct CadstarPart {
    QString name;
    QString componentName;
    QString packageName;
    QString description;
    QMap<QString, QString> properties;
};

/** @brief Cadstar ASCII 库的完整解析结果。 */
struct CadstarLibrary {
    LengthUnit unit = LengthUnit::Millimeter;
    QList<CadstarPad> pads;
    QList<CadstarPackage> packages;
    QList<CadstarComponent> components;
    QList<CadstarPart> parts;
    /** @brief 原始焊盘名称到定义候选的索引。 */
    QMap<QString, QStringList> padNameVariants;
    /** @brief 原始封装名称到定义候选的索引。 */
    QMap<QString, QStringList> packageNameVariants;
    /** @brief 原始符号名称到定义候选的索引。 */
    QMap<QString, QStringList> componentNameVariants;
    /** @brief 原始器件名称到定义候选的索引。 */
    QMap<QString, QStringList> partNameVariants;
    ParseDiagnostics diagnostics;

    /** @brief 判断输入是否至少包含一个 Cadstar 库定义。 */
    bool isRecognized() const;

    /** @brief 判断焊盘名称是否对应多个定义。 */
    bool isPadAmbiguous(const QString& name) const;

    /** @brief 判断封装名称是否对应多个定义。 */
    bool isPackageAmbiguous(const QString& name) const;

    /** @brief 判断符号名称是否对应多个定义。 */
    bool isComponentAmbiguous(const QString& name) const;

    /** @brief 判断器件名称是否对应多个定义。 */
    bool isPartAmbiguous(const QString& name) const;
};

/**
 * @brief 解析 Cadstar ASCII Pad、Package、Component 和 Part 定义。
 * @details 解析阶段只生成 Cadstar 专用模型，目标格式转换由独立 Adapter 完成。
 */
class CadstarParser {
public:
    /**
     * @brief 从内存文本解析 Cadstar ASCII 库。
     * @param content UTF-8 文本。
     * @param filePath 可选源文件路径。
     * @return Cadstar 专用模型和结构化诊断。
     */
    static CadstarLibrary parse(const QString& content, const QString& filePath = QString());
};

}  // namespace EasyKiConverter::Parser
