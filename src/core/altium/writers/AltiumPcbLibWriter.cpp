#include "AltiumPcbLibWriter.h"

#include "AltiumPcbInputValidator.h"
#include "AltiumPcbPrimitiveWriter.h"
#include "utils/AltiumConstants.h"
#include "utils/AltiumCoord.h"
#include "utils/AltiumLayerMap.h"
#include "utils/AltiumWriterUtils.h"

#include <QDateTime>
#include <QDebug>
#include <QFileInfo>
#include <QLocale>
#include <QtEndian>

#include <cmath>

namespace EasyKiConverter {
/**
 * @brief 统计封装中的图元数量
 * @param component 待统计的封装
 */
int AltiumPcbLibWriter::countPrimitives(const AltiumPcbComponent& component) const {
    return component.pads.size() + component.tracks.size() + component.arcs.size() + component.texts.size() +
           component.fills.size() + component.regions.size() + component.bodies.size();
}

/**
 * @brief 添加宽字符串并返回索引
 * @param text 待添加的文本
 */
int AltiumPcbLibWriter::addWideString(const QString& text) {
    const int existing = m_wideStrings.indexOf(text);  // 已存在文本的索引。
    if (existing >= 0) {
        return existing;
    }
    int idx = m_wideStrings.size();  // 新文本在宽字符串表中的索引。
    m_wideStrings.append(text);
    return idx;
}

/**
 * @brief V7 Layer ID 转换
 * @param layer Altium 层编号
 */
uint32_t AltiumPcbLibWriter::toV7LayerId(uint8_t layer) const {
    return AltiumLayerMap::toV7LayerId(layer);
}

/**
 * @brief 将非有限浮点值回退为可写入数值，并记录诊断信息
 * @param value 待检查的浮点值
 * @param fallback 无效值的回退值
 * @param context 诊断中的字段上下文
 * @return 有效值或回退值
 */
double AltiumPcbLibWriter::normalizeFiniteValue(double value, double fallback, const QString& context) {
    if (std::isfinite(value))
        return value;

    const QString diagnostic =
        QStringLiteral("Altium PcbLib %1无效，已规范化为 %2").arg(context).arg(fallback, 0, 'f', 6);
    m_diagnostics.append(diagnostic);
    qWarning() << "AltiumPcbLibWriter:" << diagnostic;
    return fallback;
}

/**
 * @brief 委托独立校验器检查封装输入和输出路径
 * @param components 待校验的封装列表
 * @param filePath 输出文件路径
 * @return 输入有效时返回 true
 */
bool AltiumPcbLibWriter::validateComponents(const QList<AltiumPcbComponent>& components, const QString& filePath) {
    return AltiumPcbInputValidator::validate(*this, components, filePath);
}

/**
 * @brief 写入 PcbLib 文件
 * @param components 封装列表
 * @param filePath 输出文件路径
 * @param libraryName 库名称
 * @return 写入成功时返回 true
 */
bool AltiumPcbLibWriter::write(const QList<AltiumPcbComponent>& components,
                               const QString& filePath,
                               const QString& libraryName) {
    m_diagnostics.clear();
    if (!validateComponents(components, filePath)) {
        return false;
    }
    m_wideStrings.clear();

    qDebug() << "AltiumPcbLibWriter::write: components:" << components.size() << "filePath:" << filePath;

    OLECompoundWriter ole;  // 当前输出文件的 OLE 写入器。
    if (!ole.create()) {
        qWarning() << "AltiumPcbLibWriter::write: Failed to create OLE compound document";
        return false;
    }

    QStringList names;  // 原始封装名称列表。
    for (const AltiumPcbComponent& component : components) {
        names.append(component.name);
    }
    const QStringList sectionKeys = AltiumWriterUtils::makeUniqueSectionKeys(names);  // 唯一 OLE 节名称列表。

    // 只输出 PcbLib 核心流。未知或不完整的可选 storage 比缺省更危险，
    // 因为 Altium 会尝试按其声明的版本反序列化它们。
    writeFileHeader(ole);
    writeSectionKeys(ole, components, sectionKeys);
    writeLibraryStorage(ole, components, filePath);

    // 写入每个封装
    for (int i = 0; i < components.size(); ++i) {
        writeFootprintStorage(ole, components[i], sectionKeys[i]);
    }

    if (ole.hasError()) {
        qWarning() << "AltiumPcbLibWriter: Failed to construct OLE document:" << ole.errorString();
        return false;
    }

    bool result = ole.saveToFile(filePath);  // OLE 文件保存结果。
    qDebug() << "AltiumPcbLibWriter::write: saveToFile result:" << result << "filePath:" << filePath;
    return result;
}

/**
 * @brief 写入 PcbLib v6 FileHeader 流
 * @param ole 目标 OLE 写入器
 */
void AltiumPcbLibWriter::writeFileHeader(OLECompoundWriter& ole) {
    QByteArray header;  // FileHeader 流数据。
    AltiumBinaryWriter writer(header);

    // 此流的第一个 i32 是版本文本本身的长度，随后是 Pascal short string。
    // 它不是通用 string block，后面也没有版本 double 或随机 ID。
    QByteArray versionStr = "PCB 6.0 Binary Library File";  // PcbLib 版本文本。
    writer.writeInt32(versionStr.size());
    writer.writePascalShortString(QString::fromLatin1(versionStr));

    ole.writeStream("FileHeader", header);
}

/**
 * @brief 写入 SectionKeys 流
 * @param ole 目标 OLE 写入器
 * @param components 封装列表
 * @param sectionKeys 唯一 OLE 节名称列表
 */
void AltiumPcbLibWriter::writeSectionKeys(OLECompoundWriter& ole,
                                          const QList<AltiumPcbComponent>& components,
                                          const QStringList& sectionKeys) {
    QByteArray data;  // SectionKeys 流数据。
    AltiumBinaryWriter writer(data);

    // 计算需要映射的条目数
    QVector<QPair<QString, QString>> mappings;  // 需要显式写入的名称映射。
    for (int i = 0; i < components.size(); ++i) {
        const AltiumPcbComponent& comp = components[i];  // 当前封装。
        const QString& key = sectionKeys[i];  // 当前封装的唯一节名称。
        if (key != comp.name) {
            mappings.append({comp.name, key});
        }
    }

    writer.writeUInt32(static_cast<uint32_t>(mappings.size()));
    for (const auto& mapping : mappings) {
        writer.writePascalString(mapping.first);
        writer.writeStringBlock(mapping.second);
    }

    if (!mappings.isEmpty()) {
        ole.writeStream("SectionKeys", data);
    }
}

/**
 * @brief 写入 Library 存储
 * @param ole 目标 OLE 写入器
 * @param components 封装列表
 * @param filePath 输出文件路径
 */
void AltiumPcbLibWriter::writeLibraryStorage(OLECompoundWriter& ole,
                                             const QList<AltiumPcbComponent>& components,
                                             const QString& filePath) {
    ole.addStorage("Library");

    // Header 流
    QByteArray headerData;  // Library/Header 流数据。
    AltiumBinaryWriter headerWriter(headerData);  // Library/Header 写入器。
    headerWriter.writeUInt32(1);
    ole.writeStream("Library", "Header", headerData);

    // Data 流
    QByteArray libData;  // Library/Data 流数据。
    writeLibraryData(libData, components, filePath);
    ole.writeStream("Library", "Data", libData);

    // Models 存储
    writeModelsStorage(ole, components);

    // Textures 存储（空）
    {
        ole.addStorage("Library", "Textures");
        QByteArray hdrData;  // Textures/Header 流数据。
        AltiumBinaryWriter hdr(hdrData);  // Textures/Header 写入器。
        hdr.writeUInt32(0);
        ole.writeStream("Library/Textures", "Header", hdrData);
        ole.writeStream("Library/Textures", "Data", QByteArray());
    }

    // ModelsNoEmbed 存储（空）
    {
        ole.addStorage("Library", "ModelsNoEmbed");
        QByteArray hdrData;  // ModelsNoEmbed/Header 流数据。
        AltiumBinaryWriter hdr(hdrData);  // ModelsNoEmbed/Header 写入器。
        hdr.writeUInt32(0);
        ole.writeStream("Library/ModelsNoEmbed", "Header", hdrData);
        ole.writeStream("Library/ModelsNoEmbed", "Data", QByteArray());
    }
}

/**
 * @brief 写入 Library/Data 流
 * @param buffer 目标数据缓冲区
 * @param components 封装列表
 * @param filePath 输出文件路径
 */
void AltiumPcbLibWriter::writeLibraryData(QByteArray& buffer,
                                          const QList<AltiumPcbComponent>& components,
                                          const QString& filePath) {
    AltiumBinaryWriter writer(buffer);

    writer.beginBlock();
    QByteArray metadata = buildLibraryMetadata(filePath).toLatin1();  // 库元数据编码结果。
    metadata.append('\0');
    writer.writeBytes(metadata);
    writer.endBlock();

    // 元件数量
    writer.writeUInt32(static_cast<uint32_t>(components.size()));

    // 每个元件名称
    for (const AltiumPcbComponent& comp : components) {
        writer.writeStringBlock(comp.name);
    }
}

/**
 * @brief 构建 Library/Data 流使用的参数元数据，并清理参数分隔符
 * @param filePath 输出文件路径
 * @return 序列化后的库元数据
 */
QString AltiumPcbLibWriter::buildLibraryMetadata(const QString& filePath) const {
    auto safeValue = [](QString value) {  // 清理元数据中的分隔符。
        value.replace('|', ' ');
        value.replace('\0', ' ');
        return value;
    };
    auto append = [&safeValue](QStringList& fields, const QString& key, const QString& value) {  // 追加一个键值字段。
        fields.append(key + "=" + safeValue(value));
    };

    QString absolutePath = QFileInfo(filePath).absoluteFilePath();  // 规范化后的绝对文件路径。
    absolutePath.replace('/', '\\');
    const QDateTime now = QDateTime::currentDateTime();  // 生成元数据时刻。

    // Altium serializes library settings as a sequence of Board records. We build
    // that profile from typed defaults so it remains reviewable and evolvable
    // instead of embedding an opaque captured blob.
    QList<QStringList> records;  // Board 记录列表。
    records.append(QStringList{});
    QStringList& identity = records.last();  // 库身份记录。
    append(identity, "FILENAME", absolutePath);
    append(identity, "KIND", "Protel_Advanced_PCB_Library");
    append(identity, "VERSION", "3.00");
    append(identity, "DATE", QLocale::c().toString(now.date(), "M/d/yyyy"));
    append(identity, "TIME", QLocale::c().toString(now.time(), "h:mm:ss AP"));
    append(identity, "TOPTYPE", "3");
    append(identity, "TOPCONST", "3.500");
    append(identity, "TOPHEIGHT", "0.4mil");
    append(identity, "TOPMATERIAL", "Solder Resist");
    append(identity, "BOTTOMTYPE", "3");
    append(identity, "BOTTOMCONST", "3.500");
    append(identity, "BOTTOMHEIGHT", "0.4mil");
    append(identity, "BOTTOMMATERIAL", "Solder Resist");
    append(identity, "LAYERSTACKSTYLE", "0");

    auto layerName = [](int layer) -> QString {  // 将层编号转换为 Altium 层名称。
        if (layer == 1)
            return "Top Layer";
        if (layer >= 2 && layer <= 31)
            return QString("Mid-Layer %1").arg(layer - 1);
        if (layer == 32)
            return "Bottom Layer";
        static const QStringList standardNames = {// 标准覆盖层名称。
                                                  "Top Overlay",
                                                  "Bottom Overlay",
                                                  "Top Paste",
                                                  "Bottom Paste",
                                                  "Top Solder",
                                                  "Bottom Solder"};
        if (layer >= 33 && layer <= 38)
            return standardNames[layer - 33];
        if (layer >= 39 && layer <= 54)
            return QString("Internal Plane %1").arg(layer - 38);
        if (layer == 55)
            return "Drill Guide";
        if (layer == 56)
            return "Keep-Out Layer";
        if (layer >= 57 && layer <= 72)
            return QString("Mechanical %1").arg(layer - 56);
        static const QStringList auxiliaryNames = {// 辅助层名称。
                                                   "Drill Drawing",
                                                   "Multi-Layer",
                                                   "Connections",
                                                   "Background",
                                                   "DRC Error Markers",
                                                   "Selections",
                                                   "Visible Grid 1",
                                                   "Visible Grid 2",
                                                   "Pad Holes",
                                                   "Via Holes"};
        return auxiliaryNames.value(layer - 73, QString("Layer %1").arg(layer));
    };

    for (int layer = 1; layer <= 82; ++layer) {
        if ((layer - 1) % 5 == 0 && layer != 1) {
            records.append(QStringList{});
        }
        QStringList& fields = records.last();  // 当前层的字段列表。
        const QString prefix = QString("LAYER%1").arg(layer);  // 当前层字段前缀。
        append(fields, prefix + "NAME", layerName(layer));
        append(fields, prefix + "PREV", layer == 1 ? "0" : (layer == 32 ? "1" : "0"));
        append(fields, prefix + "NEXT", layer == 1 ? "32" : "0");
        append(fields, prefix + "MECHENABLED", layer == 57 ? "TRUE" : "FALSE");
        append(fields, prefix + "COPTHICK", "1.4mil");
        append(fields, prefix + "DIELTYPE", "0");
        append(fields, prefix + "DIELCONST", "4.800");
        append(fields, prefix + "DIELHEIGHT", "12.6mil");
        append(fields, prefix + "DIELMATERIAL", "FR-4");
    }

    records.append(QStringList{});
    QStringList& grid = records.last();  // 网格设置记录。
    append(grid, "BIGVISIBLEGRIDSIZE", "0.000000");
    append(grid, "VISIBLEGRIDSIZE", "0.000000");
    append(grid, "SNAPGRIDSIZE", "50000.000000");
    append(grid, "SNAPGRIDSIZEX", "50000.000000");
    append(grid, "SNAPGRIDSIZEY", "50000.000000");
    append(grid, "ELECTRICALGRIDRANGE", "8mil");
    append(grid, "ELECTRICALGRIDENABLED", "TRUE");
    append(grid, "DISPLAYUNIT", "1");
    append(grid, "CURRENT2D3DVIEWSTATE", "2D");

    QStringList serializedRecords;  // 已序列化的 Board 记录。
    for (const QStringList& record : records) {
        if (!record.isEmpty()) {
            serializedRecords.append(record.join('|'));
        }
    }
    return serializedRecords.join("\r|RECORD=Board|");
}

/**
 * @brief 写入 Models 存储
 * @param ole 目标 OLE 写入器
 * @param components 封装列表
 */
void AltiumPcbLibWriter::writeModelsStorage(OLECompoundWriter& ole, const QList<AltiumPcbComponent>& components) {
    ole.addStorage("Library", "Models");

    // 收集所有 3D 模型
    QList<AltiumPcbComponent::Model3D> allModels;  // 所有封装去重前的模型列表。
    for (const AltiumPcbComponent& comp : components) {
        for (const AltiumPcbComponent::Model3D& model : comp.models) {
            allModels.append(model);
        }
    }

    // Header 流
    QByteArray hdrData;  // Models/Header 流数据。
    AltiumBinaryWriter hdr(hdrData);  // Models/Header 写入器。
    hdr.writeUInt32(static_cast<uint32_t>(allModels.size()));
    ole.writeStream("Library/Models", "Header", hdrData);

    // Data 流（模型元数据）
    QByteArray modelData;  // Models/Data 流数据。
    AltiumBinaryWriter modelWriter(modelData);  // 模型元数据写入器。
    const auto safeMetadataValue = [](QString value) {  // 清理模型元数据分隔符。
        value.replace('|', ' ');
        value.replace('\0', ' ');
        value.replace('\r', ' ');
        value.replace('\n', ' ');
        return value;
    };
    for (const AltiumPcbComponent::Model3D& model : allModels) {
        const QString id = safeMetadataValue(model.id.isEmpty() ? model.name : model.id);  // 模型标识。
        const QString metadata =
            QString("EMBED=TRUE|MODELSOURCE=Undefined|ID=%1|ROTX=%2|ROTY=%3|ROTZ=%4|DZ=%5|CHECKSUM=0|NAME=%6")
                .arg(id,
                     QString::number(normalizeFiniteValue(model.rotX, 0.0, QStringLiteral("3D 模型 X 旋转")), 'f', 6),
                     QString::number(normalizeFiniteValue(model.rotY, 0.0, QStringLiteral("3D 模型 Y 旋转")), 'f', 6),
                     QString::number(normalizeFiniteValue(model.rotZ, 0.0, QStringLiteral("3D 模型 Z 旋转")), 'f', 6),
                     QString::number(
                         AltiumCoord::mmToRaw(normalizeFiniteValue(model.dz, 0.0, QStringLiteral("3D 模型 Z 偏移")))),
                     safeMetadataValue(model.name));
        QByteArray encoded = metadata.toLatin1();  // 模型元数据编码结果。
        encoded.append('\0');
        modelWriter.writeInt32(encoded.size());
        modelWriter.writeBytes(encoded);
    }
    ole.writeStream("Library/Models", "Data", modelData);

    // 写入压缩的 STEP 数据（去掉 qCompress 的 4 字节头，Altium 期望原始 zlib 流）
    for (int i = 0; i < allModels.size(); ++i) {
        if (!allModels[i].stepData.isEmpty()) {
            QByteArray compressed = qCompress(allModels[i].stepData, 9);  // STEP 压缩数据。
            ole.writeStream("Library/Models", QString::number(i), compressed.mid(4));
        }
    }
}

/**
 * @brief 写入封装存储
 * @param ole 目标 OLE 写入器
 * @param component 封装数据
 * @param sectionKey 封装存储节名称
 */
void AltiumPcbLibWriter::writeFootprintStorage(OLECompoundWriter& ole,
                                               const AltiumPcbComponent& component,
                                               const QString& sectionKey) {
    ole.addStorage(sectionKey);
    QString basePath = sectionKey;  // 封装存储根路径。

    // Header 流（图元数量）
    QByteArray hdrData;  // 封装 Header 流数据。
    AltiumBinaryWriter hdr(hdrData);  // 封装 Header 写入器。
    hdr.writeUInt32(static_cast<uint32_t>(countPrimitives(component)));
    ole.writeStream(basePath, "Header", hdrData);

    // Parameters 流
    QByteArray paramsData;  // Parameters 流数据。
    writeFootprintParameters(paramsData, component);
    ole.writeStream(basePath, "Parameters", paramsData);

    // WideStrings 流
    QByteArray wsData;  // WideStrings 流数据。
    writeWideStrings(wsData, component);
    ole.writeStream(basePath, "WideStrings", wsData);

    // Data 流
    QByteArray dataData;  // Data 流数据。
    writeFootprintData(dataData, component);
    ole.writeStream(basePath, "Data", dataData);

    // UniqueIdPrimitiveInformation 流
    ole.addStorage(basePath, "UniqueIdPrimitiveInformation");
    QByteArray uidHdrData;  // 唯一标识 Header 流数据。
    AltiumBinaryWriter uidHdr(uidHdrData);  // 唯一标识 Header 写入器。
    uidHdr.writeUInt32(static_cast<uint32_t>(countPrimitives(component)));
    ole.writeStream(basePath + "/UniqueIdPrimitiveInformation", "Header", uidHdrData);

    QByteArray uidData;  // 唯一标识 Data 流数据。
    writeUniqueIdPrimitiveInformation(uidData, component);
    ole.writeStream(basePath + "/UniqueIdPrimitiveInformation", "Data", uidData);

    // ExtendedPrimitiveInformation 流（仅当有扩展信息时写入）
    if (!component.extendedPrimitives.isEmpty()) {
        ole.addStorage(basePath, "ExtendedPrimitiveInformation");
        QByteArray extHdrData;  // 扩展信息 Header 流数据。
        AltiumBinaryWriter extHdr(extHdrData);  // 扩展信息 Header 写入器。
        extHdr.writeUInt32(static_cast<uint32_t>(component.extendedPrimitives.size()));
        ole.writeStream(basePath + "/ExtendedPrimitiveInformation", "Header", extHdrData);

        QByteArray extData;  // 扩展信息 Data 流数据。
        writeExtendedPrimitiveInformation(extData, component);
        ole.writeStream(basePath + "/ExtendedPrimitiveInformation", "Data", extData);
    }
}

/**
 * @brief 写入封装参数
 * @param buffer 目标数据缓冲区
 * @param component 封装数据
 */
void AltiumPcbLibWriter::writeFootprintParameters(QByteArray& buffer, const AltiumPcbComponent& component) {
    AltiumBinaryWriter writer(buffer);

    QMap<QString, QString> params;  // 封装参数键值表。
    params["PATTERN"] = component.name;
    params["HEIGHT"] = QString::number(AltiumCoord::mmToRaw(component.height));
    if (!component.description.isEmpty()) {
        params["DESCRIPTION"] = component.description;
    }
    params["ITEMGUID"] = "";
    params["REVISIONGUID"] = "";

    writer.writeCStringParameterBlock(params);
}

/**
 * @brief 写入封装数据（二进制图元）
 * @param buffer 目标数据缓冲区
 * @param component 封装数据
 */
void AltiumPcbLibWriter::writeFootprintData(QByteArray& buffer, const AltiumPcbComponent& component) {
    AltiumBinaryWriter writer(buffer);
    AltiumPcbPrimitiveWriter primitiveWriter(*this);

    // 写入封装名称
    writer.writeStringBlock(component.name);

    // 写入所有图元
    for (const AltiumPcbPad& pad : component.pads) {
        primitiveWriter.writePad(writer, pad);
    }
    for (const AltiumPcbTrack& track : component.tracks) {
        primitiveWriter.writeTrack(writer, track, 0);
    }
    for (const AltiumPcbArc& arc : component.arcs) {
        primitiveWriter.writeArc(writer, arc);
    }
    for (const AltiumPcbText& text : component.texts) {
        primitiveWriter.writeText(writer, text);
    }
    for (const AltiumPcbFill& fill : component.fills) {
        primitiveWriter.writeFill(writer, fill);
    }
    for (const AltiumPcbRegion& region : component.regions) {
        primitiveWriter.writeRegion(writer, region);
    }
    for (const AltiumPcbComponentBody& body : component.bodies) {
        primitiveWriter.writeComponentBody(writer, body);
    }
}

/**
 * @brief 写入宽字符串流
 * @param buffer 目标数据缓冲区
 * @param component 封装数据
 */
void AltiumPcbLibWriter::writeWideStrings(QByteArray& buffer, const AltiumPcbComponent& component) {
    AltiumBinaryWriter writer(buffer);

    // 收集所有需要广字符串的文本
    m_wideStrings.clear();
    for (const AltiumPcbText& text : component.texts) {
        addWideString(text.text);
    }

    QMap<QString, QString> params;  // 宽字符串参数键值表。
    for (int i = 0; i < m_wideStrings.size(); ++i) {
        QString encoded;  // 当前文本的 Unicode 编码串。
        const QString& text = m_wideStrings[i];  // 当前宽字符串。
        for (int j = 0; j < text.size(); ++j) {
            if (j > 0)
                encoded += ",";
            encoded += QString::number(static_cast<int>(text.at(j).unicode()));
        }
        params[QString("ENCODEDTEXT%1").arg(i)] = encoded;
    }

    writer.writeCStringParameterBlock(params);
}

/**
 * @brief 写入通用图元头部（13 字节）
 * @param writer 二进制写入器
 * @param layer 图元层编号
 * @param flags 图元标志位
 */
void AltiumPcbLibWriter::writeCommonPrimitiveHeader(AltiumBinaryWriter& writer, uint8_t layer, uint16_t flags) {
    writer.writeUInt8(layer);
    writer.writeUInt16(flags);
    // PcbLib 图元不属于已放置的 board component/net/polygon，五个索引均为 -1。
    writer.writeBytes(QByteArray(10, static_cast<char>(0xFF)));
}

/**
 * @brief 编码图元标志位
 * @param isLocked 是否锁定图元
 * @param isTentingTop 是否覆盖顶层阻焊
 * @param isTentingBottom 是否覆盖底层阻焊
 * @param isKeepout 是否为禁止区图元
 * @details 根据图元属性生成 Altium 标志字：
 *   Bit 2 = Unlocked, Bit 3 = Saved, Bit 5 = TentingTop, Bit 6 = TentingBottom, Bit 9 = Keepout
 */
uint16_t AltiumPcbLibWriter::encodePrimitiveFlags(bool isLocked,
                                                  bool isTentingTop,
                                                  bool isTentingBottom,
                                                  bool isKeepout) {
    uint16_t flags = AltiumConstants::PCB_FLAG_SAVED;  // 已编码的图元标志位。
    if (!isLocked) {
        flags |= 0x04;  // Unlocked
    }
    if (isTentingTop) {
        flags |= AltiumConstants::PCB_FLAG_TENTING_TOP;
    }
    if (isTentingBottom) {
        flags |= AltiumConstants::PCB_FLAG_TENTING_BOTTOM;
    }
    if (isKeepout) {
        flags |= AltiumConstants::PCB_FLAG_KEEPOUT;
    }
    return flags;
}

/**
 * @brief 写入元件图元的唯一标识索引表
 * @param buffer 目标数据缓冲区
 * @param component 封装数据
 */
void AltiumPcbLibWriter::writeUniqueIdPrimitiveInformation(QByteArray& buffer, const AltiumPcbComponent& component) {
    AltiumBinaryWriter writer(buffer);

    // 按图元顺序写入唯一标识信息，确保后续记录可以通过索引稳定关联。
    auto writeEntry = [&](const char* objectName, int index) {
        QMap<QString, QString> params;  // 当前图元唯一标识参数。
        if (index > 0) {
            params["PRIMITIVEINDEX"] = QString::number(index);
        }
        params["PRIMITIVEOBJECTID"] = QString::fromLatin1(objectName);
        writer.writeCStringParameterBlock(params);
    };

    int idx = 0;  // 当前图元序号。
    for (int i = 0; i < component.pads.size(); ++i, ++idx)
        writeEntry("Pad", idx);
    for (int i = 0; i < component.tracks.size(); ++i, ++idx)
        writeEntry("Track", idx);
    for (int i = 0; i < component.arcs.size(); ++i, ++idx)
        writeEntry("Arc", idx);
    for (int i = 0; i < component.texts.size(); ++i, ++idx)
        writeEntry("Text", idx);
    for (int i = 0; i < component.fills.size(); ++i, ++idx)
        writeEntry("Fill", idx);
    for (int i = 0; i < component.regions.size(); ++i, ++idx)
        writeEntry("Region", idx);
    for (int i = 0; i < component.bodies.size(); ++i, ++idx)
        writeEntry("ComponentBody", idx);
}

/**
 * @brief 写入图元扩展信息
 * @details 为需要额外属性的图元（如自定义焊盘遮罩扩展）写入参数块。
 * @param buffer 目标数据缓冲区
 * @param component 封装数据
 */
void AltiumPcbLibWriter::writeExtendedPrimitiveInformation(QByteArray& buffer, const AltiumPcbComponent& component) {
    AltiumBinaryWriter writer(buffer);

    for (const AltiumPcbExtendedPrimitiveInfo& info : component.extendedPrimitives) {
        QMap<QString, QString> params;  // 当前扩展图元参数。
        params["PRIMITIVEINDEX"] = QString::number(info.primitiveIndex);
        params["PRIMITIVEOBJECTID"] = info.objectName;
        for (auto it = info.params.constBegin(); it != info.params.constEnd(); ++it) {
            params[it.key()] = it.value();
        }
        writer.writeCStringParameterBlock(params);
    }
}

}  // namespace EasyKiConverter
