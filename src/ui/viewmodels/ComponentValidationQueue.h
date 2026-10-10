#ifndef COMPONENTVALIDATIONQUEUE_H
#define COMPONENTVALIDATIONQUEUE_H

#include <QSet>
#include <QString>
#include <QStringList>

namespace EasyKiConverter {

/**
 * @brief 管理元件验证请求的排队和飞行中状态。
 * @details 该类只维护去重后的队列状态，不负责并发数量、网络请求或界面通知。
 */
class ComponentValidationQueue final {
public:
    /**
     * @brief 将尚未处理的元件编号加入验证队列。
     * @param componentId 元器件编号。
     */
    bool enqueue(const QString& componentId);

    /**
     * @brief 判断元件是否已排队或正在处理。
     * @param componentId 元器件编号。
     */
    bool contains(const QString& componentId) const;

    /**
     * @brief 取出下一个请求并标记为正在处理。
     */
    QString takeNext();

    /**
     * @brief 从排队和处理中集合移除指定元件。
     * @param componentId 元器件编号。
     */
    void remove(const QString& componentId);

    /**
     * @brief 标记指定元件验证完成。
     * @param componentId 元器件编号。
     */
    void complete(const QString& componentId);

    /**
     * @brief 判断是否没有待取出的验证请求。
     */
    bool isEmpty() const;

    /**
     * @brief 返回待取出的验证请求数量。
     */
    int size() const;

    /**
     * @brief 判断是否存在正在处理的验证请求。
     */
    bool hasInFlight() const;

    /**
     * @brief 判断指定元件是否正在处理。
     * @param componentId 元器件编号。
     */
    bool isInFlight(const QString& componentId) const;

private:
    QStringList m_pendingIds;  ///< 保存等待验证的元器件编号。
    QSet<QString> m_inFlightIds;  ///< 保存正在验证的元器件编号。
};

}  // namespace EasyKiConverter

#endif  // COMPONENTVALIDATIONQUEUE_H
