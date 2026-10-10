#include "CachePruner.h"

#include <QDir>
#include <QFileInfo>
#include <QPair>
#include <QString>

#include <algorithm>

namespace EasyKiConverter {

namespace {
/**
 * @brief 递归统计目录中的普通文件大小。
 * @param dirPath 待统计的目录路径。
 * @return 不包含符号链接的文件总大小（字节）。
 */
qint64 _calculateDirSize(const QString& dirPath) {
    qint64 size = 0;  // 当前目录及其子目录的累计大小。
    QDir dir(dirPath);  // 待统计的目录对象。
    if (!dir.exists()) {
        return size;
    }

    for (const QFileInfo& info : dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot)) {
        if (!info.isSymLink())
            size += info.size();
    }

    for (const QFileInfo& info : dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        if (!info.isSymLink())
            size += _calculateDirSize(info.absoluteFilePath());
    }

    return size;
}
}  // namespace

/**
 * @brief 创建缓存配额裁剪器。
 * @param cacheRoot 缓存根目录。
 * @param trash 回收站操作实现。
 */
CachePruner::CachePruner(const QString& cacheRoot, const CacheSafety::TrashFunction& trash)
    : m_cacheRoot(cacheRoot), m_trash(trash) {}

/**
 * @brief 计算目录占用空间。
 * @param dirPath 待统计的目录路径。
 * @return 目录中普通文件的总大小（字节）。
 */
qint64 CachePruner::calculateDirSize(const QString& dirPath) const {
    return _calculateDirSize(dirPath);
}

/**
 * @brief 计算当前所有权可验证缓存的总大小。
 * @return 可维护缓存条目的总大小（字节）。
 */
qint64 CachePruner::currentCacheSize() const {
    qint64 totalSize = 0;  // 可验证缓存条目的累计大小。
    for (const QString& componentPath : CacheSafety::ownedComponentDirectories(m_cacheRoot)) {
        for (const QString& filePath : CacheSafety::ownedComponentFiles(m_cacheRoot, componentPath))
            totalSize += QFileInfo(filePath).size();
    }
    return totalSize;
}

/**
 * @brief 将缓存裁剪到指定大小以内。
 * @param targetSizeBytes 目标大小（字节）。
 * @return 裁剪后的可验证缓存总大小（字节）。
 */
qint64 CachePruner::pruneTo(qint64 targetSizeBytes) {
    struct CacheEntry {
        QString name;  ///< 可验证缓存文件路径。
        QDateTime lastModified;  ///< 文件最后修改时间。
        qint64 size;  ///< 文件大小（字节）。
    };

    // 第一阶段：按文件收集可验证的缓存条目（排除 model3d 和未知文件）。
    QList<CacheEntry> cacheList;  // 按文件记录的可验证缓存条目。
    qint64 currentSize = 0;  // 当前可维护缓存总大小。

    for (const QString& componentPath : CacheSafety::ownedComponentDirectories(m_cacheRoot)) {
        for (const QString& path : CacheSafety::ownedComponentFiles(m_cacheRoot, componentPath)) {
            const QFileInfo info(path);  // 当前缓存文件信息。
            const qint64 entrySize = info.size();  // 当前缓存文件大小。
            cacheList.append({path, info.lastModified(), entrySize});
            currentSize += entrySize;
        }
    }

    if (currentSize <= targetSizeBytes) {
        return currentSize;
    }

    // 按最后修改时间排序，最早修改的条目优先裁剪。
    std::sort(cacheList.begin(), cacheList.end(), [](const CacheEntry& a, const CacheEntry& b) {
        return a.lastModified < b.lastModified;
    });

    // 第二阶段：将经过所有权验证的条目移入回收站。
    for (const auto& entry : cacheList) {
        if (currentSize <= targetSizeBytes) {
            break;
        }

        QString error;  // 当前回收站操作的错误信息。
        if (CacheSafety::moveToTrash(entry.name, &error, m_trash)) {
            currentSize -= entry.size;
        }
    }

    return currentSize;
}

}  // namespace EasyKiConverter
