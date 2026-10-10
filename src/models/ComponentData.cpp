#include "ComponentData.h"

#include <QDebug>
#include <QJsonDocument>

namespace EasyKiConverter {

ComponentData::ComponentData()
    : m_lcscId()
    , m_name()
    , m_prefix()
    , m_package()
    , m_manufacturer()
    , m_manufacturerPart()
    , m_datasheet()
    , m_datasheetData()
    , m_datasheetFormat("pdf")
    , m_previewImages()
    , m_previewImageData()
    , m_symbolData(nullptr)
    , m_footprintData(nullptr)
    , m_model3DData(nullptr) {}

/**
 * @brief 将组件数据序列化为 JSON 对象。
 * @return 组件数据 JSON 对象。
 */
QJsonObject ComponentData::toJson() const {
    QJsonObject json;  ///< 组件数据的 JSON 输出对象。

    // 基本信息
    json["lcsc_id"] = m_lcscId;
    json["name"] = m_name;
    json["prefix"] = m_prefix;
    json["package"] = m_package;
    json["manufacturer"] = m_manufacturer;
    json["manufacturer_part"] = m_manufacturerPart;
    json["datasheet"] = m_datasheet;

    // 预览图列表
    QJsonArray previewImagesArray;
    for (const QString& imageUrl : m_previewImages) {
        previewImagesArray.append(imageUrl);
    }
    json["preview_images"] = previewImagesArray;

    // 符号数据
    if (m_symbolData) {
        json["symbol"] = m_symbolData->toJson();
    }

    // 封装数据
    if (m_footprintData) {
        json["footprint"] = m_footprintData->toJson();
    }

    // 3D 模型数据
    if (m_model3DData) {
        json["model_3d"] = m_model3DData->toJson();
    }

    return json;
}

/**
 * @brief 从 JSON 对象替换当前组件数据。
 * @param json 组件数据 JSON 对象。
 * @return 解析成功时返回 true。
 */
bool ComponentData::fromJson(const QJsonObject& json) {
    // 反序列化表示替换完整对象，先清理未出现在新 JSON 中的旧数据。
    clear();

    // 读取基本信息
    m_lcscId = json["lcsc_id"].toString();
    m_name = json["name"].toString();
    m_prefix = json["prefix"].toString();
    m_package = json["package"].toString();
    m_manufacturer = json["manufacturer"].toString();
    m_manufacturerPart = json["manufacturer_part"].toString();
    m_datasheet = json["datasheet"].toString();

    // 读取预览图列表
    if (json.contains("preview_images") && json["preview_images"].isArray()) {
        QJsonArray previewImagesArray = json["preview_images"].toArray();  ///< 预览图链接 JSON 数组。
        m_previewImages.clear();
        for (const QJsonValue& value : previewImagesArray) {
            m_previewImages.append(value.toString());
        }
    }

    // 读取符号数据
    if (json.contains("symbol") && json["symbol"].isObject()) {
        if (!m_symbolData) {
            m_symbolData = QSharedPointer<SymbolData>::create();
        }
        if (!m_symbolData->fromJson(json["symbol"].toObject())) {
            qWarning() << "Failed to parse symbol data";
            return false;
        }
    }

    // 读取封装数据
    if (json.contains("footprint") && json["footprint"].isObject()) {
        if (!m_footprintData) {
            m_footprintData = QSharedPointer<FootprintData>::create();
        }
        if (!m_footprintData->fromJson(json["footprint"].toObject())) {
            qWarning() << "Failed to parse footprint data";
            return false;
        }
    }

    // 读取 3D 模型数据
    if (json.contains("model_3d") && json["model_3d"].isObject()) {
        if (!m_model3DData) {
            m_model3DData = QSharedPointer<Model3DData>::create();
        }
        if (!m_model3DData->fromJson(json["model_3d"].toObject())) {
            qWarning() << "Failed to parse 3D model data";
            return false;
        }
    }

    return true;
}

/**
 * @brief 检查组件是否包含有效身份和基础元数据。
 * @return 组件数据有效时返回 true。
 */
bool ComponentData::isValid() const {
    // 验证 LCSC ID 格式（以 C 开头，后面跟数字）
    if (m_lcscId.isEmpty()) {
        return false;
    }

    if (!m_lcscId.startsWith('C', Qt::CaseInsensitive)) {
        return false;
    }

    // 检查后面是否为数字
    bool ok = false;
    QString numberPart = m_lcscId.mid(1);  ///< 去除 C 前缀后的数字部分。
    if (numberPart.length() > 0) {
        numberPart.toInt(&ok);
    }
    if (!ok) {
        return false;
    }

    // 至少要有符号或封装数据
    if (!m_symbolData && !m_footprintData) {
        return false;
    }

    return true;
}

/**
 * @brief 返回组件数据的校验错误。
 * @return 首个校验错误；无错误时返回空字符串。
 */
QString ComponentData::validate() const {
    if (m_lcscId.isEmpty()) {
        return "LCSC ID is empty";
    }

    if (!m_lcscId.startsWith('C', Qt::CaseInsensitive)) {
        return QString("LCSC ID must start with 'C': %1").arg(m_lcscId);
    }

    // 检查后面是否为数字
    bool ok = false;
    QString numberPart = m_lcscId.mid(1);  ///< 去除 C 前缀后的数字部分。
    if (numberPart.length() > 0) {
        numberPart.toInt(&ok);
    }
    if (!ok) {
        return QString("LCSC ID must be in format 'C' followed by numbers: %1").arg(m_lcscId);
    }

    if (!m_symbolData && !m_footprintData) {
        return "Component must have at least symbol or footprint data";
    }

    if (m_symbolData) {
        QString symbolError = m_symbolData->validate();  ///< 符号数据的校验错误。
        if (!symbolError.isEmpty()) {
            return QString("Symbol validation failed: %1").arg(symbolError);
        }
    }

    if (m_footprintData) {
        QString footprintError = m_footprintData->validate();  ///< 封装数据的校验错误。
        if (!footprintError.isEmpty()) {
            return QString("Footprint validation failed: %1").arg(footprintError);
        }
    }

    return QString();  // 返回空字符串表示验证通过
}

/**
 * @brief 清空组件数据及其关联对象。
 */
void ComponentData::clear() {
    m_lcscId.clear();
    m_name.clear();
    m_prefix.clear();
    m_package.clear();
    m_manufacturer.clear();
    m_manufacturerPart.clear();
    m_datasheet.clear();
    m_datasheetData.clear();
    m_datasheetFormat = QStringLiteral("pdf");
    m_previewImages.clear();
    m_previewImageData.clear();
    m_cinfoJsonRaw.clear();
    m_cadJsonRaw.clear();
    m_model3DObjRaw.clear();

    m_symbolData.reset();
    m_footprintData.reset();
    m_model3DData.reset();
}

}  // namespace EasyKiConverter
