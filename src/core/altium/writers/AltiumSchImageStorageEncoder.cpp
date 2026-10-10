#include "AltiumSchImageStorageEncoder.h"

#include "utils/AltiumBinaryWriter.h"

#include <QDataStream>
#include <QIODevice>

namespace EasyKiConverter {

/**
 * @brief 压缩图片并组装 Altium Storage 流中的条目和头部。
 * @param components 待扫描嵌入图片的组件列表。
 * @param imageNames 图片对象到 Storage 文件名的映射。
 * @param diagnostics 用于追加无效图片诊断的列表。
 * @return 序列化后的 Storage 流数据。
 */
QByteArray AltiumSchImageStorageEncoder::encode(const QList<AltiumSchComponent>& components,
                                                const QHash<const AltiumSchImage*, QString>& imageNames,
                                                QStringList& diagnostics) {
    QList<const AltiumSchImage*> embeddedImages;  // 已确认需要嵌入 Storage 的图片对象。
    for (const AltiumSchComponent& component : components) {
        for (const AltiumSchImage& image : component.images) {
            if (image.embedImage && !image.data.isEmpty() && imageNames.contains(&image))
                embeddedImages.append(&image);
        }
    }

    constexpr qsizetype kMaxStorageEntrySize = 0x00FFFFFF;  // Altium Storage 单条记录的最大字节数。
    QList<QByteArray> encodedEntries;  // 已压缩并通过大小检查的 Storage 条目。
    encodedEntries.reserve(embeddedImages.size());
    for (const AltiumSchImage* image : embeddedImages) {
        const QByteArray compressed = qCompress(image->data, 9).mid(4);  // 去除 Qt 压缩头后的图片数据。
        const QByteArray name = imageNames.value(image).toLocal8Bit();  // Storage 条目文件名。
        if (compressed.isEmpty() || name.isEmpty() || name.size() > 255) {
            diagnostics.append(
                QStringLiteral("Altium SchLib 图片 Storage 条目无效，已跳过: %1").arg(QString::fromLocal8Bit(name)));
            continue;
        }

        QByteArray entry;  // 当前图片的 Storage 条目缓冲区。
        QDataStream entryStream(&entry, QIODevice::WriteOnly);  // 条目小端序写入器。
        entryStream.setByteOrder(QDataStream::LittleEndian);
        entryStream << static_cast<quint8>(0xD0) << static_cast<quint8>(name.size());
        entry.append(name);
        entryStream.device()->seek(entry.size());
        entryStream << static_cast<quint32>(compressed.size());
        entry.append(compressed);
        if (entry.size() > kMaxStorageEntrySize) {
            diagnostics.append(
                QStringLiteral("Altium SchLib 图片 Storage 条目过大，已跳过: %1").arg(QString::fromLocal8Bit(name)));
            continue;
        }
        encodedEntries.append(entry);
    }

    QByteArray storageData;  // 最终 Storage 流缓冲区。
    AltiumBinaryWriter storageWriter(storageData);  // Storage 头部参数写入器。
    QMap<QString, QString> storageParams;  // Storage 头部参数。
    storageParams["HEADER"] = "Icon storage";
    storageParams["Weight"] = QString::number(encodedEntries.size());
    storageWriter.writeCStringParameterBlock(storageParams);

    for (const QByteArray& entry : encodedEntries) {  // 追加每个图片 Storage 条目。
        QDataStream blockStream(&storageData, QIODevice::WriteOnly | QIODevice::Append);  // 条目块写入器。
        blockStream.setByteOrder(QDataStream::LittleEndian);
        blockStream << static_cast<quint32>(0x01000000U | static_cast<quint32>(entry.size()));
        blockStream.writeRawData(entry.constData(), entry.size());
    }
    return storageData;
}

}  // namespace EasyKiConverter
