#pragma once

/**
 * @file ArchiveInspector.h
 * @brief ZIP 归档结构检查和解压安全边界。
 */

#include "ParseDiagnostics.h"

#include <QByteArray>
#include <QList>
#include <QString>

namespace EasyKiConverter::Parser {

/**
 * @brief ZIP 归档检查限制。
 */
struct ArchiveLimits {
    qsizetype maxEntries = 10000;  ///< 允许的最大条目数量。
    qint64 maxEntryUncompressedBytes = 256LL * 1024 * 1024;  ///< 单个条目允许的最大展开大小。
    qint64 maxTotalUncompressedBytes = 1024LL * 1024 * 1024;  ///< 所有条目允许的最大展开大小。
    double maxCompressionRatio = 1000.0;  ///< 允许的最大压缩比。
};

/**
 * @brief 一个已通过结构检查的 ZIP 条目。
 */
struct ArchiveEntry {
    QString path;  ///< ZIP 内的相对路径。
    quint32 compressedSize = 0;  ///< 压缩后大小。
    quint32 uncompressedSize = 0;  ///< 展开后大小。
    quint16 compressionMethod = 0;  ///< ZIP 压缩方法编号。
    bool directory = false;  ///< 是否为目录条目。
};

/**
 * @brief ZIP 检查结果。
 */
struct ArchiveInspectionResult {
    QList<ArchiveEntry> entries;  ///< 通过结构检查的条目。
    qint64 totalUncompressedBytes = 0;  ///< 所有条目的展开大小总和。
    bool safe = false;  ///< 归档是否满足安全限制。
    ParseDiagnostics diagnostics;  ///< 结构检查诊断。
};

/**
 * @brief 检查 ZIP 归档的路径和大小安全边界。
 * @details 该类只读取中央目录，不执行解压；实际解压前必须先确认结果 safe。
 */
class ArchiveInspector {
public:
    /**
     * @brief 检查内存中的 ZIP 数据。
     * @param data 完整 ZIP 字节流。
     * @param limits 条目数量、展开大小和压缩比限制。
     * @param filePath 可选源文件路径，用于诊断。
     * @return 检查结果和结构化诊断。
     */
    static ArchiveInspectionResult inspect(const QByteArray& data,
                                           const ArchiveLimits& limits = {},
                                           // 可选源文件路径用于补充诊断上下文。
                                           const QString& filePath = QString());
};

}  // namespace EasyKiConverter::Parser
