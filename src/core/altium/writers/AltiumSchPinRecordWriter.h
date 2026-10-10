#pragma once

#include "models/AltiumSchComponent.h"
#include "utils/AltiumBinaryWriter.h"

namespace EasyKiConverter {

class AltiumSchLibWriter;

/**
 * @brief 写入 SchLib 二进制引脚记录的协作者。
 * @details 负责 RECORD=2 的固定字段编码，并复用主写入器的 Owner 校验和内容序号状态。
 */
class AltiumSchPinRecordWriter final {
public:
    /**
     * @brief 创建绑定到主 SchLib 写入器的引脚记录协作者。
     * @param owner 所属的 SchLib 主写入器。
     */
    explicit AltiumSchPinRecordWriter(AltiumSchLibWriter& owner);

    /**
     * @brief 写入一个二进制引脚记录。
     * @param writer 当前组件 Data 流写入器。
     * @param pin 引脚模型。
     */
    void write(AltiumBinaryWriter& writer, const AltiumSchPin& pin);

private:
    AltiumSchLibWriter& m_owner;  ///< 所属的 SchLib 主写入器。
};

}  // namespace EasyKiConverter
