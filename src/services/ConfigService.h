#ifndef CONFIGSERVICE_H
#define CONFIGSERVICE_H

#include <QJsonObject>
#include <QMutex>
#include <QObject>
#include <QString>
#include <QVariantMap>

namespace EasyKiConverter {

/**
 * @brief 配置服务
 *
 * 负责配置管理，使用单例模式
 * 不依赖任UI 组件
 */
class ConfigService : public QObject {
    Q_OBJECT

public:
    /**
     * @brief 获取单例实例
     *
     * @return ConfigService* 配置服务实例
     */
    static ConfigService* instance();

    /**
     * @brief 加载配置
     *
     * @param path 配置文件路径，为空时使用默认路径
     * @return bool 是否成功
     */
    bool loadConfig(const QString& path = QString());

    /**
     * @brief 保存配置
     *
     * @param path 配置文件路径，为空时使用默认路径
     * @return bool 是否成功
     */
    bool saveConfig(const QString& path = QString());

    /**
     * @brief 重置为默认配
     */
    void resetToDefaults();

    /**
     * @brief 开始批量更新（抑制 saveConfig 调用）
     *
     * 与 endBatchUpdate() 配对使用。在批量更新期间，
     * 所有 setter 不会触发 saveConfig()，而是在 endBatchUpdate() 时统一保存一次。
     * 支持嵌套调用。
     */
    void beginBatchUpdate();

    /**
     * @brief 结束批量更新，统一保存一次配置
     */
    void endBatchUpdate();

    // 配置访问方法

    /**
     * @brief 获取输出路径
     *
     * @return QString 输出路径
     */
    QString getOutputPath() const;

    /**
     * @brief 设置输出路径
     *
     * @param path 输出路径
     */
    void setOutputPath(const QString& path);

    /**
     * @brief 获取库名称
     *
     * @return QString 库名称
     */
    QString getLibName() const;

    /**
     * @brief 设置库名称
     *
     * @param name 库名称
     */
    void setLibName(const QString& name);

    /**
     * @brief 是否导出符号
     *
     * @return bool 是否导出符号
     */
    bool getExportSymbol() const;

    /**
     * @brief 设置是否导出符号
     *
     * @param enabled 是否启用
     */
    void setExportSymbol(bool enabled);

    /**
     * @brief 是否导出封装
     *
     * @return bool 是否导出封装
     */
    bool getExportFootprint() const;

    /**
     * @brief 设置是否导出封装
     *
     * @param enabled 是否启用
     */
    void setExportFootprint(bool enabled);

    /**
     * @brief 是否导出3D模型
     *
     * @return bool 是否导出3D模型
     */
    bool getExportModel3D() const;

    /**
     * @brief 设置是否导出3D模型
     *
     * @param enabled 是否启用
     */
    void setExportModel3D(bool enabled);

    /**
     * @brief 获取3D模型格式
     *
     * @return int 3D模型格式(位掩码): 0=NONE, 1=WRL, 2=STEP, 3=BOTH
     */
    int getExportModel3DFormat() const;

    /**
     * @brief 设置3D模型格式
     *
     * @param format 3D模型格式(位掩码): 0=NONE, 1=WRL, 2=STEP, 3=BOTH
     */
    void setExportModel3DFormat(int format);

    /**
     * @brief 获取3D模型路径模式
     *
     * @return int 路径模式: 0=相对路径, 1=绝对路径
     */
    int getExportModel3DPathMode() const;

    /**
     * @brief 设置3D模型路径模式
     *
     * @param mode 路径模式: 0=相对路径, 1=绝对路径
     */
    void setExportModel3DPathMode(int mode);

    /**
     * @brief 是否导出预览图
     *
     * @return bool 是否导出预览图
     */
    bool getExportPreviewImages() const;

    /**
     * @brief 设置是否导出预览图
     *
     * @param enabled 是否启用
     */
    void setExportPreviewImages(bool enabled);

    /**
     * @brief 是否导出手册
     *
     * @return bool 是否导出手册
     */
    bool getExportDatasheet() const;

    /**
     * @brief 设置是否导出手册
     *
     * @param enabled 是否启用
     */
    void setExportDatasheet(bool enabled);

    /**
     * @brief 是否覆盖已存在文
     *
     * @return bool 是否覆盖
     */
    bool getOverwriteExistingFiles() const;

    /**
     * @brief 设置是否覆盖已存在文
     *
     * @param enabled 是否启用
     */
    void setOverwriteExistingFiles(bool enabled);

    /**
     * @brief 是否启用客户端弱网络适配
     *
     * @return bool 是否启用客户端弱网络适配
     */
    bool getWeakNetworkSupport() const;

    /**
     * @brief 设置是否启用客户端弱网络适配
     *
     * @param enabled 是否启用
     */
    void setWeakNetworkSupport(bool enabled);

    /**
     * @brief 获取导出模式
     *
     * @return int 导出模式: 0=追加模式, 1=更新模式
     */
    int getExportMode() const;

    /**
     * @brief 设置导出模式
     *
     * @param mode 导出模式: 0=追加模式, 1=更新模式
     */
    void setExportMode(int mode);

    /**
     * @brief 获取验证阶段的并发数
     *
     * @return int 并发数
     */
    int getValidationConcurrentCount() const;

    /**
     * @brief 获取预览图阶段的并发数
     *
     * @return int 并发数
     */
    int getPreviewConcurrentCount() const;

    /**
     * @brief 是否启用深色模式
     *
     * @return bool 是否启用深色模式
     */
    bool getDarkMode() const;

    /**
     * @brief 设置是否启用深色模式
     *
     * @param enabled 是否启用
     */
    void setDarkMode(bool enabled);

    /**
     * @brief 是否启用调试模式
     *
     * @return bool 是否启用调试模式
     */
    bool getDebugMode() const;

    /**
     * @brief 设置是否启用调试模式
     *
     * @param enabled 是否启用
     * @param save 是否保存到配置文件（默认 true）
     */
    void setDebugMode(bool enabled, bool save = true);

    /**
     * @brief 获取窗口状态值对象
     *
     * @return QVariantMap 包含 x/y/width/height/maximized 字段
     */
    Q_INVOKABLE QVariantMap getWindowState() const;

    /**
     * @brief 设置窗口状态值对象
     *
     * @param state 包含 x/y/width/height/maximized 字段的值对象
     */
    Q_INVOKABLE void setWindowState(const QVariantMap& state);

    /**
     * @brief 获取退出偏好
     *
     * @return QString 退出偏好（"minimize" 或 "exit"，空字符串表示未记住）
     */
    Q_INVOKABLE QString getExitPreference() const;

    /**
     * @brief 设置退出偏好
     *
     * @param preference 退出偏好（"minimize" 或 "exit"）
     */
    Q_INVOKABLE void setExitPreference(const QString& preference);

    /**
     * @brief 获取语言设置
     *
     * @return QString 语言代码（"zh_CN", "en" 等）
     */
    Q_INVOKABLE QString getLanguage() const;

    /**
     * @brief 设置语言
     *
     * @param languageCode 语言代码（"zh_CN", "en" 等）
     */
    Q_INVOKABLE void setLanguage(const QString& languageCode);

    /**
     * @brief 获取磁盘缓存目录
     *
     * @return QString 缓存目录路径
     */
    Q_INVOKABLE QString getCacheDir() const;

    /**
     * @brief 设置磁盘缓存目录
     *
     * @param path 缓存目录路径
     */
    Q_INVOKABLE void setCacheDir(const QString& path);

    /**
     * @brief 获取磁盘缓存大小限制
     *
     * @return int 缓存大小限制（MB）
     */
    Q_INVOKABLE int getDiskCacheLimitMB() const;

    /**
     * @brief 设置磁盘缓存大小限制
     *
     * @param maxSizeMB 缓存大小限制（MB）
     */
    Q_INVOKABLE void setDiskCacheLimitMB(int maxSizeMB);

    /**
     * @brief 获取是否启用启动时自动检查更新。
     * @return 是否启用启动时自动检查更新。
     */
    bool getUpdateAutoCheck() const;

    /**
     * @brief 设置是否启用启动时自动检查更新。
     * @param enabled 是否启用启动时自动检查更新。
     */
    void setUpdateAutoCheck(bool enabled);

    /**
     * @brief 获取更新检查间隔（小时）。
     * @return 更新检查间隔。
     */
    int getUpdateCheckIntervalHours() const;

    /**
     * @brief 设置更新检查间隔（小时）。
     * @param hours 更新检查间隔。
     */
    void setUpdateCheckIntervalHours(int hours);

    /**
     * @brief 获取上次发起更新检查的时间戳。
     * @return 上次发起更新检查的时间戳。
     */
    qint64 getUpdateLastCheckTime() const;

    /**
     * @brief 保存上次发起更新检查的时间戳。
     * @param timestamp 更新检查时间戳。
     */
    void setUpdateLastCheckTime(qint64 timestamp);

    /**
     * @brief 获取上次成功检查的时间戳。
     * @return 上次成功检查的时间戳。
     */
    qint64 getUpdateLastSuccessfulCheckTime() const;

    /**
     * @brief 保存上次成功检查的时间戳。
     * @param timestamp 成功检查时间戳。
     */
    void setUpdateLastSuccessfulCheckTime(qint64 timestamp);

    /**
     * @brief 获取被用户忽略的版本号。
     * @return 被用户忽略的版本号。
     */
    QString getUpdateIgnoredVersion() const;

    /**
     * @brief 保存被用户忽略的版本号。
     * @param version 被用户忽略的版本号。
     */
    void setUpdateIgnoredVersion(const QString& version);

    /**
     * @brief 获取用户选择稍后提醒的版本号。
     * @return 用户选择稍后提醒的版本号。
     */
    QString getUpdateRemindedVersion() const;

    /**
     * @brief 保存用户选择稍后提醒的版本号。
     * @param version 用户选择稍后提醒的版本号。
     */
    void setUpdateRemindedVersion(const QString& version);

    /**
     * @brief 获取最近一次成功获取的 Release JSON 缓存。
     * @return 最近一次成功获取的 Release JSON 缓存。
     */
    QJsonObject getUpdateCachedRelease() const;

    /**
     * @brief 保存最近一次成功获取的 Release JSON 缓存。
     * @param release Release JSON 缓存对象。
     */
    void setUpdateCachedRelease(const QJsonObject& release);

signals:
    /**
     * @brief 配置改变信号
     */
    void configChanged();

public:
    static constexpr int DEFAULT_DISK_CACHE_LIMIT_MB = 5120;  ///< 默认磁盘缓存上限（MB）。
    static constexpr int MAX_DISK_CACHE_LIMIT_MB = 1048576;  ///< 磁盘缓存上限允许的最大值（MB）。

    /**
     * @brief 返回默认缓存目录路径。
     * @return 默认缓存目录路径。
     */
    static QString defaultCacheDir();

private:
    static constexpr int DEFAULT_WINDOW_WIDTH = -1;  ///< 默认窗口宽度，负值表示使用系统默认值。
    static constexpr int DEFAULT_WINDOW_HEIGHT = -1;  ///< 默认窗口高度，负值表示使用系统默认值。
    static constexpr int DEFAULT_WINDOW_X = -9999;  ///< 默认窗口横坐标，负值表示使用系统默认值。
    static constexpr int DEFAULT_WINDOW_Y = -9999;  ///< 默认窗口纵坐标，负值表示使用系统默认值。

    /**
     * @brief 构造函数
     *
     * @param parent 父对象
     */
    ConfigService(QObject* parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~ConfigService() override;

    /**
     * @brief 初始化默认配
     */
    void initializeDefaultConfig();

    /**
     * @brief 获取默认配置文件路径
     *
     * @return QString 配置文件路径
     */
    QString getDefaultConfigPath() const;
    /**
     * @brief 在配置锁已持有时构造窗口状态映射。
     * @return 当前窗口状态映射。
     */
    QVariantMap buildWindowState_locked() const;

private:
    static ConfigService* s_instance;  ///< 配置服务单例实例。
    static QMutex s_mutex;  ///< 保护配置服务单例的互斥量。

    /**
     * @brief 在未处于批量更新时保存配置。
     */
    void saveIfNotBatching();

    QJsonObject m_config;  ///< 当前配置 JSON 对象。
    QString m_configPath;  ///< 配置文件路径。
    mutable QMutex m_configMutex;  ///< 保护配置内容的互斥量。
    int m_batchUpdateDepth = 0;  ///< 当前批量更新嵌套深度。
    bool m_batchDirty = false;  ///< 批量更新期间是否发生配置修改。
};

}  // namespace EasyKiConverter

#endif  // CONFIGSERVICE_H
