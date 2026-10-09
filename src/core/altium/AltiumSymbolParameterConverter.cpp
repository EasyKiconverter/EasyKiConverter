#include "AltiumSymbolParameterConverter.h"

#include "utils/AltiumCoord.h"
#include "utils/AltiumSymbolConversionUtils.h"

namespace EasyKiConverter {

/**
 * @brief 将符号参数字段映射到 Altium 参数记录。
 * @param parameter 统一 IR 中的符号参数。
 * @return 转换后的 Altium 参数记录。
 */
AltiumSchParameter AltiumSymbolParameterConverter::convert(const IR::SymbolParameterIR& parameter) {
    AltiumSchParameter altiumParameter;  // 转换后的 Altium 参数记录。
    altiumParameter.name = parameter.name;
    altiumParameter.value = parameter.value;
    altiumParameter.locationX = AltiumCoord::mmToRaw(parameter.position.x());
    altiumParameter.locationY = AltiumCoord::mmToRaw(parameter.position.y());
    altiumParameter.fontSizeMm = parameter.fontSizeMm;
    altiumParameter.isHidden = !parameter.visible;
    altiumParameter.readOnly = parameter.readOnly;
    altiumParameter.orientation = AltiumSymbolConversionUtils::toAltiumOrientation(parameter.rotation);
    altiumParameter.ownerPartId = AltiumSymbolConversionUtils::toAltiumOwnerPartId(parameter.partIndex);
    altiumParameter.color = AltiumSymbolConversionUtils::toAltiumColor(parameter.color);
    return altiumParameter;
}

}  // namespace EasyKiConverter
