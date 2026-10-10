#pragma once

#include <QString>
#include <QStringList>

namespace EasyKiConverter {

class ComponentListViewModel;

/**
 * @brief 协调组件列表预览图缓存更新、批量请求和完成提示。
 * @details 统一维护预览图的防抖应用、有效元件筛选、完成状态和提示状态，保持 ViewModel 的公开接口不变。
 */
class ComponentListPreviewCoordinator final {
public:
    /**
     * @brief 将缓存的预览图更新应用到列表项。
     * @param owner 所属组件列表视图模型。
     */
    static void processCacheImages(ComponentListViewModel& owner);

    /**
     * @brief 为指定的有效元件批量请求预览图。
     * @param owner 所属组件列表视图模型。
     * @param componentIds 元器件编号列表。
     */
    static void fetchImages(ComponentListViewModel& owner, const QStringList& componentIds);

    /**
     * @brief 记录单个元件预览图请求完成并更新整体提示。
     * @param owner 所属组件列表视图模型。
     * @param componentId 元器件编号。
     */
    static void markFetchCompleted(ComponentListViewModel& owner, const QString& componentId);

    /**
     * @brief 清除验证和预览图完成提示。
     * @param owner 所属组件列表视图模型。
     */
    static void clearAttentionHints(ComponentListViewModel& owner);
};

}  // namespace EasyKiConverter
