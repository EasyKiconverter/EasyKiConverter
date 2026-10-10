#ifndef COMPONENTCACHEMODEL3DCOORDINATOR_H
#define COMPONENTCACHEMODEL3DCOORDINATOR_H

#include <QByteArray>
#include <QString>

#include <cstdint>

namespace EasyKiConverter {

class ComponentCacheService;

/**
 * @brief 协调三维模型缓存的读取、写入和导出复制。
 *
 * 该类只负责缓存服务层的锁、代次和容量策略，具体文件格式由
 * Model3DCacheFileStore 负责处理。
 */
class ComponentCacheModel3DCoordinator {
public:
    /**
     * @brief 创建三维模型缓存协调器。
     * @param owner 所属缓存服务。
     */
    explicit ComponentCacheModel3DCoordinator(ComponentCacheService& owner);

    /**
     * @brief 检查指定三维模型是否存在且内容有效。
     * @param uuid 三维模型标识。
     * @param extension 模型扩展名。
     * @return 模型存在且内容有效时返回 true。
     */
    bool hasCached(const QString& uuid, const QString& extension) const;

    /**
     * @brief 从缓存读取三维模型，缓存不可用时返回空数组。
     * @param uuid 三维模型标识。
     * @param extension 模型扩展名。
     * @return 模型文件数据。
     */
    QByteArray load(const QString& uuid, const QString& extension) const;

    /**
     * @brief 校验代次后保存三维模型并触发统一容量策略。
     * @param uuid 三维模型标识。
     * @param data 模型文件数据。
     * @param extension 模型扩展名。
     * @param expectedGeneration 调用方捕获的缓存代次。
     */
    void save(const QString& uuid, const QByteArray& data, const QString& extension, uint64_t expectedGeneration);

    /**
     * @brief 将缓存中的三维模型复制到导出目标。
     * @param uuid 三维模型标识。
     * @param extension 模型扩展名。
     * @param destinationPath 导出目标路径。
     * @return 复制成功时返回 true。
     */
    bool copyToFile(const QString& uuid, const QString& extension, const QString& destinationPath) const;

private:
    /**
     * @brief 保存所属的缓存服务，用于复用统一锁和路径策略。
     */
    ComponentCacheService& m_owner;
};

}  // namespace EasyKiConverter

#endif  // COMPONENTCACHEMODEL3DCOORDINATOR_H
