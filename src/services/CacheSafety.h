#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>

#include <functional>

namespace EasyKiConverter {

/**
 * @brief 缓存目录安全策略和所有权校验工具。
 *
 * 所有清理路径都必须先通过根目录标记和条目身份校验。无法证明属于
 * EasyKiConverter 的文件、目录和符号链接一律保留。
 */
class CacheSafety final {
public:
    /** @brief 系统回收站操作的可替换函数类型。 */
    using TrashFunction = std::function<bool(const QString&, QString*)>;

    /** @brief 返回缓存根目录所有权标记文件名。 */
    static QString ownershipMarkerName();

    /** @brief 返回三维模型子目录所有权标记文件名。 */
    static QString model3DMarkerName();

    /**
     * @brief 校验并规范化用户选择的缓存目录。
     * @param path 用户输入或对话框返回的路径。
     * @param normalizedPath 输出规范化后的路径。
     * @param error 输出面向用户的错误信息。
     * @return 路径安全且可作为缓存目录时返回 true。
     */
    static bool validateSelection(const QString& path, QString* normalizedPath, QString* error);

    /**
     * @brief 创建或验证应用拥有的缓存根目录。
     * @param path 缓存根目录。
     * @param error 输出错误信息。
     * @return 根目录可安全使用时返回 true。
     */
    static bool ensureOwnedRoot(const QString& path, QString* error = nullptr);

    /** @brief 判断根目录是否包含有效的 EasyKiConverter 所有权标记。 */
    static bool isOwnedRoot(const QString& path);

    /** @brief 确保应用拥有的三维模型子目录及其标记存在。 */
    static bool ensureOwnedModel3DDirectory(const QString& rootPath, QString* error = nullptr);

    /** @brief 判断元器件缓存目录是否能通过元数据证明归应用所有。 */
    static bool isOwnedComponentDirectory(const QString& rootPath, const QString& path);

    /** @brief 判断三维模型缓存文件是否属于已标记的模型缓存目录。 */
    static bool isOwnedModel3DFile(const QString& rootPath, const QString& path);

    /** @brief 列出所有权可验证的元器件缓存目录。 */
    static QStringList ownedComponentDirectories(const QString& rootPath);

    /** @brief 列出所有权可验证的三维模型文件。 */
    static QStringList ownedModel3DFiles(const QString& rootPath);

    /**
     * @brief 将已验证条目移入系统回收站。
     * @param path 已通过所有权检查的文件或目录。
     * @param error 输出错误信息。
     * @param trash 可选的测试替身；为空时使用 Qt 系统回收站接口。
     * @return 移动成功时返回 true。
     */
    static bool moveToTrash(const QString& path,
                            QString* error = nullptr,
                            const TrashFunction& trash = TrashFunction());

private:
    static bool writeMarker(const QString& path, const QString& format, QString* error);
    static bool hasValidMarker(const QString& path, const QString& format);
    static bool isSafePath(const QString& path, QString* normalizedPath, QString* error);
    static bool isDirectChild(const QString& rootPath, const QString& path);
};

}  // namespace EasyKiConverter
