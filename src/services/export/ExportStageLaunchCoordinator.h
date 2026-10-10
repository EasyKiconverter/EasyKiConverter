#pragma once

namespace EasyKiConverter {

class ParallelExportService;

/**
 * @brief 协调并行导出服务中的导出阶段启动。
 *
 * 该协调器只负责根据预加载结果初始化进度、标记缺失数据并创建各类
 * ExportTypeStage；阶段生命周期、信号转发和公开服务接口仍由宿主服务管理。
 */
class ExportStageLaunchCoordinator final {
public:
    /**
     * @brief 根据当前服务状态启动一次导出。
     * @param owner 承载导出状态、选项和阶段生命周期的服务
     */
    static void start(ParallelExportService& owner);
};

}  // namespace EasyKiConverter
