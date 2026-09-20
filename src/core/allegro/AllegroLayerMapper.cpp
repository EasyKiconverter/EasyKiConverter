#include "AllegroLayerMapper.h"

namespace EasyKiConverter {

/** 将统一 IR 图层映射为 Allegro 的 Class/Subclass 语义。 */
std::optional<AllegroLayerAssignment> AllegroLayerMapper::map(IR::LayerType layer) {
    // 按 Allegro 的 Class/Subclass 语义处理每个已知 IR 图层。
    switch (layer) {
        case IR::LayerType::TopSilk:
        case IR::LayerType::TopOverlay:
            return AllegroLayerAssignment{"PACKAGE GEOMETRY", "SILKSCREEN_TOP", "TopSilk/TopOverlay"};
        case IR::LayerType::BottomSilk:
        case IR::LayerType::BottomOverlay:
            return AllegroLayerAssignment{"PACKAGE GEOMETRY", "SILKSCREEN_BOTTOM", "BottomSilk/BottomOverlay"};
        case IR::LayerType::TopAssembly:
            return AllegroLayerAssignment{"PACKAGE GEOMETRY", "ASSEMBLY_TOP", "TopAssembly"};
        case IR::LayerType::BottomAssembly:
            return AllegroLayerAssignment{"PACKAGE GEOMETRY", "ASSEMBLY_BOTTOM", "BottomAssembly"};
        case IR::LayerType::TopMask:
            return AllegroLayerAssignment{"PACKAGE GEOMETRY", "SOLDERMASK_TOP", "TopMask"};
        case IR::LayerType::BottomMask:
            return AllegroLayerAssignment{"PACKAGE GEOMETRY", "SOLDERMASK_BOTTOM", "BottomMask"};
        case IR::LayerType::TopPaste:
            return AllegroLayerAssignment{"PACKAGE GEOMETRY", "PASTEMASK_TOP", "TopPaste"};
        case IR::LayerType::BottomPaste:
            return AllegroLayerAssignment{"PACKAGE GEOMETRY", "PASTEMASK_BOTTOM", "BottomPaste"};
        case IR::LayerType::KeepOut:
            return AllegroLayerAssignment{"PACKAGE GEOMETRY", "PLACE_BOUND_TOP", "KeepOut"};
        case IR::LayerType::Mechanical1:
            return AllegroLayerAssignment{"PACKAGE GEOMETRY", "PLACE_BOUND_TOP", "Mechanical1"};
        case IR::LayerType::Mechanical2:
            return AllegroLayerAssignment{"PACKAGE GEOMETRY", "PLACE_BOUND_BOTTOM", "Mechanical2"};
        case IR::LayerType::TopCopper:
            return AllegroLayerAssignment{"PACKAGE GEOMETRY", "TOP", "TopCopper"};
        case IR::LayerType::BottomCopper:
            return AllegroLayerAssignment{"PACKAGE GEOMETRY", "BOTTOM", "BottomCopper"};
        case IR::LayerType::MultiLayer:
            return AllegroLayerAssignment{"PACKAGE GEOMETRY", "THRU_HOLE", "MultiLayer"};
        default:
            return std::nullopt;
    }
}

}  // namespace EasyKiConverter
