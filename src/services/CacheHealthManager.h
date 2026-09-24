#pragma once

#include "CacheSafety.h"

#include <QJsonObject>
#include <QString>

namespace EasyKiConverter {

class ComponentCacheService;

/**
 * @brief 缓存自愈管理器
 *
 * 负责检测和修复损坏的缓存数据：
 * - 验证缓存目录的元数据完整性
 * - 清理无效/损坏的CAD数据文件
 * - 清理空的或损坏的预览图
 * - 迁移遗留格式的数据手册文件
 * - 清理损坏的3D模型缓存
 */
class CacheHealthManager {
public:
    explicit CacheHealthManager(const QString& cacheRoot,
                                const CacheSafety::TrashFunction& trash = CacheSafety::TrashFunction());
    ~CacheHealthManager() = default;

    /**
     * @brief 执行完整缓存自愈
     * @return 修复的缓存目录数量
     */
    int healAll();

private:
    /**
     * @brief 修复单个元器件缓存目录
     * @param lcscId 元器件ID
     * @return true 如果目录有效并保留，false 如果已删除
     */
    bool repairComponentCache(const QString& lcscId);

    /**
     * @brief 修复3D模型缓存目录
     */
    void repairModel3DCache();

    /**
     * @brief 获取元器件缓存目录路径
     */
    QString componentCacheDir(const QString& lcscId) const;

    /**
     * @brief 获取元数据文件路径
     */
    QString metadataPath(const QString& lcscId) const;

    /**
     * @brief 加载元数据
     */
    QJsonObject loadMetadata(const QString& lcscId) const;

    /**
     * @brief 保存元数据
     */
    bool saveMetadata(const QString& lcscId, const QJsonObject& metadata);

    /**
     * @brief 获取数据手册路径
     */
    QString datasheetPath(const QString& lcscId) const;

    /**
     * @brief 解析数据手册路径
     */
    QString resolveDatasheetPath(const QString& lcscId, const QString& format, bool migrate) const;

    /**
     * @brief 预览图路径
     */
    QString previewImagePath(const QString& lcscId, int index) const;

    QString m_cacheRoot;
    CacheSafety::TrashFunction m_trash;
};

}  // namespace EasyKiConverter
