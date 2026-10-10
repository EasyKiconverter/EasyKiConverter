#pragma once

#include <QString>

namespace EasyKiConverter {

/**
 * @brief 解析二级缓存中的组件、数据手册和三维模型路径。
 *
 * 所有方法均为无状态操作，不持有缓存锁；调用方负责在需要时保护缓存根目录
 * 的读取。模型编号和扩展名会在生成路径前进行严格校验。
 */
class CachePathResolver final {
public:
    /**
     * @brief 返回指定组件的缓存目录，并兼容旧目录大小写。
     * @param cacheRoot 缓存根目录
     * @param componentId 元件编号
     * @return 组件缓存目录；输入非法时为空
     */
    static QString componentDir(const QString& cacheRoot, const QString& componentId);

    /**
     * @brief 返回组件元数据文件路径。
     * @param cacheRoot 缓存根目录。
     * @param componentId 元件编号。
     * @return 组件元数据文件路径；输入非法时为空。
     */
    static QString metadataPath(const QString& cacheRoot, const QString& componentId);

    /**
     * @brief 返回指定序号的预览图文件路径。
     * @param cacheRoot 缓存根目录。
     * @param componentId 元件编号。
     * @param index 预览图序号。
     * @return 预览图文件路径；输入非法时为空。
     */
    static QString previewImagePath(const QString& cacheRoot, const QString& componentId, int index);

    /**
     * @brief 返回数据手册的无扩展名基础路径。
     * @param cacheRoot 缓存根目录。
     * @param componentId 元件编号。
     * @return 数据手册基础路径；输入非法时为空。
     */
    static QString datasheetPath(const QString& cacheRoot, const QString& componentId);

    /**
     * @brief 返回公共三维模型缓存文件路径。
     * @param cacheRoot 缓存根目录
     * @param uuid 模型标识
     * @param extension 模型扩展名
     * @return 模型文件路径；标识或扩展名非法时为空
     */
    static QString model3DPath(const QString& cacheRoot, const QString& uuid, const QString& extension);
};

}  // namespace EasyKiConverter
