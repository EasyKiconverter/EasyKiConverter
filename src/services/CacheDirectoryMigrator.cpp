#include "CacheDirectoryMigrator.h"

#include "CacheSafety.h"
#include "utils/logging/LogMacros.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace EasyKiConverter {

bool CacheDirectoryMigrator::migrate(const QString& oldCacheDir, const QString& newCacheDir) {
    if (oldCacheDir.isEmpty() || newCacheDir.isEmpty() || oldCacheDir == newCacheDir) {
        return true;
    }

    QDir source(oldCacheDir);
    if (!source.exists()) {
        return true;
    }
    if (!CacheSafety::isOwnedRoot(oldCacheDir) || !CacheSafety::isOwnedRoot(newCacheDir)) {
        LOG_WARN(LogModule::Core,
                 "Skipped cache migration because ownership cannot be verified: {} -> {}",
                 oldCacheDir,
                 newCacheDir);
        return false;
    }

    QDir target;
    if (!target.exists(newCacheDir) && !target.mkpath(newCacheDir)) {
        LOG_WARN(LogModule::Core, "Failed to create cache migration target directory: {}", newCacheDir);
        return false;
    }

    bool moved = true;
    for (const QString& sourcePath : CacheSafety::ownedComponentDirectories(oldCacheDir)) {
        const QString targetPath = QDir(newCacheDir).filePath(QFileInfo(sourcePath).fileName());
        if (!moveDirectoryContents(sourcePath, targetPath))
            moved = false;
    }
    for (const QString& sourcePath : CacheSafety::ownedModel3DFiles(oldCacheDir)) {
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

bool CacheDirectoryMigrator::moveDirectoryContents(const QString& sourceDir, const QString& targetDir) {
    QDir source(sourceDir);
    if (!source.exists()) {
        return true;
    }

    QDir target;
    if (!target.exists(targetDir) && !target.mkpath(targetDir)) {
        LOG_WARN(LogModule::Core, "Failed to create cache migration directory: {}", targetDir);
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
        if (entryInfo.isSymLink() || !knownFiles.contains(entryInfo.fileName())) {
            allMoved = false;
            continue;
        }
        const QString sourcePath = entryInfo.absoluteFilePath();
        const QString targetPath = QDir(targetDir).filePath(entryInfo.fileName());
        if (!moveCacheEntry(sourcePath, targetPath)) {
            allMoved = false;
        }
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
        if (sourceInfo.isDir() && targetInfo.isDir()) {
            const bool moved = moveDirectoryContents(sourcePath, targetPath);
            QDir sourceDir(sourcePath);
            if (sourceDir.entryList(QDir::AllEntries | QDir::NoDotAndDotDot).isEmpty()) {
                sourceDir.rmdir(sourcePath);
            }
            return moved;
        }

        LOG_WARN(LogModule::Core, "Skipping cache migration entry because target already exists: {}", targetPath);
        return false;
    }

    QDir targetParent(targetInfo.absolutePath());
    if (!targetParent.exists() && !targetParent.mkpath(QStringLiteral("."))) {
        LOG_WARN(LogModule::Core, "Failed to create cache migration parent directory: {}", targetInfo.absolutePath());
        return false;
    }

    if (sourceInfo.isDir())
        return moveDirectoryContents(sourcePath, targetPath);

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
