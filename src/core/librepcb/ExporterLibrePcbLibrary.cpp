#include "ExporterLibrePcbLibrary.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QSaveFile>
#include <QSet>
#include <QTransform>
#include <QtMath>

#include <cmath>
#include <utility>

namespace EasyKiConverter {
namespace {

constexpr int kLibrePcbFileFormatVersion = 2;  ///< 当前生成的 LibrePCB 文件格式版本。

class SExpr {
public:
    /**
     * @brief 创建 S 表达式节点。
     * @param name 节点名称。
     */
    explicit SExpr(QString name) : m_name(std::move(name)) {}

    /**
     * @brief 追加需要引用的字符串原子。
     * @param value 待追加的字符串值。
     * @return 当前表达式对象。
     */
    SExpr& atom(const QString& value) {
        m_children.append(quoteIfNeeded(value));
        return *this;
    }

    /**
     * @brief 追加无需引用的语法令牌。
     * @param value 待追加的语法令牌。
     * @return 当前表达式对象。
     */
    SExpr& token(const QString& value) {
        m_children.append(value);
        return *this;
    }

    /**
     * @brief 开始一个命名子列表。
     * @param name 子列表名称。
     * @return 当前表达式对象。
     */
    SExpr& list(const QString& name) {
        m_children.append(QStringLiteral("(") + name);
        m_openLists.append(m_children.size() - 1);
        return *this;
    }

    /**
     * @brief 关闭最近打开的子列表。
     * @return 当前表达式对象。
     */
    SExpr& close() {
        if (!m_openLists.isEmpty()) {
            m_children.append(QStringLiteral(")"));
            m_openLists.removeLast();
        }
        return *this;
    }

    /**
     * @brief 补齐未关闭列表并序列化完整表达式。
     * @return LibrePCB S 表达式文本。
     */
    QString finish() {
        while (!m_openLists.isEmpty())
            close();
        QString result = QStringLiteral("(") + m_name;  ///< 已拼接节点名称的序列化结果。
        for (const QString& child : std::as_const(m_children))
            result += QLatin1Char(' ') + child;
        return result + QStringLiteral(")\n");
    }

private:
    /**
     * @brief 转义并引用需要作为字符串原子写入的值。
     * @param value 待转义的字符串。
     * @return 可直接写入 S 表达式的字符串原子。
     */
    static QString quoteIfNeeded(const QString& value) {
        QString escaped = value;  ///< 已转义的字符串副本。
        escaped.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
        escaped.replace(QLatin1Char('"'), QStringLiteral("\\\""));
        escaped.replace(QLatin1Char('\n'), QStringLiteral("\\n"));
        return QStringLiteral("\"") + escaped + QLatin1Char('"');
    }

    QString m_name;  ///< 当前 S 表达式的根名称。
    QStringList m_children;  ///< 已追加的子表达式文本。
    QList<int> m_openLists;  ///< 尚未关闭的子列表位置栈。
};

/**
 * @brief 将数值格式化为稳定的 LibrePCB 小数文本。
 * @param value 待格式化的数值。
 * @return 去除无意义尾零的小数文本。
 */
QString number(double value) {
    if (qFuzzyIsNull(value))
        return QStringLiteral("0.0");
    QString result = QString::number(value, 'f', 6);  ///< 固定精度格式化后的数值文本。
    while (result.endsWith(QLatin1Char('0')))
        result.chop(1);
    if (result.endsWith(QLatin1Char('.')))
        result.append(QLatin1Char('0'));
    return result;
}

/**
 * @brief 将布尔值转换为 LibrePCB 的文本令牌。
 * @param value 待转换的布尔值。
 * @return true 或 false 文本。
 */
QString boolValue(bool value) {
    return value ? QStringLiteral("true") : QStringLiteral("false");
}

/**
 * @brief 清理库元素名称中的非法字符。
 * @param value 原始名称。
 * @return 可用于 LibrePCB 库文件的安全名称。
 */
QString sanitizeName(const QString& value) {
    QString result;  ///< 清理后的名称文本。
    for (const QChar character : value.trimmed()) {
        if (character.isLetterOrNumber() || character == QLatin1Char('_') || character == QLatin1Char('-') ||
            character == QLatin1Char(' ')) {
            result.append(character);
        } else {
            result.append(QLatin1Char('_'));
        }
    }
    result = result.simplified();
    return result.isEmpty() ? QStringLiteral("EasyKiConverter") : result.left(120);
}

/**
 * @brief 根据元素语义生成稳定的 RFC 4122 UUID。
 * @param kind 元素类型。
 * @param name 元素名称。
 * @param index 可选的同名元素索引。
 * @return 稳定的 UUID 文本。
 */
QString uuidFor(const QString& kind, const QString& name, int index = -1) {
    // 将元素类型、名称和可选索引组合为稳定 UUID 的输入串。
    const QByteArray input = (QStringLiteral("EasyKiConverter/LibrePCB/2/") + kind + QLatin1Char('/') + name +
                              (index >= 0 ? QStringLiteral("/") + QString::number(index) : QString()))
                                 .toUtf8();
    // LibrePCB 2.x 只接受 RFC 4122 的随机 UUID；通过哈希生成稳定值后重写版本位。
    QByteArray digest = QCryptographicHash::hash(input, QCryptographicHash::Sha1).left(16);  ///< UUID 哈希字节。
    digest[6] = char((uchar(digest.at(6)) & 0x0f) | 0x40);
    digest[8] = char((uchar(digest.at(8)) & 0x3f) | 0x80);
    const auto hex = [](uchar byte) { return QStringLiteral("%1").arg(byte, 2, 16, QLatin1Char('0')); };
    QString result;  ///< RFC 4122 格式的 UUID 文本。
    for (int i = 0; i < digest.size(); ++i) {
        result += hex(uchar(digest.at(i)));
        if (i == 3 || i == 5 || i == 7 || i == 9)
            result += QLatin1Char('-');
    }
    return result;
}

/**
 * @brief 向 S 表达式追加 name 属性。
 * @param root 待修改的根表达式。
 * @param name 元素名称。
 */
void appendName(SExpr& root, const QString& name) {
    root.list(QStringLiteral("name")).atom(name).close();
}

/**
 * @brief 向 S 表达式追加 description 属性。
 * @param root 待修改的根表达式。
 * @param description 元素描述。
 */
void appendDescription(SExpr& root, const QString& description) {
    root.list(QStringLiteral("description")).atom(description).close();
}

/**
 * @brief 写入 LibrePCB 库元素的通用元数据。
 * @param root 待初始化的根表达式。
 * @param uuid 元素 UUID。
 * @param name 元素名称。
 * @param description 元素描述。
 * @param rootName 根元素名称。
 * @param generatedBy 生成器标识。
 */
void appendCommon(SExpr& root,
                  const QString& uuid,
                  const QString& name,
                  const QString& description,
                  const QString& rootName,
                  const QString& generatedBy = QStringLiteral("EasyKiConverter")) {
    root = SExpr(rootName);
    root.token(uuid);
    appendName(root, name);
    appendDescription(root, description);
    root.list(QStringLiteral("keywords")).atom(QString()).close();
    root.list(QStringLiteral("author")).atom(QStringLiteral("EasyKiConverter")).close();
    root.list(QStringLiteral("version")).atom(QStringLiteral("0.1")).close();
    root.list(QStringLiteral("created")).token(QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)).close();
    root.list(QStringLiteral("deprecated")).token(QStringLiteral("false")).close();
    if (!generatedBy.isEmpty())
        root.list(QStringLiteral("generated_by")).atom(generatedBy).close();
}

/**
 * @brief 将统一 IR 图层映射为 LibrePCB 封装图层。
 * @param layer 待映射的 IR 图层。
 * @return LibrePCB 图层名称；无法表达时返回空字符串。
 */
QString layerForFootprint(IR::LayerType layer) {
    // 根据目标格式的层语义选择封装图层，未知层保持不可表达状态。
    switch (layer) {
        case IR::LayerType::TopSilk:
        case IR::LayerType::TopOverlay:
            return QStringLiteral("top_legend");
        case IR::LayerType::TopAssembly:
            return QStringLiteral("top_documentation");
        case IR::LayerType::UserDefined:
            // EasyEDA 的 Document/UserDefined 图层在 IR 中无法进一步区分。
            // LibrePCB 没有对应的通用用户层，保守降级到 Documentation，避免误写入铜层。
            return QStringLiteral("top_documentation");
        case IR::LayerType::KeepOut:
            // EasyEDA 的 ComponentShapeLayer（99）在 IR 中暂存为 KeepOut，
            // 对封装而言应表达为装配边界，而不是板级铜箔禁布区。
            return QStringLiteral("top_courtyard");
        case IR::LayerType::EdgeCuts:
            return QStringLiteral("top_package_outlines");
        case IR::LayerType::BottomSilk:
        case IR::LayerType::BottomOverlay:
            return QStringLiteral("bot_legend");
        case IR::LayerType::BottomAssembly:
            return QStringLiteral("bot_documentation");
        case IR::LayerType::TopMask:
            return QStringLiteral("top_stop_mask");
        case IR::LayerType::BottomMask:
            return QStringLiteral("bot_stop_mask");
        case IR::LayerType::TopPaste:
            return QStringLiteral("top_solder_paste");
        case IR::LayerType::BottomPaste:
            return QStringLiteral("bot_solder_paste");
        case IR::LayerType::TopCopper:
            return QStringLiteral("top_cu");
        case IR::LayerType::BottomCopper:
            return QStringLiteral("bot_cu");
        default:
            return {};
    }
}

/**
 * @brief 判断图层是否为装配层。
 * @param layer 待判断的 IR 图层。
 * @return 属于装配层时返回 true。
 */
bool isAssemblyLayer(IR::LayerType layer) {
    return layer == IR::LayerType::TopAssembly || layer == IR::LayerType::BottomAssembly;
}

/**
 * @brief 判断图层是否为用户自定义层。
 * @param layer 待判断的 IR 图层。
 * @return 属于用户自定义层时返回 true。
 */
bool isUserDefinedLayer(IR::LayerType layer) {
    return layer == IR::LayerType::UserDefined;
}

/**
 * @brief 检查符号引脚是否落在 LibrePCB 固定的 2.54 mm 原理图栅格上。
 * @param value 坐标值，单位为毫米。
 * @return 坐标可被 LibrePCB 原理图编辑器使用时返回 true。
 */
bool isOnLibrePcbGrid(double value) {
    constexpr double kGridMm = 2.54;  ///< LibrePCB 原理图标准栅格间距（毫米）。
    return qAbs(value - qRound(value / kGridMm) * kGridMm) < 1e-6;
}

/**
 * @brief 将接近 LibrePCB 栅格的坐标对齐到最近栅格点。
 * @param value 原始坐标，单位为毫米。
 * @param snapped 输出的对齐坐标，单位为毫米。
 * @return 坐标在允许的微小误差范围内时返回 true。
 */
bool snapToLibrePcbGrid(double value, double& snapped) {
    constexpr double kGridMm = 2.54;  ///< LibrePCB 原理图标准栅格间距（毫米）。
    constexpr double kMaxSnapDistanceMm = 1.27 + 1e-6;  ///< 允许的最大栅格对齐距离（毫米）。
    snapped = qRound(value / kGridMm) * kGridMm;
    return qAbs(value - snapped) <= kMaxSnapDistanceMm;
}

/**
 * @brief 将符号引脚连接端对齐到 LibrePCB 栅格并补偿引脚长度。
 * @param pin 原始统一 IR 引脚。
 * @param position 输出的栅格化连接端位置。
 * @param length 输出的补偿后引脚长度，单位为毫米。
 * @return 引脚偏差不超过半个栅格时返回 true。
 */
bool alignSymbolPinToGrid(const IR::SymbolPinIR& pin, QPointF& position, double& length) {
    constexpr double kGridMm = 2.54;  ///< LibrePCB 原理图标准栅格间距（毫米）。
    constexpr double kMaxExtensionMm = kGridMm / 2.0;  ///< 允许补偿的最大引脚延伸量（毫米）。
    position = pin.position;
    length = pin.length;

    double snappedX = position.x();  ///< X 方向的临时栅格坐标。
    double snappedY = position.y();  ///< Y 方向的临时栅格坐标。
    if (!snapToLibrePcbGrid(position.x(), snappedX) || !snapToLibrePcbGrid(position.y(), snappedY))
        return false;

    // 按引脚方向选择连接端所在的目标栅格边界。
    switch (pin.direction) {
        case IR::PinDirection::Right:
            snappedX = qCeil(position.x() / kGridMm - 1e-9) * kGridMm;
            if (snappedX - position.x() > kMaxExtensionMm + 1e-6)
                return false;
            length += snappedX - position.x();
            break;
        case IR::PinDirection::Left:
            snappedX = qFloor(position.x() / kGridMm + 1e-9) * kGridMm;
            if (position.x() - snappedX > kMaxExtensionMm + 1e-6)
                return false;
            length += position.x() - snappedX;
            break;
        case IR::PinDirection::Up:
            snappedY = qCeil(position.y() / kGridMm - 1e-9) * kGridMm;
            if (snappedY - position.y() > kMaxExtensionMm + 1e-6)
                return false;
            length += snappedY - position.y();
            break;
        case IR::PinDirection::Down:
            snappedY = qFloor(position.y() / kGridMm + 1e-9) * kGridMm;
            if (position.y() - snappedY > kMaxExtensionMm + 1e-6)
                return false;
            length += position.y() - snappedY;
            break;
    }
    position = QPointF(snappedX, snappedY);
    return true;
}

/**
 * @brief 将统一 IR 引脚方向转换为 LibrePCB 的引脚旋转角度。
 * @param direction 统一 IR 引脚方向。
 * @return LibrePCB 引脚旋转角度，单位为度。
 * @details LibrePCB 的引脚位置是连接端，线段从该位置沿旋转方向指向符号图形。
 */
double librePcbPinRotation(IR::PinDirection direction) {
    // LibrePCB 的旋转角度与 IR 方向采用相反的连接端朝向约定。
    switch (direction) {
        case IR::PinDirection::Right:
            return 180.0;
        case IR::PinDirection::Left:
            return 0.0;
        case IR::PinDirection::Up:
            return 270.0;
        case IR::PinDirection::Down:
            return 90.0;
    }
    return 0.0;
}

/**
 * @brief 将符号引脚名称的全局坐标转换为 LibrePCB 的引脚局部坐标。
 * @param pin 统一 IR 引脚。
 * @param pinLength 对齐后引脚长度。
 * @param rotation LibrePCB 引脚旋转角度。
 * @return LibrePCB 引脚名称相对于连接端的局部位置。
 */
QPointF symbolPinNamePosition(const IR::SymbolPinIR& pin, double pinLength, double rotation) {
    if (!pin.hasNamePosition)
        return QPointF(pinLength + 1.27, 0.0);

    const QPointF worldOffset = pin.namePosition - pin.position;  ///< 编号文本相对于连接端的全局偏移。
    return QTransform().rotate(-rotation).map(worldOffset);
}

/**
 * @brief 计算符号图形和引脚的整体边界。
 * @param symbol 统一 IR 符号。
 * @return 包含已知图形及引脚位置的边界框。
 */
QRectF symbolBounds(const IR::SymbolComponentIR& symbol) {
    double minX = 0.0;  ///< 当前边界的最小 X 坐标。
    double minY = 0.0;  ///< 当前边界的最小 Y 坐标。
    double maxX = 0.0;  ///< 当前边界的最大 X 坐标。
    double maxY = 0.0;  ///< 当前边界的最大 Y 坐标。
    bool initialized = false;  ///< 是否已纳入首个有效点。
    const auto include = [&minX, &minY, &maxX, &maxY, &initialized](const QPointF& point) {
        if (!initialized) {
            minX = maxX = point.x();
            minY = maxY = point.y();
            initialized = true;
        } else {
            minX = qMin(minX, point.x());
            minY = qMin(minY, point.y());
            maxX = qMax(maxX, point.x());
            maxY = qMax(maxY, point.y());
        }
    };
    const auto includePoints = [&include](const QList<QPointF>& points) {
        for (const QPointF& point : points)
            include(point);
    };

    for (const auto& rectangle : symbol.rectangles) {
        include(QPointF(rectangle.x0, rectangle.y0));
        include(QPointF(rectangle.x1, rectangle.y1));
    }
    for (const auto& polygon : symbol.polygons)
        includePoints(polygon.points);
    for (const auto& polyline : symbol.polylines)
        includePoints(polyline.points);
    for (const auto& pin : symbol.pins)
        include(pin.position);
    for (const auto& circle : symbol.circles) {
        include(circle.center + QPointF(-circle.radius, -circle.radius));
        include(circle.center + QPointF(circle.radius, circle.radius));
    }
    for (const auto& ellipse : symbol.ellipses) {
        include(ellipse.center + QPointF(-ellipse.radiusX, -ellipse.radiusY));
        include(ellipse.center + QPointF(ellipse.radiusX, ellipse.radiusY));
    }

    return initialized ? QRectF(QPointF(minX, minY), QPointF(maxX, maxY)) : QRectF(-5.0, -5.0, 10.0, 10.0);
}

/**
 * @brief 将统一 IR 图层映射为 LibrePCB 符号图层。
 * @param layer 待映射的 IR 图层。
 * @return LibrePCB 符号图层名称；无法表达时返回空字符串。
 */
QString symbolLayer(IR::LayerType layer) {
    // 仅保留当前格式能够无歧义表达的符号层。
    switch (layer) {
        case IR::LayerType::TopSilk:
        case IR::LayerType::TopOverlay:
            return QStringLiteral("sym_outlines");
        case IR::LayerType::TopAssembly:
            return QStringLiteral("sym_names");
        default:
            return {};
    }
}

/**
 * @brief 返回默认组件分类的稳定 UUID。
 * @return 组件分类 UUID。
 */
QString componentCategoryUuid() {
    return uuidFor(QStringLiteral("component-category"), QStringLiteral("EasyKiConverter"));
}

/**
 * @brief 返回默认封装分类的稳定 UUID。
 * @return 封装分类 UUID。
 */
QString packageCategoryUuid() {
    return uuidFor(QStringLiteral("package-category"), QStringLiteral("EasyKiConverter"));
}

/**
 * @brief 将 IR 焊盘形状映射为 LibrePCB 焊盘形状。
 * @param shape IR 焊盘形状。
 * @param size 焊盘尺寸。
 * @param customPoints 自定义焊盘顶点。
 * @param diagnostics 用于追加失败原因的诊断列表。
 * @param context 当前焊盘的诊断上下文。
 * @return LibrePCB 焊盘形状名称；无法保真时返回空字符串。
 */
QString padShape(IR::PadShape shape,
                 const QSizeF& size,
                 const QList<QPointF>& customPoints,
                 QStringList& diagnostics,
                 const QString& context) {
    // 按目标格式能力逐项处理焊盘形状，无法保真时返回空值。
    switch (shape) {
        case IR::PadShape::Ellipse:
            if (qFuzzyCompare(size.width(), size.height()))
                return QStringLiteral("roundrect");
            diagnostics.append(QStringLiteral("LibrePCB: 非圆椭圆焊盘无法准确表达：%1").arg(context));
            return {};
        case IR::PadShape::Rect:
            return QStringLiteral("roundrect");
        case IR::PadShape::RoundRect:
            diagnostics.append(
                QStringLiteral("LibrePCB: 当前 IR 未保存圆角半径，拒绝将圆角矩形静默降级为矩形：%1").arg(context));
            return {};
        case IR::PadShape::Oval:
            diagnostics.append(
                QStringLiteral("LibrePCB: 椭圆长圆焊盘无法由 2.1.1 原生焊盘形状准确表达：%1").arg(context));
            return {};
        case IR::PadShape::Polygon:
            if (customPoints.size() >= 3)
                return QStringLiteral("custom");
            diagnostics.append(QStringLiteral("LibrePCB: 多边形焊盘缺少有效顶点：%1").arg(context));
            return {};
        case IR::PadShape::Trapezoid:
            if (customPoints.size() >= 3)
                return QStringLiteral("custom");
            diagnostics.append(QStringLiteral("LibrePCB: 梯形焊盘缺少有效顶点，无法保留形状：%1").arg(context));
            return {};
    }
    diagnostics.append(QStringLiteral("LibrePCB: 未知焊盘形状，拒绝导出：%1").arg(context));
    return {};
}

/**
 * @brief 向表达式追加一个二维位置。
 * @param root 待修改的表达式。
 * @param point 二维坐标。
 */
void appendPoint(SExpr& root, const QPointF& point) {
    root.list(QStringLiteral("position")).token(number(point.x())).token(number(point.y())).close();
}

/**
 * @brief 向表达式追加由直线顶点组成的路径。
 * @param root 待修改的表达式。
 * @param points 路径顶点。
 */
void appendPath(SExpr& root, const QList<QPointF>& points) {
    for (int i = 0; i < points.size(); ++i) {
        SExpr& vertex = root.list(QStringLiteral("vertex"));  ///< 当前路径顶点表达式。
        vertex.list(QStringLiteral("position")).token(number(points.at(i).x())).token(number(points.at(i).y())).close();
        vertex.list(QStringLiteral("angle")).token(QStringLiteral("0")).close();
        vertex.close();
    }
}

/**
 * @brief 生成旋转后的矩形闭合顶点序列。
 * @param bounds 未旋转的矩形边界。
 * @param rotation 绕矩形中心旋转的角度，单位为度。
 * @return 旋转后的闭合顶点序列。
 */
QList<QPointF> rectanglePoints(const QRectF& bounds, double rotation) {
    const QPointF center = bounds.center();  ///< 矩形中心点。
    const double radians = qDegreesToRadians(rotation);  ///< 弧度制旋转角。
    const double c = std::cos(radians);  ///< 旋转矩阵的余弦项。
    const double s = std::sin(radians);  ///< 旋转矩阵的正弦项。
    QList<QPointF> result;  ///< 旋转后的顶点序列。
    for (const QPointF point : {bounds.topLeft(), bounds.topRight(), bounds.bottomRight(), bounds.bottomLeft()}) {
        const QPointF offset = point - center;  ///< 当前顶点相对矩形中心的偏移。
        result.append(center + QPointF(offset.x() * c - offset.y() * s, offset.x() * s + offset.y() * c));
    }
    result.append(result.first());
    return result;
}

/**
 * @brief 将圆弧离散为 LibrePCB 可表达的闭合/开放顶点序列。
 * @param center 圆心。
 * @param radius 半径。
 * @param startAngle 起始角度，单位为度。
 * @param endAngle 结束角度，单位为度。
 * @return 按源角度方向排列的近似顶点。
 */
QList<QPointF> sampleArc(const QPointF& center, double radius, double startAngle, double endAngle) {
    constexpr int kSegments = 32;  ///< 圆弧离散的基准分段数。
    double sweep = endAngle - startAngle;  ///< 圆弧扫过角度。
    if (qFuzzyIsNull(sweep))
        sweep = 360.0;
    const int segments = qMax(2, qCeil(qAbs(sweep) / 360.0 * kSegments));  ///< 实际分段数。
    QList<QPointF> points;  ///< 离散后的圆弧顶点。
    for (int i = 0; i <= segments; ++i) {
        const double angle = qDegreesToRadians(startAngle + sweep * i / segments);  ///< 当前采样角度。
        points.append(center + QPointF(radius * std::cos(angle), radius * std::sin(angle)));
    }
    return points;
}

/**
 * @brief 将椭圆离散为 LibrePCB 多边形。
 * @param center 椭圆中心。
 * @param radiusX X 方向半径。
 * @param radiusY Y 方向半径。
 * @param startAngle 起始角度，单位为度。
 * @param endAngle 结束角度，单位为度。
 * @return 近似顶点。
 */
QList<QPointF> sampleEllipse(const QPointF& center,
                             double radiusX,
                             double radiusY,
                             double startAngle,
                             double endAngle) {
    constexpr int kSegments = 48;  ///< 椭圆离散的基准分段数。
    double sweep = endAngle - startAngle;  ///< 椭圆扫过角度。
    if (qFuzzyIsNull(sweep))
        sweep = 360.0;
    const int segments = qMax(2, qCeil(qAbs(sweep) / 360.0 * kSegments));  ///< 实际分段数。
    QList<QPointF> points;  ///< 离散后的椭圆顶点。
    for (int i = 0; i <= segments; ++i) {
        const double angle = qDegreesToRadians(startAngle + sweep * i / segments);  ///< 当前采样角度。
        points.append(center + QPointF(radiusX * std::cos(angle), radiusY * std::sin(angle)));
    }
    return points;
}

/**
 * @brief 将三次 Bézier 曲线离散为 LibrePCB 可表达的折线。
 * @param controlPoints 四个控制点。
 * @return 近似曲线的顶点序列；控制点数量不为四个时原样返回。
 */
QList<QPointF> sampleBezier(const QList<QPointF>& controlPoints) {
    if (controlPoints.size() != 4)
        return controlPoints;
    QList<QPointF> points;  ///< 离散后的 Bézier 顶点。
    for (int i = 0; i <= 24; ++i) {
        const double t = static_cast<double>(i) / 24.0;  ///< 当前曲线参数。
        const double u = 1.0 - t;  ///< 曲线参数的补值。
        points.append(controlPoints.at(0) * (u * u * u) + controlPoints.at(1) * (3 * u * u * t) +
                      controlPoints.at(2) * (3 * u * t * t) + controlPoints.at(3) * (t * t * t));
    }
    return points;
}

/**
 * @brief 计算封装图元的保守边界，用于生成缺失的外形和 courtyard 回退图形。
 * @param footprint 统一封装 IR。
 * @param valid 输出边界是否至少包含一个有效图元。
 * @return 所有已知焊盘和二维图元的包围盒。
 */
QRectF footprintBounds(const IR::FootprintComponentIR& footprint, bool& valid) {
    QRectF bounds;  ///< 当前封装图元的边界框。
    valid = false;  // 标记是否已纳入有效图元。
    const auto include = [&bounds, &valid](const QPointF& point) {
        if (!valid) {
            bounds = QRectF(point, QSizeF());
            valid = true;
        } else {
            const double left = qMin(bounds.left(), point.x());  ///< 更新后的左边界。
            const double top = qMin(bounds.top(), point.y());  ///< 更新后的上边界。
            const double right = qMax(bounds.right(), point.x());  ///< 更新后的右边界。
            const double bottom = qMax(bounds.bottom(), point.y());  ///< 更新后的下边界。
            bounds = QRectF(QPointF(left, top), QPointF(right, bottom));
        }
    };
    for (const auto& pad : footprint.pads) {
        include(pad.position + QPointF(-pad.size.width() / 2.0, -pad.size.height() / 2.0));
        include(pad.position + QPointF(pad.size.width() / 2.0, pad.size.height() / 2.0));
    }
    for (const auto& rectangle : footprint.rectangles) {
        for (const QPointF& point : rectanglePoints(rectangle.bounds, rectangle.rotation))
            include(point);
    }
    for (const auto& circle : footprint.circles) {
        include(circle.center - QPointF(circle.radius, circle.radius));
        include(circle.center + QPointF(circle.radius, circle.radius));
    }
    for (const auto& outline : footprint.outlines) {
        for (const QPointF& point : outline.points)
            include(point);
    }
    for (const auto& track : footprint.tracks) {
        for (const QPointF& point : track.points)
            include(point);
    }
    for (const auto& arc : footprint.arcs) {
        for (const QPointF& point : sampleArc(arc.center, arc.radius, arc.startAngle, arc.endAngle))
            include(point);
    }
    for (const auto& region : footprint.regions) {
        for (const QPointF& point : region.vertices)
            include(point);
    }
    for (const auto& hole : footprint.holes) {
        include(hole.center - QPointF(hole.radius, hole.radius));
        include(hole.center + QPointF(hole.radius, hole.radius));
    }
    return bounds;
}

/**
 * @brief 将封装图元坐标平移到 LibrePCB 封装原点。
 * @param points 原始坐标序列。
 * @param origin 封装原点在输入 IR 中的坐标。
 * @return 以封装原点为基准的坐标序列。
 */
QList<QPointF> localizeFootprintPoints(const QList<QPointF>& points, const QPointF& origin) {
    QList<QPointF> localized;  ///< 以封装原点为基准的坐标序列。
    localized.reserve(points.size());
    for (const QPointF& point : points)
        localized.append(point - origin);
    return localized;
}

/**
 * @brief 向封装表达式追加笔画文本图元。
 * @param root 待修改的封装表达式。
 * @param uuid 图元 UUID。
 * @param layer LibrePCB 图层名称。
 * @param value 文本内容。
 * @param position 文本位置。
 * @param rotation 文本旋转角度，单位为度。
 * @param height 文本高度，单位为毫米。
 * @param strokeWidth 笔画宽度，单位为毫米。
 * @param mirror 是否镜像文本。
 */
void appendStrokeText(SExpr& root,
                      const QString& uuid,
                      const QString& layer,
                      const QString& value,
                      const QPointF& position,
                      double rotation = 0.0,
                      double height = 1.0,
                      double strokeWidth = 0.15,
                      bool mirror = false) {
    SExpr& node = root.list(QStringLiteral("stroke_text"));  ///< 当前笔画文本节点。
    node.token(uuid);
    node.list(QStringLiteral("layer")).token(layer).close();
    node.list(QStringLiteral("height")).token(number(height > 0 ? height : 1.0)).close();
    node.list(QStringLiteral("stroke_width")).token(number(strokeWidth > 0 ? strokeWidth : 0.15)).close();
    node.list(QStringLiteral("letter_spacing")).token(QStringLiteral("auto")).close();
    node.list(QStringLiteral("line_spacing")).token(QStringLiteral("auto")).close();
    node.list(QStringLiteral("align")).token(QStringLiteral("center")).token(QStringLiteral("center")).close();
    node.list(QStringLiteral("position")).token(number(position.x())).token(number(position.y())).close();
    node.list(QStringLiteral("rotation")).token(number(rotation)).close();
    node.list(QStringLiteral("auto_rotate")).token(QStringLiteral("true")).close();
    node.list(QStringLiteral("mirror")).token(boolValue(mirror)).close();
    node.list(QStringLiteral("lock")).token(QStringLiteral("false")).close();
    node.list(QStringLiteral("value")).atom(value).close();
    node.close();
}

/**
 * @brief 向封装表达式追加多边形图元。
 * @param root 待修改的封装表达式。
 * @param uuid 图元 UUID。
 * @param layer LibrePCB 图层名称。
 * @param points 多边形顶点。
 * @param width 轮廓宽度，单位为毫米。
 * @param filled 是否填充多边形。
 */
void appendPolygon(SExpr& root,
                   const QString& uuid,
                   const QString& layer,
                   const QList<QPointF>& points,
                   double width,
                   bool filled) {
    SExpr& polygon = root.list(QStringLiteral("polygon"));  ///< 当前多边形节点。
    polygon.token(uuid);
    polygon.list(QStringLiteral("layer")).token(layer).close();
    polygon.list(QStringLiteral("width")).token(number(width)).close();
    polygon.list(QStringLiteral("fill")).token(boolValue(filled)).close();
    polygon.list(QStringLiteral("grab_area")).token(QStringLiteral("false")).close();
    appendPath(polygon, points);
    polygon.close();
}

/**
 * @brief 以原子方式写入 LibrePCB 文件。
 * @param path 输出文件路径。
 * @param data 文件内容。
 * @param diagnostics 用于追加失败原因的诊断列表。
 * @return 写入成功返回 true，否则返回 false。
 */
bool writeFile(const QString& path, const QByteArray& data, QStringList& diagnostics) {
    QSaveFile file(path);  ///< 目标文件的原子写入句柄。
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit()) {
        diagnostics.append(QStringLiteral("LibrePCB: 写入文件失败：%1").arg(path));
        return false;
    }
    return true;
}

/**
 * @brief 创建并写入一个 LibrePCB 库元素目录。
 * @param rootPath 库根目录。
 * @param shortName 元素目录短名称。
 * @param longName 元素文件长名称。
 * @param uuid 元素 UUID。
 * @param data 元素 S 表达式文本。
 * @param diagnostics 用于追加失败原因的诊断列表。
 * @return 写入成功返回 true，否则返回 false。
 */
bool writeElement(const QString& rootPath,
                  const QString& shortName,
                  const QString& longName,
                  const QString& uuid,
                  const QString& data,
                  QStringList& diagnostics) {
    const QString directory = QDir(rootPath).filePath(shortName + QLatin1Char('/') + uuid);  ///< 元素目录路径。
    if (!QDir().mkpath(directory)) {
        diagnostics.append(QStringLiteral("LibrePCB: 创建元素目录失败：%1").arg(directory));
        return false;
    }
    return writeFile(QDir(directory).filePath(QStringLiteral(".librepcb-") + shortName),
                     QByteArray::number(kLibrePcbFileFormatVersion) + '\n',
                     diagnostics) &&
           writeFile(QDir(directory).filePath(longName + QStringLiteral(".lp")), data.toUtf8(), diagnostics);
}

/**
 * @brief 创建并写入一个 LibrePCB 分类元素。
 * @param rootPath 库根目录。
 * @param shortName 分类目录短名称。
 * @param longName 分类文件长名称。
 * @param rootName 分类根元素名称。
 * @param uuid 分类 UUID。
 * @param name 分类显示名称。
 * @param diagnostics 用于追加失败原因的诊断列表。
 * @return 写入成功返回 true，否则返回 false。
 */
bool writeCategory(const QString& rootPath,
                   const QString& shortName,
                   const QString& longName,
                   const QString& rootName,
                   const QString& uuid,
                   const QString& name,
                   QStringList& diagnostics) {
    SExpr category(rootName);  ///< 分类根表达式。
    category.token(uuid);
    category.list(QStringLiteral("name")).atom(name).close();
    category.list(QStringLiteral("description")).atom(QStringLiteral("Generated by EasyKiConverter")).close();
    category.list(QStringLiteral("keywords")).atom(QString()).close();
    category.list(QStringLiteral("author")).atom(QStringLiteral("EasyKiConverter")).close();
    category.list(QStringLiteral("version")).atom(QStringLiteral("0.1")).close();
    category.list(QStringLiteral("created")).token(QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)).close();
    category.list(QStringLiteral("deprecated")).token(QStringLiteral("false")).close();
    category.list(QStringLiteral("parent")).token(QStringLiteral("none")).close();
    const QString directory = QDir(rootPath).filePath(shortName + QLatin1Char('/') + uuid);  ///< 分类目录路径。
    if (!QDir().mkpath(directory)) {
        diagnostics.append(QStringLiteral("LibrePCB: 创建分类目录失败：%1").arg(directory));
        return false;
    }
    return writeFile(QDir(directory).filePath(QStringLiteral(".librepcb-") + shortName),
                     QByteArray::number(kLibrePcbFileFormatVersion) + '\n',
                     diagnostics) &&
           writeFile(
               QDir(directory).filePath(longName + QStringLiteral(".lp")), category.finish().toUtf8(), diagnostics);
}

/**
 * @brief 写入组件和封装分类。
 * @param rootPath 库根目录。
 * @param diagnostics 用于追加失败原因的诊断列表。
 * @return 分类写入成功返回 true，否则返回 false。
 */
bool writeLibraryCategories(const QString& rootPath, QStringList& diagnostics) {
    return writeCategory(rootPath,
                         QStringLiteral("cmpcat"),
                         QStringLiteral("component_category"),
                         QStringLiteral("librepcb_component_category"),
                         componentCategoryUuid(),
                         QStringLiteral("EasyKiConverter"),
                         diagnostics) &&
           writeCategory(rootPath,
                         QStringLiteral("pkgcat"),
                         QStringLiteral("package_category"),
                         QStringLiteral("librepcb_package_category"),
                         packageCategoryUuid(),
                         QStringLiteral("EasyKiConverter"),
                         diagnostics);
}

/**
 * @brief 写入 LibrePCB 库根描述文件。
 * @param rootPath 库根目录。
 * @param libName 库名称。
 * @param description 库描述。
 * @param keywords 库关键词。
 * @param diagnostics 用于追加失败原因的诊断列表。
 * @return 写入成功返回 true，否则返回 false。
 */
bool writeLibraryRoot(const QString& rootPath,
                      const QString& libName,
                      const QString& description,
                      const QString& keywords,
                      QStringList& diagnostics) {
    if (!QDir().mkpath(rootPath))
        return false;
    SExpr root(QStringLiteral("librepcb_library"));
    root.token(uuidFor(QStringLiteral("library"), libName));
    appendName(root, sanitizeName(libName));
    appendDescription(root, description);
    root.list(QStringLiteral("keywords")).atom(keywords).close();
    root.list(QStringLiteral("author")).atom(QStringLiteral("EasyKiConverter")).close();
    root.list(QStringLiteral("version")).atom(QStringLiteral("0.1")).close();
    root.list(QStringLiteral("created")).token(QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)).close();
    root.list(QStringLiteral("deprecated")).token(QStringLiteral("false")).close();
    root.list(QStringLiteral("url")).atom(QString()).close();
    root.list(QStringLiteral("manufacturer")).atom(QString()).close();
    return writeFile(QDir(rootPath).filePath(QStringLiteral(".librepcb-lib")),
                     QByteArray::number(kLibrePcbFileFormatVersion) + '\n',
                     diagnostics) &&
           writeFile(QDir(rootPath).filePath(QStringLiteral("library.lp")), root.finish().toUtf8(), diagnostics);
}

/**
 * @brief 将一个符号 IR 写入 LibrePCB 原生符号文件。
 * @param symbol 待导出的符号 IR。
 * @param rootPath 库根目录。
 * @param diagnostics 用于追加失败原因的诊断列表。
 * @return 写入成功返回 true，否则返回 false。
 */
bool writeSymbol(const IR::SymbolComponentIR& symbol, const QString& rootPath, QStringList& diagnostics) {
    if (symbol.name.trimmed().isEmpty()) {
        diagnostics.append(QStringLiteral("LibrePCB: 符号名称为空"));
        return false;
    }
    SExpr root(QStringLiteral("librepcb_symbol"));
    appendCommon(root,
                 uuidFor(QStringLiteral("symbol"), symbol.name),
                 sanitizeName(symbol.name),
                 symbol.description,
                 QStringLiteral("librepcb_symbol"));
    root.list(QStringLiteral("category")).token(componentCategoryUuid()).close();
    root.list(QStringLiteral("grid_interval")).token(QStringLiteral("0.01")).close();
    if (symbol.partCount != 1) {
        diagnostics.append(QStringLiteral("LibrePCB: 多部件符号当前未实现安全拆分：%1").arg(symbol.name));
        return false;
    }
    if (!symbol.aliases.isEmpty())
        diagnostics.append(QStringLiteral("LibrePCB: 符号 %1 的别名未写入 LibrePCB 原生库").arg(symbol.name));
    if (!symbol.graphicOrder.isEmpty())
        diagnostics.append(QStringLiteral("LibrePCB: 符号 %1 的原始图元顺序未保留").arg(symbol.name));
    if (!symbol.models.isEmpty()) {
        diagnostics.append(QStringLiteral("LibrePCB: 符号 %1 包含暂不支持的模型关联").arg(symbol.name));
        return false;
    }
    QSet<QString> designators;  ///< 已写入符号的引脚编号集合。
    for (int i = 0; i < symbol.pins.size(); ++i) {
        const auto& pin = symbol.pins.at(i);  ///< 当前待处理的符号引脚。
        const QString designator = pin.designator.trimmed();  ///< 清理后的引脚编号。
        if (designator.isEmpty()) {
            diagnostics.append(QStringLiteral("LibrePCB: 符号 %1 存在空引脚编号").arg(symbol.name));
            return false;
        }
        if (designators.contains(designator)) {
            diagnostics.append(QStringLiteral("LibrePCB: 符号 %1 存在重复引脚编号：%2").arg(symbol.name, designator));
            return false;
        }
        QPointF pinPosition;  ///< 对齐后的引脚连接端位置。
        double pinLength = 0.0;  ///< 对齐后的引脚长度。
        if (!alignSymbolPinToGrid(pin, pinPosition, pinLength)) {
            diagnostics.append(QStringLiteral("LibrePCB: 符号 %1 的引脚 %2 不在 2.54 mm 栅格上（坐标 %3, %4）")
                                   .arg(symbol.name, designator)
                                   .arg(number(pin.position.x()), number(pin.position.y())));
            return false;
        }
        if (!isOnLibrePcbGrid(pin.position.x()) || !isOnLibrePcbGrid(pin.position.y())) {
            diagnostics.append(
                QStringLiteral("LibrePCB: 符号 %1 的引脚 %2 已自动对齐到 2.54 mm 栅格").arg(symbol.name, designator));
        }
        designators.insert(designator);
        SExpr& node = root.list(QStringLiteral("pin"));  ///< 当前符号引脚节点。
        node.token(uuidFor(QStringLiteral("symbol-pin"), symbol.name, i));
        node.list(QStringLiteral("name")).atom(pin.name).close();
        node.list(QStringLiteral("position")).token(number(pinPosition.x())).token(number(pinPosition.y())).close();
        const double rotation = librePcbPinRotation(pin.direction);  ///< LibrePCB 引脚旋转角度。
        node.list(QStringLiteral("rotation")).token(number(rotation)).close();
        node.list(QStringLiteral("length")).token(number(pinLength)).close();
        node.list(QStringLiteral("name_position"))
            .token(number(symbolPinNamePosition(pin, pinLength, rotation).x()))
            .token(number(symbolPinNamePosition(pin, pinLength, rotation).y()))
            .close();
        node.list(QStringLiteral("name_rotation")).token(QStringLiteral("0")).close();
        node.list(QStringLiteral("name_height"))
            .token(number(pin.nameFontSizeMm > 0 ? pin.nameFontSizeMm : 1.5))
            .close();
        node.list(QStringLiteral("name_align")).token(QStringLiteral("left")).token(QStringLiteral("center")).close();
        node.close();
    }
    int geometryIndex = 0;  ///< 符号图元 UUID 的稳定索引。
    for (const auto& rectangle : symbol.rectangles) {
        appendPolygon(
            root,
            uuidFor(QStringLiteral("symbol-rectangle"), symbol.name, geometryIndex++),
            QStringLiteral("sym_outlines"),
            rectanglePoints(QRectF(QPointF(rectangle.x0, rectangle.y0), QPointF(rectangle.x1, rectangle.y1)), 0),
            rectangle.strokeWidth,
            rectangle.isFilled);
    }
    for (const auto& polygon : symbol.polygons) {
        appendPolygon(root,
                      uuidFor(QStringLiteral("symbol-polygon"), symbol.name, geometryIndex++),
                      QStringLiteral("sym_outlines"),
                      polygon.points,
                      polygon.strokeWidth,
                      polygon.isFilled);
    }
    for (const auto& polyline : symbol.polylines) {
        appendPolygon(root,
                      uuidFor(QStringLiteral("symbol-polyline"), symbol.name, geometryIndex++),
                      QStringLiteral("sym_outlines"),
                      polyline.points,
                      polyline.strokeWidth,
                      false);
    }
    for (const auto& arc : symbol.arcs) {
        appendPolygon(root,
                      uuidFor(QStringLiteral("symbol-arc"), symbol.name, geometryIndex++),
                      QStringLiteral("sym_outlines"),
                      {arc.startPoint, arc.midPoint, arc.endPoint},
                      arc.strokeWidth,
                      arc.isFilled);
        diagnostics.append(QStringLiteral("LibrePCB: 符号 %1 的三点圆弧已离散为折线").arg(symbol.name));
    }
    for (const auto& ellipse : symbol.ellipses) {
        appendPolygon(root,
                      uuidFor(QStringLiteral("symbol-ellipse"), symbol.name, geometryIndex++),
                      QStringLiteral("sym_outlines"),
                      sampleEllipse(ellipse.center, ellipse.radiusX, ellipse.radiusY, 0, 360),
                      ellipse.strokeWidth,
                      ellipse.isFilled);
        diagnostics.append(QStringLiteral("LibrePCB: 符号 %1 的椭圆已离散为多边形").arg(symbol.name));
    }
    for (const auto& pie : symbol.pies) {
        QList<QPointF> points = {pie.center};  ///< 扇形中心和边界顶点。
        points.append(sampleArc(pie.center, pie.radius, pie.startAngle, pie.endAngle));
        appendPolygon(root,
                      uuidFor(QStringLiteral("symbol-pie"), symbol.name, geometryIndex++),
                      QStringLiteral("sym_outlines"),
                      points,
                      pie.strokeWidth,
                      pie.isFilled);
        diagnostics.append(QStringLiteral("LibrePCB: 符号 %1 的扇形已离散为多边形").arg(symbol.name));
    }
    for (const auto& arc : symbol.ellipticalArcs) {
        appendPolygon(root,
                      uuidFor(QStringLiteral("symbol-elliptical-arc"), symbol.name, geometryIndex++),
                      QStringLiteral("sym_outlines"),
                      sampleEllipse(arc.center, arc.radiusX, arc.radiusY, arc.startAngle, arc.endAngle),
                      arc.strokeWidth,
                      arc.isFilled);
        diagnostics.append(QStringLiteral("LibrePCB: 符号 %1 的椭圆弧已离散为多边形").arg(symbol.name));
    }
    for (const auto& path : symbol.paths) {
        QList<QPointF> points = path.points;  ///< 路径展开后的顶点。
        if (points.isEmpty() && !path.segments.isEmpty()) {
            for (const auto& segment : path.segments) {
                if (segment.type == IR::SymbolPathSegmentIR::Type::CubicBezier)
                    points.append(sampleBezier({segment.start, segment.control1, segment.control2, segment.end}));
                else
                    points.append({segment.start, segment.end});
            }
        }
        if (points.size() < 2) {
            diagnostics.append(QStringLiteral("LibrePCB: 符号 %1 的路径没有可表达的顶点").arg(symbol.name));
            return false;
        }
        appendPolygon(root,
                      uuidFor(QStringLiteral("symbol-path"), symbol.name, geometryIndex++),
                      QStringLiteral("sym_outlines"),
                      points,
                      path.strokeWidth,
                      path.isFilled);
        diagnostics.append(QStringLiteral("LibrePCB: 符号 %1 的路径曲线已离散为折线").arg(symbol.name));
    }
    for (const auto& bezier : symbol.beziers) {
        const QList<QPointF> points = sampleBezier(bezier.controlPoints);  ///< 离散后的 Bézier 顶点。
        if (points.size() < 2) {
            diagnostics.append(QStringLiteral("LibrePCB: 符号 %1 的 Bézier 曲线控制点不足").arg(symbol.name));
            return false;
        }
        appendPolygon(root,
                      uuidFor(QStringLiteral("symbol-bezier"), symbol.name, geometryIndex++),
                      QStringLiteral("sym_outlines"),
                      points,
                      bezier.strokeWidth,
                      false);
        diagnostics.append(QStringLiteral("LibrePCB: 符号 %1 的 Bézier 曲线已离散为折线").arg(symbol.name));
    }
    if (!symbol.ieeeSymbols.isEmpty() || !symbol.images.isEmpty() || !symbol.textFrames.isEmpty()) {
        diagnostics.append(
            QStringLiteral("LibrePCB: 符号 %1 包含当前适配器尚未支持的 IEEE、图片或文本框图元").arg(symbol.name));
        return false;
    }
    for (const auto& circle : symbol.circles) {
        SExpr& node = root.list(QStringLiteral("circle"));  ///< 当前圆图元节点。
        node.token(uuidFor(QStringLiteral("symbol-circle"), symbol.name, geometryIndex++));
        node.list(QStringLiteral("layer")).token(QStringLiteral("sym_outlines")).close();
        node.list(QStringLiteral("width")).token(number(circle.strokeWidth)).close();
        node.list(QStringLiteral("fill")).token(boolValue(circle.isFilled)).close();
        node.list(QStringLiteral("grab_area")).token(QStringLiteral("false")).close();
        node.list(QStringLiteral("diameter")).token(number(circle.radius * 2.0)).close();
        node.list(QStringLiteral("position")).token(number(circle.center.x())).token(number(circle.center.y())).close();
        node.close();
    }
    const QRectF bounds = symbolBounds(symbol);  ///< 符号图形和引脚的整体边界。
    const QRectF visibleTextArea = bounds.adjusted(-25.4, -25.4, 25.4, 25.4);  ///< 允许文本保留的区域。
    int relocatedTextIndex = 0;  ///< 已下移文本的编号。
    for (const auto& text : symbol.texts) {
        if (!text.visible)
            continue;
        QPointF textPosition = text.position;  ///< 当前文本输出位置。
        double textRotation = text.rotation;  ///< 当前文本输出角度。
        if (!visibleTextArea.contains(text.position)) {
            textPosition = QPointF(0.0, qMin(bounds.top(), bounds.bottom()) - 4.0 - relocatedTextIndex * 2.0);
            textRotation = 0.0;
            ++relocatedTextIndex;
            diagnostics.append(QStringLiteral("LibrePCB: 符号 %1 的文本“%2”超出图形边界，已移动到图形下方")
                                   .arg(symbol.name, text.text));
        }
        SExpr& node = root.list(QStringLiteral("text"));  ///< 当前普通文本节点。
        node.token(uuidFor(QStringLiteral("symbol-text"), symbol.name, geometryIndex++));
        node.list(QStringLiteral("layer")).token(QStringLiteral("sym_values")).close();
        node.list(QStringLiteral("height")).token(number(text.fontSizeMm > 0 ? text.fontSizeMm : 1.5)).close();
        node.list(QStringLiteral("align")).token(QStringLiteral("center")).token(QStringLiteral("center")).close();
        node.list(QStringLiteral("position")).token(number(textPosition.x())).token(number(textPosition.y())).close();
        node.list(QStringLiteral("rotation")).token(number(textRotation)).close();
        node.list(QStringLiteral("lock")).token(QStringLiteral("false")).close();
        node.list(QStringLiteral("value")).atom(text.text).close();
        node.close();
    }
    for (const auto& parameter : symbol.parameters) {
        if (!parameter.visible) {
            diagnostics.append(
                QStringLiteral("LibrePCB: 符号 %1 的隐藏参数未写入原生库：%2").arg(symbol.name, parameter.name));
            continue;
        }
        QPointF parameterPosition = parameter.position;  ///< 当前参数输出位置。
        double parameterRotation = parameter.rotation;  ///< 当前参数输出角度。
        if (!visibleTextArea.contains(parameter.position)) {
            parameterPosition = QPointF(0.0, qMin(bounds.top(), bounds.bottom()) - 4.0 - relocatedTextIndex * 2.0);
            parameterRotation = 0.0;
            ++relocatedTextIndex;
            diagnostics.append(QStringLiteral("LibrePCB: 符号 %1 的参数“%2”超出图形边界，已移动到图形下方")
                                   .arg(symbol.name, parameter.name));
        }
        SExpr& node = root.list(QStringLiteral("text"));  ///< 当前参数文本节点。
        node.token(uuidFor(QStringLiteral("symbol-parameter"), symbol.name, geometryIndex++));
        node.list(QStringLiteral("layer")).token(QStringLiteral("sym_values")).close();
        node.list(QStringLiteral("height"))
            .token(number(parameter.fontSizeMm > 0 ? parameter.fontSizeMm : 1.5))
            .close();
        node.list(QStringLiteral("align")).token(QStringLiteral("center")).token(QStringLiteral("center")).close();
        node.list(QStringLiteral("position"))
            .token(number(parameterPosition.x()))
            .token(number(parameterPosition.y()))
            .close();
        node.list(QStringLiteral("rotation")).token(number(parameterRotation)).close();
        node.list(QStringLiteral("lock")).token(boolValue(parameter.readOnly)).close();
        node.list(QStringLiteral("value")).atom(parameter.value).close();
        node.close();
    }
    bool hasNameText = false;  ///< 是否已有名称占位文本。
    bool hasValueText = false;  ///< 是否已有值占位文本。
    for (const auto& text : symbol.texts) {
        hasNameText = hasNameText || text.text == QStringLiteral("{{NAME}}");
        hasValueText = hasValueText || text.text == QStringLiteral("{{VALUE}}");
    }
    // LibrePCB 将器件名称和值作为符号必需字段；源数据缺失时仍生成可编辑的默认占位文本。
    // QRectF 的 top/bottom 语义容易与 EDA 坐标系的视觉上下方向混淆，显式取最小 Y，
    // 确保默认字段始终位于符号图形的视觉下方，而不是落到图形顶部。
    const double fieldBaseY = qMin(bounds.top(), bounds.bottom()) - 2.54;  ///< 默认字段基线 Y 坐标。
    const auto appendDefaultText = [&root, &geometryIndex, &symbol](
                                       const QString& layer, const QString& value, double y) {
        SExpr& node = root.list(QStringLiteral("text"));  ///< 默认字段文本节点。
        node.token(uuidFor(QStringLiteral("symbol-default-text"), symbol.name, geometryIndex++));
        node.list(QStringLiteral("layer")).token(layer).close();
        node.list(QStringLiteral("height")).token(QStringLiteral("1.5")).close();
        node.list(QStringLiteral("align")).token(QStringLiteral("center")).token(QStringLiteral("center")).close();
        node.list(QStringLiteral("position")).token(QStringLiteral("0")).token(number(y)).close();
        node.list(QStringLiteral("rotation")).token(QStringLiteral("0")).close();
        node.list(QStringLiteral("lock")).token(QStringLiteral("false")).close();
        node.list(QStringLiteral("value")).atom(value).close();
        node.close();
    };
    if (!hasNameText)
        appendDefaultText(QStringLiteral("sym_names"), QStringLiteral("{{NAME}}"), fieldBaseY);
    if (!hasValueText)
        appendDefaultText(QStringLiteral("sym_values"), QStringLiteral("{{VALUE}}"), fieldBaseY - 2.0);
    root.close();
    return writeElement(rootPath,
                        QStringLiteral("sym"),
                        QStringLiteral("symbol"),
                        uuidFor(QStringLiteral("symbol"), symbol.name),
                        root.finish(),
                        diagnostics);
}

/**
 * @brief 将一个封装 IR 写入 LibrePCB 原生封装文件。
 * @param footprint 待导出的封装 IR。
 * @param rootPath 库根目录。
 * @param export3d 是否导出封装中的三维模型。
 * @param diagnostics 用于追加失败原因的诊断列表。
 * @return 写入成功返回 true，否则返回 false。
 */
bool writePackage(const IR::FootprintComponentIR& footprint,
                  const QString& rootPath,
                  bool export3d,
                  QStringList& diagnostics) {
    if (footprint.name.trimmed().isEmpty()) {
        diagnostics.append(QStringLiteral("LibrePCB: 封装名称为空"));
        return false;
    }
    if (!qFuzzyIsNull(footprint.height))
        diagnostics.append(QStringLiteral("LibrePCB: 封装高度不会写入二维库定义：%1").arg(footprint.name));
    const QString packageUuid = uuidFor(QStringLiteral("package"), footprint.name);  ///< 封装 UUID。
    SExpr root(QStringLiteral("librepcb_package"));  ///< 封装根表达式。
    appendCommon(
        root, packageUuid, sanitizeName(footprint.name), footprint.description, QStringLiteral("librepcb_package"));
    root.list(QStringLiteral("category")).token(packageCategoryUuid()).close();
    root.list(QStringLiteral("assembly_type"))
        .token(footprint.hasThroughHolePads() ? QStringLiteral("tht") : QStringLiteral("smt"))
        .close();
    root.list(QStringLiteral("grid_interval")).token(QStringLiteral("0.1")).close();
    root.list(QStringLiteral("min_copper_clearance")).token(QStringLiteral("0.2")).close();

    bool sourceBoundsValid = false;  ///< 是否成功计算出源封装边界。
    const QRectF sourceBounds = footprintBounds(footprint, sourceBoundsValid);  ///< 源封装边界。
    const QPointF footprintOrigin = sourceBoundsValid ? sourceBounds.center() : QPointF(0.0, 0.0);  ///< 封装原点。
    if (sourceBoundsValid && !qFuzzyIsNull(footprintOrigin.x()) && !qFuzzyIsNull(footprintOrigin.y()))
        diagnostics.append(QStringLiteral("LibrePCB: 封装 %1 已将绝对坐标归一化到封装原点").arg(footprint.name));
    const auto localPoint = [&footprintOrigin](const QPointF& point) { return point - footprintOrigin; };
    const auto localPoints = [&footprintOrigin](const QList<QPointF>& points) {
        return localizeFootprintPoints(points, footprintOrigin);
    };

    struct ModelFile {
        QString uuid;  ///< 三维模型文件 UUID。
        QString fileName;  ///< 三维模型文件名。
        QByteArray data;  ///< 三维模型文件内容。
        QString displayName;  ///< 三维模型显示名称。
    };

    QList<ModelFile> modelFiles;  ///< 待写入封装目录的三维模型文件。
    if (export3d) {
        for (int modelIndex = 0; modelIndex < footprint.models3d.size(); ++modelIndex) {
            const IR::Model3DIR& model = footprint.models3d.at(modelIndex);  ///< 当前三维模型。
            const QString modelUuid =
                uuidFor(QStringLiteral("package-model"), footprint.name, modelIndex);  ///< 模型 UUID。
            if (model.hasStepData()) {
                modelFiles.append(
                    {modelUuid, modelUuid + QStringLiteral(".step"), model.stepData(), sanitizeName(model.name())});
            } else if (model.hasObjData()) {
                modelFiles.append({modelUuid,
                                   modelUuid + QStringLiteral(".obj"),
                                   model.rawObj().toUtf8(),
                                   sanitizeName(model.name())});
                diagnostics.append(
                    QStringLiteral("LibrePCB: 仅复制 OBJ 三维模型，目标环境可能需要转换为 STEP：%1").arg(model.name()));
            } else {
                diagnostics.append(
                    QStringLiteral("LibrePCB: 三维模型没有可写入的 STEP/OBJ 数据：%1").arg(model.name()));
                return false;
            }
            const ModelFile& item = modelFiles.constLast();  ///< 最近追加的模型文件。
            root.list(QStringLiteral("3d_model"))
                .token(item.uuid)
                .list(QStringLiteral("name"))
                .atom(item.displayName.isEmpty() ? QStringLiteral("model") : item.displayName)
                .close()
                .close();
        }
    } else if (!footprint.models3d.isEmpty()) {
        diagnostics.append(QStringLiteral("LibrePCB: 已跳过 %1 个三维模型，因为当前导出选项未启用三维数据")
                               .arg(footprint.models3d.size()));
    }
    QHash<QString, QString> padUuids;  ///< 封装焊盘编号到 UUID 的映射。
    for (int i = 0; i < footprint.pads.size(); ++i) {
        const auto& pad = footprint.pads.at(i);  ///< 当前封装焊盘。
        const QString padNumber = pad.number.trimmed();  ///< 清理后的焊盘编号。
        if (padNumber.isEmpty()) {
            diagnostics.append(QStringLiteral("LibrePCB: 封装 %1 存在空焊盘编号").arg(footprint.name));
            return false;
        }
        if (padUuids.contains(padNumber)) {
            diagnostics.append(QStringLiteral("LibrePCB: 封装 %1 存在重复焊盘编号 %2").arg(footprint.name, padNumber));
            return false;
        }
        if (!pad.netName.isEmpty())
            diagnostics.append(QStringLiteral("LibrePCB: 封装焊盘网络名称不会写入库定义：%1").arg(pad.netName));
        if (pad.isLocked)
            diagnostics.append(QStringLiteral("LibrePCB: 封装焊盘锁定属性不会写入库定义：%1").arg(pad.number));
        const QString uuid = uuidFor(QStringLiteral("package-pad"), footprint.name, i);  ///< Package Pad UUID。
        padUuids.insert(padNumber, uuid);
        SExpr& node = root.list(QStringLiteral("pad"));  ///< 当前 Package Pad 节点。
        node.token(uuid);
        node.list(QStringLiteral("name")).atom(padNumber).close();
        node.close();
    }
    const QString fpUuid = uuidFor(QStringLiteral("footprint"), footprint.name);  ///< Footprint UUID。
    SExpr& fp = root.list(QStringLiteral("footprint"));  ///< 当前 Footprint 节点。
    fp.token(fpUuid);
    fp.list(QStringLiteral("name")).atom(sanitizeName(footprint.name)).close();
    fp.list(QStringLiteral("description")).atom(footprint.description).close();
    // 模型位置。
    const IR::Model3DVec3 modelPosition = footprint.models3d.isEmpty()
                                              ? IR::Model3DVec3()
                                              : footprint.models3d.first().translation();  ///< 三维模型位置。
    // 偏移量。
    const IR::Model3DVec3 modelOffset = footprint.models3d.isEmpty()
                                            ? IR::Model3DVec3()
                                            : footprint.models3d.first().stepOffsetMm();  ///< 三维模型偏移。
    const IR::Model3DVec3 modelRotation =
        footprint.models3d.isEmpty() ? IR::Model3DVec3() : footprint.models3d.first().rotation();  ///< 三维模型旋转。
    fp.list(QStringLiteral("3d_position"))
        .token(number(modelPosition.x + modelOffset.x - footprintOrigin.x()))
        .token(number(modelPosition.y + modelOffset.y - footprintOrigin.y()))
        .token(number(modelPosition.z + modelOffset.z))
        .close();
    fp.list(QStringLiteral("3d_rotation"))
        .token(number(modelRotation.x))
        .token(number(modelRotation.y))
        .token(number(modelRotation.z))
        .close();
    for (const ModelFile& model : modelFiles)
        fp.list(QStringLiteral("3d_model")).token(model.uuid).close();
    int index = 0;  ///< 封装图元 UUID 的索引。
    for (const auto& pad : footprint.pads) {
        const QString padUuid =
            uuidFor(QStringLiteral("footprint-pad"), footprint.name, index);  ///< Footprint Pad UUID。
        SExpr& node = fp.list(QStringLiteral("pad"));  ///< 当前 Footprint Pad 节点。
        node.token(padUuid);
        node.list(QStringLiteral("side"))
            // LibrePCB 的 through-hole 属性由 hole 节点表达，side 仍只能是 top 或 bottom。
            .token(pad.layer == IR::LayerType::BottomCopper ? QStringLiteral("bottom") : QStringLiteral("top"))
            .close();
        // 当前焊盘对应的 LibrePCB 形状名称。
        const QString shape = padShape(
            pad.shape, pad.size, pad.customShapePoints, diagnostics, footprint.name + QStringLiteral("/") + pad.number);
        // 形状名称决定后续是否写入自定义顶点。
        if (shape.isEmpty())
            return false;
        node.list(QStringLiteral("shape")).token(shape).close();
        const QPointF padPosition = localPoint(pad.position);  ///< 相对于封装原点的焊盘位置。
        node.list(QStringLiteral("position")).token(number(padPosition.x())).token(number(padPosition.y())).close();
        node.list(QStringLiteral("rotation")).token(number(pad.rotation)).close();
        node.list(QStringLiteral("size")).token(number(pad.size.width())).token(number(pad.size.height())).close();
        node.list(QStringLiteral("radius")).token(QStringLiteral("0")).close();
        node.list(QStringLiteral("stop_mask")).token(QStringLiteral("auto")).close();
        node.list(QStringLiteral("solder_paste"))
            .token(pad.isThroughHole() ? QStringLiteral("off") : QStringLiteral("auto"))
            .close();
        node.list(QStringLiteral("clearance")).token(QStringLiteral("0")).close();
        node.list(QStringLiteral("function")).token(QStringLiteral("standard")).close();
        node.list(QStringLiteral("package_pad")).token(padUuids.value(pad.number.trimmed())).close();
        if (shape == QStringLiteral("custom")) {
            for (const QPointF& point : pad.customShapePoints) {
                node.list(QStringLiteral("vertex"))
                    .list(QStringLiteral("position"))
                    .token(number(point.x()))
                    .token(number(point.y()))
                    .close()
                    .list(QStringLiteral("angle"))
                    .token(QStringLiteral("0"))
                    .close()
                    .close();
            }
        }
        if (pad.isThroughHole()) {
            SExpr& hole = node.list(QStringLiteral("hole"));  ///< 当前通孔节点。
            hole.token(uuidFor(QStringLiteral("pad-hole"), footprint.name, index));
            hole.list(QStringLiteral("diameter")).token(number(pad.holeSize > 0 ? pad.holeSize : 0.3)).close();
            hole.list(QStringLiteral("vertex"))
                .list(QStringLiteral("position"))
                .token(number(pad.holeLength > 0 ? -pad.holeLength / 2.0 : 0))
                .token(QStringLiteral("0"))
                .close()
                .list(QStringLiteral("angle"))
                .token(QStringLiteral("0"))
                .close()
                .close();
            if (pad.holeLength > 0) {
                hole.list(QStringLiteral("vertex"))
                    .list(QStringLiteral("position"))
                    .token(number(pad.holeLength / 2.0))
                    .token(QStringLiteral("0"))
                    .close()
                    .list(QStringLiteral("angle"))
                    .token(QStringLiteral("0"))
                    .close()
                    .close();
            }
            hole.close();
            if (!pad.isPlated)
                diagnostics.append(
                    QStringLiteral("LibrePCB: 封装 %1 的非镀通孔无法在 PadHole 中保留镀层语义").arg(footprint.name));
        }
        node.close();
        ++index;
    }
    for (const auto& rectangle : footprint.rectangles) {
        const QString layer = layerForFootprint(rectangle.layer);  ///< 矩形目标图层。
        if (layer.isEmpty()) {
            diagnostics.append(QStringLiteral("LibrePCB: 矩形使用无法映射的图层"));
            return false;
        }
        if (isAssemblyLayer(rectangle.layer))
            diagnostics.append(QStringLiteral("LibrePCB: 装配层矩形已映射到 Documentation 层"));
        if (rectangle.isLocked)
            diagnostics.append(QStringLiteral("LibrePCB: 封装矩形锁定属性不会写入库定义：%1").arg(footprint.name));
        appendPolygon(fp,
                      uuidFor(QStringLiteral("footprint-rectangle"), footprint.name, index++),
                      layer,
                      localPoints(rectanglePoints(rectangle.bounds, rectangle.rotation)),
                      rectangle.strokeWidth,
                      false);
    }
    for (const auto& outline : footprint.outlines) {
        const QString layer = layerForFootprint(outline.layer);  ///< 轮廓目标图层。
        if (layer.isEmpty() || outline.points.size() < 2) {
            diagnostics.append(QStringLiteral("LibrePCB: 封装轮廓无法映射或顶点不足"));
            return false;
        }
        if (isAssemblyLayer(outline.layer))
            diagnostics.append(QStringLiteral("LibrePCB: 装配层轮廓已映射到 Documentation 层"));
        if (outline.isLocked)
            diagnostics.append(QStringLiteral("LibrePCB: 封装轮廓锁定属性不会写入库定义：%1").arg(footprint.name));
        appendPolygon(fp,
                      uuidFor(QStringLiteral("footprint-outline"), footprint.name, index++),
                      layer,
                      localPoints(outline.points),
                      outline.strokeWidth,
                      false);
    }
    for (const auto& circle : footprint.circles) {
        const QString layer = layerForFootprint(circle.layer);  ///< 圆形目标图层。
        if (layer.isEmpty()) {
            diagnostics.append(QStringLiteral("LibrePCB: 圆形图元使用无法映射的图层"));
            return false;
        }
        if (isAssemblyLayer(circle.layer))
            diagnostics.append(QStringLiteral("LibrePCB: 装配层圆形已映射到 Documentation 层"));
        else if (isUserDefinedLayer(circle.layer))
            diagnostics.append(
                QStringLiteral("LibrePCB: 封装 %1 的用户定义圆形已降级到 Documentation 层").arg(footprint.name));
        SExpr& node = fp.list(QStringLiteral("circle"));  ///< 当前圆图元节点。
        node.token(uuidFor(QStringLiteral("footprint-circle"), footprint.name, index++));
        node.list(QStringLiteral("layer")).token(layer).close();
        node.list(QStringLiteral("width")).token(number(circle.strokeWidth)).close();
        node.list(QStringLiteral("fill")).token(QStringLiteral("false")).close();
        node.list(QStringLiteral("grab_area")).token(QStringLiteral("false")).close();
        node.list(QStringLiteral("diameter")).token(number(circle.radius * 2)).close();
        const QPointF circlePosition = localPoint(circle.center);  ///< 相对于封装原点的圆心。
        node.list(QStringLiteral("position"))
            .token(number(circlePosition.x()))
            .token(number(circlePosition.y()))
            .close();
        node.close();
    }
    for (int trackIndex = 0; trackIndex < footprint.tracks.size(); ++trackIndex) {
        const auto& track = footprint.tracks.at(trackIndex);  ///< 当前封装走线。
        const QString layer = layerForFootprint(track.layer);  ///< 走线目标图层。
        if (layer.isEmpty() || track.points.size() < 2) {
            diagnostics.append(QStringLiteral("LibrePCB: 封装走线无法映射或顶点不足：%1").arg(footprint.name));
            return false;
        }
        appendPolygon(fp,
                      uuidFor(QStringLiteral("footprint-track"), footprint.name, trackIndex),
                      layer,
                      localPoints(track.points),
                      track.width,
                      false);
        if (!track.netName.isEmpty())
            diagnostics.append(QStringLiteral("LibrePCB: 封装走线网络名称未写入库图元：%1").arg(track.netName));
        if (track.isLocked)
            diagnostics.append(QStringLiteral("LibrePCB: 封装走线锁定属性不会写入库定义：%1").arg(footprint.name));
    }
    for (int arcIndex = 0; arcIndex < footprint.arcs.size(); ++arcIndex) {
        const auto& arc = footprint.arcs.at(arcIndex);  ///< 当前封装圆弧。
        const QString layer = layerForFootprint(arc.layer);  ///< 圆弧目标图层。
        if (layer.isEmpty() || arc.radius <= 0) {
            diagnostics.append(QStringLiteral("LibrePCB: 封装圆弧无法映射：%1").arg(footprint.name));
            return false;
        }
        appendPolygon(fp,
                      uuidFor(QStringLiteral("footprint-arc"), footprint.name, arcIndex),
                      layer,
                      localPoints(sampleArc(arc.center, arc.radius, arc.startAngle, arc.endAngle)),
                      arc.width,
                      false);
        diagnostics.append(QStringLiteral("LibrePCB: 封装 %1 的圆弧已离散为折线").arg(footprint.name));
        if (!arc.netName.isEmpty())
            diagnostics.append(QStringLiteral("LibrePCB: 封装圆弧网络名称不会写入库图元：%1").arg(arc.netName));
        if (arc.isLocked)
            diagnostics.append(QStringLiteral("LibrePCB: 封装圆弧锁定属性不会写入库定义：%1").arg(footprint.name));
    }
    for (int regionIndex = 0; regionIndex < footprint.regions.size(); ++regionIndex) {
        const auto& region = footprint.regions.at(regionIndex);  ///< 当前封装区域。
        const QString layer = layerForFootprint(region.layer);  ///< 区域目标图层。
        if (layer.isEmpty() || region.vertices.size() < 3) {
            diagnostics.append(QStringLiteral("LibrePCB: 封装区域无法映射或顶点不足：%1").arg(footprint.name));
            return false;
        }
        if (region.isKeepOut || region.layer == IR::LayerType::KeepOut)
            diagnostics.append(QStringLiteral("LibrePCB: 封装 %1 的 KeepOut/ComponentShape 区域已映射到 Courtyard 层")
                                   .arg(footprint.name));
        appendPolygon(fp,
                      uuidFor(QStringLiteral("footprint-region"), footprint.name, regionIndex),
                      layer,
                      localPoints(region.vertices),
                      0,
                      true);
        if (region.isLocked)
            diagnostics.append(QStringLiteral("LibrePCB: 封装区域锁定属性不会写入库定义：%1").arg(footprint.name));
    }
    for (int holeIndex = 0; holeIndex < footprint.holes.size(); ++holeIndex) {
        const auto& hole = footprint.holes.at(holeIndex);  ///< 当前封装安装孔。
        if (hole.radius <= 0) {
            diagnostics.append(QStringLiteral("LibrePCB: 封装 %1 存在非法安装孔尺寸").arg(footprint.name));
            return false;
        }
        SExpr& node = fp.list(QStringLiteral("hole"));  ///< 当前安装孔节点。
        node.token(uuidFor(QStringLiteral("footprint-hole"), footprint.name, holeIndex));
        node.list(QStringLiteral("diameter")).token(number(hole.radius * 2.0)).close();
        const QPointF holePosition = localPoint(hole.center);  ///< 相对于封装原点的安装孔位置。
        node.list(QStringLiteral("position")).token(number(holePosition.x())).token(number(holePosition.y())).close();
        node.close();
        if (hole.isLocked)
            diagnostics.append(QStringLiteral("LibrePCB: 封装安装孔锁定属性不会写入库定义：%1").arg(footprint.name));
    }
    for (int textIndex = 0; textIndex < footprint.texts.size(); ++textIndex) {
        const auto& text = footprint.texts.at(textIndex);  ///< 当前封装文本。
        if (!text.isDisplayed)
            continue;
        const QString layer = layerForFootprint(text.layer);  ///< 文本目标图层。
        if (layer.isEmpty()) {
            diagnostics.append(QStringLiteral("LibrePCB: 封装文本使用无法映射的图层：%1").arg(text.text));
            return false;
        }
        if (!text.textPathPoints.isEmpty()) {
            diagnostics.append(QStringLiteral("LibrePCB: 封装文本路径已忽略，使用普通笔画文本：%1").arg(text.text));
        }
        appendStrokeText(fp,
                         uuidFor(QStringLiteral("footprint-text"), footprint.name, textIndex),
                         layer,
                         text.text,
                         localPoint(text.position),
                         text.rotation,
                         text.fontSize,
                         text.strokeWidth,
                         text.mirror);
    }
    bool hasPackageOutline = false;  ///< 是否存在源 Package Outlines。
    bool hasCourtyard = false;  ///< 是否存在源 Courtyard。
    bool hasNameText = false;  ///< 是否存在名称占位文本。
    bool hasValueText = false;  ///< 是否存在值占位文本。
    for (const auto& text : footprint.texts) {
        hasNameText = hasNameText || text.text == QStringLiteral("{{NAME}}");
        hasValueText = hasValueText || text.text == QStringLiteral("{{VALUE}}");
    }
    for (const auto& rectangle : footprint.rectangles) {
        hasPackageOutline = hasPackageOutline || rectangle.layer == IR::LayerType::EdgeCuts;
    }
    for (const auto& outline : footprint.outlines) {
        hasPackageOutline = hasPackageOutline || outline.layer == IR::LayerType::EdgeCuts;
    }
    bool boundsValid = false;  ///< 是否成功得到回退边界。
    const QRectF bounds = footprintBounds(footprint, boundsValid);  ///< 回退外形和 Courtyard 的边界。
    if (boundsValid && !hasPackageOutline) {
        appendPolygon(fp,
                      uuidFor(QStringLiteral("fallback-outline"), footprint.name),
                      QStringLiteral("top_package_outlines"),
                      localPoints(rectanglePoints(bounds, 0)),
                      0.1,
                      false);
        diagnostics.append(QStringLiteral("LibrePCB: 封装 %1 缺少可靠外形，已根据 IR 图元生成 Package Outlines 回退")
                               .arg(footprint.name));
    }
    if (boundsValid && !hasCourtyard) {
        appendPolygon(fp,
                      uuidFor(QStringLiteral("fallback-courtyard"), footprint.name),
                      QStringLiteral("top_courtyard"),
                      localPoints(rectanglePoints(bounds.adjusted(-0.25, -0.25, 0.25, 0.25), 0)),
                      0.05,
                      false);
        diagnostics.append(
            QStringLiteral("LibrePCB: 封装 %1 缺少 courtyard，已根据 IR 图元生成回退").arg(footprint.name));
    }
    if (boundsValid && !hasNameText)
        appendStrokeText(fp,
                         uuidFor(QStringLiteral("fallback-name"), footprint.name),
                         QStringLiteral("top_names"),
                         QStringLiteral("{{NAME}}"),
                         localPoint(QPointF(bounds.center().x(), bounds.bottom() + 1.0)));
    if (boundsValid && !hasValueText)
        appendStrokeText(fp,
                         uuidFor(QStringLiteral("fallback-value"), footprint.name),
                         QStringLiteral("top_values"),
                         QStringLiteral("{{VALUE}}"),
                         localPoint(QPointF(bounds.center().x(), bounds.top() - 1.0)));
    fp.close();
    root.close();
    if (!writeElement(
            rootPath, QStringLiteral("pkg"), QStringLiteral("package"), packageUuid, root.finish(), diagnostics))
        return false;
    const QString packageDirectory = QDir(rootPath).filePath(QStringLiteral("pkg/") + packageUuid);  ///< 封装目录路径。
    for (const ModelFile& model : modelFiles) {
        if (!writeFile(QDir(packageDirectory).filePath(model.fileName), model.data, diagnostics))
            return false;
    }
    return true;
}

/**
 * @brief 写入组件、器件及其符号到封装的关联关系。
 * @param component 待导出的完整组件 IR。
 * @param rootPath 库根目录。
 * @param diagnostics 用于追加失败原因的诊断列表。
 * @return 写入成功返回 true，否则返回 false。
 */
bool writeComponentAndDevice(const IR::ComponentIR& component, const QString& rootPath, QStringList& diagnostics) {
    if (!component.hasSymbol() || !component.hasFootprint()) {
        diagnostics.append(QStringLiteral("LibrePCB: 组件 %1 缺少符号或封装，无法建立完整关联").arg(component.name));
        return false;
    }
    if (!component.datasheet.isEmpty())
        diagnostics.append(QStringLiteral("LibrePCB: 组件数据手册地址未写入器件属性：%1").arg(component.datasheet));
    const QString symbolUuid = uuidFor(QStringLiteral("symbol"), component.symbol.name);  ///< 关联符号 UUID。
    const QString packageUuid = uuidFor(QStringLiteral("package"), component.footprint.name);  ///< 关联封装 UUID。
    const QString componentUuid = uuidFor(QStringLiteral("component"), component.name);  ///< 组件 UUID。
    const QString deviceUuid = uuidFor(QStringLiteral("device"), component.name);  ///< 器件 UUID。
    SExpr cmp(QStringLiteral("librepcb_component"));
    appendCommon(
        cmp, componentUuid, sanitizeName(component.name), component.description, QStringLiteral("librepcb_component"));
    cmp.list(QStringLiteral("category")).token(componentCategoryUuid()).close();
    cmp.list(QStringLiteral("schematic_only")).token(QStringLiteral("false")).close();
    cmp.list(QStringLiteral("default_value")).atom(component.name).close();
    cmp.list(QStringLiteral("prefix"))
        .atom(component.prefix.isEmpty() ? QStringLiteral("U") : component.prefix)
        .close();
    int signalIndex = 0;  ///< 信号 UUID 的稳定索引。
    QHash<QString, QString> signalUuids;  ///< 引脚编号到信号 UUID 的映射。
    for (const auto& pin : component.symbol.pins) {
        const QString designator = pin.designator.trimmed();  ///< 清理后的组件引脚编号。
        if (designator.isEmpty()) {
            diagnostics.append(
                QStringLiteral("LibrePCB: 组件 %1 存在空引脚编号，无法建立信号映射").arg(component.name));
            return false;
        }
        if (signalUuids.contains(designator)) {
            diagnostics.append(QStringLiteral("LibrePCB: 组件 %1 存在重复引脚编号，无法建立唯一信号映射：%2")
                                   .arg(component.name, designator));
            return false;
        }
        const QString signalUuid = uuidFor(QStringLiteral("signal"), component.name, signalIndex);  ///< 当前信号 UUID。
        signalUuids.insert(designator, signalUuid);
        SExpr& signal = cmp.list(QStringLiteral("signal"));  ///< 当前信号节点。
        signal.token(signalUuid)
            .list(QStringLiteral("name"))
            .atom(pin.name.isEmpty() ? pin.designator : pin.name)
            .close();
        signal.list(QStringLiteral("role")).token(QStringLiteral("passive")).close();
        signal.list(QStringLiteral("required")).token(QStringLiteral("false")).close();
        signal.list(QStringLiteral("negated")).token(QStringLiteral("false")).close();
        signal.list(QStringLiteral("clock")).token(QStringLiteral("false")).close();
        signal.list(QStringLiteral("forced_net")).atom(QString()).close();
        signal.close();
        ++signalIndex;
    }
    const QString variantUuid = uuidFor(QStringLiteral("variant"), component.name);  ///< 变体 UUID。
    const QString itemUuid = uuidFor(QStringLiteral("variant-item"), component.name);  ///< 变体项 UUID。
    SExpr& variant = cmp.list(QStringLiteral("variant"));  ///< 当前组件变体节点。
    variant.token(variantUuid).list(QStringLiteral("norm")).atom(QString()).close();
    appendName(variant, sanitizeName(component.name));
    appendDescription(variant, component.description);
    SExpr& item = variant.list(QStringLiteral("gate"));  ///< 当前 Gate 节点。
    item.token(itemUuid).list(QStringLiteral("symbol")).token(symbolUuid).close();
    item.list(QStringLiteral("position")).token(QStringLiteral("0")).token(QStringLiteral("0")).close();
    item.list(QStringLiteral("rotation")).token(QStringLiteral("0")).close();
    item.list(QStringLiteral("required")).token(QStringLiteral("true")).close();
    item.list(QStringLiteral("suffix")).atom(QString()).close();
    for (int i = 0; i < component.symbol.pins.size(); ++i) {
        const auto& pin = component.symbol.pins.at(i);  ///< 当前组件符号引脚。
        const QString signalUuid = signalUuids.value(pin.designator);  ///< 当前引脚对应的信号 UUID。
        SExpr& map = item.list(QStringLiteral("pin"));  ///< 当前引脚映射节点。
        map.token(uuidFor(QStringLiteral("symbol-pin"), component.symbol.name, i));
        map.list(QStringLiteral("signal")).token(signalUuid).close();
        map.list(QStringLiteral("text")).token(QStringLiteral("signal")).close();
        map.close();
    }
    item.close();
    variant.close();
    cmp.close();
    if (!writeElement(
            rootPath, QStringLiteral("cmp"), QStringLiteral("component"), componentUuid, cmp.finish(), diagnostics))
        return false;

    SExpr dev(QStringLiteral("librepcb_device"));  ///< 器件根表达式。
    appendCommon(
        dev, deviceUuid, sanitizeName(component.name), component.description, QStringLiteral("librepcb_device"));
    dev.list(QStringLiteral("category")).token(componentCategoryUuid()).close();
    dev.list(QStringLiteral("component")).token(componentUuid).close();
    dev.list(QStringLiteral("package")).token(packageUuid).close();
    if (!component.manufacturerPart.trimmed().isEmpty()) {
        dev.list(QStringLiteral("part"))
            .atom(component.manufacturerPart)
            .list(QStringLiteral("manufacturer"))
            .atom(component.manufacturer)
            .close()
            .close();
    }
    for (int i = 0; i < component.footprint.pads.size(); ++i) {
        const auto& pad = component.footprint.pads.at(i);  ///< 当前组件封装焊盘。
        SExpr& map = dev.list(QStringLiteral("pad"));  ///< 当前器件焊盘映射节点。
        map.token(uuidFor(QStringLiteral("package-pad"), component.footprint.name, i));
        map.list(QStringLiteral("optional")).token(QStringLiteral("false")).close();
        const QString padNumber = pad.number.trimmed();  ///< 清理后的焊盘编号。
        if (signalUuids.contains(padNumber))
            map.list(QStringLiteral("signal")).token(signalUuids.value(padNumber)).close();
        else
            map.list(QStringLiteral("signal")).token(QStringLiteral("none")).close();
        map.close();
    }
    dev.close();
    return writeElement(
        rootPath, QStringLiteral("dev"), QStringLiteral("device"), deviceUuid, dev.finish(), diagnostics);
}

/**
 * @brief 创建并初始化 LibrePCB 库目录及分类。
 * @param path 库输出路径。
 * @param name 库名称。
 * @param description 库描述。
 * @param keywords 库关键词。
 * @param diagnostics 用于追加失败原因的诊断列表。
 * @return 初始化成功返回 true，否则返回 false。
 */
bool prepareLibrary(const QString& path,
                    const QString& name,
                    const QString& description,
                    const QString& keywords,
                    QStringList& diagnostics) {
    if (QFileInfo(path).exists() && !QFileInfo(path).isDir()) {
        diagnostics.append(QStringLiteral("LibrePCB: 输出路径不是目录：%1").arg(path));
        return false;
    }
    return writeLibraryRoot(path, name, description, keywords, diagnostics) &&
           writeLibraryCategories(path, diagnostics);
}

}  // namespace

/**
 * @brief 返回 LibrePCB 原生库的目录后缀。
 * @return LibrePCB 库目录后缀。
 */
QString ExporterLibrePcbLibrary::libraryFileExtension() const {
    return QStringLiteral(".lplib");
}

/**
 * @brief 指示 LibrePCB 导出结果是否为目录。
 * @return LibrePCB 原生库使用目录结构时返回 true。
 */
bool ExporterLibrePcbLibrary::isDirectoryOutput() const {
    return true;
}

/**
 * @brief 导出单个 LibrePCB 封装库。
 * @param footprint 待导出的封装 IR。
 * @param filePath 库输出路径。
 * @param model3DPath 外部三维模型路径。
 * @return 导出成功返回 true，否则返回 false。
 */
bool ExporterLibrePcbLibrary::exportFootprint(const IR::FootprintComponentIR& footprint,
                                              const QString& filePath,
                                              const QString& model3DPath) {
    m_diagnostics.clear();
    if (!model3DPath.isEmpty())
        m_diagnostics.append(
            QStringLiteral("LibrePCB: 外部三维模型路径未读取，仅接受 IR 中的模型数据：%1").arg(model3DPath));
    return prepareLibrary(filePath, footprint.name, footprint.description, QString(), m_diagnostics) &&
           writePackage(footprint, filePath, true, m_diagnostics);
}

/**
 * @brief 批量导出 LibrePCB 封装库。
 * @param footprints 待导出的封装 IR 列表。
 * @param libName 库名称。
 * @param filePath 库输出路径。
 * @param preferWrl 是否优先使用 VRML 模型。
 * @param exportStep 是否导出 STEP 模型。
 * @param libraryDescription 库描述。
 * @param libraryKeywords 库关键词。
 * @param useAbsolutePaths 是否使用绝对模型路径。
 * @param model3DBaseDir 外部三维模型根目录。
 * @return 导出成功返回 true，否则返回 false。
 */
bool ExporterLibrePcbLibrary::exportFootprintLibrary(const QList<IR::FootprintComponentIR>& footprints,
                                                     const QString& libName,
                                                     const QString& filePath,
                                                     bool preferWrl,
                                                     bool exportStep,
                                                     const QString& libraryDescription,
                                                     const QString& libraryKeywords,
                                                     bool useAbsolutePaths,
                                                     const QString& model3DBaseDir) {
    m_diagnostics.clear();
    if (preferWrl)
        m_diagnostics.append(QStringLiteral("LibrePCB: 优先 VRML 选项不适用于原生库导出，使用 IR 中的模型数据"));
    if (useAbsolutePaths || !model3DBaseDir.isEmpty())
        m_diagnostics.append(QStringLiteral("LibrePCB: 外部三维模型路径选项不会改变原生库中的内嵌模型路径"));
    if (exportStep)
        m_diagnostics.append(QStringLiteral("LibrePCB: STEP 文件需要复制到 package 目录并关联，当前仅保留模型元数据"));
    if (!prepareLibrary(filePath, libName, libraryDescription, libraryKeywords, m_diagnostics))
        return false;
    for (const auto& footprint : footprints)
        if (!writePackage(footprint, filePath, exportStep, m_diagnostics))
            return false;
    return true;
}

/**
 * @brief 导出单个模式的 LibrePCB 符号库。
 * @param symbols 待导出的符号 IR 列表。
 * @param libName 库名称。
 * @param filePath 库输出路径。
 * @return 导出成功返回 true，否则返回 false。
 */
bool ExporterLibrePcbLibrary::exportSymbolLibrary(const QList<IR::SymbolComponentIR>& symbols,
                                                  const QString& libName,
                                                  const QString& filePath) {
    return exportSymbolLibrary(symbols, libName, filePath, false, false, QString());
}

/**
 * @brief 按指定运行模式导出 LibrePCB 符号库。
 * @param symbols 待导出的符号 IR 列表。
 * @param libName 库名称。
 * @param filePath 库输出路径。
 * @param appendMode 是否追加已有库内容。
 * @param updateMode 是否更新已有库内容。
 * @param libraryDescription 库描述。
 * @return 导出成功返回 true，否则返回 false。
 */
bool ExporterLibrePcbLibrary::exportSymbolLibrary(const QList<IR::SymbolComponentIR>& symbols,
                                                  const QString& libName,
                                                  const QString& filePath,
                                                  bool appendMode,
                                                  bool updateMode,
                                                  const QString& libraryDescription) {
    m_diagnostics.clear();
    if (appendMode || updateMode) {
        m_diagnostics.append(QStringLiteral("LibrePCB: 原生 .lplib 导出暂不支持追加或更新模式"));
        return false;
    }
    if (!prepareLibrary(filePath, libName, libraryDescription, QString(), m_diagnostics))
        return false;
    for (const auto& symbol : symbols)
        if (!writeSymbol(symbol, filePath, m_diagnostics))
            return false;
    return true;
}

/**
 * @brief 导出单个 LibrePCB 符号。
 * @param symbol 待导出的符号 IR。
 * @param filePath 库输出路径。
 * @return 导出成功返回 true，否则返回 false。
 */
bool ExporterLibrePcbLibrary::exportSymbol(const IR::SymbolComponentIR& symbol, const QString& filePath) {
    return exportSymbolLibrary({symbol}, symbol.name, filePath);
}

/**
 * @brief 批量导出 LibrePCB 组件、器件及其关联。
 * @param components 待导出的组件 IR 列表。
 * @param libName 库名称。
 * @param filePath 库输出路径。
 * @param exportModel3D 是否导出三维模型。
 * @param model3DBaseDir 外部三维模型根目录。
 * @return 导出成功返回 true，否则返回 false。
 */
bool ExporterLibrePcbLibrary::exportComponentLibrary(const QList<IR::ComponentIR>& components,
                                                     const QString& libName,
                                                     const QString& filePath,
                                                     bool exportModel3D,
                                                     const QString& model3DBaseDir) {
    m_diagnostics.clear();
    if (!model3DBaseDir.isEmpty())
        m_diagnostics.append(
            QStringLiteral("LibrePCB: 外部三维模型目录不会被扫描，仅导出 IR 中的模型数据：%1").arg(model3DBaseDir));
    if (!prepareLibrary(filePath, libName, QString(), QString(), m_diagnostics))
        return false;
    QSet<QString> symbolNames;  ///< 已使用的清理后符号名称。
    QSet<QString> packageNames;  ///< 已使用的清理后封装名称。
    QSet<QString> componentNames;  ///< 已使用的清理后组件名称。
    for (const auto& component : components) {
        const QString symbolName = sanitizeName(component.symbol.name);  ///< 清理后的符号名称。
        const QString packageName = sanitizeName(component.footprint.name);  ///< 清理后的封装名称。
        const QString componentName = sanitizeName(component.name);  ///< 清理后的组件名称。
        if (symbolNames.contains(symbolName) || packageNames.contains(packageName) ||
            componentNames.contains(componentName)) {
            m_diagnostics.append(QStringLiteral("LibrePCB: 名称清理后发生库元素冲突：%1").arg(component.name));
            return false;
        }
        symbolNames.insert(symbolName);
        packageNames.insert(packageName);
        componentNames.insert(componentName);
        if (!writeSymbol(component.symbol, filePath, m_diagnostics) ||
            !writePackage(component.footprint, filePath, exportModel3D, m_diagnostics) ||
            !writeComponentAndDevice(component, filePath, m_diagnostics))
            return false;
    }
    return true;
}

/**
 * @brief 返回本次导出的结构化诊断信息。
 * @return 导出诊断列表。
 */
QStringList ExporterLibrePcbLibrary::diagnostics() const {
    return m_diagnostics;
}

}  // namespace EasyKiConverter
