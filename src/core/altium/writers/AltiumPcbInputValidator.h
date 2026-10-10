#pragma once

#include "models/AltiumPcbComponent.h"

namespace EasyKiConverter {

class AltiumPcbLibWriter;

/**
 * @brief 校验 PcbLib 写入前的封装输入。
 * @details 检查名称、编码、图元属性、三维模型关联和扩展图元引用，避免无效数据进入 OLE 写入阶段。
 */
class AltiumPcbInputValidator final {
public:
    /**
     * @brief 校验封装列表和目标文件路径。
     * @param owner PcbLib 主写入器，用于记录诊断并计算图元数量。
     * @param components 待写入的封装列表。
     * @param filePath 输出文件路径。
     * @return 输入满足 PcbLib 约束时返回 true。
     */
    static bool validate(AltiumPcbLibWriter& owner,
                         const QList<AltiumPcbComponent>& components,
                         const QString& filePath);
};

}  // namespace EasyKiConverter
