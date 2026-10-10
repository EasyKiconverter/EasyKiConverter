#ifndef FOOTPRINTDATA_H
#define FOOTPRINTDATA_H

#include "Model3DData.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QPointF>
#include <QString>
#include <QStringList>

namespace EasyKiConverter {

// ==================== 封装信息 ====================

struct FootprintInfo {
    QString name;  ///< 名称。
    QString type;  ///< 对象类型。
    QString model3DName;  ///< 三维模型名称。
    QString description;  ///< 封装描述。

    // EasyEDA API 原始字段
    QString uuid;  ///< 源对象唯一标识。
    QString docType;  ///< 源文档类型。
    QString datastrid;  ///< 源数据串标识。
    bool writable;  ///< 是否允许编辑。
    qint64 updateTime;  ///< 更新时间戳。

    // 编辑器信
    QString editorVersion;  ///< 编辑器版本。

    // 项目信息
    QString puuid;  ///< 父对象标识。
    qint64 utime;  ///< 更新时间戳。
    bool importFlag;  ///< 是否由导入生成。
    bool hasIdFlag;  ///< 是否包含有效编号。
    bool newgId;  ///< 是否为新生成对象。

    // 附加参数
    QString link;  ///< 源链接。
    QString contributor;  ///< 贡献者名称。
    QString uuid3d;  ///< 三维模型标识。

    // 画布信息
    QString canvas;  ///< 画布名称。

    // 层定位
    QString layers;  ///< 层信息。

    // 对象可见
    QString objects;  ///< 对象信息。
};

// ==================== 边界====================

struct FootprintBBox {
    double x;  ///< X 坐标。
    double y;  ///< Y 坐标。
    double width;  ///< 宽度。
    double height;  ///< 高度。
};

// ==================== 焊盘 ====================

struct FootprintPad {
    QString shape;  ///< 焊盘形状。
    double centerX;  ///< 中心 X 坐标。
    double centerY;  ///< 中心 Y 坐标。
    double width;  ///< 宽度。
    double height;  ///< 高度。
    int layerId;  ///< 层编号。
    QString net;  ///< 网络名称。
    QString number;  ///< 编号。
    double holeRadius;  ///< 孔半径。
    QString points;  ///< 顶点数据。
    double rotation;  ///< 旋转角度。
    QString id;  ///< 对象唯一标识。
    double holeLength;  ///< 槽孔长度。
    QString holePoint;  ///< 槽孔定位点。
    bool isPlated;  ///< 是否有镀层。
    bool isLocked;  ///< 是否锁定。
};

// ==================== 走线 ====================

struct FootprintTrack {
    double strokeWidth;  ///< 描边宽度。
    int layerId;  ///< 层编号。
    QString net;  ///< 网络名称。
    QString points;  ///< 顶点数据。
    QString id;  ///< 对象唯一标识。
    bool isLocked;  ///< 是否锁定。
};

// ==================== 圆弧 ====================

struct FootprintHole {
    double centerX;  ///< 中心 X 坐标。
    double centerY;  ///< 中心 Y 坐标。
    double radius;  ///< 半径。
    QString id;  ///< 对象唯一标识。
    bool isLocked;  ///< 是否锁定。
};

// ==================== 圆弧 ====================

struct FootprintCircle {
    double cx;  ///< 中心 X 坐标。
    double cy;  ///< 中心 Y 坐标。
    double radius;  ///< 半径。
    double strokeWidth;  ///< 描边宽度。
    int layerId;  ///< 层编号。
    QString id;  ///< 对象唯一标识。
    bool isLocked;  ///< 是否锁定。
};

// ==================== 矩形 ====================

struct FootprintRectangle {
    double x;  ///< X 坐标。
    double y;  ///< Y 坐标。
    double width;  ///< 宽度。
    double height;  ///< 高度。
    double strokeWidth;  ///< 描边宽度。
    QString id;  ///< 对象唯一标识。
    int layerId;  ///< 层编号。
    bool isLocked;  ///< 是否锁定。
};

// ==================== 圆弧 ====================

struct FootprintArc {
    double strokeWidth;  ///< 描边宽度。
    int layerId;  ///< 层编号。
    QString net;  ///< 网络名称。
    QString path;  ///< 路径数据。
    QString helperDots;  ///< 辅助点数据。
    QString id;  ///< 对象唯一标识。
    bool isLocked;  ///< 是否锁定。
};

// ==================== 文本 ====================

struct FootprintText {
    QString type;  ///< 对象类型。
    double centerX;  ///< 中心 X 坐标。
    double centerY;  ///< 中心 Y 坐标。
    double strokeWidth;  ///< 描边宽度。
    int rotation;  ///< 旋转角度。
    QString mirror;  ///< 镜像方式。
    int layerId;  ///< 层编号。
    QString net;  ///< 网络名称。
    double fontSize;  ///< 字号。
    QString text;  ///< 文本内容。
    QString textPath;  ///< 文本路径。
    bool isDisplayed;  ///< 是否显示。
    QString id;  ///< 对象唯一标识。
    bool isLocked;  ///< 是否锁定。
};

// ==================== 实体填充区域 ====================
/**
 * @brief 实体填充区域，用于禁止布线区或元件占位区
 */
struct FootprintSolidRegion {
    QString path;  // 路径数据（如 "M x y L x y Z"）
    int layerId;  // 所属层（通常ComponentShapeLayer，ID=99）
    QString fillStyle;  // 填充样式（solid, none等）
    QString id;  // 唯一标识
    bool isKeepOut;  // 是否为禁止布线区
    bool isLocked;  // 是否锁定
};

// ==================== 外形轮廓 ====================
/**
 * @brief 器件外形轮廓，用于丝印标识和装配
 */
struct FootprintOutline {
    QString path;  // SVG 路径或多边形点序
    int layerId;  // 所属层（通常TopSilkLayer 或 3DModel 层）
    double strokeWidth;  // 线宽
    QString id;  // 唯一标识
    bool isLocked;  // 是否锁定
};

// ==================== 层定位====================
/**
 * @brief PCB 层定义信
     */
struct LayerDefinition {
    int layerId;  // 层 ID
    QString name;  // 层名
    QString color;  // 层颜色（#RRGGBB）
    bool isVisible;  // 是否可见
    bool isUsedForManufacturing;  // 是否用于制
    double expansion = 0.0;  // 扩展值（如阻焊层扩展
};

// ==================== 对象可见性配====================
/**
 * @brief 对象类型可见性配
     */
struct ObjectVisibility {
    QString objectType;  // 对象类型（Pad, Track, Text等）
    bool isEnabled;  // 是否启用
    bool isVisible;  // 是否可见
};

// ==================== 封装数据 ====================

class FootprintData {
public:
    FootprintData();
    ~FootprintData() = default;

    // Getter 和 Setter 方法
    /**
     * @brief 返回封装元数据。
     * @return 封装元数据。
     */
    FootprintInfo info() const {
        return m_info;
    }

    /**
     * @brief 设置封装元数据。
     */
    /**
     * @brief 设置封装元数据。
     * @param info 参数。
     */
    void setInfo(const FootprintInfo& info) {
        m_info = info;
    }

    /**
     * @brief 返回封装边界框。
     */
    FootprintBBox bbox() const {
        return m_bbox;
    }

    /**
     * @brief 设置封装边界框。
     */
    /**
     * @brief 设置封装边界框。
     * @param bbox 参数。
     */
    void setBbox(const FootprintBBox& bbox) {
        m_bbox = bbox;
    }

    /**
     * @brief 返回全部焊盘。
     */
    QList<FootprintPad> pads() const {
        return m_pads;
    }

    /**
     * @brief 批量设置焊盘。
     */
    /**
     * @brief 批量设置焊盘。
     * @param pads 参数。
     */
    void setPads(const QList<FootprintPad>& pads) {
        m_pads = pads;
    }

    /**
     * @brief 追加一个焊盘。
     */
    /**
     * @brief 追加一个焊盘。
     * @param pad 参数。
     */
    void addPad(const FootprintPad& pad) {
        m_pads.append(pad);
    }

    /**
     * @brief 返回全部铜线路径。
     */
    QList<FootprintTrack> tracks() const {
        return m_tracks;
    }

    /**
     * @brief 批量设置铜线路径。
     */
    /**
     * @brief 批量设置铜线路径。
     * @param tracks 参数。
     */
    void setTracks(const QList<FootprintTrack>& tracks) {
        m_tracks = tracks;
    }

    /**
     * @brief 追加一条铜线路径。
     */
    /**
     * @brief 追加一条铜线路径。
     * @param track 参数。
     */
    void addTrack(const FootprintTrack& track) {
        m_tracks.append(track);
    }

    /**
     * @brief 返回全部孔。
     */
    QList<FootprintHole> holes() const {
        return m_holes;
    }

    /**
     * @brief 批量设置孔。
     */
    /**
     * @brief 批量设置孔。
     * @param holes 参数。
     */
    void setHoles(const QList<FootprintHole>& holes) {
        m_holes = holes;
    }

    /**
     * @brief 追加一个孔。
     */
    /**
     * @brief 追加一个孔。
     * @param hole 参数。
     */
    void addHole(const FootprintHole& hole) {
        m_holes.append(hole);
    }

    /**
     * @brief 返回全部圆形图元。
     */
    QList<FootprintCircle> circles() const {
        return m_circles;
    }

    /**
     * @brief 批量设置圆形图元。
     */
    /**
     * @brief 批量设置圆形图元。
     * @param circles 参数。
     */
    void setCircles(const QList<FootprintCircle>& circles) {
        m_circles = circles;
    }

    /**
     * @brief 追加一个圆形图元。
     */
    /**
     * @brief 追加一个圆形图元。
     * @param circle 参数。
     */
    void addCircle(const FootprintCircle& circle) {
        m_circles.append(circle);
    }

    /**
     * @brief 返回全部矩形图元。
     */
    QList<FootprintRectangle> rectangles() const {
        return m_rectangles;
    }

    /**
     * @brief 批量设置矩形图元。
     */
    /**
     * @brief 批量设置矩形图元。
     * @param rectangles 参数。
     */
    void setRectangles(const QList<FootprintRectangle>& rectangles) {
        m_rectangles = rectangles;
    }

    /**
     * @brief 追加一个矩形图元。
     */
    /**
     * @brief 追加一个矩形图元。
     * @param rect 参数。
     */
    void addRectangle(const FootprintRectangle& rect) {
        m_rectangles.append(rect);
    }

    /**
     * @brief 返回全部圆弧图元。
     */
    QList<FootprintArc> arcs() const {
        return m_arcs;
    }

    /**
     * @brief 批量设置圆弧图元。
     */
    /**
     * @brief 批量设置圆弧图元。
     * @param arcs 参数。
     */
    void setArcs(const QList<FootprintArc>& arcs) {
        m_arcs = arcs;
    }

    /**
     * @brief 追加一个圆弧图元。
     */
    /**
     * @brief 追加一个圆弧图元。
     * @param arc 参数。
     */
    void addArc(const FootprintArc& arc) {
        m_arcs.append(arc);
    }

    /**
     * @brief 返回全部文本图元。
     */
    QList<FootprintText> texts() const {
        return m_texts;
    }

    /**
     * @brief 批量设置文本图元。
     */
    /**
     * @brief 批量设置文本图元。
     * @param texts 参数。
     */
    void setTexts(const QList<FootprintText>& texts) {
        m_texts = texts;
    }

    /**
     * @brief 追加一个文本图元。
     */
    /**
     * @brief 追加一个文本图元。
     * @param text 参数。
     */
    void addText(const FootprintText& text) {
        m_texts.append(text);
    }

    /**
     * @brief 返回全部实心区域。
     */
    QList<FootprintSolidRegion> solidRegions() const {
        return m_solidRegions;
    }

    /**
     * @brief 批量设置实心区域。
     */
    /**
     * @brief 批量设置实心区域。
     * @param solidRegions 参数。
     */
    void setSolidRegions(const QList<FootprintSolidRegion>& solidRegions) {
        m_solidRegions = solidRegions;
    }

    /**
     * @brief 追加一个实心区域。
     */
    /**
     * @brief 追加一个实心区域。
     * @param solidRegion 参数。
     */
    void addSolidRegion(const FootprintSolidRegion& solidRegion) {
        m_solidRegions.append(solidRegion);
    }

    /**
     * @brief 返回全部封装轮廓。
     */
    QList<FootprintOutline> outlines() const {
        return m_outlines;
    }

    /**
     * @brief 批量设置封装轮廓。
     */
    /**
     * @brief 批量设置封装轮廓。
     * @param outlines 参数。
     */
    void setOutlines(const QList<FootprintOutline>& outlines) {
        m_outlines = outlines;
    }

    /**
     * @brief 追加一个封装轮廓。
     */
    /**
     * @brief 追加一个封装轮廓。
     * @param outline 参数。
     */
    void addOutline(const FootprintOutline& outline) {
        m_outlines.append(outline);
    }

    /**
     * @brief 返回全部层定义。
     */
    QList<LayerDefinition> layers() const {
        return m_layers;
    }

    /**
     * @brief 批量设置层定义。
     */
    /**
     * @brief 批量设置层定义。
     * @param layers 参数。
     */
    void setLayers(const QList<LayerDefinition>& layers) {
        m_layers = layers;
    }

    /**
     * @brief 追加一个层定义。
     */
    /**
     * @brief 追加一个层定义。
     * @param layer 参数。
     */
    void addLayer(const LayerDefinition& layer) {
        m_layers.append(layer);
    }

    /**
     * @brief 返回全部对象可见性设置。
     */
    QList<ObjectVisibility> objectVisibilities() const {
        return m_objectVisibilities;
    }

    /**
     * @brief 批量设置对象可见性。
     */
    /**
     * @brief 批量设置对象可见性。
     * @param objectVisibilities 参数。
     */
    void setObjectVisibilities(const QList<ObjectVisibility>& objectVisibilities) {
        m_objectVisibilities = objectVisibilities;
    }

    /**
     * @brief 追加一个对象可见性设置。
     */
    /**
     * @brief 追加一个对象可见性设置。
     * @param objectVisibility 参数。
     */
    void addObjectVisibility(const ObjectVisibility& objectVisibility) {
        m_objectVisibilities.append(objectVisibility);
    }

    /**
     * @brief 返回封装关联的三维模型数据。
     */
    Model3DData model3D() const {
        return m_model3D;
    }

    /**
     * @brief 设置封装关联的三维模型数据。
     */
    /**
     * @brief 设置封装关联的三维模型数据。
     * @param model3D 参数。
     */
    void setModel3D(const Model3DData& model3D) {
        m_model3D = model3D;
    }

    // JSON 序列
    QJsonObject toJson() const;
    bool fromJson(const QJsonObject& json);

    // 数据验证
    bool isValid() const;
    QString validate() const;

    /**
     * @brief 获取导入阶段保留的非致命诊断。
     * @return 去重后的诊断列表。
     */
    QStringList validationErrors() const {
        return m_validationErrors;
    }

    /**
     * @brief 添加导入阶段诊断。
     * @param error 要添加的诊断文本。
     */
    void addValidationError(const QString& error) {
        if (!error.isEmpty() && !m_validationErrors.contains(error))
            m_validationErrors.append(error);
    }

    // 清空数据
    void clear();

private:
    FootprintInfo m_info{};
    FootprintBBox m_bbox{};
    QList<FootprintPad> m_pads;  ///< 焊盘列表。
    QList<FootprintTrack> m_tracks;  ///< 铜线路径列表。
    QList<FootprintHole> m_holes;  ///< 孔列表。
    QList<FootprintCircle> m_circles;  ///< 圆形图元列表。
    QList<FootprintRectangle> m_rectangles;  ///< 矩形图元列表。
    QList<FootprintArc> m_arcs;  ///< 圆弧图元列表。
    QList<FootprintText> m_texts;  ///< 文本图元列表。
    QList<FootprintSolidRegion> m_solidRegions;  ///< 实体区域列表。
    QList<FootprintOutline> m_outlines;  ///< 轮廓列表。
    QList<LayerDefinition> m_layers;  ///< 层定义列表。
    QList<ObjectVisibility> m_objectVisibilities;  ///< 可见性列表。
    Model3DData m_model3D;  ///< 三维模型数据。
    QStringList m_validationErrors;  ///< 导入诊断列表。
};

}  // namespace EasyKiConverter

#endif  // FOOTPRINTDATA_H
