#include "AltiumPcbInputValidator.h"

#include "AltiumPcbLibWriter.h"

#include <QDebug>
#include <QSet>

#include <cmath>

namespace EasyKiConverter {

/**
 * @brief 校验封装名称、图元结构和输出路径。
 * @details 诊断统一写入主写入器，确保调用方仍可通过 diagnostics() 获取拒绝原因。
 * @param owner 所属的 PCB 库写入器
 * @param components 待校验的封装列表
 * @param filePath 目标文件路径
 * @return 输入有效时返回 true
 */
bool AltiumPcbInputValidator::validate(AltiumPcbLibWriter& owner,
                                       const QList<AltiumPcbComponent>& components,
                                       const QString& filePath) {
    const auto reject = [&owner](const QString& diagnostic) {
        owner.m_diagnostics.append(diagnostic);
        qWarning() << "AltiumPcbLibWriter:" << diagnostic;
        return false;
    };

    if (components.isEmpty())
        return reject(QStringLiteral("Altium PcbLib 输入封装为空，已拒绝写入"));
    if (filePath.trimmed().isEmpty())
        return reject(QStringLiteral("Altium PcbLib 输出路径为空，已拒绝写入"));
    if (filePath.contains(QChar('|')) || filePath.contains(QChar::Null) || filePath.contains(QChar('\r')) ||
        filePath.contains(QChar('\n')))
        return reject(QStringLiteral("Altium PcbLib 输出路径包含参数分隔符、换行或 NUL，已拒绝写入"));

    const auto normalizedModelId = [](QString value) {  // 规范化 3D 模型标识。
        value.replace('|', ' ');
        value.replace('\0', ' ');
        value.replace('\r', ' ');
        value.replace('\n', ' ');
        return value.trimmed().toCaseFolded();
    };
    const auto isLosslessLatin1 = [](const QString& value) {  // 检查字符串是否可无损编码为 Latin-1。
        const QByteArray encoded = value.toLatin1();  // Latin-1 编码结果。
        return QString::fromLatin1(encoded) == value;
    };
    const auto hasModelMetadataDelimiter = [](const QString& value) {  // 检查模型元数据分隔符。
        return value.contains(QChar('|')) || value.contains(QChar::Null) || value.contains(QChar('\r')) ||
               value.contains(QChar('\n'));
    };
    QSet<QString> componentNames;  // 已校验的封装名称集合。
    for (const AltiumPcbComponent& component : components) {
        const auto validateCStringField = [&reject, &component, &isLosslessLatin1](const QString& value,
                                                                                   const QString& context) {
            if (value.contains(QChar('|')) || value.contains(QChar::Null))
                return reject(QStringLiteral("Altium PcbLib 封装 %1 的%2包含参数分隔符或 NUL，已拒绝写入")
                                  .arg(component.name, context));
            if (value.contains(QChar('\r')) || value.contains(QChar('\n')))
                return reject(
                    QStringLiteral("Altium PcbLib 封装 %1 的%2包含换行字符，已拒绝写入").arg(component.name, context));
            if (!isLosslessLatin1(value))
                return reject(QStringLiteral("Altium PcbLib 封装 %1 的%2包含无法编码的字符，已拒绝写入")
                                  .arg(component.name, context));
            return true;
        };

        if (component.name.trimmed().isEmpty())
            return reject(QStringLiteral("Altium PcbLib 封装名称为空，已拒绝写入"));
        if (!validateCStringField(component.name, QStringLiteral("封装名称")))
            return false;
        if (!validateCStringField(component.description, QStringLiteral("封装描述")))
            return false;
        if (component.name.toLatin1().size() > 255)
            return reject(QStringLiteral("Altium PcbLib 封装 %1 名称超过 255 字节，已拒绝写入").arg(component.name));
        if (!isLosslessLatin1(component.name))
            return reject(
                QStringLiteral("Altium PcbLib 封装名称包含无法编码的字符: %1，已拒绝写入").arg(component.name));
        if (!std::isfinite(component.height) || component.height < 0.0)
            return reject(
                QStringLiteral("Altium PcbLib 封装 %1 的高度必须为非负有限值，已拒绝写入").arg(component.name));
        const QString foldedName = component.name.trimmed().toCaseFolded();  // 用于重复检查的折叠名称。
        if (componentNames.contains(foldedName))
            return reject(
                QStringLiteral("Altium PcbLib 封装名称重复（不区分大小写）: %1，已拒绝写入").arg(component.name));
        componentNames.insert(foldedName);

        for (const AltiumPcbPad& pad : component.pads) {
            if (pad.designator.toLatin1().size() > 255)
                return reject(
                    QStringLiteral("Altium PcbLib 封装 %1 的焊盘编号超过 255 字节，已拒绝写入").arg(component.name));
            if (!isLosslessLatin1(pad.designator))
                return reject(QStringLiteral("Altium PcbLib 封装 %1 的焊盘编号包含无法编码的字符，已拒绝写入")
                                  .arg(component.name));
            if (pad.isSMD && (pad.layer < 1 || pad.layer > 74))
                return reject(QStringLiteral("Altium PcbLib 封装 %1 包含无效焊盘层号，已拒绝写入").arg(component.name));
            const auto isValidPadShape = [](uint8_t shape) {  // 检查焊盘形状编码。
                return shape == static_cast<uint8_t>(AltiumModels::PadShape::Round) ||
                       shape == static_cast<uint8_t>(AltiumModels::PadShape::Rectangular) ||
                       shape == static_cast<uint8_t>(AltiumModels::PadShape::Octagonal) ||
                       shape == static_cast<uint8_t>(AltiumModels::PadShape::RoundedRectangle);
            };
            if (!isValidPadShape(pad.shapeTop) || !isValidPadShape(pad.shapeMid) || !isValidPadShape(pad.shapeBot))
                return reject(QStringLiteral("Altium PcbLib 封装 %1 包含无效焊盘形状，已拒绝写入").arg(component.name));
            if (pad.sizeTopX <= 0 || pad.sizeTopY <= 0 || pad.sizeMidX <= 0 || pad.sizeMidY <= 0 || pad.sizeBotX <= 0 ||
                pad.sizeBotY <= 0)
                return reject(QStringLiteral("Altium PcbLib 封装 %1 包含非正焊盘尺寸，已拒绝写入").arg(component.name));
            if (!pad.isSMD && pad.holeSize <= 0)
                return reject(QStringLiteral("Altium PcbLib 封装 %1 包含非正通孔尺寸，已拒绝写入").arg(component.name));
            if (pad.cornerRadiusPercentage > 100 || pad.mode > 3 || pad.powerPlaneConnectStyle > 2 ||
                (pad.reliefEntries != 2 && pad.reliefEntries != 4) || pad.drillType > 2 || pad.holeType > 2)
                return reject(
                    QStringLiteral("Altium PcbLib 封装 %1 包含无效焊盘扩展属性，已拒绝写入").arg(component.name));
            if (pad.holeType == 2 && pad.holeSlotLengthRaw <= 0)
                return reject(QStringLiteral("Altium PcbLib 封装 %1 的槽孔长度非正，已拒绝写入").arg(component.name));
        }
        for (const AltiumPcbTrack& track : component.tracks) {
            if (track.layer < 1 || track.layer > 74)
                return reject(QStringLiteral("Altium PcbLib 封装 %1 包含无效走线层号，已拒绝写入").arg(component.name));
            if (track.width <= 0)
                return reject(QStringLiteral("Altium PcbLib 封装 %1 包含非正走线宽度，已拒绝写入").arg(component.name));
        }
        for (const AltiumPcbText& text : component.texts) {
            if (text.text.toLatin1().size() > 255)
                return reject(
                    QStringLiteral("Altium PcbLib 封装 %1 的文本内容超过 255 字节，已拒绝写入").arg(component.name));
            if (!isLosslessLatin1(text.text))
                return reject(QStringLiteral("Altium PcbLib 封装 %1 的文本内容包含无法编码的字符，已拒绝写入")
                                  .arg(component.name));
            if (text.layer < 1 || text.layer > 74)
                return reject(QStringLiteral("Altium PcbLib 封装 %1 包含无效文本层号，已拒绝写入").arg(component.name));
            if (text.height <= 0 || text.strokeWidth < 0)
                return reject(QStringLiteral("Altium PcbLib 封装 %1 包含无效文本尺寸，已拒绝写入").arg(component.name));
        }
        for (const AltiumPcbArc& arc : component.arcs) {
            if (arc.layer < 1 || arc.layer > 74)
                return reject(QStringLiteral("Altium PcbLib 封装 %1 包含无效弧线层号，已拒绝写入").arg(component.name));
            if (arc.radius <= 0)
                return reject(QStringLiteral("Altium PcbLib 封装 %1 包含非正弧线半径，已拒绝写入").arg(component.name));
            if (arc.width <= 0)
                return reject(QStringLiteral("Altium PcbLib 封装 %1 包含非正弧线宽度，已拒绝写入").arg(component.name));
        }
        for (const AltiumPcbFill& fill : component.fills) {
            if (fill.layer < 1 || fill.layer > 74)
                return reject(QStringLiteral("Altium PcbLib 封装 %1 包含无效填充层号，已拒绝写入").arg(component.name));
        }
        for (const AltiumPcbRegion& region : component.regions) {
            if (region.layer < 1 || region.layer > 74)
                return reject(QStringLiteral("Altium PcbLib 封装 %1 包含无效区域层号，已拒绝写入").arg(component.name));
            if (region.vertices.size() < 3)
                return reject(QStringLiteral("Altium PcbLib 封装 %1 区域顶点不足，已拒绝写入").arg(component.name));
            if (!validateCStringField(region.v7LayerName, QStringLiteral("区域 V7 层名称")) ||
                !validateCStringField(region.net, QStringLiteral("区域网络名称")) ||
                !validateCStringField(region.uniqueId, QStringLiteral("区域唯一标识")) ||
                !validateCStringField(region.name, QStringLiteral("区域名称")))
                return false;
        }
        QSet<QString> modelIds;  // 当前封装已声明的 3D 模型标识集合。
        for (const AltiumPcbComponent::Model3D& model : component.models) {
            if (model.name.trimmed().isEmpty())
                return reject(
                    QStringLiteral("Altium PcbLib 封装 %1 的 3D 模型名称为空，已拒绝写入").arg(component.name));
            if (hasModelMetadataDelimiter(model.name) || hasModelMetadataDelimiter(model.id))
                return reject(
                    QStringLiteral("Altium PcbLib 封装 %1 的 3D 模型元数据包含参数分隔符、换行或 NUL，已拒绝写入")
                        .arg(component.name));
            if (!isLosslessLatin1(model.name))
                return reject(QStringLiteral("Altium PcbLib 封装 %1 的 3D 模型名称包含无法编码的字符，已拒绝写入")
                                  .arg(component.name));
            if (model.stepData.isEmpty())
                return reject(
                    QStringLiteral("Altium PcbLib 封装 %1 的 3D 模型数据为空，已拒绝写入").arg(component.name));
            if (!model.id.isEmpty() && !isLosslessLatin1(model.id))
                return reject(QStringLiteral("Altium PcbLib 封装 %1 的 3D 模型 ID 包含无法编码的字符，已拒绝写入")
                                  .arg(component.name));
            const QString modelId = normalizedModelId(model.id.isEmpty() ? model.name : model.id);  // 规范化模型标识。
            if (!model.id.isEmpty() && modelId.isEmpty())
                return reject(
                    QStringLiteral("Altium PcbLib 封装 %1 的 3D 模型 ID 规范化后为空，已拒绝写入").arg(component.name));
            if (!modelId.isEmpty() && modelIds.contains(modelId))
                return reject(
                    QStringLiteral("Altium PcbLib 封装 %1 的 3D 模型 ID 重复，已拒绝写入").arg(component.name));
            if (!modelId.isEmpty())
                modelIds.insert(modelId);
        }
        for (const AltiumPcbComponentBody& body : component.bodies) {
            if (!validateCStringField(body.layerName, QStringLiteral("3D 元件体层名称")) ||
                !validateCStringField(body.name, QStringLiteral("3D 元件体名称")) ||
                !validateCStringField(body.modelId, QStringLiteral("3D 元件体模型 ID")) ||
                !validateCStringField(body.modelName, QStringLiteral("3D 元件体模型名称")) ||
                !validateCStringField(body.modelSource, QStringLiteral("3D 元件体模型来源")))
                return false;
            const QString normalizedLayer = body.layerName.trimmed().toUpper();  // 规范化后的机械层名称。
            bool layerNumberOk = false;  // 机械层编号转换是否成功。
            int layerNumber = 0;  // 解析得到的机械层编号。
            if (normalizedLayer.startsWith(QStringLiteral("MECHANICAL")))
                layerNumber = normalizedLayer.mid(10).toInt(&layerNumberOk);
            if (!layerNumberOk || layerNumber < 1 || layerNumber > 16)
                return reject(
                    QStringLiteral("Altium PcbLib 封装 %1 包含无效 3D 元件体层名，已拒绝写入").arg(component.name));
            if (body.kind < 0 || body.kind > 2)
                return reject(
                    QStringLiteral("Altium PcbLib 封装 %1 包含无效 3D 元件体类型，已拒绝写入").arg(component.name));
            if (std::isfinite(body.bodyOpacity3d) && (body.bodyOpacity3d < 0.0 || body.bodyOpacity3d > 1.0))
                return reject(
                    QStringLiteral("Altium PcbLib 封装 %1 包含无效 3D 元件体透明度，已拒绝写入").arg(component.name));
            if (body.modelType < 1 || body.modelType > 2)
                return reject(
                    QStringLiteral("Altium PcbLib 封装 %1 包含无效 3D 模型类型，已拒绝写入").arg(component.name));
            if (!component.models.isEmpty()) {
                const QString bodyModelId = normalizedModelId(body.modelId);  // 元件体关联模型标识。
                if (bodyModelId.isEmpty() || !modelIds.contains(bodyModelId))
                    return reject(QStringLiteral("Altium PcbLib 封装 %1 的 3D 元件体未关联有效模型，已拒绝写入")
                                      .arg(component.name));
            }
        }
        const int primitiveCount = owner.countPrimitives(component);  // 当前封装图元总数。
        QSet<int> extendedPrimitiveIndices;  // 已使用的扩展图元索引集合。
        for (const AltiumPcbExtendedPrimitiveInfo& info : component.extendedPrimitives) {
            if (info.primitiveIndex < 0 || info.primitiveIndex >= primitiveCount)
                return reject(QStringLiteral("Altium PcbLib 封装 %1 的扩展图元索引无效: %2，已拒绝写入")
                                  .arg(component.name)
                                  .arg(info.primitiveIndex));
            if (info.objectName.trimmed().isEmpty())
                return reject(
                    QStringLiteral("Altium PcbLib 封装 %1 的扩展图元对象名为空，已拒绝写入").arg(component.name));
            if (!validateCStringField(info.objectName, QStringLiteral("扩展图元对象名")))
                return false;
            for (auto it = info.params.cbegin(); it != info.params.cend(); ++it) {
                if (it.key().trimmed().isEmpty())
                    return reject(
                        QStringLiteral("Altium PcbLib 封装 %1 的扩展参数键为空，已拒绝写入").arg(component.name));
                if (!validateCStringField(it.key(), QStringLiteral("扩展参数键")) ||
                    !validateCStringField(it.value(), QStringLiteral("扩展参数值")))
                    return false;
            }
            if (extendedPrimitiveIndices.contains(info.primitiveIndex))
                return reject(QStringLiteral("Altium PcbLib 封装 %1 的扩展图元索引重复: %2，已拒绝写入")
                                  .arg(component.name)
                                  .arg(info.primitiveIndex));
            extendedPrimitiveIndices.insert(info.primitiveIndex);
        }
    }
    return true;
}

}  // namespace EasyKiConverter
