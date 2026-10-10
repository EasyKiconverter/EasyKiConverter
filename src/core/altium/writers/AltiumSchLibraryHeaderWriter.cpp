#include "AltiumSchLibraryHeaderWriter.h"

#include "AltiumSchLibWriter.h"
#include "utils/AltiumBinaryWriter.h"

#include <QByteArray>
#include <QMap>
#include <QRandomGenerator>

namespace EasyKiConverter {

/**
 * @brief 创建 SchLib 文件头写入器。
 * @param owner 所属的 SchLib 主写入器。
 */
AltiumSchLibraryHeaderWriter::AltiumSchLibraryHeaderWriter(AltiumSchLibWriter& owner) : m_owner(owner) {}

/**
 * @brief 写入 FileHeader 流。
 * @details 头部记录中的组件数量、字体表和记录权重必须与后续 Data 流保持一致。
 * @param ole SchLib 根 OLE 写入器。
 * @param components 待写入文件头的组件列表。
 */
void AltiumSchLibraryHeaderWriter::writeFileHeader(OLECompoundWriter& ole,
                                                   const QList<AltiumSchComponent>& components) {
    QMap<QString, QString> params;  // FileHeader 参数字段。
    params["HEADER"] = "Protel for Windows - Schematic Library Editor Binary File Version 5.0";
    int totalWeight = 0;  // 所有组件 Data 流的记录权重总和。
    for (const AltiumSchComponent& component : components) {
        totalWeight += m_owner.componentRecordCount(component);
    }
    params["WEIGHT"] = QString::number(totalWeight);
    params["MINORVERSION"] = "2";

    QString uid;  // 本次库文件的随机唯一标识。
    for (int i = 0; i < 8; ++i) {
        uid += QChar('A' + QRandomGenerator::global()->bounded(26));
    }
    params["UniqueID"] = uid;

    const QList<AltiumModels::FontEntry>& fonts = m_owner.m_fontRegistry.entries();  // 已注册字体表。
    params["FontIdCount"] = QString::number(fonts.size());
    for (int i = 0; i < fonts.size(); ++i) {
        const int index = i + 1;  // Altium 字体表使用的 1-based 索引。
        params[QString("FontName%1").arg(index)] = fonts[i].name;
        params[QString("Size%1").arg(index)] = QString::number(fonts[i].size);
        if (fonts[i].bold)
            params[QString("Bold%1").arg(index)] = "T";
        if (fonts[i].italic)
            params[QString("Italic%1").arg(index)] = "T";
        if (fonts[i].underline)
            params[QString("Underline%1").arg(index)] = "T";
    }

    params["UseMBCS"] = "T";
    params["IsBOC"] = "T";
    params["SheetStyle"] = "9";
    params["BorderOn"] = "T";
    params["Display_Unit"] = "0";
    params["SYSTEMFONT"] = "1";
    params["SHEETNUMBERSPACESIZE"] = "12";
    params["AREACOLOR"] = "16317695";
    params["SNAPGRIDON"] = "T";
    params["SNAPGRIDSIZE"] = "10";
    params["VISIBLEGRIDON"] = "T";
    params["VISIBLEGRIDSIZE"] = "10";
    params["COMPCOUNT"] = QString::number(components.size());
    for (int i = 0; i < components.size(); ++i) {
        const AltiumSchComponent& component = components[i];  // 当前文件头组件条目。
        params[QString("LIBREF%1").arg(i)] = component.name;
        params[QString("COMPDESCR%1").arg(i)] = component.description;
        params[QString("PARTCOUNT%1").arg(i)] = QString::number(qMax(1, component.partCount) + 1);
    }

    QByteArray headerData;  // FileHeader 流缓冲区。
    AltiumBinaryWriter writer(headerData);  // FileHeader 二进制写入器。
    writer.writeCStringParameterBlockUtf8(params);
    writer.writeInt32(static_cast<int32_t>(components.size()));
    for (const AltiumSchComponent& component : components) {
        writer.writeStringBlock(component.name);
    }
    ole.writeStream("FileHeader", headerData);
}

/**
 * @brief 写入 SectionKeys 流，仅记录需要名称到存储键映射的组件。
 * @param ole SchLib 根 OLE 写入器。
 * @param components 待写入的组件列表。
 * @param sectionKeys 组件对应的 OLE 存储键列表。
 */
void AltiumSchLibraryHeaderWriter::writeSectionKeys(OLECompoundWriter& ole,
                                                    const QList<AltiumSchComponent>& components,
                                                    const QStringList& sectionKeys) {
    QMap<QString, QString> params;  // SectionKeys 参数字段。
    int keyCount = 0;  // 需要记录的名称与存储键映射数量。
    for (int i = 0; i < components.size(); ++i) {
        const QString& sectionKey = sectionKeys[i];  // 当前组件的 OLE 存储键。
        if (sectionKey != components[i].name) {
            params[QString("LibRef%1").arg(keyCount)] = components[i].name;
            params[QString("SectionKey%1").arg(keyCount)] = sectionKey;
            ++keyCount;
        }
    }

    if (keyCount <= 0) {
        return;
    }
    params["KeyCount"] = QString::number(keyCount);
    QByteArray data;  // SectionKeys 流缓冲区。
    AltiumBinaryWriter writer(data);  // SectionKeys 二进制写入器。
    writer.writeCStringParameterBlockUtf8(params);
    ole.writeStream("SectionKeys", data);
}

}  // namespace EasyKiConverter
