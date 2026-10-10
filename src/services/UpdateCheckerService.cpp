#include "UpdateCheckerService.h"

#include "ConfigService.h"
#include "core/network/INetworkClient.h"
#include "core/network/NetworkClient.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSysInfo>
#include <QUrl>

namespace EasyKiConverter {

namespace {

constexpr auto RELEASES_URL =
    "https://api.github.com/repos/EasyKiconverter/EasyKiConverter/releases/latest";  ///< GitHub 最新 Release 接口地址。

struct SemanticVersion {
    int major = 0;  ///< 主版本号。
    int minor = 0;  ///< 次版本号。
    int patch = 0;  ///< 补丁版本号。
    QStringList prerelease;  ///< 预发布标识列表。
    bool valid = false;  ///< 校验结果。
};

/**
 * @brief 解析并校验更新服务使用的 SemVer 字符串。
 * @param value 待处理的值。
 */
SemanticVersion parseVersion(const QString& value) {
    const QRegularExpression expression(
        QStringLiteral("^v?(\\d+)(?:\\.(\\d+))?(?:\\.(\\d+))?(?:-([0-9A-Za-z-]+(?:\\.[0-9A-Za-z-]+)*))?(?:\\+[0-9A-Za-"
                       "z-]+(?:\\.[0-9A-Za-z-]+)*)?$"));
    const QRegularExpressionMatch match = expression.match(value.trimmed());  ///< 版本正则匹配结果。
    if (!match.hasMatch())
        return {};

    SemanticVersion result;  ///< 结果。
    result.major = match.captured(1).toInt();
    result.minor = match.captured(2).isEmpty() ? 0 : match.captured(2).toInt();
    result.patch = match.captured(3).isEmpty() ? 0 : match.captured(3).toInt();
    result.prerelease = match.captured(4).isEmpty() ? QStringList() : match.captured(4).split('.');
    result.valid = true;
    return result;
}

/**
 * @brief 按 SemVer 规则比较两个已解析的版本。
 * @param left 左侧比较值。
 * @param right 右侧比较值。
 */
int compareVersions(const SemanticVersion& left, const SemanticVersion& right) {
    if (left.major != right.major)
        return left.major < right.major ? -1 : 1;
    if (left.minor != right.minor)
        return left.minor < right.minor ? -1 : 1;
    if (left.patch != right.patch)
        return left.patch < right.patch ? -1 : 1;
    if (left.prerelease.isEmpty() && right.prerelease.isEmpty())
        return 0;
    if (left.prerelease.isEmpty())
        return 1;
    if (right.prerelease.isEmpty())
        return -1;

    const QRegularExpression numeric(QStringLiteral("^[0-9]+$"));
    const int count = qMax(left.prerelease.size(), right.prerelease.size());  ///< 数量。
    for (int index = 0; index < count; ++index) {
        if (index >= left.prerelease.size())
            return -1;
        if (index >= right.prerelease.size())
            return 1;
        const QString& leftPart = left.prerelease.at(index);  ///< 左侧预发布标识。
        const QString& rightPart = right.prerelease.at(index);  ///< 右侧预发布标识。
        const bool leftNumeric = numeric.match(leftPart).hasMatch();  ///< 左侧标识是否为数字。
        const bool rightNumeric = numeric.match(rightPart).hasMatch();  ///< 右侧标识是否为数字。
        if (leftNumeric && rightNumeric && leftPart.toLongLong() != rightPart.toLongLong())
            return leftPart.toLongLong() < rightPart.toLongLong() ? -1 : 1;
        if (leftNumeric != rightNumeric)
            return leftNumeric ? -1 : 1;
        if (leftPart != rightPart)
            return leftPart < rightPart ? -1 : 1;
    }
    return 0;
}

/**
 * @brief 返回当前平台支持的资产架构名称候选。
 */
QStringList assetArchitectureTokens() {
    QStringList preferred;  ///< 首选架构标记列表。
#if defined(Q_OS_WIN)
    preferred << (QSysInfo::currentCpuArchitecture().contains(QStringLiteral("arm"), Qt::CaseInsensitive)
                      ? QStringList{QStringLiteral("arm64")}
                      : QStringList{QStringLiteral("x64"), QStringLiteral("win64"), QStringLiteral("amd64")});
#elif defined(Q_OS_MACOS)
    preferred << (QSysInfo::currentCpuArchitecture().contains(QStringLiteral("arm"), Qt::CaseInsensitive)
                      ? QStringList{QStringLiteral("arm64")}
                      : QStringList{QStringLiteral("intel"), QStringLiteral("x64")});
#elif defined(Q_OS_LINUX)
    preferred << (QSysInfo::currentCpuArchitecture().contains(QStringLiteral("arm"), Qt::CaseInsensitive)
                      ? QStringList{QStringLiteral("aarch64"), QStringLiteral("arm64")}
                      : QStringList{QStringLiteral("x86_64"), QStringLiteral("amd64")});
#endif
    return preferred;
}

/**
 * @brief 判断资产扩展名是否属于当前平台的可分发格式。
 * @param name 名称。
 */
bool isPlatformAssetName(const QString& name) {
#if defined(Q_OS_WIN)
    return name.endsWith(QStringLiteral(".zip")) || name.endsWith(QStringLiteral(".msix"));
#elif defined(Q_OS_MACOS)
    return name.endsWith(QStringLiteral(".dmg"));
#elif defined(Q_OS_LINUX)
    return name.endsWith(QStringLiteral(".appimage"));
#else
    Q_UNUSED(name);
    return false;
#endif
}

/**
 * @brief 从 Release 资产中选择当前平台和架构的下载地址。
 * @param assets 发布资产列表。
 */
QString chooseAsset(const QJsonArray& assets) {
    const QStringList preferred = assetArchitectureTokens();  ///< 首选架构标记列表。

    for (const QJsonValue& value : assets) {
        const QJsonObject asset = value.toObject();  ///< 发布资产 JSON 对象。
        const QString name = asset.value(QStringLiteral("name")).toString().toLower();  ///< 名称。
        const QString url = asset.value(QStringLiteral("browser_download_url")).toString();  ///< URL。
        if (url.isEmpty() || !isPlatformAssetName(name))
            continue;
        for (const QString& token : preferred) {
            if (name.contains(token))
                return url;
        }
    }
    return QString();
}

}  // namespace

/**
 * @brief 创建更新服务并载入最近一次有效的本地缓存。
 */
UpdateCheckerService::UpdateCheckerService(QObject* parent)
    /**
     * @brief 使用项目默认网络客户端保持原有构造方式兼容。
     * @param instance 服务实例。
     */
    : QObject(parent), m_networkClient(&NetworkClient::instance()) {
    loadCachedRelease();
}

/**
 * @brief 创建更新服务并载入最近一次有效的本地缓存。
 */
UpdateCheckerService::UpdateCheckerService(INetworkClient& networkClient, QObject* parent)
    /**
     * @brief 初始化网络客户端并保持可测试的依赖注入边界。
     * @param networkClient 网络客户端。
     */
    : QObject(parent), m_networkClient(&networkClient) {
    loadCachedRelease();
}

/**
 * @brief 释放正在进行的请求，避免服务销毁后继续回调。
 */
UpdateCheckerService::~UpdateCheckerService() {
    if (m_activeRequest) {
        m_activeRequest->cancel();
        m_activeRequest->deleteLater();
        m_activeRequest = nullptr;
    }
}

/**
 * @brief 返回由应用元数据提供并经过规范化的当前版本。
 */
QString UpdateCheckerService::currentVersion() const {
    return normalizeVersion(QCoreApplication::applicationVersion());
}

/**
 * @brief 返回供 QML 稳定判断的状态键。
 */
QString UpdateCheckerService::statusText() const {
    // 将内部枚举映射为不会随语言变化的 QML 状态键。
    switch (m_status) {
        case Status::Checking:
            return QStringLiteral("checking");
        case Status::UpdateAvailable:
            return QStringLiteral("update_available");
        case Status::UpToDate:
            return QStringLiteral("up_to_date");
        case Status::Failed:
            return QStringLiteral("failed");
        case Status::Ignored:
            return QStringLiteral("ignored");
        case Status::NotChecked:
        default:
            return QStringLiteral("not_checked");
    }
}

/**
 * @brief 读取启动自动检查开关。
 */
bool UpdateCheckerService::autoCheckEnabled() const {
    return ConfigService::instance()->getUpdateAutoCheck();
}

/**
 * @brief 读取最近一次检查开始时间。
 */
qint64 UpdateCheckerService::lastCheckTime() const {
    return ConfigService::instance()->getUpdateLastCheckTime();
}

/**
 * @brief 读取最近一次成功检查时间。
 */
qint64 UpdateCheckerService::lastSuccessfulCheckTime() const {
    return ConfigService::instance()->getUpdateLastSuccessfulCheckTime();
}

/**
 * @brief 按自动检查策略发起一次不会重复并发的 Release 请求。
 * @param force 是否强制执行。
 */
void UpdateCheckerService::checkForUpdates(bool force) {
    if (m_checking)
        return;

    ConfigService* config = ConfigService::instance();  ///< 配置服务实例。
    const qint64 now = QDateTime::currentSecsSinceEpoch();  ///< 当前 Unix 时间。
    const qint64 interval = static_cast<qint64>(config->getUpdateCheckIntervalHours()) * 3600;  ///< 更新检查间隔。
    if (!force && !config->getUpdateAutoCheck())
        return;
    if (!force && config->getUpdateLastCheckTime() > 0 && now - config->getUpdateLastCheckTime() < interval)
        return;

    config->setUpdateLastCheckTime(now);
    emit metadataChanged();
    setError(QString());
    setStatus(Status::Checking);
    setChecking(true);

    const RetryPolicy policy =
        RetryPolicy::fromProfile(RequestProfiles::updateCheck(), config->getWeakNetworkSupport());
    m_activeRequest =
        m_networkClient->getAsync(QUrl(QString::fromLatin1(RELEASES_URL)), ResourceType::UpdateCheck, policy);
    connect(m_activeRequest, &AsyncNetworkRequest::finished, this, [this](const NetworkResult& result) {
        if (m_activeRequest) {
            m_activeRequest->deleteLater();
            m_activeRequest = nullptr;
        }
        setChecking(false);
        if (result.wasCancelled) {
            m_rateLimited = false;
            setError(QStringLiteral("Update check was cancelled"));
            setStatus(Status::NotChecked);
            return;
        }

        if (!result.success) {
            loadCachedRelease();
            m_rateLimited = result.statusCode == 403 || result.statusCode == 429 ||
                            result.diagnostic.errorType == NetworkErrorType::Forbidden ||
                            result.diagnostic.errorType == NetworkErrorType::RateLimited ||
                            result.diagnostic.wasRateLimited;
            setError(result.error.isEmpty() ? QStringLiteral("Update check failed") : result.error);
            setStatus(Status::Failed);
            return;
        }

        m_rateLimited = false;
        QJsonParseError parseError;  ///< 错误。
        const QJsonDocument document = QJsonDocument::fromJson(result.data, &parseError);  ///< JSON 文档。
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            setError(QStringLiteral("Invalid release response"));
            setStatus(Status::Failed);
            return;
        }
        applyRelease(document.object(), false);
    });
}

/**
 * @brief 持久化当前版本的稍后提醒选择。
 */
void UpdateCheckerService::dismissUpdate() {
    if (!m_hasUpdate)
        return;
    ConfigService::instance()->setUpdateRemindedVersion(m_latestVersion);
    m_dismissed = true;
    setStatus(Status::Ignored);
    emit updateStateChanged();
}

/**
 * @brief 持久化忽略当前版本的选择，并清除稍后提醒标记。
 */
void UpdateCheckerService::ignoreUpdate() {
    if (m_latestVersion.isEmpty())
        return;
    ConfigService::instance()->setUpdateIgnoredVersion(m_latestVersion);
    dismissUpdate();
    ConfigService::instance()->setUpdateRemindedVersion(QString());
}

/**
 * @brief 清除当前运行实例中的提示关闭状态。
 */
void UpdateCheckerService::resetDismissed() {
    if (!m_dismissed)
        return;
    m_dismissed = false;
    setStatus(m_hasUpdate ? Status::UpdateAvailable : Status::UpToDate);
    emit updateStateChanged();
}

/**
 * @brief 保存启动自动检查开关并通知界面。
 * @param enabled 是否启用。
 */
void UpdateCheckerService::setAutoCheckEnabled(bool enabled) {
    if (autoCheckEnabled() == enabled)
        return;
    ConfigService::instance()->setUpdateAutoCheck(enabled);
    emit autoCheckEnabledChanged();
}

/**
 * @brief 从 ConfigService 读取并应用最近一次有效 Release 缓存。
 */
void UpdateCheckerService::loadCachedRelease() {
    const QJsonObject cached = ConfigService::instance()->getUpdateCachedRelease();  ///< 缓存的发布信息。
    if (!cached.isEmpty())
        applyRelease(cached, true);
}

/**
 * @brief 将带 v 前缀的版本规范化为统一的 SemVer 表示。
 * @param version 版本号。
 */
QString UpdateCheckerService::normalizeVersion(const QString& version) {
    const SemanticVersion parsed = parseVersion(version);  ///< 解析后的版本。
    if (!parsed.valid)
        return QString();
    // 规范化后的 SemVer 文本。
    QString normalized = QStringLiteral("%1.%2.%3").arg(parsed.major).arg(parsed.minor).arg(parsed.patch);
    if (!parsed.prerelease.isEmpty())
        normalized += QStringLiteral("-") + parsed.prerelease.join('.');
    const int buildMarker = version.indexOf('+');  ///< 构建元数据起始位置。
    if (buildMarker >= 0)
        normalized += version.mid(buildMarker).trimmed();
    return normalized;
}

/**
 * @brief 判断远程版本是否严格高于当前版本。
 * @param current 当前版本号。
 * @param latest 最新版本号。
 */
bool UpdateCheckerService::isRemoteVersionNewer(const QString& current, const QString& latest) {
    const SemanticVersion currentVersion = parseVersion(current);  ///< 当前版本。
    const SemanticVersion latestVersion = parseVersion(latest);  ///< 最新版本。
    return currentVersion.valid && latestVersion.valid && compareVersions(currentVersion, latestVersion) < 0;
}

/**
 * @brief 校验 GitHub Release 的稳定性、版本和下载元数据。
 * @param release 发布信息 JSON 对象。
 * @param reason 失败原因。
 */
bool UpdateCheckerService::validateRelease(const QJsonObject& release, QString* reason) const {
    const QString tag = release.value(QStringLiteral("tag_name")).toString();  ///< 发布标签。
    const QString name = release.value(QStringLiteral("name")).toString();  ///< 名称。
    const QUrl url(release.value(QStringLiteral("html_url")).toString());
    if (release.value(QStringLiteral("draft")).toBool(false) ||
        release.value(QStringLiteral("prerelease")).toBool(false)) {
        if (reason)
            *reason = QStringLiteral("Release is not a stable published release");
        return false;
    }
    if (normalizeVersion(tag).isEmpty() || name.isEmpty() || !url.isValid() ||
        url.scheme() != QStringLiteral("https")) {
        if (reason)
            *reason = QStringLiteral("Incomplete release metadata");
        return false;
    }
    if (!release.value(QStringLiteral("assets")).isArray()) {
        if (reason)
            *reason = QStringLiteral("Release assets are missing");
        return false;
    }
    return true;
}

/**
 * @brief 应用远程或缓存 Release，并同步持久化检查结果。
 * @param release 发布信息 JSON 对象。
 * @param fromCache 是否来自缓存。
 */
bool UpdateCheckerService::applyRelease(const QJsonObject& release, bool fromCache) {
    QString reason;  ///< 失败原因。
    if (!validateRelease(release, &reason)) {
        setError(reason);
        setStatus(Status::Failed);
        return false;
    }
    const QString version = normalizeVersion(release.value(QStringLiteral("tag_name")).toString());  ///< 规范化版本号。
    ConfigService* config = ConfigService::instance();  ///< 配置服务实例。
    const QString ignored = config->getUpdateIgnoredVersion();  ///< 被忽略的版本号。
    const QString reminded = config->getUpdateRemindedVersion();  ///< 已提醒的版本号。
    config->beginBatchUpdate();
    if (!fromCache) {
        config->setUpdateCachedRelease(release);
        config->setUpdateLastSuccessfulCheckTime(QDateTime::currentSecsSinceEpoch());
    }
    if (!ignored.isEmpty() && isRemoteVersionNewer(ignored, version))
        config->setUpdateIgnoredVersion(QString());
    if (!reminded.isEmpty() && isRemoteVersionNewer(reminded, version))
        config->setUpdateRemindedVersion(QString());
    config->endBatchUpdate();

    applyUpdateInfo(version,
                    release.value(QStringLiteral("name")).toString(),
                    release.value(QStringLiteral("html_url")).toString(),
                    chooseAsset(release.value(QStringLiteral("assets")).toArray()));
    setError(QString());
    setStatus(m_hasUpdate ? (m_dismissed ? Status::Ignored : Status::UpdateAvailable) : Status::UpToDate);
    emit metadataChanged();
    return true;
}

/**
 * @brief 更新检查中标志并发出对应通知。
 * @param checking 是否正在检查。
 */
void UpdateCheckerService::setChecking(bool checking) {
    if (m_checking == checking)
        return;
    m_checking = checking;
    emit checkingChanged();
}

/**
 * @brief 更新状态枚举并通知 QML。
 * @param status 状态值。
 */
void UpdateCheckerService::setStatus(Status status) {
    if (m_status == status)
        return;
    m_status = status;
    emit statusChanged();
}

/**
 * @brief 将 Release 元数据映射到公开属性和用户提醒策略。
 * @param version 版本号。
 * @param name 名称。
 * @param url 资源 URL。
 * @param asset 发布资产 JSON 对象。
 */
void UpdateCheckerService::applyUpdateInfo(const QString& version,
                                           const QString& name,
                                           const QString& url,
                                           const QString& asset) {
    const QString ignoredVersion = ConfigService::instance()->getUpdateIgnoredVersion();  ///< 被忽略的版本号。
    const QString remindedVersion = ConfigService::instance()->getUpdateRemindedVersion();  ///< 被提醒的版本号。
    const bool oldState = m_hasUpdate;  ///< 旧的更新状态。
    const QString oldVersion = m_latestVersion;  ///< 旧的最新版本号。
    const bool oldDismissed = m_dismissed;  ///< 旧的忽略状态。
    m_latestVersion = version;
    m_releaseName = name;
    m_releaseUrl = url;
    m_assetUrl = asset;
    m_hasUpdate = isRemoteVersionNewer(currentVersion(), version);
    m_dismissed = m_hasUpdate && ((!ignoredVersion.isEmpty() && ignoredVersion == version) ||
                                  (!remindedVersion.isEmpty() && remindedVersion == version));
    if (oldState != m_hasUpdate || oldVersion != m_latestVersion || oldDismissed != m_dismissed)
        emit updateStateChanged();
}

/**
 * @brief 保存诊断信息并触发状态刷新。
 * @param error 错误信息。
 */
void UpdateCheckerService::setError(const QString& error) {
    if (m_error == error)
        return;
    m_error = error;
    emit updateStateChanged();
}

}  // namespace EasyKiConverter
