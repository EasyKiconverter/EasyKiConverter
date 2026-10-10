#pragma once

#include <QString>

namespace EasyKiConverter {

class ComponentListViewModel;

/**
 * @brief 协调元件列表的增删和清空操作。
 *
 * 该协调器统一维护模型行、编号索引、验证队列、请求取消和状态计数，
 * ComponentListViewModel 继续保留对外的 QML 槽接口和信号。
 */
class ComponentListMutationCoordinator final {
public:
    /**
     * @brief 添加单个元件并启动其数据验证。
     * @param owner 所属组件列表视图模型。
     * @param componentId 元器件编号。
     */
    static void add(ComponentListViewModel& owner, const QString& componentId);

    /**
     * @brief 按列表位置删除元件并取消相关请求。
     * @param owner 所属组件列表视图模型。
     * @param index 列表索引。
     */
    static void remove(ComponentListViewModel& owner, int index);

    /**
     * @brief 按元件编号删除元件。
     * @param owner 所属组件列表视图模型。
     * @param componentId 元器件编号。
     */
    static void removeById(ComponentListViewModel& owner, const QString& componentId);

    /**
     * @brief 清空元件列表及其缓存、验证状态。
     * @param owner 所属组件列表视图模型。
     */
    static void clear(ComponentListViewModel& owner);
};

}  // namespace EasyKiConverter
