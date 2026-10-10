/**
 * @file FootprintGraphicsGenerator.h
 * @brief 声明 KiCad 封装图形和图层文本生成器。
 */

#ifndef FOOTPRINTGRAPHICSGENERATOR_H
#define FOOTPRINTGRAPHICSGENERATOR_H

#include "core/ir/FootprintIR.h"
#include "core/ir/Model3DIR.h"

#include <QString>

namespace EasyKiConverter {

/**
 * @brief 封装图形生成器
 *
 * 从 ExporterFootprint 中提取的图形生成函数，
 * 负责将各类封装图形元素转换为 KiCad 格式文本。
 */
class FootprintGraphicsGenerator {
public:
    FootprintGraphicsGenerator() = default;

    /**
     * @brief 生成焊盘的 KiCad 封装文本。
     * @param pad 统一 IR 焊盘。
     * @param originX 封装原点 X 坐标。
     * @param originY 封装原点 Y 坐标。
     * @return KiCad 焊盘文本。
     */
    QString generatePad(const IR::FootprintPadIR& pad, double originX, double originY) const;

    /**
     * @brief 生成走线图元的 KiCad 文本。
     * @param track 统一 IR 走线。
     * @param originX 封装原点 X 坐标。
     * @param originY 封装原点 Y 坐标。
     * @return KiCad 走线文本。
     */
    QString generateTrack(const IR::FootprintTrackIR& track, double originX, double originY) const;

    /**
     * @brief 生成机械孔的 KiCad 焊盘文本。
     * @param hole 统一 IR 机械孔。
     * @param originX 封装原点 X 坐标。
     * @param originY 封装原点 Y 坐标。
     * @return KiCad 机械孔文本。
     */
    QString generateHole(const IR::FootprintHoleIR& hole, double originX, double originY) const;

    /**
     * @brief 生成圆形图元的 KiCad 文本。
     * @param circle 统一 IR 圆形图元。
     * @param originX 封装原点 X 坐标。
     * @param originY 封装原点 Y 坐标。
     * @return KiCad 圆形图元文本。
     */
    QString generateCircle(const IR::FootprintCircleIR& circle, double originX, double originY) const;

    /**
     * @brief 生成矩形图元的 KiCad 文本。
     * @param rectangle 统一 IR 矩形图元。
     * @param originX 封装原点 X 坐标。
     * @param originY 封装原点 Y 坐标。
     * @return KiCad 矩形边线文本。
     */
    QString generateRectangle(const IR::FootprintRectangleIR& rectangle, double originX, double originY) const;

    /**
     * @brief 将圆弧离散为 KiCad 三点弧线文本。
     * @param arc 统一 IR 圆弧。
     * @param originX 封装原点 X 坐标。
     * @param originY 封装原点 Y 坐标。
     * @return KiCad 圆弧文本。
     */
    QString generateArc(const IR::FootprintArcIR& arc, double originX, double originY) const;

    /**
     * @brief 生成封装文本或非 ASCII 文本轮廓。
     * @param text 统一 IR 文本。
     * @param originX 封装原点 X 坐标。
     * @param originY 封装原点 Y 坐标。
     * @return KiCad 文本或轮廓文本。
     */
    QString generateText(const IR::FootprintTextIR& text, double originX, double originY) const;

    /**
     * @brief 生成实心区域或禁止区的 KiCad 文本。
     * @param region 统一 IR 区域。
     * @param originX 封装原点 X 坐标。
     * @param originY 封装原点 Y 坐标。
     * @return KiCad 区域文本。
     */
    QString generateSolidRegion(const IR::FootprintRegionIR& region, double originX, double originY) const;

    /**
     * @brief 根据边界框生成封装庭院边线。
     * @param x1 边界左侧 X 坐标。
     * @param y1 边界顶部 Y 坐标。
     * @param x2 边界右侧 X 坐标。
     * @param y2 边界底部 Y 坐标。
     * @return KiCad 庭院边线文本。
     */
    QString generateCourtyardFromBBox(double x1, double y1, double x2, double y2) const;

    /**
     * @brief 生成三维模型的 KiCad 封装文本。
     * @param model3D 统一 IR 三维模型。
     * @param model3DPath 导出后使用的模型路径。
     * @return KiCad 三维模型文本。
     */
    QString generateModel3D(const IR::Model3DIR& model3D, const QString& model3DPath) const;

    /**
     * @brief 将 IR 图层映射为 KiCad 图层名称。
     * @param layerType IR 图层类型。
     * @return KiCad 图层名称。
     */
    static QString layerTypeToKicad(IR::LayerType layerType);

    /**
     * @brief 将焊盘层行为映射为 KiCad 层列表。
     * @param layer IR 焊盘层类型。
     * @return KiCad 焊盘层列表。
     */
    static QString padLayersToKicad(IR::LayerType layer);

    /**
     * @brief 将 IR 焊盘类型映射为 KiCad 焊盘类型。
     * @param pad IR 焊盘。
     * @return KiCad 焊盘类型名称。
     */
    static QString padTypeToKicad(const IR::FootprintPadIR& pad);

private:
    /**
     * @brief 向下截断数值到两位小数。
     * @param value 待处理的数值。
     * @return 向下截断后的数值。
     */
    static double roundTo2(double value);
};

}  // namespace EasyKiConverter

#endif  // FOOTPRINTGRAPHICSGENERATOR_H
