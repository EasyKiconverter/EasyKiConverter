#include "PathSecurity.h"

#include <QDebug>
#include <QFileInfo>
#include <QRegularExpression>

namespace EasyKiConverter {

/**
 * @brief 校验单个文件名或目录名是否符合跨平台安全规则。
 * @param name 待校验的文件名或目录名。
 * @return 名称符合跨平台安全规则时返回 true。
 */
bool PathSecurity::isValidPathComponent(const QString& name) {
    if (name.isEmpty() || name == QStringLiteral(".") || name == QStringLiteral(".."))
        return false;

    // 路径组件不能包含分隔符或 Windows 保留字符。
    static const QRegularExpression illegalChars(QStringLiteral(R"([\\/:*?"<>|])"));
    if (name.contains(illegalChars))
        return false;

    // 控制字符（包括 NUL）不能安全地出现在文件名中。
    for (const QChar character : name) {
        const ushort codePoint = character.unicode();  // 当前字符的 Unicode 码点。
        if (codePoint < 0x20 || codePoint == 0x7F)
            return false;
    }

    // Windows 会忽略文件名末尾的空格和句点，可能导致路径碰撞。
    if (name.endsWith(QChar(' ')) || name.endsWith(QChar('.')))
        return false;

    // 这些名称即使带扩展名，在 Windows 上仍属于设备名。
    QString deviceName = name;  // 用于检测 Windows 保留设备名的名称主体。
    const qsizetype extensionStart = deviceName.indexOf(QChar('.'));  // 第一个扩展名分隔符位置。
    if (extensionStart >= 0)
        deviceName.truncate(extensionStart);
    static const QStringList reservedNames = {
        QStringLiteral("CON"),  QStringLiteral("PRN"),  QStringLiteral("AUX"),  QStringLiteral("NUL"),
        QStringLiteral("COM1"), QStringLiteral("COM2"), QStringLiteral("COM3"), QStringLiteral("COM4"),
        QStringLiteral("COM5"), QStringLiteral("COM6"), QStringLiteral("COM7"), QStringLiteral("COM8"),
        QStringLiteral("COM9"), QStringLiteral("LPT1"), QStringLiteral("LPT2"), QStringLiteral("LPT3"),
        QStringLiteral("LPT4"), QStringLiteral("LPT5"), QStringLiteral("LPT6"), QStringLiteral("LPT7"),
        QStringLiteral("LPT8"), QStringLiteral("LPT9")};
    if (reservedNames.contains(deviceName, Qt::CaseInsensitive))
        return false;

    // 零宽字符可能造成显示内容与实际路径不一致。
    if (name.contains(QChar(0x200B)) || name.contains(QChar(0x200C)) || name.contains(QChar(0x200D)) ||
        name.contains(QChar(0xFEFF)))
        return false;

    return true;
}

/**
 * @brief 使用规范化字符串前缀检查目标路径是否位于基准目录内。
 * @param fullPath 待检查的目标路径。
 * @param baseDir 允许访问的基准目录。
 * @return 目标路径位于基准目录内时返回 true。
 */
bool PathSecurity::isSafePath(const QString& fullPath, const QString& baseDir) {
    if (fullPath.isEmpty() || baseDir.isEmpty()) {
        qWarning() << "isSafePath: received empty path";
        return false;
    }

    // 1. 获取绝对路径并规范化
    // 注意：canonicalFilePath() 在应用程序退出时可能会崩溃，因为 Qt 内部对象可能已被销毁
    // 使用 QFile::exists() 先检查，避免调用 canonicalFilePath() 在不存在的路径上
    QFileInfo baseInfo(baseDir);
    QFileInfo targetInfo(fullPath);

    // 安全获取绝对路径 - 避免在退出时调用 canonicalFilePath()
    QString absBase;  // 规范化后的基准目录绝对路径。
    QString absTarget;  // 规范化后的目标绝对路径。

    // 检查路径是否存在且可访问
    if (!baseInfo.exists() || !baseInfo.isReadable()) {
        // 路径不存在时使用 cleanPath，这是安全的
        absBase = QDir::cleanPath(baseInfo.absoluteFilePath());
    } else {
        // 路径存在但避免使用 canonicalFilePath，改用 absoluteFilePath + cleanPath
        // canonicalFilePath() 会调用 Qt 内部文件引擎，在退出时可能崩溃
        absBase = QDir::cleanPath(baseInfo.absoluteFilePath());
    }

    if (!targetInfo.exists() || !targetInfo.isReadable()) {
        absTarget = QDir::cleanPath(targetInfo.absoluteFilePath());
    } else {
        absTarget = QDir::cleanPath(targetInfo.absoluteFilePath());
    }

    // 2. 统一分隔符为 '/' (Qt 默认)
    absBase = QDir::fromNativeSeparators(absBase);
    absTarget = QDir::fromNativeSeparators(absTarget);

    // 3. 确保基准路径以 '/' 结尾，用于目录前缀匹配
    if (!absBase.endsWith('/')) {
        absBase += '/';
    }

    // 4. 特殊情况处理：允许目标路径等于基准路径
    // 移除 target 可能存在的末尾斜杠
    QString targetNoSlash = absTarget;
    while (targetNoSlash.endsWith('/'))
        targetNoSlash.chop(1);

    // 如果 target + '/' == base，说明它们代表同一个目录
    if (targetNoSlash + '/' == absBase) {
        return true;
    }
    // 或者直接比较（忽略大小写，适应 Windows）
    if (targetNoSlash.compare(absBase.left(absBase.length() - 1), Qt::CaseInsensitive) == 0) {
        return true;
    }

    // 5. 前缀比较：检查 target 是否以 base 开头
    bool isSafe = absTarget.startsWith(absBase, Qt::CaseInsensitive);

    if (!isSafe) {
        qWarning() << "isSafePath: path is outside base directory";
        qWarning() << "  Target (norm):" << absTarget;
        qWarning() << "  Base   (norm):" << absBase;
        qWarning() << "  Raw Target:" << fullPath;
        qWarning() << "  Raw Base:  " << baseDir;
    }

    return isSafe;
}

/**
 * @brief 解析符号链接后检查目标路径是否安全地位于基准目录内。
 * @param fullPath 待检查的目标路径。
 * @param baseDir 允许访问的基准目录。
 * @return 解析符号链接后目标路径位于基准目录内时返回 true。
 */
bool PathSecurity::isSafePathCanonical(const QString& fullPath, const QString& baseDir) {
    if (fullPath.isEmpty() || baseDir.isEmpty()) {
        qWarning() << "isSafePathCanonical: received empty path";
        return false;
    }

    const QString canonBase = canonicalizePath(baseDir);  // 解析后的基准目录路径。
    const QString canonTarget = canonicalizePath(fullPath);  // 解析后的目标路径。

    if (canonBase.isEmpty() || canonTarget.isEmpty()) {
        qWarning() << "isSafePathCanonical: failed to canonicalize paths";
        return false;
    }

    // 确保基准路径以 '/' 结尾
    QString normalizedBase = canonBase;  // 带有目录分隔符边界的基准路径。
    if (!normalizedBase.endsWith('/')) {
        normalizedBase += '/';
    }

    // 允许目标等于基准
    QString targetNoSlash = canonTarget;  // 移除尾部分隔符后的目标路径。
    while (targetNoSlash.endsWith('/'))
        targetNoSlash.chop(1);

    if (targetNoSlash + '/' == normalizedBase) {
        return true;
    }
    if (targetNoSlash.compare(normalizedBase.left(normalizedBase.length() - 1), Qt::CaseInsensitive) == 0) {
        return true;
    }

    bool isSafe = canonTarget.startsWith(normalizedBase, Qt::CaseInsensitive);  // 目标路径是否通过边界检查。

    if (!isSafe) {
        qWarning() << "isSafePathCanonical: path is outside base directory";
        qWarning() << "  Target (canonical):" << canonTarget;
        qWarning() << "  Base   (canonical):" << normalizedBase;
        qWarning() << "  Raw Target:" << fullPath;
        qWarning() << "  Raw Base:  " << baseDir;
    }

    return isSafe;
}

/**
 * @brief 获取路径的 canonical 形式，并为不存在的路径拼接最近存在父目录。
 * @param path 输入路径。
 * @return 解析后的路径；路径不存在时回退到规范化路径。
 */
QString PathSecurity::canonicalizePath(const QString& path) {
    QFileInfo info(path);

    // 如果路径存在，直接用 canonicalFilePath（解析符号链接）
    if (info.exists()) {
        QString canonical = info.canonicalFilePath();  // 已存在路径解析符号链接后的结果。
        if (!canonical.isEmpty()) {
            return QDir::fromNativeSeparators(canonical);
        }
    }

    // 路径不存在：找到最近存在的父目录，对其 canonicalize，再拼接剩余部分
    QString remaining;  // 最近存在父目录之后仍需拼接的路径片段。
    QString current = QDir::cleanPath(path);  // 当前向上搜索的路径。

    // 逐级向上查找存在的父目录
    while (!QFileInfo::exists(current)) {
        QDir parent = QDir(current);  // 当前路径的父目录对象。
        if (!parent.cdUp()) {
            // 到达根目录仍不存在，回退到 cleanPath
            return QDir::fromNativeSeparators(QDir::cleanPath(path));
        }
        QString childName = QDir(current).dirName();  // 当前路径相对于父目录的名称。
        remaining = remaining.isEmpty() ? childName : (childName + "/" + remaining);
        current = parent.path();
    }

    // current 现在是存在的路径，对其 canonicalize
    QFileInfo currentInfo(current);
    QString canonCurrent = currentInfo.canonicalFilePath();  // 最近存在目录的 canonical 路径。
    if (canonCurrent.isEmpty()) {
        canonCurrent = QDir::cleanPath(current);
    }

    if (remaining.isEmpty()) {
        return QDir::fromNativeSeparators(canonCurrent);
    }
    return QDir::fromNativeSeparators(canonCurrent + "/" + remaining);
}

/**
 * @brief 清洗文件名中的非法字符、控制字符和系统保留名称。
 * @param name 原始文件名。
 * @return 可安全用于输出路径的文件名。
 */
QString PathSecurity::sanitizeFilename(const QString& name) {
    QString safeName = name;  // 清洗过程中的文件名副本。

    // 替换非法字符: \ / : * ? " < > |
    // 使用十六进制转义确保安全: \x5C 是反斜杠, \x22 是双引号
    static const QRegularExpression illegalChars(QStringLiteral("[\\x5C/:*?\\x22<>|]"));
    safeName.replace(illegalChars, "_");

    // 替换控制字符
    static const QRegularExpression controlChars(R"([\x00-\x1f])");
    safeName.replace(controlChars, "");

    // 替换 Unicode 零宽字符 (可能导致显示欺骗)
    // 使用 QChar 移除这些特殊字符
    safeName.remove(QChar(0x200B));  // ZERO WIDTH SPACE
    safeName.remove(QChar(0x200C));  // ZERO WIDTH NON-JOINDER
    safeName.remove(QChar(0x200D));  // ZERO WIDTH JOINER
    safeName.remove(QChar(0xFEFF));  // ZERO WIDTH NO-BREAK SPACE

    safeName = safeName.trimmed();

    if (safeName.isEmpty()) {
        safeName = "unnamed_component";
    }

    // Windows 保留设备名，不能作为普通文件名使用。
    static const QStringList reservedNames = {"CON",  "PRN",  "AUX",  "NUL",  "COM1", "COM2", "COM3", "COM4",
                                              "COM5", "COM6", "COM7", "COM8", "COM9", "LPT1", "LPT2", "LPT3",
                                              "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9"};

    if (reservedNames.contains(safeName, Qt::CaseInsensitive)) {
        safeName += "_safe";
    }

    return safeName;
}

/**
 * @brief 在文件数量上限保护下安全递归删除目录。
 * @param path 待删除的目录路径。
 * @param maxFileCount 允许删除的最大目录项数量。
 * @return 删除成功或目录不存在时返回 true。
 */
bool PathSecurity::safeRemoveRecursively(const QString& path, int maxFileCount) {
    QDir dir(path);
    if (!dir.exists())
        return true;

    int count = countFilesRecursively(path, maxFileCount + 1);  // 包含超限哨兵的目录项数量。
    if (count > maxFileCount) {
        qCritical() << "Security Alert: Recursive delete aborted. File count" << count << "exceeds limit of"
                    << maxFileCount << "in path:" << path;
        return false;
    }

    qDebug() << "Safe delete: Removing directory with" << count << "items:" << path;
    return dir.removeRecursively();
}

/**
 * @brief 递归统计目录项数量，并在达到上限时提前停止。
 * @param path 当前统计目录。
 * @param limit 统计上限。
 * @param currentCount 当前已统计数量。
 * @return 统计后的目录项数量。
 */
int PathSecurity::countFilesRecursively(const QString& path, int limit, int currentCount) {
    if (currentCount >= limit)
        return currentCount;

    QDir dir(path);
    QFileInfoList list = dir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden);  // 当前目录项列表。

    for (const QFileInfo& info : list) {
        currentCount++;
        if (currentCount >= limit)
            return limit;

        if (info.isDir()) {
            if (!info.isSymLink()) {
                currentCount = countFilesRecursively(info.absoluteFilePath(), limit, currentCount);
            } else {
                // 注意：我们跳过符号链接指向的内容，不计入文件总数，也不深入检查
                // removeRecursively 会删除符号链接本身，但不会删除目标
                // 这是预期的安全行为
                currentCount++;
            }
            if (currentCount >= limit)
                return limit;
        } else {
            currentCount++;
        }
    }
    return currentCount;
}

}  // namespace EasyKiConverter
