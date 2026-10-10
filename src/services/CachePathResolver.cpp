#include "CachePathResolver.h"

#include "CacheFileLayout.h"
#include "services/BomParser.h"

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>

namespace EasyKiConverter {

/**
 * @brief 返回指定组件的缓存目录，并兼容旧目录大小写。
 * @param cacheRoot 缓存根目录。
 * @param componentId 元件编号。
 * @return 组件缓存目录；输入非法时为空。
 */
QString CachePathResolver::componentDir(const QString& cacheRoot, const QString& componentId) {
    if (!BomParser::validateId(componentId)) {
        return QString();
    }
    const QString normalizedId = componentId.toUpper();  // 统一大小写后的元件编号。
    const QString normalizedPath = QDir::cleanPath(cacheRoot + "/" + normalizedId);  // 规范化后的默认缓存路径。
    if (QFileInfo::exists(normalizedPath)) {
        return normalizedPath;
    }

    const QDir rootDir(cacheRoot);
    for (const QString& entry : rootDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        if (entry.compare(normalizedId, Qt::CaseInsensitive) == 0) {
            return QDir::cleanPath(rootDir.absoluteFilePath(entry));
        }
    }
    return normalizedPath;
}

/**
 * @brief 返回组件元数据文件路径。
 * @param cacheRoot 缓存根目录。
 * @param componentId 元件编号。
 * @return 组件元数据文件路径；输入非法时为空。
 */
QString CachePathResolver::metadataPath(const QString& cacheRoot, const QString& componentId) {
    const QString dir = componentDir(cacheRoot, componentId);  // 组件缓存目录。
    return dir.isEmpty() ? QString() : CacheFileLayout::metadataFile(dir);
}

/**
 * @brief 返回指定序号的预览图文件路径。
 * @param cacheRoot 缓存根目录。
 * @param componentId 元件编号。
 * @param index 预览图序号。
 * @return 预览图文件路径；输入非法时为空。
 */
QString CachePathResolver::previewImagePath(const QString& cacheRoot, const QString& componentId, int index) {
    const QString dir = componentDir(cacheRoot, componentId);  // 组件缓存目录。
    return dir.isEmpty() ? QString() : CacheFileLayout::previewImageFile(dir, index);
}

/**
 * @brief 返回数据手册的无扩展名基础路径。
 * @param cacheRoot 缓存根目录。
 * @param componentId 元件编号。
 * @return 数据手册基础路径；输入非法时为空。
 */
QString CachePathResolver::datasheetPath(const QString& cacheRoot, const QString& componentId) {
    const QString dir = componentDir(cacheRoot, componentId);  // 组件缓存目录。
    return dir.isEmpty() ? QString() : CacheFileLayout::datasheetBase(dir);
}

/**
 * @brief 校验模型标识和扩展名后返回公共模型缓存文件路径。
 * @param cacheRoot 缓存根目录。
 * @param uuid 模型标识。
 * @param extension 模型扩展名。
 * @return 模型文件路径；标识或扩展名非法时为空。
 */
QString CachePathResolver::model3DPath(const QString& cacheRoot, const QString& uuid, const QString& extension) {
    static const QRegularExpression uuidRe(QStringLiteral("^[A-Za-z0-9_-]+$"));
    if (uuid.isEmpty() || !uuidRe.match(uuid).hasMatch()) {
        return QString();
    }

    const QString normalizedExtension = extension.toLower();  // 统一小写后的模型扩展名。
    static const QRegularExpression extensionRe(QStringLiteral("^[A-Za-z0-9]+$"));
    if (normalizedExtension.isEmpty() || !extensionRe.match(normalizedExtension).hasMatch()) {
        return QString();
    }

    return CacheFileLayout::model3DFile(QDir(cacheRoot).filePath(QStringLiteral("model3d")), uuid, normalizedExtension);
}

}  // namespace EasyKiConverter
