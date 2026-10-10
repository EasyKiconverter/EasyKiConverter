#include "AltiumSymbolImplementationConverter.h"

#include <QSet>

namespace EasyKiConverter {

/**
 * @brief 根据符号封装候选、显式模型和来源元数据创建实现记录。
 * @param symbol 统一 IR 中的符号组件。
 * @return 去重后的 Altium 实现记录列表。
 */
QList<AltiumSchComponent::Implementation> AltiumSymbolImplementationConverter::convert(
    const IR::SymbolComponentIR& symbol) {
    QList<AltiumSchComponent::Implementation> implementations;  // 待写入 Altium 符号的实现记录。

    QStringList footprintNames = symbol.footprintNames;  // 符号关联的封装名称候选。
    if (footprintNames.isEmpty() && !symbol.footprintName.isEmpty())
        footprintNames.append(symbol.footprintName);

    QSet<QString> uniqueFootprints;  // 已处理的规范化封装名称。
    for (const QString& footprintName : footprintNames) {
        const QString normalizedName = footprintName.trimmed();  // 去除空白后的封装名称。
        if (normalizedName.isEmpty() || uniqueFootprints.contains(normalizedName))
            continue;
        AltiumSchComponent::Implementation implementation;  // 当前封装对应的实现记录。
        implementation.modelName = normalizedName;
        implementation.modelType = QStringLiteral("PCBLIB");
        implementation.dataFileKind = QStringLiteral("PCBLib");
        implementations.append(implementation);
        uniqueFootprints.insert(normalizedName);
    }

    for (const IR::SymbolModelIR& model : symbol.models) {
        const QString modelName = model.name.trimmed();  // 当前模型的规范化名称。
        if (modelName.isEmpty())
            continue;
        AltiumSchComponent::Implementation implementation;  // 当前显式模型对应的实现记录。
        implementation.modelName = modelName;
        implementation.modelType = model.type.trimmed().isEmpty() ? QStringLiteral("SIM") : model.type.trimmed();
        implementation.dataFileKind = model.fileKind.trimmed();
        implementation.dataFileEntity = model.fileEntity.trimmed();
        implementation.parameters = model.parameters;
        implementation.pinMappings = model.pinMappings;
        implementations.append(implementation);
    }

    // 兼容没有显式 SymbolModelIR 的调用方，允许通过来源元数据关联模型。
    const QMap<QString, QString> metadataModels = {
        // 来源元数据键到 Altium 模型类型的映射。
        {QStringLiteral("spiceModel"), QStringLiteral("SPICE")},
        {QStringLiteral("simulationModel"), QStringLiteral("SIM")},
        {QStringLiteral("model3D"), QStringLiteral("STEP")},
        {QStringLiteral("model3d"), QStringLiteral("STEP")},
    };
    for (auto it = metadataModels.constBegin(); it != metadataModels.constEnd(); ++it) {
        const QString modelName = symbol.sourceMetadata.value(it.key()).trimmed();  // 来源元数据中的模型名称。
        if (modelName.isEmpty())
            continue;
        AltiumSchComponent::Implementation implementation;  // 来源元数据对应的实现记录。
        implementation.modelName = modelName;
        implementation.modelType = it.value();
        implementation.dataFileKind = symbol.sourceMetadata.value(it.key() + QStringLiteral("FileKind")).trimmed();
        implementation.dataFileEntity = symbol.sourceMetadata.value(it.key() + QStringLiteral("File")).trimmed();
        implementations.append(implementation);
    }

    return implementations;
}

}  // namespace EasyKiConverter
