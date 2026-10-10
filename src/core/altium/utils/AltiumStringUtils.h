#pragma once

#include <QList>
#include <QPointF>
#include <QString>

namespace EasyKiConverter {

/**
 * @brief Altium 字符串解析工具
 * @details 提供 EasyEDA 数据格式中常见的字符串解析功能，
 *          用于符号和封装导出器中的坐标点解析。
 */
namespace AltiumStringUtils {

/**
 * @brief 解析空格分隔的 "x,y" 点字符串为 QPointF 列表
 * @param pointsStr 点字符串，格式："x1,y1 x2,y2 x3,y3"
 * @return 解析后的 QPointF 列表
 *
 * @details EasyEDA 的 SymbolPolygon.points、SymbolPolyline.points、
 *          FootprintTrack.points 等字段使用此格式。
 *
 * @code
 * auto points = parsePointsString("0,0 100,200 300,400");
 * // points = [{0,0}, {100,200}, {300,400}]
 * @endcode
 */
inline QList<QPointF> parsePointsString(const QString& pointsStr) {
    QList<QPointF> result;  // 解析得到的坐标点列表。
    QStringList pairs = pointsStr.trimmed().split(' ', Qt::SkipEmptyParts);  // 空格分隔的点字符串。
    for (const QString& pair : pairs) {
        QStringList coords = pair.split(',');  // 当前点的 X/Y 字段。
        if (coords.size() >= 2) {
            bool ok1 = false;  // X 坐标转换是否成功。
            bool ok2 = false;  // Y 坐标转换是否成功。
            double x = coords[0].toDouble(&ok1);  // 当前点 X 坐标。
            double y = coords[1].toDouble(&ok2);  // 当前点 Y 坐标。
            if (ok1 && ok2) {
                result.append(QPointF(x, y));
            }
        }
    }
    return result;
}

/**
 * @brief 解析 SVG 路径命令为 QPointF 列表
 * @param pathStr SVG 路径字符串（如 "M 0 0 L 100 200 Z"）
 * @return 从路径中提取的坐标点列表
 *
 * @details 简化处理：移除所有路径命令字母，提取剩余的数字对。
 *          EasyEDA 的 SymbolPath.paths、FootprintSolidRegion.path 使用 SVG 路径格式。
 *
 * @code
 * auto points = parsePathString("M 0 0 L 100 0 L 100 100 Z");
 * // points = [{0,0}, {100,0}, {100,100}]
 * @endcode
 */
inline QList<QPointF> parsePathString(const QString& pathStr) {
    QList<QPointF> result;  // 解析得到的坐标点列表。
    QString cleaned = pathStr;  // 删除 SVG 命令后的路径文本。
    // 单遍替换：SVG 路径命令字母和逗号统一替换为空格
    for (int i = 0; i < cleaned.size(); ++i) {
        QChar c = cleaned[i];  // 当前路径字符。
        if (c == ',' || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))
            cleaned[i] = ' ';
    }
    QStringList nums = cleaned.split(' ', Qt::SkipEmptyParts);  // 空格分隔的数字字段。
    for (int i = 0; i + 1 < nums.size(); i += 2) {
        bool ok1 = false;  // X 坐标转换是否成功。
        bool ok2 = false;  // Y 坐标转换是否成功。
        double x = nums[i].toDouble(&ok1);  // 当前点 X 坐标。
        double y = nums[i + 1].toDouble(&ok2);  // 当前点 Y 坐标。
        if (ok1 && ok2) {
            result.append(QPointF(x, y));
        }
    }
    return result;
}

}  // namespace AltiumStringUtils
}  // namespace EasyKiConverter
