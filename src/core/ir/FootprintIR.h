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

    /** @brief 异形焊盘的自定义形状顶点（已解析，单位 mm） */
    QList<QPointF> customShapePoints;

    /** @brief 是否为表面贴装 */
    bool isSmd() const {
        return padType == PadType::Smd;
    }

    /** @brief 是否为通孔 */
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
    bool isLocked = false;
};

/**
 * @brief 通用安装孔
 */
struct FootprintHoleIR {
    QPointF center;  ///< 中心位置（已解析，单位 mm）
    double radius = 0.0;  ///< 孔半径（mm）
    bool isLocked = false;
};

/**
 * @brief 通用圆
 */
struct FootprintCircleIR {
    QPointF center;  ///< 圆心（已解析，单位 mm）
    double radius = 0.0;  ///< 半径（mm）
    double strokeWidth = 0.0;  ///< 线宽（mm）
    LayerType layer = LayerType::TopSilk;  ///< 所在层
    bool isLocked = false;
};

/**
 * @brief 通用填充矩形
 */
struct FootprintRectangleIR {
    QRectF bounds;  ///< 边界矩形（已解析，单位 mm）
    double strokeWidth = 0.0;  ///< 线宽（mm）
    LayerType layer = LayerType::TopSilk;  ///< 所在层
    double rotation = 0.0;  ///< 旋转角度（度）
    bool isLocked = false;
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
    QString netName;
    bool isLocked = false;
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
    bool isLocked = false;

    /** @brief 非 ASCII 文本的 SVG 路径点（已解析） */
    QList<QPointF> textPathPoints;
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
    bool isLocked = false;
};

/**
 * @brief 通用板框轮廓
 */
struct FootprintOutlineIR {
    QList<QPointF> points;  ///< 轮廓点（已解析，单位 mm）
    double strokeWidth = 0.0;  ///< 线宽（mm）
    LayerType layer = LayerType::EdgeCuts;  ///< 所在层（通常为 Edge.Cuts）
    bool isLocked = false;
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

    // 图形原语列表
    QList<FootprintPadIR> pads;
    QList<FootprintTrackIR> tracks;
    QList<FootprintHoleIR> holes;
    QList<FootprintCircleIR> circles;
    QList<FootprintRectangleIR> rectangles;
    QList<FootprintArcIR> arcs;
    QList<FootprintTextIR> texts;
    QList<FootprintRegionIR> regions;
    QList<FootprintOutlineIR> outlines;

    // 3D 模型
    QList<Model3DIR> models3d;

    /** @brief 是否应自动生成 courtyard（当无显式 courtyard 区域且原始 bbox 有效时） */
    bool shouldGenerateCourtyard = false;

    /** @brief 是否包含通孔焊盘 */
    bool hasThroughHolePads() const {
        for (const auto& pad : pads) {
            if (pad.isThroughHole())
                return true;
        }
        return false;
    }

    /** @brief 是否包含任何图形数据 */
    bool hasGraphics() const {
        return !pads.isEmpty() || !tracks.isEmpty() || !holes.isEmpty() || !circles.isEmpty() ||
               !rectangles.isEmpty() || !arcs.isEmpty() || !texts.isEmpty() || !regions.isEmpty() ||
               !outlines.isEmpty();
    }

    /** @brief 清空封装名称、几何图元和三维模型。 */
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
