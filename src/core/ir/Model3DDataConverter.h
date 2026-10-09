#pragma once

/**
 * @file Model3DDataConverter.h
 * @brief Model3DData -> Model3DIR 转换桥接
 *
 * 提供从旧模型 Model3DData 到 IR Model3DIR 的转换函数。
 * 3D 模型数据基本是 EDA 无关的，转换主要是字段名映射。
 */

#include "IRTypes.h"
#include "Model3DIR.h"
#include "models/Model3DData.h"

namespace EasyKiConverter {
namespace IR {

/**
 * @brief 向后兼容别名，新代码请使用 IR::EASYEDA_PX_TO_MM
 */
constexpr double MODEL3D_PX_TO_MM = EASYEDA_PX_TO_MM;  ///< 3D 模型 EasyEDA 像素单位到毫米的换算系数。

/**
 * @brief Model3DData -> Model3DIR 转换
 * @param data 旧模型的 3D 模型数据
 * @return IR 的 3D 模型数据
 *
 * @note translation 的 xyz 为 EasyEDA 原始单位，需要转换为 mm。
 *       rotation 为角度（度），无需转换。
 *       stepOffsetMm 已经是 mm（名称中标注），无需转换。
 */
inline Model3DIR toModel3DIR(const Model3DData& data) {
    Model3DIR ir;  // 待填充的 3D 模型 IR。

    ir.setName(data.name());

    // translation: EasyEDA px -> mm
    const auto& t = data.translation();
    ir.setTranslation(Model3DVec3(t.x * MODEL3D_PX_TO_MM, t.y * MODEL3D_PX_TO_MM, t.z * MODEL3D_PX_TO_MM));

    // rotation: 角度，无需转换
    const auto& r = data.rotation();
    ir.setRotation(Model3DVec3(r.x, r.y, r.z));

    // stepOffsetMm: 已经是 mm，无需转换
    const auto& o = data.stepOffsetMm();
    ir.setStepOffsetMm(Model3DVec3(o.x, o.y, o.z));

    ir.setRawObj(data.rawObj());
    ir.setStepData(data.step());

    return ir;
}

}  // namespace IR
}  // namespace EasyKiConverter
