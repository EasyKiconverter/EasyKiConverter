#include "ConfigService.h"

#include "export/ExportProgress.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QStandardPaths>

namespace EasyKiConverter {

/**
 * @brief 返回应用默认的组件缓存目录。
 */
QString ConfigService::defaultCacheDir() {
    return QDir::cleanPath(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/cache");
}

// 静态成员初始化
ConfigService* ConfigService::s_instance = nullptr;
QMutex ConfigService::s_mutex;

ConfigService::ConfigService(QObject* parent) : QObject(parent) {
    // 初始化默认配
    initializeDefaultConfig();
}

ConfigService::~ConfigService() {
    // 自动保存配置
    saveConfig();
}

/**
 * @brief 返回线程安全的配置服务单例。
 */
ConfigService* ConfigService::instance() {
    QMutexLocker locker(&s_mutex);

    if (!s_instance) {
        s_instance = new ConfigService();
    }

    return s_instance;
}

/**
 * @brief 从指定路径加载配置，并补全缺失的默认字段。
 * @param path 配置文件路径，为空时使用默认路径
 */
bool ConfigService::loadConfig(const QString& path) {
    // 解析调用方指定或默认的配置文件路径。
    QString configPath = path.isEmpty() ? getDefaultConfigPath() : path;

    QFile file(configPath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Config file not found, using defaults:" << configPath;
        saveConfig();
        return false;
    }

    // 读取配置文件的原始 JSON 字节内容。
    QByteArray fileData = file.readAll();
    file.close();

    // 保存 JSON 解析错误，供加载失败时诊断。
    QJsonParseError parseError;
    // 保存解析后的配置 JSON 文档。
    QJsonDocument doc = QJsonDocument::fromJson(fileData, &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << "Failed to parse config file:" << parseError.errorString();
        return false;
    }

    QMutexLocker locker(&m_configMutex);

    // 合并配置：只更新文件中存在的字段，保留文件中缺失字段的默认值
    // 保存从文件解析出的配置字段，用于合并到默认配置。
    QJsonObject loadedConfig = doc.object();
    for (auto it = loadedConfig.begin(); it != loadedConfig.end(); ++it) {
        m_config[it.key()] = it.value();
    }

    // 移除 debugMode 字段，因为它不应该从配置文件加载
    // 调试模式完全由命令行参数和环境变量控制
    m_config.remove("debugMode");

    m_configPath = configPath;

    qDebug() << "Config loaded from:" << configPath;
    emit configChanged();

    // 释放锁后保存，确保补全缺失的字段到文件中（但不包含 debugMode）
    locker.unlock();
    saveConfig();

    return true;
}

/**
 * @brief 将当前配置序列化并保存到配置文件。
 * @param path 配置文件路径，为空时使用当前或默认路径
 */
bool ConfigService::saveConfig(const QString& path) {
    // 解析调用方指定、已加载或默认的配置文件路径。
    QString configPath = path.isEmpty() ? (m_configPath.isEmpty() ? getDefaultConfigPath() : m_configPath) : path;

    QMutexLocker locker(&m_configMutex);

    // 创建配置的副本，移除 debugMode 字段
    QJsonObject configToSave = m_config;
    configToSave.remove("debugMode");

    QJsonDocument doc(configToSave);

    QFile file(configPath);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "Failed to open config file for writing:" << configPath;
        return false;
    }

    file.write(doc.toJson());
    file.close();

    qDebug() << "Config saved to:" << configPath;
    return true;
}

/**
 * @brief 重置所有配置为默认值并立即持久化。
 */
void ConfigService::resetToDefaults() {
    QMutexLocker locker(&m_configMutex);
    initializeDefaultConfig();
    emit configChanged();

    // 释放锁后保存
    locker.unlock();
    saveConfig();

    qDebug() << "Config reset to defaults";
}

/**
 * @brief 开始批量配置更新，暂缓重复写入配置文件。
 */
void ConfigService::beginBatchUpdate() {
    QMutexLocker locker(&m_configMutex);
    m_batchUpdateDepth++;
}

/**
 * @brief 结束批量配置更新，并在需要时提交一次保存。
 */
void ConfigService::endBatchUpdate() {
    QMutexLocker locker(&m_configMutex);
    if (m_batchUpdateDepth > 0) {
        m_batchUpdateDepth--;
        if (m_batchUpdateDepth == 0 && m_batchDirty) {
            m_batchDirty = false;
            locker.unlock();
            saveConfig();
        }
    }
}

/**
 * @brief 在非批量更新期间保存当前配置。
 */
void ConfigService::saveIfNotBatching() {
    QMutexLocker locker(&m_configMutex);
    if (m_batchUpdateDepth > 0) {
        m_batchDirty = true;
        return;
    }
    locker.unlock();
    saveConfig();
}

/**
 * @brief 读取导出输出目录。
 */
QString ConfigService::getOutputPath() const {
    QMutexLocker locker(&m_configMutex);
    return m_config["outputPath"].toString(QStandardPaths::writableLocation(QStandardPaths::DesktopLocation));
}

/**
 * @brief 保存导出输出目录。
 * @param path 导出输出目录路径
 */
void ConfigService::setOutputPath(const QString& path) {
    QMutexLocker locker(&m_configMutex);
    m_config["outputPath"] = path;
    emit configChanged();

    // 释放锁后保存（因为 saveConfig 内部也会加锁，避免死锁）
    locker.unlock();
    saveIfNotBatching();
}

/**
 * @brief 读取默认库名称。
 */
QString ConfigService::getLibName() const {
    QMutexLocker locker(&m_configMutex);
    return m_config["libName"].toString("easyeda_convertlib");
}

/**
 * @brief 保存默认库名称。
 * @param name 库名称
 */
void ConfigService::setLibName(const QString& name) {
    QMutexLocker locker(&m_configMutex);
    m_config["libName"] = name;
    emit configChanged();

    // 释放锁后保存
    locker.unlock();
    saveIfNotBatching();
}

/**
 * @brief 读取符号导出开关。
 */
bool ConfigService::getExportSymbol() const {
    QMutexLocker locker(&m_configMutex);
    return m_config["exportSymbol"].toBool(true);
}

/**
 * @brief 保存符号导出开关。
 * @param enabled 是否导出符号库
 */
void ConfigService::setExportSymbol(bool enabled) {
    QMutexLocker locker(&m_configMutex);
    m_config["exportSymbol"] = enabled;
    emit configChanged();

    // 释放锁后保存
    locker.unlock();
    saveIfNotBatching();
}

/**
 * @brief 读取封装导出开关。
 */
bool ConfigService::getExportFootprint() const {
    QMutexLocker locker(&m_configMutex);
    return m_config["exportFootprint"].toBool(true);
}

/**
 * @brief 保存封装导出开关。
 * @param enabled 是否导出封装库
 */
void ConfigService::setExportFootprint(bool enabled) {
    QMutexLocker locker(&m_configMutex);
    m_config["exportFootprint"] = enabled;
    emit configChanged();

    // 释放锁后保存
    locker.unlock();
    saveIfNotBatching();
}

/**
 * @brief 读取三维模型导出开关。
 */
bool ConfigService::getExportModel3D() const {
    QMutexLocker locker(&m_configMutex);
    return m_config["exportModel3D"].toBool(true);
}

/**
 * @brief 保存三维模型导出开关。
 * @param enabled 是否导出三维模型
 */
void ConfigService::setExportModel3D(bool enabled) {
    QMutexLocker locker(&m_configMutex);
    m_config["exportModel3D"] = enabled;
    emit configChanged();

    // 释放锁后保存
    locker.unlock();
    saveIfNotBatching();
}

/**
 * @brief 读取三维模型导出格式。
 */
int ConfigService::getExportModel3DFormat() const {
    QMutexLocker locker(&m_configMutex);
    return m_config["exportModel3DFormat"].toInt(3);  // 默认 3=Both (WRL+STEP)
}

/**
 * @brief 保存三维模型导出格式。
 * @param format 三维模型格式枚举值
 */
void ConfigService::setExportModel3DFormat(int format) {
    QMutexLocker locker(&m_configMutex);
    m_config["exportModel3DFormat"] = format;
    emit configChanged();

    // 释放锁后保存
    locker.unlock();
    saveIfNotBatching();
}

/**
 * @brief 读取三维模型路径模式。
 */
int ConfigService::getExportModel3DPathMode() const {
    QMutexLocker locker(&m_configMutex);
    return m_config["exportModel3DPathMode"].toInt(0);
}

/**
 * @brief 保存三维模型路径模式。
 * @param mode 三维模型路径模式枚举值
 */
void ConfigService::setExportModel3DPathMode(int mode) {
    // 将用户输入的路径模式归一化为受支持的枚举值。
    const int normalized = ExportOptions::normalizePathMode(mode);
    QMutexLocker locker(&m_configMutex);
    m_config["exportModel3DPathMode"] = normalized;
    emit configChanged();

    locker.unlock();
    saveIfNotBatching();
}

/**
 * @brief 读取预览图导出开关。
 */
bool ConfigService::getExportPreviewImages() const {
    QMutexLocker locker(&m_configMutex);
    return m_config["exportPreviewImages"].toBool(false);
}

/**
 * @brief 保存预览图导出开关。
 * @param enabled 是否导出预览图
 */
void ConfigService::setExportPreviewImages(bool enabled) {
    QMutexLocker locker(&m_configMutex);
    m_config["exportPreviewImages"] = enabled;
    emit configChanged();

    // 释放锁后保存
    locker.unlock();
    saveIfNotBatching();
}

/**
 * @brief 读取数据手册导出开关。
 */
bool ConfigService::getExportDatasheet() const {
    QMutexLocker locker(&m_configMutex);
    return m_config["exportDatasheet"].toBool(false);
}

/**
 * @brief 保存数据手册导出开关。
 * @param enabled 是否导出数据手册
 */
void ConfigService::setExportDatasheet(bool enabled) {
    QMutexLocker locker(&m_configMutex);
    m_config["exportDatasheet"] = enabled;
    emit configChanged();

    // 释放锁后保存
    locker.unlock();
    saveIfNotBatching();
}

/**
 * @brief 读取弱网络兼容开关。
 */
bool ConfigService::getWeakNetworkSupport() const {
    QMutexLocker locker(&m_configMutex);
    return m_config["weakNetworkSupport"].toBool(false);
}

/**
 * @brief 保存弱网络兼容开关。
 * @param enabled 是否启用弱网络策略
 */
void ConfigService::setWeakNetworkSupport(bool enabled) {
    QMutexLocker locker(&m_configMutex);
    m_config["weakNetworkSupport"] = enabled;
    emit configChanged();

    locker.unlock();
    saveIfNotBatching();
}

/**
 * @brief 根据网络模式返回元件校验并发数。
 */
int ConfigService::getValidationConcurrentCount() const {
    return getWeakNetworkSupport() ? 5 : 10;
}

/**
 * @brief 根据网络模式返回预览图下载并发数。
 */
int ConfigService::getPreviewConcurrentCount() const {
    return getWeakNetworkSupport() ? 3 : 6;
}

/**
 * @brief 读取是否覆盖已有导出文件。
 */
bool ConfigService::getOverwriteExistingFiles() const {
    QMutexLocker locker(&m_configMutex);
    return m_config["overwriteExistingFiles"].toBool(false);
}

/**
 * @brief 保存是否覆盖已有导出文件。
 * @param enabled 是否覆盖已有文件
 */
void ConfigService::setOverwriteExistingFiles(bool enabled) {
    QMutexLocker locker(&m_configMutex);
    m_config["overwriteExistingFiles"] = enabled;
    emit configChanged();

    // 释放锁后保存
    locker.unlock();
    saveIfNotBatching();
}

/**
 * @brief 读取导出文件的写入模式。
 */
int ConfigService::getExportMode() const {
    QMutexLocker locker(&m_configMutex);
    return m_config["exportMode"].toInt(0);  // 默认 0=追加模式
}

/**
 * @brief 保存导出文件的写入模式。
 * @param mode 导出写入模式枚举值
 */
void ConfigService::setExportMode(int mode) {
    QMutexLocker locker(&m_configMutex);
    m_config["exportMode"] = mode;
    emit configChanged();

    locker.unlock();
    saveIfNotBatching();
}

/**
 * @brief 读取深色主题开关。
 */
bool ConfigService::getDarkMode() const {
    QMutexLocker locker(&m_configMutex);
    return m_config["darkMode"].toBool(false);
}

/**
 * @brief 保存深色主题开关。
 * @param enabled 是否启用深色主题
 */
void ConfigService::setDarkMode(bool enabled) {
    QMutexLocker locker(&m_configMutex);
    m_config["darkMode"] = enabled;
    emit configChanged();

    // 释放锁后保存
    locker.unlock();
    saveIfNotBatching();
}

/**
 * @brief 读取当前调试模式状态。
 */
bool ConfigService::getDebugMode() const {
    QMutexLocker locker(&m_configMutex);
    return m_config["debugMode"].toBool(false);
}

/**
 * @brief 设置调试模式状态，并按需保存非持久化配置。
 * @param enabled 是否启用调试模式
 * @param save 是否立即保存非持久化配置
 */
void ConfigService::setDebugMode(bool enabled, bool save) {
    QMutexLocker locker(&m_configMutex);
    m_config["debugMode"] = enabled;
    emit configChanged();

    // 注意：调试模式不应该保存到配置文件
    // 它完全由命令行参数和环境变量控制
    // 如果 save=true，我们需要从配置中移除 debugMode 字段
    if (save) {
        m_config.remove("debugMode");
        locker.unlock();
        saveConfig();
    }
}

/**
 * @brief 在锁已持有的前提下构造窗口状态快照。
 */
QVariantMap ConfigService::buildWindowState_locked() const {
    return {
        {"x", m_config["windowX"].toInt(DEFAULT_WINDOW_X)},
        {"y", m_config["windowY"].toInt(DEFAULT_WINDOW_Y)},
        {"width", m_config["windowWidth"].toInt(DEFAULT_WINDOW_WIDTH)},
        {"height", m_config["windowHeight"].toInt(DEFAULT_WINDOW_HEIGHT)},
        {"maximized", m_config["windowMaximized"].toBool(false)},
    };
}

/**
 * @brief 读取窗口位置、尺寸和最大化状态。
 */
QVariantMap ConfigService::getWindowState() const {
    QMutexLocker locker(&m_configMutex);
    return buildWindowState_locked();
}

/**
 * @brief 保存窗口位置、尺寸和最大化状态。
 * @param state 窗口状态映射
 */
void ConfigService::setWindowState(const QVariantMap& state) {
    QMutexLocker locker(&m_configMutex);
    // 保存当前配置中的窗口状态，补足调用方未提供的字段。
    const QVariantMap currentState = buildWindowState_locked();

    m_config["windowX"] = state.value("x", currentState.value("x")).toInt();
    m_config["windowY"] = state.value("y", currentState.value("y")).toInt();
    m_config["windowWidth"] = state.value("width", currentState.value("width")).toInt();
    m_config["windowHeight"] = state.value("height", currentState.value("height")).toInt();
    m_config["windowMaximized"] = state.value("maximized", currentState.value("maximized")).toBool();
    emit configChanged();

    // 释放锁后保存
    locker.unlock();
    saveIfNotBatching();
}

/**
 * @brief 读取退出行为偏好。
 */
QString ConfigService::getExitPreference() const {
    QMutexLocker locker(&m_configMutex);
    return m_config["exitPreference"].toString("");
}

/**
 * @brief 保存退出行为偏好。
 * @param preference 退出行为偏好值
 */
void ConfigService::setExitPreference(const QString& preference) {
    QMutexLocker locker(&m_configMutex);
    m_config["exitPreference"] = preference;
    emit configChanged();

    // 释放锁后保存
    locker.unlock();
    saveIfNotBatching();
}

/**
 * @brief 初始化应用全部配置项的默认值。
 */
void ConfigService::initializeDefaultConfig() {
    m_config["outputPath"] = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    m_config["libName"] = "easyeda_convertlib";
    m_config["exportSymbol"] = true;
    m_config["exportFootprint"] = true;
    m_config["exportModel3D"] = true;
    m_config["exportModel3DFormat"] = 3;  // 默认 3=Both (WRL+STEP)
    m_config["exportModel3DPathMode"] = 0;  // 默认 0=相对路径
    m_config["exportPreviewImages"] = false;
    m_config["exportDatasheet"] = false;
    m_config["overwriteExistingFiles"] = false;
    m_config["exportMode"] = 0;  // 默认 0=追加模式
    m_config["weakNetworkSupport"] = false;
    m_config["darkMode"] = false;
    // 注意：debugMode 不在这里设置，因为它不应该保存在配置文件中
    // 它完全由命令行参数和环境变量控制

    // 窗口配置默认值（-1 或 -9999 表示使用默认值）
    m_config["windowWidth"] = DEFAULT_WINDOW_WIDTH;
    m_config["windowHeight"] = DEFAULT_WINDOW_HEIGHT;
    m_config["windowX"] = DEFAULT_WINDOW_X;
    m_config["windowY"] = DEFAULT_WINDOW_Y;
    m_config["windowMaximized"] = false;
    // 退出偏好默认值（空字符串表示未记住）
    m_config["exitPreference"] = "";
    // 语言设置默认值（英文）
    m_config["language"] = "en";
    m_config["cacheDir"] = defaultCacheDir();
    m_config["diskCacheLimitMB"] = DEFAULT_DISK_CACHE_LIMIT_MB;
    m_config["updateAutoCheck"] = false;
    m_config["updateCheckIntervalHours"] = 24;
    m_config["updateLastCheckTime"] = 0;
    m_config["updateLastSuccessfulCheckTime"] = 0;
    m_config["updateIgnoredVersion"] = QString();
    m_config["updateRemindedVersion"] = QString();
    m_config["updateCachedRelease"] = QJsonObject();
}

/**
 * @brief 返回用户配置文件路径，并确保配置目录存在。
 */
QString ConfigService::getDefaultConfigPath() const {
    // 获取应用配置目录路径，用于创建配置文件所在目录。
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir dir(configDir);

    if (!dir.exists()) {
        dir.mkpath(".");
    }

    return configDir + "/config.json";
}

/**
 * @brief 读取启动时自动检查更新的持久化开关。
 */
bool ConfigService::getUpdateAutoCheck() const {
    QMutexLocker locker(&m_configMutex);
    return m_config.value(QStringLiteral("updateAutoCheck")).toBool(false);
}

/**
 * @brief 保存启动时自动检查更新的持久化开关。
 * @param enabled 是否启用自动检查
 */
void ConfigService::setUpdateAutoCheck(bool enabled) {
    QMutexLocker locker(&m_configMutex);
    m_config[QStringLiteral("updateAutoCheck")] = enabled;
    emit configChanged();
    locker.unlock();
    saveIfNotBatching();
}

/**
 * @brief 读取并限制自动检查间隔，避免异常配置造成频繁请求。
 */
int ConfigService::getUpdateCheckIntervalHours() const {
    QMutexLocker locker(&m_configMutex);
    return qBound(1, m_config.value(QStringLiteral("updateCheckIntervalHours")).toInt(24), 168);
}

/**
 * @brief 保存经过边界限制的自动检查间隔。
 * @param hours 自动检查间隔（小时）
 */
void ConfigService::setUpdateCheckIntervalHours(int hours) {
    QMutexLocker locker(&m_configMutex);
    m_config[QStringLiteral("updateCheckIntervalHours")] = qBound(1, hours, 168);
    emit configChanged();
    locker.unlock();
    saveIfNotBatching();
}

/**
 * @brief 读取最近一次发起更新检查的时间戳。
 */
qint64 ConfigService::getUpdateLastCheckTime() const {
    QMutexLocker locker(&m_configMutex);
    return m_config.value(QStringLiteral("updateLastCheckTime")).toVariant().toLongLong();
}

/**
 * @brief 保存最近一次发起更新检查的时间戳。
 * @param timestamp 检查时间戳
 */
void ConfigService::setUpdateLastCheckTime(qint64 timestamp) {
    QMutexLocker locker(&m_configMutex);
    m_config[QStringLiteral("updateLastCheckTime")] = timestamp;
    emit configChanged();
    locker.unlock();
    saveIfNotBatching();
}

/**
 * @brief 读取最近一次成功检查的时间戳。
 */
qint64 ConfigService::getUpdateLastSuccessfulCheckTime() const {
    QMutexLocker locker(&m_configMutex);
    return m_config.value(QStringLiteral("updateLastSuccessfulCheckTime")).toVariant().toLongLong();
}

/**
 * @brief 保存最近一次成功检查的时间戳。
 * @param timestamp 检查时间戳
 */
void ConfigService::setUpdateLastSuccessfulCheckTime(qint64 timestamp) {
    QMutexLocker locker(&m_configMutex);
    m_config[QStringLiteral("updateLastSuccessfulCheckTime")] = timestamp;
    emit configChanged();
    locker.unlock();
    saveIfNotBatching();
}

/**
 * @brief 读取用户明确忽略的版本号。
 */
QString ConfigService::getUpdateIgnoredVersion() const {
    QMutexLocker locker(&m_configMutex);
    return m_config.value(QStringLiteral("updateIgnoredVersion")).toString();
}

/**
 * @brief 保存用户明确忽略的版本号。
 * @param version 被忽略的版本号
 */
void ConfigService::setUpdateIgnoredVersion(const QString& version) {
    QMutexLocker locker(&m_configMutex);
    m_config[QStringLiteral("updateIgnoredVersion")] = version;
    emit configChanged();
    locker.unlock();
    saveIfNotBatching();
}

/**
 * @brief 读取用户选择稍后提醒的版本号。
 */
QString ConfigService::getUpdateRemindedVersion() const {
    QMutexLocker locker(&m_configMutex);
    return m_config.value(QStringLiteral("updateRemindedVersion")).toString();
}

/**
 * @brief 保存用户选择稍后提醒的版本号。
 * @param version 稍后提醒的版本号
 */
void ConfigService::setUpdateRemindedVersion(const QString& version) {
    QMutexLocker locker(&m_configMutex);
    m_config[QStringLiteral("updateRemindedVersion")] = version;
    emit configChanged();
    locker.unlock();
    saveIfNotBatching();
}

/**
 * @brief 读取最近一次成功获取的 Release JSON 缓存。
 */
QJsonObject ConfigService::getUpdateCachedRelease() const {
    QMutexLocker locker(&m_configMutex);
    return m_config.value(QStringLiteral("updateCachedRelease")).toObject();
}

/**
 * @brief 保存最近一次成功获取的 Release JSON 缓存。
 * @param release Release JSON 对象
 */
void ConfigService::setUpdateCachedRelease(const QJsonObject& release) {
    QMutexLocker locker(&m_configMutex);
    m_config[QStringLiteral("updateCachedRelease")] = release;
    emit configChanged();
    locker.unlock();
    saveIfNotBatching();
}

/**
 * @brief 读取界面语言代码。
 */
QString ConfigService::getLanguage() const {
    QMutexLocker locker(&m_configMutex);
    return m_config["language"].toString("en");
}

/**
 * @brief 保存界面语言代码。
 * @param languageCode 语言代码
 */
void ConfigService::setLanguage(const QString& languageCode) {
    QMutexLocker locker(&m_configMutex);
    m_config["language"] = languageCode;
    emit configChanged();

    // 释放锁后保存
    locker.unlock();
    saveIfNotBatching();
}

/**
 * @brief 读取组件缓存目录。
 */
QString ConfigService::getCacheDir() const {
    QMutexLocker locker(&m_configMutex);
    return m_config["cacheDir"].toString(defaultCacheDir());
}

/**
 * @brief 保存并规范化组件缓存目录。
 * @param path 组件缓存目录路径
 */
void ConfigService::setCacheDir(const QString& path) {
    // 规范化用户选择的缓存目录，避免别名路径重复保存。
    const QString normalizedPath = QDir::cleanPath(path);

    QMutexLocker locker(&m_configMutex);
    m_config["cacheDir"] = normalizedPath;
    emit configChanged();

    locker.unlock();
    saveIfNotBatching();
}

/**
 * @brief 读取并限制磁盘缓存容量。
 */
int ConfigService::getDiskCacheLimitMB() const {
    QMutexLocker locker(&m_configMutex);
    return qBound(1, m_config["diskCacheLimitMB"].toInt(DEFAULT_DISK_CACHE_LIMIT_MB), MAX_DISK_CACHE_LIMIT_MB);
}

/**
 * @brief 保存并限制磁盘缓存容量。
 * @param maxSizeMB 磁盘缓存容量上限（MB）
 */
void ConfigService::setDiskCacheLimitMB(int maxSizeMB) {
    QMutexLocker locker(&m_configMutex);
    m_config["diskCacheLimitMB"] = qBound(1, maxSizeMB, MAX_DISK_CACHE_LIMIT_MB);
    emit configChanged();

    locker.unlock();
    saveIfNotBatching();
}

}  // namespace EasyKiConverter

// 考试没过怎么办？别急别急，你先双击你的太阳穴打开你的个人面板，
// 找到你的上一个存档，直接读档，你就回到考试之前了。
// 但是注意啊兄弟们，每天睡觉会自动存档的，而且同时只能存在两个存档，
// 别不小心睡过去了给弄死档了，不然就只能等考试活动返场了
