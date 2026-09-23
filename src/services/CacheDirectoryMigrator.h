#ifndef CACHEDIRECTORYMIGRATOR_H
#define CACHEDIRECTORYMIGRATOR_H

#include <QString>

namespace EasyKiConverter {

/**
 * @brief 在缓存目录切换时迁移已有缓存内容。
 *
 * 该类只处理文件系统迁移，不持有 ComponentCacheService 的缓存锁，也不修改
 * 缓存服务状态。调用方应在适当的写锁范围内调用 migrate()。
 */
class CacheDirectoryMigrator final {
public:
    /**
     * @brief 将旧缓存目录内容迁移到新目录。
     * @param oldCacheDir 旧缓存目录。
     * @param newCacheDir 新缓存目录。
     * @return 所有条目均迁移成功时返回 true。
     */
    static bool migrate(const QString& oldCacheDir, const QString& newCacheDir);

private:
    /**
     * @brief 迁移目录中的全部缓存条目。
     */
    static bool moveDirectoryContents(const QString& sourceRoot,
                                      const QString& sourceDir,
                                      const QString& targetRoot,
                                      const QString& targetDir,
                                      bool legacySource);

    /**
     * @brief 迁移单个缓存文件或目录条目。
     */
    static bool moveCacheEntry(const QString& sourcePath, const QString& targetPath);
};

}  // namespace EasyKiConverter

#endif  // CACHEDIRECTORYMIGRATOR_H
