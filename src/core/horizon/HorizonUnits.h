#pragma once

#include <QtGlobal>

#include <cmath>
#include <limits>

namespace EasyKiConverter {

/** @brief Horizon 坐标单位转换；源码确认 1 unit = 1 nm。 */
class HorizonUnits final {
public:
    static qint64 mm(double value) {
        const double scaled = value * 1000000.0;
        if (!std::isfinite(scaled) || scaled > static_cast<double>(std::numeric_limits<qint64>::max()) ||
            scaled < static_cast<double>(std::numeric_limits<qint64>::min())) {
            return 0;
        }
        return static_cast<qint64>(std::llround(scaled));
    }
};

}  // namespace EasyKiConverter
