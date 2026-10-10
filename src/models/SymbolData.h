#ifndef SYMBOLDATA_H
#define SYMBOLDATA_H

#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QStringList>

namespace EasyKiConverter {

// ==================== 枚举类型 ====================

/**
 * @brief 引脚类型枚举
 */
enum class PinType { Unspecified = 0, Input = 1, Output = 2, Bidirectional = 3, Power = 4 };

/**
 * @brief 引脚样式枚举
 */
enum class PinStyle { Line, Inverted, Clock, InvertedClock, InputLow, ClockLow, OutputLow, EdgeClockHigh, NonLogic };

// ==================== 符号信息 ====================

/**
 * @brief 符号信息
 */
struct SymbolInfo {
    QString name;  ///< 符号名称。
    QString prefix;  ///< 默认参考标号前缀。
    QString package;  ///< 关联封装名称。
    QString manufacturer;  ///< 制造商名称。
    QString description;  ///< 符号描述。
    QString datasheet;  ///< 数据手册地址。
    QString lcscId;  ///< LCSC 器件编号。
    QString jlcId;  ///< 嘉立创器件编号。
    QStringList aliases;  ///< 符号别名列表。

    // EasyEDA API 原始字段
    QString uuid;  ///< EasyEDA 对象唯一标识。
    QString title;  ///< EasyEDA 原始标题。
    QString docType;  ///< EasyEDA 文档类型。
    QString type;  ///< EasyEDA 对象类型。
    QString thumb;  ///< 缩略图地址。
    QString datastrid;  ///< EasyEDA 数据串标识。
    bool jlcOnSale;  ///< 是否在嘉立创商城销售。
    bool writable;  ///< 是否允许编辑。
    bool isFavorite;  ///< 是否被用户收藏。
    bool verify;  ///< 是否通过源数据校验。
    bool smt;  ///< 是否为表面贴装器件。

    // 时间
    qint64 updateTime;  ///< 源数据更新时间戳。
    QString updatedAt;  ///< 源数据更新时间文本。

    // 编辑器信
    QString editorVersion;  ///< 源编辑器版本。

    // 项目信息
    QString puuid;  ///< 所属工程或父对象标识。
    qint64 utime;  ///< 工程更新时间戳。
    bool importFlag;  ///< 是否由导入流程生成。
    bool hasIdFlag;  ///< 是否包含有效源编号。

    // 附加参数
    QString timeStamp;  ///< 源数据时间戳文本。
    QString subpartNo;  ///< 子部件编号。
    QString supplierPart;  ///< 供应商器件编号。
    QString supplier;  ///< 供应商名称。
    QString manufacturerPart;  ///< 制造商器件编号。
    QString jlcpcbPartClass;  ///< 嘉立创器件分类。
};

// ==================== 边界====================

/**
 * @brief 边界
 */
struct SymbolBBox {
    double x = 0.0;  ///< 边界左上角 X 坐标。
    double y = 0.0;  ///< 边界左上角 Y 坐标。
    double width = 0.0;  ///< 边界宽度。
    double height = 0.0;  ///< 边界高度。
    double headX = 0.0;  ///< 参考中心 X 坐标。
    double headY = 0.0;  ///< 参考中心 Y 坐标。
    bool hasHeadCenter = false;  ///< 是否存在有效参考中心。
};

// ==================== 引脚相关数据结构 ====================

/**
 * @brief 引脚设置
 */
struct SymbolPinSettings {
    bool isDisplayed = true;  ///< 是否显示引脚。
    PinType type = PinType::Unspecified;  ///< 引脚电气类型。
    QString spicePinNumber;  ///< SPICE 引脚编号。
    double posX = 0.0;  ///< 引脚位置 X 坐标。
    double posY = 0.0;  ///< 引脚位置 Y 坐标。
    int rotation = 0;  ///< 引脚旋转角度。
    QString id;  ///< 引脚唯一标识。
    bool isLocked = false;  ///< 是否锁定引脚编辑。
};

/**
 * @brief 引脚
 */
struct SymbolPinDot {
    double dotX = 0.0;  ///< 连接点 X 坐标。
    double dotY = 0.0;  ///< 连接点 Y 坐标。
};

/**
 * @brief 引脚路径
 */
struct SymbolPinPath {
    QString path;  ///< 路径数据。
    QString color;  ///< 颜色。
};

/**
 * @brief 引脚名称
 */
struct SymbolPinName {
    bool isDisplayed = false;  ///< 是否显示。
    double posX = 0.0;  ///< 位置 X 坐标。
    double posY = 0.0;  ///< 位置 Y 坐标。
    int rotation = 0;  ///< 旋转角度。
    QString text;  ///< 文本内容。
    QString textAnchor;  ///< 文本锚点。
    QString font;  ///< 字体名称。
    double fontSize = 7.0;  ///< 字体大小。
};

/**
 * @brief 引脚圆点（第二个
 */
struct SymbolPinDotBis {
    bool isDisplayed = false;  ///< 是否显示。
    double circleX = 0.0;  ///< 圆点 X 坐标。
    double circleY = 0.0;  ///< 圆点 Y 坐标。
};

/**
 * @brief 引脚时钟
 */
struct SymbolPinClock {
    bool isDisplayed = false;  ///< 是否显示。
    QString path;  ///< 路径数据。
};

/**
 * @brief 引脚
 */
struct SymbolPin {
    SymbolPinSettings settings;  ///< 引脚设置。
    SymbolPinDot pinDot;  ///< 引脚连接点。
    SymbolPinPath pinPath;  ///< 引脚路径。
    SymbolPinName name;  ///< 名称。
    SymbolPinName number;  ///< 引脚编号的原始文本几何属性
    SymbolPinDotBis dot;  ///< 引脚圆点属性。
    SymbolPinClock clock;  ///< 引脚时钟属性。
};

// ==================== 图形元素 ====================

/**
 * @brief 矩形
 */
struct SymbolRectangle {
    double posX;  ///< 位置 X 坐标。
    double posY;  ///< 位置 Y 坐标。
    double rx;  ///< rx 字段值。
    double ry;  ///< ry 字段值。
    double width;  ///< 宽度。
    double height;  ///< 高度。
    QString strokeColor;  ///< 描边颜色。
    double strokeWidth;  ///< 描边宽度。
    QString strokeStyle;  ///< 描边样式。
    QString fillColor;  ///< 填充颜色或状态。
    QString id;  ///< 对象唯一标识。
    bool isLocked;  ///< 是否锁定。
};

/**
 * @brief 符号部分数据
 */
struct SymbolCircle {
    double centerX;  ///< 中心 X 坐标。
    double centerY;  ///< 中心 Y 坐标。
    double radius;  ///< 半径。
    QString strokeColor;  ///< 描边颜色。
    double strokeWidth;  ///< 描边宽度。
    QString strokeStyle;  ///< 描边样式。
    bool fillColor;  ///< 填充颜色或状态。
    QString id;  ///< 对象唯一标识。
    bool isLocked;  ///< 是否锁定。
};

/**
 * @brief 圆弧
 */
struct SymbolArc {
    QList<QPointF> path;  ///< 路径数据。
    QString helperDots;  ///< 辅助点数据。
    QString strokeColor;  ///< 描边颜色。
    double strokeWidth;  ///< 描边宽度。
    QString strokeStyle;  ///< 描边样式。
    bool fillColor;  ///< 填充颜色或状态。
    QString id;  ///< 对象唯一标识。
    bool isLocked;  ///< 是否锁定。
};

/**
 * @brief 椭圆
 */
struct SymbolEllipse {
    double centerX;  ///< 中心 X 坐标。
    double centerY;  ///< 中心 Y 坐标。
    double radiusX;  ///< X 方向半径。
    double radiusY;  ///< Y 方向半径。
    QString strokeColor;  ///< 描边颜色。
    double strokeWidth;  ///< 描边宽度。
    QString strokeStyle;  ///< 描边样式。
    bool fillColor;  ///< 填充颜色或状态。
    QString id;  ///< 对象唯一标识。
    bool isLocked;  ///< 是否锁定。
};

/**
 * @brief 多段
 */
struct SymbolPolyline {
    QString points;  ///< 顶点数据。
    QString strokeColor;  ///< 描边颜色。
    double strokeWidth;  ///< 描边宽度。
    QString strokeStyle;  ///< 描边样式。
    bool fillColor;  ///< 填充颜色或状态。
    QString id;  ///< 对象唯一标识。
    bool isLocked;  ///< 是否锁定。
};

/**
 * @brief 多边
 */
struct SymbolPolygon {
    QString points;  ///< 顶点数据。
    QString strokeColor;  ///< 描边颜色。
    double strokeWidth;  ///< 描边宽度。
    QString strokeStyle;  ///< 描边样式。
    bool fillColor;  ///< 填充颜色或状态。
    QString id;  ///< 对象唯一标识。
    bool isLocked;  ///< 是否锁定。
};

/**
 * @brief 路径
 */
struct SymbolPath {
    QString paths;  ///< 路径数据。
    QString strokeColor;  ///< 描边颜色。
    double strokeWidth;  ///< 描边宽度。
    QString strokeStyle;  ///< 描边样式。
    bool fillColor;  ///< 填充颜色或状态。
    QString id;  ///< 对象唯一标识。
    bool isLocked;  ///< 是否锁定。
};

/**
 * @brief EasyEDA 符号图片
 * @details source 保存外部 URL 或原始 data URL；data 非空时表示可直接嵌入的图片数据。
 */
struct SymbolImage {
    double posX = 0.0;  ///< 位置 X 坐标。
    double posY = 0.0;  ///< 位置 Y 坐标。
    double width = 0.0;  ///< 宽度。
    double height = 0.0;  ///< 高度。
    double rotation = 0.0;  ///< 旋转角度。
    QString source;  ///< 图片来源。
    QString fileName;  ///< 图片文件名。
    QByteArray data;  ///< 图片二进制数据。
    bool isLocked = false;  ///< 是否锁定。
};

/**
 * @brief 文本
 */
struct SymbolText {
    QString mark;  ///< 文本标记。
    double posX;  ///< 位置 X 坐标。
    double posY;  ///< 位置 Y 坐标。
    double rotation;  ///< 旋转角度。
    QString color;  ///< 颜色。
    QString font;  ///< 字体名称。
    double textSize;  ///< 文本字号。
    bool bold;  ///< 是否粗体。
    QString italic;  ///< 斜体样式。
    QString baseline;  ///< 文本基线。
    QString type;  ///< type 字段值。
    QString text;  ///< 文本内容。
    bool visible;  ///< 是否可见。
    QString anchor;  ///< 文本锚点。
    QString id;  ///< 对象唯一标识。
    bool isLocked;  ///< 是否锁定。
};

/**
 * @brief 符号图元在源数据中的绘制顺序引用
 * @details type 使用 EasyEDA shape designator（如 P、R、C、T），index 指向对应类型列表中的元素。
 */
struct SymbolGraphicOrder {
    QString type;  ///< type 字段值。
    int index = -1;  ///< 列表索引。
};

// ==================== 符号部分 ====================

/**
 * @brief 符号部分（用于多部分符号）
 */
struct SymbolPart {
    int unitNumber = 0;  // 部分编号（从 0 开始）
    double originX = 0.0;  // 子部分的坐标原点 X（从 EasyEDA head.x）
    double originY = 0.0;  // 子部分的坐标原点 Y（从 EasyEDA head.y）
    bool commonToAllParts = false;  // 是否为公共 Part Zero；序列化键为 common_to_all_parts
    QList<SymbolPin> pins;  ///< 引脚列表。
    QList<SymbolRectangle> rectangles;  ///< 矩形图元列表。
    QList<SymbolCircle> circles;  ///< 圆形图元列表。
    QList<SymbolArc> arcs;  ///< 圆弧图元列表。
    QList<SymbolEllipse> ellipses;  ///< 椭圆图元列表。
    QList<SymbolPolyline> polylines;  ///< 折线图元列表。
    QList<SymbolPolygon> polygons;  ///< 多边形图元列表。
    QList<SymbolPath> paths;  ///< 路径数据。
    QList<SymbolImage> images;  ///< 图片图元列表。
    QList<SymbolText> texts;  ///< 文本图元列表。
    QList<SymbolGraphicOrder> graphicOrder;  ///< 图元绘制顺序列表。
};

// ==================== 符号数据 ====================

/**
 * @brief 符号数据
 *
 * 包含符号的所有信息，包括引脚和各种图形元
 */
class SymbolData {
public:
    SymbolData();
    ~SymbolData() = default;

    // Getter 和 Setter 方法
    /**
     * @brief 返回符号元数据。
     * @return 符号元数据。
     */
    SymbolInfo info() const {
        return m_info;
    }

    /**
     * @brief 设置符号元数据。
     * @param info 参数。
     */
    void setInfo(const SymbolInfo& info) {
        m_info = info;
    }

    /**
     * @brief 返回符号边界框。
     */
    SymbolBBox bbox() const {
        return m_bbox;
    }

    /**
     * @brief 设置符号边界框。
     * @param bbox 参数。
     */
    void setBbox(const SymbolBBox& bbox) {
        m_bbox = bbox;
    }

    // 单部分符号的兼容接口（向后兼容）
    /**
     * @brief 返回单部分兼容接口中的全部引脚。
     */
    QList<SymbolPin> pins() const {
        return m_pins;
    }

    /**
     * @brief 批量设置单部分兼容接口中的引脚。
     * @param pins 参数。
     */
    void setPins(const QList<SymbolPin>& pins) {
        m_pins = pins;
    }

    /**
     * @brief 追加一个单部分兼容接口引脚。
     * @param pin 参数。
     */
    void addPin(const SymbolPin& pin) {
        m_pins.append(pin);
    }

    /**
     * @brief 返回全部矩形图元。
     */
    QList<SymbolRectangle> rectangles() const {
        return m_rectangles;
    }

    /**
     * @brief 批量设置矩形图元。
     * @param rectangles 参数。
     */
    void setRectangles(const QList<SymbolRectangle>& rectangles) {
        m_rectangles = rectangles;
    }

    /**
     * @brief 追加一个矩形图元。
     * @param rect 参数。
     */
    void addRectangle(const SymbolRectangle& rect) {
        m_rectangles.append(rect);
    }

    /**
     * @brief 返回全部圆形图元。
     */
    QList<SymbolCircle> circles() const {
        return m_circles;
    }

    /**
     * @brief 批量设置圆形图元。
     * @param circles 参数。
     */
    void setCircles(const QList<SymbolCircle>& circles) {
        m_circles = circles;
    }

    /**
     * @brief 追加一个圆形图元。
     * @param circle 参数。
     */
    void addCircle(const SymbolCircle& circle) {
        m_circles.append(circle);
    }

    /**
     * @brief 返回全部圆弧图元。
     */
    QList<SymbolArc> arcs() const {
        return m_arcs;
    }

    /**
     * @brief 批量设置圆弧图元。
     * @param arcs 参数。
     */
    void setArcs(const QList<SymbolArc>& arcs) {
        m_arcs = arcs;
    }

    /**
     * @brief 追加一个圆弧图元。
     * @param arc 参数。
     */
    void addArc(const SymbolArc& arc) {
        m_arcs.append(arc);
    }

    /**
     * @brief 返回全部椭圆图元。
     */
    QList<SymbolEllipse> ellipses() const {
        return m_ellipses;
    }

    /**
     * @brief 批量设置椭圆图元。
     * @param ellipses 参数。
     */
    void setEllipses(const QList<SymbolEllipse>& ellipses) {
        m_ellipses = ellipses;
    }

    /**
     * @brief 追加一个椭圆图元。
     * @param ellipse 参数。
     */
    void addEllipse(const SymbolEllipse& ellipse) {
        m_ellipses.append(ellipse);
    }

    /**
     * @brief 返回全部折线图元。
     */
    QList<SymbolPolyline> polylines() const {
        return m_polylines;
    }

    /**
     * @brief 批量设置折线图元。
     * @param polylines 参数。
     */
    void setPolylines(const QList<SymbolPolyline>& polylines) {
        m_polylines = polylines;
    }

    /**
     * @brief 追加一个折线图元。
     * @param polyline 参数。
     */
    void addPolyline(const SymbolPolyline& polyline) {
        m_polylines.append(polyline);
    }

    /**
     * @brief 返回全部多边形图元。
     */
    QList<SymbolPolygon> polygons() const {
        return m_polygons;
    }

    /**
     * @brief 批量设置多边形图元。
     * @param polygons 参数。
     */
    void setPolygons(const QList<SymbolPolygon>& polygons) {
        m_polygons = polygons;
    }

    /**
     * @brief 追加一个多边形图元。
     * @param polygon 参数。
     */
    void addPolygon(const SymbolPolygon& polygon) {
        m_polygons.append(polygon);
    }

    /**
     * @brief 返回全部路径图元。
     */
    QList<SymbolPath> paths() const {
        return m_paths;
    }

    /**
     * @brief 批量设置路径图元。
     * @param paths 参数。
     */
    void setPaths(const QList<SymbolPath>& paths) {
        m_paths = paths;
    }

    /**
     * @brief 追加一个路径图元。
     * @param path 参数。
     */
    void addPath(const SymbolPath& path) {
        m_paths.append(path);
    }

    /**
     * @brief 返回全部图片图元。
     */
    QList<SymbolImage> images() const {
        return m_images;
    }

    /**
     * @brief 批量设置图片图元。
     * @param images 参数。
     */
    void setImages(const QList<SymbolImage>& images) {
        m_images = images;
    }

    /**
     * @brief 追加一个图片图元。
     * @param image 参数。
     */
    void addImage(const SymbolImage& image) {
        m_images.append(image);
    }

    /**
     * @brief 返回全部文本图元。
     */
    QList<SymbolText> texts() const {
        return m_texts;
    }

    /**
     * @brief 批量设置文本图元。
     * @param texts 参数。
     */
    void setTexts(const QList<SymbolText>& texts) {
        m_texts = texts;
    }

    /**
     * @brief 追加一个文本图元。
     * @param text 参数。
     */
    void addText(const SymbolText& text) {
        m_texts.append(text);
    }

    /**
     * @brief 返回符号图元的绘制顺序。
     */
    QList<SymbolGraphicOrder> graphicOrder() const {
        return m_graphicOrder;
    }

    /**
     * @brief 设置符号图元的绘制顺序。
     * @param order 参数。
     */
    void setGraphicOrder(const QList<SymbolGraphicOrder>& order) {
        m_graphicOrder = order;
    }

    /**
     * @brief 追加一个绘制顺序引用。
     * @param order 参数。
     */
    void addGraphicOrder(const SymbolGraphicOrder& order) {
        m_graphicOrder.append(order);
    }

    // 多部分符号接
    /**
     * @brief 返回多部分符号的全部部分。
     */
    QList<SymbolPart> parts() const {
        return m_parts;
    }

    /**
     * @brief 批量设置多部分符号的部分。
     */
    /**
     * @brief 批量设置多部分符号的部分。
     * @param parts 参数。
     */
    void setParts(const QList<SymbolPart>& parts) {
        m_parts = parts;
    }

    /**
     * @brief 追加一个多部分符号部分。
     */
    /**
     * @brief 追加一个多部分符号部分。
     * @param part 参数。
     */
    void addPart(const SymbolPart& part) {
        m_parts.append(part);
    }

    /**
     * @brief 判断符号是否包含多个部分。
     */
    bool isMultiPart() const {
        return m_parts.size() > 1;
    }

    // JSON 序列
    QJsonObject toJson() const;
    bool fromJson(const QJsonObject& json);

    // 数据验证
    bool isValid() const;
    QString validate() const;

    /**
     * @brief 返回所有可诊断的符号数据问题
     * @details 与只返回首个错误的 validate() 不同，该接口用于导出前一次性展示所有异常图元。
     * @return 验证错误列表；为空表示未发现问题
     */
    QStringList validationErrors() const;

    // 清空数据
    void clear();

private:
    SymbolInfo m_info{};  ///< 符号元数据。
    SymbolBBox m_bbox{};  ///< 符号边界框。
    QList<SymbolPin> m_pins;  ///< 单部分符号引脚列表（向后兼容）。
    QList<SymbolRectangle> m_rectangles;  ///< 矩形图元列表。
    QList<SymbolCircle> m_circles;  ///< 圆形图元列表。
    QList<SymbolArc> m_arcs;  ///< 圆弧图元列表。
    QList<SymbolEllipse> m_ellipses;  ///< 椭圆图元列表。
    QList<SymbolPolyline> m_polylines;  ///< 折线图元列表。
    QList<SymbolPolygon> m_polygons;  ///< 多边形图元列表。
    QList<SymbolPath> m_paths;  ///< 路径图元列表。
    QList<SymbolImage> m_images;  ///< 图片图元列表。
    QList<SymbolText> m_texts;  ///< 文本图元列表。
    QList<SymbolGraphicOrder> m_graphicOrder;  ///< 图元绘制顺序列表。
    QList<SymbolPart> m_parts;  ///< 多部分符号部分列表。
};

}  // namespace EasyKiConverter

#endif  // SYMBOLDATA_H
