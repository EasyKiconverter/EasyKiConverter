#pragma once

/**
 * @file XpeditionHkpModel.h
 * @brief Xpedition HKP 格式专用模型。
 */

#include "GeometryTransforms.h"
#include "ParseDiagnostics.h"
#include "TextParsers.h"

#include <QMap>
#include <QPointF>
#include <QSizeF>
#include <QStringList>

namespace EasyKiConverter::Parser {

/** @brief Xpedition 焊盘的几何类型。 */
enum class XpeditionPadShape { Unknown, Round, Rectangle, Oblong, Square, Polygon };

/** @brief Pad 定义，供 Padstack 引用。 */
struct XpeditionPadDefinition {
    QString name;
    XpeditionPadShape shape = XpeditionPadShape::Unknown;
    QSizeF size;
    QPointF offset;
    QList<QPointF> polygon;
    int line = 0;
};

/** @brief 孔定义，供 Padstack 引用。 */
struct XpeditionHoleDefinition {
    QString name;
    XpeditionPadShape shape = XpeditionPadShape::Unknown;
    QSizeF size;
    bool plated = true;
    int line = 0;
};

/** @brief Padstack 的层焊盘和孔关联。 */
struct XpeditionPadstackDefinition {
    QString name;
    QString technology;
    QString topPad;
    QString bottomPad;
    QString topSolderMaskPad;
    QString bottomSolderMaskPad;
    QString topSolderPastePad;
    QString bottomSolderPastePad;
    QString holeName;
    int line = 0;
};

/** @brief Cell 中的引脚实例。 */
struct XpeditionCellPin {
    QString number;
    QPointF position;
    QString padstack;
    double rotation = 0.0;
    bool mirror = false;
    int line = 0;
};

/** @brief Cell 中的矩形或折线轮廓。 */
struct XpeditionCellOutline {
    QString layer;
    QList<QPointF> points;
    int line = 0;
};

/** @brief Xpedition 封装 Cell。 */
struct XpeditionCellDefinition {
    QString name;
    QString packageGroup;
    QString mountType;
    int numberOfLayers = 0;
    QList<XpeditionCellPin> pins;
    QList<XpeditionCellOutline> outlines;
    int line = 0;
};

/** @brief PDB 器件与 Cell、符号的关联。 */
struct XpeditionPartDefinition {
    QString number;
    QString name;
    QString description;
    QString referencePrefix;
    QString topCell;
    QString bottomCell;
    QString symbol;
    QMap<QString, QString> properties;
    int line = 0;
};

/**
 * @brief 解析后的 Xpedition HKP 格式模型。
 * @details 该模型保留格式语义，后续映射到统一 IR 时可以明确报告不支持的字段。
 */
struct XpeditionHkpModel {
    QList<XpeditionPadDefinition> pads;
    QList<XpeditionHoleDefinition> holes;
    QList<XpeditionPadstackDefinition> padstacks;
    QList<XpeditionCellDefinition> cells;
    QList<XpeditionPartDefinition> parts;

    /** @brief 原始 Pad 名称到解析后候选名称的映射。 */
    QMap<QString, QStringList> padNameVariants;
    /** @brief 原始孔名称到解析后候选名称的映射。 */
    QMap<QString, QStringList> holeNameVariants;
    /** @brief 原始 Padstack 名称到解析后候选名称的映射。 */
    QMap<QString, QStringList> padstackNameVariants;
    /** @brief 原始 Cell 名称到解析后候选名称的映射。 */
    QMap<QString, QStringList> cellNameVariants;

    /** @brief 按名称查找 Padstack。 */
    const XpeditionPadstackDefinition* findPadstack(const QString& name) const;

    /** @brief 按名称查找 Cell。 */
    const XpeditionCellDefinition* findCell(const QString& name) const;

    /** @brief 判断 Padstack 原始名称是否对应多个定义。 */
    bool isPadstackAmbiguous(const QString& name) const;

    /** @brief 判断 Cell 原始名称是否对应多个定义。 */
    bool isCellAmbiguous(const QString& name) const;

    /** @brief 判断 Pad 原始名称是否对应多个定义。 */
    bool isPadAmbiguous(const QString& name) const;

    /** @brief 判断孔原始名称是否对应多个定义。 */
    bool isHoleAmbiguous(const QString& name) const;
};

/**
 * @brief 从 HKP 语法树构造 Xpedition 格式模型。
 */
class XpeditionHkpModelParser {
public:
    /**
     * @brief 解析已识别的 HKP 文档节点。
     * @param sections HKP 分层节点。
     * @param unit 源文件单位。
     * @param diagnostics 诊断接收器。
     * @return 格式专用模型；非法字段不会静默转换。
     */
    static XpeditionHkpModel parse(const QList<SectionNode>& sections, LengthUnit unit, ParseDiagnostics* diagnostics);
};

}  // namespace EasyKiConverter::Parser
