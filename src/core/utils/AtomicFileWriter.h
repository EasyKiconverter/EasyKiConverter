#ifndef ATOMIC_FILE_WRITER_H
#define ATOMIC_FILE_WRITER_H

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QString>

#include <functional>

namespace EasyKiConverter {

class AtomicFileWriter {
public:
    using WriteFunc = std::function<bool(const QString& tempPath)>;  ///< 使用临时路径写入文件内容的回调。

    AtomicFileWriter() = default;
    ~AtomicFileWriter() = default;

    static bool writeAtomically(const QString& tempDir,
                                const QString& finalPath,
                                const QString& suffix,
                                WriteFunc writeFunc);

    static bool writeAtomically(const QString& tempDir,
                                const QString& finalPath,
                                const QString& suffix,
                                const QByteArray& data);

    static bool createDirectory(const QString& path);

    /**
     * @brief 原子拷贝文件（通过临时文件实现）
     * @param sourcePath 源文件路径
     * @param finalPath 目标文件路径
     * @param tempDir 临时目录
     * @return bool 是否拷贝成功
     */
    static bool copyAtomically(const QString& sourcePath, const QString& finalPath, const QString& tempDir);

private:
    static QString generateTempPath(const QString& tempDir, const QString& prefix, const QString& suffix);
};

}  // namespace EasyKiConverter

#endif  // ATOMIC_FILE_WRITER_H
