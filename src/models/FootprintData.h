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
    QString name;
    QString type;
    QString model3DName;
    QString description;

    // EasyEDA API 原始字段
    QString uuid;
    QString docType;
    QString datastrid;
    bool writable;
    qint64 updateTime;

    // 编辑器信
    QString editorVersion;

    // 项目信息
    QString puuid;
    qint64 utime;
    bool importFlag;
    bool hasIdFlag;
    bool newgId;

    // 附加参数
    QString link;
    QString contributor;
    QString uuid3d;

    // 画布信息
    QString canvas;

    // 层定位
    QString layers;

    // 对象可见
    QString objects;
};

// ==================== 边界====================

struct FootprintBBox {
    double x;
    double y;
    double width;
    double height;
};

// ==================== 焊盘 ====================

struct FootprintPad {
    QString shape;
    double centerX;
    double centerY;
    double width;
    double height;
    int layerId;
    QString net;
    QString number;
    double holeRadius;
    QString points;
    double rotation;
    QString id;
    double holeLength;
    QString holePoint;
    bool isPlated;
    bool isLocked;
};

// ==================== 走线 ====================

struct FootprintTrack {
    double strokeWidth;
    int layerId;
    QString net;
    QString points;
    QString id;
    bool isLocked;
};

// ==================== 圆弧 ====================

struct FootprintHole {
    double centerX;
    double centerY;
    double radius;
    QString id;
    bool isLocked;
};

// ==================== 圆弧 ====================

struct FootprintCircle {
    double cx;
    double cy;
    double radius;
    double strokeWidth;
    int layerId;
    QString id;
    bool isLocked;
};

// ==================== 矩形 ====================

struct FootprintRectangle {
    double x;
    double y;
    double width;
    double height;
    double strokeWidth;
    QString id;
    int layerId;
    bool isLocked;
};

// ==================== 圆弧 ====================

struct FootprintArc {
    double strokeWidth;
    int layerId;
    QString net;
    QString path;
    QString helperDots;
    QString id;
    bool isLocked;
};

// ==================== 文本 ====================

struct FootprintText {
    QString type;
    double centerX;
    double centerY;
    double strokeWidth;
    int rotation;
    QString mirror;
    int layerId;
    QString net;
    double fontSize;
    QString text;
    QString textPath;
    bool isDisplayed;
    QString id;
    bool isLocked;
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
    FootprintInfo info() const {
        return m_info;
    }

    /** @brief 设置封装元数据。 */
    void setInfo(const FootprintInfo& info) {
        m_info = info;
    }

    /** @brief 返回封装边界框。 */
    FootprintBBox bbox() const {
        return m_bbox;
    }

    /** @brief 设置封装边界框。 */
    void setBbox(const FootprintBBox& bbox) {
        m_bbox = bbox;
    }

    /** @brief 返回全部焊盘。 */
    QList<FootprintPad> pads() const {
        return m_pads;
    }

    /** @brief 批量设置焊盘。 */
    void setPads(const QList<FootprintPad>& pads) {
        m_pads = pads;
    }

    /** @brief 追加一个焊盘。 */
    void addPad(const FootprintPad& pad) {
        m_pads.append(pad);
    }

    /** @brief 返回全部铜线路径。 */
    QList<FootprintTrack> tracks() const {
        return m_tracks;
    }

    /** @brief 批量设置铜线路径。 */
    void setTracks(const QList<FootprintTrack>& tracks) {
        m_tracks = tracks;
    }

    /** @brief 追加一条铜线路径。 */
    void addTrack(const FootprintTrack& track) {
        m_tracks.append(track);
    }

    /** @brief 返回全部孔。 */
    QList<FootprintHole> holes() const {
        return m_holes;
    }

    /** @brief 批量设置孔。 */
    void setHoles(const QList<FootprintHole>& holes) {
        m_holes = holes;
    }

    /** @brief 追加一个孔。 */
    void addHole(const FootprintHole& hole) {
        m_holes.append(hole);
    }

    /** @brief 返回全部圆形图元。 */
    QList<FootprintCircle> circles() const {
        return m_circles;
    }

    /** @brief 批量设置圆形图元。 */
    void setCircles(const QList<FootprintCircle>& circles) {
        m_circles = circles;
    }

    /** @brief 追加一个圆形图元。 */
    void addCircle(const FootprintCircle& circle) {
        m_circles.append(circle);
    }

    /** @brief 返回全部矩形图元。 */
    QList<FootprintRectangle> rectangles() const {
        return m_rectangles;
    }

    /** @brief 批量设置矩形图元。 */
    void setRectangles(const QList<FootprintRectangle>& rectangles) {
        m_rectangles = rectangles;
    }

    /** @brief 追加一个矩形图元。 */
    void addRectangle(const FootprintRectangle& rect) {
        m_rectangles.append(rect);
    }

    /** @brief 返回全部圆弧图元。 */
    QList<FootprintArc> arcs() const {
        return m_arcs;
    }

    /** @brief 批量设置圆弧图元。 */
    void setArcs(const QList<FootprintArc>& arcs) {
        m_arcs = arcs;
    }

    /** @brief 追加一个圆弧图元。 */
    void addArc(const FootprintArc& arc) {
        m_arcs.append(arc);
    }

    /** @brief 返回全部文本图元。 */
    QList<FootprintText> texts() const {
        return m_texts;
    }

    /** @brief 批量设置文本图元。 */
    void setTexts(const QList<FootprintText>& texts) {
        m_texts = texts;
    }

    /** @brief 追加一个文本图元。 */
    void addText(const FootprintText& text) {
        m_texts.append(text);
    }

    /** @brief 返回全部实心区域。 */
    QList<FootprintSolidRegion> solidRegions() const {
        return m_solidRegions;
    }

    /** @brief 批量设置实心区域。 */
    void setSolidRegions(const QList<FootprintSolidRegion>& solidRegions) {
        m_solidRegions = solidRegions;
    }

    /** @brief 追加一个实心区域。 */
    void addSolidRegion(const FootprintSolidRegion& solidRegion) {
        m_solidRegions.append(solidRegion);
    }

    /** @brief 返回全部封装轮廓。 */
    QList<FootprintOutline> outlines() const {
        return m_outlines;
    }

    /** @brief 批量设置封装轮廓。 */
    void setOutlines(const QList<FootprintOutline>& outlines) {
        m_outlines = outlines;
    }

    /** @brief 追加一个封装轮廓。 */
    void addOutline(const FootprintOutline& outline) {
        m_outlines.append(outline);
    }

    /** @brief 返回全部层定义。 */
    QList<LayerDefinition> layers() const {
        return m_layers;
    }

    /** @brief 批量设置层定义。 */
    void setLayers(const QList<LayerDefinition>& layers) {
        m_layers = layers;
    }

    /** @brief 追加一个层定义。 */
    void addLayer(const LayerDefinition& layer) {
        m_layers.append(layer);
    }

    /** @brief 返回全部对象可见性设置。 */
    QList<ObjectVisibility> objectVisibilities() const {
        return m_objectVisibilities;
    }

    /** @brief 批量设置对象可见性。 */
    void setObjectVisibilities(const QList<ObjectVisibility>& objectVisibilities) {
        m_objectVisibilities = objectVisibilities;
    }

    /** @brief 追加一个对象可见性设置。 */
    void addObjectVisibility(const ObjectVisibility& objectVisibility) {
        m_objectVisibilities.append(objectVisibility);
    }

    /** @brief 返回封装关联的三维模型数据。 */
    Model3DData model3D() const {
        return m_model3D;
    }

    /** @brief 设置封装关联的三维模型数据。 */
    void setModel3D(const Model3DData& model3D) {
        m_model3D = model3D;
    }

    // JSON 序列
    QJsonObject toJson() const;
    bool fromJson(const QJsonObject& json);

    // 数据验证
    bool isValid() const;
    QString validate() const;

    /** @brief 获取导入阶段保留的非致命诊断 */
    QStringList validationErrors() const {
        return m_validationErrors;
    }

    /** @brief 添加导入阶段诊断 */
    void addValidationError(const QString& error) {
        if (!error.isEmpty() && !m_validationErrors.contains(error))
            m_validationErrors.append(error);
    }

    // 清空数据
    void clear();

private:
    FootprintInfo m_info{};
    FootprintBBox m_bbox{};
    QList<FootprintPad> m_pads;
    QList<FootprintTrack> m_tracks;
    QList<FootprintHole> m_holes;
    QList<FootprintCircle> m_circles;
    QList<FootprintRectangle> m_rectangles;
    QList<FootprintArc> m_arcs;
    QList<FootprintText> m_texts;
    QList<FootprintSolidRegion> m_solidRegions;
    QList<FootprintOutline> m_outlines;
    QList<LayerDefinition> m_layers;
    QList<ObjectVisibility> m_objectVisibilities;
    Model3DData m_model3D;
    QStringList m_validationErrors;
};

}  // namespace EasyKiConverter

#endif  // FOOTPRINTDATA_H
