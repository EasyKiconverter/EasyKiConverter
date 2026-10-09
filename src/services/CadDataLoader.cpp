#include "CadDataLoader.h"

#include "ConfigService.h"
#include "core/easyeda/EasyedaFootprintImporter.h"
#include "core/easyeda/EasyedaSymbolImporter.h"
#include "core/network/NetworkClient.h"
#include "core/utils/UrlUtils.h"
#include "models/ComponentData.h"
#include "models/FootprintData.h"
#include "models/Model3DData.h"
#include "models/SymbolData.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace EasyKiConverter {

namespace {
const QString EASYEDA_COMPONENT_API_ENDPOINT =  ///< EasyEDA CAD 数据接口模板。
    QStringLiteral("https://easyeda.com/api/products/%1/components?version=6.5.51");
}  // namespace

/**
 * @brief 解析 EasyEDA CAD JSON 载荷。
 * @param componentId LCSC 元件编号。
 * @param rawData 原始 CAD JSON。
 * @return CAD 解析结果。
 */
CadParseResult CadDataLoader::parseCadPayload(const QString& componentId, const QJsonObject& rawData) {
    CadParseResult parsed;  // 当前 CAD 解析结果。
    parsed.componentId = componentId;

    QJsonObject resultData = rawData;  // 去除外层 result 包装后的数据。
    if (rawData.contains("result")) {
        resultData = rawData.value("result").toObject();
    }

    if (resultData.isEmpty()) {
        parsed.errorMessage = QStringLiteral("Empty CAD data");
        return parsed;
    }

    ComponentData componentData;  // 统一组件数据。
    componentData.setLcscId(componentId);
    componentData.setName(resultData.value("title").toString());
    componentData.setPackage(resultData.value("package").toString());
    componentData.setManufacturer(resultData.value("manufacturer").toString());
    componentData.setDatasheet(resultData.value("datasheet").toString());

    EasyedaSymbolImporter symbolImporter;  // 符号数据导入器。
    QSharedPointer<SymbolData> symbolData = symbolImporter.importSymbolData(resultData);  // 解析后的符号数据。
    if (!symbolData) {
        parsed.errorMessage = QStringLiteral("Failed to parse Symbol data from EasyEDA JSON");
        return parsed;
    }
    componentData.setSymbolData(symbolData);

    if (!symbolData->info().thumb.isEmpty()) {
        const QString thumbUrl = UrlUtils::normalizePreviewImageUrl(symbolData->info().thumb);  // 规范化预览图 URL。
        componentData.setPreviewImages(QStringList() << thumbUrl);
    }

    if (!symbolData->info().datasheet.isEmpty() && componentData.datasheet().isEmpty()) {
        const QString datasheetUrl =
            UrlUtils::normalizePreviewImageUrl(symbolData->info().datasheet);  // 规范化数据手册 URL。
        componentData.setDatasheet(datasheetUrl);
    }

    EasyedaFootprintImporter footprintImporter;  // 封装数据导入器。
    QSharedPointer<FootprintData> footprintData =
        footprintImporter.importFootprintData(resultData);  // 解析后的封装数据。
    if (!footprintData) {
        parsed.errorMessage =
            QStringLiteral("Failed to parse Footprint data from EasyEDA JSON (Library might be corrupted)");
        return parsed;
    }
    componentData.setFootprintData(footprintData);

    // 优先使用 head.uuid_3d（EasyEDA API 3D 模型端点使用的规范 UUID），
    // 其次使用 SVGNODE outline3D 的 uuid（可能是不同 UUID）。
    QString modelUuid;  // 3D 模型 UUID。
    if (resultData.contains("dataStr")) {
        const QJsonObject dataStr = resultData.value("dataStr").toObject();  // EasyEDA dataStr 对象。
        modelUuid = dataStr.value("head").toObject().value("uuid_3d").toString();
    }
    if (modelUuid.isEmpty()) {
        modelUuid = footprintData->model3D().uuid();
    }
    if (modelUuid.isEmpty() && resultData.contains("dataStr")) {
        const QJsonObject dataStr = resultData.value("dataStr").toObject();  // EasyEDA dataStr 对象。
        const QJsonArray shapes = dataStr.value("shape").toArray();  // 原始图形列表。
        for (const QJsonValue& shapeVal : shapes) {  // 查找 3D 轮廓节点。
            const QString shapeStr = shapeVal.toString();  // 当前图形文本。
            if (!shapeStr.startsWith(QStringLiteral("SVGNODE~"))) {
                continue;
            }

            const int tildeIndex = shapeStr.indexOf('~');  // 节点数据分隔符位置。
            if (tildeIndex < 0) {
                continue;
            }

            const QJsonDocument nodeDoc =
                QJsonDocument::fromJson(shapeStr.mid(tildeIndex + 1).toUtf8());  // 节点 JSON。
            const QJsonObject attrs = nodeDoc.object().value("attrs").toObject();  // 节点属性。
            if (attrs.value("c_etype").toString() == QLatin1String("outline3D")) {
                modelUuid = attrs.value("uuid").toString();
                if (!modelUuid.isEmpty()) {
                    break;
                }
            }
        }
    }

    if (!modelUuid.isEmpty()) {
        auto model3DData = QSharedPointer<Model3DData>::create();  // 统一 3D 模型数据。
        model3DData->setUuid(modelUuid);
        model3DData->setName(footprintData->model3D().name());
        model3DData->setTranslation(footprintData->model3D().translation());
        model3DData->setRotation(footprintData->model3D().rotation());
        componentData.setModel3DData(model3DData);
    }

    parsed.resultData = resultData;
    parsed.componentData = componentData;
    parsed.success = true;
    return parsed;
}

/**
 * @brief 请求并解析元件 CAD 数据。
 * @param componentId LCSC 元件编号。
 * @param networkClient 可选网络客户端；为空时使用全局客户端。
 * @return CAD 获取和解析结果。
 */
CadFetchTaskResult CadDataLoader::fetchAndParseCadData(const QString& componentId, INetworkClient* networkClient) {
    CadFetchTaskResult taskResult;  // 当前 CAD 获取任务结果。
    taskResult.componentId = componentId;

    const RequestProfile profile = RequestProfiles::cadData();  // CAD 请求配置。
    const bool weakNetworkSupport =
        networkClient ? false : ConfigService::instance()->getWeakNetworkSupport();  // 弱网模式状态。
    const RetryPolicy policy = RetryPolicy::fromProfile(profile, weakNetworkSupport);  // 请求重试策略。
    const QUrl url(EASYEDA_COMPONENT_API_ENDPOINT.arg(componentId));  // CAD 接口 URL。
    const NetworkResult networkResult = networkClient  // CAD 网络响应。
                                            ? networkClient->get(url, ResourceType::CadData, policy)
                                            : NetworkClient::instance().get(url, ResourceType::CadData, policy);
    if (!networkResult.success) {
        taskResult.errorMessage =
            networkResult.error.isEmpty() ? QStringLiteral("CAD request failed") : networkResult.error;
        return taskResult;
    }

    QJsonParseError parseError;  // JSON 解析错误。
    const QJsonDocument jsonDoc = QJsonDocument::fromJson(networkResult.data, &parseError);  // CAD 响应 JSON。
    if (parseError.error != QJsonParseError::NoError || !jsonDoc.isObject()) {
        taskResult.errorMessage = QStringLiteral("Failed to parse CAD response JSON");
        return taskResult;
    }

    const QJsonObject root = jsonDoc.object();  // CAD 响应根对象。

    // 检查业务层错误：success == false
    if (root.contains("success") && !root.value("success").toBool()) {
        const int code = root.value("code").toInt();  // API 业务错误码。
        const QString message = root.value("message").toString();  // API 业务错误消息。
        if (code == 404) {
            taskResult.errorMessage = QStringLiteral("元器件不存在（404）");
        } else {
            taskResult.errorMessage = message.isEmpty() ? QStringLiteral("API returned success=false") : message;
        }
        return taskResult;
    }

    // 检查 result 字段是否缺失或为 null
    if (!root.contains("result") || root.value("result").isNull()) {
        taskResult.errorMessage = QStringLiteral("API response missing result field");
        return taskResult;
    }

    // 检查 result 是否为空对象
    if (root.value("result").isObject() && root.value("result").toObject().isEmpty()) {
        taskResult.errorMessage = QStringLiteral("Empty CAD data");
        return taskResult;
    }

    taskResult.parsed = parseCadPayload(componentId, root);
    if (!taskResult.parsed.success) {
        taskResult.errorMessage = taskResult.parsed.errorMessage;
        return taskResult;
    }

    taskResult.success = true;
    return taskResult;
}

}  // namespace EasyKiConverter
