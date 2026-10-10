#include "ComponentCacheLoadWorker.h"

#include "ComponentCacheService.h"
#include "core/easyeda/EasyedaFootprintImporter.h"
#include "core/easyeda/EasyedaSymbolImporter.h"

#include <QJsonDocument>
#include <QJsonObject>

namespace EasyKiConverter {

/**
 * @brief 从封装模型复制三维模型的完整元数据。
 * @param component 目标元器件数据。
 * @param footprint 已解析的封装数据。
 */
void ComponentCacheLoadWorker::restoreModel3DFromFootprint(ComponentData& component,
                                                           const QSharedPointer<FootprintData>& footprint) {
    if (!footprint) {
        return;
    }

    const Model3DData footprintModel = footprint->model3D();  // 封装中携带的三维模型元数据。
    if (footprintModel.uuid().isEmpty()) {
        return;
    }
    if (component.model3DData() && !component.model3DData()->uuid().isEmpty()) {
        return;
    }

    auto model3DData = QSharedPointer<Model3DData>::create();  // 待回填到元器件的三维模型数据。
    model3DData->setUuid(footprintModel.uuid());
    model3DData->setName(footprintModel.name());
    model3DData->setTranslation(footprintModel.translation());
    model3DData->setRotation(footprintModel.rotation());
    component.setModel3DData(model3DData);
}

/**
 * @brief 在后台线程读取缓存并完成可并行执行的解析工作。
 * @param componentId 元器件编号。
 * @param fetch3DModel 是否同时读取三维模型。
 * @param cache 缓存服务。
 * @return 缓存加载结果。
 */
ComponentCacheLoadResult ComponentCacheLoadWorker::load(const QString& componentId,
                                                        bool fetch3DModel,
                                                        ComponentCacheService* cache) {
    ComponentCacheLoadResult result;  // 当前元器件的缓存加载结果。
    result.componentId = componentId;

    QSharedPointer<ComponentData> cachedData = cache->loadComponentData(componentId);  // 一级元器件缓存。
    if (!cachedData) {
        return result;
    }

    const QByteArray cadJsonData = cache->loadCadDataJson(componentId);  // CAD JSON 原始数据。
    if (!cadJsonData.isEmpty()) {
        QJsonParseError parseError;  // CAD JSON 解析错误。
        const QJsonDocument cadDoc = QJsonDocument::fromJson(cadJsonData, &parseError);  // 解析后的 JSON 文档。
        if (parseError.error == QJsonParseError::NoError && cadDoc.isObject()) {
            const QJsonObject cadDataObj = cadDoc.object();  // CAD JSON 根对象。
            EasyedaSymbolImporter symbolImporter;  // 符号导入器。
            result.symbolData = symbolImporter.importSymbolData(cadDataObj);
            EasyedaFootprintImporter footprintImporter;  // 封装导入器。
            result.footprintData = footprintImporter.importFootprintData(cadDataObj);
        }
    }

    const QJsonObject metadata = cache->loadMetadata(componentId);  // 缓存元数据。
    if (metadata.contains(QStringLiteral("model3duuid"))) {
        const QString uuid = metadata.value(QStringLiteral("model3duuid")).toString();  // 三维模型标识。
        if (!uuid.isEmpty()) {
            if (!cachedData->model3DData()) {
                auto model3DData = QSharedPointer<Model3DData>::create();  // 从元数据恢复的三维模型。
                model3DData->setUuid(uuid);
                cachedData->setModel3DData(model3DData);
            } else if (cachedData->model3DData()->uuid().isEmpty()) {
                cachedData->model3DData()->setUuid(uuid);
            }
        }
    }

    restoreModel3DFromFootprint(*cachedData, result.footprintData);
    if (!result.symbolData || !result.footprintData) {
        return result;
    }

    result.cachedData = cachedData;
    result.success = true;
    result.encodedPreviewImages = QStringList(3);  // 供界面使用的 Base64 预览图列表。
    for (int i = 0; i < 3; ++i) {
        const QByteArray imageData = cache->loadPreviewImage(componentId, i);  // 当前序号的预览图数据。
        if (!imageData.isEmpty()) {
            result.previewImageData.append({i, imageData});
            result.encodedPreviewImages[i] = QString::fromLatin1(imageData.toBase64());
        }
    }

    result.datasheetData = cache->loadDatasheet(componentId);
    if (fetch3DModel && cachedData->model3DData() && !cachedData->model3DData()->uuid().isEmpty()) {
        const QByteArray objData =  // 缓存中的 OBJ 模型数据。
            cache->loadModel3D(cachedData->model3DData()->uuid(), QStringLiteral("obj"));
        if (!objData.isEmpty()) {
            cachedData->setModel3DObjRaw(objData);
        }
    }

    return result;
}

}  // namespace EasyKiConverter
