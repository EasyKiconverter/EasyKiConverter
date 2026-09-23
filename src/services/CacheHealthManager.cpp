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
// 生成指定元件的缓存目录路径。
QString _componentCacheDir(const QString& cacheRoot, const QString& lcscId) {
    return cacheRoot + "/" + lcscId;
}

// 生成元件元数据文件路径。
QString _metadataPath(const QString& cacheRoot, const QString& lcscId) {
    return _componentCacheDir(cacheRoot, lcscId) + "/component.json";
}

// 生成遗留数据手册文件路径。
QString _datasheetPath(const QString& cacheRoot, const QString& lcscId) {
    return _componentCacheDir(cacheRoot, lcscId) + "/datasheet";
}

// 生成指定序号的预览图缓存路径。
QString _previewImagePath(const QString& cacheRoot, const QString& lcscId, int index) {
    return _componentCacheDir(cacheRoot, lcscId) + "/preview_" + QString::number(index) + ".jpg";
}

// 根据数据手册格式生成当前缓存文件路径。
QString _resolveDatasheetPath(const QString& cacheRoot,
                              const QString& lcscId,
                              const QString& format,
                              bool /*migrate*/) {
    QString ext = format.isEmpty() ? "pdf" : format;
    return _componentCacheDir(cacheRoot, lcscId) + "/datasheet." + ext;
}

// 从指定文件读取并解析元数据 JSON 对象。
QJsonObject _loadMetadataFromPath(const QString& metaPath) {
    QFile metaFile(metaPath);
    if (!metaFile.open(QIODevice::ReadOnly)) {
        return QJsonObject();
    }
    QByteArray data = metaFile.readAll();
    metaFile.close();
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject()) {
        return QJsonObject();
    }
    return doc.object();
}

// 将元数据 JSON 以紧凑格式写入指定文件。
bool _saveMetadataToPath(const QString& metaPath, const QJsonObject& metadata) {
    QFile metaFile(metaPath);
    if (!metaFile.open(QIODevice::WriteOnly)) {
        return false;
    }
    QByteArray data = QJsonDocument(metadata).toJson(QJsonDocument::Compact);
    qint64 written = metaFile.write(data);
    metaFile.close();
    return written > 0;
}

// 校验缓存中三维模型标识及变换字段是否可解析。
bool _hasValidModel3DMetadata(const QJsonObject& metadata) {
    if (!metadata.contains(QStringLiteral("model3duuid")))
        return true;

    const QJsonValue uuid = metadata.value(QStringLiteral("model3duuid"));
    if (!uuid.isString() || uuid.toString().isEmpty())
        return false;

    QJsonObject model;
    model.insert(QStringLiteral("uuid"), uuid);
    if (metadata.contains(QStringLiteral("model3dName")))
        model.insert(QStringLiteral("name"), metadata.value(QStringLiteral("model3dName")));
    if (metadata.contains(QStringLiteral("model3dTranslation")))
        model.insert(QStringLiteral("translation"), metadata.value(QStringLiteral("model3dTranslation")));
    if (metadata.contains(QStringLiteral("model3dRotation")))
        model.insert(QStringLiteral("rotation"), metadata.value(QStringLiteral("model3dRotation")));

    Model3DData modelData;
    return modelData.fromJson(model);
}
}  // namespace

// 保存缓存根目录，供后续自愈操作解析各类缓存路径。
CacheHealthManager::CacheHealthManager(const QString& cacheRoot, const CacheSafety::TrashFunction& trash)
    : m_cacheRoot(cacheRoot), m_trash(trash) {}

// 扫描全部元件和三维模型缓存，并修复或移除无效条目。
int CacheHealthManager::healAll() {
    QDir rootDir(m_cacheRoot);
    if (!rootDir.exists()) {
        return 0;
    }
    if (!CacheSafety::isOwnedRoot(m_cacheRoot)) {
        qWarning().noquote() << "Skipped cache self-heal because ownership cannot be verified:" << m_cacheRoot;
        return 0;
    }

    int repairedComponents = 0;
    int removedComponents = 0;

    const QStringList componentDirs = rootDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
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

// 返回指定元件的缓存目录路径。
QString CacheHealthManager::componentCacheDir(const QString& lcscId) const {
    return _componentCacheDir(m_cacheRoot, lcscId);
}

// 返回指定元件的元数据文件路径。
QString CacheHealthManager::metadataPath(const QString& lcscId) const {
    return _metadataPath(m_cacheRoot, lcscId);
}

// 返回指定元件的遗留数据手册路径。
QString CacheHealthManager::datasheetPath(const QString& lcscId) const {
    return _datasheetPath(m_cacheRoot, lcscId);
}

// 返回指定序号的预览图缓存路径。
QString CacheHealthManager::previewImagePath(const QString& lcscId, int index) const {
    return _previewImagePath(m_cacheRoot, lcscId, index);
}

// 根据数据手册格式返回当前缓存路径。
QString CacheHealthManager::resolveDatasheetPath(const QString& lcscId, const QString& format, bool migrate) const {
    return _resolveDatasheetPath(m_cacheRoot, lcscId, format, migrate);
}

// 读取指定元件的缓存元数据。
QJsonObject CacheHealthManager::loadMetadata(const QString& lcscId) const {
    return _loadMetadataFromPath(metadataPath(lcscId));
}

// 保存指定元件的缓存元数据。
bool CacheHealthManager::saveMetadata(const QString& lcscId, const QJsonObject& metadata) {
    return _saveMetadataToPath(metadataPath(lcscId), metadata);
}

// 检查并修复单个元件缓存目录及其关联文件。
bool CacheHealthManager::repairComponentCache(const QString& lcscId) {
    const QString dirPath = componentCacheDir(lcscId);
    QDir componentDir(dirPath);
    if (!componentDir.exists()) {
        return false;
    }

    const QString metaPath = metadataPath(lcscId);
    if (!QFileInfo::exists(metaPath)) {
        qWarning().noquote() << "Preserved invalid cache dir without verifiable metadata:" << dirPath;
        return false;
    }

    QJsonObject metadata = loadMetadata(lcscId);
    if (metadata.isEmpty()) {
        qWarning().noquote() << "Preserved cache dir with unreadable metadata:" << dirPath;
        return false;
    }

    const QJsonValue metadataId = metadata.value(QStringLiteral("lcscId"));
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

    bool metadataChanged = false;
    if (metadata.value("lcscId").toString().isEmpty()) {
        metadata["lcscId"] = lcscId;
        metadataChanged = true;
    }
    if (!metadata.contains("cachedAt")) {
        metadata["cachedAt"] = QDateTime::currentDateTime().toString(Qt::ISODate);
        metadataChanged = true;
    }

    const QString cadPath = componentDir.filePath("cad_data.json");
    bool hasValidCad = false;
    if (QFileInfo::exists(cadPath)) {
        QFile cadFile(cadPath);
        if (cadFile.open(QIODevice::ReadOnly)) {
            const QByteArray cadData = cadFile.readAll();
            cadFile.close();

            QJsonParseError parseError;
            const QJsonDocument cadDoc = QJsonDocument::fromJson(cadData, &parseError);
            hasValidCad = !cadData.isEmpty() && parseError.error == QJsonParseError::NoError && cadDoc.isObject();
        }

        if (!hasValidCad) {
            QString error;
            if (!CacheSafety::moveToTrash(cadPath, &error, m_trash))
                qWarning().noquote() << "Preserved broken cad_data.json:" << cadPath << error;
        }
    }

    for (int index = 0; index < 3; ++index) {
        const QString previewPath = previewImagePath(lcscId, index);
        const QFileInfo previewInfo(previewPath);
        if (previewInfo.exists()) {
            QFile previewFile(previewPath);
            const bool valid = previewFile.open(QIODevice::ReadOnly) &&
                               ComponentCacheService::isValidPreviewImageData(previewFile.readAll());
            previewFile.close();
            if (!valid) {
                QString error;
                if (!CacheSafety::moveToTrash(previewPath, &error, m_trash))
                    qWarning().noquote() << "Preserved invalid preview cache file:" << previewPath << error;
            }
        }
    }

    const QString legacyDatasheetPath = datasheetPath(lcscId);
    if (QFileInfo::exists(legacyDatasheetPath)) {
        QFile legacyDatasheet(legacyDatasheetPath);
        if (legacyDatasheet.open(QIODevice::ReadOnly)) {
            const QByteArray legacyData = legacyDatasheet.readAll();
            legacyDatasheet.close();

            const QString format = legacyData.startsWith("%PDF-") ? QStringLiteral("pdf") : QStringLiteral("html");
            if (!ComponentCacheService::isValidDatasheetData(legacyData, format)) {
                QString error;
                if (!CacheSafety::moveToTrash(legacyDatasheetPath, &error, m_trash))
                    qWarning().noquote() << "Preserved invalid legacy datasheet cache file:" << legacyDatasheetPath
                                         << error;
            } else {
                const QString targetPath = resolveDatasheetPath(lcscId, format, true);
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

    const QString datasheetFormat = metadata.value("datasheetFormat").toString();
    const QString resolvedDatasheetPath = resolveDatasheetPath(lcscId, datasheetFormat, false);
    if (QFileInfo::exists(resolvedDatasheetPath)) {
        const QFileInfo datasheetInfo(resolvedDatasheetPath);
        QFile datasheetFile(resolvedDatasheetPath);
        const QString format = resolvedDatasheetPath.endsWith(QStringLiteral(".pdf"), Qt::CaseInsensitive)
                                   ? QStringLiteral("pdf")
                                   : QStringLiteral("html");
        const bool valid = datasheetFile.open(QIODevice::ReadOnly) &&
                           ComponentCacheService::isValidDatasheetData(datasheetFile.readAll(), format);
        datasheetFile.close();
        if (datasheetInfo.size() <= 0 || !valid) {
            QString error;
            if (!CacheSafety::moveToTrash(resolvedDatasheetPath, &error, m_trash))
                qWarning().noquote() << "Preserved invalid datasheet cache file:" << resolvedDatasheetPath << error;
        } else if (datasheetFormat.isEmpty()) {
            metadata["datasheetFormat"] =
                resolvedDatasheetPath.endsWith(".html") ? QStringLiteral("html") : QStringLiteral("pdf");
            metadataChanged = true;
        }
    }

    const bool hasBasicIdentity = !metadata.value("name").toString().isEmpty();
    const bool hasPreviewUrls = !metadata.value("previewImages").toArray().isEmpty();
    const bool hasDatasheetUrl = !metadata.value("datasheet").toString().isEmpty();
    const bool hasModel3DUuid = !metadata.value("model3duuid").toString().isEmpty();

    if (!hasValidCad && !hasBasicIdentity && !hasPreviewUrls && !hasDatasheetUrl && !hasModel3DUuid) {
        qWarning().noquote() << "Preserved unusable cache dir because recursive deletion is disabled:" << dirPath;
        return false;
    }

    if (metadataChanged) {
        saveMetadata(lcscId, metadata);
    }

    return true;
}

// 清理无效、空文件和临时文件形式的三维模型缓存。
void CacheHealthManager::repairModel3DCache() {
    const QString modelCacheDir = m_cacheRoot + "/model3d";
    QDir dir(modelCacheDir);
    if (!dir.exists()) {
        return;
    }
    if (!CacheSafety::isOwnedRoot(m_cacheRoot) || !QFileInfo::exists(dir.filePath(CacheSafety::model3DMarkerName()))) {
        qWarning().noquote() << "Skipped 3D cache self-heal because ownership cannot be verified:" << modelCacheDir;
        return;
    }

    const QSet<QString> validSuffixes = {QStringLiteral("obj"), QStringLiteral("step"), QStringLiteral("wrl")};
    const QFileInfoList files = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot, QDir::Name);
    for (const QFileInfo& fileInfo : files) {
        if (!CacheSafety::isOwnedModel3DFile(m_cacheRoot, fileInfo.absoluteFilePath()))
            continue;
        const QString suffix = fileInfo.suffix().toLower();
        bool validContent = false;
        if (validSuffixes.contains(suffix) && fileInfo.size() > 0) {
            QFile modelFile(fileInfo.absoluteFilePath());
            if (modelFile.open(QIODevice::ReadOnly)) {
                const QByteArray data = modelFile.readAll();
                if (suffix == QStringLiteral("obj")) {
                    validContent = Exporter3DModel::hasUsableObjGeometry(data);
                } else if (suffix == QStringLiteral("step")) {
                    validContent = Exporter3DModel::hasUsableStepData(data);
                } else {
                    validContent = Exporter3DModel::hasUsableWrlGeometry(data);
                }
            }
        }
        const bool removable =
            !validContent || !validSuffixes.contains(suffix) || fileInfo.fileName().startsWith(".tmp");
        if (removable) {
            QString error;
            if (!CacheSafety::moveToTrash(fileInfo.absoluteFilePath(), &error, m_trash))
                qWarning().noquote() << "Preserved broken 3D cache file:" << fileInfo.absoluteFilePath() << error;
        }
    }
}

}  // namespace EasyKiConverter
