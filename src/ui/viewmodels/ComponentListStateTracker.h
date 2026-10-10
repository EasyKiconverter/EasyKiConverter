#ifndef COMPONENTLISTSTATETRACKER_H
#define COMPONENTLISTSTATETRACKER_H

#include "models/ComponentListItemData.h"

#include <QList>
#include <QString>

namespace EasyKiConverter {

/**
 * @brief 管理元件列表的过滤模式和验证状态统计。
 *
 * 该类只计算列表状态，不负责 Qt 模型通知、列表生命周期或元件数据获取。
 */
class ComponentListStateTracker final {
public:
    /**
     * @brief 根据当前列表重新计算验证状态计数。
     * @param items 元件列表项。
     */
    void recompute(const QList<ComponentListItemData*>& items);

    /**
     * @brief 设置过滤模式。
     * @param mode 新过滤模式。
     * @return 模式发生变化时返回 true。
     */
    bool setFilterMode(const QString& mode);

    /**
     * @brief 返回当前过滤模式。
     */
    QString filterMode() const;

    /**
     * @brief 返回当前过滤模式下的列表数量。
     * @param totalCount 未过滤的列表总数。
     */
    int filteredCount(int totalCount) const;

    /**
     * @brief 返回验证中的元件数量。
     */
    int validatingCount() const;

    /**
     * @brief 返回验证成功的元件数量。
     */
    int validCount() const;

    /**
     * @brief 返回验证失败的元件数量。
     */
    int invalidCount() const;

    /**
     * @brief 判断是否存在可重试的失败元件。
     */
    bool hasRetryableInvalidComponents() const;

private:
    QString m_filterMode = QStringLiteral("all");  ///< 保存当前过滤模式。
    int m_validatingCount = 0;  ///< 保存正在验证的元件数量。
    int m_validCount = 0;  ///< 保存验证成功的元件数量。
    int m_invalidCount = 0;  ///< 保存验证失败的元件数量。
    int m_retryableInvalidCount = 0;  ///< 保存可重试失败元件数量。
};

}  // namespace EasyKiConverter

#endif  // COMPONENTLISTSTATETRACKER_H
