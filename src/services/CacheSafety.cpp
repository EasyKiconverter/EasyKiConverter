#include "CacheSafety.h"

#include "BomParser.h"
#include "CacheMetadataStore.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>

namespace EasyKiConverter {

namespace {
constexpr auto kOwner = "EasyKiConverter";
constexpr int kMarkerVersion = 1;

// 为已存在路径解析真实位置，为尚不存在路径生成规范绝对路径。
QString canonicalOrCleanPath(const QString& path) {
    const QFileInfo info(path);
    if (info.exists()) {
        const QString canonical = info.canonicalFilePath();
        if (!canonical.isEmpty())
            return QDir::cleanPath(canonical);
    }
    return QDir::cleanPath(QDir(path).absolutePath());
}

// 判断候选路径是否位于指定祖先目录中，包含祖先自身。
bool isAncestorOrSame(const QString& ancestor, const QString& candidate) {
    const QString relative = QDir(ancestor).relativeFilePath(candidate);
    return relative == QStringLiteral(".") ||
           (!relative.startsWith(QStringLiteral("..")) && !QDir::isAbsolutePath(relative));
}

// 判断路径是否位于系统临时目录下的隔离子目录中，避免接管整个临时目录。
bool isUnderTemporaryDirectory(const QString& path) {
    const QString temporaryRoot = canonicalOrCleanPath(QDir::tempPath());
    return temporaryRoot != path && isAncestorOrSame(temporaryRoot, path);
}

// 返回旧版组件缓存允许迁移的文件名集合。
const QSet<QString>& legacyComponentFileNames() {
    static const QSet<QString> names = {
        QStringLiteral("component.json"),
        QStringLiteral("symbol.json"),
        QStringLiteral("footprint.json"),
        QStringLiteral("cad_data.json"),
        QStringLiteral("datasheet"),
        QStringLiteral("datasheet.pdf"),
        QStringLiteral("datasheet.html"),
        QStringLiteral("preview_0.jpg"),
        QStringLiteral("preview_1.jpg"),
        QStringLiteral("preview_2.jpg"),
    };
    return names;
}

// 判断文件名是否属于组件缓存的公开布局。
bool isKnownComponentFileName(const QString& name) {
    return legacyComponentFileNames().contains(name);
}

// 判断旧版模型缓存文件是否使用当前支持的扩展名。
bool isKnownModel3DFileName(const QString& name) {
    const QString suffix = QFileInfo(name).suffix();
    return suffix.compare(QStringLiteral("obj"), Qt::CaseInsensitive) == 0 ||
           suffix.compare(QStringLiteral("step"), Qt::CaseInsensitive) == 0 ||
           suffix.compare(QStringLiteral("wrl"), Qt::CaseInsensitive) == 0;
}

// 判断旧版三维模型目录中是否只有可识别的普通模型文件。
bool containsOnlyKnownModel3DEntries(const QString& path) {
    for (const QFileInfo& info : QDir(path).entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot)) {
        if (info.fileName() == CacheSafety::model3DMarkerName() && info.isFile() && !info.isSymLink())
            continue;
        if (!info.isFile() || info.isSymLink() || !isKnownModel3DFileName(info.fileName()))
            return false;
    }
    return true;
}

// 判断旧版缓存根目录中是否只有可识别的缓存内容。
bool containsOnlyLegacyCacheEntries(const QString& path) {
    const QDir root(path);
    for (const QFileInfo& info : root.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot)) {
        if (info.isSymLink())
            return false;
        if (info.isDir()) {
            if (info.fileName() == QStringLiteral("model3d")) {
                if (!containsOnlyKnownModel3DEntries(info.absoluteFilePath()))
                    return false;
                continue;
            }
            if (QDir(info.absoluteFilePath()).entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot).isEmpty())
                continue;
            if (!CacheSafety::isLegacyComponentDirectory(info.absoluteFilePath()))
                return false;
            for (const QFileInfo& child :
                 QDir(info.absoluteFilePath()).entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot)) {
                if (!child.isFile() || child.isSymLink() || !isKnownComponentFileName(child.fileName()))
                    return false;
            }
            continue;
        }
        return false;
    }
    return true;
}

// 将旧版组件元数据补齐当前所有权字段。
bool upgradeLegacyComponentMetadata(const QString& path) {
    QJsonObject metadata = CacheMetadataStore::read(QDir(path).filePath(QStringLiteral("component.json")));
    if (metadata.isEmpty())
        return false;
    metadata[QStringLiteral("cacheOwner")] = QStringLiteral("EasyKiConverter");
    metadata[QStringLiteral("cacheEntryVersion")] = 1;
    return CacheMetadataStore::writeAtomically(QDir(path).filePath(QStringLiteral("component.json")),
                                               QJsonDocument(metadata).toJson(QJsonDocument::Indented));
}

// 生成缓存所有权标记的最小字段集合。
QJsonObject markerObject(const QString& format) {
    return QJsonObject{{QStringLiteral("format"), format},
                       {QStringLiteral("owner"), QString::fromLatin1(kOwner)},
                       {QStringLiteral("version"), kMarkerVersion}};
}
}  // namespace

// 返回缓存根目录的所有权标记文件名。
QString CacheSafety::ownershipMarkerName() {
    return QStringLiteral(".easykiconverter-cache.json");
}

// 返回三维模型缓存目录的所有权标记文件名。
QString CacheSafety::model3DMarkerName() {
    return QStringLiteral(".easykiconverter-model3d.json");
}

// 校验路径是否为可安全继续处理的缓存目录候选路径。
bool CacheSafety::isSafePath(const QString& path, QString* normalizedPath, QString* error) {
    const QString trimmed = path.trimmed();
    if (trimmed.isEmpty()) {
        if (error)
            *error = QStringLiteral("缓存目录不能为空");
        return false;
    }
    if (!QDir::isAbsolutePath(trimmed)) {
        if (error)
            *error = QStringLiteral("缓存目录必须使用绝对路径");
        return false;
    }

    const QFileInfo info(trimmed);
    if (info.exists() && info.isSymLink()) {
        if (error)
            *error = QStringLiteral("缓存目录不能是符号链接");
        return false;
    }
    const QString normalized = canonicalOrCleanPath(trimmed);
    const QString root = QDir::cleanPath(QDir::rootPath());
    const QString home = canonicalOrCleanPath(QDir::homePath());
    const QString defaultCache = canonicalOrCleanPath(
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/cache"));
    const bool isAllowedTemporaryPath = isUnderTemporaryDirectory(normalized);
    if (normalized == root ||
        (isAncestorOrSame(home, normalized) && normalized != defaultCache && !isAllowedTemporaryPath)) {
        if (error)
            *error = QStringLiteral("不能选择文件系统根目录或用户主目录及其上级目录");
        return false;
    }
    if (info.exists() && !info.isDir()) {
        if (error)
            *error = QStringLiteral("缓存路径必须是目录");
        return false;
    }
    if (normalizedPath)
        *normalizedPath = normalized;
    return true;
}

// 使用原子写入方式创建指定类型的缓存所有权标记。
bool CacheSafety::writeMarker(const QString& path, const QString& format, QString* error) {
    QSaveFile file(QDir(path).filePath(format == QStringLiteral("root") ? ownershipMarkerName() : model3DMarkerName()));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text) ||
        file.write(QJsonDocument(markerObject(format)).toJson(QJsonDocument::Compact)) < 0 || !file.commit()) {
        if (error)
            *error = QStringLiteral("无法写入缓存所有权标记：%1").arg(file.fileName());
        return false;
    }
    return true;
}

// 读取并严格校验缓存所有权标记的格式、所有者和版本。
bool CacheSafety::hasValidMarker(const QString& path, const QString& format) {
    const QString markerPath =
        QDir(path).filePath(format == QStringLiteral("root") ? ownershipMarkerName() : model3DMarkerName());
    const QFileInfo markerInfo(markerPath);
    if (!markerInfo.exists() || markerInfo.isSymLink() || !markerInfo.isFile())
        return false;
    QFile file(markerPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;
    QJsonParseError parseError;
    const QJsonObject marker = QJsonDocument::fromJson(file.readAll(), &parseError).object();
    return parseError.error == QJsonParseError::NoError &&
           marker.value(QStringLiteral("format")).toString() == format &&
           marker.value(QStringLiteral("owner")).toString() == QString::fromLatin1(kOwner) &&
           marker.value(QStringLiteral("version")).toInt() == kMarkerVersion;
}

// 校验用户选择的目录是否为空目录或已经由本应用托管。
bool CacheSafety::validateSelection(const QString& path, QString* normalizedPath, QString* error) {
    if (!isSafePath(path, normalizedPath, error))
        return false;

    const QString normalized = normalizedPath ? *normalizedPath : canonicalOrCleanPath(path);
    const QFileInfo info(normalized);
    if (!info.exists())
        return true;
    if (hasValidMarker(normalized, QStringLiteral("root")))
        return true;
    const QDir dir(normalized);
    if (!dir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot).isEmpty() && !canAdoptLegacyRoot(normalized)) {
        if (error)
            *error = QStringLiteral("缓存目录必须为空，或已经包含 EasyKiConverter 所有权标记");
        return false;
    }
    return true;
}

// 创建并标记一个由应用负责维护的缓存根目录。
bool CacheSafety::ensureOwnedRoot(const QString& path, QString* error) {
    QString normalized;
    if (!isSafePath(path, &normalized, error))
        return false;
    QDir dir;
    if (!dir.exists(normalized) && !dir.mkpath(normalized)) {
        if (error)
            *error = QStringLiteral("无法创建缓存目录：%1").arg(normalized);
        return false;
    }
    if (hasValidMarker(normalized, QStringLiteral("root")))
        return true;
    if (!QDir(normalized).entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot).isEmpty() &&
        !canAdoptLegacyRoot(normalized)) {
        if (error)
            *error = QStringLiteral("缓存目录不是空目录且无法验证所有权：%1").arg(normalized);
        return false;
    }
    const bool adoptLegacy = canAdoptLegacyRoot(normalized);
    if (!writeMarker(normalized, QStringLiteral("root"), error))
        return false;
    if (adoptLegacy) {
        for (const QString& componentPath : legacyComponentDirectories(normalized)) {
            if (!upgradeLegacyComponentMetadata(componentPath)) {
                if (error)
                    *error = QStringLiteral("无法升级旧缓存元数据：%1").arg(componentPath);
                return false;
            }
        }
    }
    return true;
}

// 判断目录是否拥有有效的 EasyKiConverter 缓存根标记。
bool CacheSafety::isOwnedRoot(const QString& path) {
    QString normalized;
    if (!isSafePath(path, &normalized, nullptr))
        return false;
    return hasValidMarker(normalized, QStringLiteral("root"));
}

// 创建并标记三维模型缓存子目录，同时拒绝接管已有未知内容。
bool CacheSafety::ensureOwnedModel3DDirectory(const QString& rootPath, QString* error) {
    if (!isOwnedRoot(rootPath)) {
        if (error)
            *error = QStringLiteral("缓存根目录没有有效所有权标记");
        return false;
    }
    const QString modelPath = QDir(rootPath).filePath(QStringLiteral("model3d"));
    if (!QDir().mkpath(modelPath)) {
        if (error)
            *error = QStringLiteral("无法创建三维模型缓存目录：%1").arg(modelPath);
        return false;
    }
    if (hasValidMarker(modelPath, QStringLiteral("model3d")))
        return true;
    const QFileInfoList entries = QDir(modelPath).entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot);
    if (!entries.isEmpty() && !containsOnlyKnownModel3DEntries(modelPath)) {
        if (error)
            *error = QStringLiteral("三维模型缓存目录不是空目录且无法验证所有权：%1").arg(modelPath);
        return false;
    }
    return writeMarker(modelPath, QStringLiteral("model3d"), error);
}

// 判断路径是否为指定目录的直接子项，避免递归越界维护。
bool CacheSafety::isDirectChild(const QString& rootPath, const QString& path) {
    const QString root = canonicalOrCleanPath(rootPath);
    const QString child = canonicalOrCleanPath(path);
    if (root == child)
        return false;
    const QString relative = QDir(root).relativeFilePath(child);
    return !relative.isEmpty() && !relative.contains(QDir::separator()) && relative != QStringLiteral("..") &&
           !relative.startsWith(QStringLiteral(".."));
}

// 校验组件目录标记、目录身份和组件编号是否全部匹配。
bool CacheSafety::isOwnedComponentDirectory(const QString& rootPath, const QString& path) {
    if (!isOwnedRoot(rootPath) || !isDirectChild(rootPath, path))
        return false;
    const QFileInfo info(path);
    if (!info.isDir() || info.isSymLink())
        return false;
    const QJsonObject metadata = CacheMetadataStore::read(QDir(path).filePath(QStringLiteral("component.json")));
    const QString id = info.fileName();
    const QJsonValue metadataId = metadata.value(QStringLiteral("lcscId"));
    return !metadata.isEmpty() &&
           metadata.value(QStringLiteral("cacheOwner")).toString() == QStringLiteral("EasyKiConverter") &&
           metadata.value(QStringLiteral("cacheEntryVersion")).toInt() == 1 && metadataId.isString() &&
           metadataId.toString().compare(id, Qt::CaseInsensitive) == 0;
}

// 校验组件缓存文件的父目录、直接层级和已知文件名，拒绝接管未知内容。
bool CacheSafety::isOwnedComponentFile(const QString& rootPath, const QString& path) {
    const QFileInfo info(path);
    if (!isOwnedRoot(rootPath) || !info.isFile() || info.isSymLink())
        return false;
    const QString componentDir = info.dir().absolutePath();
    if (!isOwnedComponentDirectory(rootPath, componentDir) || !isDirectChild(componentDir, path))
        return false;

    return isKnownComponentFileName(info.fileName());
}

// 列出指定元器件目录中的已知、非符号链接缓存文件。
QStringList CacheSafety::ownedComponentFiles(const QString& rootPath, const QString& componentPath) {
    QStringList result;
    if (!isOwnedComponentDirectory(rootPath, componentPath))
        return result;
    for (const QFileInfo& info : QDir(componentPath).entryInfoList(QDir::Files | QDir::NoDotAndDotDot, QDir::Name)) {
        if (isOwnedComponentFile(rootPath, info.absoluteFilePath()))
            result.append(info.absoluteFilePath());
    }
    return result;
}

// 判断旧版本元数据是否能证明目录对应一个元器件缓存。
bool CacheSafety::isLegacyComponentDirectory(const QString& path) {
    const QFileInfo info(path);
    if (!info.isDir() || info.isSymLink() || !BomParser::validateId(info.fileName()))
        return false;
    const QJsonObject metadata = CacheMetadataStore::read(QDir(path).filePath(QStringLiteral("component.json")));
    const QJsonValue metadataId = metadata.value(QStringLiteral("lcscId"));
    return metadataId.isString() && metadataId.toString().compare(info.fileName(), Qt::CaseInsensitive) == 0;
}

// 列出旧版本缓存根目录中可迁移的元器件目录。
QStringList CacheSafety::legacyComponentDirectories(const QString& rootPath) {
    QStringList result;
    const QDir root(rootPath);
    if (!root.exists())
        return result;
    for (const QFileInfo& info : root.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
        if (info.fileName() != QStringLiteral("model3d") && isLegacyComponentDirectory(info.absoluteFilePath()))
            result.append(info.absoluteFilePath());
    }
    return result;
}

// 判断旧版本根目录是否只包含可识别缓存，避免把用户目录标记为应用目录。
bool CacheSafety::canAdoptLegacyRoot(const QString& path) {
    QString normalized;
    if (!isSafePath(path, &normalized, nullptr))
        return false;
    const QFileInfo info(normalized);
    return info.isDir() && !hasValidMarker(normalized, QStringLiteral("root")) &&
           !QDir(normalized).entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot).isEmpty() &&
           containsOnlyLegacyCacheEntries(normalized);
}

// 校验三维模型文件是否位于受标记保护的模型目录中。
bool CacheSafety::isOwnedModel3DFile(const QString& rootPath, const QString& path) {
    const QString modelRoot = QDir(rootPath).filePath(QStringLiteral("model3d"));
    const QFileInfo info(path);
    const QString normalized = canonicalOrCleanPath(path);
    if (!isOwnedRoot(rootPath) || !hasValidMarker(modelRoot, QStringLiteral("model3d")) || info.isSymLink() ||
        !info.isFile() || !isDirectChild(modelRoot, normalized))
        return false;
    return info.suffix().compare(QStringLiteral("obj"), Qt::CaseInsensitive) == 0 ||
           info.suffix().compare(QStringLiteral("step"), Qt::CaseInsensitive) == 0 ||
           info.suffix().compare(QStringLiteral("wrl"), Qt::CaseInsensitive) == 0;
}

// 枚举根目录下所有通过所有权验证的组件缓存目录。
QStringList CacheSafety::ownedComponentDirectories(const QString& rootPath) {
    QStringList result;
    if (!isOwnedRoot(rootPath))
        return result;
    const QDir root(rootPath);
    for (const QFileInfo& info : root.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
        if (info.fileName() != QStringLiteral("model3d") &&
            isOwnedComponentDirectory(rootPath, info.absoluteFilePath()))
            result.append(info.absoluteFilePath());
    }
    return result;
}

// 枚举模型缓存目录下所有通过所有权验证的三维模型文件。
QStringList CacheSafety::ownedModel3DFiles(const QString& rootPath) {
    QStringList result;
    const QDir modelRoot(QDir(rootPath).filePath(QStringLiteral("model3d")));
    if (!isOwnedRoot(rootPath) || !hasValidMarker(modelRoot.absolutePath(), QStringLiteral("model3d")))
        return result;
    for (const QFileInfo& info : modelRoot.entryInfoList(QDir::Files | QDir::NoDotAndDotDot, QDir::Name)) {
        if (isOwnedModel3DFile(rootPath, info.absoluteFilePath()))
            result.append(info.absoluteFilePath());
    }
    return result;
}

// 将指定的已验证缓存条目移入系统回收站，并确认原路径已消失。
bool CacheSafety::moveToTrash(const QString& path, QString* error, const TrashFunction& trash) {
    const QFileInfo info(path);
    if (!info.exists() || info.isSymLink()) {
        if (error)
            *error = QStringLiteral("拒绝处理不存在或符号链接条目：%1").arg(path);
        return false;
    }
    const TrashFunction operation = trash ? trash : [](const QString& target, QString* reason) {
        if (QFile::moveToTrash(target))
            return true;
        if (reason)
            *reason = QStringLiteral("系统回收站不支持或移动失败");
        return false;
    };
    if (!operation(path, error))
        return false;
    if (QFileInfo::exists(path)) {
        if (error)
            *error = QStringLiteral("回收站操作未移除原路径：%1").arg(path);
        return false;
    }
    return true;
}

}  // namespace EasyKiConverter
