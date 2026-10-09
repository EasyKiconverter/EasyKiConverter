#include "TextParsers.h"

#include <QRegularExpression>
#include <QSet>

namespace EasyKiConverter::Parser {

namespace {

/**
 * @brief 统一处理 Unix 和 Windows 换行，保留空行对应的行号。
 * @param content 输入文本。
 * @return 按换行拆分后的文本行。
 */
QStringList linesOf(const QString& content) {
    return content.split(QRegularExpression(QStringLiteral("\\r?\\n")));
}

/**
 * @brief 解析一行 S-expression，同时保留引号和括号语义。
 * @param line 待解析文本行。
 * @param lineNumber 源文件行号。
 * @param diagnostics 诊断接收器，可为空。
 * @return 词法单元列表。
 */
QStringList tokenizeSExpressionLine(const QString& line, int lineNumber, ParseDiagnostics* diagnostics) {
    QStringList tokens;  ///< 当前行解析出的词法单元。
    QString current;  ///< 正在累积的词法单元文本。
    bool quoted = false;  ///< 是否处于引号文本中。
    bool escaped = false;  ///< 是否正在处理转义字符。
    for (int index = 0; index < line.size(); ++index) {
        const QChar character = line.at(index);  ///< 当前字符。
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

/**
 * @brief 生成带源位置的公共词法单元，供格式专用解析器复用。
 * @param content 输入文本。
 * @param diagnostics 诊断接收器，可为空。
 * @return 带源位置的词法单元列表。
 */
QList<TextToken> TextTokenizer::tokenize(const QString& content, ParseDiagnostics* diagnostics) {
    QList<TextToken> tokens;  ///< 全文词法单元列表。
    const QStringList lines = linesOf(content);  ///< 按源文件行拆分的文本。
    for (int lineIndex = 0; lineIndex < lines.size(); ++lineIndex) {
        const QString& line = lines.at(lineIndex);  ///< 当前源文件行。
        QString current;  ///< 当前行正在累积的词法单元文本。
        int startColumn = 0;  ///< 当前词法单元起始列。
        bool quoted = false;  ///< 是否处于引号文本中。
        bool escaped = false;  ///< 是否正在处理转义字符。
        for (int index = 0; index < line.size(); ++index) {
            const QChar character = line.at(index);  ///< 当前字符。
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

/**
 * @brief 根据点数量建立 HKP 等格式的父子节点关系。
 * @param content 输入文本。
 * @param diagnostics 诊断接收器，可为空。
 * @return 根节点列表。
 */
QList<SectionNode> IndentedSectionParser::parse(const QString& content, ParseDiagnostics* diagnostics) {
    struct StackEntry {
        SectionNode* node = nullptr;  ///< 当前栈项对应的节点。
        int depth = 0;  ///< 当前节点深度。
    };

    QList<SectionNode> roots;  ///< 根节点列表。
    QList<StackEntry> stack;  ///< 当前未闭合节点栈。
    const QStringList lines = linesOf(content);  ///< 按源文件行拆分的文本。
    for (int lineIndex = 0; lineIndex < lines.size(); ++lineIndex) {
        const int lineNumber = lineIndex + 1;  ///< 当前源文件行号。
        const QString rawLine = lines.at(lineIndex).trimmed();  ///< 去除首尾空白后的当前行。
        if (rawLine.isEmpty() || rawLine.startsWith(QChar('!')) || rawLine.startsWith(QChar('#')))
            continue;

        int markerCount = 0;  ///< 当前行的点号缩进数量。
        while (markerCount < rawLine.size() && rawLine.at(markerCount) == QChar('.'))
            ++markerCount;
        if (markerCount == 0) {
            if (rawLine.startsWith(QChar('(')) && !stack.isEmpty() &&
                stack.last().node->keyword.toUpper() == QStringLiteral("XY")) {
                stack.last().node->value += QChar(' ') + rawLine;
                continue;
            }
            if (diagnostics)
                diagnostics->add(ParseSeverity::Warning,
                                 ParseScope::File,
                                 QStringLiteral("跳过没有层级标记的文本行"),
                                 {},
                                 lineNumber);
            continue;
        }

        const QString payload = rawLine.mid(markerCount).trimmed();  ///< 去除缩进标记后的节点内容。
        if (payload.isEmpty()) {
            if (diagnostics)
                diagnostics->add(
                    ParseSeverity::Warning, ParseScope::File, QStringLiteral("跳过空的层级节点"), {}, lineNumber);
            continue;
        }
        const int separator = payload.indexOf(QRegularExpression(QStringLiteral("\\s")));  ///< 关键字和值的分隔位置。
        SectionNode node;  ///< 当前解析出的节点。
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

/**
 * @brief 解析 Cadstar 行中的关键字、引号参数和括号坐标，保留参数边界。
 * @param line 待解析文本行。
 * @return 解析出的字段列表。
 */
QStringList tokenizeDelimitedLine(const QString& line) {
    QStringList tokens;  ///< 当前行解析出的字段。
    QString current;  ///< 正在累积的字段文本。
    bool quoted = false;  ///< 是否处于引号文本中。
    int parentheses = 0;  ///< 当前括号嵌套深度。
    for (const QChar character : line) {
        if (character == QChar('"')) {
            quoted = !quoted;
            continue;
        }
        if (!quoted && character == QChar('('))
            ++parentheses;
        if (!quoted && character == QChar(')'))
            --parentheses;
        const bool separator =  ///< 当前字符是否构成字段分隔符。
            !quoted && parentheses == 0 && (character.isSpace() || character == QChar(','));
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

/**
 * @brief 根据显式 END* 标记建立 Cadstar 风格的分段树。
 * @param content 输入文本。
 * @param leafKeywords 叶节点关键字列表。
 * @param diagnostics 诊断接收器，可为空。
 * @return 根分段节点列表。
 */
QList<DelimitedSectionNode> DelimitedSectionParser::parse(const QString& content,
                                                          const QStringList& leafKeywords,
                                                          ParseDiagnostics* diagnostics) {
    const QSet<QString> leaves = QSet<QString>(leafKeywords.cbegin(), leafKeywords.cend());  ///< 叶节点关键字集合。
    QList<DelimitedSectionNode> roots;  ///< 根分段节点列表。
    QList<DelimitedSectionNode*> stack;  ///< 当前未闭合分段栈。
    const QStringList lines = linesOf(content);  ///< 按源文件行拆分的文本。

    // 先收集文件中实际出现的结束标记，使同一关键字可以同时作为
    // 顶层容器和嵌套引用。例如 Cadstar 的 PAD 有 ENDPAD 容器，
    // 但封装 PIN 内的 PAD 只是单行引用。
    QSet<QString> endKeywords;  ///< 文件中实际出现的结束关键字。
    for (const QString& sourceLine : lines) {
        const QStringList tokens = tokenizeDelimitedLine(sourceLine.trimmed());  ///< 当前行的字段列表。
        if (!tokens.isEmpty() && tokens.first().toUpper().startsWith(QStringLiteral("END")) &&
            tokens.first().toUpper() != QStringLiteral("END"))
            endKeywords.insert(tokens.first().toUpper().mid(3));
    }

    for (int index = 0; index < lines.size(); ++index) {
        const int lineNumber = index + 1;  ///< 当前源文件行号。
        const QString line = lines.at(index).trimmed();  ///< 去除首尾空白后的当前行。
        if (line.isEmpty() || line.startsWith(QChar('!')) || line.startsWith(QChar('#')) || line.startsWith(QChar('*')))
            continue;
        const QStringList tokens = tokenizeDelimitedLine(line);  ///< 当前行的字段列表。
        if (tokens.isEmpty())
            continue;
        const QString keyword = tokens.first().toUpper();  ///< 当前分段关键字。
        if (keyword.startsWith(QStringLiteral("END")) && keyword != QStringLiteral("END")) {
            if (stack.isEmpty()) {
                if (diagnostics)
                    diagnostics->add(ParseSeverity::Warning,
                                     ParseScope::File,
                                     QStringLiteral("没有对应起点的结束标记：%1").arg(keyword),
                                     {},
                                     lineNumber);
                continue;
            }
            DelimitedSectionNode* section = stack.takeLast();  ///< 当前待闭合分段。
            const QString expected = QStringLiteral("END") + section->keyword;  ///< 期望的结束关键字。
            if (keyword != expected && diagnostics)
                diagnostics->add(ParseSeverity::Warning,
                                 ParseScope::File,
                                 QStringLiteral("结束标记与分段不匹配：期望 %1，实际 %2").arg(expected, keyword),
                                 section->keyword,
                                 lineNumber);
            section->closed = true;
            continue;
        }
        DelimitedSectionNode node;  ///< 当前新建的分段节点。
        node.keyword = keyword;
        node.arguments = tokens.mid(1);
        node.line = lineNumber;
        if (stack.isEmpty())
            roots.append(node);
        else
            stack.last()->children.append(node);
        DelimitedSectionNode* inserted =
            stack.isEmpty() ? &roots.last() : &stack.last()->children.last();  ///< 新节点指针。
        const bool nestedReference =  ///< 当前关键字是否属于嵌套引用而非容器。
            !stack.isEmpty() && (keyword == QStringLiteral("PAD") || keyword == QStringLiteral("PACKAGE") ||
                                 keyword == QStringLiteral("COMPONENT"));
        if (!leaves.contains(keyword) || (endKeywords.contains(keyword) && !nestedReference))
            stack.append(inserted);
    }
    while (!stack.isEmpty()) {
        DelimitedSectionNode* section = stack.takeLast();  ///< 未闭合的分段节点。
        if (diagnostics)
            diagnostics->add(ParseSeverity::Error,
                             ParseScope::File,
                             QStringLiteral("分段缺少结束标记：%1").arg(section->keyword),
                             {},
                             section->line);
    }
    return roots;
}

/**
 * @brief 将括号列表构造成带行号的嵌套语法树。
 * @param content 输入文本。
 * @param diagnostics 诊断接收器，可为空。
 * @return S-expression 根节点列表。
 */
QList<SExpressionNode> SExpressionParser::parse(const QString& content, ParseDiagnostics* diagnostics) {
    QList<SExpressionNode> roots;  ///< 根节点列表。
    QList<SExpressionNode*> stack;  ///< 当前列表节点栈。
    const QStringList lines = linesOf(content);  ///< 按源文件行拆分的文本。
    for (int lineIndex = 0; lineIndex < lines.size(); ++lineIndex) {
        const int lineNumber = lineIndex + 1;  ///< 当前源文件行号。
        const QString line = lines.at(lineIndex);  ///< 当前源文件行。
        const QStringList tokens = tokenizeSExpressionLine(line, lineNumber, diagnostics);  ///< 当前行词法单元。
        for (const QString& token : tokens) {
            if (token == QStringLiteral("(")) {
                SExpressionNode node;  ///< 新建的列表节点。
                node.line = lineNumber;
                node.listNode = true;
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
                SExpressionNode* current = stack.last();  ///< 当前正在接收原子的列表节点。
                if (current->atom.isEmpty() && current->children.isEmpty()) {
                    current->atom = token;
                } else {
                    SExpressionNode scalar;  ///< 新建的标量子节点。
                    scalar.atom = token;
                    scalar.line = lineNumber;
                    current->children.append(scalar);
                }
            }
        }
    }
    if (!stack.isEmpty() && diagnostics)
        diagnostics->add(ParseSeverity::Error, ParseScope::File, QStringLiteral("S-expression 缺少右括号"));
    return roots;
}

/**
 * @brief 以扩展名和内容头部作保守判断，无法确认时不强行猜测格式。
 * @param fileName 文件名或路径。
 * @param content 文件内容或头部。
 * @return 检测出的格式。
 */
DetectedFormat FormatDetector::detect(const QString& fileName, const QByteArray& content) {
    const QString lowerName = fileName.toLower();  ///< 小写文件名。
    const QByteArray head = content.left(4096).trimmed();  ///< 限长并清理后的内容头部。
    const QByteArray lowerHead = head.toLower();  ///< 小写内容头部。
    if (lowerName.endsWith(QStringLiteral(".json")) && head.startsWith('{'))
        return DetectedFormat::EasyedaJson;
    if (lowerName.endsWith(QStringLiteral(".dsn")) || lowerHead.contains("<tinycad"))
        return DetectedFormat::TinyCadXml;
    if (lowerName.endsWith(QStringLiteral(".psk.hkp")) || lowerName.endsWith(QStringLiteral(".cel.hkp")) ||
        lowerName.endsWith(QStringLiteral(".pdb.hkp")) || lowerHead.contains(".filetype padstack_library") ||
        lowerHead.contains(".filetype cell_library") || lowerHead.contains(".filetype ascii_pdb"))
        return DetectedFormat::XpeditionHkp;
    if (head.startsWith("V ") || head.startsWith("V\t"))
        return DetectedFormat::XpeditionSymbol;
    if (lowerHead.contains("pcb[") || lowerHead.contains("pcb(") ||
        QRegularExpression(QStringLiteral("fileversion\\[\\d+\\]")).match(QString::fromUtf8(lowerHead)).hasMatch())
        return DetectedFormat::GedaLegacy;
    if (lowerName.endsWith(QStringLiteral(".pcb")) || lowerHead.contains("accel_ascii"))
        return DetectedFormat::PcadSExpression;
    if (lowerHead.contains("cadstar") || lowerName.endsWith(QStringLiteral(".cpa")))
        return DetectedFormat::CadstarAscii;
    return DetectedFormat::Unknown;
}

}  // namespace EasyKiConverter::Parser
