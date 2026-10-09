#pragma once

/**
 * @file FootprintIR.h
 * @brief PCB 封装的通用中间表示
 *
 * 替代 FootprintData.h 中的结构体，移除所有 EasyEDA 特有格式：
 * - 层 ID 使用 LayerType 枚举（替代 EasyEDA 整数层 ID）
 * - 焊盘形状使用 PadShape 枚举（替代原始字符串）
 * - 焊盘类型使用 PadType 枚举（替代 holeRadius > 0 推断）
 * - 坐标以 QList<QPointF> 存储（替代原始字符串）
 * - SVG 路径已解析为几何参数
 */

#include "IRTypes.h"
#include "Model3DIR.h"

#include <QColor>
#include <QList>
#include <QPointF>
#include <QRectF>
#include <QSizeF>
#include <QString>

namespace EasyKiConverter {
namespace IR {

/**
 * @brief 通用焊盘
 *
 * 替代 FootprintPad，关键变更：
 * - shape: QString -> PadShape 枚举
 * - layerId: int -> LayerType 枚举
 * - holeRadius > 0 推断 -> padType 枚举
 * - points: QString -> QList<QPointF> (customShapePoints)
 */
struct FootprintPadIR {
    QString number;  ///< 焊盘编号（如 "1", "2", "A1"）
    QPointF position;  ///< 焊盘中心位置（已解析，单位 mm）
    PadShape shape = PadShape::Rect;  ///< 焊盘形状（枚举，替代原始字符串）
    PadType padType = PadType::Smd;  ///< 焊盘类型（枚举，替代 holeRadius 推断）
    QSizeF size;  ///< 焊盘尺寸（宽 x 高，mm）
    LayerType layer = LayerType::TopCopper;  ///< 所在层（枚举，替代数字层 ID）
    double rotation = 0.0;  ///< 旋转角度（度）
    double holeSize = 0.0;  ///< 通孔直径（mm，仅 ThroughHole 有效）
    double holeLength = 0.0;  ///< 槽孔长度（mm，仅 ThroughHole 有效）
    QString netName;  ///< 网络名称
    bool isPlated = true;  ///< 是否镀铜（仅通孔）
    bool isLocked = false;  ///< 是否锁定

    double solderMaskExpansionMm = 0.0;  ///< 阻焊扩展量，单位为毫米，正值表示向外扩展。
    double pasteMaskContractionMm = 0.0;  ///< 锡膏收缩量，单位为毫米，正值表示向内收缩。
    bool solderMaskEnabled = true;  ///< 是否生成阻焊层图形。
    bool pasteMaskEnabled = true;  ///< 是否生成锡膏层图形。

    QList<QPointF> customShapePoints;  ///< 异形焊盘的自定义顶点，单位为毫米。

    /**
     * @brief 判断焊盘是否为表面贴装类型。
     * @return 焊盘类型为 SMD 时返回 true。
     */
    bool isSmd() const {
        return padType == PadType::Smd;
    }

    /**
     * @brief 判断焊盘是否为通孔类型。
     * @return 焊盘类型为通孔时返回 true。
     */
    bool isThroughHole() const {
        return padType == PadType::ThroughHole;
    }
};

/**
 * @brief 通用走线
 *
 * 替代 FootprintTrack，关键变更：
 * - points: QString -> QList<QPointF>
 * - layerId: int -> LayerType
 */
struct FootprintTrackIR {
    QList<QPointF> points;  ///< 顶点列表（已解析，单位 mm）
    double width = 0.0;  ///< 线宽（mm）
    LayerType layer = LayerType::TopCopper;  ///< 所在层
    QString netName;  ///< 网络名称
    bool isLocked = false;  ///< 是否锁定走线编辑。
};

/**
 * @brief 通用安装孔
 */
struct FootprintHoleIR {
    QPointF center;  ///< 中心位置（已解析，单位 mm）
    double radius = 0.0;  ///< 孔半径（mm）
    bool isLocked = false;  ///< 是否锁定安装孔编辑。
};

/**
 * @brief 通用圆
 */
struct FootprintCircleIR {
    QPointF center;  ///< 圆心（已解析，单位 mm）
    double radius = 0.0;  ///< 半径（mm）
    double strokeWidth = 0.0;  ///< 线宽（mm）
    LayerType layer = LayerType::TopSilk;  ///< 所在层
    bool isLocked = false;  ///< 是否锁定圆形图元编辑。
};

/**
 * @brief 通用填充矩形
 */
struct FootprintRectangleIR {
    QRectF bounds;  ///< 边界矩形（已解析，单位 mm）
    double strokeWidth = 0.0;  ///< 线宽（mm）
    LayerType layer = LayerType::TopSilk;  ///< 所在层
    double rotation = 0.0;  ///< 旋转角度（度）
    bool isLocked = false;  ///< 是否锁定矩形图元编辑。
};

/**
 * @brief 通用圆弧
 *
 * 替代 FootprintArc，关键变更：
 * - path: QString SVG -> center/radius/startAngle/endAngle
 * - layerId: int -> LayerType
 */
struct FootprintArcIR {
    QPointF center;  ///< 圆心（已解析，单位 mm）
    double radius = 0.0;  ///< 半径（mm）
    double startAngle = 0.0;  ///< 起始角度（度）
    double endAngle = 0.0;  ///< 终止角度（度）
    double width = 0.0;  ///< 线宽（mm）
    LayerType layer = LayerType::TopSilk;  ///< 所在层
    QString netName;  ///< 关联网络名称。
    bool isLocked = false;  ///< 是否锁定圆弧编辑。
};

/**
 * @brief 通用文本
 *
 * 替代 FootprintText，关键变更：
 * - layerId: int -> LayerType
 * - type: "N" -> isFabrication 布尔
 * - textPath: QString SVG -> 非 ASCII 文本路径点列表
 */
struct FootprintTextIR {
    QString text;  ///< 文本内容
    QPointF position;  ///< 位置（已解析，单位 mm）
    double rotation = 0.0;  ///< 旋转角度（度）
    bool mirror = false;  ///< 是否镜像（替代原始字符串）
    double strokeWidth = 0.0;  ///< 线宽（mm）
    double fontSize = 0.0;  ///< 字体大小（mm）
    LayerType layer = LayerType::TopSilk;  ///< 所在层
    bool isDisplayed = true;  ///< 是否显示
    bool isFabrication = false;  ///< 是否为装配层文本（替代 type == "N"）
    bool isLocked = false;  ///< 是否锁定文本编辑。

    /** @brief 非 ASCII 文本的 SVG 路径点（已解析） */
    QList<QPointF> textPathPoints;  ///< 非 ASCII 文本的轮廓顶点，单位为毫米。
};

/**
 * @brief 通用实心区域
 *
 * 替代 FootprintSolidRegion，关键变更：
 * - path: QString SVG -> vertices 已解析顶点列表
 * - layerId: int -> LayerType
 * - isKeepOut 保留，由 Importer 根据 layerId == 99 设置
 */
struct FootprintRegionIR {
    QList<QPointF> vertices;  ///< 区域顶点（已解析，单位 mm）
    LayerType layer = LayerType::TopCopper;  ///< 所在层
    bool isKeepOut = false;  ///< 是否为禁止区
    bool isLocked = false;  ///< 是否锁定区域编辑。
};

/**
 * @brief 通用板框轮廓
 */
struct FootprintOutlineIR {
    QList<QPointF> points;  ///< 轮廓点（已解析，单位 mm）
    double strokeWidth = 0.0;  ///< 线宽（mm）
    LayerType layer = LayerType::EdgeCuts;  ///< 所在层（通常为 Edge.Cuts）
    bool isLocked = false;  ///< 是否锁定板框编辑。
};

/**
 * @brief 通用封装组件
 *
 * 包含一个封装的所有图形原语、3D 模型和元数据。
 * 不包含任何 EDA 平台特有的元数据字段。
 */
struct FootprintComponentIR {
    QString name;  ///< 封装名称
    QString description;  ///< 封装描述
    double height = 0.0;  ///< 3D 高度（mm）

    QList<FootprintPadIR> pads;  ///< 封装焊盘列表。
    QList<FootprintTrackIR> tracks;  ///< 封装走线列表。
    QList<FootprintHoleIR> holes;  ///< 机械孔列表。
    QList<FootprintCircleIR> circles;  ///< 圆形图元列表。
    QList<FootprintRectangleIR> rectangles;  ///< 矩形图元列表。
    QList<FootprintArcIR> arcs;  ///< 圆弧图元列表。
    QList<FootprintTextIR> texts;  ///< 文本图元列表。
    QList<FootprintRegionIR> regions;  ///< 区域图元列表。
    QList<FootprintOutlineIR> outlines;  ///< 板框轮廓列表。

    QList<Model3DIR> models3d;  ///< 关联的三维模型列表。

    bool shouldGenerateCourtyard = false;  ///< 无显式庭院且边界有效时是否自动生成庭院。

    /**
     * @brief 判断封装是否包含通孔焊盘。
     * @return 至少存在一个通孔焊盘时返回 true。
     */
    bool hasThroughHolePads() const {
        for (const auto& pad : pads) {
            if (pad.isThroughHole())
                return true;
        }
        return false;
    }

    /**
     * @brief 判断封装是否包含任何图形数据。
     * @return 任一图元列表非空时返回 true。
     */
    bool hasGraphics() const {
        return !pads.isEmpty() || !tracks.isEmpty() || !holes.isEmpty() || !circles.isEmpty() ||
               !rectangles.isEmpty() || !arcs.isEmpty() || !texts.isEmpty() || !regions.isEmpty() ||
               !outlines.isEmpty();
    }

    /**
     * @brief 清空封装名称、几何图元和三维模型。
     */
    void clear() {
        name.clear();
        description.clear();
        height = 0.0;
        pads.clear();
        tracks.clear();
        holes.clear();
        circles.clear();
        rectangles.clear();
        arcs.clear();
        texts.clear();
        regions.clear();
        outlines.clear();
        models3d.clear();
    }
};

/**
 * @brief 通用封装放置实例。
 * @details 该结构只描述板级关联，不复制封装几何；封装几何通过 footprintName 引用同一份 IR 定义。
 */
struct FootprintPlacementIR {
    QString reference;  ///< 器件参考标识，例如 "U1"。
    QString footprintName;  ///< 被放置的封装定义名称。
    QPointF position;  ///< 板级位置，单位为毫米。
    double rotation = 0.0;  ///< 板级旋转角度，单位为度。
    bool mirrored = false;  ///< 是否位于底面或经过镜像放置。
};

}  // namespace IR
}  // namespace EasyKiConverter
