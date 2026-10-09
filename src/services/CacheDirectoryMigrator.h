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
     * @param error 输出具体的预检、冲突或迁移失败原因。
     * @return 所有条目均迁移成功时返回 true。
     */
    static bool migrate(const QString& oldCacheDir, const QString& newCacheDir, QString* error = nullptr);

private:
    /**
     * @brief 迁移目录中的全部缓存条目。
     * @param sourceRoot 源缓存根目录。
     * @param sourceDir 当前源组件目录。
     * @param targetRoot 目标缓存根目录。
     * @param targetDir 当前目标组件目录。
     * @param legacySource 是否按旧版未标记缓存规则迁移。
     * @return 目录内容全部迁移成功时返回 true。
     */
    static bool moveDirectoryContents(const QString& sourceRoot,
                                      const QString& sourceDir,
                                      const QString& targetRoot,
                                      const QString& targetDir,
                                      bool legacySource);

    /**
     * @brief 迁移单个缓存文件或目录条目。
     * @param sourcePath 源条目路径。
     * @param targetPath 目标条目路径。
     * @return 条目迁移成功时返回 true。
     */
    static bool moveCacheEntry(const QString& sourcePath, const QString& targetPath);
};

}  // namespace EasyKiConverter

#endif  // CACHEDIRECTORYMIGRATOR_H
