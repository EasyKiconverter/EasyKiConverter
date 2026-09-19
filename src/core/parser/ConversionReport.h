#pragma once

/**
 * @file ConversionReport.h
 * @brief 跨格式解析和转换任务的统一报告模型。
 */

#include "ParseDiagnostics.h"

#include <QList>
#include <QString>

namespace EasyKiConverter::Parser {

/** @brief 一次解析或转换任务的汇总状态。 */
enum class ConversionStatus { Ok, Warning, Failed, Skipped };

/** @brief 一次解析或转换任务的进度快照。 */
struct ConversionProgress {
    int total = 0;
    int completed = 0;
    QString currentItem;
    bool cancelled = false;
};

/**
 * @brief 收集跨格式任务的诊断、进度和取消结果。
 * @details 该报告只描述任务结果，不持有格式专用模型，也不改变统一 IR 的语义。
 */
class ConversionReport {
public:
    /** @brief 追加一条结构化诊断。 */
    void add(const ParseDiagnostic& diagnostic);

    /** @brief 将解析阶段的全部诊断追加到报告。 */
    void append(const ParseDiagnostics& diagnostics);

    /** @brief 返回任务产生的全部诊断。 */
    const QList<ParseDiagnostic>& diagnostics() const;

    /** @brief 返回依据诊断和取消状态计算出的任务状态。 */
    ConversionStatus status() const;

    /** @brief 更新当前任务进度。 */
    void setProgress(int completed, int total, const QString& currentItem = QString());

    /** @brief 返回当前任务进度。 */
    const ConversionProgress& progress() const;

    /** @brief 标记任务已取消，保留已经产生的部分结果和诊断。 */
    void markCancelled();

    /** @brief 判断任务是否已取消。 */
    bool isCancelled() const;

private:
    QList<ParseDiagnostic> m_diagnostics;
    ConversionProgress m_progress;
};

}  // namespace EasyKiConverter::Parser
