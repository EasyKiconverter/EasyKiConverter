#pragma once

#include "models/AltiumSchComponent.h"

#include <QString>

namespace EasyKiConverter {

class AltiumSchLibWriter;
class OLECompoundWriter;

/**
 * @brief 写入 SchLib 单个组件的 Data 存储区。
 * @details 负责组件记录、来源顺序图元、补充图元、参数记录和实现记录的写入，
 *          不改变 AltiumSchLibWriter 对记录格式的具体编码实现。
 */
class AltiumSchComponentStorageWriter final {
public:
    /**
     * @brief 创建绑定到指定 SchLib 写入器的组件存储写入器。
     * @param owner 所属的 SchLib 主写入器。
     */
    explicit AltiumSchComponentStorageWriter(AltiumSchLibWriter& owner);

    /**
     * @brief 将一个组件写入指定的 OLE 存储区。
     * @param ole SchLib 根 OLE 写入器
     * @param component 待写入的组件
     * @param sectionKey 组件对应的存储键
     */
    void write(OLECompoundWriter& ole, const AltiumSchComponent& component, const QString& sectionKey);

private:
    AltiumSchLibWriter& m_owner;  ///< 所属的 SchLib 主写入器。
};

}  // namespace EasyKiConverter
