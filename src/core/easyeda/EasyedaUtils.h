#ifndef EASYEDAUTILS_H
#define EASYEDAUTILS_H

#include <QString>
#include <QStringList>

namespace EasyKiConverter {

class EasyedaUtils {
public:
    /**
     * @brief 按 EasyEDA 分隔符解析数据字符串。
     * @param data 待解析的数据字符串。
     * @return 包含空字段的分段列表。
     */
    static QStringList parseDataString(const QString& data) {
        return data.split("~", Qt::KeepEmptyParts);
    }

    /**
     * @brief 将 EasyEDA 布尔文本转换为布尔值。
     * @param str 待转换的文本。
     * @return 文本表示真值时返回 true。
     */
    static bool stringToBool(const QString& str) {
        if (str.isEmpty() || str == "0" || str.toLower() == "false" || str.toLower() == "none" ||
            str.toLower() == "transparent") {
            return false;
        }
        return true;
    }
};

}  // namespace EasyKiConverter

#endif  // EASYEDAUTILS_H
