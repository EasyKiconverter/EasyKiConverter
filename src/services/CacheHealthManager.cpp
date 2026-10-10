#include "CacheHealthManager.h"

#include "ComponentCacheService.h"
#include "core/kicad/Exporter3DModel.h"
#include "models/Model3DData.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QMutex>
#include <QMutexLocker>

namespace EasyKiConverter {

namespace {
/**
 * @brief 生成指定元件的缓存目录路径。
 * @param cacheRoot 缓存根目录。
 * @param lcscId 元器件编号。
 * @return 组件缓存目录路径。
 */
QString _componentCacheDir(const QString& cacheRoot, const QString& lcscId) {
    return cacheRoot + "/" + lcscId;
}

/**
 * @brief 生成元件元数据文件路径。
 * @param cacheRoot 缓存根目录。
 * @param lcscId 元器件编号。
 * @return 元数据文件路径。
 */
QString _metadataPath(const QString& cacheRoot, const QString& lcscId) {
    return _componentCacheDir(cacheRoot, lcscId) + "/component.json";
}

/**
 * @brief 生成遗留数据手册文件路径。
 * @param cacheRoot 缓存根目录。
 * @param lcscId 元器件编号。
 * @return 遗留数据手册路径。
 */
QString _datasheetPath(const QString& cacheRoot, const QString& lcscId) {
    return _componentCacheDir(cacheRoot, lcscId) + "/datasheet";
}

/**
 * @brief 生成指定序号的预览图缓存路径。
 * @param cacheRoot 缓存根目录。
 * @param lcscId 元器件编号。
 * @param index 预览图序号。
 * @return 预览图路径。
 */
QString _previewImagePath(const QString& cacheRoot, const QString& lcscId, int index) {
    return _componentCacheDir(cacheRoot, lcscId) + "/preview_" + QString::number(index) + ".jpg";
}

/**
 * @brief 根据数据手册格式生成当前缓存文件路径。
 * @param cacheRoot 缓存根目录。
 * @param lcscId 元器件编号。
 * @param format 数据手册格式。
 * @param migrate 是否迁移旧格式文件。
 * @return 当前数据手册路径。
 */
QString _resolveDatasheetPath(const QString& cacheRoot,
                              const QString& lcscId,
                              const QString& format,
                              bool /*migrate*/) {
    QString ext = format.isEmpty() ? "pdf" : format;  // 规范化后的文件扩展名。
    return _componentCacheDir(cacheRoot, lcscId) + "/datasheet." + ext;
}

/**
 * @brief 从指定文件读取并解析元数据 JSON 对象。
 * @param metaPath 元数据文件路径。
 * @return 元数据对象；读取或解析失败时返回空对象。
 */
QJsonObject _loadMetadataFromPath(const QString& metaPath) {
    QFile metaFile(metaPath);
    if (!metaFile.open(QIODevice::ReadOnly)) {
        return QJsonObject();
    }
    QByteArray data = metaFile.readAll();  // 元数据文件内容。
    metaFile.close();
    QJsonParseError error;  // JSON 解析错误。
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);  // 解析后的 JSON 文档。
    if (error.error != QJsonParseError::NoError || !doc.isObject()) {
        return QJsonObject();
    }
    return doc.object();
}

/**
 * @brief 将元数据 JSON 以紧凑格式写入指定文件。
 * @param metaPath 元数据文件路径。
 * @param metadata 待保存的元数据。
 * @return 写入成功时返回 true。
 */
bool _saveMetadataToPath(const QString& metaPath, const QJsonObject& metadata) {
    QFile metaFile(metaPath);
    if (!metaFile.open(QIODevice::WriteOnly)) {
        return false;
    }
    QByteArray data = QJsonDocument(metadata).toJson(QJsonDocument::Compact);  // 紧凑 JSON 数据。
    qint64 written = metaFile.write(data);  // 实际写入字节数。
    metaFile.close();
    return written > 0;
}

/**
 * @brief 校验缓存中三维模型标识及变换字段是否可解析。
 * @param metadata 待校验的元数据。
 * @return 字段有效或未声明三维模型时返回 true。
 */
bool _hasValidModel3DMetadata(const QJsonObject& metadata) {
    if (!metadata.contains(QStringLiteral("model3duuid")))
        return true;

    const QJsonValue uuid = metadata.value(QStringLiteral("model3duuid"));  // 三维模型标识字段。
    if (!uuid.isString() || uuid.toString().isEmpty())
        return false;

    QJsonObject model;  // 供 Model3DData 校验的模型对象。
    model.insert(QStringLiteral("uuid"), uuid);
    if (metadata.contains(QStringLiteral("model3dName")))
        model.insert(QStringLiteral("name"), metadata.value(QStringLiteral("model3dName")));
    if (metadata.contains(QStringLiteral("model3dTranslation")))
        model.insert(QStringLiteral("translation"), metadata.value(QStringLiteral("model3dTranslation")));
    if (metadata.contains(QStringLiteral("model3dRotation")))
        model.insert(QStringLiteral("rotation"), metadata.value(QStringLiteral("model3dRotation")));

    Model3DData modelData;  // 三维模型解析器实例。
    return modelData.fromJson(model);
}
}  // namespace

/**
 * @brief 创建缓存自愈管理器。
 * @param cacheRoot 缓存根目录。
 * @param trash 回收站移动策略。
 */
CacheHealthManager::CacheHealthManager(const QString& cacheRoot, const CacheSafety::TrashFunction& trash)
    : m_cacheRoot(cacheRoot), m_trash(trash) {}

/**
 * @brief 扫描全部元件和三维模型缓存，并修复或移除无效条目。
 * @return 保留的有效元器件缓存目录数量。
 */
int CacheHealthManager::healAll() {
    QDir rootDir(m_cacheRoot);  // 缓存根目录对象。
    if (!rootDir.exists()) {
        return 0;
    }
    if (!CacheSafety::isOwnedRoot(m_cacheRoot)) {
        qWarning().noquote() << "Skipped cache self-heal because ownership cannot be verified:" << m_cacheRoot;
        return 0;
    }

    int repairedComponents = 0;  // 修复并保留的组件数量。
    int removedComponents = 0;  // 判定无效的组件数量。

    const QStringList componentDirs =  // 缓存根目录下的组件目录。
        rootDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QString& entry : componentDirs) {
        if (entry == "model3d" || entry == CacheSafety::ownershipMarkerName()) {
            continue;
        }
        if (!CacheSafety::isOwnedComponentDirectory(m_cacheRoot, rootDir.filePath(entry))) {
            qWarning().noquote() << "Preserved cache entry with unverifiable ownership:" << rootDir.filePath(entry);
            continue;
        }
        if (repairComponentCache(entry)) {
            ++repairedComponents;
        } else {
            ++removedComponents;
        }
    }

    repairModel3DCache();

    if (repairedComponents > 0 || removedComponents > 0) {
        qInfo().noquote()
            << QString("Cache self-heal completed, retained %1 component cache dirs and removed %2 invalid dirs")
                   .arg(repairedComponents)
                   .arg(removedComponents);
    }

    return repairedComponents;
}

/**
 * @brief 返回指定元件的缓存目录路径。
 * @param lcscId 元器件编号。
 * @return 组件缓存目录路径。
 */
QString CacheHealthManager::componentCacheDir(const QString& lcscId) const {
    return _componentCacheDir(m_cacheRoot, lcscId);
}

/**
 * @brief 返回指定元件的元数据文件路径。
 * @param lcscId 元器件编号。
 * @return 元数据文件路径。
 */
QString CacheHealthManager::metadataPath(const QString& lcscId) const {
    return _metadataPath(m_cacheRoot, lcscId);
}

/**
 * @brief 返回指定元件的遗留数据手册路径。
 * @param lcscId 元器件编号。
 * @return 遗留数据手册路径。
 */
QString CacheHealthManager::datasheetPath(const QString& lcscId) const {
    return _datasheetPath(m_cacheRoot, lcscId);
}

/**
 * @brief 返回指定序号的预览图缓存路径。
 * @param lcscId 元器件编号。
 * @param index 预览图序号。
 * @return 预览图路径。
 */
QString CacheHealthManager::previewImagePath(const QString& lcscId, int index) const {
    return _previewImagePath(m_cacheRoot, lcscId, index);
}

/**
 * @brief 根据数据手册格式返回当前缓存路径。
 * @param lcscId 元器件编号。
 * @param format 数据手册格式。
 * @param migrate 是否迁移旧格式文件。
 * @return 数据手册路径。
 */
QString CacheHealthManager::resolveDatasheetPath(const QString& lcscId, const QString& format, bool migrate) const {
    return _resolveDatasheetPath(m_cacheRoot, lcscId, format, migrate);
}

/**
 * @brief 读取指定元件的缓存元数据。
 * @param lcscId 元器件编号。
 * @return 元数据对象。
 */
QJsonObject CacheHealthManager::loadMetadata(const QString& lcscId) const {
    return _loadMetadataFromPath(metadataPath(lcscId));
}

/**
 * @brief 保存指定元件的缓存元数据。
 * @param lcscId 元器件编号。
 * @param metadata 元数据对象。
 * @return 保存成功时返回 true。
 */
bool CacheHealthManager::saveMetadata(const QString& lcscId, const QJsonObject& metadata) {
    return _saveMetadataToPath(metadataPath(lcscId), metadata);
}

/**
 * @brief 检查并修复单个元件缓存目录及其关联文件。
 * @param lcscId 元器件编号。
 * @return 目录有效并保留时返回 true。
 */
bool CacheHealthManager::repairComponentCache(const QString& lcscId) {
    const QString dirPath = componentCacheDir(lcscId);  // 组件缓存目录路径。
    QDir componentDir(dirPath);  // 组件缓存目录对象。
    if (!componentDir.exists()) {
        return false;
    }

    const QString metaPath = metadataPath(lcscId);  // 元数据文件路径。
    if (!QFileInfo::exists(metaPath)) {
        qWarning().noquote() << "Preserved invalid cache dir without verifiable metadata:" << dirPath;
        return false;
    }

    QJsonObject metadata = loadMetadata(lcscId);  // 当前组件元数据。
    if (metadata.isEmpty()) {
        qWarning().noquote() << "Preserved cache dir with unreadable metadata:" << dirPath;
        return false;
    }

    const QJsonValue metadataId = metadata.value(QStringLiteral("lcscId"));  // 元数据中的组件编号。
    if (metadata.contains(QStringLiteral("lcscId")) &&
        (!metadataId.isString() ||
         (!metadataId.toString().isEmpty() && metadataId.toString().compare(lcscId, Qt::CaseInsensitive) != 0))) {
        qWarning().noquote() << "Preserved cache dir with mismatched component ID:" << dirPath;
        return false;
    }

    if (!_hasValidModel3DMetadata(metadata)) {
        qWarning().noquote() << "Preserved cache dir with invalid 3D metadata:" << dirPath;
        return false;
    }

    bool metadataChanged = false;  // 是否需要重新保存元数据。
    if (metadata.value("lcscId").toString().isEmpty()) {
        metadata["lcscId"] = lcscId;
        metadataChanged = true;
    }
    if (!metadata.contains("cachedAt")) {
        metadata["cachedAt"] = QDateTime::currentDateTime().toString(Qt::ISODate);
        metadataChanged = true;
    }

    const QString cadPath = componentDir.filePath("cad_data.json");  // CAD JSON 文件路径。
    bool hasValidCad = false;  // CAD JSON 是否有效。
    if (QFileInfo::exists(cadPath)) {
        QFile cadFile(cadPath);
        if (cadFile.open(QIODevice::ReadOnly)) {
            const QByteArray cadData = cadFile.readAll();  // CAD JSON 文件内容。
            cadFile.close();

            QJsonParseError parseError;  // CAD JSON 解析错误。
            const QJsonDocument cadDoc = QJsonDocument::fromJson(cadData, &parseError);  // 解析后的文档。
            hasValidCad = !cadData.isEmpty() && parseError.error == QJsonParseError::NoError && cadDoc.isObject();
        }

        if (!hasValidCad) {
            QString error;  // 回收站操作的错误文本。
            if (!CacheSafety::moveToTrash(cadPath, &error, m_trash))
                qWarning().noquote() << "Preserved broken cad_data.json:" << cadPath << error;
        }
    }

    for (int index = 0; index < 3; ++index) {
        const QString previewPath = previewImagePath(lcscId, index);  // 当前预览图路径。
        const QFileInfo previewInfo(previewPath);  // 预览图文件信息。
        if (previewInfo.exists()) {
            QFile previewFile(previewPath);
            const bool valid = previewFile.open(QIODevice::ReadOnly) &&  // 预览图内容是否有效。
                               ComponentCacheService::isValidPreviewImageData(previewFile.readAll());
            previewFile.close();
            if (!valid) {
                QString error;  // 回收站操作的错误文本。
                if (!CacheSafety::moveToTrash(previewPath, &error, m_trash))
                    qWarning().noquote() << "Preserved invalid preview cache file:" << previewPath << error;
            }
        }
    }

    const QString legacyDatasheetPath = datasheetPath(lcscId);  // 遗留数据手册路径。
    if (QFileInfo::exists(legacyDatasheetPath)) {
        QFile legacyDatasheet(legacyDatasheetPath);
        if (legacyDatasheet.open(QIODevice::ReadOnly)) {
            const QByteArray legacyData = legacyDatasheet.readAll();  // 遗留数据手册内容。
            legacyDatasheet.close();

            const QString format =  // 根据文件头推断的手册格式。
                legacyData.startsWith("%PDF-") ? QStringLiteral("pdf") : QStringLiteral("html");
            if (!ComponentCacheService::isValidDatasheetData(legacyData, format)) {
                QString error;  // 回收站操作的错误文本。
                if (!CacheSafety::moveToTrash(legacyDatasheetPath, &error, m_trash))
                    qWarning().noquote() << "Preserved invalid legacy datasheet cache file:" << legacyDatasheetPath
                                         << error;
            } else {
                const QString targetPath = resolveDatasheetPath(lcscId, format, true);  // 新格式目标路径。
                if (QFile::exists(targetPath)) {
                    qWarning().noquote() << "Preserved legacy datasheet because target already exists:" << targetPath;
                } else if (legacyDatasheet.rename(legacyDatasheetPath, targetPath)) {
                    metadata["datasheetFormat"] = format;
                    metadataChanged = true;
                    qInfo().noquote() << "Migrated legacy datasheet cache file:" << targetPath;
                } else {
                    qWarning().noquote() << "Preserved legacy datasheet that could not be migrated:"
                                         << legacyDatasheetPath;
                }
            }
        } else {
            qWarning().noquote() << "Preserved unreadable legacy datasheet cache file:" << legacyDatasheetPath;
        }
    }

    const QString datasheetFormat = metadata.value("datasheetFormat").toString();  // 元数据声明的手册格式。
    const QString resolvedDatasheetPath =  // 当前格式数据手册路径。
        resolveDatasheetPath(lcscId, datasheetFormat, false);
    if (QFileInfo::exists(resolvedDatasheetPath)) {
        const QFileInfo datasheetInfo(resolvedDatasheetPath);  // 数据手册文件信息。
        QFile datasheetFile(resolvedDatasheetPath);  // 待读取的数据手册文件。
        // 根据文件扩展名确定数据手册格式。
        const QString format = resolvedDatasheetPath.endsWith(QStringLiteral(".pdf"), Qt::CaseInsensitive)
                                   ? QStringLiteral("pdf")
                                   : QStringLiteral("html");
        const bool valid = datasheetFile.open(QIODevice::ReadOnly) &&  // 数据手册内容是否有效。
                           ComponentCacheService::isValidDatasheetData(datasheetFile.readAll(), format);
        datasheetFile.close();
        if (datasheetInfo.size() <= 0 || !valid) {
            QString error;  // 回收站操作的错误文本。
            if (!CacheSafety::moveToTrash(resolvedDatasheetPath, &error, m_trash))
                qWarning().noquote() << "Preserved invalid datasheet cache file:" << resolvedDatasheetPath << error;
        } else if (datasheetFormat.isEmpty()) {
            metadata["datasheetFormat"] =
                resolvedDatasheetPath.endsWith(".html") ? QStringLiteral("html") : QStringLiteral("pdf");
            metadataChanged = true;
        }
    }

    const bool hasBasicIdentity = !metadata.value("name").toString().isEmpty();  // 是否有基本身份信息。
    const bool hasPreviewUrls = !metadata.value("previewImages").toArray().isEmpty();  // 是否有预览图链接。
    const bool hasDatasheetUrl = !metadata.value("datasheet").toString().isEmpty();  // 是否有手册链接。
    const bool hasModel3DUuid = !metadata.value("model3duuid").toString().isEmpty();  // 是否有关联三维模型。

    if (!hasValidCad && !hasBasicIdentity && !hasPreviewUrls && !hasDatasheetUrl && !hasModel3DUuid) {
        qWarning().noquote() << "Preserved unusable cache dir because recursive deletion is disabled:" << dirPath;
        return false;
    }

    if (metadataChanged) {
        saveMetadata(lcscId, metadata);
    }

    return true;
}

/**
 * @brief 清理无效、空文件和临时文件形式的三维模型缓存。
 */
void CacheHealthManager::repairModel3DCache() {
    const QString modelCacheDir = m_cacheRoot + "/model3d";  // 三维模型缓存目录。
    QDir dir(modelCacheDir);  // 三维模型缓存目录对象。
    if (!dir.exists()) {
        return;
    }
    const QFileInfo modelCacheInfo(modelCacheDir);  // 三维模型缓存目录信息。
    if (modelCacheInfo.isSymLink() || !CacheSafety::isOwnedRoot(m_cacheRoot) ||
        !QFileInfo::exists(dir.filePath(CacheSafety::model3DMarkerName()))) {
        qWarning().noquote() << "Skipped 3D cache self-heal because ownership cannot be verified:" << modelCacheDir;
        return;
    }

    const QSet<QString> validSuffixes =  // 支持的三维模型扩展名。
        {QStringLiteral("obj"), QStringLiteral("step"), QStringLiteral("wrl")};
    const QFileInfoList files =  // 三维模型目录中的文件。
        dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot, QDir::Name);
    for (const QFileInfo& fileInfo : files) {
        if (!CacheSafety::isOwnedModel3DFile(m_cacheRoot, fileInfo.absoluteFilePath()))
            continue;
        const QString suffix = fileInfo.suffix().toLower();  // 当前文件扩展名。
        bool validContent = false;  // 文件内容是否通过格式校验。
        if (validSuffixes.contains(suffix) && fileInfo.size() > 0) {
            QFile modelFile(fileInfo.absoluteFilePath());
            if (modelFile.open(QIODevice::ReadOnly)) {
                const QByteArray data = modelFile.readAll();  // 三维模型文件内容。
                if (suffix == QStringLiteral("obj")) {
                    validContent = Exporter3DModel::hasUsableObjGeometry(data);
                } else if (suffix == QStringLiteral("step")) {
                    validContent = Exporter3DModel::hasUsableStepData(data);
                } else {
                    validContent = Exporter3DModel::hasUsableWrlGeometry(data);
                }
            }
        }
        const bool removable =  // 当前文件是否应移入回收站。
            !validContent || !validSuffixes.contains(suffix) || fileInfo.fileName().startsWith(".tmp");
        if (removable) {
            QString error;  // 回收站操作的错误文本。
            if (!CacheSafety::moveToTrash(fileInfo.absoluteFilePath(), &error, m_trash))
                qWarning().noquote() << "Preserved broken 3D cache file:" << fileInfo.absoluteFilePath() << error;
        }
    }
}

}  // namespace EasyKiConverter

/*
 * 我从小就没得老公得。在我嘿小的时候，我就出生了。
 * 我出生的那年，一个朋友都没有来看过我。一岁之前，
 * 我连走路都不会，我从来没有说过一个苦字。
 * 我没得文化得，6岁，6岁斗把我送去读书了。
 * 我眼睛一闭，眼前就一片漆黑，啥子都看不到了，
 * 我有没有说过一个苦字？
 */
