#pragma once

#include "AllegroFootprintModel.h"

#include <optional>

namespace EasyKiConverter {

/**
 * @brief 将统一 IR 层语义映射为 Allegro Class/Subclass。
 * @details 未知层返回空值，调用方必须生成诊断，不得将其猜测映射为铜层。
 */
class AllegroLayerMapper {
public:
    /** @brief 映射一个 IR 层。 */
    static std::optional<AllegroLayerAssignment> map(IR::LayerType layer);
};

}  // namespace EasyKiConverter
