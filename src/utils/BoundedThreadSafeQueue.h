#ifndef BOUNDEDTHREADSAFEQUEUE_H
#define BOUNDEDTHREADSAFEQUEUE_H

#include <QAtomicInt>
#include <QMutex>
#include <QQueue>
#include <QWaitCondition>

namespace EasyKiConverter {

/**
 * @brief 有界线程安全队列
 *
 * 支持阻塞和非阻塞操作，用于连接流水线的各个阶段
     */
template <typename T>
class BoundedThreadSafeQueue {
public:
    /**
     * @brief 构造函数
     * @param maxSize 最大容量（0表示无限制）
     */
    explicit BoundedThreadSafeQueue(size_t maxSize = 0) : m_maxSize(maxSize), m_closed(0) {}

    /**
     * @brief 析构函数
     */
    ~BoundedThreadSafeQueue() {
        close();
    }

    /**
     * @brief 非阻塞push
     * @param item 要添加的元素
     * @return bool 是否成功（队列已满或已关闭返回false）
         */
    bool tryPush(const T& item) {
        QMutexLocker locker(&m_mutex);

        if (m_closed) {
            return false;
        }

        if (m_maxSize > 0 && m_queue.size() >= m_maxSize) {
            return false;
        }

        m_queue.enqueue(item);
        m_notEmpty.wakeOne();
        return true;
    }

    /**
     * @brief 阻塞push
     * @param item 要添加的元素
     * @param timeoutMs 超时时间（毫秒，0表示无限等待）
         * @return bool
     * 是否成功（超时或已关闭返回false）

     */
    bool push(const T& item, int timeoutMs = 0) {
        QMutexLocker locker(&m_mutex);

        while (!m_closed && m_maxSize > 0 && m_queue.size() >= m_maxSize) {
            if (!m_notFull.wait(&m_mutex, timeoutMs)) {
                return false;  // 超时
            }
        }

        if (m_closed) {
            return false;
        }

        m_queue.enqueue(item);
        m_notEmpty.wakeOne();
        return true;
    }

    /**
     * @brief 非阻塞tryPop
     * @param item 输出参数，存储取出的元素
     * @return bool 是否成功（队列为空或已关闭返回false）
         */
    bool tryPop(T& item) {
        QMutexLocker locker(&m_mutex);

        if (m_closed && m_queue.isEmpty()) {
            return false;
        }

        if (m_queue.isEmpty()) {
            return false;
        }

        item = m_queue.dequeue();
        m_notFull.wakeOne();
        return true;
    }

    /**
     * @brief 阻塞pop
     * @param item 输出参数，存储取出的元素
     * @param timeoutMs 超时时间（毫秒，0表示无限等待）
         * @return bool
     * 是否成功（超时或已关闭且队列为空返回false）
         */
    bool pop(T& item, int timeoutMs = 0) {
        QMutexLocker locker(&m_mutex);

        while (m_queue.isEmpty() && !m_closed) {
            if (!m_notEmpty.wait(&m_mutex, timeoutMs)) {
                return false;  // 超时
            }
        }

        if (m_queue.isEmpty() && m_closed) {
            return false;
        }

        item = m_queue.dequeue();
        m_notFull.wakeOne();
        return true;
    }

    /**
     * @brief 关闭队列
     *
     * 关闭后，所有阻塞的push/pop操作都会返回false
     */
    void close() {
        QMutexLocker locker(&m_mutex);
        m_closed = true;
        m_notEmpty.wakeAll();
        m_notFull.wakeAll();
    }

    /**
     * @brief 是否已关闭
         * @return bool
     */
    bool isClosed() const {
        QMutexLocker locker(&m_mutex);
        return m_closed;
    }

    /**
     * @brief 获取队列大小
     * @return size_t
     */
    size_t size() const {
        QMutexLocker locker(&m_mutex);
        return m_queue.size();
    }

    /**
     * @brief 队列是否为空
     * @return bool
     */
    bool isEmpty() const {
        QMutexLocker locker(&m_mutex);
        return m_queue.isEmpty();
    }

private:
    /// 保护队列、容量和关闭状态的互斥锁。
    mutable QMutex m_mutex;
    /// 等待队列变为非空的条件变量。
    QWaitCondition m_notEmpty;
    /// 等待队列出现空闲容量的条件变量。
    QWaitCondition m_notFull;
    /// 保存队列中的元素。
    QQueue<T> m_queue;
    /// 队列最大容量，零表示不限制容量。
    size_t m_maxSize;
    /// 标记队列是否已关闭。
    QAtomicInt m_closed;
};

}  // namespace EasyKiConverter

#endif  // BOUNDEDTHREADSAFEQUEUE_H
