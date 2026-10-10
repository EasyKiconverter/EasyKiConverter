#pragma once

/**
 * @file TextParsers.h
 * @brief 供多种 EDA 文本格式复用的结构解析基础设施。
 */

#include "ParseDiagnostics.h"

#include <QByteArray>
#include <QList>
#include <QPointF>
#include <QString>

namespace EasyKiConverter::Parser {

/**
 * @brief 文本词法单元。
 */
struct TextToken {
    QString text;  ///< 词法单元文本。
    int line = 0;  ///< 源文件行号，从 1 开始。
    int column = 0;  ///< 源文件列号，从 1 开始。
    bool quoted = false;  ///< 是否来自引号包围的文本。
};

/**
 * @brief 将格式文本拆分为带位置的词法单元。
 */
class TextTokenizer {
public:
    /**
     * @brief 按空白、逗号、括号和引号拆分文本。
     * @param content UTF-8 文本。
     * @param diagnostics 诊断接收器，可为空。
     * @return 按源文件顺序排列的词法单元。
     */
    static QList<TextToken> tokenize(const QString& content, ParseDiagnostics* diagnostics = nullptr);
};

/**
 * @brief 点缩进或空白缩进文本中的一层节点。
 */
struct SectionNode {
    QString keyword;  ///< 节点关键字。
    QString value;  ///< 节点值文本。
    QList<SectionNode> children;  ///< 嵌套子节点。
    int line = 0;  ///< 源文件行号。
    int depth = 0;  ///< 点号表示的缩进深度。
};

/**
 * @brief 使用 END* 终止符的分段文本节点。
 */
struct DelimitedSectionNode {
    QString keyword;  ///< 分段关键字。
    QStringList arguments;  ///< 分段参数列表。
    QList<DelimitedSectionNode> children;  ///< 嵌套子分段。
    int line = 0;  ///< 起始行号。
    bool closed = false;  ///< 是否遇到了对应的结束标记。
};

/**
 * @brief 解析 Xpedition HKP 等点缩进文本。
 * @details 解析阶段只建立树，不猜测格式字段，具体格式模型在上层读取器中完成。
 */
class IndentedSectionParser {
public:
    /**
     * @brief 解析点缩进文本。
     * @param content UTF-8 文本。
     * @param diagnostics 诊断接收器，可为空。
     * @return 根节点列表。
     */
    static QList<SectionNode> parse(const QString& content, ParseDiagnostics* diagnostics = nullptr);
};

/**
 * @brief 解析 Cadstar 等 END* 终止符分段格式。
 * @details 叶节点由调用方提供，未知字段仍保留在树中，未闭合节点会产生错误诊断。
 */
class DelimitedSectionParser {
public:
    /**
     * @brief 解析带显式结束标记的分段文本。
     * @param content UTF-8 文本。
     * @param leafKeywords 不带 END 前缀的叶节点关键字。
     * @param diagnostics 诊断接收器，可为空。
     * @return 分段节点树。
     */
    static QList<DelimitedSectionNode> parse(const QString& content,
                                             const QStringList& leafKeywords = {},
                                             // 可选诊断接收器。
                                             ParseDiagnostics* diagnostics = nullptr);
};

/**
 * @brief S-expression 节点，支持嵌套列表和带引号原子。
 */
struct SExpressionNode {
    QString atom;  ///< 原子文本。
    QList<SExpressionNode> children;  ///< 嵌套子节点。
    int line = 0;  ///< 源文件行号。
    bool listNode = false;  ///< 是否由括号包围形成列表节点。

    /**
     * @brief 判断节点是否为列表节点。
     * @return 节点由括号包围时返回 true。
     */
    bool isList() const {
        return listNode;
    }
};

/**
 * @brief 解析 P-CAD 等常见 S-expression 文本。
 */
class SExpressionParser {
public:
    /**
     * @brief 解析文本中的一个或多个根节点。
     * @param content UTF-8 文本。
     * @param diagnostics 诊断接收器，可为空。
     * @return 根节点列表；括号不匹配时仍保留已解析节点并记录错误。
     */
    static QList<SExpressionNode> parse(const QString& content, ParseDiagnostics* diagnostics = nullptr);
};

/**
 * @brief 可识别的输入格式。
 */
enum class DetectedFormat {
    Unknown,
    EasyedaJson,
    XpeditionHkp,
    XpeditionSymbol,
    CadstarAscii,
    PcadSExpression,
    TinyCadXml,
    GedaLegacy
};

/**
 * @brief 根据扩展名和内容头部识别输入格式。
 */
class FormatDetector {
public:
    /**
     * @brief 检测输入格式。
     * @param fileName 文件名或路径。
     * @param content 文件开头或完整内容。
     * @return 最可信的格式枚举，无法确认时返回 Unknown。
     */
    static DetectedFormat detect(const QString& fileName, const QByteArray& content);
};

}  // namespace EasyKiConverter::Parser
