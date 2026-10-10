#ifndef TEMPFILEMANAGER_H
#define TEMPFILEMANAGER_H

#include <QDir>
#include <QFileInfo>
#include <QMutex>
#include <QObject>
#include <QSet>
#include <QString>
#include <QVector>

namespace EasyKiConverter {

/**
 * @brief 临时文件管理器
 *
 * 管理导出临时路径、备份事务提交、失败回滚和启动恢复。
 * commit()/commitDirectory() 保留旧调用面，内部同样使用备份事务。
 */
class TempFileManager : public QObject {
    Q_OBJECT

public:
    struct CommitItem {
        QString tempPath;  ///< 待提交的临时文件或目录路径。
        QString finalPath;  ///< 提交后的最终文件或目录路径。
        bool isDirectory = false;  ///< 路径是否指向目录。
    };

    /**
     * @brief 构造函数
     * @param parent 父对象
     */
    explicit TempFileManager(QObject* parent = nullptr);

    /**
     * @brief 析构函数
     * @details 自动清理所有临时文件
     */
    ~TempFileManager() override;

    /**
     * @brief 设置输出目录
     * @param outputPath 导出输出目录路径
     */
    void setOutputPath(const QString& outputPath);

    /**
     * @brief 获取临时文件目录路径
     * @return .tmp 子目录的完整路径
     */
    QString tempDirectory() const;

    /**
     * @brief 创建临时文件路径
     * @param componentId 元器件ID（用于生成文件名）
     * @param suffix 文件扩展名（如 ".kicad_sym"）
     * @return 最终文件路径（用于调用方写入）
     *
     * 注意：实际文件创建在 .tmp 子目录中，此方法返回最终路径供写入使用
     */
    QString createTempFilePath(const QString& componentId, const QString& suffix);

    /**
     * @brief 获取临时文件对应的实际路径
     * @param finalPath 最终文件路径
     * @return 临时文件路径（.tmp/ 目录下的路径）
     */
    QString tempFilePath(const QString& finalPath) const;

    /**
     * @brief 获取符号库临时文件路径
     * @param libName 库名称
     * @param suffix 文件扩展名
     * @return 临时文件路径
     */
    QString createSymbolTempPath(const QString& libName, const QString& suffix);

    /**
     * @brief 创建临时目录路径
     * @param dirName 目录名称（如 "libName.pretty"）
     * @return 临时目录路径
     *
     * 注意：仅创建路径信息，实际目录由调用方创建
     */
    QString createTempDirectoryPath(const QString& dirName);

    /**
     * @brief 提交临时文件（完成导出）
     * @param finalPath 最终文件路径
     * @return true 提交成功，false 失败
     */
    bool commit(const QString& finalPath);

    /**
     * @brief 提交临时目录（完成导出）
     * @param finalDirPath 最终目录路径
     * @return true 提交成功，false 失败
     */
    bool commitDirectory(const QString& finalDirPath);

    /**
     * @brief 提交指定临时文件
     * @param tempPath 临时文件路径
     * @param finalPath 最终文件路径
     * @return true 提交成功，false 失败且已尽量恢复旧文件
     */
    bool commitWithBackup(const QString& tempPath, const QString& finalPath);

    /**
     * @brief 提交指定临时目录
     * @param tempDirPath 临时目录路径
     * @param finalDirPath 最终目录路径
     * @return true 提交成功，false 失败且已尽量恢复旧目录
     */
    bool commitDirectoryWithBackup(const QString& tempDirPath, const QString& finalDirPath);

    /**
     * @brief 全有或全无地提交一组临时文件/目录
     * @param items 临时路径与最终路径的映射
     * @return true 全部提交成功，false 失败且已尽量恢复全部旧版本
     */
    bool commitBatch(const QVector<CommitItem>& items);

    /**
     * @brief 恢复上次崩溃或异常退出留下的未完成提交事务
     * @return true 扫描和恢复流程完成，false 发生不可恢复错误
     */
    static bool recoverIncompleteTransactions();

    /**
     * @brief 回滚所有临时文件（取消导出）
     *
     * 删除所有临时文件，清空追踪列表
     */
    void rollbackAll();

    /**
     * @brief 仅删除临时目录（保留其他文件）
     *
     * 用于导出完成后的清理
     */
    void cleanupTempDirectory();

    /**
     * @brief 检查并清理遗留的临时文件
     *
     * 在应用程序启动时调用，清理上次异常退出留下的临时文件
     */
    void cleanupOrphanedTempFiles();

    /**
     * @brief 注册一个临时文件路径（用于追踪）
     * @param tempPath 临时文件路径
     */
    void registerTempFile(const QString& tempPath);

    /**
     * @brief 获取所有已注册的临时文件
     * @return 临时文件路径集合
     */
    QSet<QString> registeredTempFiles() const;

signals:
    /**
     * @brief 临时文件清理完成信号
     * @param deletedCount 删除的文件数量
     */
    void cleanupCompleted(int deletedCount);

    /**
     * @brief 提交完成信号
     * @param finalPath 最终文件路径
     * @param success 是否成功
     */
    void commitCompleted(const QString& finalPath, bool success);

private:
    /**
     * @brief 生成唯一的临时文件名
     * @param prefix 文件名前缀
     * @param suffix 文件扩展名
     * @return 唯一的临时文件名
     */
    QString generateUniqueTempName(const QString& prefix, const QString& suffix) const;

    /**
     * @brief 确保临时目录存在
     * @return true 目录存在或创建成功
     */
    bool ensureTempDirectory() const;

    /**
     * @brief 实际执行文件删除
     * @param path 文件路径
     * @return true 删除成功或文件不存在
     */
    bool deleteFile(const QString& path) const;

    bool commitBatchLocked(const QVector<CommitItem>& items);
    bool cleanupEmptyTempDirectoryLocked() const;

    QString m_outputPath;  ///< 输出目录路径
    mutable QMutex m_mutex;  ///< 保护临时文件集合
    QSet<QString> m_tempFiles;  ///< 注册的临时文件集合
    QString m_tempDirName = QStringLiteral(".tmp");  ///< 临时目录名称
    QString m_registeredTempDirectory;  ///< 当前实例登记的共享临时目录
};

}  // namespace EasyKiConverter

#endif  // TEMPFILEMANAGER_H
