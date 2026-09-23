#include "CacheDirectoryMigrator.h"

#include "CacheMetadataStore.h"
#include "CacheSafety.h"
#include "utils/logging/LogMacros.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QList>

namespace EasyKiConverter {

namespace {

struct MigrationEntry {
    QString sourcePath;
    QString targetPath;
};

const QStringList& knownComponentFiles() {
    static const QStringList files = {QStringLiteral("component.json"),
                                      QStringLiteral("symbol.json"),
                                      QStringLiteral("footprint.json"),
                                      QStringLiteral("cad_data.json"),
                                      QStringLiteral("datasheet"),
                                      QStringLiteral("datasheet.pdf"),
                                      QStringLiteral("datasheet.html"),
                                      QStringLiteral("preview_0.jpg"),
                                      QStringLiteral("preview_1.jpg"),
                                      QStringLiteral("preview_2.jpg")};
    return files;
}

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

// 在迁移开始前收集组件文件并拒绝全部已知条件之外的内容。
bool collectComponentEntries(const QString& sourceDir,
                             const QString& targetDir,
                             QList<MigrationEntry>* entries,
                             QString* error) {
    const QDir source(sourceDir);
    const QFileInfoList sourceEntries = source.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot);
    for (const QFileInfo& entry : sourceEntries) {
        if (!entry.isFile() || entry.isSymLink() || !knownComponentFiles().contains(entry.fileName())) {
            if (error)
                *error = QStringLiteral("缓存迁移遇到无法验证的组件内容：%1").arg(entry.absoluteFilePath());
            return false;
        }
        const QString targetPath = QDir(targetDir).filePath(entry.fileName());
        if (QFileInfo::exists(targetPath)) {
            if (error)
                *error = QStringLiteral("缓存迁移目标已存在同名文件：%1").arg(targetPath);
            return false;
        }
        entries->append({entry.absoluteFilePath(), targetPath});
    }
    return true;
}

// 在迁移开始前检查模型文件目标，避免组件已经移动后才发现冲突。
bool collectModelEntries(const QStringList& sourceFiles,
                         const QString& targetRoot,
                         QList<MigrationEntry>* entries,
                         QString* error) {
    for (const QString& sourcePath : sourceFiles) {
        const QString targetPath =
            QDir(targetRoot).filePath(QStringLiteral("model3d")) + QDir::separator() + QFileInfo(sourcePath).fileName();
        if (QFileInfo::exists(targetPath)) {
            if (error)
                *error = QStringLiteral("缓存迁移目标已存在同名三维模型：%1").arg(targetPath);
            return false;
        }
        entries->append({sourcePath, targetPath});
    }
    return true;
}

// 执行已完成预检的迁移计划，并在任一重命名失败时逆序回滚。
bool moveEntriesTransactional(const QList<MigrationEntry>& entries) {
    QList<MigrationEntry> moved;
    const auto rollback = [&moved]() {
        for (auto it = moved.crbegin(); it != moved.crend(); ++it) {
            if (!QFile::rename(it->targetPath, it->sourcePath)) {
                LOG_ERROR(LogModule::Core,
                          "Failed to roll back cache migration entry: {} -> {}",
                          it->targetPath,
                          it->sourcePath);
            }
        }
    };
    for (const MigrationEntry& entry : entries) {
        QDir targetParent(QFileInfo(entry.targetPath).absolutePath());
        if (!targetParent.exists() && !targetParent.mkpath(QStringLiteral("."))) {
            LOG_WARN(LogModule::Core, "Failed to create cache migration parent directory: {}", targetParent.path());
            rollback();
            return false;
        }
        if (!QFile::rename(entry.sourcePath, entry.targetPath)) {
            LOG_WARN(LogModule::Core, "Failed to migrate cache file: {} -> {}", entry.sourcePath, entry.targetPath);
            rollback();
            return false;
        }
        moved.append(entry);
    }
    return true;
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

    QList<MigrationEntry> migrationEntries;
    const QStringList componentDirectories = legacySource ? CacheSafety::legacyComponentDirectories(oldCacheDir)
                                                          : CacheSafety::ownedComponentDirectories(oldCacheDir);
    for (const QString& sourcePath : componentDirectories) {
        const QString targetPath = QDir(newCacheDir).filePath(QFileInfo(sourcePath).fileName());
        if (QDir(targetPath).exists() && !CacheSafety::isOwnedComponentDirectory(newCacheDir, targetPath) &&
            !QDir(targetPath).entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot).isEmpty()) {
            LOG_WARN(LogModule::Core, "Refusing cache migration into an unowned target directory: {}", targetPath);
            return false;
        }
        if (!collectComponentEntries(sourcePath, targetPath, &migrationEntries, nullptr)) {
            LOG_WARN(LogModule::Core, "Refusing cache migration because source contains unknown data: {}", sourcePath);
            return false;
        }
    }
    QStringList modelFiles = CacheSafety::ownedModel3DFiles(oldCacheDir);
    if (legacySource) {
        const QDir modelRoot(QDir(oldCacheDir).filePath(QStringLiteral("model3d")));
        for (const QFileInfo& info : modelRoot.entryInfoList(QDir::Files | QDir::NoDotAndDotDot, QDir::Name)) {
            if (isLegacyModelFile(info))
                modelFiles.append(info.absoluteFilePath());
        }
    }
    QString preflightError;
    if (!collectModelEntries(modelFiles, newCacheDir, &migrationEntries, &preflightError)) {
        LOG_WARN(LogModule::Core, "{}", preflightError);
        return false;
    }

    for (const QString& sourcePath : componentDirectories) {
        const QString metadataPath = QDir(sourcePath).filePath(QStringLiteral("component.json"));
        if (!upgradeMetadata(metadataPath)) {
            LOG_WARN(LogModule::Core, "Refusing cache migration because metadata is invalid: {}", metadataPath);
            return false;
        }
    }
    if (!moveEntriesTransactional(migrationEntries))
        return false;

    for (const QString& sourcePath : componentDirectories) {
        if (QDir(sourcePath).entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot).isEmpty())
            QDir().rmdir(sourcePath);
    }
    LOG_DEBUG(LogModule::Core, "Migrated owned cache entries from {} to {}", oldCacheDir, newCacheDir);
    return true;
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

    QList<MigrationEntry> entries;
    if (!collectComponentEntries(sourceDir, targetDir, &entries, nullptr))
        return false;
    if (!upgradeMetadata(QDir(sourceDir).filePath(QStringLiteral("component.json"))))
        return false;
    if (!moveEntriesTransactional(entries))
        return false;
    const QString metadataPath = QDir(targetDir).filePath(QStringLiteral("component.json"));
    return QFileInfo::exists(metadataPath);
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

    if (QFile::rename(sourcePath, targetPath))
        return true;
    LOG_WARN(LogModule::Core, "Failed to migrate cache file: {} -> {}", sourcePath, targetPath);
    return false;
}

}  // namespace EasyKiConverter
