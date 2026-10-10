#include "ConversionReport.h"

namespace EasyKiConverter::Parser {

/**
 * @brief 追加单条诊断并保留其产生顺序。
 * @param diagnostic 待追加的诊断条目。
 */
void ConversionReport::add(const ParseDiagnostic& diagnostic) {
    m_diagnostics.append(diagnostic);
}

/**
 * @brief 批量追加解析诊断，避免格式适配器重复复制字段。
 * @param diagnostics 待追加的诊断集合。
 */
void ConversionReport::append(const ParseDiagnostics& diagnostics) {
    m_diagnostics.append(diagnostics.items());
}

/**
 * @brief 返回任务诊断列表的只读视图。
 * @return 按产生顺序保存的诊断列表。
 */
const QList<ParseDiagnostic>& ConversionReport::diagnostics() const {
    return m_diagnostics;
}

/**
 * @brief 按错误、取消、跳过和警告的优先级计算任务状态。
 * @return 当前任务状态。
 */
ConversionStatus ConversionReport::status() const {
    bool hasWarning = false;  // 是否出现警告诊断。
    bool hasSkipped = false;  // 是否出现跳过诊断。
    for (const ParseDiagnostic& diagnostic : m_diagnostics) {
        if (diagnostic.severity == ParseSeverity::Error)
            return ConversionStatus::Failed;
        if (diagnostic.severity == ParseSeverity::Warning)
            hasWarning = true;
        if (diagnostic.severity == ParseSeverity::Skipped)
            hasSkipped = true;
    }
    if (m_progress.cancelled)
        return ConversionStatus::Skipped;
    if (hasWarning)
        return ConversionStatus::Warning;
    if (hasSkipped)
        return ConversionStatus::Skipped;
    return ConversionStatus::Ok;
}

/**
 * @brief 更新进度并限制进度值在合理范围内。
 * @param completed 已完成的工作量。
 * @param total 任务总工作量。
 * @param currentItem 当前处理对象描述。
 */
void ConversionReport::setProgress(int completed, int total, const QString& currentItem) {
    m_progress.total = qMax(0, total);
    m_progress.completed = qBound(0, completed, m_progress.total);
    m_progress.currentItem = currentItem;
}

/**
 * @brief 返回当前进度快照。
 * @return 当前任务进度。
 */
const ConversionProgress& ConversionReport::progress() const {
    return m_progress;
}

/**
 * @brief 标记任务取消但不丢弃已经完成的结果信息。
 */
void ConversionReport::markCancelled() {
    m_progress.cancelled = true;
}

/**
 * @brief 返回任务是否已收到取消标记。
 * @return 已取消时返回 true。
 */
bool ConversionReport::isCancelled() const {
    return m_progress.cancelled;
}

}  // namespace EasyKiConverter::Parser
