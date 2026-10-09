/**
 * @file ExporterHorizonLibrary.cpp
 * @brief 将统一 IR 导出为 Horizon Pool 源文件及其关联数据。
 */

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
#include <limits>

namespace EasyKiConverter {
namespace {

/** @brief Horizon 层编号与 IR 层语义分开维护，避免目标格式数值泄漏到统一 IR。 */
constexpr int kTopCopper = 0;  ///< 顶层铜层。
constexpr int kInnerCopper1 = -1;  ///< 第一内层铜层。
constexpr int kInnerCopper2 = -2;  ///< 第二内层铜层。
constexpr int kInnerCopper3 = -3;  ///< 第三内层铜层。
constexpr int kInnerCopper4 = -4;  ///< 第四内层铜层。
constexpr int kBottomCopper = -100;  ///< 底层铜层。
constexpr int kTopMask = 10;  ///< 顶层阻焊层。
constexpr int kBottomMask = -110;  ///< 底层阻焊层。
constexpr int kTopSilk = 20;  ///< 顶层丝印层。
constexpr int kBottomSilk = -120;  ///< 底层丝印层。
constexpr int kTopPaste = 30;  ///< 顶层锡膏层。
constexpr int kBottomPaste = -130;  ///< 底层锡膏层。
constexpr int kTopPackage = 40;  ///< 顶层封装图形层。
constexpr int kBottomPackage = -140;  ///< 底层封装图形层。
constexpr int kTopAssembly = 50;  ///< 顶层装配层。
constexpr int kBottomAssembly = -150;  ///< 底层装配层。
constexpr int kOutline = 100;  ///< 封装外形层。
constexpr int kInvalidPackageLayer = std::numeric_limits<int>::min();  ///< 无法保真表达的图元层哨兵值。

/**
 * @brief 将名称清理为安全、稳定的文件名，原始名称仍保留在对象和诊断中。
 * @param value 待清理的原始名称。
 * @return 清理后的稳定文件名。
 */
QString safeName(QString value) {
    value = value.trimmed();
    QString result;  ///< 清理后的文件名。
    for (const QChar c : value) {  ///< 当前遍历的 Unicode 字符。
        if (c.isLetterOrNumber() || c == QLatin1Char('_') || c == QLatin1Char('-') || c == QLatin1Char('.') ||
            c == QLatin1Char(' '))
            result.append(c);
        else
            result.append(QLatin1Char('_'));
    }
    result = result.simplified();
    return result.isEmpty() ? QStringLiteral("unnamed") : result.left(120);
}

/**
 * @brief 判断焊盘是否为无镀层机械通孔。
 * @param pad 待判断的焊盘。
 * @return 如果焊盘是无镀层通孔则返回 true。
 */
bool isMechanicalHolePad(const IR::FootprintPadIR& pad) {
    return pad.isThroughHole() && !pad.isPlated;
}

/**
 * @brief 生成焊盘文件名主干，并为无编号机械孔生成稳定名称。
 * @param pad 待生成名称的焊盘。
 * @param index 焊盘在当前封装中的索引。
 * @return 可用于输出文件名的名称主干。
 */
QString padFileStem(const IR::FootprintPadIR& pad, int index) {
    if (isMechanicalHolePad(pad) && pad.number.trimmed().isEmpty())
        return QStringLiteral("NPTH%1").arg(index + 1);
    return safeName(pad.number);
}

/**
 * @brief 检查清洗后的名称是否会覆盖已有输出文件。
 * @param owners 已登记的清洗名称及其原始名称。
 * @param sanitizedName 清洗后的名称。
 * @param rawName 当前名称的原始值。
 * @param kind 名称所属的对象类型。
 * @param diagnostics 导出诊断列表。
 * @return 如果名称未产生冲突则返回 true。
 */
bool validateSanitizedName(QHash<QString, QString>& owners,
                           const QString& sanitizedName,
                           const QString& rawName,
                           const QString& kind,
                           QStringList& diagnostics) {
    // 两个原始名称清洗后相同会覆盖同一个文件，必须在写盘前拒绝。
    const auto existing = owners.constFind(sanitizedName);  ///< 已登记的同名记录。
    if (existing != owners.cend() && existing.value() != rawName) {
        diagnostics.append(QStringLiteral("Horizon: %1 名称清洗后输出路径冲突：%2（%3 与 %4）")
                               .arg(kind, sanitizedName, existing.value(), rawName));
        return false;
    }
    owners.insert(sanitizedName, rawName);
    return true;
}

/**
 * @brief 检查原始名称清洗后是否仍保持唯一。
 * @param owners 已登记的清洗名称及其原始名称。
 * @param rawName 当前名称的原始值。
 * @param kind 名称所属的对象类型。
 * @param diagnostics 导出诊断列表。
 * @return 如果名称未产生冲突则返回 true。
 */
bool validateSafeName(QHash<QString, QString>& owners,
                      const QString& rawName,
                      const QString& kind,
                      QStringList& diagnostics) {
    return validateSanitizedName(owners, safeName(rawName), rawName, kind, diagnostics);
}

/**
 * @brief 将毫米坐标转换为 Horizon JSON 坐标数组。
 * @param x 毫米单位的横坐标。
 * @param y 毫米单位的纵坐标。
 * @return Horizon 使用的坐标数组。
 */
QJsonArray point(double x, double y) {
    return {HorizonUnits::mm(x), HorizonUnits::mm(y)};
}

/**
 * @brief 将 Qt 点转换为 Horizon JSON 坐标数组。
 * @param p 待转换的 Qt 点。
 * @return Horizon 使用的坐标数组。
 */
QJsonArray point(const QPointF& p) {
    return point(p.x(), p.y());
}

/**
 * @brief 构造 Horizon 的位置、角度和镜像对象。
 * @param p 平移坐标。
 * @param angle 旋转角度，单位为度。
 * @param mirror 是否镜像。
 * @return Horizon 的布局对象。
 */
QJsonObject placement(const QPointF& p = {}, double angle = 0.0, bool mirror = false) {
    QJsonObject result;  ///< 生成的布局 JSON 对象。
    result.insert(QStringLiteral("shift"), point(p));
    result.insert(QStringLiteral("angle"), qRound64(angle * 65536.0 / 360.0));
    result.insert(QStringLiteral("mirror"), mirror);
    return result;
}

/**
 * @brief 将字符串封装为 Horizon 的变体字段。
 * @param value 待封装的字符串值。
 * @return Horizon 的变体 JSON 数组。
 */
QJsonArray variant(const QString& value) {
    return QJsonArray{false, value};
}

/**
 * @brief 将 IR 引脚电气类型映射为 Horizon 引脚方向类型。
 * @param type IR 引脚电气类型。
 * @param diagnostics 导出诊断列表。
 * @param context 当前引脚的诊断上下文。
 * @return Horizon 引脚方向字符串。
 */
QString pinDirection(IR::PinElectricalType type, QStringList& diagnostics, const QString& context) {
    // Horizon 使用有限的字符串枚举，无法直接表达的类型必须进入诊断。
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

/**
 * @brief 将 IR 引脚方向映射为 Horizon 符号方向字符串。
 * @param direction IR 引脚方向。
 * @return Horizon 符号方向字符串。
 */
QString symbolOrientation(IR::PinDirection direction) {
    // 目标格式只接受四个正交方向，未知值按 right 处理。
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

/**
 * @brief 将 IR 图层映射为 Horizon 封装图层编号。
 * @param layer IR 图层语义。
 * @param diagnostics 导出诊断列表。
 * @param context 当前图元的诊断上下文。
 * @return Horizon 封装图层编号，无法表达时返回无效层哨兵值。
 */
int packageLayer(IR::LayerType layer, QStringList& diagnostics, const QString& context) {
    // KeepOut 不得回退为铜层；只有原生 Region keepout 由调用方单独序列化。
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
                QStringLiteral("Horizon: KeepOut 图元不能映射为普通封装图层；仅支持区域图元的原生 keepouts：%1")
                    .arg(context));
            return kInvalidPackageLayer;
        case IR::LayerType::EdgeCuts:
            return kOutline;
        default:
            diagnostics.append(QStringLiteral("Horizon: 不支持的层降级为 top package：%1").arg(context));
            return kTopPackage;
    }
}

/**
 * @brief 拒绝无法序列化为原生 keepout 的封装图元。
 * @param footprint 待检查的封装 IR。
 * @param diagnostics 导出诊断列表。
 * @return 如果所有图元都可安全表达则返回 true。
 */
bool validateGenericKeepOutLayers(const IR::FootprintComponentIR& footprint, QStringList& diagnostics) {
    const auto reject = [&diagnostics, &footprint](const QString& primitive,
                                                   int index) {  ///< 记录不支持图元并拒绝导出。
        diagnostics.append(
            QStringLiteral(
                "Horizon: 封装 %1 的第 %2 个 %3 使用 KeepOut 层；该图元没有等价的原生 keepout 表达，已拒绝导出")
                .arg(footprint.name)
                .arg(index + 1)
                .arg(primitive));
        return false;
    };
    for (int i = 0; i < footprint.pads.size(); ++i) {  ///< 当前焊盘索引。
        if (footprint.pads.at(i).layer == IR::LayerType::KeepOut)
            return reject(QStringLiteral("Pad"), i);
    }
    for (int i = 0; i < footprint.outlines.size(); ++i) {  ///< 当前外形索引。
        if (footprint.outlines.at(i).layer == IR::LayerType::KeepOut)
            return reject(QStringLiteral("Outline"), i);
    }
    for (int i = 0; i < footprint.tracks.size(); ++i) {  ///< 当前走线索引。
        if (footprint.tracks.at(i).layer == IR::LayerType::KeepOut)
            return reject(QStringLiteral("Track"), i);
    }
    for (int i = 0; i < footprint.circles.size(); ++i) {  ///< 当前圆形图元索引。
        if (footprint.circles.at(i).layer == IR::LayerType::KeepOut)
            return reject(QStringLiteral("Circle"), i);
    }
    for (int i = 0; i < footprint.rectangles.size(); ++i) {  ///< 当前矩形图元索引。
        if (footprint.rectangles.at(i).layer == IR::LayerType::KeepOut)
            return reject(QStringLiteral("Rectangle"), i);
    }
    for (int i = 0; i < footprint.arcs.size(); ++i) {  ///< 当前圆弧索引。
        if (footprint.arcs.at(i).layer == IR::LayerType::KeepOut)
            return reject(QStringLiteral("Arc"), i);
    }
    for (int i = 0; i < footprint.texts.size(); ++i) {  ///< 当前文本索引。
        const auto& text = footprint.texts.at(i);  ///< 当前待检查的文本图元。
        if (text.isDisplayed && text.layer == IR::LayerType::KeepOut)
            return reject(QStringLiteral("Text"), i);
    }
    for (int i = 0; i < footprint.regions.size(); ++i) {  ///< 当前区域索引。
        const auto& region = footprint.regions.at(i);  ///< 当前待检查的区域图元。
        if (!region.isKeepOut && region.layer == IR::LayerType::KeepOut)
            return reject(QStringLiteral("Region"), i);
    }
    return true;
}

/**
 * @brief 创建 Horizon 几何映射字段的空对象。
 * @return 包含各类几何映射空表的 JSON 对象。
 */
QJsonObject emptyMaps() {
    return QJsonObject{{QStringLiteral("junctions"), QJsonObject{}},
                       {QStringLiteral("lines"), QJsonObject{}},
                       {QStringLiteral("arcs"), QJsonObject{}},
                       {QStringLiteral("polygons"), QJsonObject{}},
                       {QStringLiteral("texts"), QJsonObject{}}};
}

/**
 * @brief 以原子提交方式写入一个 Horizon JSON 文件。
 * @param path 输出文件路径。
 * @param object 待序列化的 JSON 对象。
 * @param diagnostics 导出诊断列表。
 * @return 文件成功提交时返回 true。
 */
bool writeJson(const QString& path, const QJsonObject& object, QStringList& diagnostics) {
    QSaveFile file(path);  ///< 用于原子提交的输出文件。
    if (!file.open(QIODevice::WriteOnly)) {
        diagnostics.append(QStringLiteral("Horizon: 无法写入文件：%1").arg(path));
        return false;
    }
    const QByteArray data = QJsonDocument(object).toJson(QJsonDocument::Indented);  ///< 格式化后的 JSON 数据。
    if (file.write(data) != data.size() || !file.commit()) {
        diagnostics.append(QStringLiteral("Horizon: 文件提交失败：%1").arg(path));
        return false;
    }
    return true;
}

/**
 * @brief 以原子提交方式写入一个三维模型文件。
 * @param path 输出模型文件路径。
 * @param data 待写入的二进制模型数据。
 * @param diagnostics 导出诊断列表。
 * @return 文件成功提交时返回 true。
 */
bool writeBinary(const QString& path, const QByteArray& data, QStringList& diagnostics) {
    QSaveFile file(path);  ///< 用于原子提交的模型文件。
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit()) {
        diagnostics.append(QStringLiteral("Horizon: 无法提交 3D 模型文件：%1").arg(path));
        return false;
    }
    return true;
}

/**
 * @brief 创建 Horizon Pool 目录骨架，调用方必须先完成输入校验。
 * @param root Pool 根目录。
 * @param diagnostics 导出诊断列表。
 * @return 所有必需目录创建成功时返回 true。
 */
bool ensureDirs(const QString& root, QStringList& diagnostics) {
    // Horizon Pool 所需的固定子目录名称。
    const QStringList names = {QStringLiteral("units"),
                               QStringLiteral("symbols"),
                               QStringLiteral("entities"),
                               QStringLiteral("padstacks"),
                               QStringLiteral("packages"),
                               QStringLiteral("parts"),
                               QStringLiteral("3d_models")};
    for (const QString& name : names) {  ///< 当前待创建的 Pool 子目录名称。
        if (!QDir().mkpath(QDir(root).filePath(name))) {
            diagnostics.append(QStringLiteral("Horizon: 无法创建 Pool 目录：%1").arg(QDir(root).filePath(name)));
            return false;
        }
    }
    return true;
}

/**
 * @brief 写入一条线段及其两个端点。
 * @param lines 线段 JSON 对象集合。
 * @param junctions 端点 Junction JSON 对象集合。
 * @param semantic 当前图元的稳定语义标识。
 * @param from 线段起点坐标。
 * @param to 线段终点坐标。
 * @param layer Horizon 目标层编号。
 * @param width 线段宽度，单位为毫米。
 */
void addLine(QJsonObject& lines,
             QJsonObject& junctions,
             const QString& semantic,
             const QPointF& from,
             const QPointF& to,
             int layer,
             double width) {
    // 线段起点 Junction UUID。
    const QString fromUuid = HorizonUuid::make(QStringLiteral("junction"), semantic + QStringLiteral("/from"));
    // 线段终点 Junction UUID。
    const QString toUuid = HorizonUuid::make(QStringLiteral("junction"), semantic + QStringLiteral("/to"));
    const QString lineUuid = HorizonUuid::make(QStringLiteral("line"), semantic);  ///< 线段自身 UUID。
    junctions.insert(fromUuid, QJsonObject{{QStringLiteral("position"), point(from)}});
    junctions.insert(toUuid, QJsonObject{{QStringLiteral("position"), point(to)}});
    lines.insert(lineUuid,
                 QJsonObject{{QStringLiteral("from"), fromUuid},
                             {QStringLiteral("to"), toUuid},
                             {QStringLiteral("width"), HorizonUnits::mm(width)},
                             {QStringLiteral("layer"), layer}});
}

/**
 * @brief 写入一段圆弧及其起点、终点和圆心。
 * @param arcs 圆弧 JSON 对象集合。
 * @param junctions 圆弧端点和圆心的 Junction JSON 对象集合。
 * @param semantic 当前图元的稳定语义标识。
 * @param from 圆弧起点坐标。
 * @param to 圆弧终点坐标。
 * @param center 圆弧圆心坐标。
 * @param layer Horizon 目标层编号。
 * @param width 圆弧宽度，单位为毫米。
 */
void addArc(QJsonObject& arcs,
            QJsonObject& junctions,
            const QString& semantic,
            const QPointF& from,
            const QPointF& to,
            const QPointF& center,
            int layer,
            double width) {
    // 圆弧起点 Junction UUID。
    const QString fromUuid = HorizonUuid::make(QStringLiteral("junction"), semantic + QStringLiteral("/from"));
    // 圆弧终点 Junction UUID。
    const QString toUuid = HorizonUuid::make(QStringLiteral("junction"), semantic + QStringLiteral("/to"));
    // 圆弧圆心 Junction UUID。
    const QString centerUuid = HorizonUuid::make(QStringLiteral("junction"), semantic + QStringLiteral("/center"));
    const QString arcUuid = HorizonUuid::make(QStringLiteral("arc"), semantic);  ///< 圆弧自身 UUID。
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

/**
 * @brief 将圆离散为四段圆弧写入 Horizon 几何映射。
 * @param arcs 圆弧 JSON 对象集合。
 * @param junctions 圆弧端点 Junction JSON 对象集合。
 * @param semantic 当前圆形图元的稳定语义标识。
 * @param center 圆心坐标。
 * @param radius 半径，单位为毫米。
 * @param layer Horizon 目标层编号。
 * @param width 圆弧宽度，单位为毫米。
 */
void addCircle(QJsonObject& arcs,
               QJsonObject& junctions,
               const QString& semantic,
               const QPointF& center,
               double radius,
               int layer,
               double width) {
    // 四段圆弧使用的顺序顶点。
    const QList<QPointF> points = {center + QPointF(radius, 0.0),
                                   center + QPointF(0.0, radius),
                                   center + QPointF(-radius, 0.0),
                                   center + QPointF(0.0, -radius)};
    for (int i = 0; i < points.size(); ++i) {  ///< 当前圆弧段索引。
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

/**
 * @brief 将多边形顶点写入 Horizon Polygon 映射。
 * @param polygons 多边形 JSON 对象集合。
 * @param semantic 当前多边形的稳定语义标识。
 * @param points 多边形顶点坐标，按边界顺序排列。
 * @param layer Horizon 目标层编号。
 */
void addPolygon(QJsonObject& polygons, const QString& semantic, const QList<QPointF>& points, int layer) {
    QJsonArray vertices;  ///< 多边形顶点 JSON 数组。
    for (const QPointF& p : points) {  ///< 当前待写入的多边形顶点。
        vertices.append(QJsonObject{{QStringLiteral("type"), QStringLiteral("line")},
                                    {QStringLiteral("position"), point(p)},
                                    {QStringLiteral("arc_center"), point(0.0, 0.0)},
                                    {QStringLiteral("arc_reverse"), false}});
    }
    const QString polygonUuid = HorizonUuid::make(QStringLiteral("polygon"), semantic);  ///< 多边形自身 UUID。
    polygons.insert(polygonUuid, QJsonObject{{QStringLiteral("vertices"), vertices}, {QStringLiteral("layer"), layer}});
}

/**
 * @brief 将椭圆或椭圆弧离散为可序列化的折线顶点。
 * @param center 椭圆中心坐标。
 * @param radiusX 横向半径，单位为毫米。
 * @param radiusY 纵向半径，单位为毫米。
 * @param startAngle 起始角度，单位为度。
 * @param endAngle 结束角度，单位为度。
 * @param segments 完整椭圆对应的采样段数。
 * @return 离散后的折线顶点。
 */
QList<QPointF> ellipsePoints(const QPointF& center,
                             double radiusX,
                             double radiusY,
                             double startAngle,
                             double endAngle,
                             int segments = 32) {
    QList<QPointF> result;  ///< 离散后的椭圆顶点。
    const double delta = endAngle - startAngle;  ///< 椭圆弧的角度跨度。
    const int count = qMax(2, qCeil(qAbs(delta) / 360.0 * segments));  ///< 实际采样段数。
    for (int i = 0; i <= count; ++i) {  ///< 当前采样点索引。
        const double angle = qDegreesToRadians(startAngle + delta * i / count);  ///< 当前采样点的弧度角。
        result.append(center + QPointF(radiusX * qCos(angle), radiusY * qSin(angle)));
    }
    return result;
}

/**
 * @brief 将四控制点三次贝塞尔曲线离散为折线顶点。
 * @param controlPoints 三次贝塞尔曲线的四个控制点。
 * @param segments 曲线采样段数。
 * @return 离散后的折线顶点。
 */
QList<QPointF> cubicBezierPoints(const QList<QPointF>& controlPoints, int segments = 16) {
    QList<QPointF> result;  ///< 离散后的贝塞尔曲线顶点。
    if (controlPoints.size() < 4)
        return result;
    const QPointF& p0 = controlPoints.at(0);  ///< 曲线起点控制点。
    const QPointF& p1 = controlPoints.at(1);  ///< 曲线第一控制点。
    const QPointF& p2 = controlPoints.at(2);  ///< 曲线第二控制点。
    const QPointF& p3 = controlPoints.at(3);  ///< 曲线终点控制点。
    for (int i = 0; i <= segments; ++i) {  ///< 当前采样点索引。
        const double t = static_cast<double>(i) / segments;  ///< 当前采样点的归一化位置。
        const double u = 1.0 - t;  ///< 当前采样点的反向归一化位置。
        result.append(u * u * u * p0 + 3.0 * u * u * t * p1 + 3.0 * u * t * t * p2 + t * t * t * p3);
    }
    return result;
}

/**
 * @brief 将连续点序列拆分为多条线段。
 * @param lines 线段 JSON 对象集合。
 * @param junctions 端点 Junction JSON 对象集合。
 * @param semantic 当前折线的稳定语义标识。
 * @param points 折线顶点坐标。
 * @param layer Horizon 目标层编号。
 * @param width 线段宽度，单位为毫米。
 */
void addPolyline(QJsonObject& lines,
                 QJsonObject& junctions,
                 const QString& semantic,
                 const QList<QPointF>& points,
                 int layer,
                 double width) {
    for (int i = 1; i < points.size(); ++i) {  ///< 当前线段终点索引。
        addLine(lines,
                junctions,
                semantic + QStringLiteral("/") + QString::number(i),
                points.at(i - 1),
                points.at(i),
                layer,
                width);
    }
}

/**
 * @brief 绕指定中心旋转一个几何点。
 * @param point 待旋转的几何点。
 * @param center 旋转中心。
 * @param angle 旋转角度，单位为度。
 * @return 旋转后的几何点。
 */
QPointF rotateAround(const QPointF& point, const QPointF& center, double angle) {
    // 围绕指定中心执行旋转的变换矩阵。
    const QTransform transform =
        QTransform().translate(center.x(), center.y()).rotate(angle).translate(-center.x(), -center.y());
    return transform.map(point);
}

/**
 * @brief 生成一个符号 Unit 的 Horizon JSON。
 * @param symbol 符号组件 IR。
 * @param uuidBase 当前组件的稳定 UUID 基础。
 * @param part 当前 Part 索引。
 * @param unitUuid 当前 Unit UUID。
 * @param diagnostics 导出诊断列表。
 * @return 序列化后的 Unit JSON 对象。
 */
QJsonObject unitJson(const IR::SymbolComponentIR& symbol,
                     const QString& uuidBase,
                     int part,
                     const QString& unitUuid,
                     QStringList& diagnostics) {
    QJsonObject pins;  ///< 当前符号 Unit 的引脚映射对象。
    for (int i = 0; i < symbol.pins.size(); ++i) {
        const IR::SymbolPinIR& pin = symbol.pins.at(i);  ///< 当前 Part 的符号引脚。
        if (pin.designator.trimmed().isEmpty()) {
            diagnostics.append(
                QStringLiteral("Horizon: 符号 %1 Part %2 存在空引脚编号").arg(symbol.name).arg(part + 1));
            continue;
        }
        if (!pin.commonToAllParts && pin.partIndex != part)
            continue;
        // 当前符号引脚的稳定 UUID。
        // 为符号引脚生成与 Part 和编号绑定的稳定 UUID。
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

/**
 * @brief 生成一个符号图形及引脚几何的 Horizon JSON。
 * @param symbol 符号组件 IR。
 * @param uuidBase 当前组件的稳定 UUID 基础。
 * @param part 当前 Part 索引。
 * @param unitUuid 当前 Unit UUID。
 * @param diagnostics 导出诊断列表。
 * @return 序列化后的符号 JSON 对象。
 */
QJsonObject symbolJson(const IR::SymbolComponentIR& symbol,
                       const QString& uuidBase,
                       int part,
                       const QString& unitUuid,
                       QStringList& diagnostics) {
    QJsonObject result = emptyMaps();  ///< 符号 JSON 的基础对象集合。
    result.insert(QStringLiteral("type"), QStringLiteral("symbol"));
    result.insert(QStringLiteral("name"),
                  symbol.name + (symbol.partCount > 1 ? QStringLiteral("_Part%1").arg(part + 1) : QString()));
    // 当前 Part 的稳定符号 UUID。
    const QString symbolUuid =
        HorizonUuid::make(QStringLiteral("symbol"), symbol.name + QStringLiteral("/part/") + QString::number(part));
    result.insert(QStringLiteral("uuid"), symbolUuid);
    result.insert(QStringLiteral("unit"), unitUuid);
    result.insert(QStringLiteral("can_expand"), false);

    QJsonObject junctions = result[QStringLiteral("junctions")].toObject();  ///< 符号连接点集合。
    QJsonObject lines = result[QStringLiteral("lines")].toObject();  ///< 符号线段集合。
    QJsonObject pins = result[QStringLiteral("pins")].toObject();  ///< 符号引脚集合。
    QJsonObject polygons = result[QStringLiteral("polygons")].toObject();  ///< 符号多边形集合。
    QJsonObject texts = result[QStringLiteral("texts")].toObject();  ///< 符号文本集合。

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
        const IR::SymbolPinIR& pin = symbol.pins.at(i);  ///< 当前 Part 的符号引脚。
        if (!pin.commonToAllParts && pin.partIndex != part)
            continue;
        // 当前符号引脚的稳定 UUID。
        const QString pinUuid = HorizonUuid::make(QStringLiteral("unit-pin"),
                                                  uuidBase + QStringLiteral("/part/") + QString::number(part) +
                                                      QStringLiteral("/designator/") + pin.designator);
        QJsonObject value{{QStringLiteral("position"), point(pin.position)},
                          {QStringLiteral("length"), HorizonUnits::mm(pin.length)},
                          {QStringLiteral("orientation"), symbolOrientation(pin.direction)},
                          {QStringLiteral("name_visible"), pin.display.showName && pin.showName},
                          {QStringLiteral("pad_visible"), pin.display.showDesignator && pin.showDesignator}};
        QString driver = QStringLiteral("default");  ///< 引脚输出驱动类型。
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
        const auto& r = symbol.rectangles.at(i);  ///< 当前符号矩形图元。
        if (r.partIndex == part)
            addRectangle(QStringLiteral("rectangle/"), r.x0, r.y0, r.x1, r.y1, i);
    }
    for (int i = 0; i < symbol.polylines.size(); ++i) {
        const auto& poly = symbol.polylines.at(i);  ///< 当前符号折线图元。
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
        const auto& poly = symbol.polygons.at(i);  ///< 当前符号多边形图元。
        if (poly.partIndex == part)
            addPolygon(polygons, symbol.name + QStringLiteral("/polygon/") + QString::number(i), poly.points, 0);
    }
    for (int i = 0; i < symbol.paths.size(); ++i) {
        const auto& path = symbol.paths.at(i);  ///< 当前符号路径图元。
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
                const auto& segment = path.segments.at(j);  ///< 当前路径线段。
                QList<QPointF> points = {segment.start, segment.end};  ///< 当前路径线段离散后的顶点。
                if (segment.type == IR::SymbolPathSegmentIR::Type::CubicBezier)
                    points = cubicBezierPoints({segment.start, segment.control1, segment.control2, segment.end});
                /**
                 * @brief 二次贝塞尔曲线没有直接的 Horizon primitive，离散为折线保存形状。
                 * @param type 对象或数据类型。
                 */
                else if (segment.type == IR::SymbolPathSegmentIR::Type::QuadraticBezier) {
                    points.clear();
                    for (int k = 0; k <= 16; ++k) {
                        const double t = static_cast<double>(k) / 16.0;  ///< 二次贝塞尔曲线的归一化参数。
                        const double u = 1.0 - t;  ///< 二次贝塞尔曲线的反向权重。
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
        const auto& bezier = symbol.beziers.at(i);  ///< 当前符号贝塞尔图元。
        if (bezier.partIndex == part)
            addPolyline(lines,
                        junctions,
                        symbol.name + QStringLiteral("/bezier/") + QString::number(i),
                        cubicBezierPoints(bezier.controlPoints),
                        0,
                        bezier.strokeWidth);
    }
    for (int i = 0; i < symbol.circles.size(); ++i) {
        const auto& circle = symbol.circles.at(i);  ///< 当前符号圆形图元。
        if (circle.partIndex == part)
            addPolygon(polygons,
                       symbol.name + QStringLiteral("/circle/") + QString::number(i),
                       ellipsePoints(circle.center, circle.radius, circle.radius, 0.0, 360.0),
                       0);
    }
    for (int i = 0; i < symbol.ellipses.size(); ++i) {
        const auto& ellipse = symbol.ellipses.at(i);  ///< 当前符号椭圆图元。
        if (ellipse.partIndex == part)
            addPolygon(polygons,
                       symbol.name + QStringLiteral("/ellipse/") + QString::number(i),
                       ellipsePoints(ellipse.center, ellipse.radiusX, ellipse.radiusY, 0.0, 360.0),
                       0);
    }
    for (int i = 0; i < symbol.arcs.size(); ++i) {
        const auto& arc = symbol.arcs.at(i);  ///< 当前符号圆弧图元。
        if (arc.partIndex == part)
            addPolyline(lines,
                        junctions,
                        symbol.name + QStringLiteral("/arc/") + QString::number(i),
                        {arc.startPoint, arc.midPoint, arc.endPoint},
                        0,
                        arc.strokeWidth);
    }
    for (int i = 0; i < symbol.pies.size(); ++i) {
        const auto& pie = symbol.pies.at(i);  ///< 当前符号扇形图元。
        if (pie.partIndex == part) {
            QList<QPointF> points =  ///< 当前扇形离散后的边界顶点。
                ellipsePoints(pie.center, pie.radius, pie.radius, pie.startAngle, pie.endAngle);
            points.prepend(pie.center);
            addPolygon(polygons, symbol.name + QStringLiteral("/pie/") + QString::number(i), points, 0);
        }
    }
    for (int i = 0; i < symbol.ellipticalArcs.size(); ++i) {
        const auto& arc = symbol.ellipticalArcs.at(i);  ///< 当前符号椭圆弧图元。
        if (arc.partIndex == part)
            addPolyline(lines,
                        junctions,
                        symbol.name + QStringLiteral("/elliptical-arc/") + QString::number(i),
                        ellipsePoints(arc.center, arc.radiusX, arc.radiusY, arc.startAngle, arc.endAngle),
                        0,
                        arc.strokeWidth);
    }
    for (int i = 0; i < symbol.texts.size(); ++i) {
        const auto& text = symbol.texts.at(i);  ///< 当前符号文本图元。
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
        const auto& frame = symbol.textFrames.at(i);  ///< 当前符号文本框图元。
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

/**
 * @brief 将 IR 焊盘序列化为 Horizon Padstack JSON。
 * @param pad 待序列化的焊盘 IR。
 * @param uuid Padstack 稳定 UUID。
 * @param diagnostics 导出诊断列表。
 * @return 序列化后的 Padstack JSON 对象；无法表达时返回空对象。
 */
QJsonObject padstackJson(const IR::FootprintPadIR& pad, const QString& uuid, QStringList& diagnostics) {
    const QString context = pad.number;  ///< 用于诊断的焊盘编号上下文。
    QJsonObject shapes;  ///< Padstack 的形状定义集合。
    QJsonObject polygons;  ///< Padstack 的多边形 Shape 集合。
    QJsonObject parameterSet;  ///< Padstack 参数集合。
    QStringList parameterProgram;  ///< Padstack 参数化程序片段。
    const int layer = packageLayer(pad.layer, diagnostics, context);  ///< 焊盘主层编号。
    if (layer == kInvalidPackageLayer)
        return {};
    // 只有具备足够顶点的异形焊盘才使用 Polygon 输出路径。
    const bool hasCustomPolygon = (pad.shape == IR::PadShape::Polygon || pad.shape == IR::PadShape::RoundRect ||
                                   pad.shape == IR::PadShape::Trapezoid) &&
                                  pad.customShapePoints.size() >= 3;  ///< 是否有足够顶点的自定义 Pad 轮廓。
    QList<QPointF> rotatedCustomPoints;  ///< 经过焊盘旋转后的自定义 Shape 顶点。
    if (hasCustomPolygon) {
        rotatedCustomPoints.reserve(pad.customShapePoints.size());
        for (const QPointF& item : pad.customShapePoints)
            rotatedCustomPoints.append(rotateAround(item, {}, pad.rotation));
    }
    const QList<int> throughCopperLayers = {///< 通孔 Padstack 使用的铜层编号。
                                            kTopCopper,
                                            kInnerCopper1,
                                            kInnerCopper2,
                                            kInnerCopper3,
                                            kInnerCopper4,
                                            kBottomCopper};
    QString form = QStringLiteral("rectangle");  ///< Horizon Padstack 形状类型。
    QJsonArray params{HorizonUnits::mm(pad.size.width()), HorizonUnits::mm(pad.size.height())};  ///< 形状参数数组。
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
            QJsonArray vertices;  ///< 当前 Padstack 多边形的顶点数组。
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
        QString line = QStringLiteral("expand-polygon [ ") + parameterClass;  ///< 当前多边形参数程序文本。
        for (const QPointF& item : points)
            line += QStringLiteral(" %1 %2").arg(HorizonUnits::mm(item.x())).arg(HorizonUnits::mm(item.y()));
        line += QStringLiteral(" ]");
        parameterProgram.append(line);
    };
    if (!hasCustomPolygon) {
        const QString parameterClass =
            pad.isSmd() ? QStringLiteral("pad") : QStringLiteral("copper");  ///< 参数形状类别。
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
        const int maskLayer = layer == kBottomCopper ? kBottomMask : kTopMask;  ///< SMD 阻焊层编号。
        const int pasteLayer = layer == kBottomCopper ? kBottomPaste : kTopPaste;  ///< SMD 锡膏层编号。
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
        const int maskLayer = layer == kBottomCopper ? kBottomMask : kTopMask;  ///< 自定义 SMD 阻焊层编号。
        const int pasteLayer = layer == kBottomCopper ? kBottomPaste : kTopPaste;  ///< 自定义 SMD 锡膏层编号。
        for (const auto& entry :
             {qMakePair(QStringLiteral("mask"), maskLayer), qMakePair(QStringLiteral("paste"), pasteLayer)}) {
            if ((entry.first == QStringLiteral("mask") && !pad.solderMaskEnabled) ||
                (entry.first == QStringLiteral("paste") && !pad.pasteMaskEnabled))
                continue;
            QJsonArray vertices;  ///< 当前阻焊或锡膏多边形顶点。
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
        const int topMaskLayer = kTopMask;  ///< 顶层阻焊层编号。
        const int bottomMaskLayer = kBottomMask;  ///< 底层阻焊层编号。
        if (hasCustomPolygon && pad.solderMaskEnabled) {
            QList<QPointF> points;  ///< 旋转后的通孔阻焊顶点。
            points.reserve(pad.customShapePoints.size());
            for (const QPointF& item : pad.customShapePoints)
                points.append(rotateAround(item, {}, pad.rotation));
            QJsonArray vertices;  ///< 通孔阻焊多边形顶点。
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
    QJsonObject holes;  ///< Padstack 的钻孔定义集合。
    if (pad.isThroughHole()) {
        const QString holeUuid = HorizonUuid::make(QStringLiteral("padstack-hole"), uuid);  ///< 钻孔 UUID。
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
    // 通过孔、顶层 SMD 和底层 SMD 使用不同的 Padstack 类型。
    const QString padstackType =
        pad.isThroughHole()
            ? QStringLiteral("through")
            : (layer == kBottomCopper ? QStringLiteral("bottom") : QStringLiteral("top"));  ///< Padstack 类型。
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

/**
 * @brief 将独立安装孔序列化为机械 Padstack JSON。
 * @param hole 待序列化的安装孔 IR。
 * @param uuid Padstack 稳定 UUID。
 * @return 序列化后的机械 Padstack JSON 对象。
 */
QJsonObject holePadstackJson(const IR::FootprintHoleIR& hole, const QString& uuid) {
    const QString holeUuid = HorizonUuid::make(QStringLiteral("hole"), uuid);  ///< 安装孔 UUID。
    const qint64 diameter = HorizonUnits::mm(hole.radius * 2.0);  ///< 安装孔直径（Horizon 数据库单位）。
    const QString topMaskUuid =
        HorizonUuid::make(QStringLiteral("hole-mask"), uuid + QStringLiteral("/top"));  ///< 顶层阻焊 UUID。
    const QString bottomMaskUuid =
        HorizonUuid::make(QStringLiteral("hole-mask"), uuid + QStringLiteral("/bottom"));  ///< 底层阻焊 UUID。
    const QJsonObject maskShape = QJsonObject{{QStringLiteral("form"), QStringLiteral("circle")},
                                              {QStringLiteral("parameter_class"), QStringLiteral("mask")},
                                              {QStringLiteral("params"), QJsonArray{diameter}},
                                              {QStringLiteral("placement"), placement()}};
    QJsonObject topMask = maskShape;  ///< 顶层阻焊形状。
    topMask.insert(QStringLiteral("layer"), kTopMask);
    QJsonObject bottomMask = maskShape;  ///< 底层阻焊形状。
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

/**
 * @brief 生成封装几何、焊盘和三维模型引用的 Horizon JSON。
 * @param footprint 封装组件 IR。
 * @param packageUuid 封装稳定 UUID。
 * @param models 关联的三维模型列表。
 * @param padUuids 焊盘编号到 Padstack UUID 的映射。
 * @param diagnostics 导出诊断列表。
 * @return 序列化后的 Package JSON 对象。
 */
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
    QJsonObject pads = result[QStringLiteral("pads")].toObject();  ///< 封装焊盘对象集合。
    for (int i = 0; i < footprint.holes.size(); ++i) {
        const auto& hole = footprint.holes.at(i);  ///< 当前机械安装孔。
        const QString holeName = QStringLiteral("MH%1").arg(i + 1);  ///< 机械孔名称。
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
        const auto& pad = footprint.pads.at(i);  ///< 当前封装焊盘。
        if (pad.number.trimmed().isEmpty() && !isMechanicalHolePad(pad)) {
            diagnostics.append(QStringLiteral("Horizon: 封装 %1 存在空焊盘编号").arg(footprint.name));
            continue;
        }
        if (!pad.number.trimmed().isEmpty() && padUuids.contains(pad.number)) {
            diagnostics.append(QStringLiteral("Horizon: 封装 %1 存在重复焊盘编号 %2").arg(footprint.name, pad.number));
            continue;
        }
        const QString padName = padFileStem(pad, i);  ///< 焊盘文件名主干。
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

    QJsonObject junctions = result[QStringLiteral("junctions")].toObject();  ///< 封装连接点集合。
    QJsonObject lines = result[QStringLiteral("lines")].toObject();  ///< 封装线段集合。
    QJsonObject arcs = result[QStringLiteral("arcs")].toObject();  ///< 封装圆弧集合。
    QJsonObject texts = result[QStringLiteral("texts")].toObject();  ///< 封装文本集合。
    QJsonObject polygons = result[QStringLiteral("polygons")].toObject();  ///< 封装多边形集合。
    QJsonObject keepouts = result[QStringLiteral("keepouts")].toObject();  ///< 封装禁布区集合。
    for (int i = 0; i < footprint.outlines.size(); ++i) {
        const auto& outline = footprint.outlines.at(i);  ///< 当前封装轮廓。
        const int layer = packageLayer(outline.layer, diagnostics, footprint.name);  ///< 轮廓目标图层。
        if (layer == kInvalidPackageLayer)
            return {};
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
        const auto& region = footprint.regions.at(i);  ///< 当前封装区域。
        const QString semantic = footprint.name + QStringLiteral("/region/") + QString::number(i);  ///< 区域语义键。
        const QString polygonUuid = HorizonUuid::make(QStringLiteral("polygon"), semantic);  ///< 区域多边形 UUID。
        // 图层。
        const int regionLayer = region.isKeepOut
                                    ? kTopCopper
                                    : packageLayer(region.layer, diagnostics, footprint.name);  ///< 区域目标图层。
        if (regionLayer == kInvalidPackageLayer)
            return {};
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
        const auto& track = footprint.tracks.at(i);  ///< 当前封装走线。
        const int layer = packageLayer(track.layer, diagnostics, footprint.name);  ///< 走线目标图层。
        if (layer == kInvalidPackageLayer)
            return {};
        addPolyline(lines,
                    junctions,
                    footprint.name + QStringLiteral("/track/") + QString::number(i),
                    track.points,
                    layer,
                    track.width);
    }
    for (int i = 0; i < footprint.circles.size(); ++i) {
        const auto& circle = footprint.circles.at(i);  ///< 当前封装圆形图元。
        const int layer = packageLayer(circle.layer, diagnostics, footprint.name);  ///< 圆形目标图层。
        if (layer == kInvalidPackageLayer)
            return {};
        addCircle(arcs,
                  junctions,
                  footprint.name + QStringLiteral("/circle/") + QString::number(i),
                  circle.center,
                  circle.radius,
                  layer,
                  circle.strokeWidth);
    }
    for (int i = 0; i < footprint.rectangles.size(); ++i) {
        const auto& rectangle = footprint.rectangles.at(i);  ///< 当前封装矩形。
        const QRectF bounds = rectangle.bounds.normalized();  ///< 规范化后的矩形边界。
        const QPointF center = bounds.center();  ///< 矩形中心点。
        QList<QPointF> points = {
            bounds.topLeft(), bounds.topRight(), bounds.bottomRight(), bounds.bottomLeft()};  ///< 旋转前顶点。
        for (QPointF& item : points)
            item = rotateAround(item, center, rectangle.rotation);
        const int layer = packageLayer(rectangle.layer, diagnostics, footprint.name);  ///< 矩形目标图层。
        if (layer == kInvalidPackageLayer)
            return {};
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
        const auto& arc = footprint.arcs.at(i);  ///< 当前封装圆弧。
        const int layer = packageLayer(arc.layer, diagnostics, footprint.name);  ///< 圆弧目标图层。
        if (layer == kInvalidPackageLayer)
            return {};
        addPolyline(lines,
                    junctions,
                    footprint.name + QStringLiteral("/arc/") + QString::number(i),
                    ellipsePoints(arc.center, arc.radius, arc.radius, arc.startAngle, arc.endAngle, 24),
                    layer,
                    arc.width);
    }
    for (int i = 0; i < footprint.texts.size(); ++i) {
        const auto& text = footprint.texts.at(i);  ///< 当前封装文本。
        if (!text.isDisplayed)
            continue;
        const int layer = packageLayer(text.layer, diagnostics, footprint.name);  ///< 文本目标图层。
        if (layer == kInvalidPackageLayer)
            return {};
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
    QJsonObject modelObjects;  ///< 三维模型引用对象集合。
    for (int i = 0; i < models.size(); ++i) {
        const IR::Model3DIR& model = models.at(i);  ///< 当前三维模型。
        const QString modelUuid =
            HorizonUuid::make(QStringLiteral("package-model"), packageUuid + QStringLiteral("/") + QString::number(i));
        const QString suffix = QStringLiteral(".step");  ///< Horizon 三维模型文件后缀。
        const QString filename = QStringLiteral("3d_models/") + modelUuid + suffix;  ///< 模型相对路径。
        const auto translation = model.translation();  ///< 模型平移量。
        const auto stepOffset = model.stepOffsetMm();  ///< STEP 坐标偏移量。
        const auto rotation = model.rotation();  ///< 模型旋转量。
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
                         : HorizonUuid::make(QStringLiteral("package-model"),
                                             packageUuid + QStringLiteral("/0"));  ///< 默认三维模型 UUID。
    result.insert(
        QStringLiteral("default_model"),
        defaultModelUuid.isEmpty() ? QStringLiteral("00000000-0000-0000-0000-000000000000") : defaultModelUuid);
    return result;
}

/**
 * @brief 校验封装焊盘编号，保证 pad_map 的稳定键唯一。
 * @param footprint 待校验的封装组件 IR。
 * @param diagnostics 导出诊断列表。
 * @return 如果焊盘编号有效且唯一则返回 true。
 */
bool validatePadIdentifiers(const IR::FootprintComponentIR& footprint, QStringList& diagnostics) {
    QSet<QString> identifiers;  ///< 已出现的焊盘编号集合。
    bool valid = true;  ///< 当前封装的焊盘编号是否有效。
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

/**
 * @brief 预先检查所有输出名称，避免批量导出时发生文件覆盖。
 * @param footprints 待导出的封装列表。
 * @param diagnostics 导出诊断列表。
 * @return 如果所有输出名称都安全且唯一则返回 true。
 */
bool validateFootprintOutputNames(const QList<IR::FootprintComponentIR>& footprints, QStringList& diagnostics) {
    QHash<QString, QString> footprintNameOwners;  ///< 清理后封装名称到原始名称的映射。
    QHash<QString, QString> padstackNameOwners;  ///< 清理后焊盘名称到原始名称的映射。
    for (const auto& footprint : footprints) {
        if (!validateSafeName(footprintNameOwners, footprint.name, QStringLiteral("封装"), diagnostics))
            return false;
        for (int i = 0; i < footprint.pads.size(); ++i) {
            const auto& pad = footprint.pads.at(i);  ///< 当前待检查焊盘。
            const QString padStem = padFileStem(pad, i);  ///< 焊盘文件名主干。
            const QString rawPadstackName = footprint.name + QStringLiteral("/") + padStem;  ///< 原始 Padstack 名称。
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

/**
 * @brief 在创建目录和写入 Pool 元数据前完成封装级预检。
 * @param footprints 待导出的封装列表。
 * @param diagnostics 导出诊断列表。
 * @return 如果所有封装都通过焊盘和图层校验则返回 true。
 */
bool validateFootprintsBeforeOutput(const QList<IR::FootprintComponentIR>& footprints, QStringList& diagnostics) {
    for (const auto& footprint : footprints) {
        if (!validatePadIdentifiers(footprint, diagnostics) || !validateGenericKeepOutLayers(footprint, diagnostics))
            return false;
    }
    return true;
}

/**
 * @brief 校验并写入封装关联的三维模型文件。
 * @param models 待写入的三维模型列表。
 * @param root Horizon Pool 根目录。
 * @param packageUuid 所属封装稳定 UUID。
 * @param diagnostics 导出诊断列表。
 * @return 所有三维模型成功写入时返回 true。
 */
bool writeModels(const QList<IR::Model3DIR>& models,
                 const QString& root,
                 const QString& packageUuid,
                 QStringList& diagnostics) {
    for (int i = 0; i < models.size(); ++i) {
        const IR::Model3DIR& model = models.at(i);  ///< 当前三维模型。
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
        const QString suffix = QStringLiteral(".step");  ///< Horizon 三维模型文件后缀。
        const QByteArray data = model.stepData();  ///< STEP 文件内容。
        if (!writeBinary(QDir(root).filePath(QStringLiteral("3d_models/") + uuid + suffix), data, diagnostics))
            return false;
    }
    return true;
}

/**
 * @brief 写入完整组件的符号、封装和 Part 关联文件。
 * @param component 待导出的组件 IR。
 * @param root Horizon Pool 根目录。
 * @param exportModel3D 是否导出三维模型。
 * @param modelBaseDir 三维模型源文件基础目录。
 * @param diagnostics 导出诊断列表。
 * @return 组件文件全部写入成功时返回 true。
 */
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
    if (!validateGenericKeepOutLayers(component.footprint, diagnostics))
        return false;
    const IR::SymbolComponentIR& symbol = component.symbol;  ///< 组件关联的符号 IR。
    const QString baseKey = component.name.isEmpty() ? symbol.name : component.name;  ///< 组件稳定名称主键。
    const QString entityUuid = HorizonUuid::make(QStringLiteral("entity"), baseKey);  ///< Entity UUID。
    const QString packageUuid =
        HorizonUuid::make(QStringLiteral("package"), component.footprint.name);  ///< Package UUID。
    const QString partUuid = HorizonUuid::make(QStringLiteral("part"), baseKey);  ///< Part UUID。
    QJsonObject gates;  ///< 组件 Gate 集合。
    QList<QString> unitUuids;  ///< 已生成的 Unit UUID 列表。
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

    QHash<QString, QString> padUuids;  ///< 焊盘编号到 Pad UUID 的映射。
    QList<IR::Model3DIR> models;  ///< 当前组件需要写入的三维模型。
    if (exportModel3D) {
        if (component.model3D.isValid())
            models.append(component.model3D);
        models.append(component.footprint.models3d);
    }
    const QJsonObject package =
        packageJson(component.footprint, packageUuid, models, padUuids, diagnostics);  ///< 封装 JSON。
    if (package.isEmpty())
        return false;
    const QString packageDir =
        QDir(root).filePath(QStringLiteral("packages/") + safeName(component.footprint.name));  ///< 封装目录。
    if (!QDir().mkpath(packageDir) ||
        !writeJson(QDir(packageDir).filePath(QStringLiteral("package.json")), package, diagnostics))
        return false;
    if (!writeModels(models, root, packageUuid, diagnostics))
        return false;
    for (int i = 0; i < component.footprint.pads.size(); ++i) {
        const auto& pad = component.footprint.pads.at(i);  ///< 当前组件封装焊盘。
        const QString padStem = padFileStem(pad, i);  ///< 焊盘文件名主干。
        const QString padstackUuid =
            HorizonUuid::make(QStringLiteral("padstack"), component.footprint.name + QStringLiteral("/") + padStem);
        const QJsonObject padstack = padstackJson(pad, padstackUuid, diagnostics);  ///< Padstack JSON。
        if (padstack.isEmpty())
            return false;
        if (!writeJson(QDir(root).filePath(QStringLiteral("padstacks/") + safeName(component.footprint.name) +
                                           QStringLiteral("-") + padStem + QStringLiteral(".json")),
                       padstack,
                       diagnostics))
            return false;
    }
    for (int i = 0; i < component.footprint.holes.size(); ++i) {
        const QString holeName = QStringLiteral("MH%1").arg(i + 1);  ///< 机械孔名称。
        // 独立机械孔使用独立 Padstack UUID，避免与电气焊盘混用。
        const QString holeUuid = HorizonUuid::make(QStringLiteral("hole-padstack"),
                                                   component.footprint.name + QStringLiteral("/") + holeName);
        if (!writeJson(QDir(root).filePath(QStringLiteral("padstacks/") + safeName(component.footprint.name) +
                                           QStringLiteral("-") + holeName + QStringLiteral(".json")),
                       holePadstackJson(component.footprint.holes.at(i), holeUuid),
                       diagnostics))
            return false;
    }

    QJsonObject padMap;  ///< 符号引脚到封装焊盘的映射。
    bool pinPadMappingValid = true;  ///< 引脚与焊盘关联是否完整。
    QHash<QString, int> pinOccurrences;  ///< 符号引脚编号出现次数。
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
        const IR::SymbolPinIR* matched = nullptr;  ///< 当前焊盘匹配到的符号引脚。
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
        // 当前元件部分。
        const int part = matched->commonToAllParts
                             ? 0
                             : qBound(0, matched->partIndex, qMax(0, symbol.partCount - 1));  ///< 引脚所属 Part。
        const QString gateUuid =
            HorizonUuid::make(QStringLiteral("gate"), baseKey + QStringLiteral("/part/") + QString::number(part));
        // 为符号引脚生成与 Part 和编号绑定的稳定 UUID。
        const QString pinUuid = HorizonUuid::make(QStringLiteral("unit-pin"),
                                                  baseKey + QStringLiteral("/part/") + QString::number(part) +
                                                      QStringLiteral("/designator/") + matched->designator);
        padMap.insert(padUuids.value(pad.number),
                      QJsonObject{{QStringLiteral("gate"), gateUuid}, {QStringLiteral("pin"), pinUuid}});
    }
    for (const auto& pin : symbol.pins) {
        if (pin.designator.trimmed().isEmpty())
            continue;
        bool foundPad = false;  ///< 是否已找到对应的封装焊盘。
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

/**
 * @brief 写入 Horizon 识别 Pool 所需的最小元数据集合。
 * @param root Horizon Pool 根目录。
 * @param name Pool 名称。
 * @param diagnostics 导出诊断列表。
 * @return 元数据和默认 Via 都写入成功时返回 true。
 */
bool writePoolInfo(const QString& root, const QString& name, QStringList& diagnostics) {
    const QString uuid = HorizonUuid::make(QStringLiteral("pool"), name);  ///< Pool UUID。
    const QString viaUuid = HorizonUuid::make(QStringLiteral("default-via"), name);  ///< 默认 Via UUID。
    const QJsonObject info{{QStringLiteral("uuid"), uuid},
                           {QStringLiteral("default_via"), viaUuid},
                           {QStringLiteral("name"), name},
                           {QStringLiteral("type"), QStringLiteral("pool")},
                           {QStringLiteral("pools_included"), QJsonArray{}}};
    const QString shapeUuid = HorizonUuid::make(QStringLiteral("default-via-shape"), name);  ///< 默认 Via 形状 UUID。
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

/**
 * @brief 写入单个封装的 Package、Padstack 和模型文件。
 * @param footprint 待写入的封装组件 IR。
 * @param root Horizon Pool 根目录。
 * @param exportModel3D 是否写入三维模型。
 * @param diagnostics 导出诊断列表。
 * @return 封装及其关联文件全部写入成功时返回 true。
 */
bool writeFootprintFiles(const IR::FootprintComponentIR& footprint,
                         const QString& root,
                         bool exportModel3D,
                         QStringList& diagnostics) {
    // 这里保留二次校验，防止未来新增绕过公共导出入口的调用路径。
    if (!validatePadIdentifiers(footprint, diagnostics))
        return false;
    if (!validateGenericKeepOutLayers(footprint, diagnostics))
        return false;
    QHash<QString, QString> padUuids;  ///< 焊盘编号到 Pad UUID 的映射。
    const QString packageUuid = HorizonUuid::make(QStringLiteral("package"), footprint.name);  ///< Package UUID。
    const QString packageDir =
        QDir(root).filePath(QStringLiteral("packages/") + safeName(footprint.name));  ///< 封装目录。
    const QList<IR::Model3DIR> models =
        exportModel3D ? footprint.models3d : QList<IR::Model3DIR>{};  ///< 待写入三维模型。
    const QJsonObject package = packageJson(footprint, packageUuid, models, padUuids, diagnostics);  ///< 封装 JSON。
    if (package.isEmpty())
        return false;
    if (!QDir().mkpath(packageDir) ||
        !writeJson(QDir(packageDir).filePath(QStringLiteral("package.json")), package, diagnostics))
        return false;
    if (!writeModels(models, root, packageUuid, diagnostics))
        return false;
    for (int i = 0; i < footprint.pads.size(); ++i) {
        const auto& pad = footprint.pads.at(i);  ///< 当前封装焊盘。
        const QString padStem = padFileStem(pad, i);  ///< 焊盘文件名主干。
        const QString uuid =
            HorizonUuid::make(QStringLiteral("padstack"), footprint.name + QStringLiteral("/") + padStem);
        const QJsonObject padstack = padstackJson(pad, uuid, diagnostics);  ///< Padstack JSON。
        if (padstack.isEmpty())
            return false;
        if (!writeJson(QDir(root).filePath(QStringLiteral("padstacks/") + safeName(footprint.name) +
                                           QStringLiteral("-") + padStem + QStringLiteral(".json")),
                       padstack,
                       diagnostics))
            return false;
    }
    for (int i = 0; i < footprint.holes.size(); ++i) {
        const QString holeName = QStringLiteral("MH%1").arg(i + 1);  ///< 机械孔名称。
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

/**
 * @brief 返回 Horizon Pool 目录使用的扩展名。
 * @return Horizon Pool 使用的目录扩展名。
 */
QString ExporterHorizonLibrary::libraryFileExtension() const {
    return QStringLiteral(".pool");
}

/**
 * @brief Horizon Pool 使用目录型输出。
 * @return 始终返回 true，表示输出是目录而非单个文件。
 */
bool ExporterHorizonLibrary::isDirectoryOutput() const {
    return true;
}

/**
 * @brief 返回最近一次导出的诊断信息。
 * @return 最近一次导出的诊断信息列表。
 */
QStringList ExporterHorizonLibrary::diagnostics() const {
    return m_diagnostics;
}

/**
 * @brief 导出单个封装，并先完成所有输入校验。
 * @param footprint 待导出的封装组件 IR。
 * @param filePath Horizon Pool 输出目录。
 * @param model3DPath 三维模型源目录；当前由封装 IR 提供模型数据。
 * @return 导出成功时返回 true。
 */
bool ExporterHorizonLibrary::exportFootprint(const IR::FootprintComponentIR& footprint,
                                             const QString& filePath,
                                             const QString& model3DPath) {
    Q_UNUSED(model3DPath);
    m_diagnostics.clear();
    if (!validateFootprintOutputNames({footprint}, m_diagnostics))
        return false;
    if (!validateFootprintsBeforeOutput({footprint}, m_diagnostics))
        return false;
    if (!ensureDirs(filePath, m_diagnostics) || !writePoolInfo(filePath, footprint.name, m_diagnostics))
        return false;
    return writeFootprintFiles(footprint, filePath, true, m_diagnostics);
}

/**
 * @brief 批量导出封装，并在创建 Pool 前完成整个输入集合的预检。
 * @param footprints 待导出的封装列表。
 * @param filePath Horizon Pool 输出目录。
 * @param model3DPath 三维模型源目录；当前由封装 IR 提供模型数据。
 * @param exportStep 是否导出 STEP 模型。
 * @param exportObj 是否导出 OBJ 模型。
 * @return 导出成功时返回 true。
 * @param libName 目标库名称。
 */
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
    if (!validateFootprintsBeforeOutput(footprints, m_diagnostics))
        return false;
    if (!ensureDirs(filePath, m_diagnostics) || !writePoolInfo(filePath, libName, m_diagnostics))
        return false;
    for (const auto& footprint : footprints) {
        if (!writeFootprintFiles(footprint, filePath, exportStep, m_diagnostics))
            return false;
    }
    return true;
}

/**
 * @brief 将单个符号包装为 Horizon Pool 符号库。
 * @param symbol 待导出的符号 IR。
 * @param filePath Horizon Pool 输出目录。
 * @return 导出成功时返回 true。
 */
bool ExporterHorizonLibrary::exportSymbol(const IR::SymbolComponentIR& symbol, const QString& filePath) {
    return exportSymbolLibrary({symbol}, symbol.name, filePath, false, false);
}

/**
 * @brief 将符号、Unit 和 Entity 写入 Horizon Pool。
 * @param symbols 待导出的符号列表。
 * @param libName Pool 名称。
 * @param filePath Horizon Pool 输出目录。
 * @param appendMode 兼容通用接口的追加模式；当前 Horizon 导出始终重建目标 Pool。
 * @param updateMode 兼容通用接口的更新模式；当前 Horizon 导出不支持增量更新。
 * @param libraryDescription 兼容通用接口的库描述。
 * @return 导出成功时返回 true。
 */
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
    QHash<QString, QString> symbolNameOwners;  ///< 清理后符号名称到原始名称的映射。
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
        QJsonObject gates;  ///< 当前符号的 Gate 集合。
        for (int part = 0; part < qMax(1, symbol.partCount); ++part) {
            const QString baseKey = symbol.name;  ///< 当前符号的稳定名称主键。
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
        const QString entityUuid = HorizonUuid::make(QStringLiteral("entity"), symbol.name);  ///< Entity UUID。
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

/**
 * @brief 导出包含符号、封装和 Part 引用的完整组件库。
 * @param components 完整组件 IR 列表。
 * @param libName Pool 名称。
 * @param filePath Horizon Pool 根目录。
 * @param exportModel3D 是否嵌入组件和封装中的三维模型。
 * @param model3DBaseDir 兼容通用接口的模型目录参数；当前始终使用 Pool 内部目录。
 * @return 导出成功时返回 true。
 */
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
    QHash<QString, QString> componentNameOwners;  ///< 清理后组件名称到原始名称的映射。
    QList<IR::FootprintComponentIR> footprints;  ///< 用于预检的封装列表。
    footprints.reserve(components.size());
    for (const auto& component : components) {
        const QString baseKey =
            component.name.isEmpty() ? component.symbol.name : component.name;  ///< 组件稳定名称主键。
        if (!validateSafeName(componentNameOwners, baseKey, QStringLiteral("组件"), m_diagnostics))
            return false;
        footprints.append(component.footprint);
    }
    if (!validateFootprintOutputNames(footprints, m_diagnostics))
        return false;
    if (!validateFootprintsBeforeOutput(footprints, m_diagnostics))
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
