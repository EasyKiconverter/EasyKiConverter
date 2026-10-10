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
    QString sourcePath;  ///< 待迁移条目的源路径。
    QString targetPath;  ///< 待迁移条目的目标路径。
};

struct MetadataSnapshot {
    QString path;  ///< 旧缓存元数据文件路径。
    QByteArray contents;  ///< 旧缓存元数据原始内容。
};

/**
 * @brief 返回缓存组件目录允许迁移的已知文件名。
 * @return 已知缓存文件名列表。
 */
const QStringList& knownComponentFiles() {
    static const QStringList files = {// 当前版本支持迁移的组件缓存文件名。
                                      QStringLiteral("component.json"),
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

/**
 * @brief 判断旧版三维模型文件是否可以在没有目录标记时迁移。
 * @param info 待检查的文件信息。
 * @return 文件是普通 OBJ、STEP 或 WRL 模型时返回 true。
 */
bool isLegacyModelFile(const QFileInfo& info) {
    if (!info.isFile() || info.isSymLink())
        return false;
    const QString suffix = info.suffix();  // 模型文件扩展名。
    return suffix.compare(QStringLiteral("obj"), Qt::CaseInsensitive) == 0 ||
           suffix.compare(QStringLiteral("step"), Qt::CaseInsensitive) == 0 ||
           suffix.compare(QStringLiteral("wrl"), Qt::CaseInsensitive) == 0;
}

/**
 * @brief 将旧版本元数据升级为当前缓存所有权格式。
 * @param path 元数据文件路径。
 * @return 元数据读取、更新和原子写入均成功时返回 true。
 */
bool upgradeMetadata(const QString& path) {
    QJsonObject metadata = CacheMetadataStore::read(path);  // 已读取的缓存元数据。
    if (metadata.isEmpty())
        return false;
    metadata[QStringLiteral("cacheOwner")] = QStringLiteral("EasyKiConverter");
    metadata[QStringLiteral("cacheEntryVersion")] = 1;
    return CacheMetadataStore::writeAtomically(path, QJsonDocument(metadata).toJson(QJsonDocument::Indented));
}

/**
 * @brief 保存旧缓存元数据快照，供迁移失败时恢复。
 * @param paths 待保存的元数据文件路径列表。
 * @param snapshots 输出的元数据快照列表。
 * @param error 输出快照读取错误信息。
 * @return 全部元数据读取成功时返回 true。
 */
bool snapshotMetadata(const QStringList& paths, QList<MetadataSnapshot>* snapshots, QString* error) {
    for (const QString& path : paths) {
        QFile file(path);  // 当前待快照的元数据文件。
        if (!file.open(QIODevice::ReadOnly)) {
            if (error)
                *error = QStringLiteral("无法读取旧缓存元数据：%1").arg(path);
            return false;
        }
        snapshots->append({path, file.readAll()});
    }
    return true;
}

/**
 * @brief 原子恢复旧缓存元数据快照。
 * @param snapshots 待恢复的元数据快照列表。
 * @return 全部快照恢复成功时返回 true。
 */
bool restoreMetadata(const QList<MetadataSnapshot>& snapshots) {
    bool restored = true;  // 是否全部元数据恢复成功。
    for (const MetadataSnapshot& snapshot : snapshots) {
        if (!CacheMetadataStore::writeAtomically(snapshot.path, snapshot.contents)) {
            LOG_ERROR(LogModule::Core, "Failed to restore cache metadata after migration failure: {}", snapshot.path);
            restored = false;
        }
    }
    return restored;
}

/**
 * @brief 在迁移开始前收集组件文件并拒绝未知内容。
 * @param sourceDir 组件缓存源目录。
 * @param targetDir 组件缓存目标目录。
 * @param entries 输出迁移条目列表。
 * @param error 输出错误信息。
 * @return 组件目录内容全部可验证且没有目标冲突时返回 true。
 */
bool collectComponentEntries(const QString& sourceDir,
                             const QString& targetDir,
                             QList<MigrationEntry>* entries,
                             QString* error) {
    const QDir source(sourceDir);  // 组件缓存源目录对象。
    const QFileInfoList sourceEntries =  // 源目录中的实际条目。
        source.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot);
    for (const QFileInfo& entry : sourceEntries) {
        if (!entry.isFile() || entry.isSymLink() || !knownComponentFiles().contains(entry.fileName())) {
            if (error)
                *error = QStringLiteral("缓存迁移遇到无法验证的组件内容：%1").arg(entry.absoluteFilePath());
            return false;
        }
        const QString targetPath = QDir(targetDir).filePath(entry.fileName());  // 当前条目的目标路径。
        if (QFileInfo::exists(targetPath)) {
            if (error)
                *error = QStringLiteral("缓存迁移目标已存在同名文件：%1").arg(targetPath);
            return false;
        }
        entries->append({entry.absoluteFilePath(), targetPath});
    }
    return true;
}

/**
 * @brief 在迁移开始前检查三维模型目标，避免移动后才发现冲突。
 * @param sourceFiles 待迁移的模型文件路径列表。
 * @param targetRoot 缓存目标根目录。
 * @param entries 输出迁移条目列表。
 * @param error 输出错误信息。
 * @return 所有模型目标均可用时返回 true。
 */
bool collectModelEntries(const QStringList& sourceFiles,
                         const QString& targetRoot,
                         QList<MigrationEntry>* entries,
                         QString* error) {
    for (const QString& sourcePath : sourceFiles) {
        const QString targetPath =  // 当前模型文件的目标路径。
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

/**
 * @brief 执行已完成预检的迁移计划并在失败时逆序回滚。
 * @param entries 已完成冲突检查的迁移条目列表。
 * @param error 输出迁移错误信息。
 * @return 所有条目均移动成功时返回 true。
 */
bool moveEntriesTransactional(const QList<MigrationEntry>& entries, QString* error) {
    QList<MigrationEntry> moved;  // 已成功移动、失败时需要回滚的条目。
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
        QDir targetParent(QFileInfo(entry.targetPath).absolutePath());  // 当前目标文件的父目录。
        if (!targetParent.exists() && !targetParent.mkpath(QStringLiteral("."))) {
            LOG_WARN(LogModule::Core, "Failed to create cache migration parent directory: {}", targetParent.path());
            if (error)
                *error = QStringLiteral("无法创建缓存迁移目录：%1").arg(targetParent.path());
            rollback();
            return false;
        }
        if (!QFile::rename(entry.sourcePath, entry.targetPath)) {
            LOG_WARN(LogModule::Core, "Failed to migrate cache file: {} -> {}", entry.sourcePath, entry.targetPath);
            if (error)
                *error = QStringLiteral("无法迁移缓存文件：%1").arg(entry.sourcePath);
            rollback();
            return false;
        }
        moved.append(entry);
    }
    return true;
}

}  // namespace

/**
 * @brief 按预检结果事务性迁移旧缓存并在失败时保留源目录。
 * @param oldCacheDir 旧缓存根目录。
 * @param newCacheDir 新缓存根目录。
 * @param error 输出迁移错误信息。
 * @return 迁移成功或无需迁移时返回 true。
 */
bool CacheDirectoryMigrator::migrate(const QString& oldCacheDir, const QString& newCacheDir, QString* error) {
    if (oldCacheDir.isEmpty() || newCacheDir.isEmpty() || oldCacheDir == newCacheDir) {
        return true;
    }

    QDir source(oldCacheDir);
    if (!source.exists()) {
        return true;
    }
    const bool legacySource = !CacheSafety::isOwnedRoot(oldCacheDir);  // 源目录是否为旧版未标记缓存。
    if (legacySource && !CacheSafety::canAdoptLegacyRoot(oldCacheDir) &&
        !source.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot).isEmpty()) {
        if (error)
            *error = QStringLiteral("旧缓存目录包含无法验证归属的数据，源目录已保留：%1").arg(oldCacheDir);
        LOG_WARN(
            LogModule::Core, "Skipped legacy cache migration because source ownership is ambiguous: {}", oldCacheDir);
        return false;
    }
    if (!CacheSafety::isOwnedRoot(newCacheDir)) {
        if (error)
            *error = QStringLiteral("缓存迁移目标没有有效所有权标记：%1").arg(newCacheDir);
        LOG_WARN(
            LogModule::Core, "Skipped cache migration because target ownership cannot be verified: {}", newCacheDir);
        return false;
    }

    QDir target;  // 缓存迁移目标目录对象。
    if (!target.exists(newCacheDir) && !target.mkpath(newCacheDir)) {
        if (error)
            *error = QStringLiteral("无法创建缓存迁移目标目录：%1").arg(newCacheDir);
        LOG_WARN(LogModule::Core, "Failed to create cache migration target directory: {}", newCacheDir);
        return false;
    }

    QList<MigrationEntry> migrationEntries;  // 预检通过后待执行的全部迁移条目。
    const QStringList componentDirectories =  // 待迁移的组件缓存目录列表。
        legacySource ? CacheSafety::legacyComponentDirectories(oldCacheDir)
                     : CacheSafety::ownedComponentDirectories(oldCacheDir);
    for (const QString& sourcePath : componentDirectories) {
        const QString targetPath =  // 当前组件目录的迁移目标路径。
            QDir(newCacheDir).filePath(QFileInfo(sourcePath).fileName());
        if (QDir(targetPath).exists() && !CacheSafety::isOwnedComponentDirectory(newCacheDir, targetPath) &&
            !QDir(targetPath).entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot).isEmpty()) {
            if (error)
                *error = QStringLiteral("缓存迁移目标包含未托管目录：%1").arg(targetPath);
            LOG_WARN(LogModule::Core, "Refusing cache migration into an unowned target directory: {}", targetPath);
            return false;
        }
        QString componentError;  // 当前组件内容预检错误。
        if (!collectComponentEntries(sourcePath, targetPath, &migrationEntries, &componentError)) {
            if (error)
                *error = componentError;
            LOG_WARN(LogModule::Core,
                     "Refusing cache migration because source contains unknown data: {} ({})",
                     sourcePath,
                     componentError);
            return false;
        }
    }
    QStringList modelFiles = CacheSafety::ownedModel3DFiles(oldCacheDir);  // 已验证的三维模型文件列表。
    if (legacySource) {
        const QDir modelRoot(QDir(oldCacheDir).filePath(QStringLiteral("model3d")));
        for (const QFileInfo& info : modelRoot.entryInfoList(QDir::Files | QDir::NoDotAndDotDot, QDir::Name)) {
            if (isLegacyModelFile(info))
                modelFiles.append(info.absoluteFilePath());
        }
    }
    QString preflightError;  // 三维模型目标预检错误。
    if (!collectModelEntries(modelFiles, newCacheDir, &migrationEntries, &preflightError)) {
        if (error)
            *error = preflightError;
        LOG_WARN(LogModule::Core, "{}", preflightError);
        return false;
    }

    QList<MetadataSnapshot> metadataSnapshots;  // 旧版元数据快照，用于事务失败回滚。
    if (legacySource) {
        QStringList metadataPaths;  // 待升级的旧版元数据路径列表。
        for (const QString& sourcePath : componentDirectories) {
            metadataPaths.append(QDir(sourcePath).filePath(QStringLiteral("component.json")));
        }
        if (!snapshotMetadata(metadataPaths, &metadataSnapshots, error))
            return false;
        for (const QString& metadataPath : metadataPaths) {
            if (!upgradeMetadata(metadataPath)) {
                if (error)
                    *error = QStringLiteral("无法升级旧缓存元数据：%1").arg(metadataPath);
                LOG_WARN(LogModule::Core, "Refusing cache migration because metadata is invalid: {}", metadataPath);
                restoreMetadata(metadataSnapshots);
                return false;
            }
        }
    }
    if (!moveEntriesTransactional(migrationEntries, error)) {
        restoreMetadata(metadataSnapshots);
        return false;
    }

    for (const QString& sourcePath : componentDirectories) {
        if (QDir(sourcePath).entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot).isEmpty())
            QDir().rmdir(sourcePath);
    }
    LOG_DEBUG(LogModule::Core, "Migrated owned cache entries from {} to {}", oldCacheDir, newCacheDir);
    return true;
}

/**
 * @brief 迁移组件目录内容并验证源、目标所有权。
 * @param sourceRoot 源缓存根目录。
 * @param sourceDir 源组件目录。
 * @param targetRoot 目标缓存根目录。
 * @param targetDir 目标组件目录。
 * @param legacySource 源目录是否为旧版本缓存。
 * @return 目录内容迁移成功时返回 true。
 */
bool CacheDirectoryMigrator::moveDirectoryContents(const QString& sourceRoot,
                                                   const QString& sourceDir,
                                                   const QString& targetRoot,
                                                   const QString& targetDir,
                                                   bool legacySource) {
    QDir source(sourceDir);  // 源组件目录对象。
    if (!source.exists()) {
        return true;
    }

    QDir target;  // 目标组件目录对象。
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

    QList<MigrationEntry> entries;  // 当前组件目录的待迁移条目。
    if (!collectComponentEntries(sourceDir, targetDir, &entries, nullptr))
        return false;
    const QString metadataPath = QDir(sourceDir).filePath(QStringLiteral("component.json"));  // 当前组件元数据路径。
    QFile metadataFile(metadataPath);  // 当前组件元数据文件。
    if (!metadataFile.open(QIODevice::ReadOnly))
        return false;
    const MetadataSnapshot metadataSnapshot{metadataPath, metadataFile.readAll()};  // 当前组件元数据快照。
    if (!upgradeMetadata(metadataPath))
        return false;
    if (!moveEntriesTransactional(entries, nullptr)) {
        restoreMetadata({metadataSnapshot});
        return false;
    }
    const QString targetMetadataPath =  // 迁移后目标目录中的元数据路径。
        QDir(targetDir).filePath(QStringLiteral("component.json"));
    return QFileInfo::exists(targetMetadataPath);
}

/**
 * @brief 迁移单个缓存条目并拒绝覆盖已有目标。
 * @param sourcePath 缓存条目源路径。
 * @param targetPath 缓存条目目标路径。
 * @return 条目不存在或移动成功时返回 true。
 */
bool CacheDirectoryMigrator::moveCacheEntry(const QString& sourcePath, const QString& targetPath) {
    QFileInfo sourceInfo(sourcePath);  // 源条目文件信息。
    if (!sourceInfo.exists()) {
        return true;
    }

    QFileInfo targetInfo(targetPath);  // 目标条目文件信息。
    if (targetInfo.exists()) {
        LOG_WARN(LogModule::Core, "Skipping cache migration entry because target already exists: {}", targetPath);
        return false;
    }

    QDir targetParent(targetInfo.absolutePath());  // 目标条目父目录。
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
