#pragma once

#include "models/AltiumSchComponent.h"
#include "utils/AltiumBinaryWriter.h"

namespace EasyKiConverter {

class AltiumSchLibWriter;

/**
 * @brief 写入 SchLib 图片记录的协作者
 * @details 负责把图片几何、显示选项和嵌入 Storage 文件名编码为 RECORD=30 参数块。
 */
class AltiumSchImageRecordWriter {
public:
    /**
     * @brief 创建绑定到主写入器的图片记录协作者
     * @param owner 主 SchLib 写入器
     */
    explicit AltiumSchImageRecordWriter(AltiumSchLibWriter& owner);

    /**
     * @brief 写入图片记录（RECORD=30）
     * @param writer 二进制写入器
     * @param image 图片模型
     */
    void write(AltiumBinaryWriter& writer, const AltiumSchImage& image);

private:
    AltiumSchLibWriter& m_owner;  ///< 所属的 SchLib 主写入器。
};

}  // namespace EasyKiConverter
