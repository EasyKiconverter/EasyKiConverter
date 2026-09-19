#pragma once

/**
 * @file XpeditionHkpReader.h
 * @brief Xpedition HKP 分层文本的格式专用读取入口。
 */

#include "GeometryTransforms.h"
#include "TextParsers.h"
#include "XpeditionHkpModel.h"

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
};

}  // namespace EasyKiConverter::Parser
