#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

#include <optional>

namespace EasyKiConverter {

/**
 * @brief LCSC 产品搜索响应中的可用媒体信息。
 * @details 只保留当前服务需要的制造商料号、数据手册地址和预览图地址，避免业务层依赖原始 JSON 结构。
 */
struct LcscProductInfo {
    QString manufacturerPart;  ///< 制造商料号。
    QString datasheetUrl;  ///< 数据手册下载地址。
    QStringList imageUrls;  ///< 规范化后的预览图地址列表。
};

/**
 * @brief 解析 LCSC 产品搜索响应。
 * @details 仅接受与请求元件编号精确匹配的产品，避免搜索结果顺序变化时误用其他元件的图片。
 */
class LcscProductParser final {
public:
    /**
     * @brief 从产品搜索响应中提取精确匹配的产品信息。
     * @param componentId 请求中的元件编号
     * @param responseJson API 返回的 JSON 字节串
     * @return 匹配成功时返回产品媒体信息，否则返回空值
     */
    static std::optional<LcscProductInfo> parse(const QString& componentId, const QByteArray& responseJson);
};

}  // namespace EasyKiConverter
