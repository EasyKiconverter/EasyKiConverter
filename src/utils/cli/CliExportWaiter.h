#pragma once

#include <QStringList>

namespace EasyKiConverter {

class ParallelExportService;

/**
 * @brief 为 CLI 转换器提供同步等待导出阶段完成的辅助方法。
 * @details 将 Qt 信号转换为局部事件循环，不持有转换器状态，也不处理导出结果。
 */
class CliExportWaiter final {
public:
    /**
     * @brief 启动预加载并等待完成或失败信号。
     * @param service 导出服务。
     * @param componentIds 待预加载的元器件编号列表。
     */
    static void waitForPreload(ParallelExportService* service, const QStringList& componentIds);

    /**
     * @brief 启动导出并等待完成或失败信号。
     * @param service 导出服务。
     */
    static void waitForExport(ParallelExportService* service);
};

}  // namespace EasyKiConverter
