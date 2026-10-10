/**
 * @file Model3DData.h
 * @brief 定义三维模型的坐标、原始数据和序列化接口。
 */

#ifndef MODEL3DDATA_H
#define MODEL3DDATA_H

#include <QByteArray>
#include <QJsonObject>
#include <QString>

namespace EasyKiConverter {

// ==================== 3D 模型基础坐标 ====================

/**
 * @brief 表示三维模型的毫米坐标或旋转角度。
 *
 * 同一结构既用于平移/偏移坐标，也用于绕三个轴的旋转值；具体语义由所属字段决定。
 */
struct Model3DBase {
    double x;  ///< X 轴坐标或旋转值。
    double y;  ///< Y 轴坐标或旋转值。
    double z;  ///< Z 轴坐标或旋转值。

    /**
     * @brief 创建三个坐标均为零的值。
     */
    Model3DBase() : x(0.0), y(0.0), z(0.0) {}

    /**
     * @brief 创建指定三轴值的坐标。
     * @param x X 轴坐标或旋转值。
     * @param y Y 轴坐标或旋转值。
     * @param z Z 轴坐标或旋转值。
     */
    Model3DBase(double x, double y, double z) : x(x), y(y), z(z) {}

    /**
     * @brief 将坐标值序列化为 JSON 对象。
     */
    QJsonObject toJson() const;

    /**
     * @brief 从 JSON 对象恢复坐标值。
     * @param json 包含坐标字段的 JSON 对象。
     * @return 字段完整且类型有效时返回 true。
     */
    bool fromJson(const QJsonObject& json);
};

// ==================== 3D 模型 ====================

/**
 * @brief 保存封装关联的三维模型数据。
 *
 * 对象同时保留 OBJ、STEP 原始数据及对应的变换信息，供导出器按目标格式选择使用。
 */
class Model3DData {
public:
    /**
     * @brief 创建空的三维模型数据对象。
     */
    Model3DData();

    /**
     * @brief 销毁三维模型数据对象。
     */
    ~Model3DData() = default;

    /**
     * @brief 返回模型名称。
     */
    QString name() const {
        return m_name;
    }

    /**
     * @brief 设置模型名称。
     * @param name 模型名称。
     */
    void setName(const QString& name) {
        m_name = name;
    }

    /**
     * @brief 返回模型 UUID。
     */
    QString uuid() const {
        return m_uuid;
    }

    /**
     * @brief 设置模型 UUID。
     * @param uuid 模型的稳定标识符。
     */
    void setUuid(const QString& uuid) {
        m_uuid = uuid;
    }

    /**
     * @brief 返回模型平移量。
     */
    Model3DBase translation() const {
        return m_translation;
    }

    /**
     * @brief 设置模型平移量。
     * @param translation 模型的三轴平移量。
     */
    void setTranslation(const Model3DBase& translation) {
        m_translation = translation;
    }

    /**
     * @brief 返回模型旋转量。
     */
    Model3DBase rotation() const {
        return m_rotation;
    }

    /**
     * @brief 设置模型旋转量。
     * @param rotation 模型绕三个轴的旋转值。
     */
    void setRotation(const Model3DBase& rotation) {
        m_rotation = rotation;
    }

    /**
     * @brief 返回原始 OBJ 文本。
     */
    QString rawObj() const {
        return m_rawObj;
    }

    /**
     * @brief 设置原始 OBJ 文本。
     * @param rawObj OBJ 模型文本。
     */
    void setRawObj(const QString& rawObj) {
        m_rawObj = rawObj;
    }

    /**
     * @brief 返回原始 STEP 数据。
     */
    QByteArray step() const {
        return m_step;
    }

    /**
     * @brief 设置原始 STEP 数据。
     * @param step STEP 文件内容。
     */
    void setStep(const QByteArray& step) {
        m_step = step;
    }

    /**
     * @brief 返回 STEP 模型的毫米偏移量。
     */
    Model3DBase stepOffsetMm() const {
        return m_stepOffsetMm;
    }

    /**
     * @brief 设置 STEP 模型的毫米偏移量。
     * @param offset STEP 模型的三轴偏移量。
     */
    void setStepOffsetMm(const Model3DBase& offset) {
        m_stepOffsetMm = offset;
    }

    /**
     * @brief 将三维模型数据序列化为 JSON 对象。
     */
    QJsonObject toJson() const;

    /**
     * @brief 从 JSON 对象恢复三维模型数据。
     * @param json 包含模型字段的 JSON 对象。
     * @return 字段有效时返回 true。
     */
    bool fromJson(const QJsonObject& json);

    /**
     * @brief 判断模型是否具备可用的最小数据。
     */
    bool isValid() const;

    /**
     * @brief 返回模型数据的校验错误。
     * @return 校验通过时返回空字符串，否则返回第一条错误说明。
     */
    QString validate() const;

    /**
     * @brief 清空名称、标识符、原始模型和变换数据。
     */
    void clear();

private:
    QString m_name;  ///< 模型显示名称。
    QString m_uuid;  ///< 模型稳定 UUID。
    Model3DBase m_translation;  ///< 模型平移量。
    Model3DBase m_rotation;  ///< 模型旋转量。
    Model3DBase m_stepOffsetMm;  ///< STEP 模型的毫米偏移量。
    QString m_rawObj;  ///< 原始 OBJ 文本。
    QByteArray m_step;  ///< 原始 STEP 二进制数据。
};

}  // namespace EasyKiConverter

#endif  // MODEL3DDATA_H
