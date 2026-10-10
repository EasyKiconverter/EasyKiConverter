#include "AltiumSchOwnershipValidator.h"

#include "AltiumSchLibWriter.h"

#include <QDebug>

namespace EasyKiConverter {

/**
 * @brief 校验所有图元和参数记录的 OWNERPARTID 是否落在组件部件范围内。
 * @param writer 所属的 SchLib 主写入器，用于记录诊断信息。
 * @param component 待校验的原理图元件。
 * @return 所有记录归属均有效时返回 true。
 */
bool AltiumSchOwnershipValidator::validate(AltiumSchLibWriter& writer, const AltiumSchComponent& component) {
    const int partCount = qMax(1, component.partCount);  // 组件有效部件数量。
    const auto isOutOfRange = [partCount](int ownerPartId) {  // 判断部件归属是否越界。
        return ownerPartId < -1 || ownerPartId > partCount;
    };
    const auto reject = [&writer, &component](const QString& context, int ownerPartId) {  // 记录越界归属诊断。
        writer.m_diagnostics.append(QStringLiteral("Altium SchLib 组件 %1 的%2 OWNERPARTID=%3 超出部件范围，已拒绝写入")
                                        .arg(component.name, context)
                                        .arg(ownerPartId));
        qWarning() << "AltiumSchLibWriter:" << writer.m_diagnostics.constLast();
        return false;
    };

    const auto validate = [&isOutOfRange, &reject](const auto& objects, const QString& context) {  // 检查一组图元归属。
        for (const auto& object : objects) {
            if (isOutOfRange(object.ownerPartId))
                return reject(context, object.ownerPartId);
        }
        return true;
    };

    return validate(component.pins, QStringLiteral("引脚")) &&
           validate(component.rectangles, QStringLiteral("矩形图元")) &&
           validate(component.roundRectangles, QStringLiteral("圆角矩形图元")) &&
           validate(component.lines, QStringLiteral("线段图元")) &&
           validate(component.arcs, QStringLiteral("圆弧图元")) &&
           validate(component.polygons, QStringLiteral("多边形图元")) &&
           validate(component.ellipses, QStringLiteral("椭圆图元")) &&
           validate(component.pies, QStringLiteral("扇形图元")) &&
           validate(component.ellipticalArcs, QStringLiteral("椭圆弧图元")) &&
           validate(component.polylines, QStringLiteral("折线图元")) &&
           validate(component.paths, QStringLiteral("路径图元")) &&
           validate(component.beziers, QStringLiteral("Bézier 图元")) &&
           validate(component.ieeeSymbols, QStringLiteral("IEEE 图元")) &&
           validate(component.texts, QStringLiteral("文本图元")) &&
           validate(component.textFrames, QStringLiteral("文本框图元")) &&
           validate(component.images, QStringLiteral("图片图元")) &&
           validate(component.parameters, QStringLiteral("参数"));
}

}  // namespace EasyKiConverter
