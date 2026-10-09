#include "ExporterFootprint.h"

#include "KiCadExportMetadata.h"
#include "utils/PathSecurity.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>

#include <limits>

namespace EasyKiConverter {

/**
 * @brief 从 IR 封装数据计算边界框（mm 单位）
 * @param footprint 封装 IR 数据
 * @param outMinX 输出最小 X
 * @param outMinY 输出最小 Y
 * @param outMaxX 输出最大 X
 * @param outMaxY 输出最大 Y
 */
static void computeBBox(const IR::FootprintComponentIR& footprint,
                        double& outMinX,
                        double& outMinY,
                        double& outMaxX,
                        double& outMaxY) {
    outMinX = std::numeric_limits<double>::max();
    outMinY = std::numeric_limits<double>::max();
    outMaxX = std::numeric_limits<double>::lowest();
    outMaxY = std::numeric_limits<double>::lowest();

    for (const auto& pad : footprint.pads) {
        outMinX = qMin(outMinX, pad.position.x() - pad.size.width() / 2.0);
        outMinY = qMin(outMinY, pad.position.y() - pad.size.height() / 2.0);
        outMaxX = qMax(outMaxX, pad.position.x() + pad.size.width() / 2.0);
        outMaxY = qMax(outMaxY, pad.position.y() + pad.size.height() / 2.0);
    }
    for (const auto& track : footprint.tracks) {
        for (const QPointF& pt : track.points) {
            outMinX = qMin(outMinX, pt.x());
            outMinY = qMin(outMinY, pt.y());
            outMaxX = qMax(outMaxX, pt.x());
            outMaxY = qMax(outMaxY, pt.y());
        }
    }
    for (const auto& hole : footprint.holes) {
        outMinX = qMin(outMinX, hole.center.x() - hole.radius);
        outMinY = qMin(outMinY, hole.center.y() - hole.radius);
        outMaxX = qMax(outMaxX, hole.center.x() + hole.radius);
        outMaxY = qMax(outMaxY, hole.center.y() + hole.radius);
    }
    for (const auto& circle : footprint.circles) {
        outMinX = qMin(outMinX, circle.center.x() - circle.radius);
        outMinY = qMin(outMinY, circle.center.y() - circle.radius);
        outMaxX = qMax(outMaxX, circle.center.x() + circle.radius);
        outMaxY = qMax(outMaxY, circle.center.y() + circle.radius);
    }
    for (const auto& rect : footprint.rectangles) {
        outMinX = qMin(outMinX, rect.bounds.left());
        outMinY = qMin(outMinY, rect.bounds.top());
        outMaxX = qMax(outMaxX, rect.bounds.right());
        outMaxY = qMax(outMaxY, rect.bounds.bottom());
    }
    for (const auto& arc : footprint.arcs) {
        outMinX = qMin(outMinX, arc.center.x() - arc.radius);
        outMinY = qMin(outMinY, arc.center.y() - arc.radius);
        outMaxX = qMax(outMaxX, arc.center.x() + arc.radius);
        outMaxY = qMax(outMaxY, arc.center.y() + arc.radius);
    }
    for (const auto& text : footprint.texts) {
        outMinX = qMin(outMinX, text.position.x());
        outMinY = qMin(outMinY, text.position.y());
        outMaxX = qMax(outMaxX, text.position.x());
        outMaxY = qMax(outMaxY, text.position.y());
    }
    for (const auto& region : footprint.regions) {
        for (const QPointF& pt : region.vertices) {
            outMinX = qMin(outMinX, pt.x());
            outMinY = qMin(outMinY, pt.y());
            outMaxX = qMax(outMaxX, pt.x());
            outMaxY = qMax(outMaxY, pt.y());
        }
    }
}

/** @brief 创建 KiCad 封装导出器。 */
ExporterFootprint::ExporterFootprint() {}

/** @brief 销毁 KiCad 封装导出器。 */
ExporterFootprint::~ExporterFootprint() {}

/**
 * @brief 导出单个封装文件。
 * @param footprint 封装组件 IR。
 * @param filePath 输出文件路径。
 * @param model3DPath 可选的 3D 模型路径。
 * @return 文件写入成功时返回 true。
 */
bool ExporterFootprint::exportFootprint(const IR::FootprintComponentIR& footprint,
                                        const QString& filePath,
                                        const QString& model3DPath) {
    QFile file(filePath);  // 封装输出文件。
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "Failed to open file for writing:" << filePath;
        return false;
    }

    QTextStream out(&file);  // UTF-8 输出流。
    out.setEncoding(QStringConverter::Utf8);

    QString content = generateFootprintContent(footprint, model3DPath);  // 生成的封装文本。

    out << content;
    file.flush();
    file.close();

    qDebug() << "Footprint exported to:" << filePath;
    return true;
}

/**
 * @brief 导出带有 WRL 和 STEP 模型的单个封装文件。
 * @param footprint 封装组件 IR。
 * @param filePath 输出文件路径。
 * @param model3DWrlPath WRL 模型路径。
 * @param model3DStepPath STEP 模型路径。
 * @return 文件写入成功时返回 true。
 */
bool ExporterFootprint::exportFootprint(const IR::FootprintComponentIR& footprint,
                                        const QString& filePath,
                                        const QString& model3DWrlPath,
                                        const QString& model3DStepPath) {
    QFile file(filePath);  // 封装输出文件。
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "Failed to open file for writing:" << filePath;
        return false;
    }

    QTextStream out(&file);  // UTF-8 输出流。
    out.setEncoding(QStringConverter::Utf8);

    QString content = generateFootprintContent(footprint, model3DWrlPath, model3DStepPath);  // 生成的封装文本。

    out << content;
    file.flush();
    file.close();

    qDebug() << "Footprint exported to:" << filePath << "with 2 3D models";
    return true;
}

/**
 * @brief 批量导出 KiCad 封装库。
 * @param footprints 待导出的封装列表。
 * @param libName 库名称。
 * @param filePath 库输出目录。
 * @param preferWrl 是否优先写入 WRL 模型引用。
 * @param exportStep 是否写入 STEP 模型引用。
 * @param libraryDescription 库描述。
 * @param libraryKeywords 库关键词。
 * @param useAbsolutePaths 是否使用绝对模型路径。
 * @param model3DBaseDir 3D 模型基础目录。
 * @return 库文件全部完成写入时返回 true。
 */
bool ExporterFootprint::exportFootprintLibrary(const QList<IR::FootprintComponentIR>& footprints,
                                               const QString& libName,
                                               const QString& filePath,
                                               bool preferWrl,
                                               bool exportStep,
                                               const QString& libraryDescription,
                                               const QString& libraryKeywords,
                                               bool useAbsolutePaths,
                                               const QString& model3DBaseDir) {
    qDebug() << "=== Export Footprint Library ===";
    qDebug() << "Library name:" << libName;
    qDebug() << "Output path:" << filePath;
    qDebug() << "Footprint count:" << footprints.count();
    qDebug() << "Library description:" << libraryDescription;
    qDebug() << "Library keywords:" << libraryKeywords;

    QDir libDir(filePath);  // 封装库输出目录。

    if (!libDir.exists()) {
        qDebug() << "Creating footprint library directory:" << filePath;
        if (!libDir.mkpath(".")) {
            qWarning() << "Failed to create footprint library directory:" << filePath;
            return false;
        }
    }

    QSet<QString> existingFootprintNames;  // 已存在的封装名称。
    QStringList existingFiles = libDir.entryList(QStringList("*.kicad_mod"), QDir::Files);  // 已存在封装文件。
    for (const QString& fileName : existingFiles) {
        QString fpName = fileName;  // 去除扩展名后的封装名称。
        fpName.remove(".kicad_mod");
        existingFootprintNames.insert(fpName);
        qDebug() << "Found existing footprint:" << fpName;
    }

    qDebug() << "Existing footprints count:" << existingFootprintNames.count();

    const QString safeLibName = PathSecurity::sanitizeFilename(libName);  // 安全化后的库名称。
    const QString resolvedBaseDir =
        model3DBaseDir.isEmpty() ? QFileInfo(filePath).absolutePath() : model3DBaseDir;  // 解析后的模型根目录。

    int exportedCount = 0;  // 新增封装数量。
    int overwrittenCount = 0;  // 覆盖封装数量。
    for (const IR::FootprintComponentIR& footprint : footprints) {
        QString fpName = footprint.name;  // 当前封装名称。
        QString fileName = fpName + ".kicad_mod";  // 当前封装文件名。
        QString fullPath = libDir.filePath(fileName);  // 当前封装完整路径。

        bool exists = existingFootprintNames.contains(fpName);  // 当前封装是否已存在。
        if (exists) {
            qDebug() << "Overwriting existing footprint:" << fpName;
            overwrittenCount++;
        } else {
            qDebug() << "Exporting new footprint:" << fpName;
            exportedCount++;
        }

        // 确定 3D 模型路径
        // 只要 models3d 非空且有名称即视为有 3D 模型引用（不依赖 isValid()，
        // 因为 isValid() 要求有实际 OBJ/STEP 数据，而路径生成仅需名称）
        bool hasModel3D =
            !footprint.models3d.isEmpty() && !footprint.models3d.first().name().isEmpty();  // 是否存在有效模型引用。
        QString modelName;  // 当前 3D 模型名称。
        if (hasModel3D) {
            modelName = footprint.models3d.first().name();
            if (modelName.isEmpty()) {
                modelName = fpName;
            }
            modelName = PathSecurity::sanitizeFilename(modelName);
        }

        QString content;  // 当前封装生成内容。
        const bool useWrl = preferWrl && hasModel3D;  // 是否生成 WRL 模型引用。
        const bool useStep = exportStep && hasModel3D;  // 是否生成 STEP 模型引用。

        if (useWrl && useStep) {
            QString wrlPath =  // WRL 模型输出路径。
                buildModel3DPath(safeLibName, modelName, QStringLiteral("wrl"), useAbsolutePaths, resolvedBaseDir);
            QString stepPath =  // STEP 模型输出路径。
                buildModel3DPath(safeLibName, modelName, QStringLiteral("step"), useAbsolutePaths, resolvedBaseDir);
            content = generateFootprintContent(footprint, wrlPath, stepPath, libraryDescription, libraryKeywords);
        } else if (useWrl) {
            QString wrlPath =  // WRL 模型输出路径。
                buildModel3DPath(safeLibName, modelName, QStringLiteral("wrl"), useAbsolutePaths, resolvedBaseDir);
            content = generateFootprintContent(footprint, wrlPath, libraryDescription, libraryKeywords);
        } else if (useStep) {
            QString stepPath =  // STEP 模型输出路径。
                buildModel3DPath(safeLibName, modelName, QStringLiteral("step"), useAbsolutePaths, resolvedBaseDir);
            content = generateFootprintContent(footprint, stepPath, libraryDescription, libraryKeywords);
        } else {
            content = generateFootprintContent(footprint, QString(), libraryDescription, libraryKeywords);
        }

        QFile file(fullPath);  // 当前封装输出文件。
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            qWarning() << "Failed to open file for writing:" << fullPath;
            continue;
        }

        QTextStream out(&file);  // UTF-8 输出流。
        out.setEncoding(QStringConverter::Utf8);
        out << content;
        file.close();

        qDebug() << "  Exported to:" << fullPath;
    }

    qDebug() << "Export complete - New:" << exportedCount << "Overwritten:" << overwrittenCount;
    qDebug() << "Footprint library exported to:" << filePath;

    return true;
}

/**
 * @brief 生成 KiCad 封装文件头。
 * @param libName 库名称。
 * @return 封装文件头文本。
 */
QString ExporterFootprint::generateHeader(const QString& libName) const {
    return QString(
               "(kicad_pcb (version 20221018) (generator %2)\n"
               "  (version 6)\n"
               "  (generator \"%2\")\n"
               "  (name \"%1\")\n\n")
        .arg(libName, KiCadExportMetadata::generatorName());
}

/**
 * @brief 构造 KiCad 3D 模型引用路径。
 * @param safeLibName 安全化后的库名称。
 * @param modelName 模型名称。
 * @param extension 模型扩展名。
 * @param useAbsolutePaths 是否返回绝对路径。
 * @param resolvedBaseDir 解析后的模型基础目录。
 * @return 模型引用路径。
 */
QString ExporterFootprint::buildModel3DPath(const QString& safeLibName,
                                            const QString& modelName,
                                            const QString& extension,
                                            bool useAbsolutePaths,
                                            const QString& resolvedBaseDir) const {
    const QString modelFileName = QStringLiteral("%1.%2").arg(modelName, extension);  // 模型文件名。
    const QString modelDirName = QStringLiteral("%1.3dmodels").arg(safeLibName);  // 模型目录名。

    if (useAbsolutePaths) {
        return QDir::cleanPath(QDir(resolvedBaseDir).absoluteFilePath(modelDirName + "/" + modelFileName));
    }

    return QStringLiteral("../%1/%2").arg(modelDirName, modelFileName);
}

/**
 * @brief 生成封装主体及其二维图形内容。
 * @param footprint 封装组件 IR。
 * @param content 接收生成文本的输出字符串。
 * @param outOriginX 输出封装原点 X 坐标。
 * @param outOriginY 输出封装原点 Y 坐标。
 * @param libraryDescription 库描述。
 * @param libraryKeywords 库关键词。
 */
void ExporterFootprint::generateFootprintBaseContent(const IR::FootprintComponentIR& footprint,
                                                     QString& content,
                                                     double& outOriginX,
                                                     double& outOriginY,
                                                     const QString& libraryDescription,
                                                     const QString& libraryKeywords) const {
    content += QString("(footprint easykiconverter:%1\n").arg(footprint.name);
    content += "  (version 20221018)\n";

    // 导出封装描述 (descr)
    const QString fpDescription = footprint.description.trimmed();
    if (!fpDescription.isEmpty()) {
        content += QString("  (descr \"%1\")\n").arg(fpDescription);
    }

    // 导出封装标签 (tags)
    if (!libraryKeywords.isEmpty()) {
        content += QString("  (tags \"%1\")\n").arg(libraryKeywords);
    }

    // 判断是否有通孔焊盘
    if (footprint.hasThroughHolePads()) {
        content += "  (attr through_hole)\n";
    } else {
        content += "  (attr smd)\n";
    }

    // 计算边界框（mm 单位）
    double bboxMinX = 0.0, bboxMinY = 0.0, bboxMaxX = 0.0, bboxMaxY = 0.0;  // 封装边界框坐标。
    computeBBox(footprint, bboxMinX, bboxMinY, bboxMaxX, bboxMaxY);

    // 如果没有有效的边界框，使用默认值
    bool hasValidBBox = (bboxMinX <= bboxMaxX && bboxMinY <= bboxMaxY);  // 边界框是否有效。
    if (!hasValidBBox) {
        bboxMinX = bboxMinY = 0.0;
        bboxMaxX = bboxMaxY = 1.0;
    }

    // 计算边界框中心作为原点
    double originX = (bboxMinX + bboxMaxX) / 2.0;  // 封装原点 X 坐标。
    double originY = (bboxMinY + bboxMaxY) / 2.0;  // 封装原点 Y 坐标。

    // 计算 pad Y 范围（相对于原点）
    double yLow = 0;  // Pad 的最小相对 Y 坐标。
    double yHigh = 0;  // Pad 的最大相对 Y 坐标。
    if (!footprint.pads.isEmpty()) {
        yLow = footprint.pads.first().position.y() - originY;
        yHigh = yLow;
        for (const IR::FootprintPadIR& pad : footprint.pads) {
            double padY = pad.position.y() - originY;  // 当前 Pad 的相对 Y 坐标。
            if (padY < yLow)
                yLow = padY;
            if (padY > yHigh)
                yHigh = padY;
        }
    }

    content += QString("  (fp_text reference REF** (at 0 %1) (layer F.SilkS)\n").arg(yLow - 4 * IR::EASYEDA_PX_TO_MM);
    content += "    (effects (font (size 1 1) (thickness 0.15)))\n";
    content += "  )\n";

    content += QString("  (fp_text value %1 (at 0 %2) (layer F.Fab)\n")
                   .arg(footprint.name)
                   .arg(yHigh + 4 * IR::EASYEDA_PX_TO_MM);
    content += "    (effects (font (size 1 1) (thickness 0.15)))\n";
    content += "  )\n";

    for (const IR::FootprintTrackIR& track : footprint.tracks) {
        content += m_graphicsGenerator.generateTrack(track, originX, originY);
    }
    for (const IR::FootprintRectangleIR& rect : footprint.rectangles) {
        content += m_graphicsGenerator.generateRectangle(rect, originX, originY);
    }

    for (const IR::FootprintPadIR& pad : footprint.pads) {
        content += m_graphicsGenerator.generatePad(pad, originX, originY);
    }

    for (const IR::FootprintHoleIR& hole : footprint.holes) {
        content += m_graphicsGenerator.generateHole(hole, originX, originY);
    }

    for (const IR::FootprintCircleIR& circle : footprint.circles) {
        content += m_graphicsGenerator.generateCircle(circle, originX, originY);
    }

    for (const IR::FootprintArcIR& arc : footprint.arcs) {
        content += m_graphicsGenerator.generateArc(arc, originX, originY);
    }

    for (const IR::FootprintTextIR& text : footprint.texts) {
        content += m_graphicsGenerator.generateText(text, originX, originY);
    }

    bool hasCourtYard = false;  // 是否已有禁止区/边界区域可作为 courtyard。
    for (const IR::FootprintRegionIR& region : footprint.regions) {
        QString regionContent = m_graphicsGenerator.generateSolidRegion(region, originX, originY);  // 区域生成内容。
        content += regionContent;
        if (region.layer == IR::LayerType::KeepOut) {
            hasCourtYard = true;
        }
    }

    if (!hasCourtYard && footprint.shouldGenerateCourtyard && hasValidBBox) {
        double x1 = std::floor((bboxMinX - originX) * 100.0) / 100.0;  // courtyard 左边界。
        double y1 = std::floor((bboxMinY - originY) * 100.0) / 100.0;  // courtyard 上边界。
        double x2 = std::floor((bboxMaxX - originX) * 100.0) / 100.0;  // courtyard 右边界。
        double y2 = std::floor((bboxMaxY - originY) * 100.0) / 100.0;  // courtyard 下边界。
        content += m_graphicsGenerator.generateCourtyardFromBBox(x1, y1, x2, y2);
        qWarning() << "Warning: No courtyard found, generated from BBox";
    }

    outOriginX = originX;
    outOriginY = originY;
}

/**
 * @brief 生成包含单个 3D 模型的封装文本。
 * @param footprint 封装组件 IR。
 * @param model3DPath 3D 模型路径。
 * @return 完整封装文本。
 */
QString ExporterFootprint::generateFootprintContent(const IR::FootprintComponentIR& footprint,
                                                    const QString& model3DPath) const {
    return generateFootprintContent(footprint, model3DPath, QString(), QString());
}

/**
 * @brief 生成同时包含 WRL 和 STEP 模型的封装文本。
 * @param footprint 封装组件 IR。
 * @param model3DWrlPath WRL 模型路径。
 * @param model3DStepPath STEP 模型路径。
 * @return 完整封装文本。
 */
QString ExporterFootprint::generateFootprintContent(const IR::FootprintComponentIR& footprint,
                                                    const QString& model3DWrlPath,
                                                    const QString& model3DStepPath) const {
    return generateFootprintContent(footprint, model3DWrlPath, model3DStepPath, QString(), QString());
}

/**
 * @brief 生成带库描述和关键词的封装文本。
 * @param footprint 封装组件 IR。
 * @param model3DPath 3D 模型路径。
 * @param libraryDescription 库描述。
 * @param libraryKeywords 库关键词。
 * @return 完整封装文本。
 */
QString ExporterFootprint::generateFootprintContent(const IR::FootprintComponentIR& footprint,
                                                    const QString& model3DPath,
                                                    const QString& libraryDescription,
                                                    const QString& libraryKeywords) const {
    QString content;  // 封装生成文本。
    double originX = 0, originY = 0;  // 封装原点坐标。
    generateFootprintBaseContent(footprint, content, originX, originY, libraryDescription, libraryKeywords);

    const bool hasModel =
        !footprint.models3d.isEmpty() && !footprint.models3d.first().name().isEmpty();  // 是否存在模型引用。
    if (hasModel || !model3DPath.isEmpty()) {
        const IR::Model3DIR& model3D =
            footprint.models3d.isEmpty() ? IR::Model3DIR() : footprint.models3d.first();  // 当前 3D 模型。
        content += m_graphicsGenerator.generateModel3D(model3D, model3DPath);
    }

    content += ")\n";
    return content;
}

/**
 * @brief 生成同时包含两种 3D 模型及库元数据的封装文本。
 * @param footprint 封装组件 IR。
 * @param model3DWrlPath WRL 模型路径。
 * @param model3DStepPath STEP 模型路径。
 * @param libraryDescription 库描述。
 * @param libraryKeywords 库关键词。
 * @return 完整封装文本。
 */
QString ExporterFootprint::generateFootprintContent(const IR::FootprintComponentIR& footprint,
                                                    const QString& model3DWrlPath,
                                                    const QString& model3DStepPath,
                                                    const QString& libraryDescription,
                                                    const QString& libraryKeywords) const {
    QString content;  // 封装生成文本。
    double originX = 0, originY = 0;  // 封装原点坐标。
    generateFootprintBaseContent(footprint, content, originX, originY, libraryDescription, libraryKeywords);

    const bool hasModel =
        !footprint.models3d.isEmpty() && !footprint.models3d.first().name().isEmpty();  // 是否存在模型引用。
    if (hasModel || !model3DWrlPath.isEmpty() || !model3DStepPath.isEmpty()) {
        const IR::Model3DIR& model3D =
            footprint.models3d.isEmpty() ? IR::Model3DIR() : footprint.models3d.first();  // 当前 3D 模型。
        if (!model3DWrlPath.isEmpty()) {
            content += m_graphicsGenerator.generateModel3D(model3D, model3DWrlPath);
        }

        if (!model3DStepPath.isEmpty()) {
            content += m_graphicsGenerator.generateModel3D(model3D, model3DStepPath);
        }
    }

    content += ")\n";
    return content;
}

}  // namespace EasyKiConverter
