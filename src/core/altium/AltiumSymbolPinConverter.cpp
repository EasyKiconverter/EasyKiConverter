#include "AltiumSymbolPinConverter.h"

#include "utils/AltiumCoord.h"
#include "utils/AltiumLayerMap.h"
#include "utils/AltiumSymbolConversionUtils.h"

#include <QSet>

namespace EasyKiConverter {

/** @brief 根据 IR 引脚装饰枚举填充 Altium IEEE 图形字段。 */
void applyPinDecoration(const IR::SymbolPinIR& pin, AltiumSchPin& altiumPin) {
    // 将 IR 装饰枚举映射为 Altium 的内侧、外侧和边缘图形编号。
    switch (pin.style.decoration) {
        case IR::PinDecoration::Dot:
            altiumPin.symbolOuterEdge = 1;
            break;
        case IR::PinDecoration::ActiveLow:
            altiumPin.symbolOuterEdge = 4;
            break;
        case IR::PinDecoration::Clock:
            altiumPin.symbolInnerEdge = 3;
            break;
        case IR::PinDecoration::InvertedClock:
            altiumPin.symbolInnerEdge = 3;
            altiumPin.symbolOuterEdge = 1;
            break;
        case IR::PinDecoration::OpenCollector:
            altiumPin.symbolInside = 9;
            break;
        case IR::PinDecoration::OpenEmitter:
            altiumPin.symbolInside = 23;
            break;
        case IR::PinDecoration::HiZ:
            altiumPin.symbolInside = 10;
            break;
        case IR::PinDecoration::Pulse:
            altiumPin.symbolInside = 12;
            break;
        case IR::PinDecoration::Postponed:
            altiumPin.symbolInside = 8;
            break;
        case IR::PinDecoration::ShiftLeft:
            altiumPin.symbolInside = 30;
            break;
        case IR::PinDecoration::AnalogInput:
            altiumPin.symbolOutside = 5;
            break;
        case IR::PinDecoration::NoConnect:
            altiumPin.symbolOutside = 6;
            break;
        case IR::PinDecoration::GroupLine:
            altiumPin.symbolOutside = 15;
            break;
        case IR::PinDecoration::FlagRight:
            altiumPin.symbolOutside = 33;
            break;
        case IR::PinDecoration::FlagLeft:
            altiumPin.symbolOutside = 2;
            break;
        case IR::PinDecoration::ShiftRight:
            altiumPin.symbolInside = 7;
            break;
        case IR::PinDecoration::HighCurrent:
            altiumPin.symbolInside = 11;
            break;
        case IR::PinDecoration::Schmitt:
            altiumPin.symbolInside = 13;
            break;
        case IR::PinDecoration::Delay:
            altiumPin.symbolInside = 14;
            break;
        case IR::PinDecoration::ActiveLowOutput:
            altiumPin.symbolOuterEdge = 17;
            break;
        case IR::PinDecoration::OpenCollectorPullUp:
            altiumPin.symbolInside = 22;
            break;
        case IR::PinDecoration::OpenEmitterPullUp:
            altiumPin.symbolInside = 24;
            break;
        case IR::PinDecoration::DigitalInput:
            altiumPin.symbolOutside = 25;
            break;
        case IR::PinDecoration::GroupBinary:
            altiumPin.symbolOutside = 16;
            break;
        case IR::PinDecoration::InputOutput:
            altiumPin.symbolOutside = 31;
            break;
        case IR::PinDecoration::OpenCircuitOutput:
            altiumPin.symbolInside = 32;
            break;
        case IR::PinDecoration::Pi:
            altiumPin.symbolOutside = 18;
            break;
        case IR::PinDecoration::GreaterEqual:
            altiumPin.symbolOutside = 19;
            break;
        case IR::PinDecoration::LessEqual:
            altiumPin.symbolOutside = 20;
            break;
        case IR::PinDecoration::Sigma:
            altiumPin.symbolOutside = 21;
            break;
        case IR::PinDecoration::And:
            altiumPin.symbolOutside = 26;
            break;
        case IR::PinDecoration::Inverter:
            altiumPin.symbolOutside = 27;
            break;
        case IR::PinDecoration::Or:
            altiumPin.symbolOutside = 28;
            break;
        case IR::PinDecoration::Xor:
            altiumPin.symbolOutside = 29;
            break;
        case IR::PinDecoration::BidirectionalSignalFlow:
            altiumPin.symbolOutside = 34;
            break;
        default:
            break;
    }
}

/** @brief 根据电气类型补充 Altium IEEE 装饰。 */
void applyElectricalDecoration(const IR::SymbolPinIR& pin, AltiumSchPin& altiumPin) {
    // 对电气类型补充在来源数据中未显式声明的 IEEE 图形。
    switch (pin.electricalType) {
        case IR::PinElectricalType::OpenCollector:
            altiumPin.symbolInside = 9;
            break;
        case IR::PinElectricalType::OpenEmitter:
            altiumPin.symbolInside = 23;
            break;
        default:
            break;
    }
}

/** @brief 按常见网络名称将 EasyEDA 电源引脚标记为 Altium Power 类型。 */
void applyPowerPinType(const IR::SymbolPinIR& pin, AltiumSchPin& altiumPin) {
    if (altiumPin.electricalType == AltiumModels::PinElectricalType::Power)
        return;

    static const QSet<QString> powerPinNames = {
        "GND",      "AGND",    "DGND",     "PGND",      "SGND",  "CGND", "GNDP", "GNDN", "VCC",
        "VDD",      "AVCC",    "AVDD",     "DVDD",      "IOVDD", "PVDD", "SVDD", "VDDA", "VDDIO",
        "VDDS",     "VDDP",    "VBUS",     "VSYS",      "VIN",   "5V",   "3V3",  "1V8",  "USB_VDD",
        "ADC_AVDD", "VREG_IN", "VREG_OUT", "VREG_VOUT", "VEE",   "VSS",  "VSSA",
    };
    if (powerPinNames.contains(pin.name.toUpper().trimmed()))
        altiumPin.electricalType = AltiumModels::PinElectricalType::Power;
}

/** @brief 转换引脚基础几何、显示属性、电气类型和装饰。 */
AltiumSchPin AltiumSymbolPinConverter::convert(const IR::SymbolPinIR& pin) {
    AltiumSchPin altiumPin;
    altiumPin.name = pin.name;
    altiumPin.designator = pin.designator;
    altiumPin.locationX = AltiumCoord::mmToRaw(pin.position.x());
    altiumPin.locationY = AltiumCoord::mmToRaw(pin.position.y());
    altiumPin.length = pin.length > 0.0 ? AltiumCoord::mmToRaw(pin.length) : 100000;
    altiumPin.electricalType =
        static_cast<AltiumModels::PinElectricalType>(AltiumLayerMap::toAltiumElectricalType(pin.electricalType));
    altiumPin.orientation =
        static_cast<AltiumModels::PinOrientation>(AltiumLayerMap::toAltiumPinOrientation(pin.direction));
    // EasyEDA 的 pin name 显示标志在部分库中未设置，但名称字符串本身仍是符号的一部分。
    altiumPin.showName =
        !pin.hasNamePosition && (pin.display.showName || pin.showName || !pin.name.trimmed().isEmpty());
    // 编号统一由独立的水平文本记录承载，禁止 Altium 按引脚方向旋转内置 designator。
    altiumPin.showDesignator = false;
    altiumPin.isHidden = !altiumPin.showName && !altiumPin.showDesignator;
    altiumPin.color = AltiumSymbolConversionUtils::toAltiumColor(QColor(Qt::black));

    if (pin.style.inverted || pin.hasDot)
        altiumPin.symbolOuterEdge = 1;
    if (pin.style.activeLow)
        altiumPin.symbolOuterEdge = 4;
    if (pin.style.clock || pin.hasClock)
        altiumPin.symbolInnerEdge = 3;

    applyPinDecoration(pin, altiumPin);
    applyElectricalDecoration(pin, altiumPin);
    altiumPin.ownerPartId = pin.commonToAllParts ? -1 : AltiumSymbolConversionUtils::toAltiumOwnerPartId(pin.partIndex);
    altiumPin.sourcePartIndex = pin.partIndex;
    applyPowerPinType(pin, altiumPin);
    return altiumPin;
}

}  // namespace EasyKiConverter
