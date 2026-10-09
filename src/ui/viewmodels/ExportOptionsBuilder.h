#ifndef EXPORTOPTIONSBUILDER_H
#define EXPORTOPTIONSBUILDER_H

#include "services/export/ExportProgress.h"

namespace EasyKiConverter {

class ExportSettingsViewModel;

/**
 * @brief 将导出设置视图模型转换为导出服务选项。
 * @details 统一处理默认输出目录、相对路径解析和目标格式映射，不持有视图模型或服务生命周期。
 */
class ExportOptionsBuilder final {
public:
    /**
     * @brief 根据当前界面设置构建导出选项。
     * @param viewModel 导出设置视图模型。
     */
    static ExportOptions build(const ExportSettingsViewModel& viewModel);
};

}  // namespace EasyKiConverter

#endif  // EXPORTOPTIONSBUILDER_H
