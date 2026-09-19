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

/** @brief 文本词法单元。 */
struct TextToken {
    QString text;
    int line = 0;
    int column = 0;
    bool quoted = false;
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

/** @brief 点缩进或空白缩进文本中的一层节点。 */
struct SectionNode {
    QString keyword;
    QString value;
    QList<SectionNode> children;
    int line = 0;
    int depth = 0;
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

/** @brief S-expression 节点，支持嵌套列表和带引号原子。 */
struct SExpressionNode {
    QString atom;
    QList<SExpressionNode> children;
    int line = 0;

    /** @brief 判断节点是否为列表节点。 */
    bool isList() const {
        return !children.isEmpty();
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

/** @brief 可识别的输入格式。 */
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
