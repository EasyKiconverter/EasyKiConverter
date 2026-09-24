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

constexpr double kModelZOffsetEpsilonMm = 0.01;
constexpr double kEasyEdaUnitToMm = 0.254;
constexpr double kEasyEdaZOffsetBias = 0.000001;
constexpr auto kSmdFootprintType = "smd";

// 从 STEP 顶点计算模型包围盒中心，并以最低 Z 作为对齐基准。
bool calculateStepGeometryCenter(const QByteArray& stepData, Model3DBase* center) {
    if (stepData.isEmpty() || center == nullptr) {
        return false;
    }

    const QString content = QString::fromLatin1(stepData);
    static const QRegularExpression pointRegex(
        QStringLiteral("#(\\d+)\\s*=\\s*CARTESIAN_POINT\\s*\\(\\s*('[^']*'|\\$)\\s*,\\s*\\(([^()]*)\\)\\s*\\)"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression vertexPointRegex(QStringLiteral("VERTEX_POINT\\s*\\([^,]*,\\s*#(\\d+)\\s*\\)"),
                                                     QRegularExpression::CaseInsensitiveOption);

    struct Vec3 {
        double x = 0;
        double y = 0;
        double z = 0;
    };

    QHash<int, Vec3> allPoints;
    QRegularExpressionMatchIterator pointMatches = pointRegex.globalMatch(content);
    while (pointMatches.hasNext()) {
        const QRegularExpressionMatch match = pointMatches.next();
        bool idOk = false;
        const int pointId = match.captured(1).toInt(&idOk);
        if (!idOk) {
            continue;
        }

        const QStringList coordinateParts = match.captured(3).split(',', Qt::SkipEmptyParts);
        if (coordinateParts.size() < 3) {
            continue;
        }

        bool okX = false;
        bool okY = false;
        bool okZ = false;
        const double x = coordinateParts.at(0).trimmed().toDouble(&okX);
        const double y = coordinateParts.at(1).trimmed().toDouble(&okY);
        const double z = coordinateParts.at(2).trimmed().toDouble(&okZ);
        if (okX && okY && okZ) {
            allPoints.insert(pointId, {x, y, z});
        }
    }

    double minX = std::numeric_limits<double>::max();
    double maxX = std::numeric_limits<double>::lowest();
    double minY = std::numeric_limits<double>::max();
    double maxY = std::numeric_limits<double>::lowest();
    double minZ = std::numeric_limits<double>::max();
    bool hasGeometryPoint = false;
    QRegularExpressionMatchIterator vertexMatches = vertexPointRegex.globalMatch(content);
    while (vertexMatches.hasNext()) {
        bool ok = false;
        const int pointId = vertexMatches.next().captured(1).toInt(&ok);
        if (!ok || !allPoints.contains(pointId)) {
            continue;
        }

        const Vec3 point = allPoints.value(pointId);
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

// 根据 WRL 和 STEP 的最低 Z 坐标计算模型对齐偏移。
double calculateStepZOffset(double wrlDisplayMinZ, double stepMinZ) {
    if (wrlDisplayMinZ == std::numeric_limits<double>::max()) {
        return stepMinZ > kModelZOffsetEpsilonMm ? -stepMinZ : 0.0;
    }
    const double offset = wrlDisplayMinZ - stepMinZ;
    return qAbs(offset) < kModelZOffsetEpsilonMm ? 0.0 : offset;
}

// 从 CAD 原始 JSON 中读取指定三维模型的源 Z 坐标并换算为毫米。
bool readModelSourceZMm(const QByteArray& cadJsonRaw, const QString& modelUuid, double* zMm) {
    if (cadJsonRaw.isEmpty() || modelUuid.isEmpty() || zMm == nullptr) {
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument cadDoc = QJsonDocument::fromJson(cadJsonRaw, &parseError);
    if (parseError.error != QJsonParseError::NoError || !cadDoc.isObject()) {
        return false;
    }

    const QJsonObject packageData =
        cadDoc.object().value(QStringLiteral("packageDetail")).toObject().value(QStringLiteral("dataStr")).toObject();
    const QJsonArray shapes = packageData.value(QStringLiteral("shape")).toArray();
    for (int pass = 0; pass < 2; ++pass) {
        for (const QJsonValue& shapeValue : shapes) {
            const QString shape = shapeValue.toString();
            if (!shape.startsWith(QStringLiteral("SVGNODE~"))) {
                continue;
            }

            QJsonParseError svgParseError;
            const QJsonDocument svgDoc =
                QJsonDocument::fromJson(shape.mid(QStringLiteral("SVGNODE~").size()).toUtf8(), &svgParseError);
            if (svgParseError.error != QJsonParseError::NoError || !svgDoc.isObject()) {
                continue;
            }

            const QJsonObject attrs = svgDoc.object().value(QStringLiteral("attrs")).toObject();
            if (pass == 0 && attrs.value(QStringLiteral("uuid")).toString() != modelUuid) {
                continue;
            }
            if (pass == 1 && attrs.value(QStringLiteral("c_etype")).toString() != QLatin1String("outline3D")) {
                continue;
            }

            bool ok = false;
            const QJsonValue zValue = attrs.value(QStringLiteral("z"));
            const double z = zValue.isString() ? zValue.toString().toDouble(&ok) : zValue.toDouble();
            ok = ok || zValue.isDouble();
            if (ok) {
                *zMm = GeometryUtils::convertToMm(z);
            }
            return ok;
        }
    }
    return false;
}

// 计算贴片封装 WRL 显示坐标与源模型坐标之间的基础偏移。
double calculateWrlBaseZOffset(const QString& footprintType, double wrlDisplayMinZ, double sourceZMm) {
    if (footprintType != QLatin1String(kSmdFootprintType) || wrlDisplayMinZ == std::numeric_limits<double>::max()) {
        return 0.0;
    }
    const double offset = -wrlDisplayMinZ + sourceZMm;
    return offset > kModelZOffsetEpsilonMm ? offset : 0.0;
}

// 将毫米偏移按 EasyEDA 单位和偏置规则转换为导出值。
double zOffsetMmToEasyEdaUnits(double zOffsetMm) {
    const double roundedOffset = std::round(zOffsetMm * 100.0) / 100.0;
    return -(roundedOffset - kEasyEdaZOffsetBias) / kEasyEdaUnitToMm;
}

}  // namespace

void prepare(FootprintData& footprint,
             const QSharedPointer<ComponentData>& componentData,
             const QString& componentId,
             uint64_t generation) {
    Model3DData model3D = footprint.model3D();
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

    QByteArray stepData = componentData->model3DData() ? componentData->model3DData()->step() : QByteArray();
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
        Exporter3DModel exporter;
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

    Model3DBase geometryCenter;
    if (!calculateStepGeometryCenter(stepData, &geometryCenter)) {
        qWarning() << "FootprintModel3DPreparation: STEP geometry center unavailable for" << componentId;
    }

    QByteArray objData = componentData->model3DObjRaw();
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

    QByteArray wrlData;
    if (objData.isEmpty()) {
        wrlData = ComponentCacheService::instance()->loadModel3D(model3D.uuid(), QStringLiteral("wrl"));
    }
    if (!wrlData.isEmpty() && !Exporter3DModel::hasUsableWrlGeometry(wrlData)) {
        wrlData.clear();
    }
    if (objData.isEmpty() && wrlData.isEmpty()) {
        Exporter3DModel exporter;
        if (exporter.downloadObjDataSync(model3D.uuid(), &objData) && Exporter3DModel::hasUsableObjGeometry(objData)) {
            ComponentCacheService::instance()->saveModel3D(model3D.uuid(), objData, QStringLiteral("obj"), generation);
        }
    }

    double wrlDisplayMinZ = std::numeric_limits<double>::max();
    if (!objData.isEmpty()) {
        const double rawMinZ = Exporter3DModel::calculateObjMinZ(objData);
        wrlDisplayMinZ = rawMinZ > 0.0 ? 0.0 : rawMinZ;
    } else if (!wrlData.isEmpty()) {
        wrlDisplayMinZ = Exporter3DModel::calculateWrlDisplayMinZ(wrlData);
    }

    QByteArray cadJsonRaw = componentData->cadJsonRaw();
    if (cadJsonRaw.isEmpty()) {
        cadJsonRaw = ComponentCacheService::instance()->loadCadDataJson(componentId);
    }
    double sourceZMm = GeometryUtils::convertToMm(model3D.translation().z);
    (void)readModelSourceZMm(cadJsonRaw, model3D.uuid(), &sourceZMm);
    const double wrlBaseZOffset = calculateWrlBaseZOffset(footprint.info().type, wrlDisplayMinZ, sourceZMm);
    if (wrlBaseZOffset > 0.0) {
        Model3DBase translation = model3D.translation();
        translation.z = zOffsetMmToEasyEdaUnits(wrlBaseZOffset);
        model3D.setTranslation(translation);
    }

    Model3DBase stepOffset;
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
