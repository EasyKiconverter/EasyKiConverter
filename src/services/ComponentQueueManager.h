#ifndef COMPONENTQUEUEMANAGER_H
#define COMPONENTQUEUEMANAGER_H

#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QTimer>

namespace EasyKiConverter {

class ComponentQueueManager : public QObject {
    Q_OBJECT
public:
    explicit ComponentQueueManager(int maxConcurrent = 5, QObject* parent = nullptr);
    ~ComponentQueueManager() override;

    void start(const QStringList& componentIds);
    void stop();
    void setMaxConcurrentRequests(int maxConcurrent);
    void checkAndProcessNext();
    void requestCompleted(const QString& componentId);
    void reset();

    int activeRequestCount() const;
    bool isRunning() const;
    int maxConcurrentRequests() const;

    static const int QUEUE_CHECK_INTERVAL_MS = 200;  ///< 队列检查间隔（毫秒）。
    static const int TOTAL_TIMEOUT_MS = 300000;  ///< 队列整体超时时间（毫秒）。

signals:
    void requestReady(const QString& componentId);
    void queueEmpty();
    void timeout();
    void progress(int completed, int total);

private slots:
    void onCheckTimeout();
    void onTotalTimeout();

private:
    QStringList m_requestQueue;  ///< 队列。
    QSet<QString> m_pendingComponents;  ///< 待处理元器件集合。
    int m_maxConcurrentRequests;  ///< 最大并发请求数。
    QTimer* m_checkTimer;  ///< 更新检查定时器。
    QTimer* m_totalTimer;  ///< 更新检查总计时器。
    bool m_isRunning;  ///< 服务运行状态。
};

}  // namespace EasyKiConverter

#endif  // COMPONENTQUEUEMANAGER_H
