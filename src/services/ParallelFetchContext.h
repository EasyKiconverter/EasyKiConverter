#ifndef PARALLELFETCHCONTEXT_H
#define PARALLELFETCHCONTEXT_H

#include <QList>
#include <QMap>
#include <QMutex>
#include <QObject>
#include <QSet>
#include <QString>

namespace EasyKiConverter {

class ComponentData;

class ParallelFetchContext : public QObject {
    Q_OBJECT
public:
    /**
     * @brief 创建并行获取状态上下文。
     * @param parent Qt 对象父级，可为空
     */
    explicit ParallelFetchContext(QObject* parent = nullptr);
    /**
     * @brief 销毁并行获取状态上下文。
     */
    ~ParallelFetchContext() override;

    /**
     * @brief 开始一轮并行获取。
     * @param totalCount 预期获取的元器件数量
     */
    void start(int totalCount);
    /**
     * @brief 记录一个元器件获取成功。
     * @param componentId 元器件编号
     * @param data 获取到的元器件数据
     */
    void markCompleted(const QString& componentId, const ComponentData& data);
    /**
     * @brief 记录一个元器件获取失败。
     * @param componentId 元器件编号
     * @param error 失败原因
     */
    void markFailed(const QString& componentId, const QString& error);

    /**
     * @brief 判断本轮获取是否已全部完成。
     */
    bool isAllDone() const;
    /**
     * @brief 返回已完成的元器件数量。
     */
    int completedCount() const;
    /**
     * @brief 返回本轮预期获取的元器件数量。
     */
    int totalCount() const;
    /**
     * @brief 返回已成功收集的元器件数据。
     */
    QList<ComponentData> collectedData() const;
    /**
     * @brief 返回元器件编号到失败原因的映射。
     */
    QMap<QString, QString> failedComponents() const;

signals:
    void allCompleted(const QList<ComponentData>& data);

private:
    /**
     * @brief 在状态更新后检查并发出全部完成信号。
     */
    void checkCompletion();

private:
    QMap<QString, ComponentData> m_collectedData;  ///< 按元器件编号保存成功获取的数据。
    QMap<QString, QString> m_failedComponents;  ///< 按元器件编号保存失败原因。
    QSet<QString> m_finishedComponents;  ///< 已完成成功或失败处理的元器件编号集合。
    int m_totalCount;  ///< 当前批次预期获取的元器件数量。
    int m_completedCount;  ///< 当前批次已完成处理的元器件数量。
    bool m_isAllDone;  ///< 当前批次是否已经全部完成。
    mutable QMutex m_mutex;  ///< 保护并行获取状态的互斥锁。
};

}  // namespace EasyKiConverter

#endif  // PARALLELFETCHCONTEXT_H
