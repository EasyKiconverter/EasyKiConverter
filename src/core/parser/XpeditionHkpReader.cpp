#include "XpeditionHkpReader.h"

namespace EasyKiConverter::Parser {

namespace {

// 去除 HKP 字段外围引号，统一后续的关键字和关联名称比较。
QString unquote(QString value) {
    value = value.trimmed();
    if (value.size() >= 2 && value.startsWith(QChar('"')) && value.endsWith(QChar('"')))
        return value.mid(1, value.size() - 2);
    return value;
}

}  // namespace

// 将 HKP 文件头中的 FILETYPE 映射为后续格式模型使用的枚举。
XpeditionHkpType parseType(const QString& value) {
    const QString type = unquote(value).toUpper();
    if (type == QStringLiteral("PADSTACK_LIBRARY"))
        return XpeditionHkpType::PadstackLibrary;
    if (type == QStringLiteral("CELL_LIBRARY"))
        return XpeditionHkpType::CellLibrary;
    if (type == QStringLiteral("ASCII_PDB"))
        return XpeditionHkpType::PartsDatabase;
    return XpeditionHkpType::Unknown;
}

// 解析 HKP 文本并构建格式专用模型，同时保留全部字段级诊断。
XpeditionHkpDocument XpeditionHkpReader::parse(const QString& content, const QString& filePath) {
    XpeditionHkpDocument document;
    document.diagnostics.setFilePath(filePath);
    if (content.trimmed().isEmpty()) {
        document.diagnostics.add(ParseSeverity::Error, ParseScope::File, QStringLiteral("HKP 文件为空"));
        return document;
    }

    document.sections = IndentedSectionParser::parse(content, &document.diagnostics);
    for (const SectionNode& section : document.sections) {
        if (section.keyword.toUpper() == QStringLiteral("FILETYPE"))
            document.type = parseType(section.value);
        else if (section.keyword.toUpper() == QStringLiteral("UNITS"))
            document.unit = UnitConverter::parseUnit(unquote(section.value), &document.diagnostics);
    }
    if (document.type == XpeditionHkpType::Unknown) {
        for (const SectionNode& section : document.sections) {
            const QString keyword = section.keyword.toUpper();
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

}  // namespace EasyKiConverter::Parser
