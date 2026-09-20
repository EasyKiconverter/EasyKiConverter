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

/** @brief ZIP 归档检查限制。 */
struct ArchiveLimits {
    qsizetype maxEntries = 10000;
    qint64 maxEntryUncompressedBytes = 256LL * 1024 * 1024;
    qint64 maxTotalUncompressedBytes = 1024LL * 1024 * 1024;
    double maxCompressionRatio = 1000.0;
};

/** @brief 一个已通过结构检查的 ZIP 条目。 */
struct ArchiveEntry {
    QString path;
    quint32 compressedSize = 0;
    quint32 uncompressedSize = 0;
    quint16 compressionMethod = 0;
    bool directory = false;
};

/** @brief ZIP 检查结果。 */
struct ArchiveInspectionResult {
    QList<ArchiveEntry> entries;
    qint64 totalUncompressedBytes = 0;
    bool safe = false;
    ParseDiagnostics diagnostics;
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
                                           const QString& filePath = QString());
};

}  // namespace EasyKiConverter::Parser
