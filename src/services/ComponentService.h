#ifndef COMPONENTSERVICE_H
#define COMPONENTSERVICE_H

#include "ComponentCacheLoadWorker.h"
#include "ComponentCacheService.h"
#include "ComponentDataMemoryStore.h"
#include "LcscImageService.h"
#include "ParallelFetchContext.h"
#include "models/ComponentData.h"
#include "models/FootprintData.h"
#include "models/Model3DData.h"
#include "models/SymbolData.h"

#include <QByteArray>
#include <QImage>
#include <QJsonObject>
#include <QMap>
#include <QMutex>
#include <QObject>
#include <QPair>
#include <QQueue>
#include <QString>
#include <QStringList>

namespace EasyKiConverter {

struct CadFetchTaskResult;
class ComponentParallelFetchCoordinator;
class ComponentCadFetchCoordinator;
class ComponentMediaCallbackCoordinator;
class ComponentApiCallbackCoordinator;
class ComponentCacheLoadCoordinator;
class ComponentRequestCancellationCoordinator;
class ComponentRequestCoordinator;

/**
 * @brief 元件服务类
 *
 * 负责处理与元件相关的业务逻辑，不依赖任何 UI 组件
 * 包括数据获取、验证、解析和缓存管理
 */
class ComponentService : public QObject {
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     *
     * @param parent 父对象
     */
    explicit ComponentService(QObject* parent = nullptr);

    /**
     * @brief 构造函数（支持注入 API 实例）
     *
     * @param api API 实例
     * @param parent 父对象
     */
    explicit ComponentService(class EasyedaApi* api, QObject* parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~ComponentService() override;

    /**
     * @brief 中止所有批量获取任务，重置队列和并行上下文
     * @note 供 ParallelExportService 在取消导出时调用
     */
    void abortBatchFetch();

    /**
     * @brief 获取元件数据
     *
     * @param componentId 元件ID
     * @param fetch3DModel 是否获取3D模型
     *
     * 副作用：触发componentInfoReady、cadDataReady、model3DReady信号
     * 线程安全：可以同时调用多个fetchComponentData
     */
    void fetchComponentData(const QString& componentId, bool fetch3DModel = true);

    /**
     * @brief 获取 LCSC 预览图
     *
     * @param componentId 元件ID
     */
    void fetchLcscPreviewImage(const QString& componentId);

    /**
     * @brief 批量获取多个元件的 LCSC 预览图
     *
     * @param componentIds 元件ID列表
     */
    void fetchBatchPreviewImages(const QStringList& componentIds);

    /**
     * @brief 并行获取多个元件的数据
     *
     * @param componentIds 元件ID列表
     * @param fetch3DModel 是否获取3D模型
     */
    void fetchMultipleComponentsData(const QStringList& componentIds, bool fetch3DModel = true);

    /**
     * @brief 验证元件ID格式
     *
     * @param componentId 元件ID
     * @return bool 是否有效
     */
    bool validateComponentId(const QString& componentId) const;

    /**
     * @brief 从文本中智能提取元件编号
     *
     * @param text 输入文本
     * @return QStringList 提取的元件编号列表
     */
    QStringList extractComponentIdFromText(const QString& text) const;

    /**
     * @brief 解析BOM文件
     *
     * @param filePath 文件路径
     * @return QStringList 解析出的元件ID列表
     */
    QStringList parseBomFile(const QString& filePath);

    /**
     * @brief 获取元件数据(从缓存或内存)
     *
     * @param componentId 元件ID
     * @return ComponentData 元件数据
     */
    ComponentData getComponentData(const QString& componentId) const;

    /**
     * @brief 清除缓存
     */
    void clearCache();

    /**
     * @brief 更新元件缓存
     * @param componentId 元件ID
     * @param data 元件数据
     */
    void updateComponentCache(const QString& componentId, const ComponentData& data);

    /**
     * @brief 更新元件的符号/封装描述
     * @param componentId 元件ID
     * @param description 用户编辑后的描述
     */
    void updateComponentDescription(const QString& componentId, const QString& description);

    /**
     * @brief 取消所有正在进行的预览图获取操作
     *
     * 当用户点击开始导出时调用，以确保取消所有未完成的预览图获取工作
     */
    void cancelAllPreviewImageFetches();

    /**
     * @brief 取消所有正在进行的元件数据获取请求
     *
     * 当清空元器件列表时调用，以中断所有悬空请求
     */
    void cancelAllPendingRequests();

    /**
     * @brief 取消特定元件的数据获取请求
     *
     * 当删除单个元器件时调用，以中断该元器件的悬空请求
     * @param componentId 元件ID
     */
    void cancelRequestForComponent(const QString& componentId);

    /**
     * @brief 设置输出路径
     *
     * @param path 输出路径
     */
    void setOutputPath(const QString& path);

    /**
     * @brief 获取输出路径
     *
     * @return QString 输出路径
     */
    QString getOutputPath() const;

signals:
    /**
     * @brief 元件信息获取成功信号
     *
     * @param componentId 元件ID
     * @param data 元件数据
     */
    void componentInfoReady(const QString& componentId, const ComponentData& data);

    /**
     * @brief CAD数据获取成功信号
     *
     * @param componentId 元件ID
     * @param data 元件数据
     */
    void cadDataReady(const QString& componentId, const ComponentData& data);

    /**
     * @brief 3D模型获取成功信号
     *
     * @param uuid 模型UUID
     * @param filePath 文件路径
     */
    void model3DReady(const QString& uuid, const QString& filePath);

    /**
     * @brief 预览图获取成功信号
     *
     * @param componentId 元件ID
     * @param image 预览图
     * @param imageIndex 图片索引（0-2）
     */
    void previewImageReady(const QString& componentId, const QImage& image, int imageIndex);

    /**
     * @brief 预览图数据获取成功信号（内存数据）
     *
     * @param componentId 元件ID
     * @param imageData 预览图数据
     * @param imageIndex 图片索引
     */
    void previewImageDataReady(const QString& componentId, const QByteArray& imageData, int imageIndex);

    /**
     * @brief 预览图获取失败信号
     *
     * @param componentId 元件ID
     * @param error 错误信息
     */
    void previewImageFailed(const QString& componentId, const QString& error);

    /**
     * @brief 所有预览图片下载完成信号
     *
     * @param componentId 元件ID
     * @param imagePaths 图片路径列表
     */
    void allImagesReady(const QString& componentId, const QStringList& imagePaths);

    /**
     * @brief 批量预览图数据获取成功信号（用于缓存加载，避免频繁UI更新）
     *
     * @param componentId 元件ID
     * @param encodedImages Base64编码的预览图数据列表（按索引排序）
     */
    void previewImagesReady(const QString& componentId, const QStringList& encodedImages);

    /**
     * @brief 获取错误信号
     *
     * @param componentId 元件ID
     * @param error 错误信息
     */
    void fetchError(const QString& componentId, const QString& error);

    /**
     * @brief 所有元件数据收集完成信号
     *
     * @param componentDataList 元件数据列表
     */
    void allComponentsDataCollected(const QList<ComponentData>& componentDataList);

    /**
     * @brief 所有元件数据收集完成并包含失败原因的信号
     *
     * @param componentDataList 获取成功的元件数据
     * @param failedComponents 获取失败的元件编号及原因
     */
    void allComponentsDataCollectedWithErrors(const QList<ComponentData>& componentDataList,
                                              const QMap<QString, QString>& failedComponents);

    /**
     * @brief LCSC 数据更新信号（数据手册和预览图 URL）
     *
     * 当 LCSC API 返回数据手册和预览图 URL 后发送此信号
     * ComponentListViewModel 可以接收此信号并更新缓存的 ComponentData
     *
     * @param componentId 元件 ID
     * @param manufacturerPart 制造商部件号
     * @param datasheetUrl 数据手册 URL
     * @param imageUrls 预览图 URL 列表
     */
    void lcscDataUpdated(const QString& componentId,
                         const QString& manufacturerPart,
                         const QString& datasheetUrl,
                         const QStringList& imageUrls);

    /**
     * @brief 数据手册下载完成信号
     *
     * @param componentId 元件 ID
     * @param datasheetData 数据手册数据（内存）
     */
    void datasheetReady(const QString& componentId, const QByteArray& datasheetData);

private slots:
    /**
     * @brief 处理组件信息获取成功
     *
     * @param data 组件信息数据
     */
    void handleComponentInfoFetched(const QString& componentId, const QJsonObject& data);

    /**
     * @brief 处理CAD数据获取成功
     *
     * @param data CAD数据
     */
    void handleCadDataFetched(const QString& componentId, const QJsonObject& data);

    void handleFetchError(const QString& errorMessage);

    /**
     * @brief 处理预览图就绪
     * @param componentId 元件 ID
     * @param imageData 预览图数据
     * @param imageIndex 预览图索引
     */
    void handleImageReady(const QString& componentId, const QByteArray& imageData, int imageIndex);

    /**
     * @brief 处理 LCSC API 数据就绪（包含数据手册和预览图 URL）
     * @param componentId 元件 ID
     * @param manufacturerPart 制造商料号
     * @param datasheetUrl 数据手册地址
     * @param imageUrls 预览图地址列表
     */
    void handleLcscDataReady(const QString& componentId,
                             const QString& manufacturerPart,
                             const QString& datasheetUrl,
                             const QStringList& imageUrls);

    /**
     * @brief 处理数据手册下载完成
     * @param componentId 元件 ID
     * @param datasheetData 数据手册数据
     */
    void handleDatasheetReady(const QString& componentId, const QByteArray& datasheetData);

    /**
     * @brief 处理所有预览图片下载完成
     * @param componentId 元件 ID
     * @param imagePaths 已下载图片路径列表
     */
    void handleAllImagesReady(const QString& componentId, const QStringList& imagePaths);

    /**
     * @brief 处理预览图获取失败
     *
     * @param componentId 元件ID
     * @param error 错误信息
     */
    void handlePreviewImageError(const QString& componentId, const QString& error);

    /**
     * @brief 处理获取错误（带 ID）
     *
     * @param componentId 元件ID
     * @param error 错误信息
     */
    void handleFetchErrorWithId(const QString& componentId, const QString& error);

private:
    friend class ComponentParallelFetchCoordinator;
    friend class ComponentCadFetchCoordinator;
    friend class ComponentMediaCallbackCoordinator;
    friend class ComponentApiCallbackCoordinator;
    friend class ComponentCacheLoadCoordinator;
    friend class ComponentRequestCancellationCoordinator;
    friend class ComponentRequestCoordinator;

    /**
     * @brief 创建并配置异步元件请求队列。
     *
     * 两种构造方式共享同一套队列信号连接，集中初始化可避免行为漂移。
     */
    void initializeQueueManager();

    /**
     * @brief 初始化API连接
     */
    void initializeApiConnections();

    /**
     * @brief 处理获取错误
     *
     * @param componentId 元件ID
     * @param error 错误信息
     * @param expectedGeneration 请求创建时的缓存代次
     */
    void emitFetchErrorAndClearState(const QString& componentId, const QString& error, uint64_t expectedGeneration = 0);

    /**
     * @brief 处理并行数据收集完成
     *
     * @param componentId 元件ID
     * @param data 元件数据
     */
    void handleParallelDataCollected(const QString& componentId, const ComponentData& data);

    /**
     * @brief 处理并行获取错误
     *
     * @param componentId 元件ID
     * @param error 错误信息
     */
    void handleParallelFetchError(const QString& componentId, const QString& error);

    /**
     * @brief 动态队列管理：处理单个请求完成后的队列调度
     */
    void processQueueNext();

    /**
     * @brief 动态队列管理：启动队列处理
     */
    void startQueueProcessing();

    /**
     * @brief 异步队列管理：检查队列状态并启动下一个请求
     */
    void checkQueueAndProcessNext();

    /**
     * @brief 异步队列管理：处理队列超时
     */
    void handleQueueTimeout();

    /**
     * @brief 异步队列管理：重置队列状态
     */
    void resetQueueState();

    /**
     * @brief 检测数据是否为 PDF 格式
     *
     * @param data 数据
     * @return bool 是否为 PDF
     */
    bool isPDF(const QByteArray& data) const;

    // BOM 解析已移至独立的 BomParser 类

    // 图片抓取已移至独立的 LcscImageService 类

    /**
     * @brief 异步从缓存加载元件数据（后台线程执行）
     *
     * @param normalizedId 元件 ID
     * @param fetch3DModel 是否获取 3D 模型
     * @param cache 缓存服务指针
     */
    void loadComponentDataFromCacheAsync(const QString& normalizedId, bool fetch3DModel, ComponentCacheService* cache);

    /**
     * @brief 处理异步 CAD 获取结果并完成缓存、信号和批量状态更新
     * @param result CAD 获取与解析结果
     * @param expectedGeneration 请求创建时的缓存代次
     */
    void handleCadFetchResult(const CadFetchTaskResult& result, uint64_t expectedGeneration);

private:
    class EasyedaApi* m_api;  ///< EasyEDA API 服务实例。
    LcscImageService* m_imageService;  ///< LCSC 图片和数据手册服务实例。

    // 添加互斥锁保护并发访问
    mutable QMutex m_fetchingComponentsMutex;
    mutable QMutex m_currentIdMutex;  // 保护 m_currentComponentId 的并发访问

    // 数据缓存由独立存储类负责容器和并发保护。
    ComponentDataMemoryStore m_componentCache;

    // 当前正在获取的元件数据
    struct FetchingComponent {
        QString componentId;  ///< 当前请求的元件编号。
        ComponentData data;  ///< 当前请求关联的元件数据。
        bool hasComponentInfo;  ///< 是否已获取元件基础信息。
        bool hasCadData;  ///< 是否已获取 CAD 数据。
        bool fetch3DModel;  // 是否需要获取 3D 模型
        bool hasTriggeredLcscFetch;  // 是否已触发 LCSC 数据获取（防止重复触发）
        bool requestActive = false;  // 主验证请求仍在进行中，用于区分真实重复请求和陈旧条目
        QString errorMessage;  ///< 当前请求的错误信息。
        int pendingAsyncDownloads;  // 等待的异步下载数量（数据手册、预览图等）
        uint64_t cacheGeneration = 0;  // 请求创建时的缓存代次，用于过滤旧回调
    };

    QMap<QString, FetchingComponent> m_fetchingComponents;  ///< 按元件编号索引的活动请求状态。

    // 当前处理的元件ID
    QString m_currentComponentId;

    // 并行数据收集状态
    ParallelFetchContext* m_parallelContext;  ///< 当前并行数据收集上下文。
    QMutex m_parallelContextMutex;  // 保护 m_parallelContext 的访问

    // 动态队列管理
    class ComponentQueueManager* m_queueManager;  ///< 动态请求队列管理器。
    int m_activeRequestCount;  ///< 当前活动请求数量。
    int m_maxConcurrentRequests;  ///< 允许的最大并发请求数量。
    bool m_batchFetch3DModel;  ///< 批量请求时是否获取三维模型。

    // 内部状态处理

    /**
     * @brief 内部获取元件数据方法
     *
     * @param componentId 元件ID
     * @param fetch3DModel 是否获取3D模型
     */
    void fetchComponentDataInternal(const QString& componentId, bool fetch3DModel);
    void initializeFetchingComponent(FetchingComponent& fetchingComponent,
                                     const QString& componentId,
                                     bool fetch3DModel);

    // 输出路径
    QString m_outputPath;

#ifdef EASYKICONVERTER_TESTING_ACCESS
public:
    /**
     * @brief 设置测试用的元器件获取状态。
     * @param componentId 用于索引状态的元器件编号。
     * @param storedComponentId 状态中保存的原始元器件编号。
     * @param hasCadData 是否已取得 CAD 数据。
     * @param requestActive 请求是否仍处于活动状态。
     */
    void testSetFetchingState(const QString& componentId,
                              const QString& storedComponentId,
                              bool hasCadData,
                              bool requestActive) {
        const QString normalizedId = componentId.toUpper();  // 用于查询状态的规范化元件编号。
        QMutexLocker locker(&m_fetchingComponentsMutex);
        FetchingComponent& fetching = m_fetchingComponents[normalizedId];  // 待修改的测试请求状态。
        fetching.componentId = storedComponentId;
        fetching.hasCadData = hasCadData;
        fetching.requestActive = requestActive;
        fetching.cacheGeneration = ComponentCacheService::instance()->currentGeneration();
    }

    /**
     * @brief 返回测试用的元件获取状态是否存在。
     * @param componentId 元件编号。
     * @return 对应状态存在时返回 true。
     */
    bool testHasFetchingState(const QString& componentId) const {
        const QString normalizedId = componentId.toUpper();  // 用于查询状态的规范化元件编号。
        QMutexLocker locker(&m_fetchingComponentsMutex);
        return m_fetchingComponents.contains(normalizedId);
    }

    /**
     * @brief 返回测试用获取状态中保存的原始元件编号。
     * @param componentId 元件编号。
     * @return 状态中保存的原始元件编号。
     */
    QString testFetchingComponentId(const QString& componentId) const {
        const QString normalizedId = componentId.toUpper();  // 用于查询状态的规范化元件编号。
        QMutexLocker locker(&m_fetchingComponentsMutex);
        return m_fetchingComponents.value(normalizedId).componentId;
    }

    /**
     * @brief 返回测试用获取状态是否仍处于活动状态。
     * @param componentId 元件编号。
     * @return 请求仍处于活动状态时返回 true。
     */
    bool testFetchingRequestActive(const QString& componentId) const {
        const QString normalizedId = componentId.toUpper();  // 用于查询状态的规范化元件编号。
        QMutexLocker locker(&m_fetchingComponentsMutex);
        return m_fetchingComponents.value(normalizedId).requestActive;
    }

    /**
     * @brief 返回测试用获取状态是否已经拥有 CAD 数据。
     * @param componentId 元件编号。
     * @return 已拥有 CAD 数据时返回 true。
     */
    bool testFetchingHasCadData(const QString& componentId) const {
        const QString normalizedId = componentId.toUpper();  // 用于查询状态的规范化元件编号。
        QMutexLocker locker(&m_fetchingComponentsMutex);
        return m_fetchingComponents.value(normalizedId).hasCadData;
    }

    /**
     * @brief 使用指定代次触发测试用失败清理流程。
     * @param componentId 元件编号。
     * @param error 错误信息。
     * @param expectedGeneration 请求创建时的缓存代次。
     */
    void testEmitFetchErrorForGeneration(const QString& componentId,
                                         const QString& error,
                                         uint64_t expectedGeneration) {
        emitFetchErrorAndClearState(componentId, error, expectedGeneration);
    }
#endif
};

}  // namespace EasyKiConverter

#endif  // COMPONENTSERVICE_H
