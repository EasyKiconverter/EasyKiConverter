#pragma once

#include "models/ComponentData.h"

#include <QJsonObject>
#include <QString>

namespace EasyKiConverter {

/**
 * @brief 将元器件基础信息响应解析为 ComponentData。
 * @details 仅负责 API 字段映射，不参与网络请求、缓存写入或异步状态管理。
 */
class ComponentInfoParser {
public:
    /**
     * @brief 解析元器件基础信息。
     * @param componentId 规范化后的元器件编号
     * @param response API 返回的 JSON 对象
     * @return 已填充编号和基础字段的组件数据
     */
    static ComponentData parse(const QString& componentId, const QJsonObject& response);
};

}  // namespace EasyKiConverter
