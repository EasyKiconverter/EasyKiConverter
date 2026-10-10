#ifndef FETCHWORKER_H
#define FETCHWORKER_H

#include "BaseWorker.h"
#include "core/network/INetworkClient.h"
#include "models/ComponentExportStatus.h"

#include <QAtomicInt>
#include <QMutex>
#include <QRandomGenerator>

namespace EasyKiConverter {

class AsyncNetworkRequest;

/**
 * @brief 抓取工作线程
 *
 * 负责从网络下载原始数据（I/O密集型任务）
 */
class FetchWorker : public BaseWorker {
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param componentId 元件ID
     * @param networkAccessManager 共享的网络访问管理器
     * @param need3DModel 是否需要3D模型
     * @param fetch3DOnly 是否只获取3D模型（复用已有的符号/封装数据）
     * @param existing3DUuid 已有的3D模型UUID（如果非空，直接下载3D模型，无需获取CAD数据）
     * @param networkClient 可选网络客户端（测试注入；生产默认使用 NetworkClient 单例）
     * @param parent 父对象
     */
    explicit FetchWorker(const QString& componentId,
                         bool need3DModel,
                         bool fetch3DOnly = false,
                         const QString& existing3DUuid = QString(),
                         INetworkClient* networkClient = nullptr,
                         QObject* parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~FetchWorker() override;

    /**
     * @brief 执行抓取任务
     */
    void run() override;

    /**
     * @brief 中断当前正在执行的网络请求
     */
    void abort();

signals:
    /**
     * @brief 抓取完成信号
     * @param status 导出状态（使用 QSharedPointer 避免拷贝）
         */
    void fetchCompleted(QSharedPointer<ComponentExportStatus> status);

private:
    /**
     * @brief 执行HTTP GET请求
     * @param url URL
     * @param timeoutMs 超时时间（毫秒）
     * @param status 导出状态对象（用于记录网络诊断）
     * @return QByteArray 响应数据
     * @param resourceType 资源类型。
     */
    QByteArray httpGet(const QString& url,
                       int timeoutMs = 30000,
                       QSharedPointer<ComponentExportStatus> status = nullptr,
                       ResourceType resourceType = ResourceType::WorkerRequest);

    /**
     * @brief 解压ZIP数据
     * @param zipData ZIP数据
     * @return QByteArray 解压后的数据
     */
    QByteArray decompressZip(const QByteArray& zipData);

    /**
     * @brief 下载3D模型数据
     * @param status 导出状态
         * @return bool 是否成功
     */
    bool fetch3DModelData(QSharedPointer<ComponentExportStatus> status);

    /**
     * @brief 计算重试延迟（含随机抖动）
     * @param retryCount 当前重试次数（0-based）
     * @return int 延迟时间（毫秒）
     */
    static int calculateRetryDelay(int retryCount);

private:
    QString m_componentId;  ///< 标识符。
    bool m_need3DModel;  ///< 模型。
    bool m_fetch3DOnly;  // 是否只获取3D模型
    QString m_existing3DUuid;  // 已有的3D模型UUID

    INetworkClient* m_networkClient;  ///< 网络客户端。
    AsyncNetworkRequest* m_currentRequest;  ///< 请求。
    QMutex m_replyMutex;  ///< 网络响应互斥量。
    QAtomicInt m_isAborted;  ///< 是否已中止。

    // 超时配置（静态常量，可配置）
    static const int COMPONENT_INFO_TIMEOUT_MS = 15000;  ///< 组件信息超时时间（毫秒）。
    static const int MODEL_3D_TIMEOUT_MS = 30000;  ///< 三维模型请求超时时间（毫秒）。
    static const int MAX_HTTP_RETRIES = 3;  ///< HTTP 请求最大重试次数。

    // 递增重试延迟（毫秒），对应第1/2/3次重试
    static constexpr int RETRY_DELAYS_MS[] = {3000, 5000, 10000};  ///< 各次重试的等待时间（毫秒）。

    // 速率限制检测（静态成员，所有 FetchWorker 共享）
    static QAtomicInt s_activeRequests;  // 活跃请求计数
    static QMutex s_rateLimitMutex;  // 速率限制互斥锁
    static QDateTime s_lastRateLimitTime;  // 上次触发速率限制的时间
    static int s_backoffMs;  // 退避延迟（毫秒）
};

}  // namespace EasyKiConverter

#endif  // FETCHWORKER_H
