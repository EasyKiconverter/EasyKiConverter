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

// 获取元器件可用于缓存的三维模型信息。
Model3DData effectiveModel3D(const ComponentData& data) {
    Model3DData model;
    if (data.model3DData()) {
        model = *data.model3DData();
    }
    if (model.uuid().isEmpty() && data.footprintData()) {
        const Model3DData footprintModel = data.footprintData()->model3D();
        if (!footprintModel.uuid().isEmpty()) {
            model = footprintModel;
        }
    }
    return model;
}

}  // namespace

// 读取并解析磁盘中的元数据 JSON。
QJsonObject CacheMetadataStore::read(const QString& metadataPath) {
    if (!QFileInfo::exists(metadataPath)) {
        return QJsonObject();
    }

    QFile file(metadataPath);
    if (!file.open(QIODevice::ReadOnly)) {
        return QJsonObject();
    }

    const QByteArray data = file.readAll();
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(data, &error);
    if (error.error != QJsonParseError::NoError) {
        LOG_WARN(LogModule::Core, "JSON parse error: {}", error.errorString());
        return QJsonObject();
    }

    return document.object();
}

// 校验缓存元数据中的三维模型字段和变换向量。
bool CacheMetadataStore::hasValidModel3D(const QJsonObject& metadata) {
    if (!metadata.contains(QStringLiteral("model3duuid"))) {
        return true;
    }

    const QJsonValue uuid = metadata.value(QStringLiteral("model3duuid"));
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
        const QJsonValue value = metadata.value(name);
        if (!value.isObject()) {
            return false;
        }
        Model3DBase vector;
        return vector.fromJson(value.toObject());
    };

    return validateVector(QStringLiteral("model3dTranslation")) && validateVector(QStringLiteral("model3dRotation"));
}

// 从元器件对象构建完整的缓存元数据 JSON。
QJsonObject CacheMetadataStore::build(const QString& componentId, const ComponentData& data) {
    QJsonObject metadata;
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

    QJsonArray previewUrls;
    for (const QString& url : data.previewImages()) {
        const QString normalizedUrl = UrlUtils::normalizePreviewImageUrl(url);
        if (!normalizedUrl.isEmpty()) {
            previewUrls.append(normalizedUrl);
        }
    }
    metadata["previewImages"] = previewUrls;

    const Model3DData model3D = effectiveModel3D(data);
    if (!model3D.uuid().isEmpty()) {
        metadata["model3duuid"] = model3D.uuid();
        metadata["model3dName"] = model3D.name();
        metadata["model3dTranslation"] = model3D.translation().toJson();
        metadata["model3dRotation"] = model3D.rotation().toJson();
    }

    return metadata;
}

// 判断元器件自身或封装数据是否包含可持久化的三维模型。
bool CacheMetadataStore::hasModel3D(const ComponentData& data) {
    return !effectiveModel3D(data).uuid().isEmpty();
}

// 合并增量元数据，同时保留已有的非空字段。
QJsonObject CacheMetadataStore::merge(const QJsonObject& existing, const QJsonObject& incoming) {
    QJsonObject merged = existing;
    for (auto it = incoming.begin(); it != incoming.end(); ++it) {
        const QJsonValue& value = it.value();
        bool shouldWrite = true;

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

// 使用 QSaveFile 写入并提交缓存文件，避免产生半写入文件。
bool CacheMetadataStore::writeAtomically(const QString& path, const QByteArray& data) {
    QFileInfo info(path);
    if (!info.absoluteDir().exists() && !QDir().mkpath(info.absolutePath())) {
        return false;
    }

    QSaveFile file(path);
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
