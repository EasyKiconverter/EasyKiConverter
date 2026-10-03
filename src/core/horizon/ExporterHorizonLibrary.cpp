#include "ExporterHorizonLibrary.h"

#include "HorizonUnits.h"
#include "HorizonUuid.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSet>
#include <QTransform>
#include <QtMath>

#include <algorithm>

namespace EasyKiConverter {
namespace {

constexpr int kTopCopper = 0;
constexpr int kInnerCopper1 = -1;
constexpr int kInnerCopper2 = -2;
constexpr int kInnerCopper3 = -3;
constexpr int kInnerCopper4 = -4;
constexpr int kBottomCopper = -100;
constexpr int kTopMask = 10;
constexpr int kBottomMask = -110;
constexpr int kTopSilk = 20;
constexpr int kBottomSilk = -120;
constexpr int kTopPaste = 30;
constexpr int kBottomPaste = -130;
constexpr int kTopPackage = 40;
constexpr int kBottomPackage = -140;
constexpr int kTopAssembly = 50;
constexpr int kBottomAssembly = -150;
constexpr int kOutline = 100;

QString safeName(QString value) {
    value = value.trimmed();
    QString result;
    for (const QChar c : value) {
        if (c.isLetterOrNumber() || c == QLatin1Char('_') || c == QLatin1Char('-') || c == QLatin1Char('.') ||
            c == QLatin1Char(' '))
            result.append(c);
        else
            result.append(QLatin1Char('_'));
    }
    result = result.simplified();
    return result.isEmpty() ? QStringLiteral("unnamed") : result.left(120);
}

bool isMechanicalHolePad(const IR::FootprintPadIR& pad) {
    return pad.isThroughHole() && !pad.isPlated;
}

QString padFileStem(const IR::FootprintPadIR& pad, int index) {
    if (isMechanicalHolePad(pad) && pad.number.trimmed().isEmpty())
        return QStringLiteral("NPTH%1").arg(index + 1);
    return safeName(pad.number);
}

bool validateSanitizedName(QHash<QString, QString>& owners,
                           const QString& sanitizedName,
                           const QString& rawName,
                           const QString& kind,
                           QStringList& diagnostics) {
    const auto existing = owners.constFind(sanitizedName);
    if (existing != owners.cend() && existing.value() != rawName) {
        diagnostics.append(QStringLiteral("Horizon: %1 名称清洗后输出路径冲突：%2（%3 与 %4）")
                               .arg(kind, sanitizedName, existing.value(), rawName));
        return false;
    }
    owners.insert(sanitizedName, rawName);
    return true;
}

bool validateSafeName(QHash<QString, QString>& owners,
                      const QString& rawName,
                      const QString& kind,
                      QStringList& diagnostics) {
    return validateSanitizedName(owners, safeName(rawName), rawName, kind, diagnostics);
}

QJsonArray point(double x, double y) {
    return {HorizonUnits::mm(x), HorizonUnits::mm(y)};
}

QJsonArray point(const QPointF& p) {
    return point(p.x(), p.y());
}

QJsonObject placement(const QPointF& p = {}, double angle = 0.0, bool mirror = false) {
    QJsonObject result;
    result.insert(QStringLiteral("shift"), point(p));
    result.insert(QStringLiteral("angle"), qRound64(angle * 65536.0 / 360.0));
    result.insert(QStringLiteral("mirror"), mirror);
    return result;
}

QJsonArray variant(const QString& value) {
    return QJsonArray{false, value};
}

QString pinDirection(IR::PinElectricalType type, QStringList& diagnostics, const QString& context) {
    switch (type) {
        case IR::PinElectricalType::Input:
            return QStringLiteral("input");
        case IR::PinElectricalType::Output:
            return QStringLiteral("output");
        case IR::PinElectricalType::Bidirectional:
            return QStringLiteral("bidirectional");
        case IR::PinElectricalType::Passive:
            return QStringLiteral("passive");
        case IR::PinElectricalType::OpenCollector:
            return QStringLiteral("open_collector");
        case IR::PinElectricalType::Power:
            diagnostics.append(QStringLiteral("Horizon: Power 引脚降级为 power_input：%1").arg(context));
            return QStringLiteral("power_input");
        case IR::PinElectricalType::OpenEmitter:
            return QStringLiteral("open_emitter");
        case IR::PinElectricalType::Unspecified:
            diagnostics.append(QStringLiteral("Horizon: 未直接表达的引脚电气类型降级为 passive：%1").arg(context));
            return QStringLiteral("passive");
    }
    diagnostics.append(QStringLiteral("Horizon: 未知引脚电气类型：%1").arg(context));
    return QStringLiteral("passive");
}

QString symbolOrientation(IR::PinDirection direction) {
    switch (direction) {
        case IR::PinDirection::Left:
            return QStringLiteral("left");
        case IR::PinDirection::Up:
            return QStringLiteral("up");
        case IR::PinDirection::Down:
            return QStringLiteral("down");
        case IR::PinDirection::Right:
        default:
            return QStringLiteral("right");
    }
}

int packageLayer(IR::LayerType layer, QStringList& diagnostics, const QString& context) {
    switch (layer) {
        case IR::LayerType::TopCopper:
        case IR::LayerType::MultiLayer:
            return kTopCopper;
        case IR::LayerType::BottomCopper:
            return kBottomCopper;
        case IR::LayerType::InnerCopper1:
            return kInnerCopper1;
        case IR::LayerType::InnerCopper2:
            return kInnerCopper2;
        case IR::LayerType::InnerCopper3:
            return kInnerCopper3;
        case IR::LayerType::InnerCopper4:
            return kInnerCopper4;
        case IR::LayerType::TopSilk:
        case IR::LayerType::TopOverlay:
            return kTopSilk;
        case IR::LayerType::BottomSilk:
        case IR::LayerType::BottomOverlay:
            return kBottomSilk;
        case IR::LayerType::TopPaste:
            return kTopPaste;
        case IR::LayerType::BottomPaste:
            return kBottomPaste;
        case IR::LayerType::TopMask:
            return kTopMask;
        case IR::LayerType::BottomMask:
            return kBottomMask;
        case IR::LayerType::TopAssembly:
            return kTopAssembly;
        case IR::LayerType::BottomAssembly:
            return kBottomAssembly;
        case IR::LayerType::KeepOut:
            diagnostics.append(
                QStringLiteral("Horizon: KeepOut 必须通过原生 keepouts 对象输出，不能作为普通图层：%1").arg(context));
            return kTopCopper;
        case IR::LayerType::EdgeCuts:
            return kOutline;
        default:
            diagnostics.append(QStringLiteral("Horizon: 不支持的层降级为 top package：%1").arg(context));
            return kTopPackage;
    }
}

QJsonObject emptyMaps() {
    return QJsonObject{{QStringLiteral("junctions"), QJsonObject{}},
                       {QStringLiteral("lines"), QJsonObject{}},
                       {QStringLiteral("arcs"), QJsonObject{}},
                       {QStringLiteral("polygons"), QJsonObject{}},
                       {QStringLiteral("texts"), QJsonObject{}}};
}

bool writeJson(const QString& path, const QJsonObject& object, QStringList& diagnostics) {
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        diagnostics.append(QStringLiteral("Horizon: 无法写入文件：%1").arg(path));
        return false;
    }
    const QByteArray data = QJsonDocument(object).toJson(QJsonDocument::Indented);
    if (file.write(data) != data.size() || !file.commit()) {
        diagnostics.append(QStringLiteral("Horizon: 文件提交失败：%1").arg(path));
        return false;
    }
    return true;
}

bool writeBinary(const QString& path, const QByteArray& data, QStringList& diagnostics) {
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit()) {
        diagnostics.append(QStringLiteral("Horizon: 无法提交 3D 模型文件：%1").arg(path));
        return false;
    }
    return true;
}

bool ensureDirs(const QString& root, QStringList& diagnostics) {
    const QStringList names = {QStringLiteral("units"),
                               QStringLiteral("symbols"),
                               QStringLiteral("entities"),
                               QStringLiteral("padstacks"),
                               QStringLiteral("packages"),
                               QStringLiteral("parts"),
                               QStringLiteral("3d_models")};
    for (const QString& name : names) {
        if (!QDir().mkpath(QDir(root).filePath(name))) {
            diagnostics.append(QStringLiteral("Horizon: 无法创建 Pool 目录：%1").arg(QDir(root).filePath(name)));
            return false;
        }
    }
    return true;
}

void addLine(QJsonObject& lines,
             QJsonObject& junctions,
             const QString& semantic,
             const QPointF& from,
             const QPointF& to,
             int layer,
             double width) {
    const QString fromUuid = HorizonUuid::make(QStringLiteral("junction"), semantic + QStringLiteral("/from"));
    const QString toUuid = HorizonUuid::make(QStringLiteral("junction"), semantic + QStringLiteral("/to"));
    const QString lineUuid = HorizonUuid::make(QStringLiteral("line"), semantic);
    junctions.insert(fromUuid, QJsonObject{{QStringLiteral("position"), point(from)}});
    junctions.insert(toUuid, QJsonObject{{QStringLiteral("position"), point(to)}});
    lines.insert(lineUuid,
                 QJsonObject{{QStringLiteral("from"), fromUuid},
                             {QStringLiteral("to"), toUuid},
                             {QStringLiteral("width"), HorizonUnits::mm(width)},
                             {QStringLiteral("layer"), layer}});
}

void addArc(QJsonObject& arcs,
            QJsonObject& junctions,
            const QString& semantic,
            const QPointF& from,
            const QPointF& to,
            const QPointF& center,
            int layer,
            double width) {
    const QString fromUuid = HorizonUuid::make(QStringLiteral("junction"), semantic + QStringLiteral("/from"));
    const QString toUuid = HorizonUuid::make(QStringLiteral("junction"), semantic + QStringLiteral("/to"));
    const QString centerUuid = HorizonUuid::make(QStringLiteral("junction"), semantic + QStringLiteral("/center"));
    const QString arcUuid = HorizonUuid::make(QStringLiteral("arc"), semantic);
    junctions.insert(fromUuid, QJsonObject{{QStringLiteral("position"), point(from)}});
    junctions.insert(toUuid, QJsonObject{{QStringLiteral("position"), point(to)}});
    junctions.insert(centerUuid, QJsonObject{{QStringLiteral("position"), point(center)}});
    arcs.insert(arcUuid,
                QJsonObject{{QStringLiteral("from"), fromUuid},
                            {QStringLiteral("to"), toUuid},
                            {QStringLiteral("center"), centerUuid},
                            {QStringLiteral("width"), HorizonUnits::mm(width)},
                            {QStringLiteral("layer"), layer}});
}

void addCircle(QJsonObject& arcs,
               QJsonObject& junctions,
               const QString& semantic,
               const QPointF& center,
               double radius,
               int layer,
               double width) {
    const QList<QPointF> points = {center + QPointF(radius, 0.0),
                                   center + QPointF(0.0, radius),
                                   center + QPointF(-radius, 0.0),
                                   center + QPointF(0.0, -radius)};
    for (int i = 0; i < points.size(); ++i) {
        addArc(arcs,
               junctions,
               semantic + QStringLiteral("/") + QString::number(i),
               points.at(i),
               points.at((i + 1) % points.size()),
               center,
               layer,
               width);
    }
}

void addPolygon(QJsonObject& polygons, const QString& semantic, const QList<QPointF>& points, int layer) {
    QJsonArray vertices;
    for (const QPointF& p : points)
        vertices.append(QJsonObject{{QStringLiteral("type"), QStringLiteral("line")},
                                    {QStringLiteral("position"), point(p)},
                                    {QStringLiteral("arc_center"), point(0.0, 0.0)},
                                    {QStringLiteral("arc_reverse"), false}});
    polygons.insert(HorizonUuid::make(QStringLiteral("polygon"), semantic),
                    QJsonObject{{QStringLiteral("vertices"), vertices}, {QStringLiteral("layer"), layer}});
}

QList<QPointF> ellipsePoints(const QPointF& center,
                             double radiusX,
                             double radiusY,
                             double startAngle,
                             double endAngle,
                             int segments = 32) {
    QList<QPointF> result;
    const double delta = endAngle - startAngle;
    const int count = qMax(2, qCeil(qAbs(delta) / 360.0 * segments));
    for (int i = 0; i <= count; ++i) {
        const double angle = qDegreesToRadians(startAngle + delta * i / count);
        result.append(center + QPointF(radiusX * qCos(angle), radiusY * qSin(angle)));
    }
    return result;
}

QList<QPointF> cubicBezierPoints(const QList<QPointF>& controlPoints, int segments = 16) {
    QList<QPointF> result;
    if (controlPoints.size() < 4)
        return result;
    const QPointF& p0 = controlPoints.at(0);
    const QPointF& p1 = controlPoints.at(1);
    const QPointF& p2 = controlPoints.at(2);
    const QPointF& p3 = controlPoints.at(3);
    for (int i = 0; i <= segments; ++i) {
        const double t = static_cast<double>(i) / segments;
        const double u = 1.0 - t;
        result.append(u * u * u * p0 + 3.0 * u * u * t * p1 + 3.0 * u * t * t * p2 + t * t * t * p3);
    }
    return result;
}

void addPolyline(QJsonObject& lines,
                 QJsonObject& junctions,
                 const QString& semantic,
                 const QList<QPointF>& points,
                 int layer,
                 double width) {
    for (int i = 1; i < points.size(); ++i)
        addLine(lines,
                junctions,
                semantic + QStringLiteral("/") + QString::number(i),
                points.at(i - 1),
                points.at(i),
                layer,
                width);
}

QPointF rotateAround(const QPointF& point, const QPointF& center, double angle) {
    const QTransform transform =
        QTransform().translate(center.x(), center.y()).rotate(angle).translate(-center.x(), -center.y());
    return transform.map(point);
}

QJsonObject unitJson(const IR::SymbolComponentIR& symbol,
                     const QString& uuidBase,
                     int part,
                     const QString& unitUuid,
                     QStringList& diagnostics) {
    QJsonObject pins;
    for (int i = 0; i < symbol.pins.size(); ++i) {
        const IR::SymbolPinIR& pin = symbol.pins.at(i);
        if (pin.designator.trimmed().isEmpty()) {
            diagnostics.append(
                QStringLiteral("Horizon: 符号 %1 Part %2 存在空引脚编号").arg(symbol.name).arg(part + 1));
            continue;
        }
        if (!pin.commonToAllParts && pin.partIndex != part)
            continue;
        const QString pinUuid = HorizonUuid::make(QStringLiteral("unit-pin"),
                                                  uuidBase + QStringLiteral("/part/") + QString::number(part) +
                                                      QStringLiteral("/designator/") + pin.designator);
        QJsonObject value{
            {QStringLiteral("primary_name"), pin.name.trimmed().isEmpty() ? pin.designator : pin.name},
            {QStringLiteral("direction"),
             pinDirection(pin.electricalType, diagnostics, symbol.name + QStringLiteral("/") + pin.designator)},
            {QStringLiteral("swap_group"), 0},
            {QStringLiteral("names"), QJsonArray{}}};
        pins.insert(pinUuid, value);
    }
    return QJsonObject{{QStringLiteral("type"), QStringLiteral("unit")},
                       {QStringLiteral("name"),
                        symbol.name + (symbol.partCount > 1 ? QStringLiteral("_Part%1").arg(part + 1) : QString())},
                       {QStringLiteral("manufacturer"), QString()},
                       {QStringLiteral("uuid"), unitUuid},
                       {QStringLiteral("pins"), pins}};
}

QJsonObject symbolJson(const IR::SymbolComponentIR& symbol,
                       const QString& uuidBase,
                       int part,
                       const QString& unitUuid,
                       QStringList& diagnostics) {
    QJsonObject result = emptyMaps();
    result.insert(QStringLiteral("type"), QStringLiteral("symbol"));
    result.insert(QStringLiteral("name"),
                  symbol.name + (symbol.partCount > 1 ? QStringLiteral("_Part%1").arg(part + 1) : QString()));
    const QString symbolUuid =
        HorizonUuid::make(QStringLiteral("symbol"), symbol.name + QStringLiteral("/part/") + QString::number(part));
    result.insert(QStringLiteral("uuid"), symbolUuid);
    result.insert(QStringLiteral("unit"), unitUuid);
    result.insert(QStringLiteral("can_expand"), false);

    QJsonObject junctions = result[QStringLiteral("junctions")].toObject();
    QJsonObject lines = result[QStringLiteral("lines")].toObject();
    QJsonObject pins = result[QStringLiteral("pins")].toObject();
    QJsonObject polygons = result[QStringLiteral("polygons")].toObject();
    QJsonObject texts = result[QStringLiteral("texts")].toObject();

    const auto warnUnsupported = [&](const QString& primitive, int count) {
        if (count > 0)
            diagnostics.append(QStringLiteral("Horizon: 符号 %1 Part %2 的 %3 个 %4 图元暂不支持，已跳过")
                                   .arg(symbol.name)
                                   .arg(part + 1)
                                   .arg(count)
                                   .arg(primitive));
    };
    const auto warnApproximation = [&](const QString& primitive, int count) {
        if (count > 0)
            diagnostics.append(QStringLiteral("Horizon: 符号 %1 Part %2 的 %3 个 %4 图元使用折线/多边形近似")
                                   .arg(symbol.name)
                                   .arg(part + 1)
                                   .arg(count)
                                   .arg(primitive));
    };
    warnApproximation(QStringLiteral("circle"),
                      std::count_if(symbol.circles.cbegin(), symbol.circles.cend(), [part](const auto& item) {
                          return item.partIndex == part;
                      }));
    warnApproximation(QStringLiteral("arc"),
                      std::count_if(symbol.arcs.cbegin(), symbol.arcs.cend(), [part](const auto& item) {
                          return item.partIndex == part;
                      }));
    warnApproximation(QStringLiteral("ellipse"),
                      std::count_if(symbol.ellipses.cbegin(), symbol.ellipses.cend(), [part](const auto& item) {
                          return item.partIndex == part;
                      }));
    warnApproximation(QStringLiteral("pie"),
                      std::count_if(symbol.pies.cbegin(), symbol.pies.cend(), [part](const auto& item) {
                          return item.partIndex == part;
                      }));
    warnApproximation(QStringLiteral("elliptical arc"),
                      std::count_if(symbol.ellipticalArcs.cbegin(),
                                    symbol.ellipticalArcs.cend(),
                                    [part](const auto& item) { return item.partIndex == part; }));
    warnApproximation(QStringLiteral("path"),
                      std::count_if(symbol.paths.cbegin(), symbol.paths.cend(), [part](const auto& item) {
                          return item.partIndex == part;
                      }));
    warnApproximation(QStringLiteral("Bezier"),
                      std::count_if(symbol.beziers.cbegin(), symbol.beziers.cend(), [part](const auto& item) {
                          return item.partIndex == part;
                      }));
    warnApproximation(QStringLiteral("text frame"),
                      std::count_if(symbol.textFrames.cbegin(), symbol.textFrames.cend(), [part](const auto& item) {
                          return item.partIndex == part;
                      }));
    warnUnsupported(QStringLiteral("IEEE"),
                    std::count_if(symbol.ieeeSymbols.cbegin(), symbol.ieeeSymbols.cend(), [part](const auto& item) {
                        return item.partIndex == part;
                    }));
    warnUnsupported(QStringLiteral("text frame"),
                    std::count_if(symbol.textFrames.cbegin(), symbol.textFrames.cend(), [part](const auto& item) {
                        return item.partIndex == part;
                    }));
    warnUnsupported(QStringLiteral("image"),
                    std::count_if(symbol.images.cbegin(), symbol.images.cend(), [part](const auto& item) {
                        return item.partIndex == part;
                    }));

    for (int i = 0; i < symbol.pins.size(); ++i) {
        const IR::SymbolPinIR& pin = symbol.pins.at(i);
        if (!pin.commonToAllParts && pin.partIndex != part)
            continue;
        const QString pinUuid = HorizonUuid::make(QStringLiteral("unit-pin"),
                                                  uuidBase + QStringLiteral("/part/") + QString::number(part) +
                                                      QStringLiteral("/designator/") + pin.designator);
        QJsonObject value{{QStringLiteral("position"), point(pin.position)},
                          {QStringLiteral("length"), HorizonUnits::mm(pin.length)},
                          {QStringLiteral("orientation"), symbolOrientation(pin.direction)},
                          {QStringLiteral("name_visible"), pin.display.showName && pin.showName},
                          {QStringLiteral("pad_visible"), pin.display.showDesignator && pin.showDesignator}};
        QString driver = QStringLiteral("default");
        if (pin.electricalType == IR::PinElectricalType::OpenCollector ||
            pin.style.decoration == IR::PinDecoration::OpenCollector ||
            pin.style.decoration == IR::PinDecoration::OpenCollectorPullUp)
            driver = pin.style.decoration == IR::PinDecoration::OpenCollectorPullUp
                         ? QStringLiteral("open_collector_pullup")
                         : QStringLiteral("open_collector");
        else if (pin.electricalType == IR::PinElectricalType::OpenEmitter ||
                 pin.style.decoration == IR::PinDecoration::OpenEmitter ||
                 pin.style.decoration == IR::PinDecoration::OpenEmitterPullUp)
            driver = pin.style.decoration == IR::PinDecoration::OpenEmitterPullUp
                         ? QStringLiteral("open_emitter_pulldown")
                         : QStringLiteral("open_emitter");
        QJsonObject decoration{{QStringLiteral("dot"), pin.style.inverted || pin.hasDot},
                               {QStringLiteral("clock"), pin.style.clock || pin.hasClock},
                               {QStringLiteral("schmitt"), pin.style.decoration == IR::PinDecoration::Schmitt},
                               {QStringLiteral("driver"), driver}};
        value.insert(QStringLiteral("decoration"), decoration);
        pins.insert(pinUuid, value);
    }

    auto addRectangle = [&](const QString& key, double x0, double y0, double x1, double y1, int index) {
        addLine(lines,
                junctions,
                symbol.name + QStringLiteral("/part/") + QString::number(part) + QStringLiteral("/") + key +
                    QString::number(index) + QStringLiteral("/a"),
                QPointF(x0, y0),
                QPointF(x1, y0),
                0,
                0);
        addLine(lines,
                junctions,
                symbol.name + QStringLiteral("/part/") + QString::number(part) + QStringLiteral("/") + key +
                    QString::number(index) + QStringLiteral("/b"),
                QPointF(x1, y0),
                QPointF(x1, y1),
                0,
                0);
        addLine(lines,
                junctions,
                symbol.name + QStringLiteral("/part/") + QString::number(part) + QStringLiteral("/") + key +
                    QString::number(index) + QStringLiteral("/c"),
                QPointF(x1, y1),
                QPointF(x0, y1),
                0,
                0);
        addLine(lines,
                junctions,
                symbol.name + QStringLiteral("/part/") + QString::number(part) + QStringLiteral("/") + key +
                    QString::number(index) + QStringLiteral("/d"),
                QPointF(x0, y1),
                QPointF(x0, y0),
                0,
                0);
    };
    for (int i = 0; i < symbol.rectangles.size(); ++i) {
        const auto& r = symbol.rectangles.at(i);
        if (r.partIndex == part)
            addRectangle(QStringLiteral("rectangle/"), r.x0, r.y0, r.x1, r.y1, i);
    }
    for (int i = 0; i < symbol.polylines.size(); ++i) {
        const auto& poly = symbol.polylines.at(i);
        if (poly.partIndex != part)
            continue;
        for (int j = 1; j < poly.points.size(); ++j)
            addLine(lines,
                    junctions,
                    symbol.name + QStringLiteral("/polyline/") + QString::number(i) + QStringLiteral("/") +
                        QString::number(j),
                    poly.points.at(j - 1),
                    poly.points.at(j),
                    0,
                    poly.strokeWidth);
    }
    for (int i = 0; i < symbol.polygons.size(); ++i) {
        const auto& poly = symbol.polygons.at(i);
        if (poly.partIndex == part)
            addPolygon(polygons, symbol.name + QStringLiteral("/polygon/") + QString::number(i), poly.points, 0);
    }
    for (int i = 0; i < symbol.paths.size(); ++i) {
        const auto& path = symbol.paths.at(i);
        if (path.partIndex != part)
            continue;
        if (!path.points.isEmpty()) {
            addPolyline(lines,
                        junctions,
                        symbol.name + QStringLiteral("/path/") + QString::number(i),
                        path.points,
                        0,
                        path.strokeWidth);
        } else {
            for (int j = 0; j < path.segments.size(); ++j) {
                const auto& segment = path.segments.at(j);
                QList<QPointF> points = {segment.start, segment.end};
                if (segment.type == IR::SymbolPathSegmentIR::Type::CubicBezier)
                    points = cubicBezierPoints({segment.start, segment.control1, segment.control2, segment.end});
                else if (segment.type == IR::SymbolPathSegmentIR::Type::QuadraticBezier) {
                    points.clear();
                    for (int k = 0; k <= 16; ++k) {
                        const double t = static_cast<double>(k) / 16.0;
                        const double u = 1.0 - t;
                        points.append(u * u * segment.start + 2.0 * u * t * segment.control1 + t * t * segment.end);
                    }
                } else if (segment.type == IR::SymbolPathSegmentIR::Type::CircularArc ||
                           segment.type == IR::SymbolPathSegmentIR::Type::EllipticalArc) {
                    points =
                        ellipsePoints(segment.arcCenter,
                                      segment.radiusX > 0 ? segment.radiusX : segment.end.x() - segment.arcCenter.x(),
                                      segment.radiusY > 0 ? segment.radiusY : segment.end.y() - segment.arcCenter.y(),
                                      segment.arcStartAngle,
                                      segment.arcEndAngle);
                }
                addPolyline(lines,
                            junctions,
                            symbol.name + QStringLiteral("/path/") + QString::number(i) + QStringLiteral("/") +
                                QString::number(j),
                            points,
                            0,
                            path.strokeWidth);
            }
        }
    }
    for (int i = 0; i < symbol.beziers.size(); ++i) {
        const auto& bezier = symbol.beziers.at(i);
        if (bezier.partIndex == part)
            addPolyline(lines,
                        junctions,
                        symbol.name + QStringLiteral("/bezier/") + QString::number(i),
                        cubicBezierPoints(bezier.controlPoints),
                        0,
                        bezier.strokeWidth);
    }
    for (int i = 0; i < symbol.circles.size(); ++i) {
        const auto& circle = symbol.circles.at(i);
        if (circle.partIndex == part)
            addPolygon(polygons,
                       symbol.name + QStringLiteral("/circle/") + QString::number(i),
                       ellipsePoints(circle.center, circle.radius, circle.radius, 0.0, 360.0),
                       0);
    }
    for (int i = 0; i < symbol.ellipses.size(); ++i) {
        const auto& ellipse = symbol.ellipses.at(i);
        if (ellipse.partIndex == part)
            addPolygon(polygons,
                       symbol.name + QStringLiteral("/ellipse/") + QString::number(i),
                       ellipsePoints(ellipse.center, ellipse.radiusX, ellipse.radiusY, 0.0, 360.0),
                       0);
    }
    for (int i = 0; i < symbol.arcs.size(); ++i) {
        const auto& arc = symbol.arcs.at(i);
        if (arc.partIndex == part)
            addPolyline(lines,
                        junctions,
                        symbol.name + QStringLiteral("/arc/") + QString::number(i),
                        {arc.startPoint, arc.midPoint, arc.endPoint},
                        0,
                        arc.strokeWidth);
    }
    for (int i = 0; i < symbol.pies.size(); ++i) {
        const auto& pie = symbol.pies.at(i);
        if (pie.partIndex == part) {
            QList<QPointF> points = ellipsePoints(pie.center, pie.radius, pie.radius, pie.startAngle, pie.endAngle);
            points.prepend(pie.center);
            addPolygon(polygons, symbol.name + QStringLiteral("/pie/") + QString::number(i), points, 0);
        }
    }
    for (int i = 0; i < symbol.ellipticalArcs.size(); ++i) {
        const auto& arc = symbol.ellipticalArcs.at(i);
        if (arc.partIndex == part)
            addPolyline(lines,
                        junctions,
                        symbol.name + QStringLiteral("/elliptical-arc/") + QString::number(i),
                        ellipsePoints(arc.center, arc.radiusX, arc.radiusY, arc.startAngle, arc.endAngle),
                        0,
                        arc.strokeWidth);
    }
    for (int i = 0; i < symbol.texts.size(); ++i) {
        const auto& text = symbol.texts.at(i);
        if (text.partIndex != part || !text.visible)
            continue;
        texts.insert(
            HorizonUuid::make(QStringLiteral("symbol-text"), symbol.name + QString::number(part) + QString::number(i)),
            QJsonObject{{QStringLiteral("origin"), QStringLiteral("center")},
                        {QStringLiteral("font"), QStringLiteral("simplex")},
                        {QStringLiteral("text"), text.text},
                        {QStringLiteral("size"), HorizonUnits::mm(text.fontSizeMm > 0 ? text.fontSizeMm : 1.5)},
                        {QStringLiteral("width"), HorizonUnits::mm(text.fontSizeMm > 0 ? text.fontSizeMm / 10.0 : 0.0)},
                        {QStringLiteral("layer"), 0},
                        {QStringLiteral("placement"), placement(text.position, text.rotation)}});
    }
    for (int i = 0; i < symbol.textFrames.size(); ++i) {
        const auto& frame = symbol.textFrames.at(i);
        if (frame.partIndex != part)
            continue;
        addRectangle(QStringLiteral("text-frame/"), frame.x0, frame.y0, frame.x1, frame.y1, i);
        if (!frame.text.isEmpty())
            texts.insert(
                HorizonUuid::make(QStringLiteral("symbol-text-frame"),
                                  symbol.name + QString::number(part) + QString::number(i)),
                QJsonObject{{QStringLiteral("origin"), QStringLiteral("center")},
                            {QStringLiteral("font"), QStringLiteral("simplex")},
                            {QStringLiteral("text"), frame.text},
                            {QStringLiteral("size"), HorizonUnits::mm(frame.fontSizeMm > 0 ? frame.fontSizeMm : 1.5)},
                            {QStringLiteral("width"), HorizonUnits::mm(frame.strokeWidth)},
                            {QStringLiteral("layer"), 0},
                            {QStringLiteral("placement"),
                             placement(QPointF((frame.x0 + frame.x1) / 2.0, (frame.y0 + frame.y1) / 2.0))}});
    }
    // QJsonObject values are implicitly shared; assign modified maps back explicitly.
    result.insert(QStringLiteral("junctions"), junctions);
    result.insert(QStringLiteral("lines"), lines);
    result.insert(QStringLiteral("pins"), pins);
    result.insert(QStringLiteral("polygons"), polygons);
    result.insert(QStringLiteral("texts"), texts);
    result.insert(QStringLiteral("arcs"), QJsonObject{});
    result.insert(QStringLiteral("text_placements"), QJsonObject{});
    return result;
}

QJsonObject padstackJson(const IR::FootprintPadIR& pad, const QString& uuid, QStringList& diagnostics) {
    const QString context = pad.number;
    QJsonObject shapes;
    QJsonObject polygons;
    QJsonObject parameterSet;
    QStringList parameterProgram;
    const int layer = packageLayer(pad.layer, diagnostics, context);
    const bool hasCustomPolygon = (pad.shape == IR::PadShape::Polygon || pad.shape == IR::PadShape::RoundRect ||
                                   pad.shape == IR::PadShape::Trapezoid) &&
                                  pad.customShapePoints.size() >= 3;
    QList<QPointF> rotatedCustomPoints;
    if (hasCustomPolygon) {
        rotatedCustomPoints.reserve(pad.customShapePoints.size());
        for (const QPointF& item : pad.customShapePoints)
            rotatedCustomPoints.append(rotateAround(item, {}, pad.rotation));
    }
    const QList<int> throughCopperLayers = {
        kTopCopper, kInnerCopper1, kInnerCopper2, kInnerCopper3, kInnerCopper4, kBottomCopper};
    QString form = QStringLiteral("rectangle");
    QJsonArray params{HorizonUnits::mm(pad.size.width()), HorizonUnits::mm(pad.size.height())};
    if (pad.shape == IR::PadShape::Ellipse) {
        if (qFuzzyCompare(pad.size.width(), pad.size.height())) {
            form = QStringLiteral("circle");
            params = QJsonArray{HorizonUnits::mm(pad.size.width())};
        } else {
            form = QStringLiteral("obround");
        }
    } else if (pad.shape == IR::PadShape::Oval) {
        form = QStringLiteral("obround");
    } else if (pad.shape == IR::PadShape::RoundRect && !hasCustomPolygon) {
        diagnostics.append(QStringLiteral("Horizon: IR 未提供圆角半径，Pad %1 按 rectangle 输出").arg(context));
    } else if (pad.shape == IR::PadShape::Polygon || pad.shape == IR::PadShape::RoundRect ||
               pad.shape == IR::PadShape::Trapezoid) {
        if (pad.customShapePoints.size() < 3)
            diagnostics.append(QStringLiteral("Horizon: Pad %1 的自定义形状缺少至少 3 个顶点").arg(context));
        else {
            QJsonArray vertices;
            for (const QPointF& item : rotatedCustomPoints)
                vertices.append(QJsonObject{{QStringLiteral("type"), QStringLiteral("line")},
                                            {QStringLiteral("position"), point(item)},
                                            {QStringLiteral("arc_center"), point(0.0, 0.0)},
                                            {QStringLiteral("arc_reverse"), false}});
            const auto addPolygonForLayer = [&polygons, &vertices, &uuid](const QString& semantic, int polygonLayer) {
                polygons.insert(
                    HorizonUuid::make(QStringLiteral("padstack-polygon"), uuid + semantic),
                    QJsonObject{{QStringLiteral("vertices"), vertices}, {QStringLiteral("layer"), polygonLayer}});
            };
            if (pad.isThroughHole()) {
                for (const int copperLayer : throughCopperLayers)
                    addPolygonForLayer(QStringLiteral("/copper/") + QString::number(copperLayer), copperLayer);
            } else {
                addPolygonForLayer(QStringLiteral("/copper"), layer);
            }
            diagnostics.append(QStringLiteral("Horizon: Pad %1 的自定义轮廓已写入 padstack polygon").arg(context));
        }
    }

    const auto appendParameterValue = [&parameterSet](const QString& name, double value) {
        parameterSet.insert(name, HorizonUnits::mm(value));
    };
    const auto dimensionLiteral = [](double value) {
        return QString::number(static_cast<double>(HorizonUnits::mm(value)) / 1000000.0, 'f', 6) + QStringLiteral("mm");
    };
    const auto appendShapeProgram = [&parameterProgram, &dimensionLiteral, &pad, &form](
                                        const QString& parameterClass, const QString& parameterName, bool contraction) {
        if (form == QStringLiteral("circle")) {
            parameterProgram.append(dimensionLiteral(pad.size.width()));
            parameterProgram.append(
                QStringLiteral("get-parameter [ %1 ] %2").arg(parameterName, contraction ? "2 * -" : "2 * +"));
            parameterProgram.append(QStringLiteral("set-shape [ %1 circle ]").arg(parameterClass));
        } else {
            parameterProgram.append(dimensionLiteral(pad.size.width()) + QStringLiteral(" ") +
                                    dimensionLiteral(pad.size.height()));
            parameterProgram.append(
                QStringLiteral("get-parameter [ %1 ] %2").arg(parameterName, contraction ? "2 * -xy" : "2 * +xy"));
            parameterProgram.append(QStringLiteral("set-shape [ %1 %2 ]").arg(parameterClass, form));
        }
    };
    const auto appendPolygonProgram = [&parameterProgram](const QString& parameterClass,
                                                          const QString& parameterName,
                                                          bool contraction,
                                                          const QList<QPointF>& points) {
        if (contraction)
            parameterProgram.append(QStringLiteral("0 get-parameter [ %1 ] -").arg(parameterName));
        else
            parameterProgram.append(QStringLiteral("get-parameter [ %1 ]").arg(parameterName));
        QString line = QStringLiteral("expand-polygon [ ") + parameterClass;
        for (const QPointF& item : points)
            line += QStringLiteral(" %1 %2").arg(HorizonUnits::mm(item.x())).arg(HorizonUnits::mm(item.y()));
        line += QStringLiteral(" ]");
        parameterProgram.append(line);
    };
    if (!hasCustomPolygon) {
        const QString parameterClass = pad.isSmd() ? QStringLiteral("pad") : QStringLiteral("copper");
        const auto addShapeForLayer = [&shapes, &uuid, &form, &params, &pad, &parameterClass](const QString& semantic,
                                                                                              int shapeLayer) {
            shapes.insert(HorizonUuid::make(QStringLiteral("padstack-shape"), uuid + semantic),
                          QJsonObject{{QStringLiteral("form"), form},
                                      {QStringLiteral("layer"), shapeLayer},
                                      {QStringLiteral("parameter_class"), parameterClass},
                                      {QStringLiteral("params"), params},
                                      {QStringLiteral("placement"), placement({}, pad.rotation)}});
        };
        if (pad.isThroughHole()) {
            for (const int copperLayer : throughCopperLayers)
                addShapeForLayer(QStringLiteral("/copper/") + QString::number(copperLayer), copperLayer);
        } else {
            addShapeForLayer(QStringLiteral("/copper"), layer);
        }
    }
    if (pad.isSmd() && !hasCustomPolygon) {
        const int maskLayer = layer == kBottomCopper ? kBottomMask : kTopMask;
        const int pasteLayer = layer == kBottomCopper ? kBottomPaste : kTopPaste;
        if (pad.solderMaskEnabled) {
            const QString maskUuid =
                HorizonUuid::make(QStringLiteral("padstack-shape"), uuid + QStringLiteral("/mask"));
            shapes.insert(maskUuid,
                          QJsonObject{{QStringLiteral("form"), form},
                                      {QStringLiteral("layer"), maskLayer},
                                      {QStringLiteral("parameter_class"), QStringLiteral("mask")},
                                      {QStringLiteral("params"), params},
                                      {QStringLiteral("placement"), placement({}, pad.rotation)}});
        }
        if (pad.pasteMaskEnabled) {
            const QString pasteUuid =
                HorizonUuid::make(QStringLiteral("padstack-shape"), uuid + QStringLiteral("/paste"));
            shapes.insert(pasteUuid,
                          QJsonObject{{QStringLiteral("form"), form},
                                      {QStringLiteral("layer"), pasteLayer},
                                      {QStringLiteral("parameter_class"), QStringLiteral("paste")},
                                      {QStringLiteral("params"), params},
                                      {QStringLiteral("placement"), placement({}, pad.rotation)}});
        }
    } else if (pad.isSmd() && hasCustomPolygon) {
        const int maskLayer = layer == kBottomCopper ? kBottomMask : kTopMask;
        const int pasteLayer = layer == kBottomCopper ? kBottomPaste : kTopPaste;
        for (const auto& entry :
             {qMakePair(QStringLiteral("mask"), maskLayer), qMakePair(QStringLiteral("paste"), pasteLayer)}) {
            if ((entry.first == QStringLiteral("mask") && !pad.solderMaskEnabled) ||
                (entry.first == QStringLiteral("paste") && !pad.pasteMaskEnabled))
                continue;
            QJsonArray vertices;
            for (const QPointF& item : pad.customShapePoints)
                vertices.append(QJsonObject{{QStringLiteral("type"), QStringLiteral("line")},
                                            {QStringLiteral("position"), point(rotateAround(item, {}, pad.rotation))},
                                            {QStringLiteral("arc_center"), point(0.0, 0.0)},
                                            {QStringLiteral("arc_reverse"), false}});
            polygons.insert(
                HorizonUuid::make(QStringLiteral("padstack-polygon"), uuid + QStringLiteral("/") + entry.first),
                QJsonObject{{QStringLiteral("vertices"), vertices},
                            {QStringLiteral("layer"), entry.second},
                            {QStringLiteral("parameter_class"), entry.first}});
        }
    } else if (pad.isThroughHole()) {
        const int topMaskLayer = kTopMask;
        const int bottomMaskLayer = kBottomMask;
        if (hasCustomPolygon && pad.solderMaskEnabled) {
            QList<QPointF> points;
            points.reserve(pad.customShapePoints.size());
            for (const QPointF& item : pad.customShapePoints)
                points.append(rotateAround(item, {}, pad.rotation));
            QJsonArray vertices;
            for (const QPointF& item : points)
                vertices.append(QJsonObject{{QStringLiteral("type"), QStringLiteral("line")},
                                            {QStringLiteral("position"), point(item)},
                                            {QStringLiteral("arc_center"), point(0.0, 0.0)},
                                            {QStringLiteral("arc_reverse"), false}});
            for (const auto& entry : {qMakePair(QStringLiteral("/mask/top"), topMaskLayer),
                                      qMakePair(QStringLiteral("/mask/bottom"), bottomMaskLayer)}) {
                polygons.insert(HorizonUuid::make(QStringLiteral("padstack-polygon"), uuid + entry.first),
                                QJsonObject{{QStringLiteral("vertices"), vertices},
                                            {QStringLiteral("layer"), entry.second},
                                            {QStringLiteral("parameter_class"), QStringLiteral("mask")}});
            }
        } else if (pad.solderMaskEnabled) {
            for (const auto& entry : {qMakePair(QStringLiteral("/mask/top"), topMaskLayer),
                                      qMakePair(QStringLiteral("/mask/bottom"), bottomMaskLayer)}) {
                shapes.insert(HorizonUuid::make(QStringLiteral("padstack-shape"), uuid + entry.first),
                              QJsonObject{{QStringLiteral("form"), form},
                                          {QStringLiteral("layer"), entry.second},
                                          {QStringLiteral("parameter_class"), QStringLiteral("mask")},
                                          {QStringLiteral("params"), params},
                                          {QStringLiteral("placement"), placement({}, pad.rotation)}});
            }
        }
    }
    if (pad.isSmd()) {
        if (pad.solderMaskEnabled) {
            appendParameterValue(QStringLiteral("solder_mask_expansion"), pad.solderMaskExpansionMm);
            if (hasCustomPolygon)
                appendPolygonProgram(
                    QStringLiteral("mask"), QStringLiteral("solder_mask_expansion"), false, rotatedCustomPoints);
            else
                appendShapeProgram(QStringLiteral("mask"), QStringLiteral("solder_mask_expansion"), false);
        }
        if (pad.pasteMaskEnabled) {
            appendParameterValue(QStringLiteral("paste_mask_contraction"), pad.pasteMaskContractionMm);
            if (hasCustomPolygon)
                appendPolygonProgram(
                    QStringLiteral("paste"), QStringLiteral("paste_mask_contraction"), true, rotatedCustomPoints);
            else
                appendShapeProgram(QStringLiteral("paste"), QStringLiteral("paste_mask_contraction"), true);
        }
    } else if (pad.isThroughHole() && (pad.solderMaskEnabled || pad.pasteMaskEnabled)) {
        if (pad.solderMaskEnabled) {
            appendParameterValue(QStringLiteral("solder_mask_expansion"), pad.solderMaskExpansionMm);
            if (hasCustomPolygon)
                appendPolygonProgram(
                    QStringLiteral("mask"), QStringLiteral("solder_mask_expansion"), false, rotatedCustomPoints);
            else
                appendShapeProgram(QStringLiteral("mask"), QStringLiteral("solder_mask_expansion"), false);
        }
    }
    QJsonObject holes;
    if (pad.isThroughHole()) {
        const QString holeUuid = HorizonUuid::make(QStringLiteral("padstack-hole"), uuid);
        holes.insert(holeUuid,
                     QJsonObject{{QStringLiteral("diameter"), HorizonUnits::mm(pad.holeSize)},
                                 {QStringLiteral("length"),
                                  HorizonUnits::mm(pad.holeLength > 0 ? pad.holeLength : pad.holeSize)},
                                 {QStringLiteral("parameter_class"), QStringLiteral("hole")},
                                 {QStringLiteral("placement"), placement()},
                                 {QStringLiteral("plated"), pad.isPlated},
                                 {QStringLiteral("shape"),
                                  pad.holeLength > pad.holeSize ? QStringLiteral("slot") : QStringLiteral("round")}});
    }
    const QString padstackType = pad.isThroughHole()
                                     ? QStringLiteral("through")
                                     : (layer == kBottomCopper ? QStringLiteral("bottom") : QStringLiteral("top"));
    return QJsonObject{{QStringLiteral("type"), QStringLiteral("padstack")},
                       {QStringLiteral("uuid"), uuid},
                       {QStringLiteral("name"), QStringLiteral("Pad ") + pad.number},
                       {QStringLiteral("padstack_type"), padstackType},
                       {QStringLiteral("parameter_program"), parameterProgram.join(QLatin1Char('\n'))},
                       {QStringLiteral("parameter_set"), parameterSet},
                       {QStringLiteral("parameters_required"), QJsonArray{}},
                       {QStringLiteral("polygons"), polygons},
                       {QStringLiteral("holes"), holes},
                       {QStringLiteral("shapes"), shapes}};
}

QJsonObject holePadstackJson(const IR::FootprintHoleIR& hole, const QString& uuid) {
    const QString holeUuid = HorizonUuid::make(QStringLiteral("hole"), uuid);
    const qint64 diameter = HorizonUnits::mm(hole.radius * 2.0);
    const QString topMaskUuid = HorizonUuid::make(QStringLiteral("hole-mask"), uuid + QStringLiteral("/top"));
    const QString bottomMaskUuid = HorizonUuid::make(QStringLiteral("hole-mask"), uuid + QStringLiteral("/bottom"));
    const QJsonObject maskShape = QJsonObject{{QStringLiteral("form"), QStringLiteral("circle")},
                                              {QStringLiteral("parameter_class"), QStringLiteral("mask")},
                                              {QStringLiteral("params"), QJsonArray{diameter}},
                                              {QStringLiteral("placement"), placement()}};
    QJsonObject topMask = maskShape;
    topMask.insert(QStringLiteral("layer"), kTopMask);
    QJsonObject bottomMask = maskShape;
    bottomMask.insert(QStringLiteral("layer"), kBottomMask);
    return QJsonObject{{QStringLiteral("type"), QStringLiteral("padstack")},
                       {QStringLiteral("uuid"), uuid},
                       {QStringLiteral("name"), QStringLiteral("Mounting hole")},
                       {QStringLiteral("padstack_type"), QStringLiteral("mechanical")},
                       {QStringLiteral("parameter_program"), QString()},
                       {QStringLiteral("parameter_set"), QJsonObject{}},
                       {QStringLiteral("parameters_required"), QJsonArray{}},
                       {QStringLiteral("polygons"), QJsonObject{}},
                       {QStringLiteral("holes"),
                        QJsonObject{{holeUuid,
                                     QJsonObject{{QStringLiteral("diameter"), diameter},
                                                 {QStringLiteral("length"), diameter},
                                                 {QStringLiteral("parameter_class"), QStringLiteral("hole")},
                                                 {QStringLiteral("placement"), placement()},
                                                 {QStringLiteral("plated"), false},
                                                 {QStringLiteral("shape"), QStringLiteral("round")}}}}},
                       {QStringLiteral("shapes"), QJsonObject{{topMaskUuid, topMask}, {bottomMaskUuid, bottomMask}}}};
}

QJsonObject packageJson(const IR::FootprintComponentIR& footprint,
                        const QString& packageUuid,
                        const QList<IR::Model3DIR>& models,
                        QHash<QString, QString>& padUuids,
                        QStringList& diagnostics) {
    QJsonObject result{{QStringLiteral("type"), QStringLiteral("package")},
                       {QStringLiteral("uuid"), packageUuid},
                       {QStringLiteral("name"), footprint.name},
                       {QStringLiteral("manufacturer"), QString()},
                       {QStringLiteral("parameter_set"), QJsonObject{}},
                       {QStringLiteral("parameters_required"), QJsonArray{}},
                       {QStringLiteral("models"), QJsonObject{}},
                       {QStringLiteral("junctions"), QJsonObject{}},
                       {QStringLiteral("lines"), QJsonObject{}},
                       {QStringLiteral("arcs"), QJsonObject{}},
                       {QStringLiteral("texts"), QJsonObject{}},
                       {QStringLiteral("pads"), QJsonObject{}},
                       {QStringLiteral("polygons"), QJsonObject{}},
                       {QStringLiteral("keepouts"), QJsonObject{}},
                       {QStringLiteral("dimensions"), QJsonObject{}}};
    QJsonObject pads = result[QStringLiteral("pads")].toObject();
    for (int i = 0; i < footprint.holes.size(); ++i) {
        const auto& hole = footprint.holes.at(i);
        const QString holeName = QStringLiteral("MH%1").arg(i + 1);
        const QString padUuid =
            HorizonUuid::make(QStringLiteral("package-hole"), footprint.name + QStringLiteral("/") + holeName);
        const QString padstackUuid =
            HorizonUuid::make(QStringLiteral("hole-padstack"), footprint.name + QStringLiteral("/") + holeName);
        pads.insert(padUuid,
                    QJsonObject{{QStringLiteral("padstack"), padstackUuid},
                                {QStringLiteral("placement"), placement(hole.center)},
                                {QStringLiteral("name"), holeName},
                                {QStringLiteral("parameter_set"), QJsonObject{}}});
    }
    for (int i = 0; i < footprint.pads.size(); ++i) {
        const auto& pad = footprint.pads.at(i);
        if (pad.number.trimmed().isEmpty() && !isMechanicalHolePad(pad)) {
            diagnostics.append(QStringLiteral("Horizon: 封装 %1 存在空焊盘编号").arg(footprint.name));
            continue;
        }
        if (!pad.number.trimmed().isEmpty() && padUuids.contains(pad.number)) {
            diagnostics.append(QStringLiteral("Horizon: 封装 %1 存在重复焊盘编号 %2").arg(footprint.name, pad.number));
            continue;
        }
        const QString padName = padFileStem(pad, i);
        const QString padUuid =
            HorizonUuid::make(QStringLiteral("package-pad"), footprint.name + QStringLiteral("/") + padName);
        const QString padstackUuid =
            HorizonUuid::make(QStringLiteral("padstack"), footprint.name + QStringLiteral("/") + padName);
        if (!isMechanicalHolePad(pad))
            padUuids.insert(pad.number, padUuid);
        pads.insert(padUuid,
                    QJsonObject{{QStringLiteral("padstack"), padstackUuid},
                                {QStringLiteral("placement"), placement(pad.position, pad.rotation)},
                                {QStringLiteral("name"), padName},
                                {QStringLiteral("parameter_set"), QJsonObject{}}});
    }
    result.insert(QStringLiteral("pads"), pads);

    QJsonObject junctions = result[QStringLiteral("junctions")].toObject();
    QJsonObject lines = result[QStringLiteral("lines")].toObject();
    QJsonObject arcs = result[QStringLiteral("arcs")].toObject();
    QJsonObject texts = result[QStringLiteral("texts")].toObject();
    QJsonObject polygons = result[QStringLiteral("polygons")].toObject();
    QJsonObject keepouts = result[QStringLiteral("keepouts")].toObject();
    for (int i = 0; i < footprint.outlines.size(); ++i) {
        const auto& outline = footprint.outlines.at(i);
        const int layer = packageLayer(outline.layer, diagnostics, footprint.name);
        for (int j = 1; j < outline.points.size(); ++j)
            addLine(lines,
                    junctions,
                    footprint.name + QStringLiteral("/outline/") + QString::number(i) + QString::number(j),
                    outline.points.at(j - 1),
                    outline.points.at(j),
                    layer,
                    outline.strokeWidth);
    }
    for (int i = 0; i < footprint.regions.size(); ++i) {
        const auto& region = footprint.regions.at(i);
        const QString semantic = footprint.name + QStringLiteral("/region/") + QString::number(i);
        const QString polygonUuid = HorizonUuid::make(QStringLiteral("polygon"), semantic);
        const int regionLayer = region.isKeepOut ? kTopCopper : packageLayer(region.layer, diagnostics, footprint.name);
        addPolygon(polygons, semantic, region.vertices, regionLayer);
        if (region.isKeepOut) {
            keepouts.insert(HorizonUuid::make(QStringLiteral("package-keepout"), semantic),
                            QJsonObject{{QStringLiteral("polygon"), polygonUuid},
                                        {QStringLiteral("keepout_class"), QStringLiteral("copper")},
                                        {QStringLiteral("exposed_cu_only"), false},
                                        {QStringLiteral("all_cu_layers"), true},
                                        {QStringLiteral("patch_types_cu"),
                                         QJsonArray{QStringLiteral("pad"),
                                                    QStringLiteral("pad_th"),
                                                    QStringLiteral("track"),
                                                    QStringLiteral("via"),
                                                    QStringLiteral("plane"),
                                                    QStringLiteral("hole_pth")}}});
        }
    }
    for (int i = 0; i < footprint.tracks.size(); ++i) {
        const auto& track = footprint.tracks.at(i);
        addPolyline(lines,
                    junctions,
                    footprint.name + QStringLiteral("/track/") + QString::number(i),
                    track.points,
                    packageLayer(track.layer, diagnostics, footprint.name),
                    track.width);
    }
    for (int i = 0; i < footprint.circles.size(); ++i) {
        const auto& circle = footprint.circles.at(i);
        addCircle(arcs,
                  junctions,
                  footprint.name + QStringLiteral("/circle/") + QString::number(i),
                  circle.center,
                  circle.radius,
                  packageLayer(circle.layer, diagnostics, footprint.name),
                  circle.strokeWidth);
    }
    for (int i = 0; i < footprint.rectangles.size(); ++i) {
        const auto& rectangle = footprint.rectangles.at(i);
        const QRectF bounds = rectangle.bounds.normalized();
        const QPointF center = bounds.center();
        QList<QPointF> points = {bounds.topLeft(), bounds.topRight(), bounds.bottomRight(), bounds.bottomLeft()};
        for (QPointF& item : points)
            item = rotateAround(item, center, rectangle.rotation);
        const int layer = packageLayer(rectangle.layer, diagnostics, footprint.name);
        for (int j = 0; j < points.size(); ++j)
            addLine(lines,
                    junctions,
                    footprint.name + QStringLiteral("/rectangle/") + QString::number(i) + QStringLiteral("/") +
                        QString::number(j),
                    points.at(j),
                    points.at((j + 1) % points.size()),
                    layer,
                    rectangle.strokeWidth);
    }
    for (int i = 0; i < footprint.arcs.size(); ++i) {
        const auto& arc = footprint.arcs.at(i);
        addPolyline(lines,
                    junctions,
                    footprint.name + QStringLiteral("/arc/") + QString::number(i),
                    ellipsePoints(arc.center, arc.radius, arc.radius, arc.startAngle, arc.endAngle, 24),
                    packageLayer(arc.layer, diagnostics, footprint.name),
                    arc.width);
    }
    for (int i = 0; i < footprint.texts.size(); ++i) {
        const auto& text = footprint.texts.at(i);
        if (!text.isDisplayed)
            continue;
        const int layer = packageLayer(text.layer, diagnostics, footprint.name);
        texts.insert(HorizonUuid::make(QStringLiteral("package-text"), footprint.name + QString::number(i)),
                     QJsonObject{{QStringLiteral("origin"), QStringLiteral("center")},
                                 {QStringLiteral("font"), QStringLiteral("simplex")},
                                 {QStringLiteral("text"), text.text},
                                 {QStringLiteral("size"), HorizonUnits::mm(text.fontSize > 0 ? text.fontSize : 1.5)},
                                 {QStringLiteral("width"), HorizonUnits::mm(text.strokeWidth)},
                                 {QStringLiteral("layer"), layer},
                                 {QStringLiteral("placement"), placement(text.position, text.rotation, text.mirror)}});
    }
    result.insert(QStringLiteral("junctions"), junctions);
    result.insert(QStringLiteral("lines"), lines);
    result.insert(QStringLiteral("arcs"), arcs);
    result.insert(QStringLiteral("texts"), texts);
    result.insert(QStringLiteral("polygons"), polygons);
    result.insert(QStringLiteral("keepouts"), keepouts);
    QJsonObject modelObjects;
    for (int i = 0; i < models.size(); ++i) {
        const IR::Model3DIR& model = models.at(i);
        const QString modelUuid =
            HorizonUuid::make(QStringLiteral("package-model"), packageUuid + QStringLiteral("/") + QString::number(i));
        const QString suffix = QStringLiteral(".step");
        const QString filename = QStringLiteral("3d_models/") + modelUuid + suffix;
        const auto translation = model.translation();
        const auto stepOffset = model.stepOffsetMm();
        const auto rotation = model.rotation();
        modelObjects.insert(modelUuid,
                            QJsonObject{{QStringLiteral("filename"), filename},
                                        {QStringLiteral("x"), HorizonUnits::mm(translation.x + stepOffset.x)},
                                        {QStringLiteral("y"), HorizonUnits::mm(translation.y + stepOffset.y)},
                                        {QStringLiteral("z"), HorizonUnits::mm(translation.z + stepOffset.z)},
                                        {QStringLiteral("roll"), qRound(rotation.x)},
                                        {QStringLiteral("pitch"), qRound(rotation.y)},
                                        {QStringLiteral("yaw"), qRound(rotation.z)}});
    }
    result.insert(QStringLiteral("models"), modelObjects);
    const QString defaultModelUuid =
        models.isEmpty() ? QString()
                         : HorizonUuid::make(QStringLiteral("package-model"), packageUuid + QStringLiteral("/0"));
    result.insert(
        QStringLiteral("default_model"),
        defaultModelUuid.isEmpty() ? QStringLiteral("00000000-0000-0000-0000-000000000000") : defaultModelUuid);
    return result;
}

bool validatePadIdentifiers(const IR::FootprintComponentIR& footprint, QStringList& diagnostics) {
    QSet<QString> identifiers;
    bool valid = true;
    for (const auto& pad : footprint.pads) {
        if (pad.number.trimmed().isEmpty()) {
            if (!isMechanicalHolePad(pad)) {
                diagnostics.append(QStringLiteral("Horizon: 封装 %1 存在空焊盘编号").arg(footprint.name));
                valid = false;
            }
        } else if (identifiers.contains(pad.number)) {
            diagnostics.append(QStringLiteral("Horizon: 封装 %1 存在重复焊盘编号 %2").arg(footprint.name, pad.number));
            valid = false;
        } else {
            identifiers.insert(pad.number);
        }
    }
    return valid;
}

bool validateFootprintOutputNames(const QList<IR::FootprintComponentIR>& footprints, QStringList& diagnostics) {
    QHash<QString, QString> footprintNameOwners;
    QHash<QString, QString> padstackNameOwners;
    for (const auto& footprint : footprints) {
        if (!validateSafeName(footprintNameOwners, footprint.name, QStringLiteral("封装"), diagnostics))
            return false;
        for (int i = 0; i < footprint.pads.size(); ++i) {
            const auto& pad = footprint.pads.at(i);
            const QString padStem = padFileStem(pad, i);
            const QString rawPadstackName = footprint.name + QStringLiteral("/") + padStem;
            if (!validateSanitizedName(padstackNameOwners,
                                       safeName(footprint.name) + QStringLiteral("-") + padStem,
                                       rawPadstackName,
                                       QStringLiteral("焊盘"),
                                       diagnostics))
                return false;
        }
    }
    return true;
}

bool writeModels(const QList<IR::Model3DIR>& models,
                 const QString& root,
                 const QString& packageUuid,
                 QStringList& diagnostics) {
    for (int i = 0; i < models.size(); ++i) {
        const IR::Model3DIR& model = models.at(i);
        if (!model.isValid()) {
            diagnostics.append(QStringLiteral("Horizon: 第 %1 个 3D 模型没有有效的 STEP 或 OBJ 数据").arg(i + 1));
            return false;
        }
        if (!model.hasStepData()) {
            diagnostics.append(
                QStringLiteral("Horizon: 第 %1 个 3D 模型只有 OBJ 数据；当前 Horizon 仅支持 STEP 模型").arg(i + 1));
            return false;
        }
        const QString uuid =
            HorizonUuid::make(QStringLiteral("package-model"), packageUuid + QStringLiteral("/") + QString::number(i));
        const QString suffix = QStringLiteral(".step");
        const QByteArray data = model.stepData();
        if (!writeBinary(QDir(root).filePath(QStringLiteral("3d_models/") + uuid + suffix), data, diagnostics))
            return false;
    }
    return true;
}

bool writeComponent(const IR::ComponentIR& component,
                    const QString& root,
                    bool exportModel3D,
                    const QString& modelBaseDir,
                    QStringList& diagnostics) {
    if (!component.hasSymbol() || !component.hasFootprint()) {
        diagnostics.append(QStringLiteral("Horizon: 组件 %1 缺少 symbol 或 footprint").arg(component.name));
        return false;
    }
    if (!validatePadIdentifiers(component.footprint, diagnostics))
        return false;
    const IR::SymbolComponentIR& symbol = component.symbol;
    const QString baseKey = component.name.isEmpty() ? symbol.name : component.name;
    const QString entityUuid = HorizonUuid::make(QStringLiteral("entity"), baseKey);
    const QString packageUuid = HorizonUuid::make(QStringLiteral("package"), component.footprint.name);
    const QString partUuid = HorizonUuid::make(QStringLiteral("part"), baseKey);
    QJsonObject gates;
    QList<QString> unitUuids;
    for (int part = 0; part < qMax(1, symbol.partCount); ++part) {
        const QString unitUuid =
            HorizonUuid::make(QStringLiteral("unit"), baseKey + QStringLiteral("/part/") + QString::number(part));
        const QString symbolUuid =
            HorizonUuid::make(QStringLiteral("symbol"), baseKey + QStringLiteral("/part/") + QString::number(part));
        unitUuids.append(unitUuid);
        if (!writeJson(QDir(root).filePath(QStringLiteral("units/") + safeName(baseKey) + QStringLiteral("-") +
                                           QString::number(part + 1) + QStringLiteral(".json")),
                       unitJson(symbol, baseKey, part, unitUuid, diagnostics),
                       diagnostics))
            return false;
        if (!writeJson(QDir(root).filePath(QStringLiteral("symbols/") + safeName(baseKey) + QStringLiteral("-") +
                                           QString::number(part + 1) + QStringLiteral(".json")),
                       symbolJson(symbol, baseKey, part, unitUuid, diagnostics),
                       diagnostics))
            return false;
        const QString gateUuid =
            HorizonUuid::make(QStringLiteral("gate"), baseKey + QStringLiteral("/part/") + QString::number(part));
        gates.insert(
            gateUuid,
            QJsonObject{{QStringLiteral("name"),
                         symbol.partCount > 1 ? QStringLiteral("Part %1").arg(part + 1) : QStringLiteral("Main")},
                        {QStringLiteral("suffix"),
                         symbol.partCount > 1 ? QStringLiteral("%1").arg(QChar('A' + part)) : QString()},
                        {QStringLiteral("swap_group"), 0},
                        {QStringLiteral("unit"), unitUuid}});
        Q_UNUSED(symbolUuid);
    }
    if (!writeJson(QDir(root).filePath(QStringLiteral("entities/") + safeName(baseKey) + QStringLiteral(".json")),
                   QJsonObject{{QStringLiteral("type"), QStringLiteral("entity")},
                               {QStringLiteral("uuid"), entityUuid},
                               {QStringLiteral("name"), baseKey},
                               {QStringLiteral("manufacturer"), component.manufacturer},
                               {QStringLiteral("prefix"),
                                component.prefix.isEmpty() ? symbol.designatorPrefix : component.prefix},
                               {QStringLiteral("gates"), gates},
                               {QStringLiteral("tags"), QJsonArray{}}},
                   diagnostics))
        return false;

    QHash<QString, QString> padUuids;
    QList<IR::Model3DIR> models;
    if (exportModel3D) {
        if (component.model3D.isValid())
            models.append(component.model3D);
        models.append(component.footprint.models3d);
    }
    const QJsonObject package = packageJson(component.footprint, packageUuid, models, padUuids, diagnostics);
    const QString packageDir = QDir(root).filePath(QStringLiteral("packages/") + safeName(component.footprint.name));
    if (!QDir().mkpath(packageDir) ||
        !writeJson(QDir(packageDir).filePath(QStringLiteral("package.json")), package, diagnostics))
        return false;
    if (!writeModels(models, root, packageUuid, diagnostics))
        return false;
    for (int i = 0; i < component.footprint.pads.size(); ++i) {
        const auto& pad = component.footprint.pads.at(i);
        const QString padStem = padFileStem(pad, i);
        const QString padstackUuid =
            HorizonUuid::make(QStringLiteral("padstack"), component.footprint.name + QStringLiteral("/") + padStem);
        if (!writeJson(QDir(root).filePath(QStringLiteral("padstacks/") + safeName(component.footprint.name) +
                                           QStringLiteral("-") + padStem + QStringLiteral(".json")),
                       padstackJson(pad, padstackUuid, diagnostics),
                       diagnostics))
            return false;
    }
    for (int i = 0; i < component.footprint.holes.size(); ++i) {
        const QString holeName = QStringLiteral("MH%1").arg(i + 1);
        const QString holeUuid = HorizonUuid::make(QStringLiteral("hole-padstack"),
                                                   component.footprint.name + QStringLiteral("/") + holeName);
        if (!writeJson(QDir(root).filePath(QStringLiteral("padstacks/") + safeName(component.footprint.name) +
                                           QStringLiteral("-") + holeName + QStringLiteral(".json")),
                       holePadstackJson(component.footprint.holes.at(i), holeUuid),
                       diagnostics))
            return false;
    }

    QJsonObject padMap;
    bool pinPadMappingValid = true;
    QHash<QString, int> pinOccurrences;
    for (const auto& pin : symbol.pins) {
        if (pin.designator.trimmed().isEmpty()) {
            pinPadMappingValid = false;
            continue;
        }
        ++pinOccurrences[pin.designator];
    }
    for (auto it = pinOccurrences.cbegin(); it != pinOccurrences.cend(); ++it) {
        if (it.value() > 1) {
            diagnostics.append(
                QStringLiteral("Horizon: 组件 %1 的符号引脚编号 %2 重复，无法建立唯一 pad_map").arg(baseKey, it.key()));
            pinPadMappingValid = false;
        }
    }
    for (const auto& pad : component.footprint.pads) {
        if (isMechanicalHolePad(pad))
            continue;
        if (!padUuids.contains(pad.number))
            continue;
        const IR::SymbolPinIR* matched = nullptr;
        for (const auto& pin : symbol.pins) {
            if (pin.designator == pad.number) {
                matched = &pin;
                break;
            }
        }
        if (!matched) {
            diagnostics.append(QStringLiteral("Horizon: 组件 %1 的焊盘 %2 找不到符号引脚").arg(baseKey, pad.number));
            pinPadMappingValid = false;
            continue;
        }
        const int part = matched->commonToAllParts ? 0 : qBound(0, matched->partIndex, qMax(0, symbol.partCount - 1));
        const QString gateUuid =
            HorizonUuid::make(QStringLiteral("gate"), baseKey + QStringLiteral("/part/") + QString::number(part));
        const QString pinUuid = HorizonUuid::make(QStringLiteral("unit-pin"),
                                                  baseKey + QStringLiteral("/part/") + QString::number(part) +
                                                      QStringLiteral("/designator/") + matched->designator);
        padMap.insert(padUuids.value(pad.number),
                      QJsonObject{{QStringLiteral("gate"), gateUuid}, {QStringLiteral("pin"), pinUuid}});
    }
    for (const auto& pin : symbol.pins) {
        if (pin.designator.trimmed().isEmpty())
            continue;
        bool foundPad = false;
        for (const auto& pad : component.footprint.pads) {
            if (!isMechanicalHolePad(pad) && pad.number == pin.designator) {
                foundPad = true;
                break;
            }
        }
        if (!foundPad) {
            diagnostics.append(
                QStringLiteral("Horizon: 组件 %1 的符号引脚 %2 找不到对应焊盘").arg(baseKey, pin.designator));
            pinPadMappingValid = false;
        }
    }
    if (!pinPadMappingValid)
        return false;
    QJsonObject partJson{{QStringLiteral("type"), QStringLiteral("part")},
                         {QStringLiteral("uuid"), partUuid},
                         {QStringLiteral("entity"), entityUuid},
                         {QStringLiteral("package"), packageUuid},
                         {QStringLiteral("pad_map"), padMap},
                         {QStringLiteral("MPN"), variant(component.manufacturerPart)},
                         {QStringLiteral("manufacturer"), variant(component.manufacturer)},
                         {QStringLiteral("description"), variant(component.description)},
                         {QStringLiteral("datasheet"), variant(component.datasheet)},
                         {QStringLiteral("value"), variant(component.name)},
                         {QStringLiteral("inherit_tags"), false},
                         {QStringLiteral("inherit_model"), false},
                         {QStringLiteral("parametric"), QJsonObject{}}};
    if (!writeJson(QDir(root).filePath(QStringLiteral("parts/") + safeName(baseKey) + QStringLiteral(".json")),
                   partJson,
                   diagnostics))
        return false;
    Q_UNUSED(exportModel3D);
    Q_UNUSED(modelBaseDir);
    return true;
}

bool writePoolInfo(const QString& root, const QString& name, QStringList& diagnostics) {
    const QString uuid = HorizonUuid::make(QStringLiteral("pool"), name);
    const QString viaUuid = HorizonUuid::make(QStringLiteral("default-via"), name);
    const QJsonObject info{{QStringLiteral("uuid"), uuid},
                           {QStringLiteral("default_via"), viaUuid},
                           {QStringLiteral("name"), name},
                           {QStringLiteral("type"), QStringLiteral("pool")},
                           {QStringLiteral("pools_included"), QJsonArray{}}};
    const QString shapeUuid = HorizonUuid::make(QStringLiteral("default-via-shape"), name);
    const QJsonObject shape{{QStringLiteral("form"), QStringLiteral("circle")},
                            {QStringLiteral("layer"), kTopCopper},
                            {QStringLiteral("parameter_class"), QStringLiteral("via")},
                            {QStringLiteral("params"), QJsonArray{HorizonUnits::mm(0.7)}},
                            {QStringLiteral("placement"), placement()}};
    const QJsonObject via{{QStringLiteral("type"), QStringLiteral("padstack")},
                          {QStringLiteral("uuid"), viaUuid},
                          {QStringLiteral("name"), QStringLiteral("Default via")},
                          {QStringLiteral("padstack_type"), QStringLiteral("via")},
                          {QStringLiteral("parameter_program"), QString()},
                          {QStringLiteral("parameter_set"), QJsonObject{}},
                          {QStringLiteral("parameters_required"), QJsonArray{}},
                          {QStringLiteral("polygons"), QJsonObject{}},
                          {QStringLiteral("holes"), QJsonObject{}},
                          {QStringLiteral("shapes"), QJsonObject{{shapeUuid, shape}}}};
    return writeJson(QDir(root).filePath(QStringLiteral("pool.json")), info, diagnostics) &&
           writeJson(QDir(root).filePath(QStringLiteral("padstacks/default-via.json")), via, diagnostics);
}

bool writeFootprintFiles(const IR::FootprintComponentIR& footprint,
                         const QString& root,
                         bool exportModel3D,
                         QStringList& diagnostics) {
    if (!validatePadIdentifiers(footprint, diagnostics))
        return false;
    QHash<QString, QString> padUuids;
    const QString packageUuid = HorizonUuid::make(QStringLiteral("package"), footprint.name);
    const QString packageDir = QDir(root).filePath(QStringLiteral("packages/") + safeName(footprint.name));
    const QList<IR::Model3DIR> models = exportModel3D ? footprint.models3d : QList<IR::Model3DIR>{};
    if (!QDir().mkpath(packageDir) || !writeJson(QDir(packageDir).filePath(QStringLiteral("package.json")),
                                                 packageJson(footprint, packageUuid, models, padUuids, diagnostics),
                                                 diagnostics))
        return false;
    if (!writeModels(models, root, packageUuid, diagnostics))
        return false;
    for (int i = 0; i < footprint.pads.size(); ++i) {
        const auto& pad = footprint.pads.at(i);
        const QString padStem = padFileStem(pad, i);
        const QString uuid =
            HorizonUuid::make(QStringLiteral("padstack"), footprint.name + QStringLiteral("/") + padStem);
        if (!writeJson(QDir(root).filePath(QStringLiteral("padstacks/") + safeName(footprint.name) +
                                           QStringLiteral("-") + padStem + QStringLiteral(".json")),
                       padstackJson(pad, uuid, diagnostics),
                       diagnostics))
            return false;
    }
    for (int i = 0; i < footprint.holes.size(); ++i) {
        const QString holeName = QStringLiteral("MH%1").arg(i + 1);
        const QString holeUuid =
            HorizonUuid::make(QStringLiteral("hole-padstack"), footprint.name + QStringLiteral("/") + holeName);
        if (!writeJson(QDir(root).filePath(QStringLiteral("padstacks/") + safeName(footprint.name) +
                                           QStringLiteral("-") + holeName + QStringLiteral(".json")),
                       holePadstackJson(footprint.holes.at(i), holeUuid),
                       diagnostics))
            return false;
    }
    return true;
}

}  // namespace

QString ExporterHorizonLibrary::libraryFileExtension() const {
    return QStringLiteral(".pool");
}

bool ExporterHorizonLibrary::isDirectoryOutput() const {
    return true;
}

QStringList ExporterHorizonLibrary::diagnostics() const {
    return m_diagnostics;
}

bool ExporterHorizonLibrary::exportFootprint(const IR::FootprintComponentIR& footprint,
                                             const QString& filePath,
                                             const QString& model3DPath) {
    Q_UNUSED(model3DPath);
    m_diagnostics.clear();
    if (!validateFootprintOutputNames({footprint}, m_diagnostics))
        return false;
    if (!ensureDirs(filePath, m_diagnostics) || !writePoolInfo(filePath, footprint.name, m_diagnostics))
        return false;
    return writeFootprintFiles(footprint, filePath, true, m_diagnostics);
}

bool ExporterHorizonLibrary::exportFootprintLibrary(const QList<IR::FootprintComponentIR>& footprints,
                                                    const QString& libName,
                                                    const QString& filePath,
                                                    bool,
                                                    bool exportStep,
                                                    const QString&,
                                                    const QString&,
                                                    bool,
                                                    const QString&) {
    m_diagnostics.clear();
    if (footprints.isEmpty()) {
        m_diagnostics.append(QStringLiteral("Horizon: 没有可导出的封装"));
        return false;
    }
    if (!validateFootprintOutputNames(footprints, m_diagnostics))
        return false;
    if (!ensureDirs(filePath, m_diagnostics) || !writePoolInfo(filePath, libName, m_diagnostics))
        return false;
    for (const auto& footprint : footprints) {
        if (!writeFootprintFiles(footprint, filePath, exportStep, m_diagnostics))
            return false;
    }
    return true;
}

bool ExporterHorizonLibrary::exportSymbol(const IR::SymbolComponentIR& symbol, const QString& filePath) {
    return exportSymbolLibrary({symbol}, symbol.name, filePath, false, false);
}

bool ExporterHorizonLibrary::exportSymbolLibrary(const QList<IR::SymbolComponentIR>& symbols,
                                                 const QString& libName,
                                                 const QString& filePath,
                                                 bool,
                                                 bool,
                                                 const QString&) {
    m_diagnostics.clear();
    if (symbols.isEmpty()) {
        m_diagnostics.append(QStringLiteral("Horizon: 没有可导出的符号"));
        return false;
    }
    QHash<QString, QString> symbolNameOwners;
    for (const auto& symbol : symbols) {
        if (symbol.name.trimmed().isEmpty()) {
            m_diagnostics.append(QStringLiteral("Horizon: 符号名称为空"));
            return false;
        }
        if (!validateSafeName(symbolNameOwners, symbol.name, QStringLiteral("符号"), m_diagnostics))
            return false;
    }
    if (!ensureDirs(filePath, m_diagnostics) || !writePoolInfo(filePath, libName, m_diagnostics))
        return false;
    for (const auto& symbol : symbols) {
        QJsonObject gates;
        for (int part = 0; part < qMax(1, symbol.partCount); ++part) {
            const QString baseKey = symbol.name;
            const QString unitUuid =
                HorizonUuid::make(QStringLiteral("unit"), baseKey + QStringLiteral("/part/") + QString::number(part));
            if (!writeJson(QDir(filePath).filePath(QStringLiteral("units/") + safeName(baseKey) + QStringLiteral("-") +
                                                   QString::number(part + 1) + QStringLiteral(".json")),
                           unitJson(symbol, baseKey, part, unitUuid, m_diagnostics),
                           m_diagnostics))
                return false;
            if (!writeJson(
                    QDir(filePath).filePath(QStringLiteral("symbols/") + safeName(baseKey) + QStringLiteral("-") +
                                            QString::number(part + 1) + QStringLiteral(".json")),
                    symbolJson(symbol, baseKey, part, unitUuid, m_diagnostics),
                    m_diagnostics))
                return false;
            const QString gateUuid =
                HorizonUuid::make(QStringLiteral("gate"), baseKey + QStringLiteral("/part/") + QString::number(part));
            gates.insert(
                gateUuid,
                QJsonObject{{QStringLiteral("name"),
                             symbol.partCount > 1 ? QStringLiteral("Part %1").arg(part + 1) : QStringLiteral("Main")},
                            {QStringLiteral("suffix"), QString()},
                            {QStringLiteral("swap_group"), 0},
                            {QStringLiteral("unit"), unitUuid}});
        }
        const QString entityUuid = HorizonUuid::make(QStringLiteral("entity"), symbol.name);
        if (!writeJson(
                QDir(filePath).filePath(QStringLiteral("entities/") + safeName(symbol.name) + QStringLiteral(".json")),
                QJsonObject{{QStringLiteral("type"), QStringLiteral("entity")},
                            {QStringLiteral("uuid"), entityUuid},
                            {QStringLiteral("name"), symbol.name},
                            {QStringLiteral("manufacturer"), QString()},
                            {QStringLiteral("prefix"), symbol.designatorPrefix},
                            {QStringLiteral("gates"), gates},
                            {QStringLiteral("tags"), QJsonArray{}}},
                m_diagnostics))
            return false;
    }
    return true;
}

bool ExporterHorizonLibrary::exportComponentLibrary(const QList<IR::ComponentIR>& components,
                                                    const QString& libName,
                                                    const QString& filePath,
                                                    bool exportModel3D,
                                                    const QString& model3DBaseDir) {
    m_diagnostics.clear();
    if (components.isEmpty()) {
        m_diagnostics.append(QStringLiteral("Horizon: 没有可导出的完整组件"));
        return false;
    }
    QHash<QString, QString> componentNameOwners;
    QList<IR::FootprintComponentIR> footprints;
    footprints.reserve(components.size());
    for (const auto& component : components) {
        const QString baseKey = component.name.isEmpty() ? component.symbol.name : component.name;
        if (!validateSafeName(componentNameOwners, baseKey, QStringLiteral("组件"), m_diagnostics))
            return false;
        footprints.append(component.footprint);
    }
    if (!validateFootprintOutputNames(footprints, m_diagnostics))
        return false;
    if (!ensureDirs(filePath, m_diagnostics) || !writePoolInfo(filePath, libName, m_diagnostics))
        return false;
    for (const auto& component : components) {
        if (!writeComponent(component, filePath, exportModel3D, model3DBaseDir, m_diagnostics))
            return false;
    }
    return true;
}

}  // namespace EasyKiConverter
