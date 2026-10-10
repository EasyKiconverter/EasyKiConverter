#pragma once

#include "models/AltiumSchComponent.h"

namespace EasyKiConverter {

class AltiumSchLibWriter;

/**
 * @brief 校验 SchLib 写入前的组件文本、参数、名称和所有权输入。
 *
 * 几何约束与 OWNERPARTID 约束继续交给现有专用校验器；本类只负责写入器
 * 入口处的跨字段输入检查，避免主写入流程混入大量拒绝条件。
 */
class AltiumSchInputValidator final {
public:
    /**
     * @brief 校验待写入组件列表。
     * @param owner SchLib 写入器，用于复用诊断和几何校验状态。
     * @param components 待写入组件。
     * @return 输入可写入时返回 true。
     */
    static bool validate(AltiumSchLibWriter& owner, const QList<AltiumSchComponent>& components);
};

}  // namespace EasyKiConverter
