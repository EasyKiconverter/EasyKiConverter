#pragma once

#include "CacheSafety.h"

#include <QString>
#include <QtGlobal>

namespace EasyKiConverter {

class CachePruner {
public:
    explicit CachePruner(const QString& cacheRoot,
                         const CacheSafety::TrashFunction& trash = CacheSafety::TrashFunction());
    ~CachePruner() = default;

    /**
     * @brief 执行LRU缓存淘汰
     * @param targetSizeBytes 目标缓存大小
     * @return 淘汰后当前缓存大小
     */
    qint64 pruneTo(qint64 targetSizeBytes);

    /**
     * @brief 计算目录大小
     */
    qint64 calculateDirSize(const QString& dirPath) const;

    /**
     * @brief 获取当前缓存总大小
     */
    qint64 currentCacheSize() const;

private:
    QString m_cacheRoot;
    CacheSafety::TrashFunction m_trash;
};

}  // namespace EasyKiConverter
