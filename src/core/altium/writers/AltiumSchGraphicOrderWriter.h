#pragma once

#include "models/AltiumSchComponent.h"

namespace EasyKiConverter {

class AltiumBinaryWriter;
class AltiumSchLibWriter;

/**
 * @brief 按 EasyEDA 来源顺序调度 Altium SchLib 图元记录。
 *
 * 该类只负责查找来源图元并调用写入器的记录序列化方法，不改变任何二进制记录格式。
 */
class AltiumSchGraphicOrderWriter final {
public:
    /**
     * @brief 创建绑定到指定 SchLib 写入器的顺序调度器。
     * @param owner 所属的 SchLib 主写入器。
     */
    explicit AltiumSchGraphicOrderWriter(AltiumSchLibWriter& owner);

    /**
     * @brief 按来源顺序写入一个图元或一组路径片段。
     * @param writer 当前组件 Data 流写入器
     * @param component 待写入的组件
     * @param order 来源图元顺序信息
     */
    void write(AltiumBinaryWriter& writer, const AltiumSchComponent& component, const AltiumSchGraphicOrder& order);

private:
    AltiumSchLibWriter& m_owner;  ///< 所属的 SchLib 主写入器。
};

}  // namespace EasyKiConverter
