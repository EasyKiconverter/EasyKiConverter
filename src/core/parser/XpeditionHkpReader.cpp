#include "XpeditionHkpReader.h"

namespace EasyKiConverter::Parser {

namespace {

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

XpeditionHkpDocument XpeditionHkpReader::parse(const QString& content, const QString& filePath) {
    XpeditionHkpDocument document;
    document.diagnostics.setFilePath(filePath);
    if (content.trimmed().isEmpty()) {
        document.diagnostics.add(ParseSeverity::Error, ParseScope::File, QStringLiteral("HKP 文件为空"));
        return document;
    }

    document.sections = IndentedSectionParser::parse(content, &document.diagnostics);
    for (const SectionNode& section : document.sections) {
        if (section.keyword == QStringLiteral("FILETYPE"))
            document.type = parseType(section.value);
        else if (section.keyword == QStringLiteral("UNITS"))
            document.unit = UnitConverter::parseUnit(unquote(section.value), &document.diagnostics);
    }
    if (!document.isRecognized())
        document.diagnostics.add(
            ParseSeverity::Error, ParseScope::File, QStringLiteral("未识别的 Xpedition HKP 文件类型"));
    if (document.unit == LengthUnit::Unknown)
        document.diagnostics.add(
            ParseSeverity::Warning, ParseScope::File, QStringLiteral("HKP 文件未声明可识别的单位"));
    return document;
}

}  // namespace EasyKiConverter::Parser
