#include "XpeditionHkpReader.h"

#include "EncodingDetector.h"

#include <QSet>

namespace EasyKiConverter::Parser {

namespace {

/**
 * @brief 去除 HKP 字段外围引号。
 * @param value 待处理字段。
 * @return 去除成对外围引号后的文本。
 */
QString unquote(QString value) {
    value = value.trimmed();
    if (value.size() >= 2 && value.startsWith(QChar('"')) && value.endsWith(QChar('"')))
        return value.mid(1, value.size() - 2);
    return value;
}

}  // namespace

/**
 * @brief 将 HKP 文件头中的 FILETYPE 映射为格式模型枚举。
 * @param value FILETYPE 字段值。
 * @return 对应的 HKP 文件类型。
 */
XpeditionHkpType parseType(const QString& value) {
    const QString type = unquote(value).toUpper();  // 规范化后的文件类型名称。
    if (type == QStringLiteral("PADSTACK_LIBRARY"))
        return XpeditionHkpType::PadstackLibrary;
    if (type == QStringLiteral("CELL_LIBRARY"))
        return XpeditionHkpType::CellLibrary;
    if (type == QStringLiteral("ASCII_PDB"))
        return XpeditionHkpType::PartsDatabase;
    return XpeditionHkpType::Unknown;
}

/**
 * @brief 解析 HKP 文本并构建格式专用模型。
 * @param content HKP 文本内容。
 * @param filePath 可选源文件路径。
 * @return 格式模型和结构化诊断。
 */
XpeditionHkpDocument XpeditionHkpReader::parse(const QString& content, const QString& filePath) {
    XpeditionHkpDocument document;  // 当前 HKP 文档及其诊断。
    document.diagnostics.setFilePath(filePath);
    if (content.trimmed().isEmpty()) {
        document.diagnostics.add(ParseSeverity::Error, ParseScope::File, QStringLiteral("HKP 文件为空"));
        return document;
    }

    document.sections = IndentedSectionParser::parse(content, &document.diagnostics);
    for (const SectionNode& section : document.sections) {  // 读取显式文件头字段。
        if (section.keyword.toUpper() == QStringLiteral("FILETYPE"))
            document.type = parseType(section.value);
        else if (section.keyword.toUpper() == QStringLiteral("UNITS"))
            document.unit = UnitConverter::parseUnit(unquote(section.value), &document.diagnostics);
    }
    if (document.type == XpeditionHkpType::Unknown) {
        for (const SectionNode& section : document.sections) {
            const QString keyword = section.keyword.toUpper();  // 当前分段关键字。
            if (keyword == QStringLiteral("PAD") || keyword == QStringLiteral("PADSTACK") ||
                keyword == QStringLiteral("HOLE")) {
                document.type = XpeditionHkpType::PadstackLibrary;
                break;
            }
            if (keyword == QStringLiteral("PACKAGE_CELL")) {
                document.type = XpeditionHkpType::CellLibrary;
                break;
            }
            if (keyword == QStringLiteral("NUMBER")) {
                document.type = XpeditionHkpType::PartsDatabase;
                break;
            }
        }
    }
    if (!document.isRecognized())
        document.diagnostics.add(
            ParseSeverity::Error, ParseScope::File, QStringLiteral("未识别的 Xpedition HKP 文件类型"));
    if (document.unit == LengthUnit::Unknown)
        document.diagnostics.add(
            ParseSeverity::Warning, ParseScope::File, QStringLiteral("HKP 文件未声明可识别的单位"));
    document.model = XpeditionHkpModelParser::parse(document.sections, document.unit, &document.diagnostics);
    return document;
}

/**
 * @brief 先检测文本编码，再复用统一字符串解析流程。
 * @param data 原始文件字节。
 * @param filePath 可选源文件路径。
 * @return 解码并解析后的 HKP 文档。
 */
XpeditionHkpDocument XpeditionHkpReader::parseBytes(const QByteArray& data, const QString& filePath) {
    ParseDiagnostics decodingDiagnostics;  // 编码检测阶段的诊断。
    decodingDiagnostics.setFilePath(filePath);
    const QString content = EncodingDetector::decode(data, &decodingDiagnostics, filePath);  // 解码后的文本。
    XpeditionHkpDocument document = parse(content, filePath);  // 复用文本解析结果。
    document.diagnostics.append(decodingDiagnostics);
    return document;
}

namespace {

/**
 * @brief 根据已有名称生成跨文件合并时稳定且唯一的名称。
 * @param name 原始名称。
 * @param usedNames 已占用名称集合。
 * @return 可安全使用的唯一名称。
 */
QString mergedName(const QString& name, const QSet<QString>& usedNames) {
    if (!usedNames.contains(name))
        return name;
    int suffix = 2;  // 名称冲突时使用的起始后缀。
    QString candidate;  // 当前候选名称。
    do {
        candidate = QStringLiteral("%1_%2").arg(name).arg(suffix++);
    } while (usedNames.contains(candidate));
    return candidate;
}

/**
 * @brief 从源文件候选表反向取得某个唯一名称对应的原始名称。
 * @param variants 原始名称到唯一名称候选的映射。
 * @param unique 唯一名称。
 * @return 对应的原始名称；找不到时返回输入名称。
 */
QString originalName(const QMap<QString, QStringList>& variants, const QString& unique) {
    for (auto iterator = variants.cbegin(); iterator != variants.cend(); ++iterator) {
        if (iterator.value().contains(unique))
            return iterator.key();
    }
    return unique;
}

/**
 * @brief 合并 Pad、孔、Padstack 或 Cell 定义并更新原始名称候选表。
 * @param source 待合并定义。
 * @param target 合并结果集合。
 * @param variants 合并后的名称候选表。
 * @param usedNames 已占用名称集合。
 * @param sourceVariants 源文档名称候选表。
 * @param diagnostics 诊断接收器。
 * @param scope 诊断作用域。
 */
template <typename Definition>
void appendDefinition(const Definition& source,
                      QList<Definition>& target,
                      QMap<QString, QStringList>& variants,
                      QSet<QString>& usedNames,
                      const QMap<QString, QStringList>& sourceVariants,
                      ParseDiagnostics* diagnostics,
                      ParseScope scope) {
    Definition merged = source;  // 当前定义的合并副本。
    const QString rawName = originalName(sourceVariants, source.name);  // 源文件中的原始名称。
    merged.name = mergedName(source.name, usedNames);
    if (merged.name != source.name && diagnostics)
        diagnostics->add(ParseSeverity::Warning,
                         scope,
                         QStringLiteral("跨文件定义名称重复，已重命名：%1 -> %2").arg(source.name, merged.name),
                         rawName,
                         source.line);
    usedNames.insert(merged.name);
    target.append(merged);
    variants[rawName].append(merged.name);
}

/**
 * @brief 报告跨文件原始名称对应多个定义，阻止后续引用静默选择。
 * @param variants 原始名称到候选名称的映射。
 * @param diagnostics 诊断接收器。
 * @param scope 诊断作用域。
 * @param type 定义类型名称。
 */
void reportAmbiguousVariants(const QMap<QString, QStringList>& variants,
                             ParseDiagnostics* diagnostics,
                             ParseScope scope,
                             const QString& type) {
    for (auto iterator = variants.cbegin(); iterator != variants.cend(); ++iterator) {
        if (iterator.value().size() > 1 && diagnostics)
            diagnostics->add(ParseSeverity::Error,
                             scope,
                             QStringLiteral("跨文件%1名称存在歧义：%2").arg(type, iterator.key()),
                             iterator.key());
    }
}

}  // namespace

/**
 * @brief 合并 HKP 文件并对单位、类型和跨文件重名做显式诊断。
 * @param documents 待合并 HKP 文档。
 * @param filePath 合并结果的逻辑路径。
 * @return 合并后的文档模型和诊断。
 */
XpeditionHkpDocument XpeditionHkpMerger::merge(const QList<XpeditionHkpDocument>& documents, const QString& filePath) {
    XpeditionHkpDocument merged;  // 合并后的 HKP 文档。
    merged.diagnostics.setFilePath(filePath);
    if (documents.isEmpty()) {
        merged.diagnostics.add(ParseSeverity::Error, ParseScope::File, QStringLiteral("没有可合并的 HKP 文档"));
        return merged;
    }

    QSet<QString> padNames;  // 已使用的 Pad 名称。
    QSet<QString> holeNames;  // 已使用的孔名称。
    QSet<QString> padstackNames;  // 已使用的 Padstack 名称。
    QSet<QString> cellNames;  // 已使用的 Cell 名称。
    for (const XpeditionHkpDocument& document : documents) {  // 逐个合并输入文档。
        merged.diagnostics.append(document.diagnostics);
        if (!document.isRecognized())
            continue;
        if (merged.type == XpeditionHkpType::Unknown)
            merged.type = document.type;
        else if (merged.type != document.type)
            merged.diagnostics.add(
                ParseSeverity::Error, ParseScope::File, QStringLiteral("HKP 文档类型不一致，无法安全合并"), filePath);
        if (merged.unit == LengthUnit::Unknown)
            merged.unit = document.unit;
        else if (document.unit != LengthUnit::Unknown && merged.unit != document.unit)
            merged.diagnostics.add(
                ParseSeverity::Error, ParseScope::File, QStringLiteral("HKP 文档单位不一致，无法安全合并"), filePath);
        merged.sections.append(document.sections);
        for (const XpeditionPadDefinition& item : document.model.pads)
            appendDefinition(item,
                             merged.model.pads,
                             merged.model.padNameVariants,
                             padNames,
                             document.model.padNameVariants,
                             &merged.diagnostics,
                             ParseScope::Footprint);
        for (const XpeditionHoleDefinition& item : document.model.holes)
            appendDefinition(item,
                             merged.model.holes,
                             merged.model.holeNameVariants,
                             holeNames,
                             document.model.holeNameVariants,
                             &merged.diagnostics,
                             ParseScope::Footprint);
        for (const XpeditionPadstackDefinition& item : document.model.padstacks)
            appendDefinition(item,
                             merged.model.padstacks,
                             merged.model.padstackNameVariants,
                             padstackNames,
                             document.model.padstackNameVariants,
                             &merged.diagnostics,
                             ParseScope::Footprint);
        for (const XpeditionCellDefinition& item : document.model.cells)
            appendDefinition(item,
                             merged.model.cells,
                             merged.model.cellNameVariants,
                             cellNames,
                             document.model.cellNameVariants,
                             &merged.diagnostics,
                             ParseScope::Footprint);
        for (const XpeditionPartDefinition& item : document.model.parts) {
            for (const XpeditionPartDefinition& existing : merged.model.parts) {
                if (existing.number == item.number)
                    merged.diagnostics.add(ParseSeverity::Error,
                                           ParseScope::Component,
                                           QStringLiteral("跨文件器件编号重复：%1").arg(item.number),
                                           item.number,
                                           item.line);
            }
            merged.model.parts.append(item);
        }
    }
    if (merged.type == XpeditionHkpType::Unknown)
        merged.diagnostics.add(ParseSeverity::Error, ParseScope::File, QStringLiteral("没有可识别的 HKP 文档"));
    reportAmbiguousVariants(
        merged.model.padNameVariants, &merged.diagnostics, ParseScope::Footprint, QStringLiteral("Pad"));
    reportAmbiguousVariants(
        merged.model.holeNameVariants, &merged.diagnostics, ParseScope::Footprint, QStringLiteral("孔"));
    reportAmbiguousVariants(
        merged.model.padstackNameVariants, &merged.diagnostics, ParseScope::Footprint, QStringLiteral("Padstack"));
    reportAmbiguousVariants(
        merged.model.cellNameVariants, &merged.diagnostics, ParseScope::Footprint, QStringLiteral("Cell"));
    merged.model.parts.squeeze();
    return merged;
}

}  // namespace EasyKiConverter::Parser
