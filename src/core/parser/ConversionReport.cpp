#include "ConversionReport.h"

namespace EasyKiConverter::Parser {

/** @brief 追加单条诊断并保留其产生顺序。 */
void ConversionReport::add(const ParseDiagnostic& diagnostic) {
    m_diagnostics.append(diagnostic);
}

/** @brief 批量追加解析诊断，避免格式适配器重复复制字段。 */
void ConversionReport::append(const ParseDiagnostics& diagnostics) {
    m_diagnostics.append(diagnostics.items());
}

/** @brief 返回任务诊断列表的只读视图。 */
const QList<ParseDiagnostic>& ConversionReport::diagnostics() const {
    return m_diagnostics;
}

/** @brief 按错误、取消、跳过和警告的优先级计算任务状态。 */
ConversionStatus ConversionReport::status() const {
    bool hasWarning = false;
    bool hasSkipped = false;
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

/** @brief 更新进度并限制进度值在合理范围内。 */
void ConversionReport::setProgress(int completed, int total, const QString& currentItem) {
    m_progress.total = qMax(0, total);
    m_progress.completed = qBound(0, completed, m_progress.total);
    m_progress.currentItem = currentItem;
}

/** @brief 返回当前进度快照。 */
const ConversionProgress& ConversionReport::progress() const {
    return m_progress;
}

/** @brief 标记任务取消但不丢弃已经完成的结果信息。 */
void ConversionReport::markCancelled() {
    m_progress.cancelled = true;
}

/** @brief 返回任务是否已收到取消标记。 */
bool ConversionReport::isCancelled() const {
    return m_progress.cancelled;
}

}  // namespace EasyKiConverter::Parser
