#pragma once

#include "core/ir/ComponentIR.h"
#include "core/ir/FootprintIR.h"

#include <QList>
#include <QString>

namespace EasyKiConverter {

/**
 * @brief 封装导出器通用接口
 * @details 定义将 IR 封装数据导出为目标 EDA 封装库格式的通用接口。
 *          KiCad、Altium 等不同目标格式各自实现此接口。
 */
class IFootprintExporter {
public:
    virtual ~IFootprintExporter() = default;

    /**
     * @brief 获取封装库文件/目录扩展名
     * @return 文件扩展名或目录后缀（如 ".pretty"、".PcbLib" 或目标格式要求的目录后缀）
     */
    virtual QString libraryFileExtension() const = 0;

    /**
     * @brief 输出是否为目录结构
     * @return true 表示输出为目录（如 KiCad .pretty），false 表示单文件（如 Altium .PcbLib）
     */
    virtual bool isDirectoryOutput() const = 0;

    /**
     * @brief 导出单个封装到文件（单 3D 模型路径）
     * @param footprint 封装 IR 数据
     * @param filePath 输出文件路径
     * @param model3DPath 3D 模型路径（可为空）
     * @return 是否成功
     */
    virtual bool exportFootprint(const IR::FootprintComponentIR& footprint,
                                 const QString& filePath,
                                 const QString& model3DPath = QString()) = 0;

    /**
     * @brief 导出封装库（多个封装）
     * @param footprints 封装 IR 列表
     * @param libName 库名称
     * @param filePath 输出目录路径
     * @param preferWrl 是否优先使用 WRL 格式
     * @param exportStep 是否同时导出 STEP 格式
     * @param libraryDescription 库描述文本
     * @param libraryKeywords 库关键词
     * @param useAbsolutePaths 3D 模型路径是否使用绝对路径
     * @param model3DBaseDir 3D 模型基础目录（绝对路径模式下使用）
     * @return 是否成功
     */
    virtual bool exportFootprintLibrary(const QList<IR::FootprintComponentIR>& footprints,
                                        const QString& libName,
                                        const QString& filePath,
                                        bool preferWrl = true,
                                        bool exportStep = false,
                                        const QString& libraryDescription = QString(),
                                        const QString& libraryKeywords = QString(),
                                        bool useAbsolutePaths = false,
                                        const QString& model3DBaseDir = QString()) = 0;

    /**
     * @brief 导出同时包含符号、封装和器件关联的完整库。
     * @details 仅由目标格式能够在一个库文件中表达完整组件时覆盖此接口。
     * @param components 完整组件 IR 列表
     * @param libName 库名称
     * @param filePath 输出文件路径
     * @param exportModel3D 是否写入目标格式可表达的三维关联
     * @param model3DBaseDir 外部三维模型输出目录
     * @return 是否成功
     */
    virtual bool exportComponentLibrary(const QList<IR::ComponentIR>&,
                                        const QString&,
                                        const QString&,
                                        bool = false,
                                        const QString& = QString()) {
        return false;
    }

    /**
     * @brief 获取最近一次导出的非致命诊断
     * @return 目标格式写入器产生的诊断列表
     */
    virtual QStringList diagnostics() const {
        return {};
    }
};

}  // namespace EasyKiConverter
