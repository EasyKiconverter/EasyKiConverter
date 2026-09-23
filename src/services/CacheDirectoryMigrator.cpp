#include "CacheDirectoryMigrator.h"

#include "CacheMetadataStore.h"
#include "CacheSafety.h"
#include "utils/logging/LogMacros.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>

namespace EasyKiConverter {

namespace {

// 判断旧版模型文件是否可以在没有目录标记时迁移。
bool isLegacyModelFile(const QFileInfo& info) {
    if (!info.isFile() || info.isSymLink())
        return false;
    const QString suffix = info.suffix();
    return suffix.compare(QStringLiteral("obj"), Qt::CaseInsensitive) == 0 ||
           suffix.compare(QStringLiteral("step"), Qt::CaseInsensitive) == 0 ||
           suffix.compare(QStringLiteral("wrl"), Qt::CaseInsensitive) == 0;
}

// 将旧版本元数据升级为当前缓存所有权格式。
bool upgradeMetadata(const QString& path) {
    QJsonObject metadata = CacheMetadataStore::read(path);
    if (metadata.isEmpty())
        return false;
    metadata[QStringLiteral("cacheOwner")] = QStringLiteral("EasyKiConverter");
    metadata[QStringLiteral("cacheEntryVersion")] = 1;
    return CacheMetadataStore::writeAtomically(path, QJsonDocument(metadata).toJson(QJsonDocument::Indented));
}

}  // namespace

bool CacheDirectoryMigrator::migrate(const QString& oldCacheDir, const QString& newCacheDir) {
    if (oldCacheDir.isEmpty() || newCacheDir.isEmpty() || oldCacheDir == newCacheDir) {
        return true;
    }

    QDir source(oldCacheDir);
    if (!source.exists()) {
        return true;
    }
    const bool legacySource = !CacheSafety::isOwnedRoot(oldCacheDir);
    if (legacySource && !CacheSafety::canAdoptLegacyRoot(oldCacheDir) &&
        !source.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot).isEmpty()) {
        LOG_WARN(
            LogModule::Core, "Skipped legacy cache migration because source ownership is ambiguous: {}", oldCacheDir);
        return false;
    }
    if (!CacheSafety::isOwnedRoot(newCacheDir)) {
        LOG_WARN(
            LogModule::Core, "Skipped cache migration because target ownership cannot be verified: {}", newCacheDir);
        return false;
    }

    QDir target;
    if (!target.exists(newCacheDir) && !target.mkpath(newCacheDir)) {
        LOG_WARN(LogModule::Core, "Failed to create cache migration target directory: {}", newCacheDir);
        return false;
    }

    bool moved = true;
    const QStringList componentDirectories = legacySource ? CacheSafety::legacyComponentDirectories(oldCacheDir)
                                                          : CacheSafety::ownedComponentDirectories(oldCacheDir);
    for (const QString& sourcePath : componentDirectories) {
        const QString targetPath = QDir(newCacheDir).filePath(QFileInfo(sourcePath).fileName());
        if (!moveDirectoryContents(oldCacheDir, sourcePath, newCacheDir, targetPath, legacySource))
            moved = false;
    }
    QStringList modelFiles = CacheSafety::ownedModel3DFiles(oldCacheDir);
    if (legacySource) {
        const QDir modelRoot(QDir(oldCacheDir).filePath(QStringLiteral("model3d")));
        for (const QFileInfo& info : modelRoot.entryInfoList(QDir::Files | QDir::NoDotAndDotDot, QDir::Name)) {
            if (isLegacyModelFile(info))
                modelFiles.append(info.absoluteFilePath());
        }
    }
    for (const QString& sourcePath : modelFiles) {
        const QString targetPath = QDir(newCacheDir).filePath(QStringLiteral("model3d")) + QDir::separator() +
                                   QFileInfo(sourcePath).fileName();
        if (!moveCacheEntry(sourcePath, targetPath))
            moved = false;
    }
    if (moved)
        LOG_DEBUG(LogModule::Core, "Migrated owned cache entries from {} to {}", oldCacheDir, newCacheDir);
    else
        LOG_WARN(LogModule::Core,
                 "Cache migration preserved source entries after a conflict: {} -> {}",
                 oldCacheDir,
                 newCacheDir);
    return moved;
}

bool CacheDirectoryMigrator::moveDirectoryContents(const QString& sourceRoot,
                                                   const QString& sourceDir,
                                                   const QString& targetRoot,
                                                   const QString& targetDir,
                                                   bool legacySource) {
    QDir source(sourceDir);
    if (!source.exists()) {
        return true;
    }

    QDir target;
    if (target.exists(targetDir) && !CacheSafety::isOwnedComponentDirectory(targetRoot, targetDir) &&
        !QDir(targetDir).entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot).isEmpty()) {
        LOG_WARN(LogModule::Core, "Refusing cache migration into an unowned target directory: {}", targetDir);
        return false;
    }
    if (!target.exists(targetDir) && !target.mkpath(targetDir)) {
        LOG_WARN(LogModule::Core, "Failed to create cache migration directory: {}", targetDir);
        return false;
    }

    // 组件目录的元数据会在迁移第一个文件后暂时离开源目录，因此必须在循环前完成一次所有权确认。
    const bool sourceOwned = legacySource || CacheSafety::isOwnedComponentDirectory(sourceRoot, sourceDir);
    if (!sourceOwned) {
        LOG_WARN(LogModule::Core, "Refusing cache migration from an unowned component directory: {}", sourceDir);
        return false;
    }

    bool allMoved = true;
    const QStringList knownFiles = {QStringLiteral("component.json"),
                                    QStringLiteral("symbol.json"),
                                    QStringLiteral("footprint.json"),
                                    QStringLiteral("cad_data.json"),
                                    QStringLiteral("datasheet"),
                                    QStringLiteral("datasheet.pdf"),
                                    QStringLiteral("datasheet.html"),
                                    QStringLiteral("preview_0.jpg"),
                                    QStringLiteral("preview_1.jpg"),
                                    QStringLiteral("preview_2.jpg")};
    const QFileInfoList entries = source.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot);
    for (const QFileInfo& entryInfo : entries) {
        const bool known = entryInfo.isFile() && !entryInfo.isSymLink() && knownFiles.contains(entryInfo.fileName());
        if (!known) {
            allMoved = false;
            continue;
        }
        const QString sourcePath = entryInfo.absoluteFilePath();
        const QString targetPath = QDir(targetDir).filePath(entryInfo.fileName());
        if (!moveCacheEntry(sourcePath, targetPath)) {
            allMoved = false;
        }
    }

    const QString metadataPath = QDir(targetDir).filePath(QStringLiteral("component.json"));
    if (QFileInfo::exists(metadataPath) && !upgradeMetadata(metadataPath))
        allMoved = false;
    if (allMoved && source.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot).isEmpty() &&
        !QDir().rmdir(sourceDir)) {
        LOG_WARN(LogModule::Core, "Failed to remove empty cache migration directory: {}", sourceDir);
        allMoved = false;
    }
    return allMoved;
}

bool CacheDirectoryMigrator::moveCacheEntry(const QString& sourcePath, const QString& targetPath) {
    QFileInfo sourceInfo(sourcePath);
    if (!sourceInfo.exists()) {
        return true;
    }

    QFileInfo targetInfo(targetPath);
    if (targetInfo.exists()) {
        LOG_WARN(LogModule::Core, "Skipping cache migration entry because target already exists: {}", targetPath);
        return false;
    }

    QDir targetParent(targetInfo.absolutePath());
    if (!targetParent.exists() && !targetParent.mkpath(QStringLiteral("."))) {
        LOG_WARN(LogModule::Core, "Failed to create cache migration parent directory: {}", targetInfo.absolutePath());
        return false;
    }

    if (sourceInfo.isDir()) {
        LOG_WARN(LogModule::Core, "Refusing to migrate an unexpected cache directory entry: {}", sourcePath);
        return false;
    }

    if (QFile::rename(sourcePath, targetPath)) {
        return true;
    }

    if (QFile::copy(sourcePath, targetPath)) {
        LOG_WARN(LogModule::Core,
                 "Copied cache entry without removing the source because permanent deletion is disabled: {}",
                 sourcePath);
        return false;
    }

    LOG_WARN(LogModule::Core, "Failed to migrate cache file: {} -> {}", sourcePath, targetPath);
    return false;
}

}  // namespace EasyKiConverter
