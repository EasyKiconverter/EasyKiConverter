#ifndef EXPORTERFOOTPRINT_H
#define EXPORTERFOOTPRINT_H

#include "FootprintGraphicsGenerator.h"
#include "core/interfaces/IFootprintExporter.h"

#include <QJsonObject>
#include <QString>
#include <QTextStream>

namespace EasyKiConverter {

/**
 * @brief KiCad 封装导出器类
 *
 * 将 IR 封装数据导出为 KiCad 封装格式。
 */
class ExporterFootprint : public IFootprintExporter {
public:
    ExporterFootprint();

    ~ExporterFootprint();

    /**
     * @brief 获取封装库目录扩展名
     * @return ".pretty"
     */
    QString libraryFileExtension() const override {
        return QStringLiteral(".pretty");
    }

    /**
     * @brief 输出是否为目录结构
     * @return true（KiCad 封装库为 .pretty 目录）
     */
    bool isDirectoryOutput() const override {
        return true;
    }

    /**
     * @brief 导出单个 KiCad 封装（单 3D 模型）。
     *
     * @param footprint 封装 IR 数据
     * @param filePath 输出文件路径
     * @param model3DPath 3D 模型路径
     * @return bool 是否成功
     */
    bool exportFootprint(const IR::FootprintComponentIR& footprint,
                         const QString& filePath,
                         const QString& model3DPath = QString()) override;

    /**
     * @brief 导出单个 KiCad 封装（WRL 和 STEP 两个 3D 模型）。
     *
     * @param footprint 封装 IR 数据
     * @param filePath 输出文件路径
     * @param model3DWrlPath WRL模型路径
     * @param model3DStepPath STEP模型路径
     * @return bool 是否成功
     */
    bool exportFootprint(const IR::FootprintComponentIR& footprint,
                         const QString& filePath,
                         const QString& model3DWrlPath,
                         const QString& model3DStepPath);

    /**
     * @brief 批量导出封装库（KiCad .kicad_mod 格式）
     *
     * @param footprints 封装 IR 列表
     * @param libName 库名称（用于构建 3D 模型相对路径）
     * @param filePath 输出目录路径（.pretty 目录）
     * @param preferWrl 是否优先使用 WRL 格式（默认 true，向后兼容）
     * @param exportStep 是否同时导出 STEP 格式（默认 false，向后兼容）
     * @param libraryDescription 库描述
     * @param libraryKeywords 库关键词
     * @param useAbsolutePaths 3D模型路径是否使用绝对路径（默认 false，使用相对路径）
     * @param model3DBaseDir 3D模型基础目录
     * @return bool 是否成功
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
    QString generateHeader(const QString& libName) const;
    QString buildModel3DPath(const QString& safeLibName,
                             const QString& modelName,
                             const QString& extension,
                             bool useAbsolutePaths,
                             const QString& resolvedBaseDir) const;
    void generateFootprintBaseContent(const IR::FootprintComponentIR& footprint,
                                      QString& content,
                                      double& outOriginX,
                                      double& outOriginY,
                                      // 库描述和关键词用于生成封装文件头部元数据。
                                      const QString& libraryDescription = QString(),
                                      const QString& libraryKeywords = QString()) const;
    // 基础版本：支持描述和关键词，内部委托到双路径重载
    QString generateFootprintContent(const IR::FootprintComponentIR& footprint,
                                     const QString& model3DPath,
                                     const QString& libraryDescription,
                                     const QString& libraryKeywords) const;
    // 双3D模型路径版本：支持描述和关键词
    QString generateFootprintContent(const IR::FootprintComponentIR& footprint,
                                     const QString& model3DWrlPath,
                                     const QString& model3DStepPath,
                                     const QString& libraryDescription,
                                     const QString& libraryKeywords) const;
    // 兼容旧调用：无描述/关键词，内部委托到基础版本
    /**
     * @brief 生成不带额外库元数据的兼容封装内容。
     * @param footprint 封装统一中间表示。
     * @param model3DPath 可选的三维模型路径。
     * @return 生成的 KiCad 封装文本。
     */
    QString generateFootprintContent(const IR::FootprintComponentIR& footprint,
                                     const QString& model3DPath = QString()) const;
    QString generateFootprintContent(const IR::FootprintComponentIR& footprint,
                                     const QString& model3DWrlPath,
                                     const QString& model3DStepPath) const;

private:
    mutable FootprintGraphicsGenerator m_graphicsGenerator;  ///< 负责生成封装图形文本的辅助对象。
};

}  // namespace EasyKiConverter

#endif  // EXPORTERFOOTPRINT_H
