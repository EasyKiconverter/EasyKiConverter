#include "CacheMetadataStore.h"

#include "core/utils/UrlUtils.h"
#include "utils/logging/LogMacros.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QSaveFile>

namespace EasyKiConverter {

namespace {

/**
 * @brief 获取元器件可用于缓存的三维模型信息。
 * @param data 元器件数据。
 * @return 元器件自身或封装关联的三维模型数据。
 */
Model3DData effectiveModel3D(const ComponentData& data) {
    Model3DData model;  // 当前选中的三维模型数据。
    if (data.model3DData()) {
        model = *data.model3DData();
    }
    if (model.uuid().isEmpty() && data.footprintData()) {
        const Model3DData footprintModel = data.footprintData()->model3D();  // 封装关联的三维模型数据。
        if (!footprintModel.uuid().isEmpty()) {
            model = footprintModel;
        }
    }
    return model;
}

}  // namespace

/**
 * @brief 读取并解析磁盘中的元数据 JSON。
 * @param metadataPath 元数据文件路径。
 * @return 解析成功的 JSON 对象，文件不存在或内容无效时返回空对象。
 */
QJsonObject CacheMetadataStore::read(const QString& metadataPath) {
    if (!QFileInfo::exists(metadataPath)) {
        return QJsonObject();
    }

    QFile file(metadataPath);
    if (!file.open(QIODevice::ReadOnly)) {
        return QJsonObject();
    }

    const QByteArray data = file.readAll();  // 元数据文件原始内容。
    QJsonParseError error;  // JSON 解析错误信息。
    const QJsonDocument document = QJsonDocument::fromJson(data, &error);  // 解析后的 JSON 文档。
    if (error.error != QJsonParseError::NoError) {
        LOG_WARN(LogModule::Core, "JSON parse error: {}", error.errorString());
        return QJsonObject();
    }

    return document.object();
}

/**
 * @brief 校验缓存元数据中的三维模型字段和变换向量。
 * @param metadata 待校验的缓存元数据。
 * @return 三维模型字段有效或未声明模型时返回 true。
 */
bool CacheMetadataStore::hasValidModel3D(const QJsonObject& metadata) {
    if (!metadata.contains(QStringLiteral("model3duuid"))) {
        return true;
    }

    const QJsonValue uuid = metadata.value(QStringLiteral("model3duuid"));  // 三维模型 UUID 字段。
    if (!uuid.isString() || uuid.toString().isEmpty()) {
        return false;
    }

    if (metadata.contains(QStringLiteral("model3dName")) && !metadata.value(QStringLiteral("model3dName")).isString()) {
        return false;
    }

    const auto validateVector = [&metadata](const QString& name) {
        if (!metadata.contains(name)) {
            return true;
        }
        const QJsonValue value = metadata.value(name);  // 当前变换字段值。
        if (!value.isObject()) {
            return false;
        }
        Model3DBase vector;  // 待解析的三维变换向量。
        return vector.fromJson(value.toObject());
    };

    return validateVector(QStringLiteral("model3dTranslation")) && validateVector(QStringLiteral("model3dRotation"));
}

/**
 * @brief 从元器件对象构建完整的缓存元数据 JSON。
 * @param componentId 元器件编号。
 * @param data 元器件数据。
 * @return 可持久化的缓存元数据对象。
 */
QJsonObject CacheMetadataStore::build(const QString& componentId, const ComponentData& data) {
    QJsonObject metadata;  // 待写入磁盘的缓存元数据。
    metadata[QStringLiteral("cacheOwner")] = QStringLiteral("EasyKiConverter");
    metadata[QStringLiteral("cacheEntryVersion")] = 1;
    metadata["lcscId"] = componentId;
    metadata["name"] = data.name();
    metadata["prefix"] = data.prefix();
    metadata["package"] = data.package();
    metadata["manufacturer"] = data.manufacturer();
    metadata["manufacturerPart"] = data.manufacturerPart();
    metadata["datasheet"] = data.datasheet();
    metadata["datasheetFormat"] = data.datasheetFormat();
    metadata["cachedAt"] = QDateTime::currentDateTime().toString(Qt::ISODate);

    QJsonArray previewUrls;  // 通过规范化校验的预览图地址数组。
    for (const QString& url : data.previewImages()) {
        const QString normalizedUrl = UrlUtils::normalizePreviewImageUrl(url);  // 当前预览图地址的规范化结果。
        if (!normalizedUrl.isEmpty()) {
            previewUrls.append(normalizedUrl);
        }
    }
    metadata["previewImages"] = previewUrls;

    const Model3DData model3D = effectiveModel3D(data);  // 元器件可持久化的三维模型。
    if (!model3D.uuid().isEmpty()) {
        metadata["model3duuid"] = model3D.uuid();
        metadata["model3dName"] = model3D.name();
        metadata["model3dTranslation"] = model3D.translation().toJson();
        metadata["model3dRotation"] = model3D.rotation().toJson();
    }

    return metadata;
}

/**
 * @brief 判断元器件自身或封装数据是否包含可持久化的三维模型。
 * @param data 元器件数据。
 * @return 存在有效三维模型 UUID 时返回 true。
 */
bool CacheMetadataStore::hasModel3D(const ComponentData& data) {
    return !effectiveModel3D(data).uuid().isEmpty();
}

/**
 * @brief 合并增量元数据，同时保留已有的非空字段。
 * @param existing 已有缓存元数据。
 * @param incoming 新产生的增量元数据。
 * @return 合并后的缓存元数据。
 */
QJsonObject CacheMetadataStore::merge(const QJsonObject& existing, const QJsonObject& incoming) {
    QJsonObject merged = existing;  // 合并结果，初始保留已有字段。
    for (auto it = incoming.begin(); it != incoming.end(); ++it) {
        const QJsonValue& value = it.value();  // 当前增量字段值。
        bool shouldWrite = true;  // 是否将当前字段写入合并结果。

        if (value.isString() && value.toString().isEmpty()) {
            shouldWrite = false;
        } else if (value.isArray() && value.toArray().isEmpty()) {
            shouldWrite = false;
        } else if (value.isObject() && value.toObject().isEmpty()) {
            shouldWrite = false;
        }

        if (shouldWrite) {
            merged[it.key()] = value;
        }
    }

    merged["cachedAt"] = incoming.value("cachedAt").toString(QDateTime::currentDateTime().toString(Qt::ISODate));
    return merged;
}

/**
 * @brief 使用 QSaveFile 写入并提交缓存文件，避免产生半写入文件。
 * @param path 目标文件路径。
 * @param data 待写入的数据。
 * @return 文件成功提交时返回 true。
 */
bool CacheMetadataStore::writeAtomically(const QString& path, const QByteArray& data) {
    QFileInfo info(path);  // 目标文件及其父目录信息。
    if (!info.absoluteDir().exists() && !QDir().mkpath(info.absolutePath())) {
        return false;
    }

    QSaveFile file(path);  // 提供原子提交语义的临时写入文件。
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }

    if (file.write(data) != data.size()) {
        file.cancelWriting();
        return false;
    }

    return file.commit();
}

}  // namespace EasyKiConverter
