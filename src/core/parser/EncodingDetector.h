#pragma once

/**
 * @file EncodingDetector.h
 * @brief 文本编码检测和安全解码入口。
 */

#include "ParseDiagnostics.h"

#include <QByteArray>
#include <QString>

namespace EasyKiConverter::Parser {

/** @brief 当前解析器可识别的文本编码。 */
enum class TextEncoding { Utf8, Utf16LittleEndian, Utf16BigEndian, Latin1, Unknown };

/** @brief 编码检测结果。 */
struct EncodingDetectionResult {
    TextEncoding encoding = TextEncoding::Unknown;
    bool valid = false;
    int confidence = 0;
};

/**
 * @brief 检测 BOM 和 UTF-8 合法性并执行有限回退解码。
 * @details 不对未知二进制内容强行宣称为 UTF-8，回退到 Latin-1 时会写入警告诊断。
 */
class EncodingDetector {
public:
    /**
     * @brief 检测输入字节的编码。
     * @param data 原始文本字节。
     * @return 编码类型、合法性和置信度。
     */
    static EncodingDetectionResult detect(const QByteArray& data);

    /**
     * @brief 按检测结果将文本解码为 QString。
     * @param data 原始文本字节。
     * @param diagnostics 可选诊断接收器。
     * @param filePath 可选源文件路径。
     * @return 解码后的文本；无法识别时返回空字符串。
     */
    static QString decode(const QByteArray& data,
                          ParseDiagnostics* diagnostics = nullptr,
                          const QString& filePath = QString());
};

}  // namespace EasyKiConverter::Parser
