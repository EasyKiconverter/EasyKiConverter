#pragma once

#include "core/interfaces/IFootprintExporter.h"

namespace EasyKiConverter {

/**
 * @brief Xpedition 封装库导出器
 * @details 将统一封装 IR 写入 Pads/Cell HKP 文件并打包为 ZIP。
 */
class ExporterXpeditionFootprint : public IFootprintExporter {
public:
    /**
     * @brief 返回封装库压缩包的扩展名。
     * @return Xpedition 封装库使用的 ZIP 文件扩展名。
     */
    QString libraryFileExtension() const override;

    /**
     * @brief 指示导出结果是否为目录。
     * @return 固定返回 false，因为导出结果是单个 ZIP 文件。
     */
    bool isDirectoryOutput() const override;

    /**
     * @brief 返回本次导出过程中收集的诊断信息。
     * @return 包含降级转换、跳过项和写入失败原因的消息列表。
     */
    QStringList diagnostics() const override;

    /**
     * @brief 导出单个统一封装表示。
     * @param footprint 待导出的封装数据。
     * @param filePath 目标 ZIP 文件路径。
     * @param model3DPath 保留接口兼容性，当前格式不写入三维关联。
     * @return 导出成功时返回 true。
     */
    bool exportFootprint(const IR::FootprintComponentIR& footprint,
                         const QString& filePath,
                         const QString& model3DPath = QString()) override;

    /**
     * @brief 将多个统一封装表示写入一个 Xpedition 封装库。
     * @param footprints 待导出的封装列表。
     * @param libName 保留的库名称参数。
     * @param filePath 目标 ZIP 文件路径。
     * @param preferWrl 保留接口兼容性，当前格式不使用。
     * @param exportStep 保留接口兼容性，当前格式不使用。
     * @param libraryDescription 保留的库描述参数。
     * @param libraryKeywords 保留的库关键字参数。
     * @param useAbsolutePaths 保留接口兼容性，当前格式不使用。
     * @param model3DBaseDir 保留接口兼容性，当前格式不使用。
     * @return 所有有效封装写入成功时返回 true。
     */
    bool exportFootprintLibrary(const QList<IR::FootprintComponentIR>& footprints,
                                const QString& libName,
                                const QString& filePath,
                                bool preferWrl = true,
                                bool exportStep = false,
                                const QString& libraryDescription = QString(),
                                const QString& libraryKeywords = QString(),
                                bool useAbsolutePaths = false,
                                const QString& model3DBaseDir = QString()) override;

private:
    /**
     * @brief 生成一个封装对应的 Padstack HKP 内容。
     * @param footprint 包含焊盘和孔信息的封装。
     * @return UTF-8 编码的 Padstack 文本。
     */
    QByteArray padstackFile(const IR::FootprintComponentIR& footprint) const;

    /**
     * @brief 生成一个封装对应的 Cell HKP 内容。
     * @param footprint 包含图元、引脚和文本的封装。
     * @return UTF-8 编码的 Cell 文本。
     */
    QByteArray cellFile(const IR::FootprintComponentIR& footprint) const;

    /**
     * @brief 将用户可见名称转换为安全的库文件名。
     * @param name 原始封装名称。
     * @return 只包含安全字符且不为空的名称。
     */
    QString safeName(QString name) const;
    QStringList m_diagnostics;  ///< 最近一次导出的诊断信息。
};

}  // namespace EasyKiConverter
