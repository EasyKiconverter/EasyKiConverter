#pragma once

#include "core/interfaces/IFootprintExporter.h"
#include "core/ir/FootprintIR.h"
#include "models/AltiumPcbComponent.h"
#include "writers/AltiumPcbLibWriter.h"

namespace EasyKiConverter {

/**
 * @brief Altium 封装导出器
 * @details 实现 IFootprintExporter 接口，将 FootprintComponentIR 转换为 Altium PcbLib 格式
 */
class ExporterAltiumFootprint : public IFootprintExporter {
public:
    /**
     * @brief 获取封装库文件扩展名
     * @return ".PcbLib"
     */
    QString libraryFileExtension() const override {
        return QStringLiteral(".PcbLib");
    }

    /**
     * @brief 输出是否为目录结构
     * @return false（Altium 封装库为单个 .PcbLib 文件）
     */
    bool isDirectoryOutput() const override {
        return false;
    }

    /**
     * @brief 获取最近一次 PcbLib 导出的非致命诊断
     */
    QStringList diagnostics() const override {
        return m_diagnostics;
    }

    /**
     * @brief 导出单个封装到 PcbLib 文件。
     * @param footprint 待导出的封装 IR。
     * @param filePath 输出文件路径。
     * @param model3DPath 可选的 STEP 文件路径。
     * @return 导出是否成功。
     */
    bool exportFootprint(const IR::FootprintComponentIR& footprint,
                         const QString& filePath,
                         const QString& model3DPath = QString()) override;

    /**
     * @brief 导出封装库（多个封装）。
     * @param footprints 待导出的封装列表。
     * @param libName 输出库名称。
     * @param filePath 输出文件路径。
     * @param preferWrl 是否优先使用 WRL 模型。
     * @param exportStep 是否导出 STEP 模型。
     * @param libraryDescription 库描述。
     * @param libraryKeywords 库关键词。
     * @param useAbsolutePaths 是否使用绝对模型路径。
     * @param model3DBaseDir 3D 模型基础目录。
     * @return 导出是否成功。
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
     * @brief 将 FootprintComponentIR 转换为 AltiumPcbComponent。
     * @param data 封装组件 IR。
     * @param model3DPath 可选的 STEP 文件路径。
     * @return 转换后的 Altium 封装组件。
     */
    AltiumPcbComponent convertFootprint(const IR::FootprintComponentIR& data, const QString& model3DPath = QString());

    /**
     * @brief 将 FootprintPadIR 转换为 AltiumPcbPad。
     * @param pad 封装焊盘 IR。
     * @return 转换后的 Altium 焊盘。
     */
    AltiumPcbPad convertPad(const IR::FootprintPadIR& pad);

    /**
     * @brief 将 FootprintTrackIR 转换为 AltiumPcbTrack。
     * @param track 封装走线 IR。
     * @return 转换后的 Altium 走线。
     */
    AltiumPcbTrack convertTrack(const IR::FootprintTrackIR& track);

    /**
     * @brief 将 FootprintCircleIR 转换为 AltiumPcbArc。
     * @param circle 封装圆形图元 IR。
     * @return 转换后的 Altium 圆弧。
     */
    AltiumPcbArc convertCircle(const IR::FootprintCircleIR& circle);

    /**
     * @brief 将 FootprintArcIR 转换为 AltiumPcbArc。
     * @param arc 封装圆弧图元 IR。
     * @return 转换后的 Altium 圆弧。
     */
    AltiumPcbArc convertArc(const IR::FootprintArcIR& arc);

    /**
     * @brief 将 FootprintRectangleIR 转换为 AltiumPcbFill。
     * @param rect 封装矩形图元 IR。
     * @return 转换后的 Altium 填充。
     */
    AltiumPcbFill convertRectangle(const IR::FootprintRectangleIR& rect);

    /**
     * @brief 将 FootprintTextIR 转换为 AltiumPcbText。
     * @param text 封装文本图元 IR。
     * @return 转换后的 Altium 文本。
     */
    AltiumPcbText convertText(const IR::FootprintTextIR& text);

    /**
     * @brief 将 FootprintRegionIR 转换为 AltiumPcbRegion。
     * @param region 封装区域图元 IR。
     * @return 转换后的 Altium 区域。
     */
    AltiumPcbRegion convertSolidRegion(const IR::FootprintRegionIR& region);

    /**
     * @brief 加载 STEP 文件数据。
     * @param stepPath STEP 文件路径。
     * @return 文件内容；打开失败时返回空数组。
     */
    QByteArray loadStepData(const QString& stepPath);

    /**
     * @brief 将封装图元坐标居中到原点附近。
     * @param component 待居中的 Altium 封装组件。
     */
    void centerComponent(AltiumPcbComponent& component);

    /**
     * @brief 从封装包围盒和 3D 模型生成 ComponentBody。
     * @param component 待生成 ComponentBody 的 Altium 封装组件。
     */
    void generateComponentBody(AltiumPcbComponent& component);

    AltiumPcbLibWriter m_writer;  ///< Altium PcbLib 文件写入器。
    QStringList m_diagnostics;  ///< 最近一次导出的非致命诊断信息。
};

}  // namespace EasyKiConverter
