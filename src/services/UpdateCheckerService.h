#ifndef UPDATECHECKERSERVICE_H
#define UPDATECHECKERSERVICE_H

#include "core/network/AsyncNetworkRequest.h"

#include <QJsonObject>
#include <QObject>
#include <QString>

namespace EasyKiConverter {

class INetworkClient;

/**
 * @brief 管理 GitHub Release 更新检查、缓存和用户提醒策略。
 *
 * 网络请求统一通过 INetworkClient 注入，默认使用项目的 NetworkClient 单例；
 * 测试可以注入 MockNetworkClient，避免访问真实 GitHub 服务。
 */
class UpdateCheckerService : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool checking READ checking NOTIFY checkingChanged)
    Q_PROPERTY(bool hasUpdate READ hasUpdate NOTIFY updateStateChanged)
    Q_PROPERTY(bool dismissed READ dismissed NOTIFY updateStateChanged)
    Q_PROPERTY(QString currentVersion READ currentVersion CONSTANT)
    Q_PROPERTY(QString latestVersion READ latestVersion NOTIFY updateStateChanged)
    Q_PROPERTY(QString releaseName READ releaseName NOTIFY updateStateChanged)
    Q_PROPERTY(QString releaseUrl READ releaseUrl NOTIFY updateStateChanged)
    Q_PROPERTY(QString latestReleasePageUrl READ latestReleasePageUrl CONSTANT)
    Q_PROPERTY(QString assetUrl READ assetUrl NOTIFY updateStateChanged)
    Q_PROPERTY(QString error READ error NOTIFY updateStateChanged)
    Q_PROPERTY(bool rateLimited READ rateLimited NOTIFY updateStateChanged)
    Q_PROPERTY(Status status READ status NOTIFY statusChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusChanged)
    Q_PROPERTY(bool autoCheckEnabled READ autoCheckEnabled WRITE setAutoCheckEnabled NOTIFY autoCheckEnabledChanged)
    Q_PROPERTY(qint64 lastCheckTime READ lastCheckTime NOTIFY metadataChanged)
    Q_PROPERTY(qint64 lastSuccessfulCheckTime READ lastSuccessfulCheckTime NOTIFY metadataChanged)

public:
    /**
     * @brief 更新检查状态。
     */
    enum class Status {
        NotChecked,  ///< 尚未执行更新检查。
        Checking,  ///< 正在请求远程版本信息。
        UpdateAvailable,  ///< 检测到可用的新版本。
        UpToDate,  ///< 当前版本已是最新版本。
        Failed,  ///< 更新检查失败。
        Ignored,  ///< 用户已忽略当前版本。
    };
    Q_ENUM(Status)

    /**
     * @brief 创建使用项目默认网络客户端的更新服务。
     * @param parent Qt 对象父级，可为空
     */
    explicit UpdateCheckerService(QObject* parent = nullptr);

    /**
     * @brief 创建可注入测试网络客户端的更新服务。
     * @param networkClient 网络客户端，不由本对象管理其生命周期
     * @param parent Qt 对象父级，可为空
     */
    explicit UpdateCheckerService(INetworkClient& networkClient, QObject* parent = nullptr);
    /**
     * @brief 销毁更新检查服务。
     */
    ~UpdateCheckerService() override;

    /**
     * @brief 返回当前是否正在检查。
     */
    bool checking() const {
        return m_checking;
    }

    /**
     * @brief 返回是否存在高于当前版本的 Release。
     */
    bool hasUpdate() const {
        return m_hasUpdate;
    }

    /**
     * @brief 返回当前 Release 是否已被关闭提醒。
     */
    bool dismissed() const {
        return m_dismissed;
    }

    /**
     * @brief 返回应用当前版本。
     */
    QString currentVersion() const;

    /**
     * @brief 返回最新 Release 版本。
     */
    QString latestVersion() const {
        return m_latestVersion;
    }

    /**
     * @brief 返回最新 Release 名称。
     */
    QString releaseName() const {
        return m_releaseName;
    }

    /**
     * @brief 返回 Release 页面地址。
     */
    QString releaseUrl() const {
        return m_releaseUrl;
    }

    /**
     * @brief 返回官方最新 Release 页面地址，供检查失败时人工访问。
     */
    QString latestReleasePageUrl() const {
        return QStringLiteral("https://github.com/EasyKiconverter/EasyKiConverter/releases/latest");
    }

    /**
     * @brief 返回匹配平台的资产地址。
     */
    QString assetUrl() const {
        return m_assetUrl;
    }

    /**
     * @brief 返回最近一次检查错误。
     */
    QString error() const {
        return m_error;
    }

    /**
     * @brief 返回最近一次检查是否被 GitHub 访问限制。
     */
    bool rateLimited() const {
        return m_rateLimited;
    }

    /**
     * @brief 返回当前检查状态。
     */
    Status status() const {
        return m_status;
    }

    /**
     * @brief 返回供 QML 使用的状态键。
     */
    QString statusText() const;
    /**
     * @brief 返回是否启用启动时自动检查。
     */
    bool autoCheckEnabled() const;
    /**
     * @brief 返回最近一次检查时间。
     */
    qint64 lastCheckTime() const;
    /**
     * @brief 返回最近一次成功检查时间。
     */
    qint64 lastSuccessfulCheckTime() const;

    /**
     * @brief 检查更新。
     * @param force 为 true 时绕过自动检查间隔
     */
    Q_INVOKABLE void checkForUpdates(bool force = true);
    /**
     * @brief 稍后提醒，重启后仍保持当前版本的提醒状态。
     */
    Q_INVOKABLE void dismissUpdate();
    /**
     * @brief 忽略当前 Release，直到出现更高版本。
     */
    Q_INVOKABLE void ignoreUpdate();
    /**
     * @brief 清除当前内存中的关闭状态。
     */
    Q_INVOKABLE void resetDismissed();
    /**
     * @brief 设置是否启用启动时自动检查。
     * @param enabled 是否启用自动检查
     */
    Q_INVOKABLE void setAutoCheckEnabled(bool enabled);
    /**
     * @brief 手动读取最近一次缓存的 Release 信息。
     */
    Q_INVOKABLE void loadCachedRelease();

    /**
     * @brief 规范化并校验 SemVer 字符串。
     * @param version 待规范化的版本字符串
     * @return 规范化版本字符串，无效时为空
     */
    static QString normalizeVersion(const QString& version);
    /**
     * @brief 按 SemVer 规则判断远程版本是否高于当前版本。
     * @param currentVersion 当前版本字符串
     * @param latestVersion 远程最新版本字符串
     * @return 远程版本较新时返回 true
     */
    static bool isRemoteVersionNewer(const QString& currentVersion, const QString& latestVersion);

signals:
    /**
     * @brief 当前检查中状态发生变化。
     */
    void checkingChanged();
    /**
     * @brief 更新结果或提醒状态发生变化。
     */
    void updateStateChanged();
    /**
     * @brief 更新检查状态枚举发生变化。
     */
    void statusChanged();
    /**
     * @brief 自动检查开关发生变化。
     */
    void autoCheckEnabledChanged();
    /**
     * @brief 更新检查时间等元数据发生变化。
     */
    void metadataChanged();

private:
    /**
     * @brief 更新正在检查状态并通知 QML。
     * @param checking 是否正在检查
     */
    void setChecking(bool checking);
    /**
     * @brief 更新内部检查状态并发出状态信号。
     * @param status 新的检查状态
     */
    void setStatus(Status status);
    /**
     * @brief 保存错误信息并切换到失败状态。
     * @param error 可读错误信息
     */
    void setError(const QString& error);
    /**
     * @brief 应用远程或缓存的 Release 信息。
     * @param release Release JSON 对象
     * @param fromCache 是否来自本地缓存
     * @return 信息有效并应用成功时返回 true
     */
    bool applyRelease(const QJsonObject& release, bool fromCache);
    /**
     * @brief 校验 Release JSON 是否包含可用信息。
     * @param release Release JSON 对象
     * @param reason 输出失败原因，可为空
     * @return 校验通过时返回 true
     */
    bool validateRelease(const QJsonObject& release, QString* reason) const;
    /**
     * @brief 更新当前 Release 的版本、名称和下载地址。
     * @param latestVersion 最新版本
     * @param releaseName Release 名称
     * @param releaseUrl Release 页面地址
     * @param assetUrl 当前平台资产地址
     */
    void applyUpdateInfo(const QString& latestVersion,
                         const QString& releaseName,
                         const QString& releaseUrl,
                         const QString& assetUrl);

    INetworkClient* m_networkClient{nullptr};  ///< 注入的网络客户端，不由本对象管理其生命周期。
    AsyncNetworkRequest* m_activeRequest{nullptr};  ///< 当前活动的异步更新请求。
    bool m_checking{false};  ///< 是否正在执行更新检查。
    bool m_hasUpdate{false};  ///< 是否检测到可用更新。
    bool m_dismissed{false};  ///< 用户是否已关闭当前更新提醒。
    QString m_latestVersion;  ///< 最近一次获取到的远程版本。
    QString m_releaseName;  ///< 最近一次获取到的 Release 名称。
    QString m_releaseUrl;  ///< 最近一次获取到的 Release 页面地址。
    QString m_assetUrl;  ///< 当前平台对应的下载资产地址。
    QString m_error;  ///< 最近一次更新检查错误信息。
    bool m_rateLimited{false};  ///< 最近一次请求是否受到 GitHub 访问限制。
    Status m_status{Status::NotChecked};  ///< 当前更新检查状态。
};

}  // namespace EasyKiConverter

#endif  // UPDATECHECKERSERVICE_H
