#ifndef TEMPFILETRANSACTIONUTILS_H
#define TEMPFILETRANSACTIONUTILS_H

#include <QString>
#include <QVector>

namespace EasyKiConverter::TempFileTransaction {

/**
 * @brief 记录一次备份事务中临时路径、最终路径和备份路径的映射。
 */
struct BackupEntry {
    QString finalPath;  ///< 事务提交使用的最终路径。
    QString backupPath;  ///< 覆盖前保存旧内容的备份路径。
    QString tempPath;  ///< 本次事务生成的临时路径。
    bool isDirectory = false;  ///< 路径是否指向目录。
    bool existedBefore = false;  ///< 事务开始前最终路径是否已存在。
};

/**
 * @brief 返回应用数据目录下的备份事务根目录。
 */
QString backupRootPath();

/**
 * @brief 创建具有时间和随机后缀的事务标识。
 */
QString createTransactionId();

/**
 * @brief 确保目标路径的父目录存在。
 * @param path 目标路径
 * @return 父目录存在或创建成功时返回 true
 */
bool ensureParentDirectory(const QString& path);

/**
 * @brief 按路径类型移动对象，并在跨卷或占用时使用复制回退。
 * @param sourcePath 源路径
 * @param targetPath 目标路径
 * @param isDirectory 路径是否指向目录
 * @return 移动成功时返回 true
 */
bool movePathWithFallback(const QString& sourcePath, const QString& targetPath, bool isDirectory);

/**
 * @brief 按路径类型删除对象，目标不存在时视为成功。
 * @param path 待删除路径
 * @param isDirectory 路径是否指向目录
 * @return 删除成功或目标不存在时返回 true
 */
bool removePath(const QString& path, bool isDirectory);

/**
 * @brief 按路径类型检查对象是否存在。
 * @param path 待检查路径
 * @param isDirectory 路径是否指向目录
 * @return 对象存在时返回 true
 */
bool pathExists(const QString& path, bool isDirectory);

/**
 * @brief 写入备份事务清单。
 * @param manifestPath 清单文件路径
 * @param transactionId 事务标识
 * @param outputRoot 输出根目录
 * @param entries 事务条目列表
 * @param committed 是否已提交
 * @return 清单写入成功时返回 true
 */
bool writeManifest(const QString& manifestPath,
                   const QString& transactionId,
                   const QString& outputRoot,
                   const QVector<BackupEntry>& entries,
                   bool committed);

/**
 * @brief 读取备份事务清单，并返回提交状态和路径映射。
 * @param manifestPath 清单文件路径
 * @param committed 输出事务提交状态，可为空
 * @return 读取到的事务条目列表
 */
QVector<BackupEntry> readManifestEntries(const QString& manifestPath, bool* committed);

}  // namespace EasyKiConverter::TempFileTransaction

#endif  // TEMPFILETRANSACTIONUTILS_H
