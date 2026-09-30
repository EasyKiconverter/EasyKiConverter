#include "ExporterLibrePcbLibrary.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QSaveFile>
#include <QSet>
#include <QtMath>

#include <cmath>
#include <utility>

namespace EasyKiConverter {
namespace {

constexpr int kLibrePcbFileFormatVersion = 2;

class SExpr {
public:
    explicit SExpr(QString name) : m_name(std::move(name)) {}

    SExpr& atom(const QString& value) {
        m_children.append(quoteIfNeeded(value));
        return *this;
    }

    SExpr& token(const QString& value) {
        m_children.append(value);
        return *this;
    }

    SExpr& list(const QString& name) {
        m_children.append(QStringLiteral("(") + name);
        m_openLists.append(m_children.size() - 1);
        return *this;
    }

    SExpr& close() {
        if (!m_openLists.isEmpty()) {
            m_children.append(QStringLiteral(")"));
            m_openLists.removeLast();
        }
        return *this;
    }

    QString finish() {
        while (!m_openLists.isEmpty())
            close();
        QString result = QStringLiteral("(") + m_name;
        for (const QString& child : std::as_const(m_children))
            result += QLatin1Char(' ') + child;
        return result + QStringLiteral(")\n");
    }

private:
    static QString quoteIfNeeded(const QString& value) {
        QString escaped = value;
        escaped.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
        escaped.replace(QLatin1Char('"'), QStringLiteral("\\\""));
        escaped.replace(QLatin1Char('\n'), QStringLiteral("\\n"));
        return QStringLiteral("\"") + escaped + QLatin1Char('"');
    }

    QString m_name;
    QStringList m_children;
    QList<int> m_openLists;
};

QString number(double value) {
    if (qFuzzyIsNull(value))
        return QStringLiteral("0.0");
    QString result = QString::number(value, 'f', 6);
    while (result.endsWith(QLatin1Char('0')))
        result.chop(1);
    if (result.endsWith(QLatin1Char('.')))
        result.append(QLatin1Char('0'));
    return result;
}

QString boolValue(bool value) {
    return value ? QStringLiteral("true") : QStringLiteral("false");
}

QString sanitizeName(const QString& value) {
    QString result;
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

QString uuidFor(const QString& kind, const QString& name, int index = -1) {
    const QByteArray input = (QStringLiteral("EasyKiConverter/LibrePCB/2/") + kind + QLatin1Char('/') + name +
                              (index >= 0 ? QStringLiteral("/") + QString::number(index) : QString()))
                                 .toUtf8();
    // LibrePCB 2.x 只接受 RFC 4122 的随机 UUID；通过哈希生成稳定值后重写版本位。
    QByteArray digest = QCryptographicHash::hash(input, QCryptographicHash::Sha1).left(16);
    digest[6] = char((uchar(digest.at(6)) & 0x0f) | 0x40);
    digest[8] = char((uchar(digest.at(8)) & 0x3f) | 0x80);
    const auto hex = [](uchar byte) { return QStringLiteral("%1").arg(byte, 2, 16, QLatin1Char('0')); };
    QString result;
    for (int i = 0; i < digest.size(); ++i) {
        result += hex(uchar(digest.at(i)));
        if (i == 3 || i == 5 || i == 7 || i == 9)
            result += QLatin1Char('-');
    }
    return result;
}

void appendName(SExpr& root, const QString& name) {
    root.list(QStringLiteral("name")).atom(name).close();
}

void appendDescription(SExpr& root, const QString& description) {
    root.list(QStringLiteral("description")).atom(description).close();
}

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

QString layerForFootprint(IR::LayerType layer) {
    switch (layer) {
        case IR::LayerType::TopSilk:
        case IR::LayerType::TopOverlay:
            return QStringLiteral("top_legend");
        case IR::LayerType::TopAssembly:
            return QStringLiteral("top_documentation");
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

bool isAssemblyLayer(IR::LayerType layer) {
    return layer == IR::LayerType::TopAssembly || layer == IR::LayerType::BottomAssembly;
}

/**
 * @brief 检查符号引脚是否落在 LibrePCB 固定的 2.54 mm 原理图栅格上。
 * @param value 坐标值，单位为毫米。
 * @return 坐标可被 LibrePCB 原理图编辑器使用时返回 true。
 */
bool isOnLibrePcbGrid(double value) {
    constexpr double kGridMm = 2.54;
    return qAbs(value - qRound(value / kGridMm) * kGridMm) < 1e-6;
}

QString symbolLayer(IR::LayerType layer) {
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

QString componentCategoryUuid() {
    return uuidFor(QStringLiteral("component-category"), QStringLiteral("EasyKiConverter"));
}

QString packageCategoryUuid() {
    return uuidFor(QStringLiteral("package-category"), QStringLiteral("EasyKiConverter"));
}

QString padShape(IR::PadShape shape,
                 const QSizeF& size,
                 const QList<QPointF>& customPoints,
                 QStringList& diagnostics,
                 const QString& context) {
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

void appendPoint(SExpr& root, const QPointF& point) {
    root.list(QStringLiteral("position")).token(number(point.x())).token(number(point.y())).close();
}

void appendPath(SExpr& root, const QList<QPointF>& points) {
    for (int i = 0; i < points.size(); ++i) {
        SExpr& vertex = root.list(QStringLiteral("vertex"));
        vertex.list(QStringLiteral("position")).token(number(points.at(i).x())).token(number(points.at(i).y())).close();
        vertex.list(QStringLiteral("angle")).token(QStringLiteral("0")).close();
        vertex.close();
    }
}

QList<QPointF> rectanglePoints(const QRectF& bounds, double rotation) {
    const QPointF center = bounds.center();
    const double radians = qDegreesToRadians(rotation);
    const double c = std::cos(radians);
    const double s = std::sin(radians);
    QList<QPointF> result;
    for (const QPointF point : {bounds.topLeft(), bounds.topRight(), bounds.bottomRight(), bounds.bottomLeft()}) {
        const QPointF offset = point - center;
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
    constexpr int kSegments = 32;
    double sweep = endAngle - startAngle;
    if (qFuzzyIsNull(sweep))
        sweep = 360.0;
    const int segments = qMax(2, qCeil(qAbs(sweep) / 360.0 * kSegments));
    QList<QPointF> points;
    for (int i = 0; i <= segments; ++i) {
        const double angle = qDegreesToRadians(startAngle + sweep * i / segments);
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
    constexpr int kSegments = 48;
    double sweep = endAngle - startAngle;
    if (qFuzzyIsNull(sweep))
        sweep = 360.0;
    const int segments = qMax(2, qCeil(qAbs(sweep) / 360.0 * kSegments));
    QList<QPointF> points;
    for (int i = 0; i <= segments; ++i) {
        const double angle = qDegreesToRadians(startAngle + sweep * i / segments);
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
    QList<QPointF> points;
    for (int i = 0; i <= 24; ++i) {
        const double t = static_cast<double>(i) / 24.0;
        const double u = 1.0 - t;
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
    QRectF bounds;
    valid = false;
    const auto include = [&bounds, &valid](const QPointF& point) {
        if (!valid) {
            bounds = QRectF(point, QSizeF());
            valid = true;
        } else {
            const double left = qMin(bounds.left(), point.x());
            const double top = qMin(bounds.top(), point.y());
            const double right = qMax(bounds.right(), point.x());
            const double bottom = qMax(bounds.bottom(), point.y());
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
    return bounds;
}

void appendStrokeText(SExpr& root,
                      const QString& uuid,
                      const QString& layer,
                      const QString& value,
                      const QPointF& position,
                      double rotation = 0.0,
                      double height = 1.0,
                      double strokeWidth = 0.15,
                      bool mirror = false) {
    SExpr& node = root.list(QStringLiteral("stroke_text"));
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

void appendPolygon(SExpr& root,
                   const QString& uuid,
                   const QString& layer,
                   const QList<QPointF>& points,
                   double width,
                   bool filled) {
    SExpr& polygon = root.list(QStringLiteral("polygon"));
    polygon.token(uuid);
    polygon.list(QStringLiteral("layer")).token(layer).close();
    polygon.list(QStringLiteral("width")).token(number(width)).close();
    polygon.list(QStringLiteral("fill")).token(boolValue(filled)).close();
    polygon.list(QStringLiteral("grab_area")).token(QStringLiteral("false")).close();
    appendPath(polygon, points);
    polygon.close();
}

bool writeFile(const QString& path, const QByteArray& data, QStringList& diagnostics) {
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit()) {
        diagnostics.append(QStringLiteral("LibrePCB: 写入文件失败：%1").arg(path));
        return false;
    }
    return true;
}

bool writeElement(const QString& rootPath,
                  const QString& shortName,
                  const QString& longName,
                  const QString& uuid,
                  const QString& data,
                  QStringList& diagnostics) {
    const QString directory = QDir(rootPath).filePath(shortName + QLatin1Char('/') + uuid);
    if (!QDir().mkpath(directory)) {
        diagnostics.append(QStringLiteral("LibrePCB: 创建元素目录失败：%1").arg(directory));
        return false;
    }
    return writeFile(QDir(directory).filePath(QStringLiteral(".librepcb-") + shortName),
                     QByteArray::number(kLibrePcbFileFormatVersion) + '\n',
                     diagnostics) &&
           writeFile(QDir(directory).filePath(longName + QStringLiteral(".lp")), data.toUtf8(), diagnostics);
}

bool writeCategory(const QString& rootPath,
                   const QString& shortName,
                   const QString& longName,
                   const QString& rootName,
                   const QString& uuid,
                   const QString& name,
                   QStringList& diagnostics) {
    SExpr category(rootName);
    category.token(uuid);
    category.list(QStringLiteral("name")).atom(name).close();
    category.list(QStringLiteral("description")).atom(QStringLiteral("Generated by EasyKiConverter")).close();
    category.list(QStringLiteral("keywords")).atom(QString()).close();
    category.list(QStringLiteral("author")).atom(QStringLiteral("EasyKiConverter")).close();
    category.list(QStringLiteral("version")).atom(QStringLiteral("0.1")).close();
    category.list(QStringLiteral("created")).token(QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)).close();
    category.list(QStringLiteral("deprecated")).token(QStringLiteral("false")).close();
    category.list(QStringLiteral("parent")).token(QStringLiteral("none")).close();
    const QString directory = QDir(rootPath).filePath(shortName + QLatin1Char('/') + uuid);
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
    QSet<QString> designators;
    for (int i = 0; i < symbol.pins.size(); ++i) {
        const auto& pin = symbol.pins.at(i);
        const QString designator = pin.designator.trimmed();
        if (designator.isEmpty()) {
            diagnostics.append(QStringLiteral("LibrePCB: 符号 %1 存在空引脚编号").arg(symbol.name));
            return false;
        }
        if (designators.contains(designator)) {
            diagnostics.append(QStringLiteral("LibrePCB: 符号 %1 存在重复引脚编号：%2").arg(symbol.name, designator));
            return false;
        }
        if (!isOnLibrePcbGrid(pin.position.x()) || !isOnLibrePcbGrid(pin.position.y())) {
            diagnostics.append(
                QStringLiteral("LibrePCB: 符号 %1 的引脚 %2 不在 2.54 mm 栅格上").arg(symbol.name, designator));
            return false;
        }
        designators.insert(designator);
        SExpr& node = root.list(QStringLiteral("pin"));
        node.token(uuidFor(QStringLiteral("symbol-pin"), symbol.name, i));
        node.list(QStringLiteral("name")).atom(pin.name).close();
        node.list(QStringLiteral("position")).token(number(pin.position.x())).token(number(pin.position.y())).close();
        node.list(QStringLiteral("rotation"))
            .token(QString::number(pin.direction == IR::PinDirection::Left   ? 180
                                   : pin.direction == IR::PinDirection::Up   ? 90
                                   : pin.direction == IR::PinDirection::Down ? 270
                                                                             : 0))
            .close();
        node.list(QStringLiteral("length")).token(number(pin.length)).close();
        node.list(QStringLiteral("name_position"))
            .token(number(pin.hasNamePosition ? pin.namePosition.x() : pin.position.x()))
            .token(number(pin.hasNamePosition ? pin.namePosition.y() : pin.position.y()))
            .close();
        node.list(QStringLiteral("name_rotation")).token(QStringLiteral("0")).close();
        node.list(QStringLiteral("name_height"))
            .token(number(pin.nameFontSizeMm > 0 ? pin.nameFontSizeMm : 1.5))
            .close();
        node.list(QStringLiteral("name_align")).token(QStringLiteral("left")).token(QStringLiteral("center")).close();
        node.close();
    }
    int geometryIndex = 0;
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
        QList<QPointF> points = {pie.center};
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
        QList<QPointF> points = path.points;
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
        const QList<QPointF> points = sampleBezier(bezier.controlPoints);
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
        SExpr& node = root.list(QStringLiteral("circle"));
        node.token(uuidFor(QStringLiteral("symbol-circle"), symbol.name, geometryIndex++));
        node.list(QStringLiteral("layer")).token(QStringLiteral("sym_outlines")).close();
        node.list(QStringLiteral("width")).token(number(circle.strokeWidth)).close();
        node.list(QStringLiteral("fill")).token(boolValue(circle.isFilled)).close();
        node.list(QStringLiteral("grab_area")).token(QStringLiteral("false")).close();
        node.list(QStringLiteral("diameter")).token(number(circle.radius * 2.0)).close();
        node.list(QStringLiteral("position")).token(number(circle.center.x())).token(number(circle.center.y())).close();
        node.close();
    }
    for (const auto& text : symbol.texts) {
        SExpr& node = root.list(QStringLiteral("text"));
        node.token(uuidFor(QStringLiteral("symbol-text"), symbol.name, geometryIndex++));
        node.list(QStringLiteral("layer")).token(QStringLiteral("sym_values")).close();
        node.list(QStringLiteral("height")).token(number(text.fontSizeMm > 0 ? text.fontSizeMm : 1.5)).close();
        node.list(QStringLiteral("align")).token(QStringLiteral("center")).token(QStringLiteral("center")).close();
        node.list(QStringLiteral("position")).token(number(text.position.x())).token(number(text.position.y())).close();
        node.list(QStringLiteral("rotation")).token(number(text.rotation)).close();
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
        SExpr& node = root.list(QStringLiteral("text"));
        node.token(uuidFor(QStringLiteral("symbol-parameter"), symbol.name, geometryIndex++));
        node.list(QStringLiteral("layer")).token(QStringLiteral("sym_values")).close();
        node.list(QStringLiteral("height"))
            .token(number(parameter.fontSizeMm > 0 ? parameter.fontSizeMm : 1.5))
            .close();
        node.list(QStringLiteral("align")).token(QStringLiteral("center")).token(QStringLiteral("center")).close();
        node.list(QStringLiteral("position"))
            .token(number(parameter.position.x()))
            .token(number(parameter.position.y()))
            .close();
        node.list(QStringLiteral("rotation")).token(number(parameter.rotation)).close();
        node.list(QStringLiteral("lock")).token(boolValue(parameter.readOnly)).close();
        node.list(QStringLiteral("value")).atom(parameter.value).close();
        node.close();
    }
    bool hasNameText = false;
    bool hasValueText = false;
    for (const auto& text : symbol.texts) {
        hasNameText = hasNameText || text.text == QStringLiteral("{{NAME}}");
        hasValueText = hasValueText || text.text == QStringLiteral("{{VALUE}}");
    }
    // LibrePCB 将器件名称和值作为符号必需字段；源数据缺失时仍生成可编辑的默认占位文本。
    const auto appendDefaultText = [&root, &geometryIndex, &symbol](
                                       const QString& layer, const QString& value, double y) {
        SExpr& node = root.list(QStringLiteral("text"));
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
        appendDefaultText(QStringLiteral("sym_names"), QStringLiteral("{{NAME}}"), -4.0);
    if (!hasValueText)
        appendDefaultText(QStringLiteral("sym_values"), QStringLiteral("{{VALUE}}"), -5.5);
    root.close();
    return writeElement(rootPath,
                        QStringLiteral("sym"),
                        QStringLiteral("symbol"),
                        uuidFor(QStringLiteral("symbol"), symbol.name),
                        root.finish(),
                        diagnostics);
}

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
    const QString packageUuid = uuidFor(QStringLiteral("package"), footprint.name);
    SExpr root(QStringLiteral("librepcb_package"));
    appendCommon(
        root, packageUuid, sanitizeName(footprint.name), footprint.description, QStringLiteral("librepcb_package"));
    root.list(QStringLiteral("category")).token(packageCategoryUuid()).close();
    root.list(QStringLiteral("assembly_type"))
        .token(footprint.hasThroughHolePads() ? QStringLiteral("tht") : QStringLiteral("smt"))
        .close();
    root.list(QStringLiteral("grid_interval")).token(QStringLiteral("0.1")).close();
    root.list(QStringLiteral("min_copper_clearance")).token(QStringLiteral("0.2")).close();

    struct ModelFile {
        QString uuid;
        QString fileName;
        QByteArray data;
        QString displayName;
    };

    QList<ModelFile> modelFiles;
    if (export3d) {
        for (int modelIndex = 0; modelIndex < footprint.models3d.size(); ++modelIndex) {
            const IR::Model3DIR& model = footprint.models3d.at(modelIndex);
            const QString modelUuid = uuidFor(QStringLiteral("package-model"), footprint.name, modelIndex);
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
            const ModelFile& item = modelFiles.constLast();
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
    QHash<QString, QString> padUuids;
    for (int i = 0; i < footprint.pads.size(); ++i) {
        const auto& pad = footprint.pads.at(i);
        const QString padNumber = pad.number.trimmed();
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
        const QString uuid = uuidFor(QStringLiteral("package-pad"), footprint.name, i);
        padUuids.insert(padNumber, uuid);
        SExpr& node = root.list(QStringLiteral("pad"));
        node.token(uuid);
        node.list(QStringLiteral("name")).atom(padNumber).close();
        node.close();
    }
    const QString fpUuid = uuidFor(QStringLiteral("footprint"), footprint.name);
    SExpr& fp = root.list(QStringLiteral("footprint"));
    fp.token(fpUuid);
    fp.list(QStringLiteral("name")).atom(sanitizeName(footprint.name)).close();
    fp.list(QStringLiteral("description")).atom(footprint.description).close();
    const IR::Model3DVec3 modelPosition =
        footprint.models3d.isEmpty() ? IR::Model3DVec3() : footprint.models3d.first().translation();
    const IR::Model3DVec3 modelOffset =
        footprint.models3d.isEmpty() ? IR::Model3DVec3() : footprint.models3d.first().stepOffsetMm();
    const IR::Model3DVec3 modelRotation =
        footprint.models3d.isEmpty() ? IR::Model3DVec3() : footprint.models3d.first().rotation();
    fp.list(QStringLiteral("3d_position"))
        .token(number(modelPosition.x + modelOffset.x))
        .token(number(modelPosition.y + modelOffset.y))
        .token(number(modelPosition.z + modelOffset.z))
        .close();
    fp.list(QStringLiteral("3d_rotation"))
        .token(number(modelRotation.x))
        .token(number(modelRotation.y))
        .token(number(modelRotation.z))
        .close();
    for (const ModelFile& model : modelFiles)
        fp.list(QStringLiteral("3d_model")).token(model.uuid).close();
    int index = 0;
    for (const auto& pad : footprint.pads) {
        const QString padUuid = uuidFor(QStringLiteral("footprint-pad"), footprint.name, index);
        SExpr& node = fp.list(QStringLiteral("pad"));
        node.token(padUuid);
        node.list(QStringLiteral("side"))
            // LibrePCB 的 through-hole 属性由 hole 节点表达，side 仍只能是 top 或 bottom。
            .token(pad.layer == IR::LayerType::BottomCopper ? QStringLiteral("bottom") : QStringLiteral("top"))
            .close();
        const QString shape = padShape(
            pad.shape, pad.size, pad.customShapePoints, diagnostics, footprint.name + QStringLiteral("/") + pad.number);
        if (shape.isEmpty())
            return false;
        node.list(QStringLiteral("shape")).token(shape).close();
        node.list(QStringLiteral("position")).token(number(pad.position.x())).token(number(pad.position.y())).close();
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
            SExpr& hole = node.list(QStringLiteral("hole"));
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
        const QString layer = layerForFootprint(rectangle.layer);
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
                      rectanglePoints(rectangle.bounds, rectangle.rotation),
                      rectangle.strokeWidth,
                      false);
    }
    for (const auto& outline : footprint.outlines) {
        const QString layer = layerForFootprint(outline.layer);
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
                      outline.points,
                      outline.strokeWidth,
                      false);
    }
    for (const auto& circle : footprint.circles) {
        const QString layer = layerForFootprint(circle.layer);
        if (layer.isEmpty()) {
            diagnostics.append(QStringLiteral("LibrePCB: 圆形图元使用无法映射的图层"));
            return false;
        }
        if (isAssemblyLayer(circle.layer))
            diagnostics.append(QStringLiteral("LibrePCB: 装配层圆形已映射到 Documentation 层"));
        SExpr& node = fp.list(QStringLiteral("circle"));
        node.token(uuidFor(QStringLiteral("footprint-circle"), footprint.name, index++));
        node.list(QStringLiteral("layer")).token(layer).close();
        node.list(QStringLiteral("width")).token(number(circle.strokeWidth)).close();
        node.list(QStringLiteral("fill")).token(QStringLiteral("false")).close();
        node.list(QStringLiteral("grab_area")).token(QStringLiteral("false")).close();
        node.list(QStringLiteral("diameter")).token(number(circle.radius * 2)).close();
        node.list(QStringLiteral("position")).token(number(circle.center.x())).token(number(circle.center.y())).close();
        node.close();
    }
    for (int trackIndex = 0; trackIndex < footprint.tracks.size(); ++trackIndex) {
        const auto& track = footprint.tracks.at(trackIndex);
        const QString layer = layerForFootprint(track.layer);
        if (layer.isEmpty() || track.points.size() < 2) {
            diagnostics.append(QStringLiteral("LibrePCB: 封装走线无法映射或顶点不足：%1").arg(footprint.name));
            return false;
        }
        appendPolygon(fp,
                      uuidFor(QStringLiteral("footprint-track"), footprint.name, trackIndex),
                      layer,
                      track.points,
                      track.width,
                      false);
        if (!track.netName.isEmpty())
            diagnostics.append(QStringLiteral("LibrePCB: 封装走线网络名称未写入库图元：%1").arg(track.netName));
        if (track.isLocked)
            diagnostics.append(QStringLiteral("LibrePCB: 封装走线锁定属性不会写入库定义：%1").arg(footprint.name));
    }
    for (int arcIndex = 0; arcIndex < footprint.arcs.size(); ++arcIndex) {
        const auto& arc = footprint.arcs.at(arcIndex);
        const QString layer = layerForFootprint(arc.layer);
        if (layer.isEmpty() || arc.radius <= 0) {
            diagnostics.append(QStringLiteral("LibrePCB: 封装圆弧无法映射：%1").arg(footprint.name));
            return false;
        }
        appendPolygon(fp,
                      uuidFor(QStringLiteral("footprint-arc"), footprint.name, arcIndex),
                      layer,
                      sampleArc(arc.center, arc.radius, arc.startAngle, arc.endAngle),
                      arc.width,
                      false);
        diagnostics.append(QStringLiteral("LibrePCB: 封装 %1 的圆弧已离散为折线").arg(footprint.name));
        if (!arc.netName.isEmpty())
            diagnostics.append(QStringLiteral("LibrePCB: 封装圆弧网络名称不会写入库图元：%1").arg(arc.netName));
        if (arc.isLocked)
            diagnostics.append(QStringLiteral("LibrePCB: 封装圆弧锁定属性不会写入库定义：%1").arg(footprint.name));
    }
    for (int regionIndex = 0; regionIndex < footprint.regions.size(); ++regionIndex) {
        const auto& region = footprint.regions.at(regionIndex);
        if (region.isKeepOut) {
            diagnostics.append(
                QStringLiteral("LibrePCB: 封装 %1 的 KeepOut 区域无法由 LibrePCB Package 表达").arg(footprint.name));
            return false;
        }
        const QString layer = layerForFootprint(region.layer);
        if (layer.isEmpty() || region.vertices.size() < 3) {
            diagnostics.append(QStringLiteral("LibrePCB: 封装区域无法映射或顶点不足：%1").arg(footprint.name));
            return false;
        }
        appendPolygon(fp,
                      uuidFor(QStringLiteral("footprint-region"), footprint.name, regionIndex),
                      layer,
                      region.vertices,
                      0,
                      true);
        if (region.isLocked)
            diagnostics.append(QStringLiteral("LibrePCB: 封装区域锁定属性不会写入库定义：%1").arg(footprint.name));
    }
    for (int holeIndex = 0; holeIndex < footprint.holes.size(); ++holeIndex) {
        const auto& hole = footprint.holes.at(holeIndex);
        if (hole.radius <= 0) {
            diagnostics.append(QStringLiteral("LibrePCB: 封装 %1 存在非法安装孔尺寸").arg(footprint.name));
            return false;
        }
        SExpr& node = fp.list(QStringLiteral("hole"));
        node.token(uuidFor(QStringLiteral("footprint-hole"), footprint.name, holeIndex));
        node.list(QStringLiteral("diameter")).token(number(hole.radius * 2.0)).close();
        node.list(QStringLiteral("position")).token(number(hole.center.x())).token(number(hole.center.y())).close();
        node.close();
        if (hole.isLocked)
            diagnostics.append(QStringLiteral("LibrePCB: 封装安装孔锁定属性不会写入库定义：%1").arg(footprint.name));
    }
    for (int textIndex = 0; textIndex < footprint.texts.size(); ++textIndex) {
        const auto& text = footprint.texts.at(textIndex);
        if (!text.isDisplayed)
            continue;
        const QString layer = layerForFootprint(text.layer);
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
                         text.position,
                         text.rotation,
                         text.fontSize,
                         text.strokeWidth,
                         text.mirror);
    }
    bool hasPackageOutline = false;
    bool hasCourtyard = false;
    bool hasNameText = false;
    bool hasValueText = false;
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
    bool boundsValid = false;
    const QRectF bounds = footprintBounds(footprint, boundsValid);
    if (boundsValid && !hasPackageOutline) {
        appendPolygon(fp,
                      uuidFor(QStringLiteral("fallback-outline"), footprint.name),
                      QStringLiteral("top_package_outlines"),
                      rectanglePoints(bounds, 0),
                      0.1,
                      false);
        diagnostics.append(QStringLiteral("LibrePCB: 封装 %1 缺少可靠外形，已根据 IR 图元生成 Package Outlines 回退")
                               .arg(footprint.name));
    }
    if (boundsValid && !hasCourtyard) {
        appendPolygon(fp,
                      uuidFor(QStringLiteral("fallback-courtyard"), footprint.name),
                      QStringLiteral("top_courtyard"),
                      rectanglePoints(bounds.adjusted(-0.25, -0.25, 0.25, 0.25), 0),
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
                         QPointF(bounds.center().x(), bounds.bottom() + 1.0));
    if (boundsValid && !hasValueText)
        appendStrokeText(fp,
                         uuidFor(QStringLiteral("fallback-value"), footprint.name),
                         QStringLiteral("top_values"),
                         QStringLiteral("{{VALUE}}"),
                         QPointF(bounds.center().x(), bounds.top() - 1.0));
    fp.close();
    root.close();
    if (!writeElement(
            rootPath, QStringLiteral("pkg"), QStringLiteral("package"), packageUuid, root.finish(), diagnostics))
        return false;
    const QString packageDirectory = QDir(rootPath).filePath(QStringLiteral("pkg/") + packageUuid);
    for (const ModelFile& model : modelFiles) {
        if (!writeFile(QDir(packageDirectory).filePath(model.fileName), model.data, diagnostics))
            return false;
    }
    return true;
}

bool writeComponentAndDevice(const IR::ComponentIR& component, const QString& rootPath, QStringList& diagnostics) {
    if (!component.hasSymbol() || !component.hasFootprint()) {
        diagnostics.append(QStringLiteral("LibrePCB: 组件 %1 缺少符号或封装，无法建立完整关联").arg(component.name));
        return false;
    }
    if (!component.datasheet.isEmpty())
        diagnostics.append(QStringLiteral("LibrePCB: 组件数据手册地址未写入器件属性：%1").arg(component.datasheet));
    const QString symbolUuid = uuidFor(QStringLiteral("symbol"), component.symbol.name);
    const QString packageUuid = uuidFor(QStringLiteral("package"), component.footprint.name);
    const QString componentUuid = uuidFor(QStringLiteral("component"), component.name);
    const QString deviceUuid = uuidFor(QStringLiteral("device"), component.name);
    SExpr cmp(QStringLiteral("librepcb_component"));
    appendCommon(
        cmp, componentUuid, sanitizeName(component.name), component.description, QStringLiteral("librepcb_component"));
    cmp.list(QStringLiteral("category")).token(componentCategoryUuid()).close();
    cmp.list(QStringLiteral("schematic_only")).token(QStringLiteral("false")).close();
    cmp.list(QStringLiteral("default_value")).atom(component.name).close();
    cmp.list(QStringLiteral("prefix"))
        .atom(component.prefix.isEmpty() ? QStringLiteral("U") : component.prefix)
        .close();
    int signalIndex = 0;
    QHash<QString, QString> signalUuids;
    for (const auto& pin : component.symbol.pins) {
        const QString designator = pin.designator.trimmed();
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
        const QString signalUuid = uuidFor(QStringLiteral("signal"), component.name, signalIndex);
        signalUuids.insert(designator, signalUuid);
        SExpr& signal = cmp.list(QStringLiteral("signal"));
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
    const QString variantUuid = uuidFor(QStringLiteral("variant"), component.name);
    const QString itemUuid = uuidFor(QStringLiteral("variant-item"), component.name);
    SExpr& variant = cmp.list(QStringLiteral("variant"));
    variant.token(variantUuid).list(QStringLiteral("norm")).atom(QString()).close();
    appendName(variant, sanitizeName(component.name));
    appendDescription(variant, component.description);
    SExpr& item = variant.list(QStringLiteral("gate"));
    item.token(itemUuid).list(QStringLiteral("symbol")).token(symbolUuid).close();
    item.list(QStringLiteral("position")).token(QStringLiteral("0")).token(QStringLiteral("0")).close();
    item.list(QStringLiteral("rotation")).token(QStringLiteral("0")).close();
    item.list(QStringLiteral("required")).token(QStringLiteral("true")).close();
    item.list(QStringLiteral("suffix")).atom(QString()).close();
    for (int i = 0; i < component.symbol.pins.size(); ++i) {
        const auto& pin = component.symbol.pins.at(i);
        const QString signalUuid = signalUuids.value(pin.designator);
        SExpr& map = item.list(QStringLiteral("pin"));
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

    SExpr dev(QStringLiteral("librepcb_device"));
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
        const auto& pad = component.footprint.pads.at(i);
        SExpr& map = dev.list(QStringLiteral("pad"));
        map.token(uuidFor(QStringLiteral("package-pad"), component.footprint.name, i));
        map.list(QStringLiteral("optional")).token(QStringLiteral("false")).close();
        const QString padNumber = pad.number.trimmed();
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

QString ExporterLibrePcbLibrary::libraryFileExtension() const {
    return QStringLiteral(".lplib");
}

bool ExporterLibrePcbLibrary::isDirectoryOutput() const {
    return true;
}

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

bool ExporterLibrePcbLibrary::exportSymbolLibrary(const QList<IR::SymbolComponentIR>& symbols,
                                                  const QString& libName,
                                                  const QString& filePath) {
    return exportSymbolLibrary(symbols, libName, filePath, false, false, QString());
}

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

bool ExporterLibrePcbLibrary::exportSymbol(const IR::SymbolComponentIR& symbol, const QString& filePath) {
    return exportSymbolLibrary({symbol}, symbol.name, filePath);
}

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
    QSet<QString> symbolNames;
    QSet<QString> packageNames;
    QSet<QString> componentNames;
    for (const auto& component : components) {
        const QString symbolName = sanitizeName(component.symbol.name);
        const QString packageName = sanitizeName(component.footprint.name);
        const QString componentName = sanitizeName(component.name);
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

QStringList ExporterLibrePcbLibrary::diagnostics() const {
    return m_diagnostics;
}

}  // namespace EasyKiConverter
