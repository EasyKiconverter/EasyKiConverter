#include "AltiumPcbPrimitiveWriter.h"

#include "AltiumPcbLibWriter.h"
#include "utils/AltiumConstants.h"
#include "utils/AltiumLayerMap.h"

namespace EasyKiConverter {

/**
 * @brief 创建 PCB 图元写入器。
 * @param owner 所属的 PcbLib 主写入器。
 */
AltiumPcbPrimitiveWriter::AltiumPcbPrimitiveWriter(AltiumPcbLibWriter& owner) : m_owner(owner) {
    // 图元写入必须复用主写入器的文件级状态，避免层映射和字符串表出现分叉。
}

/**
 * @brief 写入一个焊盘图元及其扩展参数。
 * @param writer 目标二进制写入器。
 * @param pad 待写入的焊盘数据。
 */
void AltiumPcbPrimitiveWriter::writePad(AltiumBinaryWriter& writer, const AltiumPcbPad& pad) {
    writer.writeUInt8(AltiumConstants::PCB_OBJECT_PAD);

    // 子记录 1: Designator
    writer.writeStringBlock(pad.designator);

    // 子记录 2: PadSubrecord2（空）
    writer.writeStringBlock("");

    // 子记录 3: Net 字符串
    writer.writeStringBlock("|&|0");

    // 子记录 4: 标记字节
    writer.beginBlock();
    writer.writeUInt8(0);
    writer.endBlock();

    // 子记录 5: 主焊盘数据
    writer.beginBlock();
    {
        uint8_t layer = pad.isSMD ? pad.layer : AltiumConstants::PCB_LAYER_MULTI;  // 焊盘写入层。
        uint16_t flags =  // 焊盘图元标志位。
            m_owner.encodePrimitiveFlags(pad.isLocked, pad.isTentingTop, pad.isTentingBottom, pad.isKeepout);
        m_owner.writeCommonPrimitiveHeader(writer, layer, flags);

        // 位置
        writer.writeInt32(pad.locationX);
        writer.writeInt32(pad.locationY);

        // 三层尺寸
        writer.writeInt32(pad.sizeTopX);
        writer.writeInt32(pad.sizeTopY);
        writer.writeInt32(pad.sizeMidX);
        writer.writeInt32(pad.sizeMidY);
        writer.writeInt32(pad.sizeBotX);
        writer.writeInt32(pad.sizeBotY);

        // 孔径
        writer.writeInt32(pad.holeSize);

        // 三层形状
        writer.writeUInt8(pad.shapeTop);
        writer.writeUInt8(pad.shapeMid);
        writer.writeUInt8(pad.shapeBot);

        // 旋转 + 电镀
        writer.writeDouble(m_owner.normalizeFiniteValue(pad.rotation, 0.0, QStringLiteral("焊盘旋转角度")));
        writer.writeUInt8(pad.isPlated ? 1 : 0);

        // 补齐主记录固定布局（61 字节之后的字段）
        writer.writeUInt8(0);  // stack mode = Simple
        writer.writeUInt8(pad.mode);
        writer.writeUInt8(pad.powerPlaneConnectStyle);
        writer.writeInt32(pad.reliefAirGapRaw);
        writer.writeInt32(pad.reliefConductorWidthRaw);
        writer.writeInt16(pad.reliefEntries);
        writer.writeInt32(pad.powerPlaneClearanceRaw);
        writer.writeInt32(pad.powerPlaneReliefExpansionRaw);
        writer.writeInt32(0);  // reserved
        writer.writeInt32(pad.pasteMaskExpansionRaw);
        writer.writeInt32(pad.solderMaskExpansionRaw);
        writer.writeBytes(QByteArray(7, 0));  // reserved
        writer.writeUInt8(pad.pasteMaskExpansionRaw != 0 ? 2 : 0);  // paste mask expansion mode
        writer.writeUInt8(pad.solderMaskExpansionRaw != 0 ? 2 : 1);  // solder mask expansion mode
        writer.writeUInt8(pad.drillType);
        writer.writeInt16(0);  // reserved
        writer.writeInt32(0);  // reserved
        writer.writeInt16(0);  // jumper ID
        writer.writeInt16(0);  // reserved
    }
    writer.endBlock();

    // 子记录 6: 扩展块（各层尺寸/形状覆盖 + 孔元数据）
    writer.beginBlock();
    writePadExtendedBlock(writer, pad);
    writer.endBlock();
}

/**
 * @brief 写入焊盘扩展块
 * @details 包含各层尺寸/形状覆盖、孔形状/槽孔/旋转、圆角半径等。
 *          布局：29层尺寸X(116) + 29层尺寸Y(116) + 29层形状及保留位(30) +
 *                孔元数据(13) + 两组32层保留坐标(256) +
 *                圆角标记(1) + 32层形状(32) + 32层圆角(32) = 596
 * @param writer 目标二进制写入器。
 * @param pad 待写入扩展块的焊盘数据。
 */
void AltiumPcbPrimitiveWriter::writePadExtendedBlock(AltiumBinaryWriter& writer, const AltiumPcbPad& pad) {
    // 尺寸 X 覆盖：29 层 × 4 字节 = 116 字节
    for (int i = 0; i < AltiumConstants::PCB_PAD_LAYER_COUNT; ++i) {
        writer.writeInt32(pad.sizeMidX);
    }
    // 尺寸 Y 覆盖：29 层 × 4 字节 = 116 字节
    for (int i = 0; i < AltiumConstants::PCB_PAD_LAYER_COUNT; ++i) {
        writer.writeInt32(pad.sizeMidY);
    }
    // 形状覆盖：29 个铜层，随后一个保留字节
    for (int i = 0; i < AltiumConstants::PCB_PAD_LAYER_COUNT; ++i) {
        writer.writeUInt8(pad.shapeMid);
    }
    writer.writeUInt8(0);

    // 孔元数据
    writer.writeUInt8(pad.holeType);
    writer.writeInt32(pad.holeSlotLengthRaw);
    writer.writeDouble(m_owner.normalizeFiniteValue(pad.holeRotation, 0.0, QStringLiteral("焊盘孔旋转角度")));

    // 保留区域
    writer.writeBytes(QByteArray(32 * 4, 0));
    writer.writeBytes(QByteArray(32 * 4, 0));

    // 圆角矩形标记
    bool hasRoundedRect = (pad.shapeTop == AltiumConstants::PCB_PAD_SHAPE_ROUNDED_RECT) ||  // 是否存在圆角矩形。
                          (pad.shapeMid == AltiumConstants::PCB_PAD_SHAPE_ROUNDED_RECT) ||
                          (pad.shapeBot == AltiumConstants::PCB_PAD_SHAPE_ROUNDED_RECT);
    writer.writeUInt8(hasRoundedRect ? 1 : 0);

    // 各层形状列表（32 字节：top + 30*mid + bot）
    writer.writeUInt8(pad.shapeTop);
    for (int i = 0; i < 30; ++i) {
        writer.writeUInt8(pad.shapeMid);
    }
    writer.writeUInt8(pad.shapeBot);

    // 各层圆角半径百分比（32 字节）
    for (int i = 0; i < 32; ++i) {
        writer.writeUInt8(pad.cornerRadiusPercentage);
    }
}

/**
 * @brief 写入走线记录 (Object ID = 4)
 * @param writer 目标二进制写入器。
 * @param track 待写入的走线数据。
 * @param componentIndex 所属元件在库中的索引。
 */
void AltiumPcbPrimitiveWriter::writeTrack(AltiumBinaryWriter& writer, const AltiumPcbTrack& track, int componentIndex) {
    writer.writeUInt8(AltiumConstants::PCB_OBJECT_TRACK);

    writer.beginBlock();
    {
        uint16_t flags = m_owner.encodePrimitiveFlags(false, false, false, false);  // 走线图元标志位。
        m_owner.writeCommonPrimitiveHeader(writer, track.layer, flags);

        writer.writeInt32(track.startX);
        writer.writeInt32(track.startY);
        writer.writeInt32(track.endX);
        writer.writeInt32(track.endY);
        writer.writeInt32(track.width);

        writer.writeUInt16(track.netIndex);
        writer.writeUInt8(static_cast<uint8_t>(qBound(0, componentIndex, 255)));
    }
    writer.endBlock();
}

/**
 * @brief 写入弧线记录 (Object ID = 1)
 * @param writer 目标二进制写入器。
 * @param arc 待写入的弧线数据。
 */
void AltiumPcbPrimitiveWriter::writeArc(AltiumBinaryWriter& writer, const AltiumPcbArc& arc) {
    writer.writeUInt8(AltiumConstants::PCB_OBJECT_ARC);

    writer.beginBlock();
    {
        uint16_t flags = m_owner.encodePrimitiveFlags(false, false, false, false);  // 弧线图元标志位。
        m_owner.writeCommonPrimitiveHeader(writer, arc.layer, flags);

        writer.writeInt32(arc.centerX);
        writer.writeInt32(arc.centerY);
        writer.writeInt32(arc.radius);
        writer.writeDouble(m_owner.normalizeFiniteValue(arc.startAngle, 0.0, QStringLiteral("PCB 弧线起始角度")));
        writer.writeDouble(m_owner.normalizeFiniteValue(arc.endAngle, 360.0, QStringLiteral("PCB 弧线结束角度")));
        writer.writeInt32(arc.width);
    }
    writer.endBlock();
}

/**
 * @brief 写入文本记录 (Object ID = 5)
 * @param writer 目标二进制写入器。
 * @param text 待写入的文本数据。
 */
void AltiumPcbPrimitiveWriter::writeText(AltiumBinaryWriter& writer, const AltiumPcbText& text) {
    writer.writeUInt8(5);  // Object ID

    writer.beginBlock();
    {
        uint16_t flags = 0x08;  // 文本图元标志位。
        m_owner.writeCommonPrimitiveHeader(writer, text.layer, flags);

        writer.writeInt32(text.locationX);
        writer.writeInt32(text.locationY);
        writer.writeInt32(text.height);
        writer.writeInt16(0);  // font ID
        writer.writeDouble(m_owner.normalizeFiniteValue(text.rotation, 0.0, QStringLiteral("PCB 文本旋转角度")));
        writer.writeUInt8(text.isMirrored ? 1 : 0);
        writer.writeInt32(text.strokeWidth);
        writer.writeUInt8(0);  // is comment
        writer.writeUInt8(0);  // is designator
        writer.writeUInt8(0);  // char set
        writer.writeUInt8(0);  // base font type (Stroke)

        // 剩余填充到文本记录总大小（已写字段精确占 44 字节）。
        constexpr int WRITTEN_TEXT_HEADER_SIZE = 44;  ///< 已写入的文本头字节数。
        QByteArray padding(AltiumConstants::PCB_TEXT_RECORD_SIZE - WRITTEN_TEXT_HEADER_SIZE, 0);  // 文本尾部填充。
        // wide string index
        int wsIdx = m_owner.addWideString(text.text);  // 宽字符串索引。
        int wsBase = AltiumConstants::PCB_TEXT_WS_INDEX_FIELD_OFFSET - WRITTEN_TEXT_HEADER_SIZE;  // 索引字段偏移。
        padding[wsBase] = static_cast<char>(wsIdx & 0xFF);
        padding[wsBase + 1] = static_cast<char>((wsIdx >> 8) & 0xFF);
        padding[wsBase + 2] = static_cast<char>((wsIdx >> 16) & 0xFF);
        padding[wsBase + 3] = static_cast<char>((wsIdx >> 24) & 0xFF);
        // text kind = Stroke
        padding[AltiumConstants::PCB_TEXT_KIND_FIELD_OFFSET - WRITTEN_TEXT_HEADER_SIZE] = 0;
        // V7 layer ID
        uint32_t v7id = m_owner.toV7LayerId(text.layer);  // 文本对应的 V7 层标识。
        int v7Base = AltiumConstants::PCB_TEXT_V7LAYER_FIELD_OFFSET - WRITTEN_TEXT_HEADER_SIZE;  // 层字段偏移。
        padding[v7Base] = static_cast<char>(v7id & 0xFF);
        padding[v7Base + 1] = static_cast<char>((v7id >> 8) & 0xFF);
        padding[v7Base + 2] = static_cast<char>((v7id >> 16) & 0xFF);
        padding[v7Base + 3] = static_cast<char>((v7id >> 24) & 0xFF);

        writer.writeBytes(padding);
    }
    writer.endBlock();

    // 文本字符串块
    writer.writeStringBlock(text.text);
}

/**
 * @brief 写入填充记录 (Object ID = 6, 50 字节)
 * @param writer 目标二进制写入器。
 * @param fill 待写入的填充数据。
 */
void AltiumPcbPrimitiveWriter::writeFill(AltiumBinaryWriter& writer, const AltiumPcbFill& fill) {
    writer.writeUInt8(6);  // Object ID

    writer.beginBlock();
    {
        uint16_t flags = 0x08;  // 填充图元标志位。
        m_owner.writeCommonPrimitiveHeader(writer, fill.layer, flags);

        writer.writeInt32(fill.corner1X);
        writer.writeInt32(fill.corner1Y);
        writer.writeInt32(fill.corner2X);
        writer.writeInt32(fill.corner2Y);
        writer.writeDouble(m_owner.normalizeFiniteValue(fill.rotation, 0.0, QStringLiteral("PCB 填充旋转角度")));
        writer.writeInt32(0);  // solder mask expansion
        writer.writeUInt8(0);  // paste mask expansion
        writer.writeUInt32(m_owner.toV7LayerId(fill.layer));
        writer.writeUInt8(0);  // keepout restrictions
        writer.writeBytes(QByteArray(3, 0));  // reserved
    }
    writer.endBlock();
}

/**
 * @brief 写入区域记录 (Object ID = 11)
 * @param writer 目标二进制写入器。
 * @param region 待写入的区域数据。
 */
void AltiumPcbPrimitiveWriter::writeRegion(AltiumBinaryWriter& writer, const AltiumPcbRegion& region) {
    writer.writeUInt8(AltiumConstants::PCB_OBJECT_REGION);

    writer.beginBlock();
    {
        uint16_t flags = m_owner.encodePrimitiveFlags(false, false, false, false);  // 区域图元标志位。
        m_owner.writeCommonPrimitiveHeader(writer, region.layer, flags);

        writer.writeUInt32(0);
        writer.writeUInt8(0);

        // 嵌套参数块
        QMap<QString, QString> params;  // 区域参数键值表。
        // 根据显式层名称或 Altium 层映射确定 V7 层名称。
        QString v7Name = region.v7LayerName.isEmpty() ? AltiumLayerMap::toLayerName(region.layer)
                                                      : region.v7LayerName;  ///< V7 层名称。
        params["V7_LAYER"] = v7Name;
        params["KIND"] = QString::number(region.kind);
        params["SUBPOLYINDEX"] = "0";
        params["UNIONINDEX"] = "0";
        params["ARCRESOLUTION"] = "0mil";
        if (region.isBoardCutout)
            params["ISBOARDCUTOUT"] = "TRUE";
        if (!region.net.isEmpty())
            params["NET"] = region.net;
        if (!region.uniqueId.isEmpty())
            params["UNIQUEID"] = region.uniqueId;
        if (!region.name.isEmpty())
            params["NAME"] = region.name;
        writer.writeCStringParameterBlock(params);

        // 轮廓顶点
        writer.writeUInt32(static_cast<uint32_t>(region.vertices.size()));
        // 按输入顺序写入区域轮廓的每个顶点。
        for (const QPointF& v : region.vertices) {  // 当前待写入的区域轮廓顶点。
            writer.writeDouble(m_owner.normalizeFiniteValue(v.x(), 0.0, QStringLiteral("PCB 区域 X 坐标")));
            writer.writeDouble(m_owner.normalizeFiniteValue(v.y(), 0.0, QStringLiteral("PCB 区域 Y 坐标")));
        }
    }
    writer.endBlock();
}

/**
 * @brief 写入 3D 元件体记录 (Object ID = 12)
 * @param writer 目标二进制写入器。
 * @param body 待写入的 3D 元件体数据。
 */
void AltiumPcbPrimitiveWriter::writeComponentBody(AltiumBinaryWriter& writer, const AltiumPcbComponentBody& body) {
    writer.writeUInt8(AltiumConstants::PCB_OBJECT_COMPONENT_BODY);

    writer.beginBlock();
    {
        uint16_t flags = m_owner.encodePrimitiveFlags(false, false, false, false);  // 元件体图元标志位。
        uint8_t layer = 57;  // 默认机械层编号。
        const QString normalizedLayer = body.layerName.trimmed().toUpper();  // 规范化后的机械层名称。
        if (normalizedLayer.startsWith("MECHANICAL")) {
            bool ok = false;  // 机械层编号转换是否成功。
            const int number = normalizedLayer.mid(10).toInt(&ok);  // 解析得到的机械层编号。
            if (ok && number >= 1 && number <= 16) {
                layer = static_cast<uint8_t>(56 + number);
            }
        }
        m_owner.writeCommonPrimitiveHeader(writer, layer, flags);

        writer.writeUInt32(0);  // reserved
        writer.writeUInt8(0);  // reserved

        QMap<QString, QString> params;  // 元件体参数键值表。
        params["V7_LAYER"] = normalizedLayer;
        params["NAME"] = body.name;
        params["KIND"] = QString::number(body.kind);
        params["SUBPOLYINDEX"] = QString::number(body.subpolyIndex);
        params["UNIONINDEX"] = QString::number(body.unionIndex);
        params["ARCRESOLUTION"] = QString::number(body.arcResolutionRaw / 10000.0, 'f', 4) + "mil";
        params["ISSHAPEBASED"] = body.isShapeBased ? "TRUE" : "FALSE";
        params["CAVITYHEIGHT"] = QString::number(body.cavityHeightRaw / 10000.0, 'f', 4) + "mil";
        params["STANDOFFHEIGHT"] = QString::number(body.standoffHeightRaw / 10000.0, 'f', 4) + "mil";
        params["OVERALLHEIGHT"] = QString::number(body.overallHeightRaw / 10000.0, 'f', 4) + "mil";
        params["BODYCOLOR3D"] = QString::number(body.bodyColor3d);
        params["BODYOPACITY3D"] = QString::number(
            m_owner.normalizeFiniteValue(body.bodyOpacity3d, 1.0, QStringLiteral("3D 元件体不透明度")), 'f', 3);
        params["BODYPROJECTION"] = QString::number(body.bodyProjection);
        params["IDENTIFIER"] = "";
        params["TEXTURE"] = "";
        params["TEXTURECENTERX"] = "0mil";
        params["TEXTURECENTERY"] = "0mil";
        params["TEXTURESIZEX"] = "0mil";
        params["TEXTURESIZEY"] = "0mil";
        params["TEXTUREROTATION"] = "0.000000";
        params["MODELID"] = body.modelId;
        params["MODEL.CHECKSUM"] = QString::number(body.modelChecksum);
        params["MODEL.EMBED"] = body.modelEmbed ? "TRUE" : "FALSE";
        params["MODEL.NAME"] = body.modelName;
        params["MODEL.2D.X"] = QString::number(body.model2dRotX / 10000.0, 'f', 4) + "mil";
        params["MODEL.2D.Y"] = QString::number(body.model2dRotY / 10000.0, 'f', 4) + "mil";
        params["MODEL.2D.ROTATION"] = QString::number(
            m_owner.normalizeFiniteValue(body.model2dRotation, 0.0, QStringLiteral("3D 元件体二维旋转")), 'f', 3);
        params["MODEL.3D.ROTX"] = QString::number(
            m_owner.normalizeFiniteValue(body.model3dRotX, 0.0, QStringLiteral("3D 元件体 X 旋转")), 'f', 3);
        params["MODEL.3D.ROTY"] = QString::number(
            m_owner.normalizeFiniteValue(body.model3dRotY, 0.0, QStringLiteral("3D 元件体 Y 旋转")), 'f', 3);
        params["MODEL.3D.ROTZ"] = QString::number(
            m_owner.normalizeFiniteValue(body.model3dRotZ, 0.0, QStringLiteral("3D 元件体 Z 旋转")), 'f', 3);
        params["MODEL.3D.DZ"] = QString::number(body.model3dDzRaw / 10000.0, 'f', 4) + "mil";
        params["MODEL.MODELTYPE"] = QString::number(body.modelType);
        params["MODEL.MODELSOURCE"] = body.modelSource;
        writer.writeCStringParameterBlock(params);

        // 轮廓顶点
        writer.writeUInt32(static_cast<uint32_t>(body.outline.size()));
        for (const QPointF& v : body.outline) {
            writer.writeDouble(m_owner.normalizeFiniteValue(v.x(), 0.0, QStringLiteral("3D 元件体轮廓 X 坐标")));
            writer.writeDouble(m_owner.normalizeFiniteValue(v.y(), 0.0, QStringLiteral("3D 元件体轮廓 Y 坐标")));
        }
    }
    writer.endBlock();
}

/**
 * @brief 写入图元唯一标识信息
 * @details 为每个图元生成 PRIMITIVEOBJECTID 条目，用于 Altium 内部引用追踪。
 */

}  // namespace EasyKiConverter
