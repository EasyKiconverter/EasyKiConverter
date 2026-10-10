#include "FootprintModel3DPreparation.h"

#include "core/kicad/Exporter3DModel.h"
#include "core/utils/GeometryUtils.h"
#include "models/ComponentData.h"
#include "services/ComponentCacheService.h"

#include <QDebug>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QRegularExpression>

#include <cmath>
#include <limits>

namespace EasyKiConverter::FootprintModel3DPreparation {

namespace {

constexpr double kModelZOffsetEpsilonMm = 0.01;  ///< 判断模型 Z 偏移是否可忽略的毫米阈值。
constexpr double kEasyEdaUnitToMm = 0.254;  ///< EasyEDA 内部长度单位到毫米的换算系数。
constexpr double kEasyEdaZOffsetBias = 0.000001;  ///< EasyEDA Z 偏移换算时使用的微小修正量。
constexpr auto kSmdFootprintType = "smd";  ///< SMD 封装类型标识。

/**
 * @brief 从 STEP 顶点计算模型包围盒中心，并以最低 Z 作为对齐基准。
 * @param stepData STEP 模型数据。
 * @param center 几何中心。
 */
bool calculateStepGeometryCenter(const QByteArray& stepData, Model3DBase* center) {
    if (stepData.isEmpty() || center == nullptr) {
        return false;
    }

    const QString content = QString::fromLatin1(stepData);  ///< STEP 文本内容。
    static const QRegularExpression pointRegex(  // STEP 笛卡尔点定义匹配器。
        QStringLiteral("#(\\d+)\\s*=\\s*CARTESIAN_POINT\\s*\\(\\s*('[^']*'|\\$)\\s*,\\s*\\(([^()]*)\\)\\s*\\)"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression vertexPointRegex(QStringLiteral("VERTEX_POINT\\s*\\([^,]*,\\s*#(\\d+)\\s*\\)"),
                                                     QRegularExpression::CaseInsensitiveOption);

    struct Vec3 {
        double x = 0;  ///< 三维点的 X 坐标。
        double y = 0;  ///< 三维点的 Y 坐标。
        double z = 0;  ///< 三维点的 Z 坐标。
    };

    QHash<int, Vec3> allPoints;  ///< STEP 点编号到坐标的映射。
    QRegularExpressionMatchIterator pointMatches = pointRegex.globalMatch(content);  ///< 笛卡尔点匹配迭代器。
    while (pointMatches.hasNext()) {
        const QRegularExpressionMatch match = pointMatches.next();  ///< 当前点定义匹配结果。
        bool idOk = false;  ///< 点编号是否解析成功。
        const int pointId = match.captured(1).toInt(&idOk);  ///< 当前 STEP 点编号。
        if (!idOk) {
            continue;
        }

        const QStringList coordinateParts = match.captured(3).split(',', Qt::SkipEmptyParts);  ///< 点坐标分量。
        if (coordinateParts.size() < 3) {
            continue;
        }

        bool okX = false;  ///< X 坐标是否解析成功。
        bool okY = false;  ///< Y 坐标是否解析成功。
        bool okZ = false;  ///< Z 坐标是否解析成功。
        const double x = coordinateParts.at(0).trimmed().toDouble(&okX);  ///< 点的 X 坐标。
        const double y = coordinateParts.at(1).trimmed().toDouble(&okY);  ///< 点的 Y 坐标。
        const double z = coordinateParts.at(2).trimmed().toDouble(&okZ);  ///< 点的 Z 坐标。
        if (okX && okY && okZ) {
            allPoints.insert(pointId, {x, y, z});
        }
    }

    double minX = std::numeric_limits<double>::max();  ///< 几何点的最小 X 坐标。
    double maxX = std::numeric_limits<double>::lowest();  ///< 几何点的最大 X 坐标。
    double minY = std::numeric_limits<double>::max();  ///< 几何点的最小 Y 坐标。
    double maxY = std::numeric_limits<double>::lowest();  ///< 几何点的最大 Y 坐标。
    double minZ = std::numeric_limits<double>::max();  ///< 几何点的最小 Z 坐标。
    bool hasGeometryPoint = false;  ///< 是否找到至少一个被几何引用的点。
    QRegularExpressionMatchIterator vertexMatches = vertexPointRegex.globalMatch(content);  ///< 顶点匹配迭代器。
    while (vertexMatches.hasNext()) {
        bool ok = false;  ///< 顶点编号是否解析成功。
        const int pointId = vertexMatches.next().captured(1).toInt(&ok);  ///< 当前顶点引用的点编号。
        if (!ok || !allPoints.contains(pointId)) {
            continue;
        }

        const Vec3 point = allPoints.value(pointId);  ///< 当前几何点坐标。
        minX = qMin(minX, point.x);
        maxX = qMax(maxX, point.x);
        minY = qMin(minY, point.y);
        maxY = qMax(maxY, point.y);
        minZ = qMin(minZ, point.z);
        hasGeometryPoint = true;
    }

    if (!hasGeometryPoint) {
        return false;
    }

    center->x = (minX + maxX) / 2.0;
    center->y = (minY + maxY) / 2.0;
    center->z = minZ;
    return true;
}

/**
 * @brief 根据 WRL 和 STEP 的最低 Z 坐标计算模型对齐偏移。
 * @param wrlDisplayMinZ WRL 显示最小 Z 坐标。
 * @param stepMinZ STEP 模型最小 Z 坐标。
 */
double calculateStepZOffset(double wrlDisplayMinZ, double stepMinZ) {
    if (wrlDisplayMinZ == std::numeric_limits<double>::max()) {
        return stepMinZ > kModelZOffsetEpsilonMm ? -stepMinZ : 0.0;
    }
    const double offset = wrlDisplayMinZ - stepMinZ;  ///< WRL 与 STEP 最低点之间的 Z 偏移。
    return qAbs(offset) < kModelZOffsetEpsilonMm ? 0.0 : offset;
}

/**
 * @brief 从 CAD 原始 JSON 中读取指定三维模型的源 Z 坐标并换算为毫米。
 * @param cadJsonRaw 原始 CAD JSON 数据。
 * @param modelUuid 三维模型 UUID。
 * @param zMm Z 轴坐标（毫米）。
 */
bool readModelSourceZMm(const QByteArray& cadJsonRaw, const QString& modelUuid, double* zMm) {
    if (cadJsonRaw.isEmpty() || modelUuid.isEmpty() || zMm == nullptr) {
        return false;
    }

    QJsonParseError parseError;  ///< CAD JSON 解析错误信息。
    const QJsonDocument cadDoc = QJsonDocument::fromJson(cadJsonRaw, &parseError);  ///< 解析后的 CAD JSON 文档。
    if (parseError.error != QJsonParseError::NoError || !cadDoc.isObject()) {
        return false;
    }

    const QJsonObject packageData =  // 封装详情中的原始数据对象。
        cadDoc.object().value(QStringLiteral("packageDetail")).toObject().value(QStringLiteral("dataStr")).toObject();
    const QJsonArray shapes = packageData.value(QStringLiteral("shape")).toArray();  ///< 封装图形数组。
    for (int pass = 0; pass < 2; ++pass) {  // 当前按 UUID 或类型的匹配轮次。
        for (const QJsonValue& shapeValue : shapes) {  // 当前图形 JSON 值。
            const QString shape = shapeValue.toString();  ///< SVG 节点编码字符串。
            if (!shape.startsWith(QStringLiteral("SVGNODE~"))) {
                continue;
            }

            QJsonParseError svgParseError;  ///< SVG 节点 JSON 解析错误信息。
            const QJsonDocument svgDoc =  // 解析后的 SVG 节点文档。
                QJsonDocument::fromJson(shape.mid(QStringLiteral("SVGNODE~").size()).toUtf8(), &svgParseError);
            if (svgParseError.error != QJsonParseError::NoError || !svgDoc.isObject()) {
                continue;
            }

            const QJsonObject attrs = svgDoc.object().value(QStringLiteral("attrs")).toObject();  ///< SVG 节点属性。
            if (pass == 0 && attrs.value(QStringLiteral("uuid")).toString() != modelUuid) {
                continue;
            }
            if (pass == 1 && attrs.value(QStringLiteral("c_etype")).toString() != QLatin1String("outline3D")) {
                continue;
            }

            bool ok = false;  ///< Z 坐标是否解析成功。
            const QJsonValue zValue = attrs.value(QStringLiteral("z"));  ///< 源模型 Z 坐标值。
            const double z = zValue.isString() ? zValue.toString().toDouble(&ok) : zValue.toDouble();  ///< 源 Z 坐标。
            ok = ok || zValue.isDouble();
            if (ok) {
                *zMm = GeometryUtils::convertToMm(z);
            }
            return ok;
        }
    }
    return false;
}

/**
 * @brief 计算贴片封装 WRL 显示坐标与源模型坐标之间的基础偏移。
 * @param footprintType 封装类型。
 * @param wrlDisplayMinZ WRL 显示最小 Z 坐标。
 * @param sourceZMm 源 Z 轴坐标（毫米）。
 */
double calculateWrlBaseZOffset(const QString& footprintType, double wrlDisplayMinZ, double sourceZMm) {
    if (footprintType != QLatin1String(kSmdFootprintType) || wrlDisplayMinZ == std::numeric_limits<double>::max()) {
        return 0.0;
    }
    const double offset = -wrlDisplayMinZ + sourceZMm;  ///< SMD 模型需要补偿的基础 Z 偏移。
    return offset > kModelZOffsetEpsilonMm ? offset : 0.0;
}

/**
 * @brief 将毫米偏移按 EasyEDA 单位和偏置规则转换为导出值。
 * @param zOffsetMm Z 轴偏移量（毫米）。
 */
double zOffsetMmToEasyEdaUnits(double zOffsetMm) {
    const double roundedOffset = std::round(zOffsetMm * 100.0) / 100.0;  ///< 按 EasyEDA 精度舍入后的偏移。
    return -(roundedOffset - kEasyEdaZOffsetBias) / kEasyEdaUnitToMm;
}

}  // namespace

/**
 * @brief 准备封装关联的三维模型数据、缓存内容和坐标偏移。
 * @param footprint 待更新的封装数据。
 * @param componentData 元器件缓存数据。
 * @param componentId 元器件编号。
 * @param generation 当前缓存代次。
 */
void prepare(FootprintData& footprint,
             const QSharedPointer<ComponentData>& componentData,
             const QString& componentId,
             uint64_t generation) {
    Model3DData model3D = footprint.model3D();  ///< 当前封装的三维模型描述。
    if (componentData->model3DData() && !componentData->model3DData()->uuid().isEmpty()) {
        model3D.setUuid(componentData->model3DData()->uuid());
    }
    if (model3D.name().isEmpty() && componentData->model3DData()) {
        model3D.setName(componentData->model3DData()->name());
    }
    if (model3D.name().isEmpty()) {
        model3D.setName(footprint.info().model3DName);
    }
    if (model3D.uuid().isEmpty()) {
        return;
    }

    QByteArray stepData =
        componentData->model3DData() ? componentData->model3DData()->step() : QByteArray();  ///< STEP 数据。
    if (stepData.isEmpty()) {
        stepData = ComponentCacheService::instance()->loadModel3D(model3D.uuid(), QStringLiteral("step"));
    }
    if (!stepData.isEmpty() && !Exporter3DModel::hasUsableStepData(stepData)) {
        qWarning() << "FootprintModel3DPreparation: Ignoring malformed cached STEP for" << componentId << "uuid"
                   << model3D.uuid();
        stepData.clear();
    }
    if (!stepData.isEmpty()) {
        ComponentCacheService::instance()->saveModel3D(model3D.uuid(), stepData, QStringLiteral("step"), generation);
    }
    if (stepData.isEmpty()) {
        Exporter3DModel exporter;  ///< 用于同步补回 STEP 数据的三维模型导出器。
        if (exporter.downloadStepDataSync(model3D.uuid(), &stepData) && Exporter3DModel::hasUsableStepData(stepData)) {
            ComponentCacheService::instance()->saveModel3D(
                model3D.uuid(), stepData, QStringLiteral("step"), generation);
        }
    }
    if (!stepData.isEmpty() && !Exporter3DModel::hasUsableStepData(stepData)) {
        qWarning() << "FootprintModel3DPreparation: Rejecting invalid downloaded STEP for" << componentId << "uuid"
                   << model3D.uuid();
        stepData.clear();
    }
    if (stepData.isEmpty()) {
        return;
    }

    Model3DBase geometryCenter;  ///< STEP 几何中心及最低 Z 坐标。
    if (!calculateStepGeometryCenter(stepData, &geometryCenter)) {
        qWarning() << "FootprintModel3DPreparation: STEP geometry center unavailable for" << componentId;
    }

    QByteArray objData = componentData->model3DObjRaw();  ///< OBJ 三维模型数据。
    if (objData.isEmpty() && componentData->model3DData()) {
        objData = componentData->model3DData()->rawObj().toUtf8();
    }
    if (!objData.isEmpty() && !Exporter3DModel::hasUsableObjGeometry(objData)) {
        qWarning() << "FootprintModel3DPreparation: Ignoring malformed OBJ data for" << componentId;
        objData.clear();
    }
    if (!objData.isEmpty()) {
        ComponentCacheService::instance()->saveModel3D(model3D.uuid(), objData, QStringLiteral("obj"), generation);
    }
    if (objData.isEmpty()) {
        objData = ComponentCacheService::instance()->loadModel3D(model3D.uuid(), QStringLiteral("obj"));
    }
    if (!objData.isEmpty() && !Exporter3DModel::hasUsableObjGeometry(objData)) {
        objData.clear();
    }

    QByteArray wrlData;  ///< WRL 三维模型数据。
    if (objData.isEmpty()) {
        wrlData = ComponentCacheService::instance()->loadModel3D(model3D.uuid(), QStringLiteral("wrl"));
    }
    if (!wrlData.isEmpty() && !Exporter3DModel::hasUsableWrlGeometry(wrlData)) {
        wrlData.clear();
    }
    if (objData.isEmpty() && wrlData.isEmpty()) {
        Exporter3DModel exporter;  ///< 用于同步补回 OBJ 数据的三维模型导出器。
        if (exporter.downloadObjDataSync(model3D.uuid(), &objData) && Exporter3DModel::hasUsableObjGeometry(objData)) {
            ComponentCacheService::instance()->saveModel3D(model3D.uuid(), objData, QStringLiteral("obj"), generation);
        }
    }

    double wrlDisplayMinZ = std::numeric_limits<double>::max();  ///< WRL/OBJ 显示坐标的最低 Z 值。
    if (!objData.isEmpty()) {
        const double rawMinZ = Exporter3DModel::calculateObjMinZ(objData);  ///< OBJ 原始几何的最低 Z 值。
        wrlDisplayMinZ = rawMinZ > 0.0 ? 0.0 : rawMinZ;
    } else if (!wrlData.isEmpty()) {
        wrlDisplayMinZ = Exporter3DModel::calculateWrlDisplayMinZ(wrlData);
    }

    QByteArray cadJsonRaw = componentData->cadJsonRaw();  ///< 用于读取模型源 Z 坐标的 CAD JSON。
    if (cadJsonRaw.isEmpty()) {
        cadJsonRaw = ComponentCacheService::instance()->loadCadDataJson(componentId);
    }
    double sourceZMm = GeometryUtils::convertToMm(model3D.translation().z);  ///< 源模型 Z 坐标（毫米）。
    (void)readModelSourceZMm(cadJsonRaw, model3D.uuid(), &sourceZMm);
    const double wrlBaseZOffset =
        calculateWrlBaseZOffset(footprint.info().type, wrlDisplayMinZ, sourceZMm);  ///< WRL 基础 Z 补偿。
    if (wrlBaseZOffset > 0.0) {
        Model3DBase translation = model3D.translation();  ///< 应用补偿前的模型平移量。
        translation.z = zOffsetMmToEasyEdaUnits(wrlBaseZOffset);
        model3D.setTranslation(translation);
    }

    Model3DBase stepOffset;  ///< STEP 模型相对于封装原点的中心偏移。
    stepOffset.x = -geometryCenter.x;
    stepOffset.y = -geometryCenter.y;
    stepOffset.z = calculateStepZOffset(wrlDisplayMinZ, geometryCenter.z);
    model3D.setStepOffsetMm(stepOffset);
    model3D.setStep(stepData);
    footprint.setModel3D(model3D);
    qDebug() << "FootprintModel3DPreparation: Prepared model for" << componentId << "uuid" << model3D.uuid()
             << "offset:" << stepOffset.x << stepOffset.y << stepOffset.z;
}

}  // namespace EasyKiConverter::FootprintModel3DPreparation
