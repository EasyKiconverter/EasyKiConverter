#pragma once

#include "INetworkClient.h"

#include <QMutex>
#include <QWaitCondition>

namespace EasyKiConverter {

/**
 * @brief 将异步网络完成信号转换为可安全等待的同步结果。
 * @details 该上下文只负责一次请求的结果交付，不处理请求取消或请求对象生命周期。
 */
class BlockingRequestContext final {
public:
    /**
     * @brief 保存请求结果并唤醒等待线程。
     * @param result 异步请求产生的结果。
     */
    void complete(const NetworkResult& result) {
        QMutexLocker locker(&m_mutex);
        if (m_finished) {
            return;
        }
        m_result = result;
        m_finished = true;
        m_condition.wakeAll();
    }

    /**
     * @brief 阻塞等待请求完成。
     * @return 已保存的网络请求结果。
     */
    NetworkResult wait() {
        QMutexLocker locker(&m_mutex);
        while (!m_finished) {
            m_condition.wait(&m_mutex);
        }
        return m_result;
    }

private:
    QMutex m_mutex;  ///< 保护完成状态和结果对象的互斥锁。
    QWaitCondition m_condition;  ///< 等待和唤醒同步请求线程的条件变量。
    NetworkResult m_result;  ///< 最近一次保存的网络请求结果。
    bool m_finished = false;  ///< 是否已经收到并保存请求结果。
};

}  // namespace EasyKiConverter
