#include "ExporterAllegroFootprint.h"

#include "AllegroLayerMapper.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QRegularExpression>
#include <QSet>

#include <cmath>

namespace EasyKiConverter {
namespace {

/** 清理用户名称并阻断路径穿越，生成可用于 Import Package 的文件名。 */
QString safeName(const QString& value, const QString& fallback) {
    QString name = value.trimmed();
    name.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9._-]")), QStringLiteral("_"));
    while (name.contains(QStringLiteral("..")))
        name.replace(QStringLiteral(".."), QStringLiteral("_"));
    if (name.isEmpty() || name == QStringLiteral(".") || name == QStringLiteral("_"))
        name = fallback;
    return name.left(120);
}

/** 以固定精度序列化毫米或角度值，保证 Padstack 键稳定。 */
QString number(double value) {
    return QString::number(value, 'f', 6);
}

/** 将 IR 焊盘形状转换为 Import Package 的稳定名称。 */
QString shapeName(IR::PadShape shape) {
    // 形状名称参与 Padstack 规范键，必须覆盖所有 IR 形状分支。
    switch (shape) {
        case IR::PadShape::Rect:
            return QStringLiteral("rect");
        case IR::PadShape::Ellipse:
            return QStringLiteral("ellipse");
        case IR::PadShape::Oval:
            return QStringLiteral("oval");
        case IR::PadShape::RoundRect:
            return QStringLiteral("roundrect");
        case IR::PadShape::Polygon:
            return QStringLiteral("polygon");
        case IR::PadShape::Trapezoid:
            return QStringLiteral("trapezoid");
    }
    return QStringLiteral("unknown");
}

/** 将 IR 焊盘类型转换为 Import Package 的类型名称。 */
QString padTypeName(IR::PadType type) {
    return type == IR::PadType::ThroughHole ? QStringLiteral("through-hole") : QStringLiteral("smd");
}

/** 将点列表编码为可读的毫米坐标 JSON 数组。 */
QJsonArray pointArray(const QList<QPointF>& points) {
    QJsonArray array;
    for (const QPointF& point : points) {
        QJsonObject item;
        item.insert(QStringLiteral("x_mm"), point.x());
        item.insert(QStringLiteral("y_mm"), point.y());
        array.append(item);
    }
    return array;
}

/** 将 Allegro 层分配对象编码为 JSON。 */
QJsonObject layerObject(const AllegroLayerAssignment& layer) {
    return {{QStringLiteral("class"), layer.className},
            {QStringLiteral("subclass"), layer.subclassName},
            {QStringLiteral("source_semantic"), layer.sourceSemantic}};
}

/** 原子性地写入文本文件内容。 */
bool writeText(const QString& path, const QByteArray& content) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    return file.write(content) == content.size();
}

/** 将 JSON 对象格式化后写入文件。 */
bool writeJson(const QString& path, const QJsonObject& object) {
    return writeText(path, QJsonDocument(object).toJson(QJsonDocument::Indented));
}

/** 构造包含制造语义的规范 Padstack 去重键。 */
QString canonicalPadKey(const IR::FootprintPadIR& pad) {
    QString key = padTypeName(pad.padType) + QLatin1Char('|') + shapeName(pad.shape) + QLatin1Char('|') +
                  number(pad.size.width()) + QLatin1Char('|') + number(pad.size.height()) + QLatin1Char('|') +
                  number(pad.holeSize) + QLatin1Char('|') + number(pad.holeLength) + QLatin1Char('|') +
                  number(pad.rotation) + QLatin1Char('|') + QString::number(static_cast<int>(pad.layer)) +
                  QLatin1Char('|') + (pad.isPlated ? QStringLiteral("plated") : QStringLiteral("non-plated"));
    for (const QPointF& point : pad.customShapePoints)
        key += QLatin1Char('|') + number(point.x()) + QLatin1Char(',') + number(point.y());
    return key;
}

/** 将规范 Padstack 模型转换为 manifest 中的 JSON 对象。 */
QJsonObject padstackObject(const AllegroPadstackModel& padstack) {
    return {
        {QStringLiteral("name"), padstack.name},
        {QStringLiteral("shape"), shapeName(padstack.shape)},
        {QStringLiteral("pad_type"), padTypeName(padstack.padType)},
        {QStringLiteral("size_mm"),
         QJsonObject{{QStringLiteral("x"), padstack.size.width()}, {QStringLiteral("y"), padstack.size.height()}}},
        {QStringLiteral("drill_mm"),
         QJsonObject{{QStringLiteral("x"), padstack.drill.width()}, {QStringLiteral("y"), padstack.drill.height()}}},
        {QStringLiteral("slot_length_mm"), padstack.slotLength},
        {QStringLiteral("rotation_deg"), padstack.rotation},
        {QStringLiteral("plated"), padstack.plated},
        {QStringLiteral("layer"), static_cast<int>(padstack.layer)},
        {QStringLiteral("polygon_mm"), pointArray(padstack.polygon)},
        {QStringLiteral("canonical_key"), padstack.canonicalKey}};
}

/** 将封装几何模型转换为带边界和层语义的 JSON 对象。 */
QJsonObject geometryObject(const AllegroGeometryModel& item) {
    QJsonObject object{{QStringLiteral("primitive"), item.primitive},
                       {QStringLiteral("layer"), layerObject(item.layer)},
                       {QStringLiteral("points_mm"), pointArray(item.points)},
                       {QStringLiteral("rotation_deg"), item.rotation},
                       {QStringLiteral("width_mm"), item.width},
                       {QStringLiteral("text"), item.text}};
    object.insert(QStringLiteral("bounds_mm"),
                  QJsonObject{{QStringLiteral("x"), item.bounds.x()},
                              {QStringLiteral("y"), item.bounds.y()},
                              {QStringLiteral("width"), item.bounds.width()},
                              {QStringLiteral("height"), item.bounds.height()}});
    object.insert(QStringLiteral("center_mm"),
                  QJsonObject{{QStringLiteral("x"), item.center.x()}, {QStringLiteral("y"), item.center.y()}});
    object.insert(QStringLiteral("radius_mm"), item.radius);
    return object;
}

/** 追加图层几何，并拒绝无法映射的 IR 层。 */
bool appendLayeredGeometry(const std::optional<AllegroLayerAssignment>& layer,
                           const QString& primitive,
                           const QList<QPointF>& points,
                           const QRectF& bounds,
                           const QPointF& center,
                           double radius,
                           double rotation,
                           double width,
                           const QString& text,
                           AllegroPackageModel& package,
                           QStringList& diagnostics) {
    if (!layer) {
        diagnostics.append(QStringLiteral("Allegro: 无法映射图元 %1 的 IR 层，已拒绝静默映射").arg(primitive));
        return false;
    }
    package.geometry.append({primitive, *layer, points, bounds, center, radius, rotation, width, text});
    return true;
}

/** 从一个 Footprint IR 构建独立的 Allegro 目标模型。 */
bool buildPackage(const IR::FootprintComponentIR& footprint, AllegroPackageModel& package, QStringList& diagnostics) {
    package.name = safeName(footprint.name, QStringLiteral("footprint"));
    package.description = footprint.description;
    package.height = footprint.height;

    QMap<QString, QString> keyToName;
    QSet<QString> pinNumbers;
    int padIndex = 0;
    for (const IR::FootprintPadIR& pad : footprint.pads) {
        if (pad.number.trimmed().isEmpty()) {
            diagnostics.append(QStringLiteral("Allegro: 封装 %1 存在空编号焊盘，无法建立 Pin 关联").arg(package.name));
            return false;
        }
        if (pinNumbers.contains(pad.number)) {
            diagnostics.append(QStringLiteral("Allegro: 封装 %1 存在重复 Pin 编号 %2").arg(package.name, pad.number));
            return false;
        }
        pinNumbers.insert(pad.number);

        const QString key = canonicalPadKey(pad);
        QString padstackName = keyToName.value(key);
        if (padstackName.isEmpty()) {
            const QByteArray digest =
                QCryptographicHash::hash(key.toUtf8(), QCryptographicHash::Sha256).toHex().left(12);
            padstackName = QStringLiteral("PS_%1").arg(QString::fromLatin1(digest));
            keyToName.insert(key, padstackName);
            package.padstacks.append({padstackName,
                                      pad.shape,
                                      pad.padType,
                                      pad.size,
                                      pad.isThroughHole() ? QSizeF(pad.holeSize, pad.holeSize) : QSizeF(),
                                      pad.holeLength,
                                      pad.rotation,
                                      pad.isPlated,
                                      pad.layer,
                                      pad.customShapePoints,
                                      key});
            if (pad.shape == IR::PadShape::Polygon && pad.customShapePoints.isEmpty()) {
                diagnostics.append(
                    QStringLiteral("Allegro: Padstack %1 声明为 Polygon 但没有 Shape 顶点").arg(padstackName));
                return false;
            }
        }
        package.pins.append({pad.number, padstackName, pad.position, pad.rotation, false});
        ++padIndex;
    }

    // 独立安装孔不生成电气 Pin，只作为机械几何写入 Import Package。
    for (const IR::FootprintHoleIR& hole : footprint.holes) {
        const auto layer = AllegroLayerMapper::map(IR::LayerType::Mechanical1);
        if (!appendLayeredGeometry(
                layer,
                QStringLiteral("mounting-hole"),
                {},
                QRectF(
                    hole.center.x() - hole.radius, hole.center.y() - hole.radius, hole.radius * 2.0, hole.radius * 2.0),
                hole.center,
                hole.radius,
                0.0,
                0.0,
                {},
                package,
                diagnostics))
            return false;
    }

    for (const IR::FootprintTrackIR& track : footprint.tracks) {
        if (!appendLayeredGeometry(AllegroLayerMapper::map(track.layer),
                                   QStringLiteral("track"),
                                   track.points,
                                   {},
                                   {},
                                   0.0,
                                   0.0,
                                   track.width,
                                   {},
                                   package,
                                   diagnostics))
            return false;
    }

    for (const IR::FootprintCircleIR& circle : footprint.circles) {
        if (!appendLayeredGeometry(AllegroLayerMapper::map(circle.layer),
                                   QStringLiteral("circle"),
                                   {},
                                   QRectF(circle.center.x() - circle.radius,
                                          circle.center.y() - circle.radius,
                                          circle.radius * 2.0,
                                          circle.radius * 2.0),
                                   circle.center,
                                   circle.radius,
                                   0.0,
                                   circle.strokeWidth,
                                   {},
                                   package,
                                   diagnostics))
            return false;
    }
    for (const IR::FootprintRectangleIR& rectangle : footprint.rectangles) {
        if (!appendLayeredGeometry(AllegroLayerMapper::map(rectangle.layer),
                                   QStringLiteral("rotated-rectangle"),
                                   {},
                                   rectangle.bounds,
                                   rectangle.bounds.center(),
                                   0.0,
                                   rectangle.rotation,
                                   rectangle.strokeWidth,
                                   {},
                                   package,
                                   diagnostics))
            return false;
    }
    for (const IR::FootprintArcIR& arc : footprint.arcs) {
        if (!appendLayeredGeometry(
                AllegroLayerMapper::map(arc.layer),
                QStringLiteral("arc"),
                {},
                QRectF(arc.center.x() - arc.radius, arc.center.y() - arc.radius, arc.radius * 2.0, arc.radius * 2.0),
                arc.center,
                arc.radius,
                arc.startAngle,
                arc.width,
                {},
                package,
                diagnostics))
            return false;
    }
    for (const IR::FootprintTextIR& text : footprint.texts) {
        if (!appendLayeredGeometry(AllegroLayerMapper::map(text.layer),
                                   QStringLiteral("text"),
                                   text.textPathPoints,
                                   {},
                                   text.position,
                                   0.0,
                                   text.rotation,
                                   text.strokeWidth,
                                   text.text,
                                   package,
                                   diagnostics))
            return false;
    }
    for (const IR::FootprintRegionIR& region : footprint.regions) {
        const IR::LayerType layer = region.isKeepOut ? IR::LayerType::KeepOut : region.layer;
        if (!appendLayeredGeometry(AllegroLayerMapper::map(layer),
                                   region.isKeepOut ? QStringLiteral("keep-out") : QStringLiteral("region"),
                                   region.vertices,
                                   {},
                                   {},
                                   0.0,
                                   0.0,
                                   0.0,
                                   {},
                                   package,
                                   diagnostics))
            return false;
    }
    for (const IR::FootprintOutlineIR& outline : footprint.outlines) {
        if (!appendLayeredGeometry(AllegroLayerMapper::map(outline.layer),
                                   QStringLiteral("outline"),
                                   outline.points,
                                   {},
                                   {},
                                   0.0,
                                   0.0,
                                   outline.strokeWidth,
                                   {},
                                   package,
                                   diagnostics))
            return false;
    }

    if (footprint.shouldGenerateCourtyard) {
        QRectF bounds;
        for (const IR::FootprintPadIR& pad : footprint.pads)
            bounds = bounds.isNull() ? QRectF(pad.position, pad.size) : bounds.united(QRectF(pad.position, pad.size));
        if (!bounds.isNull()) {
            bounds.adjust(-0.25, -0.25, 0.25, 0.25);
            package.usedPlaceBoundFallback = true;
            diagnostics.append(
                QStringLiteral("Allegro: 封装 %1 没有可靠 Place Bound，已按焊盘包围盒回退").arg(package.name));
            if (!appendLayeredGeometry(AllegroLayerMapper::map(IR::LayerType::Mechanical1),
                                       QStringLiteral("place-bound-fallback"),
                                       {},
                                       bounds,
                                       bounds.center(),
                                       0.0,
                                       0.0,
                                       0.05,
                                       {},
                                       package,
                                       diagnostics))
                return false;
        }
    }

    for (const IR::Model3DIR& model : footprint.models3d) {
        if (!model.hasStepData()) {
            diagnostics.append(
                QStringLiteral("Allegro: 封装 %1 的 STEP 模型 %2 缺少数据，未写入").arg(package.name, model.name()));
            continue;
        }
        QString modelFile = safeName(model.name(), QStringLiteral("model_%1.step").arg(package.stepFiles.size()));
        if (QFileInfo(modelFile).suffix().isEmpty())
            modelFile += QStringLiteral(".step");
        const QString modelStem = QFileInfo(modelFile).completeBaseName();
        const QString modelSuffix = QFileInfo(modelFile).suffix().isEmpty()
                                        ? QStringLiteral(".step")
                                        : QStringLiteral(".%1").arg(QFileInfo(modelFile).suffix());
        int modelIndex = 2;
        while (package.stepFiles.contains(modelFile))
            modelFile = QStringLiteral("%1_%2%3").arg(modelStem).arg(modelIndex++).arg(modelSuffix);
        package.stepFiles.append(modelFile);
        const auto translation = model.translation();
        const auto rotation = model.rotation();
        const auto offset = model.stepOffsetMm();
        package.stepTransforms.append(QJsonObject{
            {QStringLiteral("translation_mm"),
             QJsonObject{{QStringLiteral("x"), translation.x},
                         {QStringLiteral("y"), translation.y},
                         {QStringLiteral("z"), translation.z}}},
            {QStringLiteral("rotation_deg"),
             QJsonObject{{QStringLiteral("x"), rotation.x},
                         {QStringLiteral("y"), rotation.y},
                         {QStringLiteral("z"), rotation.z}}},
            {QStringLiteral("step_offset_mm"),
             QJsonObject{
                 {QStringLiteral("x"), offset.x}, {QStringLiteral("y"), offset.y}, {QStringLiteral("z"), offset.z}}},
            {QStringLiteral("coordinate_system"), QStringLiteral("IR-mm, Allegro import mapping required")}});
    }
    return true;
}

/** 将 Allegro 封装模型编码为 normalized-data JSON。 */
QJsonObject packageObject(const AllegroPackageModel& package) {
    QJsonArray padstacks;
    for (const AllegroPadstackModel& padstack : package.padstacks)
        padstacks.append(padstackObject(padstack));
    QJsonArray pins;
    for (const AllegroPinModel& pin : package.pins) {
        pins.append(QJsonObject{{QStringLiteral("number"), pin.number},
                                {QStringLiteral("padstack"), pin.padstackName},
                                {QStringLiteral("x_mm"), pin.position.x()},
                                {QStringLiteral("y_mm"), pin.position.y()},
                                {QStringLiteral("rotation_deg"), pin.rotation},
                                {QStringLiteral("mechanical"), pin.mechanical}});
    }
    QJsonArray geometry;
    for (const AllegroGeometryModel& item : package.geometry)
        geometry.append(geometryObject(item));
    QJsonArray models;
    for (const QString& model : package.stepFiles)
        models.append(model);
    return {{QStringLiteral("name"), package.name},
            {QStringLiteral("description"), package.description},
            {QStringLiteral("height_mm"), package.height},
            {QStringLiteral("padstacks"), padstacks},
            {QStringLiteral("pins"), pins},
            {QStringLiteral("geometry"), geometry},
            {QStringLiteral("step_models"), models},
            {QStringLiteral("step_transforms"), package.stepTransforms},
            {QStringLiteral("mask_paste_semantics"),
             QStringLiteral("IR 当前未提供独立阻焊/锡膏扩展，需在 Allegro 导入环境按工艺规则补充")},
            {QStringLiteral("place_bound_fallback"), package.usedPlaceBoundFallback}};
}

/** 将符号引脚写成不依赖 Allegro 私有编号的规范化 JSON。 */
QJsonObject symbolPinObject(const IR::SymbolPinIR& pin) {
    return {{QStringLiteral("name"), pin.name},
            {QStringLiteral("designator"), pin.designator},
            {QStringLiteral("x_mm"), pin.position.x()},
            {QStringLiteral("y_mm"), pin.position.y()},
            {QStringLiteral("length_mm"), pin.length},
            {QStringLiteral("direction"), static_cast<int>(pin.direction)},
            {QStringLiteral("electrical_type"), static_cast<int>(pin.electricalType)},
            {QStringLiteral("part_index"), pin.partIndex},
            {QStringLiteral("common_to_all_parts"), pin.commonToAllParts},
            {QStringLiteral("show_name"), pin.display.showName},
            {QStringLiteral("show_designator"), pin.display.showDesignator},
            {QStringLiteral("name_rotation_deg"), pin.nameRotation},
            {QStringLiteral("number_rotation_deg"), pin.numberRotation},
            {QStringLiteral("inverted"), pin.style.inverted},
            {QStringLiteral("clock"), pin.style.clock},
            {QStringLiteral("active_low"), pin.style.activeLow}};
}

/** 将 Allegro Import Package 需要的符号语义写入 JSON。 */
QJsonObject symbolObject(const IR::SymbolComponentIR& symbol) {
    QJsonArray pins;
    for (const IR::SymbolPinIR& pin : symbol.pins)
        pins.append(symbolPinObject(pin));

    QJsonArray rectangles;
    for (const IR::SymbolRectangleIR& rectangle : symbol.rectangles) {
        rectangles.append(QJsonObject{{QStringLiteral("x0_mm"), rectangle.x0},
                                      {QStringLiteral("y0_mm"), rectangle.y0},
                                      {QStringLiteral("x1_mm"), rectangle.x1},
                                      {QStringLiteral("y1_mm"), rectangle.y1},
                                      {QStringLiteral("corner_radius_x_mm"), rectangle.cornerRadiusX},
                                      {QStringLiteral("corner_radius_y_mm"), rectangle.cornerRadiusY},
                                      {QStringLiteral("stroke_width_mm"), rectangle.strokeWidth},
                                      {QStringLiteral("filled"), rectangle.isFilled},
                                      {QStringLiteral("part_index"), rectangle.partIndex}});
    }

    QJsonArray circles;
    for (const IR::SymbolCircleIR& circle : symbol.circles) {
        circles.append(QJsonObject{
            {QStringLiteral("center"),
             QJsonObject{{QStringLiteral("x"), circle.center.x()}, {QStringLiteral("y"), circle.center.y()}}},
            {QStringLiteral("radius_mm"), circle.radius},
            {QStringLiteral("stroke_width_mm"), circle.strokeWidth},
            {QStringLiteral("filled"), circle.isFilled},
            {QStringLiteral("part_index"), circle.partIndex}});
    }

    QJsonArray arcs;
    for (const IR::SymbolArcIR& arc : symbol.arcs) {
        arcs.append(QJsonObject{
            {QStringLiteral("start"),
             QJsonObject{{QStringLiteral("x"), arc.startPoint.x()}, {QStringLiteral("y"), arc.startPoint.y()}}},
            {QStringLiteral("mid"),
             QJsonObject{{QStringLiteral("x"), arc.midPoint.x()}, {QStringLiteral("y"), arc.midPoint.y()}}},
            {QStringLiteral("end"),
             QJsonObject{{QStringLiteral("x"), arc.endPoint.x()}, {QStringLiteral("y"), arc.endPoint.y()}}},
            {QStringLiteral("stroke_width_mm"), arc.strokeWidth},
            {QStringLiteral("filled"), arc.isFilled},
            {QStringLiteral("part_index"), arc.partIndex}});
    }

    QJsonArray polylines;
    for (const IR::SymbolPolylineIR& polyline : symbol.polylines)
        polylines.append(QJsonObject{{QStringLiteral("points_mm"), pointArray(polyline.points)},
                                     {QStringLiteral("stroke_width_mm"), polyline.strokeWidth},
                                     {QStringLiteral("filled"), polyline.isFilled},
                                     {QStringLiteral("part_index"), polyline.partIndex}});

    QJsonArray polygons;
    for (const IR::SymbolPolygonIR& polygon : symbol.polygons)
        polygons.append(QJsonObject{{QStringLiteral("points_mm"), pointArray(polygon.points)},
                                    {QStringLiteral("stroke_width_mm"), polygon.strokeWidth},
                                    {QStringLiteral("filled"), polygon.isFilled},
                                    {QStringLiteral("part_index"), polygon.partIndex}});

    QJsonArray texts;
    for (const IR::SymbolTextIR& text : symbol.texts)
        texts.append(QJsonObject{{QStringLiteral("text"), text.text},
                                 {QStringLiteral("x_mm"), text.position.x()},
                                 {QStringLiteral("y_mm"), text.position.y()},
                                 {QStringLiteral("rotation_deg"), text.rotation},
                                 {QStringLiteral("font_family"), text.fontFamily},
                                 {QStringLiteral("font_size_mm"), text.fontSizeMm},
                                 {QStringLiteral("visible"), text.visible},
                                 {QStringLiteral("part_index"), text.partIndex}});

    QJsonObject graphicsCounts{{QStringLiteral("pins"), symbol.pins.size()},
                               {QStringLiteral("rectangles"), symbol.rectangles.size()},
                               {QStringLiteral("circles"), symbol.circles.size()},
                               {QStringLiteral("arcs"), symbol.arcs.size()},
                               {QStringLiteral("ellipses"), symbol.ellipses.size()},
                               {QStringLiteral("pies"), symbol.pies.size()},
                               {QStringLiteral("elliptical_arcs"), symbol.ellipticalArcs.size()},
                               {QStringLiteral("polylines"), symbol.polylines.size()},
                               {QStringLiteral("polygons"), symbol.polygons.size()},
                               {QStringLiteral("paths"), symbol.paths.size()},
                               {QStringLiteral("beziers"), symbol.beziers.size()},
                               {QStringLiteral("ieee_symbols"), symbol.ieeeSymbols.size()},
                               {QStringLiteral("texts"), symbol.texts.size()},
                               {QStringLiteral("text_frames"), symbol.textFrames.size()},
                               {QStringLiteral("images"), symbol.images.size()}};

    return {{QStringLiteral("name"), symbol.name},
            {QStringLiteral("description"), symbol.description},
            {QStringLiteral("designator_prefix"), symbol.designatorPrefix},
            {QStringLiteral("part_count"), symbol.partCount},
            {QStringLiteral("origin_mm"),
             QJsonObject{{QStringLiteral("x"), symbol.originX}, {QStringLiteral("y"), symbol.originY}}},
            {QStringLiteral("preserve_logical_origin"), symbol.preserveLogicalOrigin},
            {QStringLiteral("footprint_name"), symbol.footprintName},
            {QStringLiteral("footprint_names"), QJsonArray::fromStringList(symbol.footprintNames)},
            {QStringLiteral("aliases"), QJsonArray::fromStringList(symbol.aliases)},
            {QStringLiteral("pins"), pins},
            {QStringLiteral("rectangles"), rectangles},
            {QStringLiteral("circles"), circles},
            {QStringLiteral("arcs"), arcs},
            {QStringLiteral("polylines"), polylines},
            {QStringLiteral("polygons"), polygons},
            {QStringLiteral("texts"), texts},
            {QStringLiteral("graphics_counts"), graphicsCounts}};
}

/** 写入符号文件并返回 manifest 中的符号条目。 */
bool writeAllegroSymbolFiles(const QList<IR::SymbolComponentIR>& symbols,
                             const QString& packageDir,
                             QJsonArray& symbolEntries,
                             QStringList& diagnostics) {
    const QString symbolDir = packageDir + QDir::separator() + QStringLiteral("symbols");
    if (!QDir().mkpath(symbolDir)) {
        diagnostics.append(QStringLiteral("Allegro: 无法创建符号规范化目录"));
        return false;
    }
    QSet<QString> names;
    for (const IR::SymbolComponentIR& symbol : symbols) {
        const QString name = safeName(symbol.name, QStringLiteral("symbol"));
        if (names.contains(name)) {
            diagnostics.append(QStringLiteral("Allegro: 符号名称清洗后冲突：%1").arg(name));
            return false;
        }
        names.insert(name);
        const QString relativePath = QStringLiteral("symbols/%1.json").arg(name);
        if (!writeJson(packageDir + QDir::separator() + relativePath, symbolObject(symbol))) {
            diagnostics.append(QStringLiteral("Allegro: 无法写入符号规范化数据：%1").arg(name));
            return false;
        }
        symbolEntries.append(QJsonObject{{QStringLiteral("name"), name},
                                         {QStringLiteral("path"), relativePath},
                                         {QStringLiteral("part_count"), symbol.partCount},
                                         {QStringLiteral("pin_count"), symbol.pins.size()}});

        if (!symbol.paths.isEmpty() || !symbol.beziers.isEmpty() || !symbol.images.isEmpty())
            diagnostics.append(
                QStringLiteral("Allegro: 符号 %1 的部分高级图元仅保留图元计数，需在导入环境复核").arg(name));
    }
    return true;
}

/** 将符号与封装、引脚与焊盘关系写入 Import Package 清单。 */
QJsonArray componentEntries(const QList<IR::ComponentIR>& components) {
    QJsonArray entries;
    for (const IR::ComponentIR& component : components) {
        QJsonArray pinMappings;
        for (const IR::SymbolPinIR& pin : component.symbol.pins)
            pinMappings.append(
                QJsonObject{{QStringLiteral("pin"), pin.designator}, {QStringLiteral("pad"), pin.designator}});
        entries.append(QJsonObject{{QStringLiteral("component"), component.name},
                                   {QStringLiteral("symbol"), safeName(component.symbol.name, component.name)},
                                   {QStringLiteral("footprint"), safeName(component.footprint.name, component.name)},
                                   {QStringLiteral("pin_to_pad"), pinMappings}});
    }
    return entries;
}

}  // namespace

/** Allegro Import Package 使用目录后缀，而不是原生库扩展名。 */
QString ExporterAllegroFootprint::libraryFileExtension() const {
    return QStringLiteral("_Allegro");
}

/** 声明 Allegro 导出结果是一个目录包。 */
bool ExporterAllegroFootprint::isDirectoryOutput() const {
    return true;
}

/** 将单个封装委托给统一的 Import Package 导出入口。 */
bool ExporterAllegroFootprint::exportFootprint(const IR::FootprintComponentIR& footprint,
                                               const QString& filePath,
                                               const QString& model3DPath) {
    Q_UNUSED(model3DPath)
    return exportFootprintLibrary({footprint}, QFileInfo(filePath).baseName(), filePath, false, true);
}

/** 生成 Allegro Import Package 及其引用文件和诊断信息。 */
bool ExporterAllegroFootprint::exportFootprintLibrary(const QList<IR::FootprintComponentIR>& footprints,
                                                      const QString& libName,
                                                      const QString& filePath,
                                                      bool preferWrl,
                                                      bool exportStep,
                                                      const QString& libraryDescription,
                                                      const QString& libraryKeywords,
                                                      bool useAbsolutePaths,
                                                      const QString& model3DBaseDir) {
    Q_UNUSED(preferWrl)
    Q_UNUSED(useAbsolutePaths)
    Q_UNUSED(model3DBaseDir)
    m_diagnostics.clear();
    if (footprints.isEmpty()) {
        m_diagnostics.append(QStringLiteral("Allegro: 没有可导出的封装"));
        return false;
    }
    if (!QDir().mkpath(filePath)) {
        m_diagnostics.append(QStringLiteral("Allegro: 无法创建 Import Package 目录 %1").arg(filePath));
        return false;
    }
    const QString normalizedDir = filePath + QDir::separator() + QStringLiteral("normalized-data");
    const QString padstackDir = filePath + QDir::separator() + QStringLiteral("padstacks");
    const QString shapeDir = filePath + QDir::separator() + QStringLiteral("shapes");
    const QString modelDir = filePath + QDir::separator() + QStringLiteral("models");
    if (!QDir().mkpath(normalizedDir) || !QDir().mkpath(padstackDir) || !QDir().mkpath(shapeDir) ||
        !QDir().mkpath(modelDir)) {
        m_diagnostics.append(QStringLiteral("Allegro: 无法创建 Import Package 子目录"));
        return false;
    }

    QJsonArray packages;
    QSet<QString> packageNames;
    QSet<QString> padstackFiles;
    for (const IR::FootprintComponentIR& footprint : footprints) {
        AllegroPackageModel package;
        if (!buildPackage(footprint, package, m_diagnostics))
            return false;
        if (!exportStep) {
            // 未启用 STEP 输出时不能在规范化数据中留下不存在的模型引用。
            package.stepFiles.clear();
            package.stepTransforms = QJsonArray();
        }
        if (packageNames.contains(package.name)) {
            m_diagnostics.append(QStringLiteral("Allegro: 封装名称冲突，清理后仍为 %1").arg(package.name));
            return false;
        }
        packageNames.insert(package.name);
        const QString dataPath = normalizedDir + QDir::separator() + package.name + QStringLiteral(".json");
        if (!writeJson(dataPath, packageObject(package))) {
            m_diagnostics.append(QStringLiteral("Allegro: 无法写入规范化封装数据 %1").arg(dataPath));
            return false;
        }
        for (const AllegroPadstackModel& padstack : package.padstacks) {
            if (padstackFiles.contains(padstack.name))
                continue;
            padstackFiles.insert(padstack.name);
            if (!writeJson(padstackDir + QDir::separator() + padstack.name + QStringLiteral(".json"),
                           padstackObject(padstack))) {
                m_diagnostics.append(QStringLiteral("Allegro: 无法写入 Padstack %1").arg(padstack.name));
                return false;
            }
            if (padstack.shape == IR::PadShape::Polygon || padstack.shape == IR::PadShape::Trapezoid ||
                padstack.shape == IR::PadShape::RoundRect) {
                if (!writeJson(shapeDir + QDir::separator() + padstack.name + QStringLiteral(".json"),
                               QJsonObject{{QStringLiteral("padstack"), padstack.name},
                                           {QStringLiteral("shape"), shapeName(padstack.shape)},
                                           {QStringLiteral("points_mm"), pointArray(padstack.polygon)}})) {
                    m_diagnostics.append(QStringLiteral("Allegro: 无法写入 Shape 依赖 %1").arg(padstack.name));
                    return false;
                }
            }
        }
        int stepIndex = 0;
        for (const IR::Model3DIR& model : footprint.models3d) {
            if (!model.hasStepData())
                continue;
            if (stepIndex >= package.stepFiles.size())
                break;
            const QString modelPath =
                modelDir + QDir::separator() + package.name + QLatin1Char('_') + package.stepFiles.at(stepIndex++);
            if (exportStep && !writeText(modelPath, model.stepData())) {
                m_diagnostics.append(QStringLiteral("Allegro: 无法写入 STEP 模型 %1").arg(modelPath));
                return false;
            }
        }
        packages.append(QJsonObject{
            {QStringLiteral("name"), package.name},
            {QStringLiteral("normalized_data"), QStringLiteral("normalized-data/%1.json").arg(package.name)},
            {QStringLiteral("padstack_count"), package.padstacks.size()},
            {QStringLiteral("pin_count"), package.pins.size()},
            {QStringLiteral("step_count"), package.stepFiles.size()}});
    }

    const QJsonObject manifest{
        {QStringLiteral("schema"), QStringLiteral("easykiconverter.allegro.import-package.v1")},
        {QStringLiteral("target"), QStringLiteral("Allegro PCB Footprint")},
        {QStringLiteral("library_name"), safeName(libName, QStringLiteral("EasyKiConverter"))},
        {QStringLiteral("generation_mode"), QStringLiteral("import-package")},
        {QStringLiteral("native_database_generated"), false},
        {QStringLiteral("allegro_version"), QStringLiteral("未锁定，请使用目标环境验证")},
        {QStringLiteral("description"), libraryDescription},
        {QStringLiteral("keywords"), libraryKeywords},
        {QStringLiteral("packages"), packages},
        {QStringLiteral("required_directories"), QJsonArray{"normalized-data", "padstacks", "shapes", "models"}},
        {QStringLiteral("entrypoint"), QStringLiteral("generator.il")},
        {QStringLiteral("native_outputs"), QJsonArray{".dra", ".psm", ".pad"}}};
    if (!writeJson(filePath + QDir::separator() + QStringLiteral("manifest.json"), manifest)) {
        m_diagnostics.append(QStringLiteral("Allegro: 无法写入 manifest.json"));
        return false;
    }
    const QByteArray generator =
        ";; EasyKiConverter Allegro Import Package validator\n"
        ";; This file does not write Cadence private databases.\n"
        ";; Validate manifest.json, then use the documented Allegro import flow.\n"
        "procedure(ekcImportPackage(manifestPath)\n"
        "  printf(\"EasyKiConverter package: %s\\n\" manifestPath)\n"
        "  printf(\"Use the supported Allegro environment to generate .dra/.psm/.pad.\\n\")\n"
        ")\n";
    if (!writeText(filePath + QDir::separator() + QStringLiteral("generator.il"), generator) ||
        !writeText(
            filePath + QDir::separator() + QStringLiteral("README_ALLEGRO.md"),
            QByteArray(
                "# Allegro Import Package\n\n"
                "This package contains normalized footprint data. It does not contain native Cadence databases.\n\n"
                "1. Validate manifest.json and configure PSMPATH, PADPATH and steppath.\n"
                "2. Load generator.il in the supported Cadence Allegro environment.\n"
                "3. Use the target Allegro import procedure to generate .dra, .psm and .pad files.\n\n"
                "The package was not validated by a locally installed Allegro executable.\n"))) {
        m_diagnostics.append(QStringLiteral("Allegro: 无法写入 Import Package 使用说明"));
        return false;
    }
    return true;
}

/** 生成仅包含 Allegro 符号规范化数据的 Import Package。 */
bool ExporterAllegroFootprint::exportSymbolLibrary(const QList<IR::SymbolComponentIR>& symbols,
                                                   const QString& libName,
                                                   const QString& filePath) {
    m_diagnostics.clear();
    if (symbols.isEmpty()) {
        m_diagnostics.append(QStringLiteral("Allegro: 没有可导出的符号"));
        return false;
    }
    if (!QDir().mkpath(filePath)) {
        m_diagnostics.append(QStringLiteral("Allegro: 无法创建符号 Import Package 目录 %1").arg(filePath));
        return false;
    }

    QJsonArray symbolEntries;
    if (!writeAllegroSymbolFiles(symbols, filePath, symbolEntries, m_diagnostics))
        return false;

    const QJsonObject manifest{{QStringLiteral("schema"), QStringLiteral("easykiconverter.allegro.import-package.v1")},
                               {QStringLiteral("target"), QStringLiteral("Allegro semantic symbol package")},
                               {QStringLiteral("library_name"), safeName(libName, QStringLiteral("EasyKiConverter"))},
                               {QStringLiteral("generation_mode"), QStringLiteral("normalized-symbol-data")},
                               {QStringLiteral("native_database_generated"), false},
                               {QStringLiteral("symbols"), symbolEntries},
                               {QStringLiteral("required_directories"), QJsonArray{"symbols"}},
                               {QStringLiteral("entrypoint"), QStringLiteral("generator.il")},
                               {QStringLiteral("native_outputs"), QJsonArray{".dra", ".psm", ".pad"}}};
    if (!writeJson(filePath + QDir::separator() + QStringLiteral("manifest.json"), manifest) ||
        !writeText(filePath + QDir::separator() + QStringLiteral("generator.il"),
                   QByteArray(";; Symbol data is normalized for a target Allegro workflow.\n")) ||
        !writeText(filePath + QDir::separator() + QStringLiteral("README_ALLEGRO.md"),
                   QByteArray("# Allegro semantic symbol package\n\n"
                              "This package preserves symbol and pin semantics as normalized JSON.\n"
                              "It does not claim to generate a native Cadence schematic database.\n"))) {
        m_diagnostics.append(QStringLiteral("Allegro: 无法写入符号 Import Package 清单或说明"));
        return false;
    }
    return true;
}

/** 通过通用符号接口导出 Allegro 语义包，并拒绝未实现的合并模式。 */
bool ExporterAllegroFootprint::exportSymbolLibrary(const QList<IR::SymbolComponentIR>& symbols,
                                                   const QString& libName,
                                                   const QString& filePath,
                                                   bool appendMode,
                                                   bool updateMode,
                                                   const QString& libraryDescription) {
    Q_UNUSED(libraryDescription)
    if (appendMode || updateMode) {
        m_diagnostics = {QStringLiteral("Allegro 符号 Import Package 暂不支持追加或更新模式")};
        return false;
    }
    return exportSymbolLibrary(symbols, libName, filePath);
}

/** 通过通用符号接口导出单个 Allegro 符号。 */
bool ExporterAllegroFootprint::exportSymbol(const IR::SymbolComponentIR& symbol, const QString& filePath) {
    return exportSymbolLibrary({symbol}, symbol.name, filePath, false, false);
}

/** 在已有封装 Import Package 中追加符号和器件关联清单。 */
bool ExporterAllegroFootprint::exportComponentLibrary(const QList<IR::ComponentIR>& components,
                                                      const QString& libName,
                                                      const QString& filePath,
                                                      bool exportModel3D,
                                                      const QString& model3DBaseDir) {
    if (components.isEmpty()) {
        m_diagnostics = {QStringLiteral("Allegro: 没有可导出的完整组件")};
        return false;
    }
    QList<IR::FootprintComponentIR> footprints;
    QList<IR::SymbolComponentIR> symbols;
    footprints.reserve(components.size());
    symbols.reserve(components.size());
    for (const IR::ComponentIR& component : components) {
        if (!component.hasSymbol() || !component.hasFootprint()) {
            m_diagnostics = {QStringLiteral("Allegro: 组件 %1 缺少符号或封装，无法建立完整关联").arg(component.name)};
            return false;
        }
        footprints.append(component.footprint);
        symbols.append(component.symbol);
    }

    if (!exportFootprintLibrary(
            footprints, libName, filePath, false, exportModel3D, QString(), QString(), false, model3DBaseDir))
        return false;

    QJsonArray symbolEntries;
    if (!writeAllegroSymbolFiles(symbols, filePath, symbolEntries, m_diagnostics))
        return false;

    QFile manifestFile(filePath + QDir::separator() + QStringLiteral("manifest.json"));
    if (!manifestFile.open(QIODevice::ReadOnly)) {
        m_diagnostics.append(QStringLiteral("Allegro: 无法读取已生成的 manifest.json"));
        return false;
    }
    QJsonParseError parseError;
    QJsonDocument document = QJsonDocument::fromJson(manifestFile.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        m_diagnostics.append(QStringLiteral("Allegro: 已生成的 manifest.json 无法解析"));
        return false;
    }
    QJsonObject manifest = document.object();
    manifest.insert(QStringLiteral("target"), QStringLiteral("Allegro semantic symbol and PCB package"));
    manifest.insert(QStringLiteral("symbols"), symbolEntries);
    manifest.insert(QStringLiteral("components"), componentEntries(components));
    manifest.insert(QStringLiteral("required_directories"),
                    QJsonArray{"normalized-data", "padstacks", "shapes", "models", "symbols"});
    if (!writeJson(filePath + QDir::separator() + QStringLiteral("manifest.json"), manifest) ||
        !writeText(filePath + QDir::separator() + QStringLiteral("README_ALLEGRO.md"),
                   QByteArray("# Allegro semantic symbol and PCB package\n\n"
                              "This package contains normalized symbol, footprint, pin-to-pad, and STEP data.\n"
                              "It does not contain native Cadence schematic or PCB databases.\n"))) {
        m_diagnostics.append(QStringLiteral("Allegro: 无法更新包含符号关联的 Import Package 文件"));
        return false;
    }
    return true;
}

/** 返回本次导出产生的结构化可见诊断文本。 */
QStringList ExporterAllegroFootprint::diagnostics() const {
    return m_diagnostics;
}

}  // namespace EasyKiConverter
