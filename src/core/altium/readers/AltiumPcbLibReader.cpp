#include "AltiumPcbLibReader.h"

#include "core/altium/utils/AltiumBinaryReader.h"
#include "core/altium/utils/AltiumConstants.h"

#include <QSet>

#include <cmath>

namespace EasyKiConverter {

namespace {

/**
 * @brief 返回每种 PcbLib 图元对应的子块数量
 * @param objectId 图元对象类型编号
 * @return 该类型的子块数量；未知类型返回 -1
 */
int primitiveBlockCount(quint8 objectId) {
    // 不同图元的块布局固定，焊盘和文本包含额外的字符串或扩展块。
    switch (objectId) {
        case AltiumConstants::PCB_OBJECT_ARC:
        case AltiumConstants::PCB_OBJECT_VIA:
        case AltiumConstants::PCB_OBJECT_TRACK:
        case AltiumConstants::PCB_OBJECT_FILL:
        case AltiumConstants::PCB_OBJECT_REGION:
        case AltiumConstants::PCB_OBJECT_COMPONENT_BODY:
            return 1;
        case AltiumConstants::PCB_OBJECT_PAD:
            return 6;
        case AltiumConstants::PCB_OBJECT_TEXT:
            return 2;
        default:
            return -1;
    }
}

/**
 * @brief 解析不含长度字段的字符串子块载荷
 * @param payload 字符串子块载荷
 * @param value 用于接收字符串的输出指针
 * @return 载荷格式有效时返回 true
 */
bool readStringBlockPayload(const QByteArray& payload, QString* value) {
    AltiumBinaryReader reader(payload);
    uint8_t stringSize = 0;  // 字符串字节数。
    QByteArray stringData;  // 字符串原始字节。
    if (!reader.readUInt8(&stringSize) || !reader.readBytes(stringSize, &stringData) || reader.remaining() != 0)
        return false;
    if (value != nullptr)
        *value = QString::fromLatin1(stringData);
    return true;
}

/**
 * @brief 判断载荷是否符合字符串子块的长度和边界约束
 * @param payload 待检查的字符串子块载荷
 * @return 载荷有效时返回 true
 */
bool isStringBlockPayload(const QByteArray& payload) {
    return readStringBlockPayload(payload, nullptr);
}

/**
 * @brief 读取 WideStrings 参数块中的连续文本索引
 * @param data WideStrings 参数块数据
 * @param indices 用于接收文本索引集合的输出指针
 * @return 索引连续且格式有效时返回 true
 */
bool readWideStringIndices(const QByteArray& data, QSet<quint32>* indices) {
    if (indices == nullptr)
        return false;
    indices->clear();

    AltiumBinaryReader reader(data);
    QMap<QString, QString> parameters;  // 参数块键值表。
    if (!reader.readCStringParameterBlock(&parameters) || reader.hasError() || reader.remaining() != 0)
        return false;

    for (auto it = parameters.cbegin(); it != parameters.cend(); ++it) {
        if (!it.key().startsWith(QStringLiteral("ENCODEDTEXT")))
            continue;
        const QString suffix = it.key().mid(QStringLiteral("ENCODEDTEXT").size());  // 文本索引后缀。
        bool ok = false;  // 文本索引转换是否成功。
        const quint32 index = suffix.toUInt(&ok);  // 解析得到的文本索引。
        if (!ok || suffix.isEmpty() || indices->contains(index))
            return false;
        indices->insert(index);
    }
    for (quint32 expected = 0; expected < static_cast<quint32>(indices->size()); ++expected) {
        if (!indices->contains(expected))
            return false;
    }
    return true;
}

/**
 * @brief 读取所有 PcbLib 图元共享的层号、标志和保留字段
 * @param reader 当前图元数据读取器
 * @param layer 用于接收层编号的输出指针
 * @param flags 用于接收图元标志的输出指针
 * @return 公共头部读取成功时返回 true
 */
bool readCommonPrimitiveHeader(AltiumBinaryReader& reader, quint8* layer, quint16* flags) {
    if (layer == nullptr || flags == nullptr)
        return false;

    QByteArray objectIndexes;  // 公共头部中的保留对象索引字节。
    return reader.readUInt8(layer) && reader.readUInt16(flags) && reader.readBytes(10, &objectIndexes);
}

/**
 * @brief 从独立载荷创建读取器并解析公共图元头部
 * @param payload 图元公共头部载荷
 * @param layer 用于接收层编号的输出指针
 * @param flags 用于接收图元标志的输出指针
 * @return 公共头部读取成功时返回 true
 */
bool readCommonPrimitiveHeader(const QByteArray& payload, quint8* layer, quint16* flags) {
    AltiumBinaryReader reader(payload);
    return readCommonPrimitiveHeader(reader, layer, flags);
}

/**
 * @brief 解析走线图元的坐标、宽度和连接索引字段
 * @param payload 走线图元载荷
 * @param fields 用于接收结构化字段的输出指针
 * @return 字段读取成功时返回 true
 */
bool parseTrackFields(const QByteArray& payload, AltiumPcbLibReader::TrackFields* fields) {
    if (fields == nullptr)
        return false;
    AltiumBinaryReader reader(payload);
    quint8 layer = 0;  // 走线所在层。
    quint16 flags = 0;  // 走线公共标志。
    return readCommonPrimitiveHeader(reader, &layer, &flags) && reader.readInt32(&fields->startX) &&
           reader.readInt32(&fields->startY) && reader.readInt32(&fields->endX) && reader.readInt32(&fields->endY) &&
           reader.readInt32(&fields->width) && reader.readUInt16(&fields->netIndex) &&
           reader.readUInt8(&fields->componentIndex);
}

/**
 * @brief 解析弧线图元的圆心、半径、角度和线宽字段
 * @param payload 弧线图元载荷
 * @param fields 用于接收结构化字段的输出指针
 * @return 字段读取成功时返回 true
 */
bool parseArcFields(const QByteArray& payload, AltiumPcbLibReader::ArcFields* fields) {
    if (fields == nullptr)
        return false;
    AltiumBinaryReader reader(payload);
    quint8 layer = 0;  // 弧线所在层。
    quint16 flags = 0;  // 弧线公共标志。
    return readCommonPrimitiveHeader(reader, &layer, &flags) && reader.readInt32(&fields->centerX) &&
           reader.readInt32(&fields->centerY) && reader.readInt32(&fields->radius) &&
           reader.readDouble(&fields->startAngle) && reader.readDouble(&fields->endAngle) &&
           reader.readInt32(&fields->width);
}

/**
 * @brief 解析焊盘主块中的尺寸、形状、孔和扩展属性字段
 * @param payload 焊盘主块载荷
 * @param fields 用于接收结构化字段的输出指针
 * @return 字段读取成功时返回 true
 */
bool parsePadMainFields(const QByteArray& payload, AltiumPcbLibReader::PadFields* fields) {
    if (fields == nullptr)
        return false;
    AltiumBinaryReader reader(payload);
    quint8 layer = 0;  // 焊盘所在层。
    quint16 flags = 0;  // 焊盘公共标志。
    QByteArray reserved;  // 主块中的保留字节。
    return readCommonPrimitiveHeader(reader, &layer, &flags) && reader.readInt32(&fields->locationX) &&
           reader.readInt32(&fields->locationY) && reader.readInt32(&fields->sizeTopX) &&
           reader.readInt32(&fields->sizeTopY) && reader.readInt32(&fields->sizeMidX) &&
           reader.readInt32(&fields->sizeMidY) && reader.readInt32(&fields->sizeBotX) &&
           reader.readInt32(&fields->sizeBotY) && reader.readInt32(&fields->holeSize) &&
           reader.readUInt8(&fields->shapeTop) && reader.readUInt8(&fields->shapeMid) &&
           reader.readUInt8(&fields->shapeBot) && reader.readDouble(&fields->rotation) &&
           reader.readUInt8(&fields->isPlated) && reader.readUInt8(&fields->stackMode) &&
           reader.readUInt8(&fields->mode) && reader.readUInt8(&fields->powerPlaneConnectStyle) &&
           reader.readInt32(&fields->reliefAirGapRaw) && reader.readInt32(&fields->reliefConductorWidthRaw) &&
           reader.readInt16(&fields->reliefEntries) && reader.readInt32(&fields->powerPlaneClearanceRaw) &&
           reader.readInt32(&fields->powerPlaneReliefExpansionRaw) &&
           reader.readInt32(&fields->pasteMaskExpansionRaw) && reader.readInt32(&fields->solderMaskExpansionRaw) &&
           reader.readBytes(7, &reserved) && reader.readUInt8(&fields->pasteMaskExpansionMode) &&
           reader.readUInt8(&fields->solderMaskExpansionMode) && reader.readUInt8(&fields->drillType) &&
           reader.readBytes(2, &reserved) && reader.readBytes(4, &reserved) && reader.readBytes(2, &reserved) &&
           reader.readBytes(2, &reserved);
}

/**
 * @brief 解析焊盘扩展块中的各层覆盖、孔型和圆角属性
 * @param payload 焊盘扩展块载荷
 * @param fields 用于接收结构化字段的输出指针
 * @return 字段读取成功时返回 true
 */
bool parsePadExtendedFields(const QByteArray& payload, AltiumPcbLibReader::PadFields* fields) {
    if (fields == nullptr)
        return false;
    AltiumBinaryReader reader(payload);
    QByteArray reserved;  // 扩展块中的保留字节。
    qint32 reservedInt32 = 0;  // 扩展块中的保留整数。
    quint8 reservedByte = 0;  // 扩展块中的保留字节值。
    for (int i = 0; i < 29; ++i) {
        if (!reader.readInt32(&reservedInt32))
            return false;
    }
    for (int i = 0; i < 29; ++i) {
        if (!reader.readInt32(&reservedInt32))
            return false;
    }
    for (int i = 0; i < 29; ++i) {
        if (!reader.readUInt8(&fields->extendedShapeMid))
            return false;
    }
    if (!reader.readUInt8(&reservedByte))
        return false;
    if (!reader.readUInt8(&fields->holeType) || !reader.readInt32(&fields->holeSlotLengthRaw) ||
        !reader.readDouble(&fields->holeRotation) || !reader.readBytes(32 * 4, &reserved) ||
        !reader.readBytes(32 * 4, &reserved) || !reader.readUInt8(&fields->hasRoundedRect) ||
        !reader.readUInt8(&fields->extendedShapeTop))
        return false;
    for (int i = 0; i < 30; ++i) {
        if (!reader.readUInt8(&fields->extendedShapeMid))
            return false;
    }
    if (!reader.readUInt8(&fields->extendedShapeBottom))
        return false;
    for (int i = 0; i < 32; ++i) {
        if (!reader.readUInt8(&fields->cornerRadiusPercentage))
            return false;
    }
    return true;
}

/**
 * @brief 读取带数量前缀的二维顶点列表，并拒绝尾部未消费数据
 * @param reader 当前顶点数据读取器
 * @param vertices 用于接收顶点列表的输出指针
 * @return 顶点列表读取成功时返回 true
 */
bool readVertexList(AltiumBinaryReader& reader, QVector<QPointF>* vertices) {
    if (vertices == nullptr)
        return false;
    quint32 count = 0;  // 顶点数量。
    if (!reader.readUInt32(&count) || count > static_cast<quint32>(reader.remaining() / 16))
        return false;
    vertices->clear();
    vertices->reserve(static_cast<int>(count));
    for (quint32 i = 0; i < count; ++i) {
        double x = 0.0;  // 当前顶点 X 坐标。
        double y = 0.0;  // 当前顶点 Y 坐标。
        if (!reader.readDouble(&x) || !reader.readDouble(&y))
            return false;
        vertices->append(QPointF(x, y));
    }
    return reader.remaining() == 0;
}

/**
 * @brief 解析填充图元的边界、旋转、层标识和掩膜属性
 * @param payload 填充图元载荷
 * @param fields 用于接收结构化字段的输出指针
 * @return 字段读取成功时返回 true
 */
bool parseFillFields(const QByteArray& payload, AltiumPcbLibReader::FillFields* fields) {
    if (fields == nullptr)
        return false;
    AltiumBinaryReader reader(payload);
    quint8 layer = 0;  // 填充所在层。
    quint16 flags = 0;  // 填充公共标志。
    QByteArray reserved;  // 填充块中的保留字节。
    return readCommonPrimitiveHeader(reader, &layer, &flags) && reader.readInt32(&fields->corner1X) &&
           reader.readInt32(&fields->corner1Y) && reader.readInt32(&fields->corner2X) &&
           reader.readInt32(&fields->corner2Y) && reader.readDouble(&fields->rotation) &&
           reader.readInt32(&fields->solderMaskExpansionRaw) && reader.readUInt8(&fields->pasteMaskExpansion) &&
           reader.readUInt32(&fields->v7LayerId) && reader.readUInt8(&fields->keepoutRestrictions) &&
           reader.readBytes(3, &reserved) && reader.remaining() == 0;
}

/**
 * @brief 解析区域图元的参数块和轮廓顶点
 * @param payload 区域图元载荷
 * @param fields 用于接收结构化字段的输出指针
 * @return 字段读取成功时返回 true
 */
bool parseRegionFields(const QByteArray& payload, AltiumPcbLibReader::RegionFields* fields) {
    if (fields == nullptr)
        return false;
    AltiumBinaryReader reader(payload);
    quint8 layer = 0;  // 区域所在层。
    quint16 flags = 0;  // 区域公共标志。
    return readCommonPrimitiveHeader(reader, &layer, &flags) && reader.readUInt32(&fields->reserved) &&
           reader.readUInt8(&fields->reservedByte) && reader.readCStringParameterBlock(&fields->parameters) &&
           readVertexList(reader, &fields->vertices);
}

/**
 * @brief 解析三维元件体的参数块和外形轮廓
 * @param payload 元件体图元载荷
 * @param fields 用于接收结构化字段的输出指针
 * @return 字段读取成功时返回 true
 */
bool parseComponentBodyFields(const QByteArray& payload, AltiumPcbLibReader::ComponentBodyFields* fields) {
    if (fields == nullptr)
        return false;
    AltiumBinaryReader reader(payload);
    quint8 layer = 0;  // 元件体所在层。
    quint16 flags = 0;  // 元件体公共标志。
    return readCommonPrimitiveHeader(reader, &layer, &flags) && reader.readUInt32(&fields->reserved) &&
           reader.readUInt8(&fields->reservedByte) && reader.readCStringParameterBlock(&fields->parameters) &&
           readVertexList(reader, &fields->outline);
}

/**
 * @brief 解析文本图元的排版、字体、旋转和宽字符串索引
 * @param payload 文本图元载荷
 * @param fields 用于接收结构化字段的输出指针
 * @return 字段读取成功时返回 true
 */
bool parseTextFields(const QByteArray& payload, AltiumPcbLibReader::TextFields* fields) {
    if (fields == nullptr)
        return false;
    AltiumBinaryReader reader(payload);
    quint8 layer = 0;  // 文本所在层。
    quint16 flags = 0;  // 文本公共标志。
    QByteArray padding;  // 文本块中的保留字节。
    return readCommonPrimitiveHeader(reader, &layer, &flags) && reader.readInt32(&fields->locationX) &&
           reader.readInt32(&fields->locationY) && reader.readInt32(&fields->height) &&
           reader.readInt16(&fields->fontId) && reader.readDouble(&fields->rotation) &&
           reader.readUInt8(&fields->mirrored) && reader.readInt32(&fields->strokeWidth) &&
           reader.readUInt8(&fields->isComment) && reader.readUInt8(&fields->isDesignator) &&
           reader.readUInt8(&fields->characterSet) && reader.readUInt8(&fields->baseFontType) &&
           reader.readBytes(71, &padding) && reader.readUInt32(&fields->wideStringIndex) &&
           reader.readBytes(41, &padding) && reader.readUInt8(&fields->kind) && reader.readBytes(65, &padding) &&
           reader.readUInt32(&fields->v7LayerId);
}

/**
 * @brief 校验已解析图元的层号、几何尺寸、枚举值和数值范围
 * @param object 待校验的图元记录
 * @param error 用于接收具体错误说明的输出指针
 * @return 图元字段有效时返回 true
 */
bool validatePrimitiveFields(const AltiumPcbLibReader::PrimitiveRecord& object, QString* error) {
    auto reject = [error](const QString& message) {  // 记录校验错误并返回失败。
        if (error != nullptr)
            *error = message;
        return false;
    };

    if (object.hasTrackFields || object.hasArcFields || object.hasPadFields || object.hasFillFields ||
        object.hasRegionFields || object.hasComponentBodyFields || object.hasTextFields) {
        if (object.layer < 1 || object.layer > 74)
            return reject(QStringLiteral("PcbLib 图元层号必须在 1..74 范围内"));
    }

    if (object.hasArcFields) {
        if (object.arc.radius <= 0)
            return reject(QStringLiteral("PcbLib 弧线半径必须为正"));
        if (object.arc.width <= 0)
            return reject(QStringLiteral("PcbLib 弧线宽度必须为正"));
        if (!std::isfinite(object.arc.startAngle) || !std::isfinite(object.arc.endAngle))
            return reject(QStringLiteral("PcbLib 弧线角度必须为有限值"));
    }
    if (object.hasTrackFields && object.track.width <= 0)
        return reject(QStringLiteral("PcbLib 走线宽度必须为正"));
    if (object.hasPadFields) {
        const auto& pad = object.pad;  // 当前焊盘字段。
        const auto isValidPadShape = [](quint8 shape) {  // 检查焊盘形状编码。
            return shape == AltiumConstants::PCB_PAD_SHAPE_ROUND ||
                   shape == AltiumConstants::PCB_PAD_SHAPE_RECTANGULAR ||
                   shape == AltiumConstants::PCB_PAD_SHAPE_OCTAGONAL ||
                   shape == AltiumConstants::PCB_PAD_SHAPE_ROUNDED_RECT;
        };
        if (pad.sizeTopX <= 0 || pad.sizeTopY <= 0 || pad.sizeMidX <= 0 || pad.sizeMidY <= 0 || pad.sizeBotX <= 0 ||
            pad.sizeBotY <= 0)
            return reject(QStringLiteral("PcbLib 焊盘尺寸必须为正"));
        if (!isValidPadShape(pad.shapeTop) || !isValidPadShape(pad.shapeMid) || !isValidPadShape(pad.shapeBot))
            return reject(QStringLiteral("PcbLib 焊盘形状无效"));
        if (pad.holeSize < 0 || pad.holeSlotLengthRaw < 0)
            return reject(QStringLiteral("PcbLib 焊盘孔尺寸不能为负"));
        if (object.layer == AltiumConstants::PCB_LAYER_MULTI && pad.holeSize <= 0)
            return reject(QStringLiteral("PcbLib 通孔焊盘孔径必须为正"));
        if (pad.cornerRadiusPercentage > 100 || pad.mode > 3 || pad.powerPlaneConnectStyle > 2 ||
            (pad.reliefEntries != 2 && pad.reliefEntries != 4) || pad.drillType > 2 || pad.holeType > 2)
            return reject(QStringLiteral("PcbLib 焊盘扩展属性无效"));
        if (pad.holeType == 2 && pad.holeSlotLengthRaw <= 0)
            return reject(QStringLiteral("PcbLib 焊盘槽孔长度必须为正"));
        if (!std::isfinite(pad.rotation) || !std::isfinite(pad.holeRotation))
            return reject(QStringLiteral("PcbLib 焊盘旋转角度必须为有限值"));
    }
    if (object.hasFillFields && !std::isfinite(object.fill.rotation))
        return reject(QStringLiteral("PcbLib 填充旋转角度必须为有限值"));
    if (object.hasTextFields) {
        if (object.textFields.height <= 0 || object.textFields.strokeWidth < 0)
            return reject(QStringLiteral("PcbLib 文本尺寸必须有效"));
        if (!std::isfinite(object.textFields.rotation))
            return reject(QStringLiteral("PcbLib 文本旋转角度必须为有限值"));
    }
    if (object.hasRegionFields) {
        if (object.region.vertices.size() < 3)
            return reject(QStringLiteral("PcbLib 区域至少需要三个顶点"));
        for (const QPointF& vertex : object.region.vertices) {
            if (!std::isfinite(vertex.x()) || !std::isfinite(vertex.y()))
                return reject(QStringLiteral("PcbLib 区域顶点必须为有限值"));
        }
    }
    if (object.hasComponentBodyFields) {
        for (const QPointF& vertex : object.componentBody.outline) {
            if (!std::isfinite(vertex.x()) || !std::isfinite(vertex.y()))
                return reject(QStringLiteral("PcbLib 三维元件体轮廓顶点必须为有限值"));
        }
        const auto& parameters = object.componentBody.parameters;  // 元件体参数表。
        if (parameters.contains(QStringLiteral("BODYOPACITY3D"))) {
            bool opacityOk = false;  // 透明度文本转换是否成功。
            const double opacity =
                parameters.value(QStringLiteral("BODYOPACITY3D")).toDouble(&opacityOk);  // 透明度值。
            if (!opacityOk || !std::isfinite(opacity) || opacity < 0.0 || opacity > 1.0)
                return reject(QStringLiteral("PcbLib 三维元件体透明度必须在 0..1 范围内"));
        }
    }
    return true;
}

}  // namespace

/**
 * @brief 清理已读取内容并记录 PcbLib 解析失败原因
 * @param message 解析失败说明
 * @return 始终返回 false，便于错误路径直接返回
 */
bool AltiumPcbLibReader::fail(const QString& message) {
    m_components.clear();
    m_fileVersion.clear();
    m_libraryMetadata.clear();
    m_errorMessage = message;
    return false;
}

/**
 * @brief 打开 PcbLib 容器，读取库头、封装名称和存储键映射
 * @param filePath PcbLib 文件路径
 * @return 文件结构和封装映射有效时返回 true
 */
bool AltiumPcbLibReader::open(const QString& filePath) {
    m_components.clear();
    m_fileVersion.clear();
    m_libraryMetadata.clear();
    m_errorMessage.clear();

    if (!m_oleReader.open(filePath))
        return fail(m_oleReader.errorString());

    QByteArray fileHeaderData;  // FileHeader 流数据。
    if (!m_oleReader.readStream(QStringLiteral("FileHeader"), &fileHeaderData))
        return fail(QStringLiteral("PcbLib 缺少 FileHeader 流"));
    AltiumBinaryReader fileHeaderReader(fileHeaderData);
    int32_t versionLength = 0;  // 版本字符串长度。
    if (!fileHeaderReader.readInt32(&versionLength) || versionLength < 0 || versionLength > 255 ||
        !fileHeaderReader.readPascalShortString(&m_fileVersion) || m_fileVersion.toLatin1().size() != versionLength ||
        fileHeaderReader.remaining() != 0)
        return fail(QStringLiteral("PcbLib FileHeader 无效"));

    QByteArray libraryData;  // Library/Data 流数据。
    if (!m_oleReader.readStream(QStringLiteral("Library/Data"), &libraryData))
        return fail(QStringLiteral("PcbLib 缺少 Library/Data 流"));
    AltiumBinaryReader libraryReader(libraryData);
    if (!libraryReader.readBlock(&m_libraryMetadata))
        return fail(QStringLiteral("PcbLib Library/Data 元数据块无效: %1").arg(libraryReader.errorString()));
    if (!m_libraryMetadata.isEmpty() && m_libraryMetadata.endsWith('\0'))
        m_libraryMetadata.chop(1);

    uint32_t componentCount = 0;  // 库中封装数量。
    if (!libraryReader.readUInt32(&componentCount) ||
        componentCount > static_cast<uint32_t>(libraryReader.remaining() / 5))
        return fail(QStringLiteral("PcbLib Library/Data 的封装数量无效"));

    QVector<QString> names;  // 封装名称列表。
    names.reserve(static_cast<int>(componentCount));
    QSet<QString> componentNames;  // 大小写折叠后的封装名称集合。
    for (uint32_t i = 0; i < componentCount; ++i) {
        QString name;  // 当前封装名称。
        if (!libraryReader.readStringBlock(&name) || name.isEmpty())
            return fail(QStringLiteral("PcbLib Library/Data 的封装名称无效"));
        const QString foldedName = name.toCaseFolded();  // 用于重复检查的折叠名称。
        if (componentNames.contains(foldedName))
            return fail(QStringLiteral("PcbLib Library/Data 的封装名称重复: %1").arg(name));
        componentNames.insert(foldedName);
        names.append(name);
    }
    if (libraryReader.hasError() || libraryReader.remaining() != 0)
        return fail(QStringLiteral("PcbLib Library/Data 末尾包含无效数据"));

    QMap<QString, QString> sectionKeys;  // 封装名称到 OLE 节名称的映射。
    if (m_oleReader.containsStream(QStringLiteral("SectionKeys"))) {
        QByteArray sectionKeyData;  // SectionKeys 流数据。
        if (!m_oleReader.readStream(QStringLiteral("SectionKeys"), &sectionKeyData))
            return fail(QStringLiteral("无法读取 PcbLib SectionKeys 流"));
        AltiumBinaryReader sectionKeyReader(sectionKeyData);
        uint32_t keyCount = 0;  // 节映射数量。
        if (!sectionKeyReader.readUInt32(&keyCount) ||
            keyCount > static_cast<uint32_t>(sectionKeyReader.remaining() / 9))
            return fail(QStringLiteral("PcbLib SectionKeys 的数量无效"));
        QSet<QString> mappedComponents;  // 已映射的封装名称集合。
        QSet<QString> mappedSectionKeys;  // 已使用的节名称集合。
        for (uint32_t i = 0; i < keyCount; ++i) {
            QString libRef;  // SectionKeys 中的封装名称。
            QString sectionKey;  // 对应的 OLE 节名称。
            if (!sectionKeyReader.readPascalString(&libRef) || !sectionKeyReader.readStringBlock(&sectionKey) ||
                libRef.isEmpty() || sectionKey.isEmpty())
                return fail(QStringLiteral("PcbLib SectionKeys 映射不完整"));
            if (!names.contains(libRef))
                return fail(QStringLiteral("PcbLib SectionKeys 包含未知封装: %1").arg(libRef));
            if (mappedComponents.contains(libRef.toCaseFolded()) ||
                mappedSectionKeys.contains(sectionKey.toCaseFolded()))
                return fail(QStringLiteral("PcbLib SectionKeys 映射重复: %1").arg(libRef));
            mappedComponents.insert(libRef.toCaseFolded());
            mappedSectionKeys.insert(sectionKey.toCaseFolded());
            sectionKeys.insert(libRef, sectionKey);
        }
        if (sectionKeyReader.hasError() || sectionKeyReader.remaining() != 0)
            return fail(QStringLiteral("PcbLib SectionKeys 末尾包含无效数据"));
    }

    QSet<QString> resolvedSectionKeys;  // 已解析的 OLE 节名称集合。
    for (const QString& name : names) {
        const QString sectionKey = sectionKeys.value(name, name);  // 当前封装对应的节名称。
        if (resolvedSectionKeys.contains(sectionKey.toCaseFolded()))
            return fail(QStringLiteral("PcbLib 封装存储键重复: %1").arg(sectionKey));
        resolvedSectionKeys.insert(sectionKey.toCaseFolded());
        const QString dataPath = sectionKey + QStringLiteral("/Data");  // 封装 Data 流路径。
        if (!m_oleReader.containsStream(dataPath))
            return fail(QStringLiteral("PcbLib 缺少封装 Data 流: %1").arg(dataPath));
        m_components.append({name, sectionKey});
    }
    return true;
}

/**
 * @brief 返回当前库中按文件顺序解析出的封装信息
 * @return 封装目录项列表
 */
QVector<AltiumPcbLibReader::ComponentInfo> AltiumPcbLibReader::components() const {
    return m_components;
}

/**
 * @brief 返回 PcbLib 文件头声明的版本字符串
 * @return 文件版本字符串
 */
QString AltiumPcbLibReader::fileVersion() const {
    return m_fileVersion;
}

/**
 * @brief 返回 Library/Data 流中的原始库元数据
 * @return 原始元数据块
 */
QByteArray AltiumPcbLibReader::libraryMetadata() const {
    return m_libraryMetadata;
}

/**
 * @brief 按封装名称查找其在库组件列表中的索引
 * @param componentName 封装名称
 * @return 封装索引；未找到时返回 -1
 */
int AltiumPcbLibReader::componentIndex(const QString& componentName) const {
    for (int i = 0; i < m_components.size(); ++i) {
        if (m_components.at(i).name == componentName)
            return i;
    }
    return -1;
}

/**
 * @brief 按封装索引读取其指定名称的 OLE 流
 * @param componentIndexValue 封装目录项索引
 * @param streamName OLE 流名称
 * @param data 用于接收流数据的输出指针
 * @return 读取成功时返回 true
 */
bool AltiumPcbLibReader::readFootprintStream(int componentIndexValue,
                                             const QString& streamName,
                                             QByteArray* data) const {
    if (data == nullptr || componentIndexValue < 0 || componentIndexValue >= m_components.size() ||
        streamName.isEmpty())
        return false;
    return m_oleReader.readStream(m_components.at(componentIndexValue).sectionKey + "/" + streamName, data);
}

/**
 * @brief 按封装名称读取其指定名称的 OLE 流
 * @param componentName 封装名称
 * @param streamName OLE 流名称
 * @param data 用于接收流数据的输出指针
 * @return 读取成功时返回 true
 */
bool AltiumPcbLibReader::readFootprintStream(const QString& componentName,
                                             const QString& streamName,
                                             QByteArray* data) const {
    return readFootprintStream(componentIndex(componentName), streamName, data);
}

/**
 * @brief 解析指定封装 Data 流中的图元块，并保留原始编码供后续导出使用
 * @param componentIndexValue 封装目录项索引
 * @param objects 用于接收图元记录的输出指针
 * @return 解析成功时返回 true
 */
bool AltiumPcbLibReader::readFootprintObjects(int componentIndexValue, QVector<PrimitiveRecord>* objects) const {
    if (objects == nullptr) {
        m_errorMessage = QStringLiteral("读取 PcbLib 图元时输出容器为空");
        return false;
    }
    objects->clear();
    m_errorMessage.clear();

    const auto failRead = [this, objects](const QString& message) {
        objects->clear();
        m_errorMessage = message;
        return false;
    };

    QByteArray data;  // 封装 Data 流数据。
    if (!readFootprintStream(componentIndexValue, QStringLiteral("Data"), &data))
        return failRead(QStringLiteral("无法读取 PcbLib 封装 Data 流"));

    AltiumBinaryReader reader(data);
    QString footprintName;  // Data 流头部声明的封装名称。
    if (!reader.readStringBlock(&footprintName) || footprintName.isEmpty())
        return failRead(QStringLiteral("PcbLib 封装 Data 首部无效: %1").arg(reader.errorString()));

    QSet<quint32> wideStringIndices;  // WideStrings 中已声明的文本索引。
    bool wideStringsLoaded = false;  // 是否已经加载 WideStrings 流。

    while (reader.remaining() > 0) {
        const int objectStart = reader.position();  // 当前图元起始偏移。
        uint8_t objectId = 0;  // 当前图元对象类型编号。
        if (!reader.readUInt8(&objectId)) {
            return failRead(QStringLiteral("读取 PcbLib 图元 ID 失败: %1").arg(reader.errorString()));
        }
        const int blockCount = primitiveBlockCount(objectId);  // 当前图元的子块数量。
        if (blockCount < 0)
            return failRead(QStringLiteral("PcbLib 图元 ID %1 不受支持").arg(objectId));

        PrimitiveRecord object;  // 当前图元解析结果。
        object.objectId = objectId;
        object.blocks.reserve(blockCount);
        for (int blockIndex = 0; blockIndex < blockCount; ++blockIndex) {
            const int blockStart = reader.position();  // 当前子块起始偏移。
            QByteArray payload;  // 子块载荷。
            uint8_t flags = 0;  // 子块标志位。
            if (!reader.readBlock(&payload, &flags))
                return failRead(QStringLiteral("读取 PcbLib 图元 %1 的子块 %2 失败: %3")
                                    .arg(objectId)
                                    .arg(blockIndex)
                                    .arg(reader.errorString()));
            if (payload.isEmpty())
                return failRead(QStringLiteral("PcbLib 图元 %1 的子块 %2 为空").arg(objectId).arg(blockIndex));
            const bool isPadStringBlock =
                objectId == AltiumConstants::PCB_OBJECT_PAD && blockIndex < 3;  // 是否为焊盘字符串块。
            const bool isTextStringBlock =
                objectId == AltiumConstants::PCB_OBJECT_TEXT && blockIndex == 1;  // 是否为文本字符串块。
            if ((isPadStringBlock || isTextStringBlock) && !isStringBlockPayload(payload)) {
                return failRead(QStringLiteral("PcbLib 图元 %1 的字符串子块 %2 无效").arg(objectId).arg(blockIndex));
            }
            const bool isMainBlock =
                (objectId == AltiumConstants::PCB_OBJECT_PAD && blockIndex == 4) ||
                (objectId != AltiumConstants::PCB_OBJECT_PAD && blockIndex == 0);  // 是否为主字段块。
            if (isMainBlock && !readCommonPrimitiveHeader(payload, &object.layer, &object.primitiveFlags))
                return failRead(QStringLiteral("PcbLib 图元 %1 的公共头部不完整").arg(objectId));
            if (isMainBlock && objectId == AltiumConstants::PCB_OBJECT_TRACK) {
                if (!parseTrackFields(payload, &object.track))
                    return failRead(QStringLiteral("PcbLib 走线图元字段不完整"));
                object.hasTrackFields = true;
            } else if (isMainBlock && objectId == AltiumConstants::PCB_OBJECT_ARC) {
                if (!parseArcFields(payload, &object.arc))
                    return failRead(QStringLiteral("PcbLib 弧线图元字段不完整"));
                object.hasArcFields = true;
            } else if (isMainBlock && objectId == AltiumConstants::PCB_OBJECT_PAD) {
                if (!parsePadMainFields(payload, &object.pad))
                    return failRead(QStringLiteral("PcbLib 焊盘图元主块字段不完整"));
                object.hasPadFields = true;
            } else if (isMainBlock && objectId == AltiumConstants::PCB_OBJECT_FILL) {
                if (!parseFillFields(payload, &object.fill))
                    return failRead(QStringLiteral("PcbLib 填充图元字段不完整"));
                object.hasFillFields = true;
            } else if (isMainBlock && objectId == AltiumConstants::PCB_OBJECT_REGION) {
                if (!parseRegionFields(payload, &object.region))
                    return failRead(QStringLiteral("PcbLib 区域图元字段不完整"));
                object.hasRegionFields = true;
            } else if (isMainBlock && objectId == AltiumConstants::PCB_OBJECT_COMPONENT_BODY) {
                if (!parseComponentBodyFields(payload, &object.componentBody))
                    return failRead(QStringLiteral("PcbLib 三维元件体图元字段不完整"));
                object.hasComponentBodyFields = true;
            } else if (isMainBlock && objectId == AltiumConstants::PCB_OBJECT_TEXT) {
                if (!parseTextFields(payload, &object.textFields))
                    return failRead(QStringLiteral("PcbLib 文本图元字段不完整"));
                object.hasTextFields = true;
            }
            PrimitiveBlock block;  // 当前子块的原始记录。
            block.flags = flags;
            block.payload = payload;
            block.encoded = data.mid(blockStart, reader.position() - blockStart);
            object.blocks.append(block);
        }
        if (objectId == AltiumConstants::PCB_OBJECT_PAD) {
            if (!readStringBlockPayload(object.blocks.at(0).payload, &object.designator) ||
                !parsePadExtendedFields(object.blocks.at(5).payload, &object.pad))
                return failRead(QStringLiteral("PcbLib 焊盘图元扩展字段不完整"));
        }
        if (objectId == AltiumConstants::PCB_OBJECT_TEXT &&
            !readStringBlockPayload(object.blocks.at(1).payload, &object.text))
            return failRead(QStringLiteral("PcbLib 文本图元字符串内容无效"));
        QString semanticError;  // 图元语义校验错误说明。
        if (!validatePrimitiveFields(object, &semanticError))
            return failRead(semanticError);
        if (object.hasTextFields) {
            if (!wideStringsLoaded) {
                QByteArray wideStrings;  // WideStrings 流数据。
                if (!readFootprintStream(componentIndexValue, QStringLiteral("WideStrings"), &wideStrings))
                    return failRead(QStringLiteral("无法读取 PcbLib 封装 WideStrings 流"));
                if (!readWideStringIndices(wideStrings, &wideStringIndices))
                    return failRead(QStringLiteral("PcbLib 封装 WideStrings 流无效"));
                wideStringsLoaded = true;
            }
            if (!wideStringIndices.contains(object.textFields.wideStringIndex))
                return failRead(QStringLiteral("PcbLib 文本图元引用了不存在的 WideStrings 索引: %1")
                                    .arg(object.textFields.wideStringIndex));
        }
        object.encoded = data.mid(objectStart, reader.position() - objectStart);
        objects->append(object);
    }
    return true;
}

/**
 * @brief 按封装名称解析其 Data 流中的全部图元
 * @param componentName 封装名称
 * @param objects 用于接收图元记录的输出指针
 * @return 解析成功时返回 true
 */
bool AltiumPcbLibReader::readFootprintObjects(const QString& componentName, QVector<PrimitiveRecord>* objects) const {
    return readFootprintObjects(componentIndex(componentName), objects);
}

/**
 * @brief 返回最近一次 PcbLib 读取错误
 * @return 最近一次错误说明；没有错误时为空
 */
QString AltiumPcbLibReader::errorString() const {
    return m_errorMessage;
}

/**
 * @brief 判断 PcbLib 读取器是否处于错误状态
 * @return 已发生读取错误时返回 true
 */
bool AltiumPcbLibReader::hasError() const {
    return !m_errorMessage.isEmpty();
}

}  // namespace EasyKiConverter
