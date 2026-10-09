#include "CacheComponentDataReader.h"

#include "core/utils/UrlUtils.h"

#include <QJsonArray>

namespace EasyKiConverter {

/**
 * @brief 将缓存元数据中的基础字段复制到组件数据对象。
 * @param metadata 已解析的缓存元数据。
 * @param componentData 待填充的组件数据对象。
 */
static void readBasicFields(const QJsonObject& metadata, ComponentData& componentData) {
    componentData.setName(metadata.value(QStringLiteral("name")).toString());
    componentData.setPrefix(metadata.value(QStringLiteral("prefix")).toString());
    componentData.setPackage(metadata.value(QStringLiteral("package")).toString());
    componentData.setManufacturer(metadata.value(QStringLiteral("manufacturer")).toString());
    componentData.setManufacturerPart(metadata.value(QStringLiteral("manufacturerPart")).toString());
    componentData.setDatasheet(metadata.value(QStringLiteral("datasheet")).toString());
    componentData.setDatasheetFormat(metadata.value(QStringLiteral("datasheetFormat")).toString());
}

/**
 * @brief 只保留通过 URL 规范化的预览图链接，避免缓存中的错误地址继续传播。
 * @param metadata 已解析的缓存元数据。
 * @param componentData 待填充预览图地址的组件数据对象。
 */
static void readPreviewUrls(const QJsonObject& metadata, ComponentData& componentData) {
    const QJsonArray previewUrls =
        metadata.value(QStringLiteral("previewImages")).toArray();  // 缓存中的预览图地址数组。
    QStringList normalizedUrls;  // 通过 URL 规范化后可继续使用的地址列表。
    for (const QJsonValue& value : previewUrls) {
        const QString normalizedUrl = UrlUtils::normalizePreviewImageUrl(value.toString());  // 当前地址的规范化结果。
        if (!normalizedUrl.isEmpty()) {
            normalizedUrls.append(normalizedUrl);
        }
    }
    componentData.setPreviewImages(normalizedUrls);
}

/**
 * @brief 读取独立三维模型及其可选的平移、旋转变换。
 * @param metadata 已解析的缓存元数据。
 * @param componentData 待填充三维模型数据的组件数据对象。
 * @return 三维模型及其变换均可还原时返回 true。
 */
static bool readModel3D(const QJsonObject& metadata, ComponentData& componentData) {
    if (!metadata.contains(QStringLiteral("model3duuid"))) {
        return true;
    }

    auto model3DData = QSharedPointer<Model3DData>::create();  // 还原后的三维模型数据对象。
    model3DData->setUuid(metadata.value(QStringLiteral("model3duuid")).toString());
    model3DData->setName(metadata.value(QStringLiteral("model3dName")).toString());

    if (metadata.contains(QStringLiteral("model3dTranslation")) &&
        metadata.value(QStringLiteral("model3dTranslation")).isObject()) {
        Model3DBase translation;  // 三维模型平移变换。
        if (!translation.fromJson(metadata.value(QStringLiteral("model3dTranslation")).toObject())) {
            return false;
        }
        model3DData->setTranslation(translation);
    }

    if (metadata.contains(QStringLiteral("model3dRotation")) &&
        metadata.value(QStringLiteral("model3dRotation")).isObject()) {
        Model3DBase rotation;  // 三维模型旋转变换。
        if (!rotation.fromJson(metadata.value(QStringLiteral("model3dRotation")).toObject())) {
            return false;
        }
        model3DData->setRotation(rotation);
    }

    componentData.setModel3DData(model3DData);
    return true;
}

/**
 * @brief 组装完整组件数据并拒绝无法还原的三维变换。
 * @param componentId 元件编号。
 * @param metadata 已解析并通过基础校验的缓存元数据。
 * @return 构造成功的组件数据；三维变换无效时返回空指针。
 */
QSharedPointer<ComponentData> CacheComponentDataReader::read(const QString& componentId, const QJsonObject& metadata) {
    auto componentData = QSharedPointer<ComponentData>::create();  // 由缓存元数据构造的组件数据对象。
    componentData->setLcscId(componentId);
    readBasicFields(metadata, *componentData);
    readPreviewUrls(metadata, *componentData);
    if (!readModel3D(metadata, *componentData)) {
        return nullptr;
    }
    return componentData;
}

}  // namespace EasyKiConverter
