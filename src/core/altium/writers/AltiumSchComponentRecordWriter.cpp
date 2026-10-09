#include "AltiumSchComponentRecordWriter.h"

#include "AltiumSchLibWriter.h"
#include "utils/AltiumCoord.h"

#include <QSet>

#include <cmath>

namespace EasyKiConverter {

namespace {

/** @brief 表示一个待写入 SchLib RECORD=41 的参数字段。 */
struct ParameterField {
    QString name;  ///< 参数名称。
    QString value;  ///< 参数显示值。
    int locationX = 0;  ///< 参数 X 坐标。
    int locationY = 0;  ///< 参数 Y 坐标。
    int fontId = 1;  ///< Altium 字体表索引。
    uint32_t color = 0x000000;  ///< 参数文本颜色。
    bool hasLocation = false;  ///< 是否存在显式位置。
    bool hasVisibility = false;  ///< 是否提供可见性状态。
    bool isHidden = true;  ///< 参数是否隐藏。
    bool readOnly = false;  ///< 参数是否只读。
    int orientation = 0;  ///< 参数文本方向。
    int ownerPartId = -1;  ///< 所属元件部件编号。
    double fontSizeMm = 0.0;  ///< 参数字体大小，单位为毫米。
};

/**
 * @brief 将来源元数据和显式参数统一转换为参数字段。
 * @details 保留未知元数据键，使新增供应商字段无需修改 SchLib 协议模型。
 * @param component 待转换的原理图元件。
 * @return 去重后的参数字段列表。
 */
QList<ParameterField> componentParameterFields(const AltiumSchComponent& component) {
    static const QMap<QString, QString> knownNames = {
        // 来源元数据到 Altium 字段名的映射。
        {QStringLiteral("description"), QStringLiteral("Description")},
        {QStringLiteral("manufacturer"), QStringLiteral("Manufacturer")},
        {QStringLiteral("manufacturerPart"), QStringLiteral("Manufacturer Part Number")},
        {QStringLiteral("datasheet"), QStringLiteral("Datasheet")},
        {QStringLiteral("lcscId"), QStringLiteral("LCSC Part")},
        {QStringLiteral("jlcId"), QStringLiteral("JLCPCB Part")},
        {QStringLiteral("jlcpcbPartClass"), QStringLiteral("JLCPCB Part Class")},
        {QStringLiteral("supplierPart"), QStringLiteral("Supplier Part Number")},
        {QStringLiteral("supplier"), QStringLiteral("Supplier")},
        {QStringLiteral("category"), QStringLiteral("Category")},
        {QStringLiteral("source"), QStringLiteral("Source")},
    };

    QList<ParameterField> fields;  // 按输出顺序收集的参数字段。
    QSet<QString> names;  // 已写入参数的去重键集合。
    const auto appendField = [&](const QString& name,
                                 const QString& value,
                                 bool hasLocation = false,
                                 int locationX = 0,
                                 int locationY = 0,
                                 bool hasVisibility = false,
                                 bool isHidden = true,
                                 bool readOnly = false,
                                 int orientation = 0,
                                 int ownerPartId = -1,
                                 int fontId = 1,
                                 uint32_t color = 0x000000,
                                 double fontSizeMm = 0.0) {
        const QString normalizedName = name.trimmed();  // 去除空白后的参数名称。
        const QString deduplicationKey = QStringLiteral("%1:%2").arg(normalizedName).arg(ownerPartId);  // 参数去重键。
        if (normalizedName.isEmpty() || value.trimmed().isEmpty() || names.contains(deduplicationKey))
            return;
        ParameterField field;  // 当前规范化后的参数字段。
        field.name = normalizedName;
        field.value = value.trimmed();
        field.hasLocation = hasLocation;
        field.locationX = locationX;
        field.locationY = locationY;
        field.hasVisibility = hasVisibility;
        field.isHidden = isHidden;
        field.readOnly = readOnly;
        field.orientation = orientation;
        field.ownerPartId = ownerPartId;
        field.fontId = fontId;
        field.color = color;
        field.fontSizeMm = fontSizeMm;
        fields.append(field);
        names.insert(deduplicationKey);
    };

    QString value = component.sourceMetadata.value(QStringLiteral("value")).trimmed();  // 来源元数据中的器件值。
    if (value.isEmpty())
        value = component.name.trimmed();
    if (!value.isEmpty())
        appendField(QStringLiteral("Comment"), value);

    QString description = component.sourceMetadata.value(QStringLiteral("description")).trimmed();  // 元件描述。
    if (description.isEmpty())
        description = component.description.trimmed();
    if (!description.isEmpty())
        appendField(QStringLiteral("Description"), description);

    for (auto it = component.sourceMetadata.constBegin(); it != component.sourceMetadata.constEnd(); ++it) {
        const QString metadataValue = it.value().trimmed();  // 当前元数据值。
        if (metadataValue.isEmpty() || it.key() == QStringLiteral("value") || it.key() == QStringLiteral("description"))
            continue;
        appendField(knownNames.value(it.key(), it.key()), metadataValue);
    }

    if (!component.aliases.isEmpty())
        appendField(QStringLiteral("Aliases"), component.aliases.join(QStringLiteral(", ")));

    for (const AltiumSchParameter& parameter : component.parameters) {
        QString name = parameter.name.trimmed();  // 当前参数名称。
        if (name.compare(QStringLiteral("Value"), Qt::CaseInsensitive) == 0)
            name = QStringLiteral("Comment");
        appendField(name,
                    parameter.value,
                    parameter.locationX != 0 || parameter.locationY != 0,
                    parameter.locationX,
                    parameter.locationY,
                    true,
                    parameter.isHidden,
                    parameter.readOnly,
                    parameter.orientation,
                    parameter.ownerPartId,
                    parameter.fontId,
                    parameter.color,
                    parameter.fontSizeMm);
    }
    return fields;
}

}  // namespace

/**
 * @brief 创建 SchLib 元件记录写入器。
 * @param owner 所属的 SchLib 主写入器。
 */
AltiumSchComponentRecordWriter::AltiumSchComponentRecordWriter(AltiumSchLibWriter& owner) : m_owner(owner) {}

/**
 * @brief 写入 Designator、来源元数据和显式参数字段。
 * @param writer 目标二进制写入器。
 * @param component 待写入的原理图元件。
 */
void AltiumSchComponentRecordWriter::writeParameters(AltiumBinaryWriter& writer, const AltiumSchComponent& component) {
    QString designator = component.designatorPrefix.trimmed();  // 规范化后的元件标识符。
    if (designator.isEmpty())
        designator = "?";
    else if (!designator.endsWith('?'))
        designator += '?';

    QMap<QString, QString> designatorParams;  // Designator RECORD=34 字段。
    designatorParams["RECORD"] = "34";
    designatorParams["OWNERPARTID"] = "-1";
    designatorParams["LOCATION.X_FRAC"] = "-5";
    designatorParams["LOCATION.Y_FRAC"] = "5";
    designatorParams["COLOR"] = "8388608";
    designatorParams["FONTID"] = "1";
    designatorParams["TEXT"] = designator;
    designatorParams["NAME"] = "Designator";
    designatorParams["READONLYSTATE"] = "1";
    m_owner.addUniqueID(designatorParams);
    writer.writeCStringParameterBlockUtf8(designatorParams);

    for (const ParameterField& field : componentParameterFields(component)) {  // 写入每个参数字段。
        QMap<QString, QString> parameterParams;  // 当前参数的 RECORD=41 字段。
        parameterParams["RECORD"] = "41";
        const int ownerPartId =
            m_owner.normalizeOwnerPartId(field.ownerPartId, QStringLiteral("参数"));  // 规范化后的部件编号。
        parameterParams["OWNERPARTID"] = QString::number(ownerPartId);
        if (ownerPartId >= 1) {
            parameterParams["OWNERPARTDISPLAYMODE"] = "1";
            m_owner.addContentIndex(parameterParams);
        }
        if (field.hasLocation) {
            m_owner.addCoordParam(parameterParams, "LOCATION.X", field.locationX);
            m_owner.addCoordParam(parameterParams, "LOCATION.Y", field.locationY);
        } else {
            parameterParams["LOCATION.X_FRAC"] = "-5";
            parameterParams["LOCATION.Y_FRAC"] = "-15";
        }
        if (field.color != 0)
            parameterParams["COLOR"] = QString::number(field.color);
        int fontId = field.fontId;  // 当前参数使用的字体 ID。
        if (std::isfinite(field.fontSizeMm) && field.fontSizeMm > 0.0) {
            constexpr double MILLIMETERS_PER_POINT = 25.4 / 72.0;  // 点制字体到毫米的换算系数。
            fontId = m_owner.m_fontRegistry.getOrAdd(QStringLiteral("Times New Roman"),
                                                     qMax(1, qRound(field.fontSizeMm / MILLIMETERS_PER_POINT)));
        } else if (fontId < 1 || fontId > m_owner.m_fontRegistry.size()) {
            fontId = 1;
        }
        parameterParams["FONTID"] = QString::number(fontId);
        parameterParams["TEXT"] = field.value;
        parameterParams["NAME"] = field.name;
        if (field.orientation != 0)
            parameterParams["Orientation"] = QString::number(field.orientation);
        if (field.hasVisibility && field.isHidden)
            parameterParams["IsHidden"] = "T";
        if (field.readOnly)
            parameterParams["READONLYSTATE"] = "1";
        m_owner.addUniqueID(parameterParams);
        writer.writeCStringParameterBlockUtf8(parameterParams);
    }
}

/**
 * @brief 写入实现容器、模型信息、引脚映射和实现参数。
 * @param writer 目标二进制写入器。
 * @param component 待写入的原理图元件。
 */
void AltiumSchComponentRecordWriter::writeImplementations(AltiumBinaryWriter& writer,
                                                          const AltiumSchComponent& component) {
    QMap<QString, QString> listParams;  // RECORD=44 实现列表字段。
    listParams["RECORD"] = "44";
    listParams["DataFileFormatID"] = "";
    listParams["Description"] = "";
    listParams["FileName"] = "";
    writer.writeCStringParameterBlockUtf8(listParams);

    for (int implementationIndex = 0; implementationIndex < component.implementations.size(); ++implementationIndex) {
        const AltiumSchComponent::Implementation& implementation =
            component.implementations.at(implementationIndex);  // 当前封装实现。
        QMap<QString, QString> implementationParams;  // RECORD=45 实现字段。
        implementationParams["RECORD"] = "45";
        implementationParams["DESCRIPTION"] = implementation.modelName;
        implementationParams["MODELNAME"] = implementation.modelName;
        implementationParams["MODELTYPE"] = implementation.modelType;
        const QString modelType = implementation.modelType.trimmed().toUpper();  // 规范化后的模型类型。
        const bool isPcbLibrary = modelType.isEmpty() || modelType == QStringLiteral("PCBLIB");  // 是否为 PCB 库实现。
        const bool hasExplicitDataFileKind =  // 是否提供了非默认数据文件类型。
            !implementation.dataFileKind.trimmed().isEmpty() &&
            (isPcbLibrary || implementation.dataFileKind.compare(QStringLiteral("PCBLib"), Qt::CaseInsensitive) != 0);
        const bool hasDataFile =  // 当前实现是否引用数据文件。
            isPcbLibrary || hasExplicitDataFileKind || !implementation.dataFileEntity.trimmed().isEmpty();
        implementationParams["DATAFILECOUNT"] = hasDataFile ? "1" : "0";
        if (hasDataFile) {
            implementationParams["MODELDATAFILEKIND1"] =
                implementation.dataFileKind.isEmpty() ? "PCBLib" : implementation.dataFileKind;
            implementationParams["MODELDATAFILEENTITY1"] =
                implementation.dataFileEntity.isEmpty()
                    ? (isPcbLibrary && !m_owner.m_libraryName.isEmpty() ? m_owner.m_libraryName + ".PcbLib" : "*")
                    : implementation.dataFileEntity;
        }
        // Altium 只允许一个默认实现，候选封装和附加模型不能全部标记为当前。
        if (implementationIndex == 0)
            implementationParams["ISCURRENT"] = "T";
        m_owner.addUniqueID(implementationParams);
        writer.writeCStringParameterBlockUtf8(implementationParams);

        QMap<QString, QString> mapListParams;  // RECORD=46 引脚映射列表字段。
        mapListParams["RECORD"] = "46";
        writer.writeCStringParameterBlockUtf8(mapListParams);

        // Altium 通过 RECORD=47 建立符号引脚与当前 PCBLib 实现之间的关联。
        for (int pinIndex = 1; pinIndex <= component.pins.size(); ++pinIndex) {
            QMap<QString, QString> pinMappingParams;  // 当前引脚的 RECORD=47 映射字段。
            pinMappingParams["RECORD"] = "47";
            pinMappingParams["DESINTF"] = QString::number(pinIndex);
            pinMappingParams["DESIMPCOUNT"] = "1";
            pinMappingParams["DESIMP0"] =
                implementation.pinMappings.value(QString::number(pinIndex), QString::number(pinIndex));
            pinMappingParams["ISTRIVIAL"] = "T";
            m_owner.addUniqueID(pinMappingParams);
            writer.writeCStringParameterBlockUtf8(pinMappingParams);
        }

        QMap<QString, QString> parameterParams;  // RECORD=48 实现参数字段。
        parameterParams["RECORD"] = "48";
        for (auto it = implementation.parameters.constBegin(); it != implementation.parameters.constEnd(); ++it)
            parameterParams[it.key()] = it.value();
        writer.writeCStringParameterBlockUtf8(parameterParams);
    }
}

/**
 * @brief 返回 Designator 加上所有去重后的参数字段数量。
 * @param component 待统计的原理图元件。
 * @return 需要写入的参数记录数量。
 */
int AltiumSchComponentRecordWriter::parameterRecordCount(const AltiumSchComponent& component) {
    return 1 + componentParameterFields(component).size();
}

}  // namespace EasyKiConverter
