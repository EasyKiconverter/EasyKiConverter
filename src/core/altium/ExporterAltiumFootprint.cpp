#include "ExporterAltiumFootprint.h"

#include "AltiumFootprintGeometryNormalizer.h"
#include "utils/AltiumConstants.h"
#include "utils/AltiumCoord.h"
#include "utils/AltiumLayerMap.h"

#include <QFile>
#include <QFileInfo>

namespace EasyKiConverter {

/**
 * @brief 导出单个封装
 * @param footprint 待导出的封装 IR。
 * @param filePath 输出文件路径。
 * @param model3DPath 可选的 STEP 文件路径。
 * @return 导出是否成功。
 */
bool ExporterAltiumFootprint::exportFootprint(const IR::FootprintComponentIR& footprint,
                                              const QString& filePath,
                                              const QString& model3DPath) {
    m_diagnostics.clear();
    QList<AltiumPcbComponent> components;  // 待写入单个封装文件的组件列表。
    components.append(convertFootprint(footprint, model3DPath));
    bool ok = m_writer.write(components, filePath);  // PcbLib 写入结果。
    m_diagnostics = m_writer.diagnostics();
    if (!ok) {
        qWarning() << "ExporterAltiumFootprint: Failed to write footprint to" << filePath;
    }
    return ok;
}

/**
 * @brief 导出封装库
 * @param footprints 待导出的封装列表。
 * @param libName 输出库名称。
 * @param filePath 输出文件路径。
 * @param preferWrl 是否优先使用 WRL 模型。
 * @param exportStep 是否导出 STEP 模型。
 * @param libraryDescription 库描述。
 * @param libraryKeywords 库关键词。
 * @param useAbsolutePaths 是否使用绝对模型路径。
 * @param model3DBaseDir 3D 模型基础目录。
 * @return 导出是否成功。
 */
bool ExporterAltiumFootprint::exportFootprintLibrary(const QList<IR::FootprintComponentIR>& footprints,
                                                     const QString& libName,
                                                     const QString& filePath,
                                                     bool preferWrl,
                                                     bool exportStep,
                                                     const QString& libraryDescription,
                                                     const QString& libraryKeywords,
                                                     bool useAbsolutePaths,
                                                     const QString& model3DBaseDir) {
    m_diagnostics.clear();
    QList<AltiumPcbComponent> components;  // 待写入封装库的组件列表。
    for (const IR::FootprintComponentIR& fp : footprints) {
        // 对于库级别导出，3D 模型路径在后续处理
        components.append(convertFootprint(fp));
    }
    bool ok = m_writer.write(components, filePath, libName);  // PcbLib 写入结果。
    m_diagnostics = m_writer.diagnostics();
    if (!ok) {
        qWarning() << "ExporterAltiumFootprint: Failed to write footprint library to" << filePath;
    }
    return ok;
}

/**
 * @brief FootprintComponentIR → AltiumPcbComponent
 * @param data 封装组件 IR。
 * @param model3DPath 可选的 STEP 文件路径。
 * @return 转换后的 Altium 封装组件。
 */
AltiumPcbComponent ExporterAltiumFootprint::convertFootprint(const IR::FootprintComponentIR& data,
                                                             const QString& model3DPath) {
    AltiumPcbComponent component;  // 转换后的 Altium 封装组件。
    component.name = data.name;
    component.description = data.description;
    component.height = data.height;  // IR height 已经是 mm

    // 转换焊盘
    for (const IR::FootprintPadIR& pad : data.pads) {
        component.pads.append(convertPad(pad));
    }

    // IR 折线必须逐段展开。PcbLib Track 原语只表示一个起点和终点，
    // 直接取首尾点会把折线错误地压成一条对角线。
    for (const IR::FootprintTrackIR& track : data.tracks) {
        for (int i = 1; i < track.points.size(); ++i) {
            IR::FootprintTrackIR segment = track;  // 当前折线段的临时 IR 表示。
            segment.points = {track.points[i - 1], track.points[i]};
            component.tracks.append(convertTrack(segment));
        }
    }

    // 独立安装孔用非电镀 MultiLayer pad 表示，使孔径、钻孔属性和图元归属
    // 都进入同一套已验证的 pad 序列化路径。
    for (const IR::FootprintHoleIR& hole : data.holes) {
        AltiumPcbPad pad;  // 由独立机械孔生成的非电镀焊盘。
        pad.locationX = AltiumCoord::mmToRaw(hole.center.x());
        pad.locationY = AltiumCoord::mmToRaw(hole.center.y());
        pad.holeSize = AltiumCoord::mmToRaw(hole.radius * 2.0);
        pad.sizeTopX = pad.sizeMidX = pad.sizeBotX = pad.holeSize;
        pad.sizeTopY = pad.sizeMidY = pad.sizeBotY = pad.holeSize;
        pad.shapeTop = pad.shapeMid = pad.shapeBot = AltiumConstants::PCB_PAD_SHAPE_ROUND;
        pad.layer = AltiumConstants::PCB_LAYER_MULTI;
        pad.isSMD = false;
        pad.isPlated = false;
        pad.isLocked = hole.isLocked;
        component.pads.append(pad);
    }

    // 转换圆
    for (const IR::FootprintCircleIR& circle : data.circles) {
        component.arcs.append(convertCircle(circle));
    }

    // 转换弧线
    for (const IR::FootprintArcIR& arc : data.arcs) {
        component.arcs.append(convertArc(arc));
    }

    // 转换矩形
    for (const IR::FootprintRectangleIR& rect : data.rectangles) {
        component.fills.append(convertRectangle(rect));
    }

    // 转换文本
    for (const IR::FootprintTextIR& text : data.texts) {
        component.texts.append(convertText(text));
    }

    // 转换区域
    for (const IR::FootprintRegionIR& region : data.regions) {
        component.regions.append(convertSolidRegion(region));
    }

    // 板框同样是逐段线原语；保留 IR 的层和线宽，不引入导入器特有假设。
    for (const IR::FootprintOutlineIR& outline : data.outlines) {
        for (int i = 1; i < outline.points.size(); ++i) {
            IR::FootprintTrackIR segment;  // 当前板框线段的临时 IR 表示。
            segment.points = {outline.points[i - 1], outline.points[i]};
            segment.width = outline.strokeWidth;
            segment.layer = outline.layer;
            segment.isLocked = outline.isLocked;
            component.tracks.append(convertTrack(segment));
        }
    }

    // 加载 3D 模型
    QByteArray stepData;  // 待嵌入的 STEP 文件数据。
    QString modelName;  // 3D 模型显示名称。
    if (!model3DPath.isEmpty()) {
        stepData = loadStepData(model3DPath);
        modelName = QFileInfo(model3DPath).fileName();
    } else if (!data.models3d.isEmpty()) {
        // 从 IR 的 3D 模型中加载 STEP 数据
        for (const IR::Model3DIR& model3d : data.models3d) {
            if (model3d.hasStepData()) {
                AltiumPcbComponent::Model3D model;  // 从 IR 转换的三维模型记录。
                model.name = model3d.name();
                model.stepData = model3d.stepData();
                const auto& rot = model3d.rotation();  // 模型旋转变换。
                model.rotX = rot.x;
                model.rotY = rot.y;
                model.rotZ = rot.z;
                const auto& translation = model3d.translation();  // 模型平移变换。
                const auto& stepOffset = model3d.stepOffsetMm();  // STEP 原点偏移。
                model.x = AltiumCoord::mmToRaw(translation.x + stepOffset.x);
                model.y = AltiumCoord::mmToRaw(translation.y + stepOffset.y);
                model.dz = translation.z + stepOffset.z;
                component.models.append(model);
            }
        }
        if (!component.models.isEmpty()) {
            stepData = component.models.first().stepData;
            modelName = component.models.first().name;
        }
    }

    // 如果有 3D 模型数据但 models 列表为空，补全
    if (!stepData.isEmpty() && component.models.isEmpty()) {
        AltiumPcbComponent::Model3D model;  // 从单独 STEP 文件补建的三维模型记录。
        model.name = modelName;
        model.stepData = stepData;
        component.models.append(model);
    }

    // 封装图元和 3D 模型必须经过同一原点平移，避免焊盘与模型错位。
    centerComponent(component);

    // 为 3D 模型生成 ComponentBody（基于封装包围盒）
    if (!component.models.isEmpty()) {
        generateComponentBody(component);
    }

    return component;
}

/**
 * @brief FootprintPadIR → AltiumPcbPad
 * @param pad 封装焊盘 IR。
 * @return 转换后的 Altium 焊盘记录。
 */
AltiumPcbPad ExporterAltiumFootprint::convertPad(const IR::FootprintPadIR& pad) {
    AltiumPcbPad altiumPad;  // 转换后的 Altium 焊盘记录。
    altiumPad.designator = pad.number;
    altiumPad.locationX = AltiumCoord::mmToRaw(pad.position.x());
    altiumPad.locationY = AltiumCoord::mmToRaw(pad.position.y());

    // IR size 已经是 mm
    int width = AltiumCoord::mmToRaw(pad.size.width());  // 焊盘宽度的 Altium 原始单位。
    int height = AltiumCoord::mmToRaw(pad.size.height());  // 焊盘高度的 Altium 原始单位。

    // 确定焊盘类型
    altiumPad.isSMD = pad.isSmd();
    altiumPad.isPlated = pad.isPlated;
    altiumPad.isLocked = pad.isLocked;

    if (pad.isThroughHole()) {
        // 通孔焊盘
        altiumPad.layer = AltiumConstants::PCB_LAYER_MULTI;
        altiumPad.holeSize = AltiumCoord::mmToRaw(pad.holeSize);

        // 槽孔检测：holeLength > holeSize 时为槽孔
        if (pad.holeLength > pad.holeSize + 0.001) {
            altiumPad.holeType = 2;  // Slot
            altiumPad.holeSlotLengthRaw = AltiumCoord::mmToRaw(pad.holeLength);
            // 槽孔旋转：长轴方向
            altiumPad.holeRotation = (pad.size.height() > pad.size.width()) ? 90.0 : 0.0;
        }
    } else {
        // 表贴焊盘：从 IR LayerType 映射到 Altium 层字节
        altiumPad.layer = static_cast<uint8_t>(AltiumLayerMap::fromLayerTypeToAltium(pad.layer));
    }

    // 形状：从 IR PadShape 映射到 Altium 焊盘形状字节
    uint8_t shape = AltiumLayerMap::toAltiumPadShape(pad.shape);  // Altium 焊盘形状编号。
    altiumPad.shapeTop = shape;
    altiumPad.shapeMid = shape;
    altiumPad.shapeBot = shape;

    // 圆角矩形的默认圆角百分比
    if (shape == AltiumConstants::PCB_PAD_SHAPE_ROUNDED_RECT) {
        altiumPad.cornerRadiusPercentage = 50;
    }

    // 尺寸
    altiumPad.sizeTopX = width;
    altiumPad.sizeTopY = height;
    altiumPad.sizeMidX = width;
    altiumPad.sizeMidY = height;
    altiumPad.sizeBotX = width;
    altiumPad.sizeBotY = height;

    altiumPad.rotation = pad.rotation;

    return altiumPad;
}

/**
 * @brief FootprintTrackIR → AltiumPcbTrack
 * @param track 封装走线 IR。
 * @return 转换后的 Altium 走线记录。
 */
AltiumPcbTrack ExporterAltiumFootprint::convertTrack(const IR::FootprintTrackIR& track) {
    AltiumPcbTrack altiumTrack;  // 转换后的 Altium 走线记录。
    altiumTrack.layer = static_cast<uint8_t>(AltiumLayerMap::fromLayerTypeToAltium(track.layer));
    altiumTrack.width = AltiumCoord::mmToRaw(track.width);

    // IR points 已经是解析后的 QList<QPointF>，单位 mm
    if (track.points.size() >= 2) {
        altiumTrack.startX = AltiumCoord::mmToRaw(track.points.first().x());
        altiumTrack.startY = AltiumCoord::mmToRaw(track.points.first().y());
        altiumTrack.endX = AltiumCoord::mmToRaw(track.points.last().x());
        altiumTrack.endY = AltiumCoord::mmToRaw(track.points.last().y());
    }

    return altiumTrack;
}

/**
 * @brief FootprintCircleIR → AltiumPcbArc（360° 圆弧）
 * @param circle 封装圆形图元 IR。
 * @return 转换后的 Altium 圆弧记录。
 */
AltiumPcbArc ExporterAltiumFootprint::convertCircle(const IR::FootprintCircleIR& circle) {
    AltiumPcbArc altiumArc;  // 转换后的 Altium 整圆弧记录。
    altiumArc.centerX = AltiumCoord::mmToRaw(circle.center.x());
    altiumArc.centerY = AltiumCoord::mmToRaw(circle.center.y());
    altiumArc.radius = AltiumCoord::mmToRaw(circle.radius);
    altiumArc.startAngle = 0.0;
    altiumArc.endAngle = 360.0;
    altiumArc.width = AltiumCoord::mmToRaw(circle.strokeWidth);
    altiumArc.layer = static_cast<uint8_t>(AltiumLayerMap::fromLayerTypeToAltium(circle.layer));
    return altiumArc;
}

/**
 * @brief FootprintArcIR → AltiumPcbArc
 * @param arc 封装圆弧图元 IR。
 * @return 转换后的 Altium 圆弧记录。
 */
AltiumPcbArc ExporterAltiumFootprint::convertArc(const IR::FootprintArcIR& arc) {
    AltiumPcbArc altiumArc;  // 转换后的 Altium 圆弧记录。
    altiumArc.centerX = AltiumCoord::mmToRaw(arc.center.x());
    altiumArc.centerY = AltiumCoord::mmToRaw(arc.center.y());
    altiumArc.radius = AltiumCoord::mmToRaw(arc.radius);
    altiumArc.startAngle = arc.startAngle;
    altiumArc.endAngle = arc.endAngle;
    altiumArc.width = AltiumCoord::mmToRaw(arc.width);
    altiumArc.layer = static_cast<uint8_t>(AltiumLayerMap::fromLayerTypeToAltium(arc.layer));
    return altiumArc;
}

/**
 * @brief FootprintRectangleIR → AltiumPcbFill
 * @param rect 封装矩形图元 IR。
 * @return 转换后的 Altium 填充记录。
 */
AltiumPcbFill ExporterAltiumFootprint::convertRectangle(const IR::FootprintRectangleIR& rect) {
    AltiumPcbFill altiumFill;  // 转换后的 Altium 填充记录。
    altiumFill.corner1X = AltiumCoord::mmToRaw(rect.bounds.left());
    altiumFill.corner1Y = AltiumCoord::mmToRaw(rect.bounds.top());
    altiumFill.corner2X = AltiumCoord::mmToRaw(rect.bounds.right());
    altiumFill.corner2Y = AltiumCoord::mmToRaw(rect.bounds.bottom());
    altiumFill.layer = static_cast<uint8_t>(AltiumLayerMap::fromLayerTypeToAltium(rect.layer));
    return altiumFill;
}

/**
 * @brief FootprintTextIR → AltiumPcbText
 * @param text 封装文本图元 IR。
 * @return 转换后的 Altium 文本记录。
 */
AltiumPcbText ExporterAltiumFootprint::convertText(const IR::FootprintTextIR& text) {
    AltiumPcbText altiumText;  // 转换后的 Altium 文本记录。
    altiumText.locationX = AltiumCoord::mmToRaw(text.position.x());
    altiumText.locationY = AltiumCoord::mmToRaw(text.position.y());
    altiumText.text = text.text;
    altiumText.rotation = text.rotation;
    altiumText.height = AltiumCoord::mmToRaw(text.fontSize);
    altiumText.strokeWidth = AltiumCoord::mmToRaw(text.strokeWidth);
    altiumText.isMirrored = text.mirror;
    altiumText.layer = static_cast<uint8_t>(AltiumLayerMap::fromLayerTypeToAltium(text.layer));
    return altiumText;
}

/**
 * @brief FootprintRegionIR → AltiumPcbRegion
 * @param region 封装区域图元 IR。
 * @return 转换后的 Altium 区域记录。
 */
AltiumPcbRegion ExporterAltiumFootprint::convertSolidRegion(const IR::FootprintRegionIR& region) {
    AltiumPcbRegion altiumRegion;  // 转换后的 Altium 区域记录。
    altiumRegion.layer = static_cast<uint8_t>(AltiumLayerMap::fromLayerTypeToAltium(region.layer));
    altiumRegion.isBoardCutout = region.isKeepOut;
    altiumRegion.v7LayerName = AltiumLayerMap::toLayerName(altiumRegion.layer);

    // IR vertices 已经是解析后的 QList<QPointF>，单位 mm
    for (const QPointF& point : region.vertices) {
        altiumRegion.vertices.append(QPointF(static_cast<double>(AltiumCoord::mmToRaw(point.x())),
                                             static_cast<double>(AltiumCoord::mmToRaw(point.y()))));
    }

    return altiumRegion;
}

/**
 * @brief 加载 STEP 文件数据
 * @param stepPath STEP 文件路径。
 * @return 文件内容；打开失败时返回空数组。
 */
QByteArray ExporterAltiumFootprint::loadStepData(const QString& stepPath) {
    QFile file(stepPath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "ExporterAltiumFootprint: Failed to open STEP file:" << stepPath;
        return QByteArray();
    }
    return file.readAll();
}

/**
 * @brief 将封装图元坐标居中到原点附近
 * @details 计算所有焊盘/走线/区域的包围盒中心，将所有坐标平移使中心对齐原点。
 *          Altium Designer 标准库的封装通常以原点为参考点。
 * @param component 待居中的 Altium 封装组件。
 */
void ExporterAltiumFootprint::centerComponent(AltiumPcbComponent& component) {
    const AltiumFootprintBounds bounds =
        AltiumFootprintGeometryNormalizer::computeBounds(component, false);  // 封装图元包围盒。
    if (!bounds.valid)
        return;
    const int offsetX = static_cast<int>((bounds.minX + bounds.maxX) / 2);  // X 方向中心偏移。
    const int offsetY = static_cast<int>((bounds.minY + bounds.maxY) / 2);  // Y 方向中心偏移。
    AltiumFootprintGeometryNormalizer::translate(component, offsetX, offsetY);
}

/**
 * @brief 从封装包围盒和 3D 模型生成 ComponentBody
 * @details 在 MECHANICAL1 层为每个嵌入的 STEP 模型创建一个矩形轮廓。
 *          轮廓基于所有图元的包围盒生成。
 * @param component 待生成 ComponentBody 的 Altium 封装组件。
 */
void ExporterAltiumFootprint::generateComponentBody(AltiumPcbComponent& component) {
    if (component.models.isEmpty())
        return;

    const AltiumFootprintBounds bounds =
        AltiumFootprintGeometryNormalizer::computeBounds(component, true);  // 用于生成轮廓的包围盒。
    if (!bounds.valid)
        return;  // 无图元，无法生成轮廓

    const auto makeModelId = [&component](int modelIndex) {
        // 使用组件名称和模型序号构造稳定的模型标识种子。
        const QString seed = modelIndex == 0 ? component.name + QStringLiteral("|body")
                                             : component.name + QStringLiteral("|body|") + QString::number(modelIndex);
        const QByteArray seedBytes = seed.toUtf8();  // 稳定 UUID 计算用的 UTF-8 种子。
        uint64_t hash = 0xCBF29CE484222325ULL;  // FNV-1a 哈希初始值。
        for (char byte : seedBytes) {
            hash ^= static_cast<uint8_t>(byte);
            hash *= 0x100000001B3ULL;
        }
        return QString("{%1-%2-%3-%4-%5}")
            .arg((hash >> 32) & 0xFFFFFFFF, 8, 16, QChar('0'))
            .arg((hash >> 16) & 0xFFFF, 4, 16, QChar('0'))
            .arg(hash & 0xFFFF, 4, 16, QChar('0'))
            .arg(((~hash) >> 48) & 0xFFFF, 4, 16, QChar('0'))
            .arg((~hash) & 0xFFFFFFFFFFFFLL, 12, 16, QChar('0'))
            .toUpper();
    };

    // 每个模型都需要独立的 ComponentBody，否则虽然模型数据写入了 Library/Models，
    // Altium 仍然只会通过第一个元件体显示第一个模型。
    for (int modelIndex = 0; modelIndex < component.models.size(); ++modelIndex) {
        auto& model = component.models[modelIndex];  // 当前待关联的三维模型。
        const QString modelId = makeModelId(modelIndex);  // 当前模型的稳定标识符。
        model.id = modelId;

        AltiumPcbComponentBody body;  // 当前模型对应的 ComponentBody 记录。
        body.modelId = modelId;
        body.modelName = model.name;
        body.model2dRotX = model.x;
        body.model2dRotY = model.y;
        body.model3dRotX = model.rotX;
        body.model3dRotY = model.rotY;
        body.model3dRotZ = model.rotZ;
        body.model3dDzRaw = AltiumCoord::mmToRaw(model.dz);
        body.overallHeightRaw = AltiumCoord::mmToRaw(qMax(component.height, 0.2));

        // 矩形轮廓
        body.outline = {QPointF(bounds.minX, bounds.minY),
                        QPointF(bounds.maxX, bounds.minY),
                        QPointF(bounds.maxX, bounds.maxY),
                        QPointF(bounds.minX, bounds.maxY)};

        component.bodies.append(body);
    }
}

}  // namespace EasyKiConverter
