#include "KiCadTableUtils.h"

namespace EasyKiConverter {
namespace KiCadTableUtils {

/**
 * @brief 转义 KiCad 表格字段中的反斜杠和引号。
 * @param value 待转义的字段值。
 * @return 可安全写入表格的字段值。
 */
QString escapeTableValue(const QString& value) {
    QString escaped = value;  // 字段值的可修改副本。
    escaped.replace("\\", "\\\\");
    escaped.replace("\"", "\\\"");
    return escaped;
}

}  // namespace KiCadTableUtils
}  // namespace EasyKiConverter
