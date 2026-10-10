#include "LcscProductParser.h"

#include "core/utils/UrlUtils.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace EasyKiConverter {

namespace {

/**
 * @brief 从产品对象中提取并规范化最多三张预览图地址。
 * @param product LCSC 产品 JSON 对象
 * @return 去重并规范化后的预览图地址列表
 */
QStringList extractPreviewImageUrls(const QJsonObject& product) {
    // 保存从产品字段解析出的预览图地址，最终限制为最多三项。
    QStringList imageUrls;
    // 读取 LCSC 使用 <$\> 分隔的原始图片地址字段。
    const QString imagesString = product.value(QStringLiteral("image")).toString();
    if (!imagesString.isEmpty()) {
        imageUrls = imagesString.split(QStringLiteral("<$>"), Qt::SkipEmptyParts);
    }

    while (imageUrls.size() > 3) {
        imageUrls.removeLast();
    }
    return UrlUtils::deduplicateAndNormalizeUrls(imageUrls);
}

/**
 * @brief 从产品、设备信息和属性中提取元件编号。
 * @param product LCSC 产品 JSON 对象
 * @return 规范化为大写的元件编号，未找到时为空
 */
QString extractComponentCode(const QJsonObject& product) {
    // 按兼容性顺序检查产品对象中可能承载元件编号的字段。
    static const QStringList directKeys = {QStringLiteral("component_code"),
                                           QStringLiteral("productCode"),
                                           QStringLiteral("product_code"),
                                           QStringLiteral("productNo"),
                                           QStringLiteral("product_no"),
                                           QStringLiteral("number"),
                                           QStringLiteral("code"),
                                           QStringLiteral("lcscPart"),
                                           QStringLiteral("lcscPartNumber")};

    // 在指定 JSON 对象中查找第一个非空的直接编号字段。
    const auto findValue = [](const QJsonObject& object) {
        for (const QString& key : directKeys) {
            // 读取当前候选字段并去除空白，统一返回大写编号。
            const QString value = object.value(key).toString().trimmed();
            if (!value.isEmpty()) {
                return value.toUpper();
            }
        }
        return QString();
    };

    // 优先从产品顶层字段提取编号。
    const QString directCode = findValue(product);
    if (!directCode.isEmpty()) {
        return directCode;
    }

    // 读取产品中的设备信息对象作为第二级编号来源。
    const QJsonObject deviceInfo = product.value(QStringLiteral("device_info")).toObject();
    // 尝试从设备信息对象的直接字段提取编号。
    const QString deviceCode = findValue(deviceInfo);
    if (!deviceCode.isEmpty()) {
        return deviceCode;
    }

    // 读取设备属性对象作为最后一级编号来源。
    const QJsonObject attributes = deviceInfo.value(QStringLiteral("attributes")).toObject();
    // 属性接口可能使用不同的名称，按已知别名依次匹配。
    static const QStringList attributeKeys = {QStringLiteral("LCSC Part"),
                                              QStringLiteral("LCSC Part #"),
                                              QStringLiteral("LCSC Part Number"),
                                              QStringLiteral("Part Number")};
    for (const QString& key : attributeKeys) {
        // 读取当前属性别名对应的元件编号。
        const QString value = attributes.value(key).toString().trimmed();
        if (!value.isEmpty()) {
            return value.toUpper();
        }
    }
    return QString();
}

/**
 * @brief 在搜索结果中选择与请求编号精确匹配的产品。
 * @param componentId 请求中的元件编号
 * @param products 搜索结果产品数组
 * @return 精确匹配的产品对象，未找到时为空对象
 */
QJsonObject selectProduct(const QString& componentId, const QJsonArray& products) {
    // 统一请求编号格式，避免大小写和首尾空白导致匹配失败。
    const QString normalizedId = componentId.trimmed().toUpper();
    for (const QJsonValue& value : products) {
        // 将当前搜索结果转换为产品对象后进行编号比对。
        const QJsonObject product = value.toObject();
        if (extractComponentCode(product) == normalizedId) {
            return product;
        }
    }
    return QJsonObject();
}

}  // namespace

/**
 * @brief 解析 JSON、精确匹配产品并提取制造商与数据手册字段。
 * @param componentId 请求中的元件编号
 * @param responseJson API 返回的 JSON 字节串
 * @return 匹配成功时返回产品媒体信息，否则返回空值
 */
std::optional<LcscProductInfo> LcscProductParser::parse(const QString& componentId, const QByteArray& responseJson) {
    // 将网络响应解析为 JSON 文档，解析失败时立即返回空结果。
    const QJsonDocument document = QJsonDocument::fromJson(responseJson);
    if (document.isNull()) {
        return std::nullopt;
    }

    // 读取 API 响应中的结果对象和产品数组。
    const QJsonObject result = document.object().value(QStringLiteral("result")).toObject();
    // 保存搜索结果中的产品条目，供编号精确匹配使用。
    const QJsonArray products = result.value(QStringLiteral("productList")).toArray();
    if (products.isEmpty()) {
        return std::nullopt;
    }

    // 从搜索结果中选择编号精确匹配的产品。
    const QJsonObject product = selectProduct(componentId, products);
    if (product.isEmpty()) {
        return std::nullopt;
    }

    // 组装供缓存和导出流程使用的产品媒体信息。
    LcscProductInfo info;
    info.imageUrls = extractPreviewImageUrls(product);
    // 读取产品设备属性中的制造商料号和数据手册地址。
    const QJsonObject attributes =
        product.value(QStringLiteral("device_info")).toObject().value(QStringLiteral("attributes")).toObject();
    info.manufacturerPart = attributes.value(QStringLiteral("Manufacturer Part")).toString();
    info.datasheetUrl = attributes.value(QStringLiteral("Datasheet")).toString();
    return info;
}

}  // namespace EasyKiConverter
