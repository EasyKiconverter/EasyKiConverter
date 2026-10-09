#pragma once

#include <QByteArray>
#include <QString>

#include <cstdint>

namespace EasyKiConverter {

class ComponentCacheService;

/**
 * @brief 负责数据手册缓存文件的读取和原子写入。
 * @details 该类只处理二级磁盘缓存，不负责网络下载、重试策略或网络诊断。
 */
class DatasheetCacheFileStore final {
public:
    /**
     * @brief 创建绑定到指定缓存服务的数据手册文件存储器。
     * @param owner 所属的组件缓存服务。
     */
    explicit DatasheetCacheFileStore(ComponentCacheService& owner);

    /**
     * @brief 从磁盘读取并校验数据手册。
     * @param owner 缓存服务，只读取其路径和锁状态
     * @param componentId 元器件编号
     * @return 有效的数据手册内容，不存在或无效时返回空数据
     */
    static QByteArray load(const ComponentCacheService& owner, const QString& componentId);

    /**
     * @brief 校验并原子写入数据手册。
     * @param componentId 元器件编号
     * @param data 数据手册内容
     * @param format 声明格式（pdf/html）
     * @param expectedGeneration 请求创建时捕获的缓存代次
     */
    void save(const QString& componentId, const QByteArray& data, const QString& format, uint64_t expectedGeneration);

private:
    ComponentCacheService& m_owner;  ///< 所属的组件缓存服务。
};

}  // namespace EasyKiConverter
