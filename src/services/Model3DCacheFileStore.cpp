#include "Model3DCacheFileStore.h"

#include "CacheDataValidator.h"
#include "CacheMetadataStore.h"
#include "CacheSafety.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace EasyKiConverter {

/** @brief 读取并校验模型文件，损坏文件会被删除。 */
QByteArray Model3DCacheFileStore::read(const QString& filePath, const QString& extension) {
    if (!QFileInfo::exists(filePath)) {
        return QByteArray();
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return QByteArray();
    }

    const QByteArray data = file.readAll();
    // Windows 不允许删除仍被 QFile 打开的文件，先显式关闭再清理损坏缓存。
    file.close();
    if (CacheDataValidator::isUsableModel3D(data, extension)) {
        return data;
    }

    QString trashError;
    CacheSafety::moveToTrash(filePath, &trashError);
    return QByteArray();
}

/** @brief 校验模型数据并通过临时文件原子提交。 */
bool Model3DCacheFileStore::write(const QString& filePath, const QByteArray& data, const QString& extension) {
    if (!CacheDataValidator::isUsableModel3D(data, extension)) {
        return false;
    }
    return CacheMetadataStore::writeAtomically(filePath, data);
}

/** @brief 校验源文件后复制到目标路径，并确保目标目录存在。 */
bool Model3DCacheFileStore::copy(const QString& sourcePath, const QString& destinationPath, const QString& extension) {
    const QByteArray sourceData = read(sourcePath, extension);
    if (sourceData.isEmpty()) {
        return false;
    }

    const QFileInfo destinationInfo(destinationPath);
    QDir destinationDir = destinationInfo.dir();
    if (!destinationDir.exists() && !destinationDir.mkpath(destinationDir.path())) {
        return false;
    }

    if (QFile::exists(destinationPath)) {
        QFile::remove(destinationPath);
    }
    if (QFile::copy(sourcePath, destinationPath) && QFileInfo(destinationPath).size() > 0) {
        return true;
    }

    QFile::remove(destinationPath);
    return false;
}

}  // namespace EasyKiConverter
