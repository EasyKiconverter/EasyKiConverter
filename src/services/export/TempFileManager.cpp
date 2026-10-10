#include "TempFileManager.h"

#include "TempFileTransactionUtils.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QHash>
#include <QRandomGenerator>
#include <QStandardPaths>
#include <QThread>

namespace EasyKiConverter {

namespace {

constexpr int kDefaultBackupRetentionDays = 3;  ///< 临时文件备份默认保留天数。
constexpr int kDefaultMaxBackupSets = 3;  ///< 临时文件备份默认保留组数。

QMutex g_tempDirectoryUsersMutex;  ///< 保护共享临时目录使用者计数的互斥量。
QHash<QString, int> g_tempDirectoryUsers;  ///< 共享临时目录规范路径到使用者数量的映射。

/**
 * @brief 记录共享临时目录的使用者数量。
 * @param path 共享临时目录路径。
 */
void registerTempDirectoryUser(const QString& path) {
    if (path.isEmpty())
        return;
    QMutexLocker locker(&g_tempDirectoryUsersMutex);  ///< 保护共享目录引用计数的锁。
    ++g_tempDirectoryUsers[path];
}

/**
 * @brief 释放共享临时目录的一个使用者引用。
 * @param path 共享临时目录路径。
 */
void unregisterTempDirectoryUser(const QString& path) {
    if (path.isEmpty())
        return;
    QMutexLocker locker(&g_tempDirectoryUsersMutex);  ///< 保护共享目录引用计数的锁。
    auto it = g_tempDirectoryUsers.find(path);  ///< 当前路径的引用计数迭代器。
    if (it == g_tempDirectoryUsers.end())
        return;
    if (--it.value() <= 0)
        g_tempDirectoryUsers.erase(it);
}

/**
 * @brief 判断临时目录是否仍有任意阶段在使用。
 * @param path 共享临时目录路径。
 * @return 如果仍有使用者则返回 true。
 */
bool hasTempDirectoryUsers(const QString& path) {
    QMutexLocker locker(&g_tempDirectoryUsersMutex);  ///< 保护共享目录引用计数的锁。
    return g_tempDirectoryUsers.value(path, 0) > 0;
}

/**
 * @brief 判断临时目录是否由当前阶段之外的阶段使用。
 * @param path 共享临时目录路径。
 * @return 如果存在其他阶段使用者则返回 true。
 */
bool hasOtherTempDirectoryUsers(const QString& path) {
    QMutexLocker locker(&g_tempDirectoryUsersMutex);  ///< 保护共享目录引用计数的锁。
    return g_tempDirectoryUsers.value(path, 0) > 1;
}

using TempFileTransaction::BackupEntry;
using TempFileTransaction::backupRootPath;
using TempFileTransaction::createTransactionId;
using TempFileTransaction::ensureParentDirectory;
using TempFileTransaction::movePathWithFallback;
using TempFileTransaction::pathExists;
using TempFileTransaction::readManifestEntries;
using TempFileTransaction::removePath;
using TempFileTransaction::writeManifest;

}  // namespace

/**
 * @brief 初始化临时文件管理器。
 */
TempFileManager::TempFileManager(QObject* parent) : QObject(parent) {}

TempFileManager::~TempFileManager() {
    /**
     * @brief 析构时不调用 rollbackAll()，避免删除其他 Stage 共享的 .tmp 目录
     * @brief 只清理本实例注册的临时文件（如果还存在的话）
     */
    QMutexLocker locker(&m_mutex);  ///< 保护本实例临时文件集合的锁。
    for (const QString& tempPath : m_tempFiles) {  ///< 当前待清理的临时路径。
        if (QFileInfo(tempPath).isDir()) {
            QDir(tempPath).removeRecursively();
        } else {
            QFile::remove(tempPath);
        }
    }
    m_tempFiles.clear();
    const QString releasedTempDirectory = m_registeredTempDirectory;  ///< 本实例即将释放的共享临时目录。
    unregisterTempDirectoryUser(releasedTempDirectory);
    m_registeredTempDirectory.clear();
    if (!releasedTempDirectory.isEmpty() && !hasTempDirectoryUsers(releasedTempDirectory)) {
        QDir tempDir(releasedTempDirectory);  ///< 已释放共享目录的目录对象。
        if (tempDir.exists() && tempDir.isEmpty())
            QDir().rmdir(releasedTempDirectory);
    }
}

/**
 * @brief 设置导出输出目录并更新共享临时目录引用。
 * @param outputPath 输出路径。
 */
void TempFileManager::setOutputPath(const QString& outputPath) {
    QMutexLocker locker(&m_mutex);  ///< 保护输出目录和共享引用状态的锁。
    const QString newTempDirectory =
        outputPath.isEmpty() ? QString() : QDir(outputPath).filePath(m_tempDirName);  ///< 新的共享临时目录。
    if (newTempDirectory != m_registeredTempDirectory) {
        unregisterTempDirectoryUser(m_registeredTempDirectory);
        registerTempDirectoryUser(newTempDirectory);
        m_registeredTempDirectory = newTempDirectory;
    }
    m_outputPath = outputPath;
}

/**
 * @brief 返回当前输出目录下的共享临时目录。
 */
QString TempFileManager::tempDirectory() const {
    if (m_outputPath.isEmpty()) {
        return QString();
    }
    /**
     * @brief 使用与 setOutputPath() 注册引用时相同的路径构造方式，避免 Windows
     * @brief 下不同路径分隔符导致共享临时目录引用计数无法匹配。
     * @param m_tempDirName 待处理的 m_tempDirName。
     */
    return QDir(m_outputPath).filePath(m_tempDirName);
}

/**
 * @brief 创建并登记单个元器件的临时文件路径。
 * @param componentId 元器件编号。
 * @param suffix 待处理的 suffix。
 */
QString TempFileManager::createTempFilePath(const QString& componentId, const QString& suffix) {
    QMutexLocker locker(&m_mutex);  ///< 保护临时文件集合的锁。

    if (!ensureTempDirectory()) {
        return QString();
    }

    QString tempName = generateUniqueTempName(componentId, suffix);  ///< 生成的临时文件名。
    QString tempPath = tempDirectory() + QDir::separator() + tempName;  ///< 临时文件完整路径。

    m_tempFiles.insert(tempPath);
    qDebug() << "TempFileManager: Created temp path:" << tempPath << "for component:" << componentId;

    return tempPath;
}

/**
 * @brief 根据最终文件名查找已登记的临时文件路径。
 * @param finalPath 待处理的 finalPath。
 */
QString TempFileManager::tempFilePath(const QString& finalPath) const {
    QMutexLocker locker(&m_mutex);  ///< 保护临时文件集合的锁。

    if (finalPath.isEmpty() || m_outputPath.isEmpty()) {
        return QString();
    }

    QString fileName = QFileInfo(finalPath).fileName();  ///< 最终路径中的文件名。

    for (const QString& tempPath : m_tempFiles) {  ///< 当前已登记的临时路径。
        if (tempPath.endsWith(fileName)) {
            return tempPath;
        }
    }

    return tempDirectory() + QDir::separator() + fileName;
}

/**
 * @brief 创建并登记符号库文件的临时路径。
 * @param libName 目标库名称。
 * @param suffix 待处理的 suffix。
 */
QString TempFileManager::createSymbolTempPath(const QString& libName, const QString& suffix) {
    QMutexLocker locker(&m_mutex);  ///< 保护临时文件集合的锁。

    if (!ensureTempDirectory()) {
        return QString();
    }

    QString tempName = libName + suffix;  ///< 符号库临时文件名。
    const QString tempDir = tempDirectory();  ///< 共享临时目录路径。
    QString tempPath = tempDir + QDir::separator() + tempName;  ///< 符号库临时文件完整路径。

    m_tempFiles.insert(tempPath);
    qDebug() << "TempFileManager: Created symbol temp path:" << tempPath;

    return tempPath;
}

/**
 * @brief 创建并登记目录型导出结果的临时路径。
 * @param dirName 待处理的 dirName。
 */
QString TempFileManager::createTempDirectoryPath(const QString& dirName) {
    QMutexLocker locker(&m_mutex);  ///< 保护临时文件集合的锁。

    if (!ensureTempDirectory()) {
        return QString();
    }

    QString tempPath = tempDirectory() + QDir::separator() + dirName;  ///< 目录型导出的临时路径。

    m_tempFiles.insert(tempPath);
    qDebug() << "TempFileManager: Created temp directory path:" << tempPath;

    return tempPath;
}

/**
 * @brief 将匹配的临时文件提交到最终文件路径。
 * @param finalPath 待处理的 finalPath。
 */
bool TempFileManager::commit(const QString& finalPath) {
    QMutexLocker locker(&m_mutex);  ///< 保护临时事务状态的锁。

    if (finalPath.isEmpty()) {
        qWarning() << "TempFileManager: Cannot commit empty final path";
        return false;
    }

    QString tempPath;  ///< 与最终文件匹配的临时路径。
    for (const QString& t : m_tempFiles) {  ///< 当前已登记的临时路径。
        if (t.endsWith(QFileInfo(finalPath).fileName())) {
            tempPath = t;
            break;
        }
    }

    if (tempPath.isEmpty()) {
        qWarning() << "TempFileManager: No temp file found for:" << finalPath;
        return false;
    }

    if (!QFile::exists(tempPath)) {
        qWarning() << "TempFileManager: Temp file does not exist:" << tempPath;
        m_tempFiles.remove(tempPath);
        return false;
    }

    const bool committed = commitBatchLocked({CommitItem{tempPath, finalPath, false}});  ///< 批量提交结果。

    locker.unlock();
    emit commitCompleted(finalPath, committed);

    return committed;
}

/**
 * @brief 将匹配的临时目录提交到最终目录路径。
 * @param finalDirPath 待处理的 finalDirPath。
 */
bool TempFileManager::commitDirectory(const QString& finalDirPath) {
    QMutexLocker locker(&m_mutex);  ///< 保护临时事务状态的锁。

    if (finalDirPath.isEmpty()) {
        qWarning() << "TempFileManager: Cannot commit empty final directory path";
        return false;
    }

    QString tempPath;  ///< 与最终目录匹配的临时路径。
    QString dirName = QFileInfo(finalDirPath).fileName();  ///< 最终目录名称。
    for (const QString& t : m_tempFiles) {  ///< 当前已登记的临时路径。
        if (t.endsWith(dirName)) {
            tempPath = t;
            break;
        }
    }

    if (tempPath.isEmpty()) {
        qWarning() << "TempFileManager: No temp directory found for:" << finalDirPath;
        return false;
    }

    if (!QDir(tempPath).exists()) {
        qWarning() << "TempFileManager: Temp directory does not exist:" << tempPath;
        m_tempFiles.remove(tempPath);
        return false;
    }

    const bool committed = commitBatchLocked({CommitItem{tempPath, finalDirPath, true}});  ///< 批量提交结果。

    locker.unlock();
    emit commitCompleted(finalDirPath, committed);

    return committed;
}

/**
 * @brief 带备份地提交单个临时文件。
 * @param tempPath 待处理的 tempPath。
 * @param finalPath 待处理的 finalPath。
 */
bool TempFileManager::commitWithBackup(const QString& tempPath, const QString& finalPath) {
    QMutexLocker locker(&m_mutex);  ///< 保护临时事务状态的锁。
    const bool committed = commitBatchLocked({CommitItem{tempPath, finalPath, false}});  ///< 批量提交结果。

    locker.unlock();
    emit commitCompleted(finalPath, committed);

    return committed;
}

/**
 * @brief 带备份地提交单个临时目录。
 * @param tempDirPath 待处理的 tempDirPath。
 * @param finalDirPath 待处理的 finalDirPath。
 */
bool TempFileManager::commitDirectoryWithBackup(const QString& tempDirPath, const QString& finalDirPath) {
    QMutexLocker locker(&m_mutex);  ///< 保护临时事务状态的锁。
    const bool committed = commitBatchLocked({CommitItem{tempDirPath, finalDirPath, true}});  ///< 批量提交结果。

    locker.unlock();
    emit commitCompleted(finalDirPath, committed);

    return committed;
}

/**
 * @brief 原子提交一组文件或目录，并为每项发出提交结果信号。
 * @param items 待处理的 items。
 */
bool TempFileManager::commitBatch(const QVector<CommitItem>& items) {
    QMutexLocker locker(&m_mutex);  ///< 保护临时事务状态的锁。
    const bool committed = commitBatchLocked(items);  ///< 批量提交结果。

    locker.unlock();
    for (const CommitItem& item : items) {  ///< 当前需要发送结果信号的提交项。
        emit commitCompleted(item.finalPath, committed);
    }

    return committed;
}

/**
 * @brief 扫描并恢复未完成的备份事务，同时清理过期备份。
 */
bool TempFileManager::recoverIncompleteTransactions() {
    QDir backupRoot(backupRootPath());  ///< 备份事务根目录。
    if (!backupRoot.exists()) {
        return true;
    }

    bool recoveredAll = true;  ///< 是否所有未完成事务都恢复成功。
    QList<QFileInfo> committedTransactionDirs;  ///< 已完成、可参与过期清理的事务目录。
    const QFileInfoList transactionDirs =
        backupRoot.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Time);  ///< 按时间排序的事务目录。
    for (const QFileInfo& transactionDirInfo : transactionDirs) {  ///< 当前扫描的事务目录。
        const QString manifestPath =
            QDir(transactionDirInfo.absoluteFilePath()).filePath(QStringLiteral("manifest.json"));
        if (!QFile::exists(manifestPath)) {
            continue;
        }

        bool committed = false;  ///< 清单是否记录该事务已完成提交。
        const QVector<BackupEntry> entries = readManifestEntries(manifestPath, &committed);  ///< 事务清单条目。
        if (committed) {
            committedTransactionDirs.append(transactionDirInfo);
            continue;
        }

        for (const BackupEntry& entry : entries) {  ///< 当前需要恢复的备份条目。
            if (!entry.tempPath.isEmpty() && pathExists(entry.tempPath, entry.isDirectory)) {
                if (!removePath(entry.tempPath, entry.isDirectory)) {
                    qWarning() << "TempFileManager: Failed to remove orphaned temp path during recovery:"
                               << entry.tempPath;
                    recoveredAll = false;
                }
            }

            if (entry.existedBefore && !pathExists(entry.finalPath, entry.isDirectory) &&
                pathExists(entry.backupPath, entry.isDirectory)) {
                if (!movePathWithFallback(entry.backupPath, entry.finalPath, entry.isDirectory)) {
                    qWarning() << "TempFileManager: Failed to restore backup during recovery:" << entry.backupPath
                               << "to" << entry.finalPath;
                    recoveredAll = false;
                }
            }
        }
    }

    const QDateTime expirationCutoff =
        QDateTime::currentDateTimeUtc().addDays(-kDefaultBackupRetentionDays);  ///< 备份过期时间界限。
    for (int i = 0; i < committedTransactionDirs.size(); ++i) {  ///< 当前已完成事务索引。
        const QFileInfo& transactionDirInfo = committedTransactionDirs.at(i);  ///< 当前已完成事务目录。
        const bool exceedsMaxSets = i >= kDefaultMaxBackupSets;  ///< 是否超出最大保留组数。
        const bool isExpired = transactionDirInfo.lastModified().toUTC() < expirationCutoff;  ///< 是否已超过保留期限。
        if ((exceedsMaxSets || isExpired) && !QDir(transactionDirInfo.absoluteFilePath()).removeRecursively()) {
            qWarning() << "TempFileManager: Failed to prune committed backup set:"
                       << transactionDirInfo.absoluteFilePath();
            recoveredAll = false;
        }
    }

    return recoveredAll;
}

/**
 * @brief 在持有互斥锁时执行带备份的批量提交事务。
 * @param items 待处理的 items。
 */
bool TempFileManager::commitBatchLocked(const QVector<CommitItem>& items) {
    if (items.isEmpty()) {
        return true;
    }

    const QString transactionId = createTransactionId();  ///< 当前提交事务标识。
    const QString transactionDirPath = QDir(backupRootPath()).filePath(transactionId);  ///< 事务备份目录。
    if (!QDir().mkpath(transactionDirPath)) {
        qWarning() << "TempFileManager: Failed to create backup transaction directory:" << transactionDirPath;
        return false;
    }

    QVector<BackupEntry> entries;  ///< 待写入清单的备份条目。
    entries.reserve(items.size());
    for (int i = 0; i < items.size(); ++i) {  ///< 当前提交项索引。
        const CommitItem& item = items[i];  ///< 当前待提交的文件或目录。
        if (item.tempPath.isEmpty() || item.finalPath.isEmpty()) {
            qWarning() << "TempFileManager: Cannot commit empty temp/final path";
            return false;
        }
        if (!pathExists(item.tempPath, item.isDirectory)) {
            qWarning() << "TempFileManager: Temp path does not exist:" << item.tempPath;
            m_tempFiles.remove(item.tempPath);
            return false;
        }
        if (!ensureParentDirectory(item.finalPath)) {
            qWarning() << "TempFileManager: Failed to create final parent directory:" << item.finalPath;
            return false;
        }

        const QString backupName =
            QStringLiteral("%1_%2").arg(i, 4, 10, QLatin1Char('0')).arg(QFileInfo(item.finalPath).fileName());
        BackupEntry entry;  ///< 当前提交项对应的备份清单条目。
        entry.finalPath = item.finalPath;
        entry.backupPath = QDir(transactionDirPath).filePath(backupName);
        entry.tempPath = item.tempPath;
        entry.isDirectory = item.isDirectory;
        entry.existedBefore = pathExists(item.finalPath, item.isDirectory);
        entries.append(entry);
    }

    const QString manifestPath =
        QDir(transactionDirPath).filePath(QStringLiteral("manifest.json"));  ///< 事务清单路径。
    if (!writeManifest(manifestPath, transactionId, m_outputPath, entries, false)) {
        return false;
    }

    QVector<BackupEntry> backedUpEntries;  ///< 已成功备份的目标条目。
    for (const BackupEntry& entry : entries) {  ///< 当前需要备份的条目。
        if (!entry.existedBefore) {
            backedUpEntries.append(entry);
            continue;
        }

        if (!movePathWithFallback(entry.finalPath, entry.backupPath, entry.isDirectory)) {
            qWarning() << "TempFileManager: Failed to back up existing target:" << entry.finalPath << "to"
                       << entry.backupPath;
            for (auto it = backedUpEntries.crbegin(); it != backedUpEntries.crend(); ++it) {
                if (it->existedBefore && pathExists(it->backupPath, it->isDirectory) &&
                    !pathExists(it->finalPath, it->isDirectory)) {
                    movePathWithFallback(it->backupPath, it->finalPath, it->isDirectory);
                }
            }
            return false;
        }

        backedUpEntries.append(entry);
    }

    QVector<BackupEntry> promotedEntries;  ///< 已从临时路径提升到最终路径的条目。
    for (const BackupEntry& entry : entries) {  ///< 当前需要提升的条目。
        if (!movePathWithFallback(entry.tempPath, entry.finalPath, entry.isDirectory)) {
            qWarning() << "TempFileManager: Failed to promote temp path:" << entry.tempPath << "to" << entry.finalPath;

            removePath(entry.finalPath, entry.isDirectory);
            for (auto it = promotedEntries.crbegin(); it != promotedEntries.crend(); ++it) {
                removePath(it->finalPath, it->isDirectory);
            }
            for (auto it = backedUpEntries.crbegin(); it != backedUpEntries.crend(); ++it) {
                if (it->existedBefore && pathExists(it->backupPath, it->isDirectory)) {
                    if (!movePathWithFallback(it->backupPath, it->finalPath, it->isDirectory)) {
                        qWarning() << "TempFileManager: Failed to restore backup:" << it->backupPath << "to"
                                   << it->finalPath;
                    }
                }
            }
            return false;
        }

        promotedEntries.append(entry);
    }

    if (!writeManifest(manifestPath, transactionId, m_outputPath, entries, true)) {
        qWarning() << "TempFileManager: Commit succeeded but manifest could not be finalized:" << manifestPath;
    }

    for (const BackupEntry& entry : entries) {
        m_tempFiles.remove(entry.tempPath);
        qDebug() << "TempFileManager: Committed with backup:" << entry.tempPath << "->" << entry.finalPath;
    }
    cleanupEmptyTempDirectoryLocked();

    return true;
}

/**
 * @brief 删除本实例登记的全部临时文件并回收空临时目录。
 */
void TempFileManager::rollbackAll() {
    QMutexLocker locker(&m_mutex);  ///< 保护临时文件集合的锁。

    int deletedCount = 0;  ///< 已成功删除的临时路径数量。
    for (const QString& tempPath : m_tempFiles) {  ///< 当前待回滚的临时路径。
        if (QFileInfo(tempPath).isDir()) {
            if (QDir(tempPath).removeRecursively()) {
                deletedCount++;
            } else {
                qWarning() << "TempFileManager: Failed to remove temp directory:" << tempPath;
            }
        } else if (deleteFile(tempPath)) {
            deletedCount++;
        }
    }

    m_tempFiles.clear();
    cleanupEmptyTempDirectoryLocked();

    locker.unlock();
    emit cleanupCompleted(deletedCount);

    qDebug() << "TempFileManager: Rolled back" << deletedCount << "temp files";
}

/**
 * @brief 清理当前临时目录中的普通临时文件。
 */
void TempFileManager::cleanupTempDirectory() {
    QMutexLocker locker(&m_mutex);  ///< 保护清理过程和临时目录状态的锁。

    QString tempDir = tempDirectory();  ///< 当前共享临时目录路径。
    if (tempDir.isEmpty() || !QDir(tempDir).exists()) {
        return;
    }
    // 共享临时目录仍被其他导出阶段使用时，不能清理其中的文件。
    if (hasOtherTempDirectoryUsers(tempDir)) {
        qDebug() << "TempFileManager: Skip cleanup while another stage uses" << tempDir;
        return;
    }

    int deletedCount = 0;  ///< 已成功删除的普通临时文件数量。
    QDir dir(tempDir);  ///< 当前共享临时目录对象。
    for (const QFileInfo& info : dir.entryInfoList(QDir::Files)) {  ///< 当前待处理的临时文件。
        if (deleteFile(info.absoluteFilePath())) {
            deletedCount++;
        }
    }

    if (dir.isEmpty()) {
        QDir().rmdir(tempDir);
    }

    locker.unlock();
    emit cleanupCompleted(deletedCount);
}

/**
 * @brief 清理临时目录中未登记的孤立文件。
 */
void TempFileManager::cleanupOrphanedTempFiles() {
    QString tempDir = tempDirectory();  ///< 当前共享临时目录路径。
    if (tempDir.isEmpty() || !QDir(tempDir).exists()) {
        return;
    }
    // 孤立文件清理同样必须尊重共享目录引用，避免破坏并行导出任务。
    if (hasOtherTempDirectoryUsers(tempDir)) {
        qDebug() << "TempFileManager: Skip orphan cleanup while another stage uses" << tempDir;
        return;
    }

    int deletedCount = 0;  ///< 已成功删除的孤立文件数量。
    QDir dir(tempDir);  ///< 当前共享临时目录对象。
    for (const QFileInfo& info : dir.entryInfoList(QDir::Files)) {  ///< 当前待处理的孤立文件。
        if (info.fileName().startsWith('.')) {
            continue;  // 跳过隐藏文件
        }
        if (deleteFile(info.absoluteFilePath())) {
            deletedCount++;
        }
    }

    if (dir.isEmpty()) {
        QDir().rmdir(tempDir);
    }

    qDebug() << "TempFileManager: Cleaned up" << deletedCount << "orphaned temp files";
}

/**
 * @brief 登记一个由本实例负责回收的临时路径。
 * @param tempPath 待处理的 tempPath。
 */
void TempFileManager::registerTempFile(const QString& tempPath) {
    QMutexLocker locker(&m_mutex);
    m_tempFiles.insert(tempPath);
}

/**
 * @brief 返回当前实例登记的临时路径集合快照。
 */
QSet<QString> TempFileManager::registeredTempFiles() const {
    QMutexLocker locker(&m_mutex);  ///< 保护临时文件集合的锁。
    return m_tempFiles;
}

/**
 * @brief 根据前缀、后缀和随机值生成临时文件名。
 * @param prefix 待处理的 prefix。
 * @param suffix 待处理的 suffix。
 */
QString TempFileManager::generateUniqueTempName(const QString& prefix, const QString& suffix) const {
    quint64 random = QRandomGenerator::global()->generate64();  ///< 用于降低临时文件名冲突概率的随机值。
    QString uuid = QString::number(random, 16);  ///< 随机值的十六进制字符串。
    return prefix + QStringLiteral("_") + uuid + suffix;
}

/**
 * @brief 确保当前共享临时目录存在。
 */
bool TempFileManager::ensureTempDirectory() const {
    QString tempDir = tempDirectory();  ///< 当前共享临时目录路径。
    if (tempDir.isEmpty()) {
        qWarning() << "TempFileManager::ensureTempDirectory: tempDir is empty, m_outputPath:" << m_outputPath;
        return false;
    }

    if (QDir(tempDir).exists()) {
        return true;
    }

    bool result = QDir().mkpath(tempDir);  ///< 创建共享临时目录的结果。
    qDebug() << "TempFileManager::ensureTempDirectory: Created" << tempDir << "result:" << result;
    return result;
}

/**
 * @brief 删除指定文件，并将不存在的文件视为已完成清理。
 * @param path 目标路径。
 */
bool TempFileManager::deleteFile(const QString& path) const {
    if (path.isEmpty()) {
        return true;
    }

    QFile file(path);  ///< 待删除的文件对象。
    if (!file.exists()) {
        return true;
    }

    if (file.remove()) {
        qDebug() << "TempFileManager: Deleted:" << path;
        return true;
    }

    qWarning() << "TempFileManager: Failed to delete:" << path;
    return false;
}

/**
 * @brief 在持锁状态下清理空的共享临时目录。
 */
bool TempFileManager::cleanupEmptyTempDirectoryLocked() const {
    const QString tempDir = tempDirectory();  ///< 当前共享临时目录路径。
    if (tempDir.isEmpty()) {
        return true;
    }

    QDir dir(tempDir);  ///< 当前共享临时目录对象。
    if (!dir.exists()) {
        return true;
    }

    // 多个导出阶段共享同一个 .tmp 根目录，不能由其中一个阶段提前删除。
    if (hasOtherTempDirectoryUsers(tempDir)) {
        return false;
    }

    const QFileInfoList entries =
        dir.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries | QDir::Hidden | QDir::System);
    if (!entries.isEmpty()) {
        return false;
    }

    if (QDir().rmdir(tempDir)) {
        qDebug() << "TempFileManager: Removed empty temp directory:" << tempDir;
        return true;
    }

    qWarning() << "TempFileManager: Failed to remove empty temp directory:" << tempDir;
    return false;
}

}  // namespace EasyKiConverter
