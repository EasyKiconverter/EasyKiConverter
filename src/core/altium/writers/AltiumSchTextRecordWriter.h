#pragma once

#include "models/AltiumSchComponent.h"
#include "utils/AltiumBinaryWriter.h"

namespace EasyKiConverter {

class AltiumSchLibWriter;

/**
 * @brief 写入 SchLib 文本类记录的协作者
 * @details 复用主写入器的字体表、诊断列表和记录辅助方法，集中维护文本与文本框的参数编码规则。
 */
class AltiumSchTextRecordWriter {
public:
    /**
     * @brief 创建绑定到主写入器的文本记录协作者
     * @param owner 主 SchLib 写入器
     */
    explicit AltiumSchTextRecordWriter(AltiumSchLibWriter& owner);

    /**
     * @brief 写入文本记录（RECORD=4）
     * @param writer 二进制写入器
     * @param text 文本模型
     */
    void writeText(AltiumBinaryWriter& writer, const AltiumSchText& text);

    /**
     * @brief 写入文本框记录（RECORD=28）
     * @param writer 二进制写入器
     * @param frame 文本框模型
     */
    void writeTextFrame(AltiumBinaryWriter& writer, const AltiumSchTextFrame& frame);

private:
    AltiumSchLibWriter& m_owner;  ///< 所属的 SchLib 主写入器。
};

}  // namespace EasyKiConverter
