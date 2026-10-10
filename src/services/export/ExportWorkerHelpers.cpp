#include "ExportWorkerHelpers.h"

#include "core/easyeda/EasyedaFootprintImporter.h"
#include "core/easyeda/EasyedaSymbolImporter.h"
#include "models/ComponentData.h"
#include "models/Model3DData.h"
#include "services/ComponentCacheService.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

// ExportOptions is defined in ExportProgress.h which is included by the header

namespace EasyKiConverter {

namespace {

/**
 * @brief 在磁盘缓存缺少独立模型时，从封装模型恢复三维模型元数据。
 * @param data 待补齐的元器件数据。
 */
void restoreModel3DFromFootprint(ComponentData& data) {
    if (data.model3DData() && !data.model3DData()->uuid().isEmpty())
        return;
    if (!data.footprintData())
        return;

    const Model3DData footprintModel = data.footprintData()->model3D();  ///< 封装中携带的三维模型元数据。
    if (footprintModel.uuid().isEmpty())
        return;

    auto model3DData = QSharedPointer<Model3DData>::create();  ///< 从封装模型恢复的独立三维模型数据。
    model3DData->setUuid(footprintModel.uuid());
    model3DData->setName(footprintModel.name());
    model3DData->setTranslation(footprintModel.translation());
    model3DData->setRotation(footprintModel.rotation());
    data.setModel3DData(model3DData);
}

}  // namespace

/**
 * @brief 返回指定导出类型的默认输出目录。
 * @param subdir 输出目录子目录。
 * @return 默认输出目录路径。
 */
QString ExportWorkerHelpers::defaultOutputDir(const QString& subdir) {
    return QDir::currentPath() + QStringLiteral("/export/") + subdir;
}

/**
 * @brief 创建并返回导出输出目录。
 * @param options 导出选项。
 * @param subdir 输出目录子目录。
 * @return 创建成功的输出目录路径，失败时返回空字符串。
 */
QString ExportWorkerHelpers::ensureOutputDir(const struct ExportOptions& options, const QString& subdir) {
    QString outputDir = options.outputPath;  ///< 当前导出的目标目录。
    if (outputDir.isEmpty()) {
        outputDir = defaultOutputDir(subdir);
    }
    QDir dir;  ///< 用于创建目标目录的目录对象。
    if (!dir.mkpath(outputDir)) {
        return QString();
    }
    return outputDir;
}

/**
 * @brief 根据元器件编号构造导出文件路径。
 * @param componentId 元器件编号。
 * @param outputDir 输出目录。
 * @param fileExtension 文件扩展名。
 * @return 拼接后的导出文件路径。
 */
QString ExportWorkerHelpers::buildFilePath(const QString& componentId,
                                           const QString& outputDir,
                                           const QString& fileExtension) {
    return outputDir + QStringLiteral("/") + componentId + fileExtension;
}

/**
 * @brief 判断已有文件是否应因禁止覆盖而跳过。
 * @param filePath 待检查文件路径。
 * @param options 导出选项。
 * @return 文件存在且禁止覆盖时返回 true。
 */
bool ExportWorkerHelpers::shouldSkipExisting(const QString& filePath, const struct ExportOptions& options) {
    return QFile::exists(filePath) && !options.overwriteExistingFiles;
}

/**
 * @brief 加载磁盘缓存中的元器件、CAD 数据和三维模型。
 * @param componentId 元器件编号。
 * @return 加载得到的元器件数据，失败时返回空指针。
 */
QSharedPointer<ComponentData> ExportWorkerHelpers::loadDiskCachedComponentData(const QString& componentId) {
    ComponentCacheService* cache = ComponentCacheService::instance();  ///< 组件缓存服务实例。
    QSharedPointer<ComponentData> cachedData = cache->loadComponentData(componentId);  ///< 从磁盘恢复的组件数据。
    if (!cachedData) {
        return nullptr;
    }

    const QByteArray cadJsonData = cache->loadCadDataJson(componentId);  ///< 缓存中的原始 CAD JSON。
    if (!cadJsonData.isEmpty()) {
        QJsonParseError parseError;  ///< CAD JSON 解析错误信息。
        const QJsonDocument cadDoc = QJsonDocument::fromJson(cadJsonData, &parseError);  ///< 解析后的 CAD JSON 文档。
        if (parseError.error == QJsonParseError::NoError && cadDoc.isObject()) {
            const QJsonObject cadDataObject = cadDoc.object();  ///< CAD JSON 根对象。
            EasyedaSymbolImporter symbolImporter;  ///< EasyEDA 符号导入器。
            EasyedaFootprintImporter footprintImporter;  ///< EasyEDA 封装导入器。
            cachedData->setSymbolData(symbolImporter.importSymbolData(cadDataObject));
            cachedData->setFootprintData(footprintImporter.importFootprintData(cadDataObject));
        }
        // 保存原始 CAD JSON 数据用于 debug 导出
        cachedData->setCadJsonRaw(cadJsonData);
    }

    restoreModel3DFromFootprint(*cachedData);

    // 加载 3D 模型 OBJ 原始数据
    if (cachedData->model3DData() && !cachedData->model3DData()->uuid().isEmpty()) {
        const QByteArray model3DObjData =
            cache->loadModel3D(cachedData->model3DData()->uuid(), QStringLiteral("obj"));  ///< 缓存中的 OBJ 原始数据。
        if (!model3DObjData.isEmpty()) {
            cachedData->setModel3DObjRaw(model3DObjData);
        }
    }

    const QByteArray datasheetData = cache->loadDatasheet(componentId);  ///< 缓存中的数据手册原始数据。
    if (!datasheetData.isEmpty()) {
        cachedData->setDatasheetData(datasheetData);
    }

    return cachedData;
}

/**
 * @brief 使用回退数据补齐目标元器件的缺失字段。
 * @param target 待补齐的目标数据。
 * @param fallback 用于补齐字段的回退数据。
 */
void ExportWorkerHelpers::mergeComponentData(ComponentData& target, const QSharedPointer<ComponentData>& fallback) {
    if (!fallback) {
        return;
    }

    if (target.lcscId().isEmpty()) {
        target = *fallback;
        return;
    }

    if (target.symbolData() == nullptr && fallback->symbolData() != nullptr) {
        target.setSymbolData(fallback->symbolData());
    }
    if (target.footprintData() == nullptr && fallback->footprintData() != nullptr) {
        target.setFootprintData(fallback->footprintData());
    }
    if ((target.model3DData() == nullptr || target.model3DData()->uuid().isEmpty()) &&
        fallback->model3DData() != nullptr) {
        target.setModel3DData(fallback->model3DData());
    }
    if (target.previewImages().isEmpty() && !fallback->previewImages().isEmpty()) {
        target.setPreviewImages(fallback->previewImages());
    }
    if (target.previewImageData().isEmpty() && !fallback->previewImageData().isEmpty()) {
        target.setPreviewImageData(fallback->previewImageData());
    }
    if (target.datasheet().isEmpty() && !fallback->datasheet().isEmpty()) {
        target.setDatasheet(fallback->datasheet());
    }
    if (target.datasheetFormat().isEmpty() && !fallback->datasheetFormat().isEmpty()) {
        target.setDatasheetFormat(fallback->datasheetFormat());
    }
    if (target.datasheetData().isEmpty() && !fallback->datasheetData().isEmpty()) {
        target.setDatasheetData(fallback->datasheetData());
    }
    if (target.name().isEmpty() && !fallback->name().isEmpty()) {
        target.setName(fallback->name());
    }
    if (target.prefix().isEmpty() && !fallback->prefix().isEmpty()) {
        target.setPrefix(fallback->prefix());
    }
    if (target.package().isEmpty() && !fallback->package().isEmpty()) {
        target.setPackage(fallback->package());
    }
    if (target.manufacturer().isEmpty() && !fallback->manufacturer().isEmpty()) {
        target.setManufacturer(fallback->manufacturer());
    }
    if (target.manufacturerPart().isEmpty() && !fallback->manufacturerPart().isEmpty()) {
        target.setManufacturerPart(fallback->manufacturerPart());
    }

    // 合并 raw 数据用于 debug 导出
    if (target.cadJsonRaw().isEmpty() && !fallback->cadJsonRaw().isEmpty()) {
        target.setCadJsonRaw(fallback->cadJsonRaw());
    }
    if (target.cinfoJsonRaw().isEmpty() && !fallback->cinfoJsonRaw().isEmpty()) {
        target.setCinfoJsonRaw(fallback->cinfoJsonRaw());
    }
    if (target.model3DObjRaw().isEmpty() && !fallback->model3DObjRaw().isEmpty()) {
        target.setModel3DObjRaw(fallback->model3DObjRaw());
    }
}

/**
 * @brief 根据各元器件状态重新计算导出阶段计数。
 * @param progress 待重新计算的阶段进度。
 */
void ExportWorkerHelpers::recomputeTypeProgressCounts(ExportTypeProgress& progress) {
    progress.completedCount = 0;
    progress.successCount = 0;
    progress.failedCount = 0;
    progress.skippedCount = 0;
    progress.inProgressCount = 0;

    // 按每个元器件的最终状态累加阶段统计。
    for (auto it = progress.itemStatus.cbegin(); it != progress.itemStatus.cend(); ++it) {
        // 根据当前状态累加完成、成功、失败和跳过数量。
        switch (it.value().status) {
            case ExportItemStatus::Status::Pending:
                break;
            case ExportItemStatus::Status::InProgress:
                progress.inProgressCount++;
                break;
            case ExportItemStatus::Status::Success:
                progress.completedCount++;
                progress.successCount++;
                break;
            case ExportItemStatus::Status::Failed:
                progress.completedCount++;
                progress.failedCount++;
                break;
            case ExportItemStatus::Status::Skipped:
                progress.completedCount++;
                progress.skippedCount++;
                break;
        }
    }
}

}  // namespace EasyKiConverter
