#include "AltiumSchImageRecordWriter.h"

#include "AltiumSchLibWriter.h"

namespace EasyKiConverter {

/**
 * @brief 创建 SchLib 图片记录写入器。
 * @param owner 所属的 SchLib 主写入器。
 */
AltiumSchImageRecordWriter::AltiumSchImageRecordWriter(AltiumSchLibWriter& owner) : m_owner(owner) {}

/**
 * @brief 写入图片几何、显示属性和嵌入图片引用
 * @param writer 目标二进制写入器。
 * @param image 待写入的图片数据。
 */
void AltiumSchImageRecordWriter::write(AltiumBinaryWriter& writer, const AltiumSchImage& image) {
    QMap<QString, QString> params;  // RECORD=30 图片字段。
    params["RECORD"] = "30";
    m_owner.addOwnerParams(params, image.ownerPartId);
    m_owner.addCoordParam(params, "Location.X", image.locationX);
    m_owner.addCoordParam(params, "Location.Y", image.locationY);
    m_owner.addCoordParam(params, "Corner.X", image.cornerX);
    m_owner.addCoordParam(params, "Corner.Y", image.cornerY);
    if (image.rotation != 0.0)
        params["Rotation"] = QString::number(image.rotation, 'f', 3);
    if (image.lineWidth != 0)
        params["LineWidth"] = QString::number(image.lineWidth);
    if (image.lineStyle != 0)
        params["LineStyle"] = QString::number(image.lineStyle);
    m_owner.addColorParam(params, "Color", image.color);
    if (image.areaColor != 0)
        params["AreaColor"] = QString::number(image.areaColor);
    if (image.isSolid)
        params["IsSolid"] = "T";
    if (image.transparent)
        params["Transparent"] = "T";
    if (image.showBorder)
        params["ShowBorder"] = "T";
    if (image.keepAspect)
        params["KeepAspect"] = "T";
    const bool isEmbeddedImage = image.embedImage;  // 是否请求将图片嵌入 SchLib Storage。
    const QString storageFileName =
        isEmbeddedImage ? m_owner.m_embeddedImageNames.value(&image) : image.fileName;  // 最终引用文件名。
    // 只有数据、文件名和长度均符合约束时才声明图片已嵌入。
    const bool hasEmbeddedImage = isEmbeddedImage && !image.data.isEmpty() && !storageFileName.isEmpty() &&
                                  storageFileName.toLocal8Bit().size() <= 255;  // 是否具备有效的嵌入图片引用。
    const bool canUseExternalFallback =
        isEmbeddedImage && image.data.isEmpty() && !image.fileName.trimmed().isEmpty();  // 是否可回退为外部文件。
    if (hasEmbeddedImage)
        params["EmbedImage"] = "T";
    if (!isEmbeddedImage && !storageFileName.isEmpty())
        params["FileName"] = storageFileName;
    else if (hasEmbeddedImage)
        params["FileName"] = storageFileName;
    else if (canUseExternalFallback)
        params["FileName"] = image.fileName;
    m_owner.addUniqueID(params);
    writer.writeCStringParameterBlockUtf8(params);
}

}  // namespace EasyKiConverter
