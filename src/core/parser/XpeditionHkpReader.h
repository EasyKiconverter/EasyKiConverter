#pragma once

/**
 * @file XpeditionHkpReader.h
 * @brief Xpedition HKP 分层文本的格式专用读取入口。
 */

#include "GeometryTransforms.h"
#include "TextParsers.h"
#include "XpeditionHkpModel.h"

#include <QByteArray>

namespace EasyKiConverter::Parser {

/** @brief Xpedition HKP 文件类型。 */
enum class XpeditionHkpType { Unknown, PadstackLibrary, CellLibrary, PartsDatabase };

/**
 * @brief 已完成语法层解析的 Xpedition HKP 文档。
 * @details 节点树保留格式字段，后续格式模型可以在此基础上解析 Padstack、Cell 和器件关联。
 */
struct XpeditionHkpDocument {
    XpeditionHkpType type = XpeditionHkpType::Unknown;
    LengthUnit unit = LengthUnit::Unknown;
    QList<SectionNode> sections;
    XpeditionHkpModel model;
    ParseDiagnostics diagnostics;

    /** @brief 判断文档是否具备可继续映射的文件类型。 */
    bool isRecognized() const {
        return type != XpeditionHkpType::Unknown;
    }
};

/**
 * @brief 解析 Xpedition HKP 文件头和分层结构。
 */
class XpeditionHkpReader {
public:
    /**
     * @brief 从内存文本解析 HKP 文档。
     * @param content UTF-8 文件内容。
     * @param filePath 可选源文件路径，用于诊断。
     * @return 语法树、文件类型、单位和诊断集合。
     */
    static XpeditionHkpDocument parse(const QString& content, const QString& filePath = QString());

    /**
     * @brief 从原始字节检测编码后解析 HKP 文档。
     * @param data 原始文件字节。
     * @param filePath 可选源文件路径。
     * @return 经过安全编码解码后的 HKP 文档。
     */
    static XpeditionHkpDocument parseBytes(const QByteArray& data, const QString& filePath = QString());
};

/**
 * @brief 合并多个 HKP 文件的格式模型并检查跨文件关联歧义。
 * @details 重名定义保留为稳定后缀；使用原始名称的引用在多候选时保持歧义，禁止静默绑定。
 */
class XpeditionHkpMerger {
public:
    /**
     * @brief 合并多个已经解析的 HKP 文档。
     * @param documents 待合并文档，允许包含不同类型但会产生错误诊断。
     * @param filePath 合并结果的逻辑路径。
     * @return 合并后的文档模型和诊断。
     */
    static XpeditionHkpDocument merge(const QList<XpeditionHkpDocument>& documents,
                                      const QString& filePath = QString());
};

}  // namespace EasyKiConverter::Parser
