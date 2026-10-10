#include "Model3DCacheFileStore.h"

#include "CacheDataValidator.h"
#include "CacheMetadataStore.h"
#include "CacheSafety.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace EasyKiConverter {

/**
 * @brief 读取并校验模型文件，损坏文件会被移入回收站。
 * @param filePath 模型缓存文件路径。
 * @param extension 模型文件扩展名。
 * @return 可用模型数据，文件不存在或校验失败时返回空字节。
 */
QByteArray Model3DCacheFileStore::read(const QString& filePath, const QString& extension) {
    if (!QFileInfo::exists(filePath)) {
        return QByteArray();
    }

    QDir modelDir = QFileInfo(filePath).dir();  // 模型文件所在的组件缓存目录。
    if (!modelDir.cdUp() || !CacheSafety::isOwnedModel3DFile(modelDir.absolutePath(), filePath)) {
        return QByteArray();
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return QByteArray();
    }

    const QByteArray data = file.readAll();  // 模型文件原始字节。
    // Windows 不允许删除仍被 QFile 打开的文件，先显式关闭再清理损坏缓存。
    file.close();
    if (CacheDataValidator::isUsableModel3D(data, extension)) {
        return data;
    }

    QString trashError;  // 回收站操作失败时的诊断信息。
    CacheSafety::moveToTrash(filePath, &trashError);
    return QByteArray();
}

/**
 * @brief 校验模型数据并通过临时文件原子提交。
 * @param filePath 模型缓存文件路径。
 * @param data 待写入的模型数据。
 * @param extension 模型文件扩展名。
 * @return 校验和写入均成功时返回 true。
 */
bool Model3DCacheFileStore::write(const QString& filePath, const QByteArray& data, const QString& extension) {
    if (!CacheDataValidator::isUsableModel3D(data, extension)) {
        return false;
    }
    return CacheMetadataStore::writeAtomically(filePath, data);
}

/**
 * @brief 校验源文件后复制到目标路径，并确保目标目录存在。
 * @param sourcePath 源模型文件路径。
 * @param destinationPath 目标模型文件路径。
 * @param extension 模型文件扩展名。
 * @return 复制并完成目标文件校验时返回 true。
 */
bool Model3DCacheFileStore::copy(const QString& sourcePath, const QString& destinationPath, const QString& extension) {
    const QByteArray sourceData = read(sourcePath, extension);  // 经校验的源模型数据。
    if (sourceData.isEmpty()) {
        return false;
    }

    const QFileInfo destinationInfo(destinationPath);  // 目标文件信息。
    QDir destinationDir = destinationInfo.dir();  // 目标文件所在目录。
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
