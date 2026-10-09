#pragma once

#include "AltiumCommon.h"

#include <QByteArray>
#include <QList>
#include <QMap>
#include <QPointF>
#include <QString>

namespace EasyKiConverter {

/**
 * @brief Altium PCB 焊盘
 */
struct AltiumPcbPad {
    QString designator;  ///< 焊盘编号
    int32_t locationX = 0;  ///< X 坐标（PCB 原始单位）
    int32_t locationY = 0;  ///< Y 坐标
    int32_t sizeTopX = 0, sizeTopY = 0;  ///< 顶层尺寸
    int32_t sizeMidX = 0, sizeMidY = 0;  ///< 中间层尺寸
    int32_t sizeBotX = 0, sizeBotY = 0;  ///< 底层尺寸
    int32_t holeSize = 0;  ///< 孔径
    uint8_t shapeTop = 1;  ///< 顶层形状 (1=Round, 2=Rect, 3=Oct, 9=RndRect)
    uint8_t shapeMid = 1;  ///< 中间层焊盘形状。
    uint8_t shapeBot = 1;  ///< 底层焊盘形状。
    double rotation = 0.0;  ///< 旋转角度
    bool isPlated = true;  ///< 是否电镀
    uint8_t layer = 1;  ///< 所在层 (1=Top, 32=Bottom)
    uint16_t netIndex = 0xFFFF;  ///< 网络索引
    bool isSMD = false;  ///< 是否表贴（无孔）

    // ---- 扩展属性 ----
    uint8_t mode = 0;  ///< 焊盘模式（0=Simple, 1=Local Routed, 2=From To, 3=From Middle）
    uint8_t powerPlaneConnectStyle = 0;  ///< 电源层连接方式（0=Relief, 1=Direct, 2=No Connect）
    int32_t reliefAirGapRaw = 0;  ///< 电源层 Relief 气隙（原始单位）
    int32_t reliefConductorWidthRaw = 0;  ///< 电源层 Relief 导体宽度（原始单位）
    int16_t reliefEntries = 4;  ///< Relief 连接数（2 或 4）
    int32_t powerPlaneClearanceRaw = 0;  ///< 电源层间距（原始单位）
    int32_t powerPlaneReliefExpansionRaw = 0;  ///< 电源层 Relief 扩展（原始单位）
    int32_t pasteMaskExpansionRaw = 0;  ///< 锡膏层扩展（原始单位，0 = Rule）
    int32_t solderMaskExpansionRaw = 0;  ///< 阻焊层扩展（原始单位，0 = Rule）
    uint8_t drillType = 0;  ///< 钻孔类型（0=Round, 1=Square, 2=Slot）
    uint8_t holeType = 0;  ///< 孔类型（0=Round, 1=Square, 2=Slot）
    int32_t holeSlotLengthRaw = 0;  ///< 槽孔长度（原始单位）
    double holeRotation = 0.0;  ///< 孔旋转角度
    uint8_t cornerRadiusPercentage = 0;  ///< 圆角半径百分比（0-100）
    bool isLocked = false;  ///< 是否锁定
    bool isTentingTop = false;  ///< 顶部阻焊覆盖
    bool isTentingBottom = false;  ///< 底部阻焊覆盖
    bool isKeepout = false;  ///< 是否禁止布线区
};

/**
 * @brief Altium PCB 走线
 */
struct AltiumPcbTrack {
    int32_t startX = 0, startY = 0;  ///< 起点坐标（PCB 原始单位）
    int32_t endX = 0, endY = 0;  ///< 终点坐标（PCB 原始单位）
    int32_t width = 0;  ///< 线宽（PCB 原始单位）
    uint8_t layer = 1;  ///< 所在层（1=Top, 32=Bottom, 33=Top Overlay）
    uint16_t netIndex = 0xFFFF;  ///< 网络索引（0xFFFF = 无网络）
};

/**
 * @brief Altium PCB 弧线
 */
struct AltiumPcbArc {
    int32_t centerX = 0, centerY = 0;  ///< 圆心坐标（PCB 原始单位）
    int32_t radius = 0;  ///< 半径（PCB 原始单位）
    double startAngle = 0.0, endAngle = 360.0;  ///< 起始/结束角度
    int32_t width = 0;  ///< 线宽（PCB 原始单位）
    uint8_t layer = 1;  ///< 所在层
    uint16_t netIndex = 0xFFFF;  ///< 网络索引
};

/**
 * @brief Altium PCB 文本
 */
struct AltiumPcbText {
    int32_t locationX = 0, locationY = 0;  ///< 文本位置坐标（PCB 原始单位）。
    int32_t height = 60000;  ///< 文字高度（默认 6mil）
    int32_t strokeWidth = 10000;  ///< 笔画宽度（默认 1mil）
    double rotation = 0.0;  ///< 文本旋转角度。
    bool isMirrored = false;  ///< 是否镜像文本。
    uint8_t layer = 33;  ///< 默认 Top Overlay
    QString text;  ///< 文本内容。
};

/**
 * @brief Altium PCB 填充
 */
struct AltiumPcbFill {
    int32_t corner1X = 0, corner1Y = 0;  ///< 角点 1 坐标（PCB 原始单位）
    int32_t corner2X = 0, corner2Y = 0;  ///< 角点 2 坐标（PCB 原始单位）
    double rotation = 0.0;  ///< 旋转角度
    uint8_t layer = 1;  ///< 所在层
    uint16_t netIndex = 0xFFFF;  ///< 网络索引
};

/**
 * @brief Altium PCB 区域
 */
struct AltiumPcbRegion {
    QList<QPointF> vertices;  ///< 顶点（double 精度，PCB 原始单位）
    QList<QList<QPointF>> holes;  ///< 孔洞轮廓
    uint8_t layer = 1;  ///< 区域所在层。
    bool isBoardCutout = false;  ///< 是否为板框切割区域。
    int kind = 0;  ///< 区域类型编码。
    QString v7LayerName;  ///< V7 层名称（如 "MECHANICAL1", "TOP"）
    QString net;  ///< 网络名称
    QString uniqueId;  ///< 唯一标识
    QString name;  ///< 区域名称
};

/**
 * @brief Altium PCB 图元扩展信息
 * @details 用于记录自定义焊盘遮罩扩展等额外属性，
 *          写入 ExtendedPrimitiveInformation 流。
 */
struct AltiumPcbExtendedPrimitiveInfo {
    int primitiveIndex = 0;  ///< 图元在 Data 流中的序号
    QString objectName;  ///< 图元类型名（如 "Pad", "Region"）
    QMap<QString, QString> params;  ///< 扩展参数
};

/**
 * @brief Altium PCB 3D 元件体 (Object ID = 12)
 * @details 描述元件的 3D 外形体，关联 STEP/WRL 模型。
 *          每个元件体对应一个 MECHANICAL 层的闭合轮廓。
 */
struct AltiumPcbComponentBody {
    QString layerName = "MECHANICAL1";  ///< 所在层名称
    QString name = "__BODY__";  ///< 体名称
    int kind = 0;  ///< 体类型（0=Extruded, 1=Sphere, 2=Cylinder）
    int subpolyIndex = -1;  ///< 所属子多边形索引。
    int unionIndex = 0;  ///< 所属联合体索引。
    int32_t arcResolutionRaw = 5000;  ///< 弧线分辨率（0.5mil）
    bool isShapeBased = false;  ///< 是否由轮廓形状定义实体。
    int32_t cavityHeightRaw = 0;  ///< 腔体高度
    int32_t standoffHeightRaw = 0;  ///< 支架高度
    int32_t overallHeightRaw = 0;  ///< 总高度
    int32_t bodyColor3d = 0x808080;  ///< 3D 颜色
    double bodyOpacity3d = 1.0;  ///< 3D 不透明度
    int bodyProjection = 0;  ///< 投影方式
    QString modelId;  ///< 模型 ID（GUID）
    bool modelEmbed = true;  ///< 是否嵌入模型
    double model2dRotX = 0, model2dRotY = 0;  ///< 2D 模型绕 X/Y 轴的旋转角度。
    double model2dRotation = 0.0;  ///< 2D 模型旋转
    double model3dRotX = 0, model3dRotY = 0, model3dRotZ = 0;  ///< 3D 旋转
    int32_t model3dDzRaw = 0;  ///< 3D Z 偏移
    int32_t modelChecksum = 0;  ///< 模型校验和
    QString modelName;  ///< 模型文件名
    int modelType = 1;  ///< 模型类型（1=STEP, 2=WRL）
    QString modelSource = "Undefined";  ///< 模型来源
    QList<QPointF> outline;  ///< 轮廓顶点（原始单位）
};

/**
 * @brief Altium PCB 封装元件
 * @details IR 与 PcbLib 二进制协议之间的强类型边界，包含导出所需的 PCB 图元。
 */
struct AltiumPcbComponent {
    QString name;  ///< 封装名称
    QString description;  ///< 封装描述
    double height = 0.0;  ///< 元件高度（mil）

    QList<AltiumPcbPad> pads;  ///< 封装焊盘列表。
    QList<AltiumPcbTrack> tracks;  ///< 封装走线列表。
    QList<AltiumPcbArc> arcs;  ///< 封装弧线列表。
    QList<AltiumPcbText> texts;  ///< 封装文本列表。
    QList<AltiumPcbFill> fills;  ///< 封装填充区域列表。
    QList<AltiumPcbRegion> regions;  ///< 封装区域列表。
    QList<AltiumPcbComponentBody> bodies;  ///< 3D 元件体列表
    QList<AltiumPcbExtendedPrimitiveInfo> extendedPrimitives;  ///< 图元扩展信息

    /** @brief 3D 模型信息 */
    struct Model3D {
        QString name;  ///< 模型文件名
        QString id;  ///< 与 ComponentBody 关联的稳定 GUID
        QByteArray stepData;  ///< STEP 文件数据
        double rotX = 0, rotY = 0, rotZ = 0;  ///< 旋转角度
        double x = 0, y = 0;  ///< 模型相对封装原点的 XY 放置坐标（原始单位）
        double dz = 0;  ///< Z 偏移（mil）
    };

    QList<Model3D> models;  ///< 封装关联的 3D 模型列表。
};

}  // namespace EasyKiConverter
