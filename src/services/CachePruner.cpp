#include "CachePruner.h"

#include <QDir>
#include <QFileInfo>
#include <QPair>
#include <QString>

#include <algorithm>

namespace EasyKiConverter {

namespace {
qint64 _calculateDirSize(const QString& dirPath) {
    qint64 size = 0;
    QDir dir(dirPath);
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

CachePruner::CachePruner(const QString& cacheRoot, const CacheSafety::TrashFunction& trash)
    : m_cacheRoot(cacheRoot), m_trash(trash) {}

qint64 CachePruner::calculateDirSize(const QString& dirPath) const {
    return _calculateDirSize(dirPath);
}

qint64 CachePruner::currentCacheSize() const {
    qint64 totalSize = 0;
    for (const QString& path : CacheSafety::ownedComponentDirectories(m_cacheRoot))
        totalSize += _calculateDirSize(path);
    return totalSize;
}

qint64 CachePruner::pruneTo(qint64 targetSizeBytes) {
    struct CacheEntry {
        QString name;
        QDateTime lastModified;
        qint64 size;
    };

    // 第一阶段：收集所有缓存条目信息（排除 model3d）
    QList<CacheEntry> cacheList;
    qint64 currentSize = 0;

    for (const QString& path : CacheSafety::ownedComponentDirectories(m_cacheRoot)) {
        const QFileInfo info(path);
        const qint64 entrySize = _calculateDirSize(path);
        cacheList.append({path, info.lastModified(), entrySize});
        currentSize += entrySize;
    }

    if (currentSize <= targetSizeBytes) {
        return currentSize;
    }

    // 按访问时间排序（最老的在前）
    std::sort(cacheList.begin(), cacheList.end(), [](const CacheEntry& a, const CacheEntry& b) {
        return a.lastModified < b.lastModified;
    });

    // 第二阶段：执行删除操作（使用缓存的大小值，避免重复扫描）
    for (const auto& entry : cacheList) {
        if (currentSize <= targetSizeBytes) {
            break;
        }

        QString error;
        if (CacheSafety::moveToTrash(entry.name, &error, m_trash)) {
            currentSize -= entry.size;
        }
    }

    return currentSize;
}

}  // namespace EasyKiConverter
