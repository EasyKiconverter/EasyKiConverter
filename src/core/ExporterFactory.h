#pragma once

#include "core/interfaces/IFootprintExporter.h"
#include "core/interfaces/IModel3DExporter.h"
#include "core/interfaces/ISymbolExporter.h"
#include "services/export/ExportProgress.h"

#include <memory>

namespace EasyKiConverter {

/**
 * @brief 导出器工厂类
 * @details 根据目标 EDA 格式创建对应的导出器实例。
 *          使用工厂模式隔离格式特定的导出器实现，
 *          使服务层无需关心具体的导出格式细节。
 */
class ExporterFactory {
public:
    /**
     * @brief 创建符号导出器
     * @param format 目标 EDA 格式
     * @return 符号导出器实例，调用方拥有所有权
     */
    static std::unique_ptr<ISymbolExporter> createSymbolExporter(TargetEdaFormat format);

    /**
     * @brief 创建封装导出器
     * @param format 目标 EDA 格式
     * @return 封装导出器实例，调用方拥有所有权
     */
    static std::unique_ptr<IFootprintExporter> createFootprintExporter(TargetEdaFormat format);

    /**
     * @brief 创建 3D 模型导出器
     * @param format 目标 EDA 格式
     * @param parent QObject 父对象（3D 导出器需要 QObject 以支持信号槽）
     * @return 独立 WRL/STEP 模型导出器实例，调用方拥有所有权；目标格式未知时返回空指针
     */
    static std::unique_ptr<IModel3DExporter> createModel3DExporter(TargetEdaFormat format, QObject* parent = nullptr);
};

}  // namespace EasyKiConverter
