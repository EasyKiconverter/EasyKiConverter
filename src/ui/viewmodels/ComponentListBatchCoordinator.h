#ifndef COMPONENTLISTBATCHCOORDINATOR_H
#define COMPONENTLISTBATCHCOORDINATOR_H

#include <QString>
#include <QStringList>

namespace EasyKiConverter {

class ComponentListViewModel;

/**
 * @brief 协调组件列表的批量添加和 BOM 导入流程。
 *
 * 该协作者只负责批处理队列、模型行插入和 BOM 解析回调，
 * ComponentListViewModel 仍保留对外的 QML 槽函数与状态属性。
 */
class ComponentListBatchCoordinator final {
public:
    /**
     * @brief 收集并排队一批待添加的元件编号。
     * @param viewModel 列表视图模型。
     * @param componentIds 元器件编号列表。
     */
    static void addComponents(ComponentListViewModel& viewModel, const QStringList& componentIds);

    /**
     * @brief 处理下一批待添加元件并启动验证。
     * @param viewModel 列表视图模型。
     */
    static void processNext(ComponentListViewModel& viewModel);

    /**
     * @brief 异步解析 BOM 文件并将结果加入列表。
     * @param viewModel 列表视图模型。
     * @param filePath BOM 文件路径。
     */
    static void selectBomFile(ComponentListViewModel& viewModel, const QString& filePath);
};

}  // namespace EasyKiConverter

#endif  // COMPONENTLISTBATCHCOORDINATOR_H
