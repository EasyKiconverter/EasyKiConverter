#include "TextParsers.h"

#include <QRegularExpression>
#include <QSet>

namespace EasyKiConverter::Parser {

namespace {

// 统一处理 Unix 和 Windows 换行，保留空行对应的行号。
QStringList linesOf(const QString& content) {
    return content.split(QRegularExpression(QStringLiteral("\\r?\\n")));
}

// 解析一行 S-expression，同时保留引号和括号语义。
QStringList tokenizeSExpressionLine(const QString& line, int lineNumber, ParseDiagnostics* diagnostics) {
    QStringList tokens;
    QString current;
    bool quoted = false;
    bool escaped = false;
    for (int index = 0; index < line.size(); ++index) {
        const QChar character = line.at(index);
        if (escaped) {
            current += character;
            escaped = false;
        } else if (quoted && character == QChar('\\')) {
            escaped = true;
        } else if (character == QChar('"')) {
            quoted = !quoted;
            if (!quoted) {
                tokens.append(current);
                current.clear();
            }
        } else if (!quoted && (character == QChar('(') || character == QChar(')'))) {
            if (!current.isEmpty()) {
                tokens.append(current);
                current.clear();
            }
            tokens.append(QString(character));
        } else if (!quoted && character.isSpace()) {
            if (!current.isEmpty()) {
                tokens.append(current);
                current.clear();
            }
        } else {
            current += character;
        }
    }
    if (escaped)
        current += QChar('\\');
    if (quoted && diagnostics)
        diagnostics->add(ParseSeverity::Error, ParseScope::File, QStringLiteral("字符串引号未闭合"), {}, lineNumber);
    if (!current.isEmpty())
        tokens.append(current);
    return tokens;
}

}  // namespace

// 生成带源位置的公共词法单元，供格式专用解析器复用。
QList<TextToken> TextTokenizer::tokenize(const QString& content, ParseDiagnostics* diagnostics) {
    QList<TextToken> tokens;
    const QStringList lines = linesOf(content);
    for (int lineIndex = 0; lineIndex < lines.size(); ++lineIndex) {
        const QString& line = lines.at(lineIndex);
        QString current;
        int startColumn = 0;
        bool quoted = false;
        bool escaped = false;
        for (int index = 0; index < line.size(); ++index) {
            const QChar character = line.at(index);
            if (escaped) {
                current += character;
                escaped = false;
            } else if (quoted && character == QChar('\\')) {
                escaped = true;
            } else if (character == QChar('"')) {
                if (current.isEmpty())
                    startColumn = index + 1;
                quoted = !quoted;
                if (!quoted) {
                    tokens.append({current, lineIndex + 1, startColumn, true});
                    current.clear();
                }
            } else if (!quoted && (character.isSpace() || character == QChar(',') || character == QChar('(') ||
                                   character == QChar(')'))) {
                if (!current.isEmpty()) {
                    tokens.append({current, lineIndex + 1, startColumn, false});
                    current.clear();
                }
                if (character == QChar('(') || character == QChar(')'))
                    tokens.append({QString(character), lineIndex + 1, index + 1, false});
            } else {
                if (current.isEmpty())
                    startColumn = index + 1;
                current += character;
            }
        }
        if (escaped)
            current += QChar('\\');
        if (!current.isEmpty())
            tokens.append({current, lineIndex + 1, startColumn, quoted});
        if (quoted && diagnostics)
            diagnostics->add(
                ParseSeverity::Error, ParseScope::File, QStringLiteral("字符串引号未闭合"), {}, lineIndex + 1);
    }
    return tokens;
}

// 根据点数量建立 HKP 等格式的父子节点关系。
QList<SectionNode> IndentedSectionParser::parse(const QString& content, ParseDiagnostics* diagnostics) {
    struct StackEntry {
        SectionNode* node = nullptr;
        int depth = 0;
    };

    QList<SectionNode> roots;
    QList<StackEntry> stack;
    const QStringList lines = linesOf(content);
    for (int lineIndex = 0; lineIndex < lines.size(); ++lineIndex) {
        const int lineNumber = lineIndex + 1;
        const QString rawLine = lines.at(lineIndex).trimmed();
        if (rawLine.isEmpty() || rawLine.startsWith(QChar('!')) || rawLine.startsWith(QChar('#')))
            continue;

        int markerCount = 0;
        while (markerCount < rawLine.size() && rawLine.at(markerCount) == QChar('.'))
            ++markerCount;
        if (markerCount == 0) {
            if (diagnostics)
                diagnostics->add(ParseSeverity::Warning,
                                 ParseScope::File,
                                 QStringLiteral("跳过没有层级标记的文本行"),
                                 {},
                                 lineNumber);
            continue;
        }

        const QString payload = rawLine.mid(markerCount).trimmed();
        if (payload.isEmpty()) {
            if (diagnostics)
                diagnostics->add(
                    ParseSeverity::Warning, ParseScope::File, QStringLiteral("跳过空的层级节点"), {}, lineNumber);
            continue;
        }
        const int separator = payload.indexOf(QRegularExpression(QStringLiteral("\\s")));
        SectionNode node;
        node.keyword = (separator < 0 ? payload : payload.left(separator)).trimmed();
        node.value = separator < 0 ? QString() : payload.mid(separator).trimmed();
        node.line = lineNumber;
        node.depth = markerCount;

        while (!stack.isEmpty() && stack.last().depth >= markerCount)
            stack.removeLast();
        if (stack.isEmpty()) {
            roots.append(node);
            stack.append({&roots.last(), markerCount});
        } else {
            stack.last().node->children.append(node);
            stack.append({&stack.last().node->children.last(), markerCount});
        }
    }
    return roots;
}

namespace {

// 解析 Cadstar 行中的关键字、引号参数和括号坐标，保留参数边界。
QStringList tokenizeDelimitedLine(const QString& line) {
    QStringList tokens;
    QString current;
    bool quoted = false;
    int parentheses = 0;
    for (const QChar character : line) {
        if (character == QChar('"')) {
            quoted = !quoted;
            continue;
        }
        if (!quoted && character == QChar('('))
            ++parentheses;
        if (!quoted && character == QChar(')'))
            --parentheses;
        const bool separator = !quoted && parentheses == 0 && (character.isSpace() || character == QChar(','));
        if (separator) {
            if (!current.isEmpty()) {
                tokens.append(current);
                current.clear();
            }
        } else {
            current.append(character);
        }
    }
    if (!current.isEmpty())
        tokens.append(current);
    return tokens;
}

}  // namespace

// 根据显式 END* 标记建立 Cadstar 风格的分段树。
QList<DelimitedSectionNode> DelimitedSectionParser::parse(const QString& content,
                                                          const QStringList& leafKeywords,
                                                          ParseDiagnostics* diagnostics) {
    const QSet<QString> leaves = QSet<QString>(leafKeywords.cbegin(), leafKeywords.cend());
    QList<DelimitedSectionNode> roots;
    QList<DelimitedSectionNode*> stack;
    const QStringList lines = linesOf(content);
    for (int index = 0; index < lines.size(); ++index) {
        const int lineNumber = index + 1;
        const QString line = lines.at(index).trimmed();
        if (line.isEmpty() || line.startsWith(QChar('!')) || line.startsWith(QChar('#')) || line.startsWith(QChar('*')))
            continue;
        const QStringList tokens = tokenizeDelimitedLine(line);
        if (tokens.isEmpty())
            continue;
        const QString keyword = tokens.first().toUpper();
        if (keyword.startsWith(QStringLiteral("END"))) {
            if (stack.isEmpty()) {
                if (diagnostics)
                    diagnostics->add(ParseSeverity::Warning,
                                     ParseScope::File,
                                     QStringLiteral("没有对应起点的结束标记：%1").arg(keyword),
                                     {},
                                     lineNumber);
                continue;
            }
            DelimitedSectionNode* section = stack.takeLast();
            const QString expected = QStringLiteral("END") + section->keyword;
            if (keyword != expected && diagnostics)
                diagnostics->add(ParseSeverity::Warning,
                                 ParseScope::File,
                                 QStringLiteral("结束标记与分段不匹配：期望 %1，实际 %2").arg(expected, keyword),
                                 section->keyword,
                                 lineNumber);
            section->closed = true;
            continue;
        }
        DelimitedSectionNode node;
        node.keyword = keyword;
        node.arguments = tokens.mid(1);
        node.line = lineNumber;
        if (stack.isEmpty())
            roots.append(node);
        else
            stack.last()->children.append(node);
        DelimitedSectionNode* inserted = stack.isEmpty() ? &roots.last() : &stack.last()->children.last();
        if (!leaves.contains(keyword))
            stack.append(inserted);
    }
    while (!stack.isEmpty()) {
        DelimitedSectionNode* section = stack.takeLast();
        if (diagnostics)
            diagnostics->add(ParseSeverity::Error,
                             ParseScope::File,
                             QStringLiteral("分段缺少结束标记：%1").arg(section->keyword),
                             {},
                             section->line);
    }
    return roots;
}

// 将括号列表构造成带行号的嵌套语法树。
QList<SExpressionNode> SExpressionParser::parse(const QString& content, ParseDiagnostics* diagnostics) {
    QList<SExpressionNode> roots;
    QList<SExpressionNode*> stack;
    const QStringList lines = linesOf(content);
    for (int lineIndex = 0; lineIndex < lines.size(); ++lineIndex) {
        const int lineNumber = lineIndex + 1;
        const QString line = lines.at(lineIndex);
        const QStringList tokens = tokenizeSExpressionLine(line, lineNumber, diagnostics);
        for (const QString& token : tokens) {
            if (token == QStringLiteral("(")) {
                SExpressionNode node;
                node.line = lineNumber;
                if (stack.isEmpty()) {
                    roots.append(node);
                    stack.append(&roots.last());
                } else {
                    stack.last()->children.append(node);
                    stack.append(&stack.last()->children.last());
                }
            } else if (token == QStringLiteral(")")) {
                if (stack.isEmpty()) {
                    if (diagnostics)
                        diagnostics->add(ParseSeverity::Error,
                                         ParseScope::File,
                                         QStringLiteral("S-expression 出现多余右括号"),
                                         {},
                                         lineNumber);
                } else {
                    stack.removeLast();
                }
            } else if (stack.isEmpty()) {
                if (diagnostics)
                    diagnostics->add(ParseSeverity::Warning,
                                     ParseScope::File,
                                     QStringLiteral("跳过根列表之外的原子"),
                                     token,
                                     lineNumber);
            } else {
                SExpressionNode* current = stack.last();
                if (current->atom.isEmpty() && current->children.isEmpty()) {
                    current->atom = token;
                } else {
                    current->children.append({token, {}, lineNumber});
                }
            }
        }
    }
    if (!stack.isEmpty() && diagnostics)
        diagnostics->add(ParseSeverity::Error, ParseScope::File, QStringLiteral("S-expression 缺少右括号"));
    return roots;
}

// 以扩展名和内容头部作保守判断，无法确认时不强行猜测格式。
DetectedFormat FormatDetector::detect(const QString& fileName, const QByteArray& content) {
    const QString lowerName = fileName.toLower();
    const QByteArray head = content.left(4096).trimmed();
    if (lowerName.endsWith(QStringLiteral(".json")) && head.startsWith('{'))
        return DetectedFormat::EasyedaJson;
    if (lowerName.endsWith(QStringLiteral(".dsn")) || head.contains("<TinyCAD"))
        return DetectedFormat::TinyCadXml;
    if (lowerName.endsWith(QStringLiteral(".psk.hkp")) || lowerName.endsWith(QStringLiteral(".cel.hkp")) ||
        lowerName.endsWith(QStringLiteral(".pdb.hkp")) || head.contains(".FILETYPE PADSTACK_LIBRARY") ||
        head.contains(".FILETYPE CELL_LIBRARY") || head.contains(".FILETYPE ASCII_PDB"))
        return DetectedFormat::XpeditionHkp;
    if (head.startsWith("V ") || head.startsWith("V\t"))
        return DetectedFormat::XpeditionSymbol;
    if (lowerName.endsWith(QStringLiteral(".lib")) && head.contains(".LIB"))
        return DetectedFormat::PcadSExpression;
    if (head.contains("CADSTAR") || lowerName.endsWith(QStringLiteral(".cpa")) ||
        lowerName.endsWith(QStringLiteral(".cpa")))
        return DetectedFormat::CadstarAscii;
    if (head.startsWith("PCB["))
        return DetectedFormat::GedaLegacy;
    return DetectedFormat::Unknown;
}

}  // namespace EasyKiConverter::Parser
