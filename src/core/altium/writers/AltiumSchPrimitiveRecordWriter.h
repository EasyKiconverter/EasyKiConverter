#pragma once

#include "models/AltiumSchComponent.h"
#include "utils/AltiumBinaryWriter.h"

namespace EasyKiConverter {

class AltiumSchLibWriter;

/**
 * @brief 写入 SchLib 文本参数图元记录的协作者。
 * @details 负责几何图元的 RECORD 参数编码，并复用主写入器的坐标、唯一标识和诊断状态。
 */
class AltiumSchPrimitiveRecordWriter final {
public:
    /**
     * @brief 创建绑定到主 SchLib 写入器的图元记录协作者。
     * @param owner 所属的 SchLib 主写入器。
     */
    explicit AltiumSchPrimitiveRecordWriter(AltiumSchLibWriter& owner);

    /**
     * @brief 写入矩形记录。
     * @param writer 目标二进制写入器。
     * @param rectangle 矩形模型。
     */
    void writeRectangle(AltiumBinaryWriter& writer, const AltiumSchRectangle& rectangle);
    /**
     * @brief 写入圆角矩形记录。
     * @param writer 目标二进制写入器。
     * @param rectangle 圆角矩形模型。
     */
    void writeRoundRectangle(AltiumBinaryWriter& writer, const AltiumSchRoundRectangle& rectangle);
    /**
     * @brief 写入线段记录。
     * @param writer 目标二进制写入器。
     * @param line 线段模型。
     */
    void writeLine(AltiumBinaryWriter& writer, const AltiumSchLine& line);
    /**
     * @brief 写入圆弧记录。
     * @param writer 目标二进制写入器。
     * @param arc 圆弧模型。
     */
    void writeArc(AltiumBinaryWriter& writer, const AltiumSchArc& arc);
    /**
     * @brief 写入多边形记录。
     * @param writer 目标二进制写入器。
     * @param polygon 多边形模型。
     */
    void writePolygon(AltiumBinaryWriter& writer, const AltiumSchPolygon& polygon);
    /**
     * @brief 写入椭圆记录。
     * @param writer 目标二进制写入器。
     * @param ellipse 椭圆模型。
     */
    void writeEllipse(AltiumBinaryWriter& writer, const AltiumSchEllipse& ellipse);
    /**
     * @brief 写入扇形记录。
     * @param writer 目标二进制写入器。
     * @param pie 扇形模型。
     */
    void writePie(AltiumBinaryWriter& writer, const AltiumSchPie& pie);
    /**
     * @brief 写入椭圆弧记录。
     * @param writer 目标二进制写入器。
     * @param arc 椭圆弧模型。
     */
    void writeEllipticalArc(AltiumBinaryWriter& writer, const AltiumSchEllipticalArc& arc);
    /**
     * @brief 写入折线记录。
     * @param writer 目标二进制写入器。
     * @param polyline 折线模型。
     */
    void writePolyline(AltiumBinaryWriter& writer, const AltiumSchPolyline& polyline);
    /**
     * @brief 将路径记录转换为折线记录后写入。
     * @param writer 目标二进制写入器。
     * @param path 路径模型。
     */
    void writePath(AltiumBinaryWriter& writer, const AltiumSchPath& path);
    /**
     * @brief 写入三次 Bezier 曲线记录。
     * @param writer 目标二进制写入器。
     * @param bezier Bezier 曲线模型。
     */
    void writeBezier(AltiumBinaryWriter& writer, const AltiumSchBezier& bezier);
    /**
     * @brief 写入 IEEE 图形记录。
     * @param writer 目标二进制写入器。
     * @param ieee IEEE 图形模型。
     */
    void writeIeee(AltiumBinaryWriter& writer, const AltiumSchIeee& ieee);

private:
    AltiumSchLibWriter& m_owner;  ///< 所属的 SchLib 主写入器。
};

}  // namespace EasyKiConverter
