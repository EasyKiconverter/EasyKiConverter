#include "TempFileTransactionUtils.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QStandardPaths>

#include <chrono>
#include <thread>

namespace EasyKiConverter::TempFileTransaction {

namespace {

/**
 * @brief 递归复制目录及其中的文件，供移动操作在跨卷时回退使用。
 * @param sourcePath 待处理的 sourcePath。
 * @param targetPath 待处理的 targetPath。
 */
bool copyDirectoryRecursively(const QString& sourcePath, const QString& targetPath) {
    QDir sourceDir(sourcePath);
    if (!sourceDir.exists()) {
        return false;
    }

    if (!QDir().mkpath(targetPath)) {
        return false;
    }

    const QFileInfoList entries =
        sourceDir.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries | QDir::Hidden | QDir::System);
    for (const QFileInfo& entry : entries) {
        const QString sourceEntryPath = entry.absoluteFilePath();  ///< 源目录项的绝对路径。
        const QString targetEntryPath = QDir(targetPath).filePath(entry.fileName());  ///< 目标目录项的绝对路径。
        if (entry.isDir()) {
            if (!copyDirectoryRecursively(sourceEntryPath, targetEntryPath)) {
                return false;
            }
        } else {
            if (QFile::exists(targetEntryPath) && !QFile::remove(targetEntryPath)) {
                return false;
            }
            if (!QFile::copy(sourceEntryPath, targetEntryPath)) {
                return false;
            }
        }
    }

    return true;
}

/**
 * @brief 优先重命名移动文件，失败时重试并回退到复制删除。
 * @param sourcePath 待处理的 sourcePath。
 * @param targetPath 待处理的 targetPath。
 */
bool moveFileWithFallback(const QString& sourcePath, const QString& targetPath) {
    if (!ensureParentDirectory(targetPath)) {
        return false;
    }

    constexpr int kMaxRetries = 3;  ///< 文件重命名移动的最大重试次数。
    for (int attempt = 0; attempt < kMaxRetries; ++attempt) {  // 当前重试序号。
        if (attempt > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50 * attempt));
        }
        if (QFile::rename(sourcePath, targetPath)) {
            return true;
        }
    }

    if (!QFile::copy(sourcePath, targetPath)) {
        return false;
    }

    return QFile::remove(sourcePath);
}

/**
 * @brief 优先重命名移动目录，失败时回退到递归复制删除。
 * @param sourcePath 待处理的 sourcePath。
 * @param targetPath 待处理的 targetPath。
 */
bool moveDirectoryWithFallback(const QString& sourcePath, const QString& targetPath) {
    if (!ensureParentDirectory(targetPath)) {
        return false;
    }

    if (QDir().rename(sourcePath, targetPath)) {
        return true;
    }

    if (!copyDirectoryRecursively(sourcePath, targetPath)) {
        return false;
    }

    return QDir(sourcePath).removeRecursively();
}

}  // namespace

/**
 * @brief 返回应用数据目录下的备份事务根目录。
 */
QString backupRootPath() {
    QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);  ///< 应用数据目录。
    if (appDataPath.isEmpty()) {
        appDataPath = QDir::homePath() + QDir::separator() + QStringLiteral(".easykiconverter");
    }
    return QDir(appDataPath).filePath(QStringLiteral("backups"));
}

/**
 * @brief 创建具有时间和随机后缀的事务标识。
 */
QString createTransactionId() {
    return QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMddTHHmmsszzzZ")) + QStringLiteral("_") +
           QString::number(QRandomGenerator::global()->generate64(), 16);
}

/**
 * @brief 确保目标路径的父目录存在。
 * @param path 目标路径。
 */
bool ensureParentDirectory(const QString& path) {
    const QFileInfo info(path);  ///< 目标路径信息。
    const QDir parentDir = info.absoluteDir();  ///< 目标路径的父目录。
    return parentDir.exists() || QDir().mkpath(info.absolutePath());
}

/**
 * @brief 按路径类型移动对象，并在跨卷或占用时使用复制回退。
 * @param sourcePath 待处理的 sourcePath。
 * @param targetPath 待处理的 targetPath。
 * @param isDirectory 待处理的 isDirectory。
 */
bool movePathWithFallback(const QString& sourcePath, const QString& targetPath, bool isDirectory) {
    return isDirectory ? moveDirectoryWithFallback(sourcePath, targetPath)
                       : moveFileWithFallback(sourcePath, targetPath);
}

/**
 * @brief 按路径类型删除对象，目标不存在时视为成功。
 * @param path 目标路径。
 * @param isDirectory 待处理的 isDirectory。
 */
bool removePath(const QString& path, bool isDirectory) {
    if (path.isEmpty()) {
        return true;
    }
    if (isDirectory) {
        QDir dir(path);
        return !dir.exists() || dir.removeRecursively();
    }
    return !QFile::exists(path) || QFile::remove(path);
}

/**
 * @brief 按路径类型检查对象是否存在。
 * @param path 目标路径。
 * @param isDirectory 待处理的 isDirectory。
 */
bool pathExists(const QString& path, bool isDirectory) {
    return isDirectory ? QDir(path).exists() : QFile::exists(path);
}

/**
 * @brief 写入备份事务清单，记录提交状态和全部路径映射。
 * @param manifestPath 待处理的 manifestPath。
 * @param transactionId 待处理的 transactionId。
 * @param outputRoot 待处理的 outputRoot。
 * @param entries 待写入的条目列表。
 * @param committed 待处理的 committed。
 */
bool writeManifest(const QString& manifestPath,
                   const QString& transactionId,
                   const QString& outputRoot,
                   const QVector<BackupEntry>& entries,
                   bool committed) {
    QJsonObject root;  ///< 事务清单根对象。
    root.insert(QStringLiteral("transactionId"), transactionId);
    root.insert(QStringLiteral("outputRoot"), outputRoot);
    root.insert(QStringLiteral("createdAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    root.insert(QStringLiteral("appVersion"), QCoreApplication::applicationVersion());
    root.insert(QStringLiteral("committed"), committed);

    QJsonArray entryArray;  ///< 事务条目 JSON 数组。
    for (const BackupEntry& entry : entries) {  // 当前备份条目。
        QJsonObject entryObject;  ///< 当前备份条目的 JSON 对象。
        entryObject.insert(QStringLiteral("finalPath"), entry.finalPath);
        entryObject.insert(QStringLiteral("backupPath"), entry.backupPath);
        entryObject.insert(QStringLiteral("tempPath"), entry.tempPath);
        entryObject.insert(QStringLiteral("isDirectory"), entry.isDirectory);
        entryObject.insert(QStringLiteral("existedBefore"), entry.existedBefore);
        entryArray.append(entryObject);
    }
    root.insert(QStringLiteral("entries"), entryArray);

    QFile manifestFile(manifestPath);  ///< 事务清单文件对象。
    if (!manifestFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning() << "TempFileManager: Failed to write manifest:" << manifestPath << manifestFile.errorString();
        return false;
    }
    return manifestFile.write(QJsonDocument(root).toJson(QJsonDocument::Indented)) > 0;
}

/**
 * @brief 读取备份事务清单，并返回提交状态和路径映射。
 * @param manifestPath 待处理的 manifestPath。
 * @param committed 待处理的 committed。
 */
QVector<BackupEntry> readManifestEntries(const QString& manifestPath, bool* committed) {
    QVector<BackupEntry> entries;  ///< 从事务清单解析出的备份条目。
    QFile manifestFile(manifestPath);  ///< 事务清单文件对象。
    if (!manifestFile.open(QIODevice::ReadOnly)) {
        qWarning() << "TempFileManager: Failed to read manifest:" << manifestPath << manifestFile.errorString();
        return entries;
    }

    const QJsonDocument document = QJsonDocument::fromJson(manifestFile.readAll());  ///< 解析后的事务清单文档。
    const QJsonObject root = document.object();  ///< 事务清单根对象。
    if (committed != nullptr) {
        *committed = root.value(QStringLiteral("committed")).toBool(false);
    }

    const QJsonArray entryArray = root.value(QStringLiteral("entries")).toArray();  ///< 事务条目 JSON 数组。
    for (const QJsonValue& value : entryArray) {  // 当前事务条目值。
        const QJsonObject object = value.toObject();  ///< 当前事务条目对象。
        BackupEntry entry;  ///< 当前解析出的备份条目。
        entry.finalPath = object.value(QStringLiteral("finalPath")).toString();
        entry.backupPath = object.value(QStringLiteral("backupPath")).toString();
        entry.tempPath = object.value(QStringLiteral("tempPath")).toString();
        entry.isDirectory = object.value(QStringLiteral("isDirectory")).toBool(false);
        entry.existedBefore = object.value(QStringLiteral("existedBefore")).toBool(false);
        entries.append(entry);
    }
    return entries;
}

}  // namespace EasyKiConverter::TempFileTransaction
