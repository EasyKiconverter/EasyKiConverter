#pragma once

#include "CacheSafety.h"

#include <QString>
#include <QtGlobal>

namespace EasyKiConverter {

class CachePruner {
public:
    /**
     * @brief 创建缓存配额裁剪器。
     * @param cacheRoot 缓存根目录。
     * @param trash 可注入的回收站操作实现。
     */
    explicit CachePruner(const QString& cacheRoot,
                         const CacheSafety::TrashFunction& trash = CacheSafety::TrashFunction());
    ~CachePruner() = default;

    /**
     * @brief 执行LRU缓存淘汰
     * @param targetSizeBytes 目标缓存大小（字节）。
     * @return 淘汰后当前可验证缓存大小（字节）。
     */
    qint64 pruneTo(qint64 targetSizeBytes);

    /**
     * @brief 计算目录大小。
     * @param dirPath 待统计的目录路径。
     * @return 目录中普通文件的总大小（字节）。
     */
    qint64 calculateDirSize(const QString& dirPath) const;

    /**
     * @brief 获取当前缓存总大小。
     * @return 当前所有权可验证缓存的总大小（字节）。
     */
    qint64 currentCacheSize() const;

private:
    QString m_cacheRoot;  ///< 待维护的缓存根目录。
    CacheSafety::TrashFunction m_trash;  ///< 可注入的系统回收站操作。
};

}  // namespace EasyKiConverter
