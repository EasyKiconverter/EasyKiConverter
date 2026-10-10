#pragma once

#include "ExportProgress.h"

#include <QList>
#include <QMap>
#include <QMutex>
#include <QObject>
#include <QPointer>
#include <QSharedPointer>
#include <QString>
#include <QStringList>

#include <atomic>

namespace EasyKiConverter {

class ComponentService;
class ComponentData;
class ExportTypeStage;
class ExportStageLaunchCoordinator;
class ParallelExportPreloadCoordinator;

/**
 * @brief 并行导出服务
 *
 * 协调管理所有导出类型的并行任务执行。
 *
 * 工作流程:
 * 1. 预加载阶段(Preloading): 从内存缓存加载已验证的元器件数据（网络获取在元器件验证时已完成）
 * 2. 导出阶段(Exporting): 根据用户选项并行导出各类型文件
 *
 * 使用示例:
 * @code
 * ParallelExportService service;
 * service.setOutputPath("/path/to/output");
 * service.setOptions(options);
 *
 * // 连接信号
 * QObject::connect(&service, &ParallelExportService::progressChanged,
 *                  this, &MyClass::onProgressChanged);
 *
 * // 开始导出
 * QStringList componentIds = {"C12345", "R12345"};
 * service.startPreload(componentIds);
 * // ... 等待预加载完成
 * service.startExport();
 * @endcode
 *
 * 信号线程安全性:
 * - progressChanged 可能在子线程发射，需要用QueuedConnection或InvokeMethod更新UI
 */
class ParallelExportService : public QObject {
    Q_OBJECT

    friend class ExportStageLaunchCoordinator;
    friend class ParallelExportPreloadCoordinator;

public:
    /**
     * @brief 构造函数
     * @param parent 父对象指针
     */
    explicit ParallelExportService(QObject* parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~ParallelExportService() override;

    /**
     * @brief 设置导出选项
     * @param options 导出选项配置
     *
     * @see ExportOptions
     */
    void setOptions(const ExportOptions& options);

    /**
     * @brief 获取当前导出选项
     * @return 当前配置的导出选项
     */
    ExportOptions options() const {
        return m_options;
    }

    /**
     * @brief 设置输出目录
     * @param path 输出目录路径
     */
    void setOutputPath(const QString& path);

    /**
     * @brief 设置元器件服务引用
     * @param componentService 元器件服务指针
     *
     * 用于在预加载阶段从ComponentService的内存缓存获取已验证的元器件数据
     */
    void setComponentService(ComponentService* componentService);

    /**
     * @brief 获取输出目录
     * @return 当前设置的输出目录
     */
    QString outputPath() const {
        return m_options.outputPath;
    }

    /**
     * @brief 开始预加载元器件数据
     * @param componentIds 需要预加载的元器件ID列表
     *
     * 从ComponentService的内存缓存中加载已验证的元器件数据。
     * 注意：实际的网络获取已在元器件验证阶段完成（用户添加元器件时），
     * 此处仅做数据拷贝，不涉及网络请求。
     *
     * 预加载完成后发射preloadCompleted()信号。
     */
    void startPreload(const QStringList& componentIds);

    /**
     * @brief 取消预加载
     */
    void cancelPreload();

    /**
     * @brief 开始导出
     *
     * 根据已设置的选项，开始并行导出各类型文件。
     * 调用前应确保 startPreload() 已完成（即使它是同步的）。
     *
     * @see startPreload()
     * @see setOptions()
     */
    void startExport();

    /**
     * @brief 取消导出
     *
     * 向所有导出Stage发送取消信号。
     * 安全的多次调用。
     */
    void cancelExport();

    /**
     * @brief 获取整体进度
     * @return 当前导出任务的整体进度
     */
    ExportOverallProgress getProgress() const;

    /**
     * @brief 获取指定导出类型的进度
     * @param typeName 导出类型名称
     * @return 该类型的进度信息
     */
    ExportTypeProgress getTypeProgress(const QString& typeName) const;

    /**
     * @brief 检查是否正在运行
     * @return true 表示预加载或导出正在进行
     */
    bool isRunning() const;

    /**
     * @brief 获取缓存的数据
     * @return 缓存的元器件数据映射
     */
    QMap<QString, QSharedPointer<ComponentData>> cachedData() const {
        return m_cachedData;
    }

signals:
    /**
     * @brief 预加载进度更新
     * @param progress 预加载进度
     */
    void preloadProgressChanged(const PreloadProgress& progress);

    /**
     * @brief 预加载完成
     * @param successCount 成功数量
     * @param failedCount 失败数量
     */
    void preloadCompleted(int successCount, int failedCount);

    /**
     * @brief 导出进度更新
     * @param progress 整体导出进度
     */
    void progressChanged(const ExportOverallProgress& progress);

    /**
     * @brief 单个元器件状态更新
     * @param componentId 元器件ID
     * @param typeName 导出类型
     * @param status 状态信息
     */
    void itemStatusChanged(const QString& componentId, const QString& typeName, const ExportItemStatus& status);

    /**
     * @brief 导出类型完成
     * @param typeName 导出类型名称
     * @param successCount 成功数量
     * @param failedCount 失败数量
     * @param skippedCount 跳过数量
     */
    void typeCompleted(const QString& typeName, int successCount, int failedCount, int skippedCount);

    /**
     * @brief 整体导出完成
     * @param successCount 总成功数量
     * @param failedCount 总失败数量
     */
    void completed(int successCount, int failedCount);

    /**
     * @brief 导出被取消
     */
    void cancelled();

    /**
     * @brief 导出失败
     * @param error 错误信息
     */
    void failed(const QString& error);

private slots:
    void onPreloadItemCompleted(const QString& componentId, bool success, const QString& error);
    void onExportTypeProgressChanged(const QString& typeName, const ExportTypeProgress& progress);
    void onExportTypeCompleted(const QString& typeName, int successCount, int failedCount, int skippedCount);
    void onExportItemStatusChanged(const QString& componentId, const QString& typeName, const ExportItemStatus& status);
    void processNextPreloadBatch();
    /**
     * @brief 处理并行获取完成的组件数据
     * @param componentDataList 获取到的组件数据列表
     */
    void onAllComponentDataCollected(const QList<ComponentData>& componentDataList);
    void onAllComponentDataCollectedWithErrors(const QList<ComponentData>& componentDataList,
                                               const QMap<QString, QString>& failedComponents);

private:
    void logNetworkRuntimeStats(const QString& context) const;
    void writeExportDetailedReport(const QString& reason) const;
    /**
     * @brief 创建导出Stage实例
     * @param typeName 导出类型名称
     * @return 创建的Stage实例
     */
    ExportTypeStage* createExportStage(const QString& typeName);

    /**
     * @brief 注册并启动一个导出 Stage
     * @param stage 已创建的 Stage 实例（所有权转移给 ParallelExportService）
     * @param typeName 导出类型名称（如 "Symbol"、"Footprint"）
     * @param componentIds 需要导出的元器件 ID 列表
     * @param runGeneration 当前导出会话代数（用于屏蔽取消后的迟到回调）
     *
     * 封装信号连接和启动逻辑，消除各 Stage 的重复样板代码。
     */
    void registerStageAndStart(ExportTypeStage* stage,
                               const QString& typeName,
                               const QStringList& componentIds,
                               quint64 runGeneration);

    /**
     * @brief 更新整体进度
     */
    void updateOverallProgress();

    /**
     * @brief 检查所有导出是否完成
     */
    void checkAllExportCompleted();

    /**
     * @brief 清理已结束的Stage对象
     */
    void cleanupExportStages();
    void discardExportStage(const QString& typeName, ExportTypeStage* stage);
    void retireExportStage(ExportTypeStage* stage);
    void cleanupRetiredExportStages();

    ExportOptions m_options;  ///< 导出选项
    QStringList m_componentIds;  ///< 待处理的元器件ID列表
    QMap<QString, QSharedPointer<ComponentData>> m_cachedData;  ///< 缓存的元器件数据
    QMap<QString, ExportTypeStage*> m_exportStages;  ///< 各导出类型的Stage实例
    QList<QPointer<ExportTypeStage>> m_retiredExportStages;  ///< 取消后仍需等待自然结束的旧Stage
    ComponentService* m_componentService{nullptr};  ///< 元器件服务引用（用于获取已验证的数据）

    ExportOverallProgress m_progress;  ///< 整体进度
    mutable QMutex m_progressMutex;  ///< 保护m_progress的互斥量
    bool m_preloadCompleted{false};  ///< 预加载是否已完成
    int m_runningExportStages{0};  ///< 正在运行的导出Stage数量
    std::atomic<bool> m_cancelRequested{false};  ///< 当前导出是否已请求取消
    int m_nextPreloadIndex{0};  ///< 下一批预加载索引
    quint64 m_activeRunGeneration{0};  ///< 当前导出会话代数，用于屏蔽取消后的迟到回调
};

}  // namespace EasyKiConverter
