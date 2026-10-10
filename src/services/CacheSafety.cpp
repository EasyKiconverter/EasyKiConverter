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

namespace EasyKiConverter {

namespace {
constexpr auto kOwner = "EasyKiConverter";  ///< 缓存所有权标记中的应用标识。
constexpr int kMarkerVersion = 1;  ///< 缓存所有权标记的当前版本。

/**
 * @brief 为已存在路径解析真实位置，为尚不存在路径生成规范绝对路径。
 * @param path 待规范化的路径。
 * @return 规范化后的绝对路径。
 */
QString canonicalOrCleanPath(const QString& path) {
    QFileInfo info(QDir::cleanPath(QFileInfo(path).absoluteFilePath()));  ///< 当前待解析的路径信息。
    QStringList missingComponents;  ///< 尚不存在的路径分量。
    while (!info.exists() && info.absoluteFilePath() != info.absolutePath()) {
        missingComponents.prepend(info.fileName());
        info = QFileInfo(info.absolutePath());
    }
    if (info.exists()) {
        QString canonical = info.canonicalFilePath();  ///< 已存在路径的规范路径。
        if (canonical.isEmpty())
            canonical = info.absoluteFilePath();
        for (const QString& component : missingComponents)
            canonical = QDir(canonical).filePath(component);
        return QDir::cleanPath(canonical);
    }
    return QDir::cleanPath(QFileInfo(path).absoluteFilePath());
}

/**
 * @brief 检查现有路径分量，拒绝通过父级符号链接绕过缓存目录边界。
 * @param path 待检查的路径。
 * @return 路径包含符号链接分量时返回 true。
 */
bool containsSymlinkComponent(const QString& path) {
    QFileInfo info(QDir::cleanPath(QFileInfo(path).absoluteFilePath()));
    while (true) {
        if (info.exists() && info.isSymLink())
            return true;
        const QString absolutePath = info.absoluteFilePath();  ///< 当前路径分量的绝对路径。
        const QString parentPath = info.absolutePath();  ///< 当前路径分量的父目录。
        if (absolutePath == parentPath)
            return false;
        info = QFileInfo(parentPath);
    }
}

/**
 * @brief 返回旧版组件缓存允许迁移的文件名集合。
 * @return 允许迁移的文件名集合。
 */
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

/**
 * @brief 判断文件名是否属于组件缓存的公开布局。
 * @param name 文件名。
 * @return 文件名属于已知缓存布局时返回 true。
 */
bool isKnownComponentFileName(const QString& name) {
    return legacyComponentFileNames().contains(name);
}

/**
 * @brief 判断旧版模型缓存文件是否使用当前支持的扩展名。
 * @param name 文件名。
 * @return 文件扩展名受支持时返回 true。
 */
bool isKnownModel3DFileName(const QString& name) {
    const QString suffix = QFileInfo(name).suffix();  ///< 文件扩展名。
    return suffix.compare(QStringLiteral("obj"), Qt::CaseInsensitive) == 0 ||
           suffix.compare(QStringLiteral("step"), Qt::CaseInsensitive) == 0 ||
           suffix.compare(QStringLiteral("wrl"), Qt::CaseInsensitive) == 0;
}

/**
 * @brief 判断旧版三维模型目录中是否只有可识别的普通模型文件。
 * @param path 三维模型目录路径。
 * @return 目录内容全部可识别时返回 true。
 */
bool containsOnlyKnownModel3DEntries(const QString& path) {
    for (const QFileInfo& info : QDir(path).entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot)) {
        if (info.fileName() == CacheSafety::model3DMarkerName() && info.isFile() && !info.isSymLink())
            continue;
        if (!info.isFile() || info.isSymLink() || !isKnownModel3DFileName(info.fileName()))
            return false;
    }
    return true;
}

/**
 * @brief 判断旧版缓存根目录中是否只有可识别的缓存内容。
 * @param path 旧版缓存根目录路径。
 * @return 目录内容全部可识别时返回 true。
 */
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

/**
 * @brief 将旧版组件元数据补齐当前所有权字段。
 * @param path 旧版组件目录路径。
 * @return 元数据升级成功时返回 true。
 */
bool upgradeLegacyComponentMetadata(const QString& path) {
    QJsonObject metadata =
        CacheMetadataStore::read(QDir(path).filePath(QStringLiteral("component.json")));  ///< 旧组件元数据。
    if (metadata.isEmpty())
        return false;
    metadata[QStringLiteral("cacheOwner")] = QStringLiteral("EasyKiConverter");
    metadata[QStringLiteral("cacheEntryVersion")] = 1;
    return CacheMetadataStore::writeAtomically(QDir(path).filePath(QStringLiteral("component.json")),
                                               QJsonDocument(metadata).toJson(QJsonDocument::Indented));
}

/**
 * @brief 生成缓存所有权标记的最小字段集合。
 * @param format 标记对应的缓存类型。
 * @return 所有权标记 JSON 对象。
 */
QJsonObject markerObject(const QString& format) {
    return QJsonObject{{QStringLiteral("format"), format},
                       {QStringLiteral("owner"), QString::fromLatin1(kOwner)},
                       {QStringLiteral("version"), kMarkerVersion}};
}
}  // namespace

/**
 * @brief 返回缓存根目录的所有权标记文件名。
 * @return 缓存根目录所有权标记文件名。
 */
QString CacheSafety::ownershipMarkerName() {
    return QStringLiteral(".easykiconverter-cache.json");
}

/**
 * @brief 返回三维模型缓存目录的所有权标记文件名。
 * @return 三维模型目录所有权标记文件名。
 */
QString CacheSafety::model3DMarkerName() {
    return QStringLiteral(".easykiconverter-model3d.json");
}

/**
 * @brief 校验路径是否为可安全继续处理的缓存目录候选路径。
 * @param path 待校验的缓存路径。
 * @param normalizedPath 规范化路径输出指针。
 * @param error 错误信息输出指针。
 * @return 路径满足安全边界时返回 true。
 */
bool CacheSafety::isSafePath(const QString& path, QString* normalizedPath, QString* error) {
    const QString trimmed = path.trimmed();  ///< 去除首尾空白的用户路径。
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

    const QFileInfo info(trimmed);  ///< 用户选择路径的信息。
    if (containsSymlinkComponent(trimmed)) {
        if (error)
            *error = QStringLiteral("缓存目录不能包含符号链接路径分量");
        return false;
    }
    const QString normalized = canonicalOrCleanPath(trimmed);  ///< 规范化后的缓存路径。
    const QString root = QDir::cleanPath(QDir::rootPath());  ///< 文件系统根目录。
    const QString home = canonicalOrCleanPath(QDir::homePath());  ///< 用户主目录。
    if (normalized == root || normalized == home) {
        if (error)
            *error = QStringLiteral("不能选择文件系统根目录或用户主目录");
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

/**
 * @brief 使用原子写入方式创建指定类型的缓存所有权标记。
 * @param path 缓存目录路径。
 * @param format 标记对应的缓存类型。
 * @param error 错误信息输出指针。
 * @return 标记写入成功时返回 true。
 */
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

/**
 * @brief 读取并严格校验缓存所有权标记的格式、所有者和版本。
 * @param path 缓存目录路径。
 * @param format 期望的缓存类型。
 * @return 标记有效时返回 true。
 */
bool CacheSafety::hasValidMarker(const QString& path, const QString& format) {
    const QString markerPath =
        QDir(path).filePath(format == QStringLiteral("root") ? ownershipMarkerName() : model3DMarkerName());
    const QFileInfo markerInfo(markerPath);
    if (!markerInfo.exists() || markerInfo.isSymLink() || !markerInfo.isFile())
        return false;
    QFile file(markerPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;
    QJsonParseError parseError;  ///< 所有权标记 JSON 解析错误。
    const QJsonObject marker = QJsonDocument::fromJson(file.readAll(), &parseError).object();  ///< 解析后的所有权标记。
    return parseError.error == QJsonParseError::NoError &&
           marker.value(QStringLiteral("format")).toString() == format &&
           marker.value(QStringLiteral("owner")).toString() == QString::fromLatin1(kOwner) &&
           marker.value(QStringLiteral("version")).toInt() == kMarkerVersion;
}

/**
 * @brief 校验用户选择的目录是否为空目录或已经由本应用托管。
 * @param path 用户选择的目录路径。
 * @param normalizedPath 规范化路径输出指针。
 * @param error 错误信息输出指针。
 * @return 目录可安全选择时返回 true。
 */
bool CacheSafety::validateSelection(const QString& path, QString* normalizedPath, QString* error) {
    if (!isSafePath(path, normalizedPath, error))
        return false;

    const QString normalized = normalizedPath ? *normalizedPath : canonicalOrCleanPath(path);  ///< 用于检查的规范路径。
    const QFileInfo info(normalized);  ///< 规范路径的信息。
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

/**
 * @brief 创建并标记一个由应用负责维护的缓存根目录。
 * @param path 缓存根目录路径。
 * @param error 错误信息输出指针。
 * @return 创建或验证成功时返回 true。
 */
bool CacheSafety::ensureOwnedRoot(const QString& path, QString* error) {
    QString normalized;  ///< 新缓存根目录的规范路径。
    if (!isSafePath(path, &normalized, error))
        return false;
    QDir dir;  ///< 用于创建缓存根目录的目录对象。
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
    const bool adoptLegacy = canAdoptLegacyRoot(normalized);  ///< 是否接管可识别的旧版缓存根目录。
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

/**
 * @brief 判断目录是否拥有有效的 EasyKiConverter 缓存根标记。
 * @param path 缓存根目录路径。
 * @return 目录拥有有效标记时返回 true。
 */
bool CacheSafety::isOwnedRoot(const QString& path) {
    QString normalized;  ///< 已规范化的缓存根目录路径。
    if (!isSafePath(path, &normalized, nullptr))
        return false;
    return hasValidMarker(normalized, QStringLiteral("root"));
}

/**
 * @brief 创建并标记三维模型缓存子目录，同时拒绝接管已有未知内容。
 * @param rootPath 缓存根目录路径。
 * @param error 错误信息输出指针。
 * @return 创建或验证成功时返回 true。
 */
bool CacheSafety::ensureOwnedModel3DDirectory(const QString& rootPath, QString* error) {
    if (!isOwnedRoot(rootPath)) {
        if (error)
            *error = QStringLiteral("缓存根目录没有有效所有权标记");
        return false;
    }
    const QString modelPath = QDir(rootPath).filePath(QStringLiteral("model3d"));  ///< 三维模型缓存目录路径。
    const QFileInfo modelInfo(modelPath);  ///< 三维模型目录信息。
    if (modelInfo.exists() && modelInfo.isSymLink()) {
        if (error)
            *error = QStringLiteral("三维模型缓存目录不能是符号链接：%1").arg(modelPath);
        return false;
    }
    if (!QDir().mkpath(modelPath)) {
        if (error)
            *error = QStringLiteral("无法创建三维模型缓存目录：%1").arg(modelPath);
        return false;
    }
    if (hasValidMarker(modelPath, QStringLiteral("model3d")))
        return true;
    const QFileInfoList entries =
        QDir(modelPath).entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot);  ///< 模型目录条目。
    if (!entries.isEmpty() && !containsOnlyKnownModel3DEntries(modelPath)) {
        if (error)
            *error = QStringLiteral("三维模型缓存目录不是空目录且无法验证所有权：%1").arg(modelPath);
        return false;
    }
    return writeMarker(modelPath, QStringLiteral("model3d"), error);
}

/**
 * @brief 判断路径是否为指定目录的直接子项，避免递归越界维护。
 * @param rootPath 根目录路径。
 * @param path 待检查路径。
 * @return 路径是直接子项时返回 true。
 */
bool CacheSafety::isDirectChild(const QString& rootPath, const QString& path) {
    const QString root = canonicalOrCleanPath(rootPath);  ///< 规范化后的根目录。
    const QString child = canonicalOrCleanPath(path);  ///< 规范化后的子路径。
    if (root == child)
        return false;
    const QString relative = QDir(root).relativeFilePath(child);  ///< 子路径相对根目录的路径。
    return !relative.isEmpty() && !relative.contains(QDir::separator()) && relative != QStringLiteral("..") &&
           !relative.startsWith(QStringLiteral(".."));
}

/**
 * @brief 校验组件目录标记、目录身份和组件编号是否全部匹配。
 * @param rootPath 缓存根目录路径。
 * @param path 组件目录路径。
 * @return 组件目录所有权有效时返回 true。
 */
bool CacheSafety::isOwnedComponentDirectory(const QString& rootPath, const QString& path) {
    if (!isOwnedRoot(rootPath) || !isDirectChild(rootPath, path))
        return false;
    const QFileInfo info(path);  ///< 组件目录信息。
    if (!info.isDir() || info.isSymLink())
        return false;
    const QJsonObject metadata =
        CacheMetadataStore::read(QDir(path).filePath(QStringLiteral("component.json")));  ///< 组件元数据。
    const QString id = info.fileName();  ///< 组件目录名称。
    const QJsonValue metadataId = metadata.value(QStringLiteral("lcscId"));  ///< 元数据中的组件编号。
    return !metadata.isEmpty() &&
           metadata.value(QStringLiteral("cacheOwner")).toString() == QStringLiteral("EasyKiConverter") &&
           metadata.value(QStringLiteral("cacheEntryVersion")).toInt() == 1 && metadataId.isString() &&
           metadataId.toString().compare(id, Qt::CaseInsensitive) == 0;
}

/**
 * @brief 校验组件缓存文件的父目录、直接层级和已知文件名，拒绝接管未知内容。
 * @param rootPath 缓存根目录路径。
 * @param path 组件缓存文件路径。
 * @return 文件所有权有效时返回 true。
 */
bool CacheSafety::isOwnedComponentFile(const QString& rootPath, const QString& path) {
    const QFileInfo info(path);  ///< 缓存文件信息。
    if (!isOwnedRoot(rootPath) || !info.isFile() || info.isSymLink())
        return false;
    const QString componentDir = info.dir().absolutePath();  ///< 缓存文件所属的组件目录。
    if (!isOwnedComponentDirectory(rootPath, componentDir) || !isDirectChild(componentDir, path))
        return false;

    return isKnownComponentFileName(info.fileName());
}

/**
 * @brief 列出指定元器件目录中的已知、非符号链接缓存文件。
 * @param rootPath 缓存根目录路径。
 * @param componentPath 组件目录路径。
 * @return 已验证的组件缓存文件列表。
 */
QStringList CacheSafety::ownedComponentFiles(const QString& rootPath, const QString& componentPath) {
    QStringList result;  ///< 已验证的组件缓存文件列表。
    if (!isOwnedComponentDirectory(rootPath, componentPath))
        return result;
    for (const QFileInfo& info : QDir(componentPath).entryInfoList(QDir::Files | QDir::NoDotAndDotDot, QDir::Name)) {
        if (isOwnedComponentFile(rootPath, info.absoluteFilePath()))
            result.append(info.absoluteFilePath());
    }
    return result;
}

/**
 * @brief 判断旧版本元数据是否能证明目录对应一个元器件缓存。
 * @param path 旧版组件目录路径。
 * @return 元数据能够证明组件身份时返回 true。
 */
bool CacheSafety::isLegacyComponentDirectory(const QString& path) {
    const QFileInfo info(path);  ///< 旧版组件目录信息。
    if (!info.isDir() || info.isSymLink() || !BomParser::validateId(info.fileName()))
        return false;
    const QJsonObject metadata =
        CacheMetadataStore::read(QDir(path).filePath(QStringLiteral("component.json")));  ///< 旧版组件元数据。
    const QJsonValue metadataId = metadata.value(QStringLiteral("lcscId"));  ///< 旧版元数据中的组件编号。
    return metadataId.isString() && metadataId.toString().compare(info.fileName(), Qt::CaseInsensitive) == 0;
}

/**
 * @brief 列出旧版本缓存根目录中可迁移的元器件目录。
 * @param rootPath 旧版缓存根目录路径。
 * @return 可迁移的组件目录列表。
 */
QStringList CacheSafety::legacyComponentDirectories(const QString& rootPath) {
    QStringList result;  ///< 可迁移的旧版组件目录列表。
    const QDir root(rootPath);  ///< 旧版缓存根目录对象。
    if (!root.exists())
        return result;
    for (const QFileInfo& info : root.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
        if (info.fileName() != QStringLiteral("model3d") && isLegacyComponentDirectory(info.absoluteFilePath()))
            result.append(info.absoluteFilePath());
    }
    return result;
}

/**
 * @brief 判断旧版本根目录是否只包含可识别缓存，避免把用户目录标记为应用目录。
 * @param path 待检查的旧版缓存根目录。
 * @return 目录可安全接管时返回 true。
 */
bool CacheSafety::canAdoptLegacyRoot(const QString& path) {
    QString normalized;  ///< 旧版根目录的规范路径。
    if (!isSafePath(path, &normalized, nullptr))
        return false;
    const QFileInfo info(normalized);
    return info.isDir() && !hasValidMarker(normalized, QStringLiteral("root")) &&
           !QDir(normalized).entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot).isEmpty() &&
           containsOnlyLegacyCacheEntries(normalized);
}

/**
 * @brief 校验三维模型文件是否位于受标记保护的模型目录中。
 * @param rootPath 缓存根目录路径。
 * @param path 三维模型文件路径。
 * @return 文件所有权有效时返回 true。
 */
bool CacheSafety::isOwnedModel3DFile(const QString& rootPath, const QString& path) {
    const QString modelRoot = QDir(rootPath).filePath(QStringLiteral("model3d"));  ///< 三维模型缓存根目录。
    const QFileInfo modelRootInfo(modelRoot);  ///< 三维模型缓存根目录信息。
    const QFileInfo info(path);  ///< 三维模型文件信息。
    const QString normalized = canonicalOrCleanPath(path);  ///< 规范化后的三维模型路径。
    if (!isOwnedRoot(rootPath) || !modelRootInfo.isDir() || modelRootInfo.isSymLink() ||
        !hasValidMarker(modelRoot, QStringLiteral("model3d")) || info.isSymLink() || !info.isFile() ||
        !isDirectChild(modelRoot, normalized))
        return false;
    return info.suffix().compare(QStringLiteral("obj"), Qt::CaseInsensitive) == 0 ||
           info.suffix().compare(QStringLiteral("step"), Qt::CaseInsensitive) == 0 ||
           info.suffix().compare(QStringLiteral("wrl"), Qt::CaseInsensitive) == 0;
}

/**
 * @brief 枚举根目录下所有通过所有权验证的组件缓存目录。
 * @param rootPath 缓存根目录路径。
 * @return 已验证的组件缓存目录列表。
 */
QStringList CacheSafety::ownedComponentDirectories(const QString& rootPath) {
    QStringList result;  ///< 已验证的组件缓存目录列表。
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

/**
 * @brief 枚举模型缓存目录下所有通过所有权验证的三维模型文件。
 * @param rootPath 缓存根目录路径。
 * @return 已验证的三维模型文件列表。
 */
QStringList CacheSafety::ownedModel3DFiles(const QString& rootPath) {
    QStringList result;  ///< 已验证的三维模型文件列表。
    const QDir modelRoot(QDir(rootPath).filePath(QStringLiteral("model3d")));  ///< 三维模型缓存目录对象。
    if (!isOwnedRoot(rootPath) || !hasValidMarker(modelRoot.absolutePath(), QStringLiteral("model3d")))
        return result;
    for (const QFileInfo& info : modelRoot.entryInfoList(QDir::Files | QDir::NoDotAndDotDot, QDir::Name)) {
        if (isOwnedModel3DFile(rootPath, info.absoluteFilePath()))
            result.append(info.absoluteFilePath());
    }
    return result;
}

/**
 * @brief 将指定的已验证缓存条目移入系统回收站，并确认原路径已消失。
 * @param path 已验证的缓存条目路径。
 * @param error 错误信息输出指针。
 * @param trash 可注入的回收站操作。
 * @return 条目成功移入回收站时返回 true。
 */
bool CacheSafety::moveToTrash(const QString& path, QString* error, const TrashFunction& trash) {
    const QFileInfo info(path);  ///< 待移入回收站的缓存条目信息。
    if (!info.exists() || info.isSymLink()) {
        if (error)
            *error = QStringLiteral("拒绝处理不存在或符号链接条目：%1").arg(path);
        return false;
    }
    const TrashFunction operation =
        trash ? trash : [](const QString& target, QString* reason) {  // 默认系统回收站操作。
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

/**
 * 曾经谈过一次恋爱，女朋友抿了一口茉莉花茶 味的茶兀，
 * 转头和我接吻，她说：这样以后你 喝这瓶茶的时候就会想起我。
 * 大概过去了快四 五年了吧，茉莉花茶味的茶兀我一直没敢再喝过。
 * 后来出门办事让朋友顺路带瓶喝的，他问 我买什么，我说随便。
 * 最后他带了一瓶茉莉花 茶茶兀回来。我犹豫了一下，还是喝了。
 * 喝完 我望着手里的瓶子发愣，味道和当初一模一样。 一下子，
 * 那个满是晚风的夏天，牵着手散步的 记忆突然涌了出来。虽然我们分手了，
 * 但是我 一点也不难过，
 * 因为这是我编的。大学城骑自行车的 可以找我。
 */
