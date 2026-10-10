#include "Model3DData.h"

#include <QDebug>

namespace EasyKiConverter {

// ==================== Model3DBase ====================

/**
 * @brief 将三维坐标基类序列化为 JSON。
 * @return 坐标 JSON 对象。
 */
QJsonObject Model3DBase::toJson() const {
    QJsonObject json;  ///< 坐标 JSON 输出对象。
    json["x"] = x;
    json["y"] = y;
    json["z"] = z;
    return json;
}

/**
 * @brief 从 JSON 读取有限的坐标值，并保留缺失字段的当前值。
 * @param json 坐标 JSON 对象。
 * @return 解析成功时返回 true。
 */
bool Model3DBase::fromJson(const QJsonObject& json) {
    const auto readCoordinate = [&json](const QString& name, double current, double* value) {  ///< 读取单个坐标字段。
        if (!json.contains(name)) {
            *value = current;
            return true;
        }
        const QJsonValue jsonValue = json.value(name);  ///< 当前坐标字段的 JSON 值。
        if (!jsonValue.isDouble() || !std::isfinite(jsonValue.toDouble()))
            return false;
        *value = jsonValue.toDouble();
        return true;
    };
    double parsedX = x;  ///< 暂存解析后的 X 坐标。
    double parsedY = y;  ///< 暂存解析后的 Y 坐标。
    double parsedZ = z;  ///< 暂存解析后的 Z 坐标。
    if (!readCoordinate(QStringLiteral("x"), x, &parsedX) || !readCoordinate(QStringLiteral("y"), y, &parsedY) ||
        !readCoordinate(QStringLiteral("z"), z, &parsedZ))
        return false;
    x = parsedX;
    y = parsedY;
    z = parsedZ;
    return true;
}

// ==================== Model3DData ====================

// 初始化三维模型的元数据、变换、原始模型和 STEP 内容。
Model3DData::Model3DData()
    // 成员初始化保持为空值，便于后续从 JSON 或导入阶段逐项填充。
    : m_name(), m_uuid(), m_translation(), m_rotation(), m_stepOffsetMm(), m_rawObj(), m_step() {}

/**
 * @brief 序列化三维模型元数据、变换参数和可选的二进制 STEP 数据。
 * @return 三维模型 JSON 对象。
 */
QJsonObject Model3DData::toJson() const {
    QJsonObject json;  ///< 三维模型 JSON 输出对象。

    json["name"] = m_name;
    json["uuid"] = m_uuid;
    json["translation"] = m_translation.toJson();
    json["rotation"] = m_rotation.toJson();
    json["step_offset_mm"] = m_stepOffsetMm.toJson();
    json["raw_obj"] = m_rawObj;

    // STEP 数据Base64 编码存储
    if (!m_step.isEmpty()) {
        json["step"] = QString::fromLatin1(m_step.toBase64());
    }

    return json;
}

/**
 * @brief 以临时变量解析 JSON，全部成功后再原子更新三维模型状态。
 * @param json 三维模型 JSON 对象。
 * @return 解析成功时返回 true。
 */
bool Model3DData::fromJson(const QJsonObject& json) {
    QString parsedName = m_name;  ///< 暂存解析后的模型名称。
    QString parsedUuid = m_uuid;  ///< 暂存解析后的模型 UUID。
    QString parsedRawObj = m_rawObj;  ///< 暂存解析后的 OBJ 原始数据。
    Model3DBase parsedTranslation = m_translation;  ///< 暂存解析后的平移。
    Model3DBase parsedRotation = m_rotation;  ///< 暂存解析后的旋转。
    Model3DBase parsedStepOffsetMm = m_stepOffsetMm;  ///< 暂存解析后的 STEP 偏移。
    QByteArray parsedStep = m_step;  ///< 暂存解析后的 STEP 数据。

    const auto readOptionalString = [&json](const QString& name, QString* value) {  ///< 读取可选字符串字段。
        if (!json.contains(name))
            return true;
        const QJsonValue jsonValue = json.value(name);  ///< 当前字符串字段的 JSON 值。
        if (!jsonValue.isString())
            return false;
        *value = jsonValue.toString();
        return true;
    };
    if (!readOptionalString(QStringLiteral("name"), &parsedName) ||
        !readOptionalString(QStringLiteral("uuid"), &parsedUuid) ||
        !readOptionalString(QStringLiteral("raw_obj"), &parsedRawObj))
        return false;

    const auto readOptionalVector = [&json](const QString& name, Model3DBase* value) {  ///< 读取可选坐标对象。
        if (!json.contains(name))
            return true;
        const QJsonValue jsonValue = json.value(name);  ///< 当前坐标字段的 JSON 值。
        if (!jsonValue.isObject())
            return false;
        return value->fromJson(jsonValue.toObject());
    };
    if (!readOptionalVector(QStringLiteral("translation"), &parsedTranslation)) {
        qWarning() << "Failed to parse 3D model translation";
        return false;
    }

    if (!readOptionalVector(QStringLiteral("rotation"), &parsedRotation)) {
        qWarning() << "Failed to parse 3D model rotation";
        return false;
    }

    if (!readOptionalVector(QStringLiteral("step_offset_mm"), &parsedStepOffsetMm)) {
        qWarning() << "Failed to parse 3D model STEP offset";
        return false;
    }

    // 解析 Base64 编码STEP 数据
    if (json.contains(QStringLiteral("step"))) {
        const QJsonValue jsonValue = json.value(QStringLiteral("step"));  ///< STEP Base64 字段值。
        if (!jsonValue.isString())
            return false;
        parsedStep = QByteArray::fromBase64(jsonValue.toString().toLatin1());
    }

    m_name = parsedName;
    m_uuid = parsedUuid;
    m_rawObj = parsedRawObj;
    m_translation = parsedTranslation;
    m_rotation = parsedRotation;
    m_stepOffsetMm = parsedStepOffsetMm;
    m_step = parsedStep;

    return true;
}

/**
 * @brief 检查三维模型是否至少包含名称或 UUID 标识。
 * @return 三维模型具有有效标识时返回 true。
 */
bool Model3DData::isValid() const {
    // 至少要有名称UUID
    if (m_name.isEmpty() && m_uuid.isEmpty()) {
        return false;
    }

    return true;
}

/**
 * @brief 返回三维模型标识校验的首个错误或空字符串。
 * @return 首个校验错误；无错误时返回空字符串。
 */
QString Model3DData::validate() const {
    if (m_name.isEmpty() && m_uuid.isEmpty()) {
        return "3D model must have either name or UUID";
    }

    return QString();  // 返回空字符串表示验证通过
}

/**
 * @brief 清空三维模型标识、变换、原始 OBJ 和 STEP 数据。
 */
void Model3DData::clear() {
    m_name.clear();
    m_uuid.clear();
    m_translation = Model3DBase();
    m_rotation = Model3DBase();
    m_stepOffsetMm = Model3DBase();
    m_rawObj.clear();
    m_step.clear();
}

}  // namespace EasyKiConverter
