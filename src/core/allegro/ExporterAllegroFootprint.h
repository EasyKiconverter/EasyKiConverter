#pragma once

#include "core/interfaces/IFootprintExporter.h"

namespace EasyKiConverter {

/**
 * @brief 生成 Allegro PCB Footprint Import Package。
 * @details 不直接写入 Cadence 私有 .dra/.psm 数据库；输出由用户安装的 Allegro 工具继续生成原生库。
 */
class ExporterAllegroFootprint final : public IFootprintExporter {
public:
    /** @brief 获取 Import Package 目录后缀。 */
    QString libraryFileExtension() const override;

    /** @brief Allegro Import Package 使用目录输出。 */
    bool isDirectoryOutput() const override;

    /** @brief 导出一个封装到 Import Package 目录。 */
    bool exportFootprint(const IR::FootprintComponentIR& footprint,
                         const QString& filePath,
                         const QString& model3DPath = QString()) override;

    /** @brief 导出多个封装及其规范化依赖文件。 */
    bool exportFootprintLibrary(const QList<IR::FootprintComponentIR>& footprints,
                                const QString& libName,
                                const QString& filePath,
                                bool preferWrl = true,
                                bool exportStep = false,
                                const QString& libraryDescription = QString(),
                                const QString& libraryKeywords = QString(),
                                bool useAbsolutePaths = false,
                                const QString& model3DBaseDir = QString()) override;

    /** @brief 获取最近一次导出的诊断。 */
    QStringList diagnostics() const override;

private:
    QStringList m_diagnostics;
};

}  // namespace EasyKiConverter
