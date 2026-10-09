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

/**
 * @brief Xpedition 焊盘的几何类型。
 */
enum class XpeditionPadShape { Unknown, Round, Rectangle, Oblong, Square, Polygon };

/**
 * @brief Pad 定义，供 Padstack 引用。
 */
struct XpeditionPadDefinition {
    QString name;  ///< Pad 定义名称。
    XpeditionPadShape shape = XpeditionPadShape::Unknown;  ///< Pad 形状。
    QSizeF size;  ///< Pad 尺寸。
    QPointF offset;  ///< Pad 相对原点偏移。
    QList<QPointF> polygon;  ///< 自定义多边形顶点。
    int line = 0;  ///< 定义所在源文件行号。
};

/**
 * @brief 孔定义，供 Padstack 引用。
 */
struct XpeditionHoleDefinition {
    QString name;  ///< 孔定义名称。
    XpeditionPadShape shape = XpeditionPadShape::Unknown;  ///< 孔形状。
    QSizeF size;  ///< 孔尺寸。
    bool plated = true;  ///< 是否镀层。
    int line = 0;  ///< 定义所在源文件行号。
};

/**
 * @brief Padstack 的层焊盘和孔关联。
 */
struct XpeditionPadstackDefinition {
    QString name;  ///< Padstack 名称。
    QString technology;  ///< 工艺类型。
    QString topPad;  ///< 顶层 Pad 名称。
    QString bottomPad;  ///< 底层 Pad 名称。
    QString topSolderMaskPad;  ///< 顶层阻焊 Pad 名称。
    QString bottomSolderMaskPad;  ///< 底层阻焊 Pad 名称。
    QString topSolderPastePad;  ///< 顶层锡膏 Pad 名称。
    QString bottomSolderPastePad;  ///< 底层锡膏 Pad 名称。
    QString holeName;  ///< 关联孔名称。
    int line = 0;  ///< 定义所在源文件行号。
};

/**
 * @brief Cell 中的引脚实例。
 */
struct XpeditionCellPin {
    QString number;  ///< 引脚编号。
    QPointF position;  ///< 引脚位置。
    QString padstack;  ///< 关联 Padstack 名称。
    double rotation = 0.0;  ///< 引脚旋转角度。
    bool mirror = false;  ///< 是否镜像放置。
    int line = 0;  ///< 定义所在源文件行号。
};

/**
 * @brief Cell 中的矩形或折线轮廓。
 */
struct XpeditionCellOutline {
    QString layer;  ///< 轮廓所在层名称。
    QList<QPointF> points;  ///< 轮廓顶点。
    int line = 0;  ///< 定义所在源文件行号。
};

/**
 * @brief Xpedition 封装 Cell。
 */
struct XpeditionCellDefinition {
    QString name;  ///< Cell 名称。
    QString packageGroup;  ///< 封装分组名称。
    QString mountType;  ///< 安装类型。
    int numberOfLayers = 0;  ///< 层数。
    QList<XpeditionCellPin> pins;  ///< Cell 引脚。
    QList<XpeditionCellOutline> outlines;  ///< Cell 轮廓。
    int line = 0;  ///< 定义所在源文件行号。
};

/**
 * @brief PDB 器件与 Cell、符号的关联。
 */
struct XpeditionPartDefinition {
    QString number;  ///< 器件编号。
    QString name;  ///< 器件名称。
    QString label;  ///< 器件显示标签。
    QString description;  ///< 器件描述。
    QString referencePrefix;  ///< 参考标号前缀。
    QString topCell;  ///< 顶层 Cell 名称。
    QString bottomCell;  ///< 底层 Cell 名称。
    QString symbol;  ///< 关联符号名称。
    QMap<QString, QString> properties;  ///< 器件扩展属性。
    QStringList symbolPinNames;  ///< 符号引脚名称列表。
    QList<QStringList> slotPinNumbers;  ///< 分组引脚编号。
    int line = 0;  ///< 定义所在源文件行号。
};

/**
 * @brief 解析后的 Xpedition HKP 格式模型。
 * @details 该模型保留格式语义，后续映射到统一 IR 时可以明确报告不支持的字段。
 */
struct XpeditionHkpModel {
    QList<XpeditionPadDefinition> pads;  ///< Pad 定义集合。
    QList<XpeditionHoleDefinition> holes;  ///< 孔定义集合。
    QList<XpeditionPadstackDefinition> padstacks;  ///< Padstack 定义集合。
    QList<XpeditionCellDefinition> cells;  ///< Cell 定义集合。
    QList<XpeditionPartDefinition> parts;  ///< 器件定义集合。

    /**
     * @brief 原始 Pad 名称到解析后候选名称的映射。
     */
    QMap<QString, QStringList> padNameVariants;
    /**
     * @brief 原始孔名称到解析后候选名称的映射。
     */
    QMap<QString, QStringList> holeNameVariants;
    /**
     * @brief 原始 Padstack 名称到解析后候选名称的映射。
     */
    QMap<QString, QStringList> padstackNameVariants;
    /**
     * @brief 原始 Cell 名称到解析后候选名称的映射。
     */
    QMap<QString, QStringList> cellNameVariants;

    /**
     * @brief 按名称查找 Padstack。
     * @param name Padstack 名称。
     * @return 找到的 Padstack，未找到时返回 nullptr。
     */
    const XpeditionPadstackDefinition* findPadstack(const QString& name) const;

    /**
     * @brief 按名称查找 Cell。
     * @param name Cell 名称。
     * @return 找到的 Cell，未找到时返回 nullptr。
     */
    const XpeditionCellDefinition* findCell(const QString& name) const;

    /**
     * @brief 判断 Padstack 原始名称是否对应多个定义。
     * @param name 原始 Padstack 名称。
     * @return 若对应多个候选则返回 true。
     */
    bool isPadstackAmbiguous(const QString& name) const;

    /**
     * @brief 判断 Cell 原始名称是否对应多个定义。
     * @param name 原始 Cell 名称。
     * @return 若对应多个候选则返回 true。
     */
    bool isCellAmbiguous(const QString& name) const;

    /**
     * @brief 判断 Pad 原始名称是否对应多个定义。
     * @param name 原始 Pad 名称。
     * @return 若对应多个候选则返回 true。
     */
    bool isPadAmbiguous(const QString& name) const;

    /**
     * @brief 判断孔原始名称是否对应多个定义。
     * @param name 原始孔名称。
     * @return 若对应多个候选则返回 true。
     */
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
