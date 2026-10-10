#include "FootprintData.h"

#include "FootprintDataSerializer.h"

namespace EasyKiConverter {

/**
 * @brief 初始化封装元数据和边界框，其他图元容器使用默认空状态。
 */
FootprintData::FootprintData() : m_info(), m_bbox() {}

/**
 * @brief 将封装数据交给统一序列化器转换为 JSON。
 * @return 封装数据 JSON 对象。
 */
QJsonObject FootprintData::toJson() const {
    return FootprintDataSerializer::toJson(*this);
}

/**
 * @brief 从 JSON 恢复封装数据，并返回字段校验结果。
 * @param json 封装数据 JSON 对象。
 * @return 解析成功时返回 true。
 */
bool FootprintData::fromJson(const QJsonObject& json) {
    return FootprintDataSerializer::fromJson(*this, json);
}

/**
 * @brief 检查封装是否具备名称和至少一个焊盘等最小有效数据。
 * @return 封装数据有效时返回 true。
 */
bool FootprintData::isValid() const {
    // 检查基本信
    if (m_info.name.isEmpty()) {
        return false;
    }

    // 至少要有一个焊
    if (m_pads.isEmpty()) {
        return false;
    }

    return true;
}

/**
 * @brief 按固定顺序检查封装元数据和焊盘字段，并返回首个错误。
 * @return 首个校验错误；无错误时返回空字符串。
 */
QString FootprintData::validate() const {
    if (m_info.name.isEmpty()) {
        return "Footprint name is empty";
    }

    if (m_pads.isEmpty()) {
        return "Footprint must have at least one pad";
    }

    // 检查焊
    for (int i = 0; i < m_pads.size(); ++i) {
        const FootprintPad& pad = m_pads[i];  ///< 当前正在检查的焊盘。
        if (pad.number.isEmpty()) {
            return QString("Pad %1 has empty number").arg(i);
        }
    }

    return QString();  // 返回空字符串表示验证通过
}

/**
 * @brief 清空封装的元数据、所有图元、层信息和导入诊断。
 */
void FootprintData::clear() {
    m_info = FootprintInfo();
    m_bbox = FootprintBBox();
    m_pads.clear();
    m_tracks.clear();
    m_holes.clear();
    m_circles.clear();
    m_rectangles.clear();
    m_arcs.clear();
    m_texts.clear();
    m_solidRegions.clear();
    m_outlines.clear();
    m_layers.clear();
    m_objectVisibilities.clear();
    m_validationErrors.clear();
    m_model3D.clear();
}

}  // namespace EasyKiConverter
