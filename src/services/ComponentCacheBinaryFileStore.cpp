#include "ComponentCacheBinaryFileStore.h"

#include "CacheDataValidator.h"
#include "CacheSafety.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace EasyKiConverter {

namespace {

/**
 * @brief 读取文件内容并在校验失败时移入回收站。
 * @param filePath 缓存文件路径。
 * @param validateAsPreview 是否按预览图格式校验。
 * @return 通过所有权和内容校验的文件数据；失败时返回空数组。
 */
QByteArray readValidatedFile(const QString& filePath, bool validateAsPreview) {
    if (!QFileInfo::exists(filePath))
        return QByteArray();

    QDir componentDir = QFileInfo(filePath).dir();  // 文件所属的组件目录。
    if (!componentDir.cdUp() || !CacheSafety::isOwnedComponentFile(componentDir.absolutePath(), filePath))
        return QByteArray();

    QFile file(filePath);  // 待读取的缓存文件。
    if (!file.open(QIODevice::ReadOnly))
        return QByteArray();

    const QByteArray data = file.readAll();  // 文件原始内容。
    file.close();
    const bool valid =  // 内容是否通过对应格式校验。
        validateAsPreview ? CacheDataValidator::isValidPreviewImage(data) : CacheDataValidator::isValidCadData(data);
    if (valid)
        return data;

    QString trashError;  // 回收站操作失败时的错误文本。
    CacheSafety::moveToTrash(filePath, &trashError);
    return QByteArray();
}

}  // namespace

/**
 * @brief 读取缓存中的 CAD、符号或封装数据。
 * @param filePath 缓存文件路径。
 * @return 有效的缓存数据；失败时返回空数组。
 */
QByteArray ComponentCacheBinaryFileStore::readCadData(const QString& filePath) {
    return readValidatedFile(filePath, false);
}

/**
 * @brief 读取缓存中的预览图并清理无效图片文件。
 * @param filePath 预览图文件路径。
 * @return 有效的图片数据；失败时返回空数组。
 */
QByteArray ComponentCacheBinaryFileStore::readPreviewImage(const QString& filePath) {
    return readValidatedFile(filePath, true);
}

}  // namespace EasyKiConverter
