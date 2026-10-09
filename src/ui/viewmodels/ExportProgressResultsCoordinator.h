#ifndef EXPORTPROGRESSRESULTSCOORDINATOR_H
#define EXPORTPROGRESSRESULTSCOORDINATOR_H

#include <QString>
#include <QVariantList>
#include <QVariantMap>

namespace EasyKiConverter {

class ExportProgressViewModel;

/**
 * @brief 协调导出结果列表的状态计算、过滤和统计。
 *
 * 该类只操作 ExportProgressViewModel 的结果状态，不负责导出服务请求或进度阶段推进。
 */
class ExportProgressResultsCoordinator final {
public:
    /**
     * @brief 更新成功和失败结果数量。
     * @param owner 所属导出进度视图模型。
     */
    static void updateResultsList(ExportProgressViewModel& owner);

    /**
     * @brief 通知过滤结果列表需要刷新。
     * @param owner 所属导出进度视图模型。
     */
    static void updateFilteredResults(ExportProgressViewModel& owner);

    /**
     * @brief 返回当前过滤模式下的结果列表。
     * @param owner 所属导出进度视图模型。
     */
    static QVariantList filteredResultsList(const ExportProgressViewModel& owner);

    /**
     * @brief 返回当前过滤模式下的成功数量。
     * @param owner 所属导出进度视图模型。
     */
    static int filteredSuccessCount(const ExportProgressViewModel& owner);

    /**
     * @brief 返回当前过滤模式下的失败数量。
     * @param owner 所属导出进度视图模型。
     */
    static int filteredFailedCount(const ExportProgressViewModel& owner);

    /**
     * @brief 返回当前过滤模式下的待处理数量。
     * @param owner 所属导出进度视图模型。
     */
    static int filteredPendingCount(const ExportProgressViewModel& owner);

    /**
     * @brief 返回指定类型的成功数量。
     * @param owner 所属导出进度视图模型。
     * @param statusKey 状态字段名。
     */
    static int typeSuccessCount(const ExportProgressViewModel& owner, const QString& statusKey);

    /**
     * @brief 将类型名称转换为结果列表中的状态字段名。
     * @param typeName 导出类型名称。
     */
    static QString typeStatusKey(const QString& typeName);

    /**
     * @brief 根据各导出类型状态计算元器件总体状态。
     * @param owner 所属导出进度视图模型。
     * @param result 待更新的结果项。
     */
    static void updateOverallItemStatus(const ExportProgressViewModel& owner, QVariantMap& result);

    /**
     * @brief 将结果项重置为当前导出选项对应的待处理状态。
     * @param owner 所属导出进度视图模型。
     * @param result 待重置的结果项。
     */
    static void resetItemForRetry(const ExportProgressViewModel& owner, QVariantMap& result);

private:
    /**
     * @brief 统计结果列表中指定字段处于指定状态的项目数。
     * @param owner 所属导出进度视图模型。
     * @param key 状态字段键。
     * @param expectedStatus 期望的状态值。
     */
    static int countItemsWithTypeStatus(const ExportProgressViewModel& owner,
                                        const QString& key,
                                        const QString& expectedStatus);
};

}  // namespace EasyKiConverter

#endif  // EXPORTPROGRESSRESULTSCOORDINATOR_H
