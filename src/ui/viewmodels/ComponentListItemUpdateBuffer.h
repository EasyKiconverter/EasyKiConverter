#ifndef COMPONENTLISTITEMUPDATEBUFFER_H
#define COMPONENTLISTITEMUPDATEBUFFER_H

#include "models/ComponentListItemData.h"

#include <QList>
#include <QMutex>
#include <QPointer>

namespace EasyKiConverter {

/**
 * @brief 收集需要批量通知界面刷新的元件列表项。
 *
 * 异步服务回调可能在短时间内连续更新同一个列表项，缓冲器负责去重并
 * 在定时器触发时一次性转移待通知项，从而避免重复发出 dataChanged 信号。
 */
class ComponentListItemUpdateBuffer final {
public:
    /**
     * @brief 添加一个待刷新列表项。
     * @param item 需要通知数据变化的列表项。
     */
    void add(ComponentListItemData* item);

    /**
     * @brief 原子取出并清空全部待刷新列表项。
     * @return 当前批次的列表项快照。
     */
    QList<QPointer<ComponentListItemData>> take();

    /**
     * @brief 丢弃当前尚未发送通知的列表项。
     */
    void clear();

private:
    /**
     * @brief 保护待刷新列表项集合的互斥锁。
     */
    mutable QMutex m_mutex;
    /**
     * @brief 保存待刷新列表项并避免同一对象重复加入。
     */
    QList<QPointer<ComponentListItemData>> m_items;
};

}  // namespace EasyKiConverter

#endif  // COMPONENTLISTITEMUPDATEBUFFER_H
