#pragma once

/**
 * @file CadstarModel.h
 * @brief Cadstar ASCII 库的格式专用模型和解析入口。
 */

#include "GeometryTransforms.h"
#include "ParseDiagnostics.h"
#include "TextParsers.h"

#include <QByteArray>
#include <QList>
#include <QMap>
#include <QPointF>
#include <QString>

namespace EasyKiConverter::Parser {

/**
 * @brief Cadstar 焊盘形状。
 */
enum class CadstarPadShape { Round, Square, Rectangle, Oblong, Octagon, Custom, Unknown };

/**
 * @brief Cadstar 焊盘定义。
 */
struct CadstarPad {
    QString name;  ///< 焊盘名称。
    CadstarPadShape shape = CadstarPadShape::Unknown;  ///< 焊盘形状。
    double diameter = 0.0;  ///< 圆形焊盘直径。
    double width = 0.0;  ///< 焊盘宽度。
    double height = 0.0;  ///< 焊盘高度。
    double offsetX = 0.0;  ///< 焊盘 X 偏移。
    double offsetY = 0.0;  ///< 焊盘 Y 偏移。
    double holeDiameter = 0.0;  ///< 圆孔直径。
    double holeWidth = 0.0;  ///< 孔宽度。
    double holeHeight = 0.0;  ///< 孔高度。
    QList<QPointF> polygon;  ///< 自定义焊盘顶点。
};

/**
 * @brief Cadstar 封装引脚。
 */
struct CadstarPackagePin {
    QString number;  ///< 引脚编号。
    QPointF position;  ///< 引脚位置。
    QString padName;  ///< 关联焊盘名称。
    double rotation = 0.0;  ///< 引脚旋转角度。
};

/**
 * @brief Cadstar 封装图形类型。
 */
enum class CadstarGraphicType { Line, Rectangle, Circle, Arc, Polyline, Polygon };

/**
 * @brief Cadstar 封装或符号图形。
 */
struct CadstarGraphic {
    CadstarGraphicType type = CadstarGraphicType::Line;  ///< 图形类型。
    QList<QPointF> points;  ///< 图形顶点。
    double width = 0.0;  ///< 线宽。
    double radius = 0.0;  ///< 圆或圆弧半径。
};

/**
 * @brief Cadstar 封装定义。
 */
struct CadstarPackage {
    QString name;  ///< 封装名称。
    QString description;  ///< 封装描述。
    QList<CadstarPackagePin> pins;  ///< 封装引脚。
    QList<CadstarGraphic> graphics;  ///< 封装图形。
    QMap<QString, QString> properties;  ///< 扩展属性。
};

/**
 * @brief Cadstar 符号引脚。
 */
struct CadstarComponentPin {
    QString id;  ///< 引脚标识。
    QPointF start;  ///< 引脚起点。
    QPointF end;  ///< 引脚终点。
    double rotation = 0.0;  ///< 引脚旋转角度。
    bool inverted = false;  ///< 是否反相。
    QString pinType;  ///< 引脚类型。
    QStringList numbers;  ///< 引脚编号列表。
    QString label;  ///< 引脚标签。
    QPointF labelPosition;  ///< 标签位置。
    bool labelVisible = true;  ///< 标签是否可见。
    bool numberVisible = true;  ///< 编号是否可见。
};

/**
 * @brief Cadstar 符号定义。
 */
struct CadstarComponent {
    QString name;  ///< 符号名称。
    int version = 0;  ///< 符号版本。
    QList<CadstarComponentPin> pins;  ///< 符号引脚。
    QList<CadstarGraphic> graphics;  ///< 符号图形。
    QMap<QString, QString> properties;  ///< 扩展属性。
};

/**
 * @brief Cadstar 器件与符号、封装之间的关联。
 */
struct CadstarPart {
    QString name;  ///< 器件名称。
    QString componentName;  ///< 关联符号名称。
    QString packageName;  ///< 关联封装名称。
    QString description;  ///< 器件描述。
    QMap<QString, QString> properties;  ///< 扩展属性。
};

/**
 * @brief Cadstar ASCII 库的完整解析结果。
 */
struct CadstarLibrary {
    LengthUnit unit = LengthUnit::Millimeter;  ///< 库使用的长度单位。
    QList<CadstarPad> pads;  ///< 焊盘定义。
    QList<CadstarPackage> packages;  ///< 封装定义。
    QList<CadstarComponent> components;  ///< 符号定义。
    QList<CadstarPart> parts;  ///< 器件关联定义。
    /**
     * @brief 原始焊盘名称到定义候选的索引。
     */
    QMap<QString, QStringList> padNameVariants;
    /**
     * @brief 原始封装名称到定义候选的索引。
     */
    QMap<QString, QStringList> packageNameVariants;
    /**
     * @brief 原始符号名称到定义候选的索引。
     */
    QMap<QString, QStringList> componentNameVariants;
    /**
     * @brief 原始器件名称到定义候选的索引。
     */
    QMap<QString, QStringList> partNameVariants;
    ParseDiagnostics diagnostics;  ///< 解析和合并诊断。

    /**
     * @brief 判断输入是否至少包含一个 Cadstar 库定义。
     */
    bool isRecognized() const;

    /**
     * @brief 判断焊盘名称是否对应多个定义。
     * @param name 原始焊盘名称。
     * @return 若存在多个候选则返回 true。
     */
    bool isPadAmbiguous(const QString& name) const;

    /**
     * @brief 判断封装名称是否对应多个定义。
     * @param name 原始封装名称。
     * @return 若存在多个候选则返回 true。
     */
    bool isPackageAmbiguous(const QString& name) const;

    /**
     * @brief 判断符号名称是否对应多个定义。
     * @param name 原始符号名称。
     * @return 若存在多个候选则返回 true。
     */
    bool isComponentAmbiguous(const QString& name) const;

    /**
     * @brief 判断器件名称是否对应多个定义。
     * @param name 原始器件名称。
     * @return 若存在多个候选则返回 true。
     */
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

    /**
     * @brief 从原始字节检测编码后解析 Cadstar ASCII 库。
     * @param data 原始文件字节。
     * @param filePath 可选源文件路径。
     * @return Cadstar 专用模型和编码、语法诊断。
     */
    static CadstarLibrary parseBytes(const QByteArray& data, const QString& filePath = QString());
};

/**
 * @brief 合并多个 Cadstar ASCII 库模型并保留跨文件关联歧义。
 * @details 重名定义会生成稳定后缀；引用仍按原始名称解析，多个候选时必须由调用方显式处理。
 */
class CadstarMerger {
public:
    /**
     * @brief 合并已解析的 Cadstar 库。
     * @param libraries 待合并库模型。
     * @param filePath 合并结果的逻辑路径。
     * @return 合并后的 Cadstar 专用模型和诊断。
     */
    static CadstarLibrary merge(const QList<CadstarLibrary>& libraries, const QString& filePath = QString());
};

}  // namespace EasyKiConverter::Parser
