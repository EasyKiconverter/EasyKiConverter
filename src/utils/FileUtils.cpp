#include "FileUtils.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDesktopServices>
#include <QDir>
#include <QStandardPaths>
#include <QUrl>

#if defined(Q_OS_WIN)
#    include <QProcess>
#elif defined(Q_OS_LINUX)
#    include <QProcess>
#elif defined(Q_OS_MAC)
#    include <QProcess>
#endif

namespace EasyKiConverter {

/**
 * @brief 创建文件夹操作辅助对象。
 */
FileUtils::FileUtils(QObject* parent) : QObject(parent) {}

/**
 * @brief 将路径规范化后打开对应目录。
 * @param path 目标路径。
 */
bool FileUtils::openFolder(const QString& path) {
    if (path.isEmpty()) {
        qWarning() << "FileUtils::openFolder: Path is empty";
        return false;
    }

    // 转换为绝对路径
    QString absolutePath = toAbsolutePath(path);  ///< 待打开目录的规范化绝对路径。

    // 检查路径是否存在
    if (!pathExists(absolutePath)) {
        qWarning() << "FileUtils::openFolder: Path does not exist:" << absolutePath;
        return false;
    }

    qDebug() << "FileUtils::openFolder: Opening folder:" << absolutePath;

#if defined(Q_OS_WIN)
    return openFolderOnWindows(absolutePath);
#elif defined(Q_OS_LINUX)
    return openFolderOnLinux(absolutePath);
#elif defined(Q_OS_MAC)
    return openFolderOnMacOS(absolutePath);
#else
    qWarning() << "FileUtils::openFolder: Unsupported platform";
    return false;
#endif
}

/**
 * @brief 将路径转换为规范化绝对路径。
 * @param path 目标路径。
 */
QString FileUtils::toAbsolutePath(const QString& path) {
    if (path.isEmpty()) {
        return "";
    }

    QDir dir(path);  ///< 用于判断和规范化输入路径的目录对象。
    if (dir.isAbsolute()) {
        // 已经是绝对路径，规范化路径分隔符
        return dir.cleanPath(path);
    }

    // 相对路径，转换为绝对路径
    QString absolutePath = dir.absolutePath();  ///< 输入相对路径对应的绝对路径。
    qDebug() << "FileUtils::toAbsolutePath: Converting relative path" << path << "to" << absolutePath;
    return absolutePath;
}

/**
 * @brief 将本地路径转换为文件 URL。
 * @param path 目标路径。
 */
QUrl FileUtils::localPathToFileUrl(const QString& path) const {
    if (path.isEmpty()) {
        return {};
    }
    const QUrl existingUrl(path);  ///< 输入内容按 URL 解析后的结果。
    if (existingUrl.isValid() && existingUrl.scheme().compare(QStringLiteral("file"), Qt::CaseInsensitive) == 0) {
        return existingUrl;
    }
    return QUrl::fromLocalFile(path);
}

/**
 * @brief 将文件 URL 转换为本地路径。
 * @param url 资源 URL。
 */
QString FileUtils::urlToLocalPath(const QUrl& url) const {
    if (!url.isValid() || url.scheme().compare(QStringLiteral("file"), Qt::CaseInsensitive) != 0) {
        return {};
    }
    return url.toLocalFile();
}

/**
 * @brief 检查目录路径是否存在。
 * @param path 目标路径。
 */
bool FileUtils::pathExists(const QString& path) {
    if (path.isEmpty()) {
        return false;
    }

    QDir dir(path);  ///< 输入路径对应的目录对象。
    return dir.exists();
}

/**
 * @brief 使用 Windows Explorer 打开目录。
 * @param absolutePath 待处理的 absolutePath。
 */
bool FileUtils::openFolderOnWindows(const QString& absolutePath) {
#ifdef Q_OS_WIN
    // 使用 explorer 命令直接打开文件夹
    QStringList arguments;  ///< Explorer 命令行参数。
    arguments << QDir::toNativeSeparators(absolutePath);

    qDebug() << "FileUtils::openFolderOnWindows: Running explorer with arguments:" << arguments;

    QProcess process;  ///< 用于启动 Explorer 的进程对象。
    bool success = process.startDetached("explorer", arguments);  ///< Explorer 是否成功启动。

    if (!success) {
        qWarning() << "FileUtils::openFolderOnWindows: Failed to start explorer, trying QDesktopServices fallback";
        // 降级方案：使用 QDesktopServices
        QUrl url = QUrl::fromLocalFile(absolutePath);  ///< 回退方式使用的目录 URL。
        return QDesktopServices::openUrl(url);
    }

    return success;
#else
    Q_UNUSED(absolutePath);
    return false;
#endif
}

/**
 * @brief 使用 Linux xdg-open 打开目录。
 * @param absolutePath 待处理的 absolutePath。
 */
bool FileUtils::openFolderOnLinux(const QString& absolutePath) {
#ifdef Q_OS_LINUX
    // 使用 xdg-open 命令打开文件夹
    QStringList arguments;  ///< xdg-open 命令行参数。
    arguments << absolutePath;

    qDebug() << "FileUtils::openFolderOnLinux: Running xdg-open with arguments:" << arguments;

    QProcess process;  ///< 用于启动 xdg-open 的进程对象。
    bool success = process.startDetached("xdg-open", arguments);  ///< xdg-open 是否成功启动。

    if (!success) {
        qWarning() << "FileUtils::openFolderOnLinux: Failed to start xdg-open, trying QDesktopServices fallback";
        // 降级方案：使用 QDesktopServices
        QUrl url = QUrl::fromLocalFile(absolutePath);  ///< 回退方式使用的目录 URL。
        return QDesktopServices::openUrl(url);
    }

    return success;
#else
    Q_UNUSED(absolutePath);
    return false;
#endif
}

/**
 * @brief 使用 macOS open 命令打开目录。
 * @param absolutePath 待处理的 absolutePath。
 */
bool FileUtils::openFolderOnMacOS(const QString& absolutePath) {
#ifdef Q_OS_MAC
    // 使用 open 命令打开文件夹
    QStringList arguments;  ///< open 命令行参数。
    arguments << absolutePath;

    qDebug() << "FileUtils::openFolderOnMacOS: Running open with arguments:" << arguments;

    QProcess process;  ///< 用于启动 open 的进程对象。
    bool success = process.startDetached("open", arguments);  ///< open 是否成功启动。

    if (!success) {
        qWarning() << "FileUtils::openFolderOnMacOS: Failed to start open, trying QDesktopServices fallback";
        // 降级方案：使用 QDesktopServices
        QUrl url = QUrl::fromLocalFile(absolutePath);  ///< 回退方式使用的目录 URL。
        return QDesktopServices::openUrl(url);
    }

    return success;
#else
    Q_UNUSED(absolutePath);
    return false;
#endif
}

}  // namespace EasyKiConverter
