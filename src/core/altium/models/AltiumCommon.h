#pragma once

#include <QString>

#include <cstdint>

namespace EasyKiConverter {

/**
 * @brief Altium 公共类型定义
 */
namespace AltiumModels {

/** @brief Altium 焊盘形状枚举 */
enum class PadShape : uint8_t { Round = 1, Rectangular = 2, Octagonal = 3, RoundedRectangle = 9 };

/** @brief Altium 引脚电气类型 */
enum class PinElectricalType : uint8_t {
    Input = 0,
    IO = 1,
    Output = 2,
    OpenCollector = 3,
    Passive = 4,
    HiZ = 5,
    OpenEmitter = 6,
    Power = 7
};

/** @brief Altium 引脚方向 */
enum class PinOrientation : uint8_t {
    Right = 0,  ///< 0°（引脚向右延伸）
    Up = 1,  ///< 90°（引脚向上延伸）
    Left = 2,  ///< 180°（引脚向左延伸）
    Down = 3  ///< 270°（引脚向下延伸）
};

/** @brief 字体表条目 */
struct FontEntry {
    QString name = "Times New Roman";  ///< 字体名称。
    int size = 10;  ///< 字体大小。
    bool bold = false;  ///< 是否使用粗体。
    bool italic = false;  ///< 是否使用斜体。
    bool underline = false;  ///< 是否添加下划线。
};

/** @brief 焊盘模式 */
enum class PadMode : uint8_t { Simple = 0, TopMidBot = 1, FullStack = 2 };

}  // namespace AltiumModels
}  // namespace EasyKiConverter
