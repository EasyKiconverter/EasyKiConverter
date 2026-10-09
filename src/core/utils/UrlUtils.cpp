#include "core/utils/UrlUtils.h"

#include "core/network/INetworkClient.h"

#include <QSet>
#include <QString>
#include <QStringList>
#include <QUrl>

namespace EasyKiConverter::UrlUtils {

namespace {

/**
 * @brief 按资源类型返回允许访问的 host 集合。
 * @param type 资源类型。
 * @return 对应的 host 白名单。
 */
QSet<QString> allowedHostsForType(ResourceType type) {
    /** @brief 按资源类型选择对应的 host 白名单。 */
    switch (type) {
        case ResourceType::ComponentInfo:
        case ResourceType::CadData:
            return {
                QStringLiteral("easyeda.com"),
                QStringLiteral("modules.easyeda.com"),
                QStringLiteral("lceda.cn"),
            };
        case ResourceType::ProductSearch:
            return {
                QStringLiteral("lcsc.com"),
                QStringLiteral("www.lcsc.com"),
                QStringLiteral("so.szlcsc.com"),
                QStringLiteral("item.szlcsc.com"),
                QStringLiteral("pro.lceda.cn"),
            };
        case ResourceType::PreviewImage:
            return {
                QStringLiteral("image.lceda.cn"),
                QStringLiteral("file.elecfans.com"),
                QStringLiteral("alimg.szlcsc.com"),
                QStringLiteral("www.lcsc.com"),
                QStringLiteral("lcsc.com"),
            };
        case ResourceType::Datasheet:
            return {
                QStringLiteral("file.elecfans.com"),
                QStringLiteral("image.lceda.cn"),
                QStringLiteral("lcsc.com"),
                QStringLiteral("www.lcsc.com"),
                QStringLiteral("item.szlcsc.com"),
                QStringLiteral("atta.szlcsc.com"),
                QStringLiteral("www.ti.com"),
                QStringLiteral("www.everlighteurope.com"),
            };
        case ResourceType::Model3DObj:
        case ResourceType::Model3DStep:
            return {
                QStringLiteral("modules.easyeda.com"),
                QStringLiteral("easyeda.com"),
                QStringLiteral("lceda.cn"),
                QStringLiteral("file.elecfans.com"),
            };
        case ResourceType::UpdateCheck:
            return {
                QStringLiteral("github.com"),
                QStringLiteral("api.github.com"),
            };
        case ResourceType::WorkerRequest:
            // WorkerRequest 是通用请求，允许所有已知 host
            return {
                QStringLiteral("easyeda.com"),
                QStringLiteral("modules.easyeda.com"),
                QStringLiteral("lceda.cn"),
                QStringLiteral("lcsc.com"),
                QStringLiteral("www.lcsc.com"),
                QStringLiteral("so.szlcsc.com"),
                QStringLiteral("item.szlcsc.com"),
                QStringLiteral("pro.lceda.cn"),
                QStringLiteral("image.lceda.cn"),
                QStringLiteral("file.elecfans.com"),
                QStringLiteral("alimg.szlcsc.com"),
                QStringLiteral("github.com"),
                QStringLiteral("api.github.com"),
            };
        case ResourceType::Unknown:
        default:
            return {};
    }
}

/**
 * @brief 检查 host 是否匹配允许列表。
 * @param host 待检查的 host。
 * @param allowedHosts 允许访问的 host 集合。
 * @return 精确匹配或合法子域名匹配时返回 true。
 */
bool isHostAllowed(const QString& host, const QSet<QString>& allowedHosts) {
    if (host.isEmpty()) {
        return false;
    }

    // 先执行完整 host 的精确匹配。
    if (allowedHosts.contains(host)) {
        return true;
    }

    // 再检查 host 是否为允许 host 的子域名。
    for (const QString& allowed : allowedHosts) {
        if (host.endsWith(QLatin1Char('.') + allowed)) {
            return true;
        }
    }

    return false;
}

}  // namespace

/**
 * @brief 规范化 EasyEDA 预览图 URL。
 * @param imageUrl 输入的原始图片 URL。
 * @return 可供网络客户端使用的 HTTPS URL。
 */
QString normalizePreviewImageUrl(const QString& imageUrl) {
    QString normalizedUrl = imageUrl.trimmed();  // 待规范化的 URL 副本。
    if (normalizedUrl.isEmpty()) {
        return QString();
    }

    if (normalizedUrl.startsWith(QStringLiteral("//"))) {
        normalizedUrl.prepend(QStringLiteral("https:"));
    } else if (normalizedUrl.startsWith(QStringLiteral("/image.lceda.cn/")) ||
               normalizedUrl.startsWith(QStringLiteral("/file.elecfans.com/")) ||
               normalizedUrl.startsWith(QStringLiteral("/www.lcsc.com/"))) {
        normalizedUrl.remove(0, 1);
        normalizedUrl.prepend(QStringLiteral("https://"));
    } else if (normalizedUrl.startsWith(QStringLiteral("/web1/")) ||
               normalizedUrl.startsWith(QStringLiteral("/M00/"))) {
        normalizedUrl.remove(0, 1);
        normalizedUrl.prepend(QStringLiteral("https://file.elecfans.com/"));
    } else if (normalizedUrl.startsWith('/')) {
        normalizedUrl.remove(0, 1);
        normalizedUrl.prepend(QStringLiteral("https://image.lceda.cn/"));
    } else if (normalizedUrl.contains("alimg.szlcsc.com")) {
        if (!normalizedUrl.startsWith("http")) {
            normalizedUrl = "https://" + normalizedUrl;
        }
    } else if (!normalizedUrl.contains("://")) {
        normalizedUrl = "https://image.lceda.cn/" + normalizedUrl;
    }

    normalizedUrl.replace(QStringLiteral("https://image.lceda.cn//image.lceda.cn/"),
                          QStringLiteral("https://image.lceda.cn/"));
    normalizedUrl.replace(QStringLiteral("http://image.lceda.cn//image.lceda.cn/"),
                          QStringLiteral("https://image.lceda.cn/"));
    normalizedUrl.replace(QStringLiteral("https://image.lceda.cn/image.lceda.cn/"),
                          QStringLiteral("https://image.lceda.cn/"));

    return normalizedUrl;
}

/**
 * @brief 规范化并去除重复的预览图 URL。
 * @param imageUrls 原始图片 URL 列表。
 * @return 去重后的规范化 URL 列表。
 */
QStringList deduplicateAndNormalizeUrls(const QStringList& imageUrls) {
    QStringList normalizedUrls;  // 规范化后的去重结果。
    for (const QString& imageUrl : imageUrls) {  // 依次处理原始 URL。
        const QString normalizedUrl = normalizePreviewImageUrl(imageUrl);  // 当前 URL 的规范化结果。
        if (!normalizedUrl.isEmpty() && !normalizedUrls.contains(normalizedUrl)) {
            normalizedUrls.append(normalizedUrl);
        }
    }
    return normalizedUrls;
}

/**
 * @brief 按资源类型验证 URL 是否属于允许的 HTTPS host。
 * @param url 待验证的 URL。
 * @param resourceType URL 对应的资源类型。
 * @param errorMessage 可选的错误信息输出。
 * @return URL 合法且 host 在白名单内时返回 true。
 */
bool isAllowedUrl(const QUrl& url, ResourceType resourceType, QString* errorMessage) {
    if (!url.isValid()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Invalid URL: %1").arg(url.toString());
        }
        return false;
    }

    // 仅允许 https scheme
    const QString scheme = url.scheme().toLower();  // 规范化后的协议名。
    if (scheme != QStringLiteral("https")) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("URL scheme must be https, got: %1").arg(scheme);
        }
        return false;
    }

    const QString host = url.host().toLower();  // 规范化后的 host。

    // Unknown 类型：仅允许已知域名（拒绝未知 host）
    if (resourceType == ResourceType::Unknown) {
        static const QSet<QString> allKnown =  // 所有资源类型合并后的 host 白名单。
            allowedHostsForType(ResourceType::ComponentInfo) + allowedHostsForType(ResourceType::ProductSearch) +
            allowedHostsForType(ResourceType::PreviewImage) + allowedHostsForType(ResourceType::Datasheet) +
            allowedHostsForType(ResourceType::Model3DObj) + allowedHostsForType(ResourceType::UpdateCheck) +
            allowedHostsForType(ResourceType::WorkerRequest);
        if (!isHostAllowed(host, allKnown)) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("URL host '%1' is not in any known allowlist").arg(host);
            }
            return false;
        }
        return true;
    }
    const QSet<QString> allowed = allowedHostsForType(resourceType);  // 当前资源类型的 host 白名单。

    if (allowed.isEmpty()) {
        // 空白名单 = 允许所有（向后兼容）
        return true;
    }

    if (!isHostAllowed(host, allowed)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("URL host '%1' is not allowed for resource type %2")
                                .arg(host)
                                .arg(static_cast<int>(resourceType));
        }
        return false;
    }

    return true;
}

}  // namespace EasyKiConverter::UrlUtils
