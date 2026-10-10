#pragma once

#include <QByteArray>
#include <QString>

namespace EasyKiConverter {

/**
 * @brief 读取并校验缓存中的二进制文件。
 * @details 只负责文件级 I/O 和损坏数据清理，不管理缓存目录、锁或缓存代次。
 */
class ComponentCacheBinaryFileStore {
public:
    /**
     * @brief 读取并校验 CAD 或符号封装二进制数据。
     * @param filePath 文件路径
     * @return 有效数据；文件不存在、无法读取或校验失败时返回空数组
     */
    static QByteArray readCadData(const QString& filePath);

    /**
     * @brief 读取并校验预览图数据。
     * @param filePath 文件路径
     * @return 有效图片数据；文件不存在、无法读取或校验失败时返回空数组
     */
    static QByteArray readPreviewImage(const QString& filePath);
};

}  // namespace EasyKiConverter
