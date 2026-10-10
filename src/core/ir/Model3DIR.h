#pragma once

/**
 * @file Model3DIR.h
 * @brief 通用 3D 模型中间表示
 *
 * 从 Model3DData 迁移，移除 EasyEDA 特有依赖。
 * 3D 模型数据本身已经是 EDA 无关的（STEP/OBJ 是通用格式）。
 */

#include <QByteArray>
#include <QString>

namespace EasyKiConverter {
namespace IR {

/**
 * @brief 三维向量（用于位移、旋转等）
 */
struct Model3DVec3 {
    double x = 0.0;  ///< X 分量
    double y = 0.0;  ///< Y 分量
    double z = 0.0;  ///< Z 分量

    /**
     * @brief 默认构造零向量。
     */
    Model3DVec3() = default;

    /**
     * @brief 创建指定三轴值的向量。
     * @param x X 分量。
     * @param y Y 分量。
     * @param z Z 分量。
     */
    Model3DVec3(double x, double y, double z) : x(x), y(y), z(z) {}

    /**
     * @brief 比较两个三维向量是否完全相等。
     * @param other 待比较的向量。
     * @return 三个分量均相等时返回 true。
     */
    bool operator==(const Model3DVec3& other) const {
        return x == other.x && y == other.y && z == other.z;
    }

    /**
     * @brief 判断两个三维向量是否不相等。
     * @param other 待比较的向量。
     * @return 任一分量不同时返回 true。
     */
    bool operator!=(const Model3DVec3& other) const {
        return !(*this == other);
    }
};

/**
 * @brief 通用 3D 模型数据
 *
 * 包含 3D 模型的几何数据（STEP/OBJ）和放置参数（位移、旋转）。
 * 不包含任何 EDA 特有的标识符或格式信息。
 */
class Model3DIR {
public:
    /**
     * @brief 创建空的三维模型中间表示。
     */
    Model3DIR() = default;

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
     * @brief 返回模型位移，单位为毫米。
     */
    Model3DVec3 translation() const {
        return m_translation;
    }

    /**
     * @brief 设置模型位移。
     * @param t 三轴毫米位移。
     */
    void setTranslation(const Model3DVec3& t) {
        m_translation = t;
    }

    /**
     * @brief 返回模型旋转角度，单位为度。
     */
    Model3DVec3 rotation() const {
        return m_rotation;
    }

    /**
     * @brief 设置模型旋转角度。
     * @param r 绕三个轴的旋转角度。
     */
    void setRotation(const Model3DVec3& r) {
        m_rotation = r;
    }

    /**
     * @brief 返回 STEP 文件偏移量，单位为毫米。
     */
    Model3DVec3 stepOffsetMm() const {
        return m_stepOffsetMm;
    }

    /**
     * @brief 设置 STEP 文件偏移量。
     * @param offset 三轴毫米偏移量。
     */
    void setStepOffsetMm(const Model3DVec3& offset) {
        m_stepOffsetMm = offset;
    }

    /**
     * @brief 返回原始 OBJ 数据。
     */
    QString rawObj() const {
        return m_rawObj;
    }

    /**
     * @brief 设置原始 OBJ 数据。
     * @param obj OBJ 文本。
     */
    void setRawObj(const QString& obj) {
        m_rawObj = obj;
    }

    /**
     * @brief 返回 STEP 文件数据。
     */
    QByteArray stepData() const {
        return m_step;
    }

    /**
     * @brief 设置 STEP 文件数据。
     * @param data STEP 文件内容。
     */
    void setStepData(const QByteArray& data) {
        m_step = data;
    }

    /**
     * @brief 判断是否包含有效的 STEP 数据。
     * @return STEP 数据非空时返回 true。
     */
    bool hasStepData() const {
        return !m_step.isEmpty();
    }

    /**
     * @brief 判断是否包含有效的 OBJ 数据。
     * @return OBJ 数据非空时返回 true。
     */
    bool hasObjData() const {
        return !m_rawObj.isEmpty();
    }

    /**
     * @brief 判断是否包含任一种有效的三维数据。
     * @return 存在 STEP 或 OBJ 数据时返回 true。
     */
    bool isValid() const {
        return hasStepData() || hasObjData();
    }

    /**
     * @brief 清空模型名称、原始数据和全部变换参数。
     */
    void clear() {
        m_name.clear();
        m_translation = {};
        m_rotation = {};
        m_stepOffsetMm = {};
        m_rawObj.clear();
        m_step.clear();
    }

private:
    QString m_name;  ///< 模型名称。
    Model3DVec3 m_translation;  ///< 模型平移量，单位为毫米。
    Model3DVec3 m_rotation;  ///< 模型旋转量，单位为度。
    Model3DVec3 m_stepOffsetMm;  ///< STEP 模型偏移量，单位为毫米。
    QString m_rawObj;  ///< 原始 OBJ 文本。
    QByteArray m_step;  ///< 原始 STEP 文件数据。
};

}  // namespace IR
}  // namespace EasyKiConverter
