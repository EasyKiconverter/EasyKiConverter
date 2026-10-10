#ifndef MODEL3DCACHEFILESTORE_H
#define MODEL3DCACHEFILESTORE_H

#include <QByteArray>
#include <QString>

namespace EasyKiConverter {

/**
 * @brief 负责三维模型缓存文件的读取、校验、写入和复制。
 *
 * 该类只处理文件级 I/O，不管理缓存目录、锁、缓存代次或磁盘配额。
 */
class Model3DCacheFileStore final {
public:
    /**
     * @brief 读取并校验三维模型文件。
     * @param filePath 模型文件路径
     * @param extension 模型扩展名
     * @return 有效模型数据；文件不存在、无法读取或校验失败时返回空数组
     */
    static QByteArray read(const QString& filePath, const QString& extension);

    /**
     * @brief 校验并原子写入三维模型文件。
     * @param filePath 模型文件路径
     * @param data 模型数据
     * @param extension 模型扩展名
     * @return 写入成功时返回 true
     */
    static bool write(const QString& filePath, const QByteArray& data, const QString& extension);

    /**
     * @brief 校验源模型并复制到目标文件。
     * @param sourcePath 缓存中的源文件路径
     * @param destinationPath 导出目标路径
     * @param extension 模型扩展名
     * @return 复制成功时返回 true
     */
    static bool copy(const QString& sourcePath, const QString& destinationPath, const QString& extension);
};

}  // namespace EasyKiConverter

#endif  // MODEL3DCACHEFILESTORE_H
