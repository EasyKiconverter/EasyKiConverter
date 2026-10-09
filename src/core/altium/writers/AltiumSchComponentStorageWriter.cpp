#include "AltiumSchComponentStorageWriter.h"

#include "AltiumSchGraphicOrderValidator.h"
#include "AltiumSchGraphicOrderWriter.h"
#include "AltiumSchLibWriter.h"
#include "compound/OLECompoundWriter.h"
#include "utils/AltiumBinaryWriter.h"

#include <algorithm>

namespace EasyKiConverter {

/** @brief 保存组件存储写入器所属的 SchLib 写入器。 */
AltiumSchComponentStorageWriter::AltiumSchComponentStorageWriter(AltiumSchLibWriter& owner) : m_owner(owner) {}

/**
 * @brief 写入组件的 Data 流。
 * @details 保持默认图元顺序、graphicOrder 顺序、补充图元以及参数和实现记录的原有顺序。
 * @param ole SchLib 根 OLE 写入器。
 * @param component 待写入的原理图元件。
 * @param sectionKey 组件对应的 OLE 存储键。
 */
void AltiumSchComponentStorageWriter::write(OLECompoundWriter& ole,
                                            const AltiumSchComponent& component,
                                            const QString& sectionKey) {
    // 创建组件存储区，并准备对应的 Data 流。
    ole.addStorage(sectionKey);
    QByteArray data;  // 当前组件 Data 流的序列化缓冲区。
    AltiumBinaryWriter writer(data);
    AltiumSchGraphicOrderWriter graphicOrderWriter(m_owner);

    // Altium 对图元和二进制引脚使用同一个从 0 开始的内容记录计数器。
    // 首条内容记录隐含索引 0，文本记录因此省略 IndexInSheet=0。
    m_owner.m_nextIndexInSheet = 0;
    m_owner.writeComponentRecord(writer, component);

    const bool useGraphicOrder =
        !component.graphicOrder.isEmpty() && AltiumSchGraphicOrderValidator::validate(component);
    const bool hasImageOrder = std::any_of(
        component.graphicOrder.cbegin(), component.graphicOrder.cend(), [](const AltiumSchGraphicOrder& order) {
            return order.type == QStringLiteral("I");
        });
    if (!component.graphicOrder.isEmpty() && !useGraphicOrder) {
        m_owner.m_diagnostics.append(
            QStringLiteral("符号 %1 的 graphicOrder 不完整或包含无效引用，已回退到默认图元顺序").arg(component.name));
    }

    if (useGraphicOrder) {
        // 优先按照源文件顺序写入图元。
        for (const AltiumSchGraphicOrder& order : component.graphicOrder)
            graphicOrderWriter.write(writer, component, order);
        // 引脚名称和编号是由引脚派生出的文本，不在源 shape 顺序中。
        for (const AltiumSchText& text : component.texts) {
            if (text.isPinLabel)
                m_owner.writeTextRecord(writer, text);
        }
        for (const AltiumSchText& text : component.texts) {
            if (!text.isPinLabel && text.sourceGraphicIndex < 0)
                m_owner.writeTextRecord(writer, text);
        }
        // 兼容没有来源顺序引用的扩展图元，例如手工构造的 Bézier。
        for (const AltiumSchLine& line : component.lines)
            m_owner.writeLineRecord(writer, line);
        for (const AltiumSchPie& pie : component.pies)
            m_owner.writePieRecord(writer, pie);
        for (const AltiumSchEllipticalArc& arc : component.ellipticalArcs) {
            if (arc.sourceGraphicType.isEmpty())
                m_owner.writeEllipticalArcRecord(writer, arc);
        }
        for (const AltiumSchIeee& ieee : component.ieeeSymbols)
            m_owner.writeIeeeRecord(writer, ieee);
        for (const AltiumSchBezier& bezier : component.beziers) {
            if (bezier.sourceGraphicType.isEmpty())
                m_owner.writeBezierRecord(writer, bezier);
        }
        for (const AltiumSchRectangle& rect : component.rectangles) {
            if (rect.sourceGraphicIndex < 0)
                m_owner.writeRectangleRecord(writer, rect);
        }
        for (const AltiumSchRoundRectangle& rect : component.roundRectangles) {
            if (rect.sourceGraphicIndex < 0)
                m_owner.writeRoundRectangleRecord(writer, rect);
        }
        for (const AltiumSchPolygon& polygon : component.polygons) {
            if (polygon.sourceGraphicIndex < 0)
                m_owner.writePolygonRecord(writer, polygon);
        }
        for (const AltiumSchEllipse& ellipse : component.ellipses) {
            if (ellipse.sourceGraphicType.isEmpty())
                m_owner.writeEllipseRecord(writer, ellipse);
        }
        for (const AltiumSchArc& arc : component.arcs) {
            if (arc.sourceGraphicType.isEmpty())
                m_owner.writeArcRecord(writer, arc);
        }
        for (const AltiumSchPolyline& polyline : component.polylines) {
            if (polyline.sourceGraphicIndex < 0)
                m_owner.writePolylineRecord(writer, polyline);
        }
        for (const AltiumSchPath& path : component.paths) {
            if (path.sourceGraphicType.isEmpty())
                m_owner.writePathRecord(writer, path);
        }
    } else {
        // 没有可用来源顺序时，保持兼容格式的默认记录顺序。
        for (const AltiumSchPin& pin : component.pins)
            m_owner.writePinRecord(writer, pin);
        for (const AltiumSchRectangle& rect : component.rectangles)
            m_owner.writeRectangleRecord(writer, rect);
        for (const AltiumSchRoundRectangle& rect : component.roundRectangles)
            m_owner.writeRoundRectangleRecord(writer, rect);
        for (const AltiumSchLine& line : component.lines)
            m_owner.writeLineRecord(writer, line);
        for (const AltiumSchArc& arc : component.arcs)
            m_owner.writeArcRecord(writer, arc);
        for (const AltiumSchPolygon& polygon : component.polygons)
            m_owner.writePolygonRecord(writer, polygon);
        for (const AltiumSchEllipse& ellipse : component.ellipses)
            m_owner.writeEllipseRecord(writer, ellipse);
        for (const AltiumSchPie& pie : component.pies)
            m_owner.writePieRecord(writer, pie);
        for (const AltiumSchEllipticalArc& arc : component.ellipticalArcs)
            m_owner.writeEllipticalArcRecord(writer, arc);
        for (const AltiumSchPolyline& polyline : component.polylines)
            m_owner.writePolylineRecord(writer, polyline);
        for (const AltiumSchPath& path : component.paths)
            m_owner.writePathRecord(writer, path);
        for (const AltiumSchBezier& bezier : component.beziers)
            m_owner.writeBezierRecord(writer, bezier);
        for (const AltiumSchIeee& ieee : component.ieeeSymbols)
            m_owner.writeIeeeRecord(writer, ieee);
        for (const AltiumSchText& text : component.texts)
            m_owner.writeTextRecord(writer, text);
    }

    // 这些记录不参与来源图元排序，统一放在图元之后。
    for (const AltiumSchTextFrame& frame : component.textFrames)
        m_owner.writeTextFrameRecord(writer, frame);
    for (const AltiumSchImage& image : component.images) {
        if (!useGraphicOrder || !hasImageOrder || image.sourceGraphicIndex < 0)
            m_owner.writeImageRecord(writer, image);
    }
    m_owner.writeComponentParameterRecords(writer, component);
    m_owner.writeImplementationRecords(writer, component);

    // 将完整 Data 流写入组件存储区。
    ole.writeStream(sectionKey, "Data", data);
}

}  // namespace EasyKiConverter
