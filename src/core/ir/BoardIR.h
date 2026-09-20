#pragma once

/**
 * @file BoardIR.h
 * @brief PCB 板级图形和放置的通用中间表示。
 */

#include "FootprintIR.h"

namespace EasyKiConverter::IR {

/**
 * @brief 通用 PCB 板级图形集合。
 * @details 复用已经标准化的几何图元，但不携带任何来源格式的节点或枚举。
 */
struct BoardIR {
    QList<FootprintTrackIR> tracks;  ///< 板级线段、折线或导线。
    QList<FootprintCircleIR> circles;  ///< 板级圆形图元。
    QList<FootprintRectangleIR> rectangles;  ///< 板级矩形图元。
    QList<FootprintArcIR> arcs;  ///< 板级圆弧图元。
    QList<FootprintTextIR> texts;  ///< 板级文本图元。
    QList<FootprintRegionIR> regions;  ///< 板级多边形或填充区域。
    QList<FootprintOutlineIR> outlines;  ///< 板框轮廓图元。

    /** @brief 判断板级图形集合是否为空。 */
    bool isEmpty() const {
        return tracks.isEmpty() && circles.isEmpty() && rectangles.isEmpty() && arcs.isEmpty() && texts.isEmpty() &&
               regions.isEmpty() && outlines.isEmpty();
    }

    /** @brief 清空全部板级图形。 */
    void clear() {
        tracks.clear();
        circles.clear();
        rectangles.clear();
        arcs.clear();
        texts.clear();
        regions.clear();
        outlines.clear();
    }
};

}  // namespace EasyKiConverter::IR
