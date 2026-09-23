#include "ComponentCacheBinaryFileStore.h"

#include "CacheDataValidator.h"
#include "CacheSafety.h"

#include <QFile>
#include <QFileInfo>

namespace EasyKiConverter {

namespace {

/** @brief 读取文件内容并在校验失败时删除损坏文件。 */
QByteArray readValidatedFile(const QString& filePath, bool validateAsPreview) {
    if (!QFileInfo::exists(filePath))
        return QByteArray();

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
        return QByteArray();

    const QByteArray data = file.readAll();
    file.close();
    const bool valid =
        validateAsPreview ? CacheDataValidator::isValidPreviewImage(data) : CacheDataValidator::isValidCadData(data);
    if (valid)
        return data;

    QString trashError;
    CacheSafety::moveToTrash(filePath, &trashError);
    return QByteArray();
}

}  // namespace

/** @brief 读取缓存中的 CAD、符号或封装数据。 */
QByteArray ComponentCacheBinaryFileStore::readCadData(const QString& filePath) {
    return readValidatedFile(filePath, false);
}

/** @brief 读取缓存中的预览图并清理无效图片文件。 */
QByteArray ComponentCacheBinaryFileStore::readPreviewImage(const QString& filePath) {
    return readValidatedFile(filePath, true);
}

}  // namespace EasyKiConverter
