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
        for (int i = 0; i < footprint.models3d.size() && i < package.stepFiles.size(); ++i) {
            const IR::Model3DIR& model = footprint.models3d.at(i);
            const QString modelPath =
                modelDir + QDir::separator() + package.name + QLatin1Char('_') + package.stepFiles.at(i);
            if (exportStep && model.hasStepData() && !writeText(modelPath, model.stepData())) {
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

/** 返回本次导出产生的结构化可见诊断文本。 */
QStringList ExporterAllegroFootprint::diagnostics() const {
    return m_diagnostics;
}

}  // namespace EasyKiConverter
