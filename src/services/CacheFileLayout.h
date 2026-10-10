#ifndef CACHEFILELAYOUT_H
#define CACHEFILELAYOUT_H

#include <QDir>
#include <QString>

namespace EasyKiConverter {

/**
 * @brief 集中定义元器件磁盘缓存的文件布局。
 *
 * 该类只负责根据已经解析和校验过的目录参数构造路径，不负责目录创建、
 * 文件读写或缓存生命周期管理，避免 ComponentCacheService 重复拼接文件名。
 */
class CacheFileLayout final {
public:
    /**
     * @brief 构造元器件元数据文件路径。
     * @param componentDir 元器件缓存目录。
     */
    static QString metadataFile(const QString& componentDir) {
        return QDir(componentDir).filePath(QStringLiteral("component.json"));
    }

    /**
     * @brief 构造符号 CAD 数据文件路径。
     * @param componentDir 元器件缓存目录。
     */
    static QString symbolFile(const QString& componentDir) {
        return QDir(componentDir).filePath(QStringLiteral("symbol.json"));
    }

    /**
     * @brief 构造封装 CAD 数据文件路径。
     * @param componentDir 元器件缓存目录。
     */
    static QString footprintFile(const QString& componentDir) {
        return QDir(componentDir).filePath(QStringLiteral("footprint.json"));
    }

    /**
     * @brief 构造完整 CAD 数据文件路径。
     * @param componentDir 元器件缓存目录。
     */
    static QString cadDataFile(const QString& componentDir) {
        return QDir(componentDir).filePath(QStringLiteral("cad_data.json"));
    }

    /**
     * @brief 构造预览图文件路径。
     * @param componentDir 元器件缓存目录。
     * @param index 预览图索引。
     */
    static QString previewImageFile(const QString& componentDir, int index) {
        return QDir(componentDir).filePath(QStringLiteral("preview_%1.jpg").arg(index));
    }

    /**
     * @brief 构造数据手册基础路径。
     * @param componentDir 元器件缓存目录。
     * @return 不含 .pdf 或 .html 后缀的基础路径。
     */
    static QString datasheetBase(const QString& componentDir) {
        return QDir(componentDir).filePath(QStringLiteral("datasheet"));
    }

    /**
     * @brief 构造三维模型文件路径。
     * @param model3DDir 三维模型缓存目录。
     * @param uuid 已校验的模型标识。
     * @param extension 已校验的文件扩展名。
     */
    static QString model3DFile(const QString& model3DDir, const QString& uuid, const QString& extension) {
        return QDir(model3DDir).filePath(uuid + QStringLiteral(".") + extension);
    }
};

}  // namespace EasyKiConverter

#endif  // CACHEFILELAYOUT_H
