#ifndef PARALLELEXPORTPRELOADCOORDINATOR_H
#define PARALLELEXPORTPRELOADCOORDINATOR_H

#include <QList>
#include <QMap>

namespace EasyKiConverter {

class ComponentData;
class ParallelExportService;

/**
 * @brief 协调并行导出前的组件数据预加载。
 * @details 统一处理网络批量结果、磁盘缓存回退、严格数据校验和预加载进度收敛。
 */
class ParallelExportPreloadCoordinator final {
public:
    /**
     * @brief 处理没有网络服务时的磁盘缓存分批预加载。
     * @param service 并行导出服务实例。
     */
    static void processNextBatch(ParallelExportService& service);

    /**
     * @brief 合并网络批量获取结果并完成预加载统计。
     * @param service 并行导出服务实例。
     * @param componentDataList 预加载得到的组件数据列表。
     * @param failedComponents 预加载失败的元器件编号和错误信息。
     */
    static void handleCollectedData(ParallelExportService& service,
                                    const QList<ComponentData>& componentDataList,
                                    const QMap<QString, QString>& failedComponents);
};

}  // namespace EasyKiConverter

#endif  // PARALLELEXPORTPRELOADCOORDINATOR_H
