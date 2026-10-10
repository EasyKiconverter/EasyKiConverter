#include "Exporter3DModel.h"

#include "core/network/NetworkClient.h"

#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QTextStream>
#include <QVector>

#include <cmath>
#include <limits>

namespace EasyKiConverter {

// API 端点
static const QString ENDPOINT_3D_MODEL = "https://modules.easyeda.com/3dmodel/%1";
static const QString ENDPOINT_3D_MODEL_STEP = "https://modules.easyeda.com/qAxj6KHrDKw4blvCG8QJPs7Y/%1";
static constexpr double WRL_UNIT_TO_MM = 2.54;  ///< WRL 单位到毫米的换算系数。

Exporter3DModel::Exporter3DModel(QObject* parent) : QObject(parent) {}

Exporter3DModel::~Exporter3DModel() {
    cancel();
}

/**
 * @brief 异步接口包装 OBJ 文件下载并发送结果信号。
 * @param uuid 稳定对象标识符。
 * @param savePath 目标保存路径。
 */
void Exporter3DModel::downloadObjModel(const QString& uuid, const QString& savePath) {
    QString errorMessage;  ///< OBJ 下载失败时返回的错误信息。
    if (downloadObjModelSync(uuid, savePath, &errorMessage)) {
        emit downloadSuccess(savePath);
        return;
    }

    emit downloadError(errorMessage);
}

/**
 * @brief 异步接口包装 STEP 文件下载并发送结果信号。
 * @param uuid 稳定对象标识符。
 * @param savePath 目标保存路径。
 */
void Exporter3DModel::downloadStepModel(const QString& uuid, const QString& savePath) {
    QString errorMessage;  ///< STEP 下载失败时返回的错误信息。
    if (downloadStepModelSync(uuid, savePath, &errorMessage)) {
        emit downloadSuccess(savePath);
        return;
    }

    emit downloadError(errorMessage);
}

/**
 * @brief 同步下载 OBJ 文件并保存到指定路径。
 * @param uuid 稳定对象标识符。
 * @param savePath 目标保存路径。
 * @param errorMessage 输出错误信息的指针。
 */
bool Exporter3DModel::downloadObjModelSync(const QString& uuid, const QString& savePath, QString* errorMessage) {
    return downloadModelSync(uuid, savePath, ModelFormat::OBJ, errorMessage);
}

/**
 * @brief 同步下载 STEP 文件并保存到指定路径。
 * @param uuid 稳定对象标识符。
 * @param savePath 目标保存路径。
 * @param errorMessage 输出错误信息的指针。
 */
bool Exporter3DModel::downloadStepModelSync(const QString& uuid, const QString& savePath, QString* errorMessage) {
    return downloadModelSync(uuid, savePath, ModelFormat::STEP, errorMessage);
}

/**
 * @brief 同步下载 OBJ 原始字节数据。
 * @param uuid 稳定对象标识符。
 * @param data 数据缓冲区。
 * @param errorMessage 输出错误信息的指针。
 */
bool Exporter3DModel::downloadObjDataSync(const QString& uuid, QByteArray* data, QString* errorMessage) {
    return downloadModelDataSync(uuid, ModelFormat::OBJ, data, errorMessage);
}

/**
 * @brief 同步下载 STEP 原始字节数据。
 * @param uuid 稳定对象标识符。
 * @param data 数据缓冲区。
 * @param errorMessage 输出错误信息的指针。
 */
bool Exporter3DModel::downloadStepDataSync(const QString& uuid, QByteArray* data, QString* errorMessage) {
    return downloadModelDataSync(uuid, ModelFormat::STEP, data, errorMessage);
}

/**
 * @brief 将模型 IR 中的 OBJ 数据转换为 WRL 文件。
 * @param model 三维模型数据。
 * @param savePath 目标保存路径。
 */
bool Exporter3DModel::exportToWrl(const IR::Model3DIR& model, const QString& savePath) {
    QFile file(savePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "Failed to open file for writing:" << savePath;
        return false;
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);

    // 获取 OBJ 数据
    QByteArray objData = model.rawObj().toUtf8();

    // 生成 WRL 文件内容
    QString content = generateWrlContent(model, objData);

    out << content;
    file.flush();
    file.close();

    return true;
}

/**
 * @brief 将模型 IR 中的 STEP 字节数据写入文件。
 * @param model 三维模型数据。
 * @param savePath 目标保存路径。
 */
bool Exporter3DModel::exportToStep(const IR::Model3DIR& model, const QString& savePath) {
    QFile file(savePath);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "Failed to open file for writing:" << savePath;
        return false;
    }

    file.write(model.stepData());
    file.close();

    return true;
}

/**
 * @brief 解析 OBJ 顶点并计算其最小 Z 坐标。
 * @param objData OBJ 模型数据。
 */
double Exporter3DModel::calculateObjMinZ(const QByteArray& objData) {
    if (objData.isEmpty()) {
        return std::numeric_limits<double>::max();
    }

    double minZ = std::numeric_limits<double>::max();  ///< 当前解析到的最小 Z 坐标。
    int start = 0;  ///< 当前 OBJ 行的起始偏移。
    while (start < objData.size()) {
        int end = objData.indexOf('\n', start);  ///< 当前 OBJ 行的结束偏移。
        if (end < 0) {
            end = objData.size();
        }

        // 检查行是否以 "v " 开头
        if (end - start >= 2 && objData.at(start) == 'v' && objData.at(start + 1) == ' ') {
            // 跳过 "v " 前缀，解析三个坐标分量
            int pos = start + 2;
            // 跳过前两个分量 (x, y)
            int component = 0;
            while (pos < end && component < 2) {
                if (objData.at(pos) == ' ') {
                    component++;
                    // 跳过连续空格
                    while (pos < end && objData.at(pos) == ' ') {
                        pos++;
                    }
                } else {
                    pos++;
                }
            }

            // 解析 Z 分量
            if (pos < end) {
                bool ok = false;  ///< Z 坐标解析是否成功。
                double z = QByteArray(objData.constData() + pos, end - pos).toDouble(&ok);  ///< 原始 Z 坐标。
                if (ok) {
                    minZ = qMin(minZ, z / WRL_UNIT_TO_MM);
                }
            }
        }

        start = end + 1;
    }
    if (minZ == std::numeric_limits<double>::max()) {
        return minZ;
    }

    return minZ;
}

/**
 * @brief 检查 OBJ 是否至少包含可解析的顶点和三点面，避免把错误响应当作模型缓存。
 * @param objData OBJ 模型数据。
 */
bool Exporter3DModel::hasUsableObjGeometry(const QByteArray& objData) {
    if (objData.isEmpty()) {
        return false;
    }

    bool hasVertex = false;  ///< 是否发现至少一个有效顶点。
    bool hasFace = false;  ///< 是否发现至少一个有效面。
    const QStringList lines = QString::fromUtf8(objData).split('\n');  ///< OBJ 文本行列表。
    for (const QString& line : lines) {
        const QStringList parts = line.trimmed().split(' ', Qt::SkipEmptyParts);  ///< 当前 OBJ 行的字段。
        if (parts.isEmpty()) {
            continue;
        }

        if (parts[0] == QStringLiteral("v") && parts.size() >= 4) {
            bool validVertex = true;  ///< 当前顶点的三个坐标是否都有效。
            for (int index = 1; index <= 3; ++index) {
                bool ok = false;  ///< 当前坐标字段解析是否成功。
                const double coordinate = parts[index].toDouble(&ok);  ///< 当前顶点坐标分量。
                validVertex = validVertex && ok && std::isfinite(coordinate);
            }
            hasVertex = hasVertex || validVertex;
        } else if (parts[0] == QStringLiteral("f") && parts.size() >= 4) {
            bool validFace = true;  ///< 当前面的所有顶点索引是否有效。
            for (int index = 1; index < parts.size(); ++index) {
                const QString vertexIndex = parts[index].section('/', 0, 0);  ///< 当前面的顶点索引文本。
                bool ok = false;  ///< 顶点索引解析是否成功。
                const int parsedIndex = vertexIndex.toInt(&ok);  ///< 解析后的顶点索引。
                validFace = validFace && ok && parsedIndex != 0;
            }
            hasFace = hasFace || validFace;
        }
    }

    return hasVertex && hasFace;
}

/**
 * @brief 检查 WRL 是否包含文件头、至少三个坐标点和可闭合的面索引。
 * @param wrlData WRL 模型数据。
 */
bool Exporter3DModel::hasUsableWrlGeometry(const QByteArray& wrlData) {
    if (wrlData.isEmpty()) {
        return false;
    }

    const QString text = QString::fromUtf8(wrlData);  ///< WRL 文本内容。
    if (!text.trimmed().startsWith(QStringLiteral("#VRML V2.0"))) {
        return false;
    }

    static const QRegularExpression numberRegex(QStringLiteral("[-+]?(?:\\d+\\.?\\d*|\\.\\d+)(?:[eE][-+]?\\d+)?"));
    const int pointStart = text.indexOf(QStringLiteral("point ["));  ///< WRL 顶点数组起始位置。
    const int pointEnd = pointStart < 0 ? -1 : text.indexOf(QLatin1Char(']'), pointStart);  ///< WRL 顶点数组结束位置。
    const int coordIndexStart = text.indexOf(QStringLiteral("coordIndex ["));  ///< 面索引数组起始位置。
    const int coordIndexEnd =
        coordIndexStart < 0 ? -1 : text.indexOf(QLatin1Char(']'), coordIndexStart);  ///< 面索引数组结束位置。
    if (pointStart < 0 || pointEnd < 0 || coordIndexStart < 0 || coordIndexEnd < 0) {
        return false;
    }

    const QString pointData =
        text.mid(pointStart + QStringLiteral("point [").size(), pointEnd - pointStart);  ///< 顶点数组文本。
    const int coordinateCount = pointData.count(numberRegex);  ///< 顶点坐标数。
    const QString indexData =
        text.mid(coordIndexStart + QStringLiteral("coordIndex [").size(), coordIndexEnd - coordIndexStart);
    return coordinateCount >= 9 && indexData.contains(QStringLiteral("-1"));
}

/**
 * @brief 检查 STEP 是否包含完整交换文件的起始和结束标记。
 * @param stepData STEP 模型数据。
 */
bool Exporter3DModel::hasUsableStepData(const QByteArray& stepData) {
    const QByteArray normalized = stepData.trimmed().toUpper();  ///< 规范化后的 STEP 文本。
    return normalized.startsWith("ISO-10303-21;") && normalized.contains("END-ISO-10303-21;");
}

/**
 * @brief 解析 WRL 坐标点并计算 KiCad 显示单位下的最小 Z 坐标。
 * @param wrlData WRL 模型数据。
 */
double Exporter3DModel::calculateWrlDisplayMinZ(const QByteArray& wrlData) {
    if (wrlData.isEmpty()) {
        return std::numeric_limits<double>::max();
    }

    static const QRegularExpression numberRegex(QStringLiteral("[-+]?(?:\\d+\\.?\\d*|\\.\\d+)(?:[eE][-+]?\\d+)?"));

    double minZ = std::numeric_limits<double>::max();  ///< 当前解析到的最小 Z 坐标。
    bool inPointArray = false;  ///< 是否正在解析 WRL 顶点数组。
    int coordinateComponent = 0;  ///< 当前顶点坐标分量序号。
    const QStringList lines = QString::fromUtf8(wrlData).split('\n');  ///< WRL 文本行列表。
    for (const QString& line : lines) {
        QString segment = line;  ///< 当前待解析的 WRL 文本片段。
        if (!inPointArray) {
            const int pointStart = segment.indexOf(QStringLiteral("point ["));  ///< 当前行中顶点数组的起始位置。
            if (pointStart < 0) {
                continue;
            }
            inPointArray = true;
            coordinateComponent = 0;
            segment = segment.mid(pointStart + QStringLiteral("point [").size());
        }

        const int pointEnd = segment.indexOf(QLatin1Char(']'));  ///< 当前片段中顶点数组的结束位置。
        if (pointEnd >= 0) {
            segment = segment.left(pointEnd);
        }

        QRegularExpressionMatchIterator matches = numberRegex.globalMatch(segment);  ///< 当前片段中的数值匹配迭代器。
        while (matches.hasNext()) {
            const QRegularExpressionMatch match = matches.next();  ///< 当前匹配到的数值文本。
            bool ok = false;  ///< 数值解析是否成功。
            const double value = match.captured(0).toDouble(&ok);  ///< 当前坐标分量。
            if (!ok) {
                continue;
            }
            if (coordinateComponent % 3 == 2) {
                minZ = qMin(minZ, value * WRL_UNIT_TO_MM);
            }
            coordinateComponent++;
        }

        if (pointEnd >= 0) {
            inPointArray = false;
        }
    }

    return minZ;
}

/**
 * @brief 同步网络请求无需额外取消动作，保留接口以满足导出器协议。
 */
void Exporter3DModel::cancel() {
    // 使用同步的 NetworkClient，不需要取消请求
}

/**
 * @brief 根据解析后的 OBJ 数据生成 WRL 文本内容。
 * @param model 三维模型数据。
 * @param objData OBJ 模型数据。
 */
QString Exporter3DModel::generateWrlContent(const IR::Model3DIR& model, const QByteArray& objData) {
    QString content;  ///< 待写入 WRL 文件的文本内容。

    // 解析 OBJ 数据
    QJsonObject parsedData = parseObjData(objData);  ///< 解析后的 OBJ 数据对象。
    QJsonArray vertices = parsedData["vertices"].toArray();  ///< 归一化后的顶点数组。
    QJsonArray faces = parsedData["faces"].toArray();  ///< OBJ 面索引数组。
    QJsonObject materials = parsedData["materials"].toObject();  ///< OBJ 材质映射。
    QJsonArray shapes = parsedData["shapes"].toArray();  ///< 按材质拆分的形状数组。

    // WRL 文件头部
    content += "#VRML V2.0 utf8\n";
    content += "# 3D model generated by EasyKiConverter (https://github.com/EasyKiconverter/EasyKiConverter)\n";
    content += "\n";

    // 如果有形状数据，使用形状数据生成 WRL
    if (!shapes.isEmpty()) {
        for (const QJsonValue& shapeValue : shapes) {
            QJsonObject shape = shapeValue.toObject();  ///< 当前 WRL 形状对象。
            QString materialId = shape["materialId"].toString();  ///< 当前形状关联的材质 ID。
            QJsonArray shapePoints = shape["points"].toArray();  ///< 当前形状的顶点文本数组。
            QJsonArray coordIndex = shape["coordIndex"].toArray();  ///< 当前形状的面索引数组。

            // 在倒数第二个位置插入最后一个点的副本
            if (shapePoints.size() > 0) {
                shapePoints.insert(shapePoints.size() - 1, shapePoints.last());
            }

            // 获取材质信息
            QJsonObject material = materials[materialId].toObject();  ///< 当前形状的材质对象。
            QJsonArray diffuseColor = material["diffuseColor"].toArray();  ///< 材质漫反射颜色。
            QJsonArray specularColor = material["specularColor"].toArray();  ///< 材质高光颜色。
            QString transparency = material["transparency"].toString("0");  ///< 材质透明度文本。

            // 生成 Shape
            content += "Shape {\n";
            content += "  appearance Appearance {\n";
            content += "    material Material {\n";
            content += QString("      diffuseColor %1 %2 %3\n")
                           .arg(diffuseColor.isEmpty() ? "0.8" : diffuseColor[0].toString())
                           .arg(diffuseColor.isEmpty() ? "0.8" : diffuseColor[1].toString())
                           .arg(diffuseColor.isEmpty() ? "0.8" : diffuseColor[2].toString());
            content += QString("      specularColor %1 %2 %3\n")
                           .arg(specularColor.isEmpty() ? "0.2" : specularColor[0].toString())
                           .arg(specularColor.isEmpty() ? "0.2" : specularColor[1].toString())
                           .arg(specularColor.isEmpty() ? "0.2" : specularColor[2].toString());
            content += "      ambientIntensity 0.2\n";
            content += QString("      transparency %1\n").arg(transparency);
            content += "      shininess 0.5\n";
            content += "    }\n";
            content += "  }\n";
            content += "  geometry IndexedFaceSet {\n";
            content += "    ccw TRUE\n";
            content += "    solid FALSE\n";
            content += "    coord DEF co Coordinate {\n";
            content += "      point [\n";

            for (const QJsonValue& pointValue : shapePoints) {
                content += "        " + pointValue.toString() + ",\n";
            }

            content += "      ]\n";
            content += "    }\n";
            content += "    coordIndex [\n";

            for (const QJsonValue& indexValue : coordIndex) {
                QJsonArray faceIndices = indexValue.toArray();  ///< 当前面的顶点索引数组。
                QString indexStr;  ///< 当前面的 WRL 索引文本。
                for (const QJsonValue& idx : faceIndices) {
                    indexStr += QString::number(idx.toInt()) + " ";
                }
                content += "        " + indexStr + "\n";
            }

            content += "    ]\n";
            content += "  }\n";
            content += "}\n";
        }
    } else {
        // 如果没有形状数据，使用简单的 Transform 包装
        IR::Model3DVec3 translation = model.translation();  ///< 模型平移参数。
        IR::Model3DVec3 rotation = model.rotation();  ///< 模型旋转参数。

        content += "Transform {\n";
        content += "  translation ";
        content += QString("%1 %2 %3\n").arg(translation.x).arg(translation.y).arg(translation.z);
        content += "  rotation ";
        content += QString("%1 %2 %3 %4\n").arg(rotation.x).arg(rotation.y).arg(rotation.z).arg(1.0);
        content += "  scale 1 1 1\n";
        content += "  children [\n";
        content += "    Shape {\n";
        content += "      appearance Appearance {\n";
        content += "        material Material {\n";
        content += "          diffuseColor 0.8 0.8 0.8\n";
        content += "          specularColor 0.2 0.2 0.2\n";
        content += "          ambientIntensity 0.2\n";
        content += "          shininess 0.2\n";
        content += "        }\n";
        content += "      }\n";
        content += "      geometry IndexedFaceSet {\n";
        content += "        coord Coordinate {\n";
        content += "          point [\n";

        for (const QJsonValue& vertexValue : vertices) {
            QJsonArray vertex = vertexValue.toArray();  ///< 当前顶点的坐标数组。
            if (vertex.size() >= 3) {
                content += QString("          %1 %2 %3\n")
                               .arg(vertex[0].toDouble())
                               .arg(vertex[1].toDouble())
                               .arg(vertex[2].toDouble());
            }
        }

        content += "          ]\n";
        content += "        }\n";
        content += "        coordIndex [\n";

        for (const QJsonValue& faceValue : faces) {
            QJsonArray face = faceValue.toArray();  ///< 当前面的顶点索引数组。
            QString faceStr;  ///< 当前面的 WRL 索引文本。
            for (const QJsonValue& indexValue : face) {
                faceStr += QString::number(indexValue.toInt() - 1) + " ";
            }
            faceStr += "-1";
            content += "          " + faceStr + "\n";
        }

        content += "        ]\n";
        content += "      }\n";
        content += "    }\n";
        content += "  ]\n";
        content += "}\n";
    }

    return content;
}

/**
 * @brief 同步下载并保存指定格式的三维模型。
 * @param uuid 三维模型唯一标识。
 * @param savePath 模型保存路径。
 * @param format 模型格式。
 * @param errorMessage 可选的错误信息输出指针。
 * @return 下载并保存成功时返回 true。
 */
bool Exporter3DModel::downloadModelSync(const QString& uuid,
                                        const QString& savePath,
                                        ModelFormat format,
                                        QString* errorMessage) {
    QByteArray data;  ///< 下载到的三维模型原始数据。
    if (!downloadModelDataSync(uuid, format, &data, errorMessage)) {
        return false;
    }

    QFile file(savePath);
    if (!file.open(QIODevice::WriteOnly)) {
        const QString errorMsg = QString("Failed to open file for writing: %1").arg(savePath);  ///< 文件打开失败信息。
        qWarning() << errorMsg;
        if (errorMessage) {
            *errorMessage = errorMsg;
        }
        return false;
    }

    file.write(data);
    file.close();
    qDebug() << "Exporter3DModel: Successfully downloaded 3D model to" << savePath;
    return true;
}

/**
 * @brief 同步获取三维模型原始数据并执行响应校验。
 * @param uuid 三维模型唯一标识。
 * @param format 模型格式。
 * @param data 原始模型数据输出指针。
 * @param errorMessage 可选的错误信息输出指针。
 * @return 请求成功且数据通过校验时返回 true。
 */
bool Exporter3DModel::downloadModelDataSync(const QString& uuid,
                                            ModelFormat format,
                                            QByteArray* data,
                                            QString* errorMessage) {
    if (errorMessage) {
        errorMessage->clear();
    }
    if (data) {
        data->clear();
    }

    if (uuid.isEmpty()) {
        const QString errorMsg = QStringLiteral("UUID is empty");  ///< UUID 为空时的错误信息。
        qWarning() << errorMsg;
        if (errorMessage) {
            *errorMessage = errorMsg;
        }
        return false;
    }

    m_currentUuid = uuid;

    RetryPolicy policy;  ///< 三维模型请求的重试策略。
    policy.maxRetries = 3;
    policy.baseTimeoutMs = 60000;

    const QString url = getModelUrl(uuid, format);  ///< 三维模型下载地址。
    const ResourceType resourceType =
        format == ModelFormat::OBJ ? ResourceType::Model3DObj : ResourceType::Model3DStep;  ///< 请求资源类型。
    const NetworkResult result = NetworkClient::instance().get(QUrl(url), resourceType, policy);  ///< 网络请求结果。
    if (!result.success) {
        const QString errorMsg =
            QString("Download failed for %1 (%2): %3")
                .arg(uuid, format == ModelFormat::OBJ ? QStringLiteral("OBJ") : QStringLiteral("STEP"), result.error);
        qWarning() << errorMsg;
        if (errorMessage) {
            *errorMessage = errorMsg;
        }
        return false;
    }
    if (data) {
        *data = result.data;
    }
    return true;
}

/**
 * @brief 解析 OBJ 顶点、面、材质和分组信息。
 * @param objData OBJ 模型数据。
 */
QJsonObject Exporter3DModel::parseObjData(const QByteArray& objData) {
    QJsonObject result;  ///< 解析后返回的 OBJ 数据对象。
    QJsonArray vertices;  ///< 解析出的顶点数组。
    QJsonArray faces;  ///< 解析出的面数组。
    QJsonObject materials;  ///< 解析出的材质映射。
    QJsonArray shapes;  ///< 按材质整理的形状数组。

    QString objString = QString::fromUtf8(objData);  ///< OBJ 文本内容。
    QStringList lines = objString.split('\n');  ///< OBJ 文本行列表。

    // 第一遍：提取材质信息
    QString currentMaterialId;  ///< 当前材质 ID。
    QJsonObject currentMaterial;  ///< 当前材质属性对象。
    bool inMaterial = false;  ///< 是否正在读取材质块。

    for (const QString& line : lines) {
        QString trimmedLine = line.trimmed();  ///< 去除空白后的 OBJ 行。
        if (trimmedLine.isEmpty() || trimmedLine.startsWith('#')) {
            continue;
        }

        QStringList parts = trimmedLine.split(' ', Qt::SkipEmptyParts);  ///< 当前 OBJ 行的字段。
        if (parts.isEmpty()) {
            continue;
        }

        if (parts[0] == "newmtl") {
            if (!currentMaterialId.isEmpty() && !currentMaterial.isEmpty()) {
                materials[currentMaterialId] = currentMaterial;
            }
            currentMaterialId = parts[1];
            currentMaterial = QJsonObject();
            inMaterial = true;
        } else if (inMaterial) {
            if (parts[0] == "endmtl") {
                if (!currentMaterialId.isEmpty() && !currentMaterial.isEmpty()) {
                    materials[currentMaterialId] = currentMaterial;
                }
                currentMaterialId.clear();
                currentMaterial = QJsonObject();
                inMaterial = false;
            } else if (parts[0] == "Ka") {
                QJsonArray ambientColor;  ///< 当前材质环境光颜色。
                for (int i = 1; i < parts.size(); ++i) {
                    ambientColor.append(parts[i]);
                }
                currentMaterial["ambientColor"] = ambientColor;
            } else if (parts[0] == "Kd") {
                QJsonArray diffuseColor;  ///< 当前材质漫反射颜色。
                for (int i = 1; i < parts.size(); ++i) {
                    diffuseColor.append(parts[i]);
                }
                currentMaterial["diffuseColor"] = diffuseColor;
            } else if (parts[0] == "Ks") {
                QJsonArray specularColor;  ///< 当前材质高光颜色。
                for (int i = 1; i < parts.size(); ++i) {
                    specularColor.append(parts[i]);
                }
                currentMaterial["specularColor"] = specularColor;
            } else if (parts[0] == "d") {
                currentMaterial["transparency"] = parts[1];
            }
        }
    }

    if (!currentMaterialId.isEmpty() && !currentMaterial.isEmpty()) {
        materials[currentMaterialId] = currentMaterial;
    }

    // 第二遍：提取顶点数据
    QVector<IR::Model3DVec3> convertedVertices;  ///< 换算为毫米并归一化的顶点。
    double minX = std::numeric_limits<double>::max();  ///< 顶点最小 X 坐标。
    double maxX = std::numeric_limits<double>::lowest();  ///< 顶点最大 X 坐标。
    double minY = std::numeric_limits<double>::max();  ///< 顶点最小 Y 坐标。
    double maxY = std::numeric_limits<double>::lowest();  ///< 顶点最大 Y 坐标。
    double minZ = std::numeric_limits<double>::max();  ///< 顶点最小 Z 坐标。
    for (const QString& line : lines) {
        QString trimmedLine = line.trimmed();  ///< 去除空白后的 OBJ 行。
        if (trimmedLine.isEmpty() || trimmedLine.startsWith('#')) {
            continue;
        }

        QStringList parts = trimmedLine.split(' ', Qt::SkipEmptyParts);  ///< 当前 OBJ 行的字段。
        if (parts.isEmpty()) {
            continue;
        }

        if (parts[0] == 'v') {
            if (parts.size() >= 4) {
                double x = parts[1].toDouble() / WRL_UNIT_TO_MM;  ///< 换算后的顶点 X 坐标。
                double y = parts[2].toDouble() / WRL_UNIT_TO_MM;  ///< 换算后的顶点 Y 坐标。
                double z = parts[3].toDouble() / WRL_UNIT_TO_MM;  ///< 换算后的顶点 Z 坐标。
                convertedVertices.append(IR::Model3DVec3(x, y, z));
                minX = qMin(minX, x);
                maxX = qMax(maxX, x);
                minY = qMin(minY, y);
                maxY = qMax(maxY, y);
                minZ = qMin(minZ, z);
            }
        }
    }

    const bool hasVertices = !convertedVertices.isEmpty();  ///< 是否存在可归一化的顶点。
    const double normalizeX = hasVertices ? (minX + maxX) / 2.0 : 0.0;  ///< 顶点中心的 X 偏移。
    const double normalizeY = hasVertices ? (minY + maxY) / 2.0 : 0.0;  ///< 顶点中心的 Y 偏移。
    const double normalizeZ = hasVertices && minZ > 0.0 ? minZ : 0.0;  ///< 模型底面的 Z 偏移。

    QStringList vertexStrings;  ///< 归一化后供形状索引引用的顶点文本。
    for (IR::Model3DVec3 vertexData : convertedVertices) {
        vertexData.x -= normalizeX;
        vertexData.y -= normalizeY;
        vertexData.z -= normalizeZ;

        QString vertexStr =
            QString("%1 %2 %3").arg(vertexData.x, 0, 'f', 4).arg(vertexData.y, 0, 'f', 4).arg(vertexData.z, 0, 'f', 4);
        vertexStrings.append(vertexStr);

        QJsonArray vertex;  ///< 当前顶点的 JSON 坐标数组。
        vertex.append(vertexData.x);
        vertex.append(vertexData.y);
        vertex.append(vertexData.z);
        vertices.append(vertex);
    }

    // 第三遍：按材质分割形状并提取面数
    QString currentShapeMaterial = "default";  ///< 当前形状使用的材质 ID。
    QStringList currentShapePoints;  ///< 当前形状的顶点文本。
    QJsonArray currentShapeCoordIndex;  ///< 当前形状的面索引。
    QMap<int, int> vertexIndexMap;  ///< OBJ 顶点到形状顶点的索引映射。
    int shapeVertexCounter = 0;  ///< 当前形状已生成的顶点数量。

    for (const QString& line : lines) {
        QString trimmedLine = line.trimmed();  ///< 去除空白后的 OBJ 行。
        if (trimmedLine.isEmpty() || trimmedLine.startsWith('#')) {
            continue;
        }

        QStringList parts = trimmedLine.split(' ', Qt::SkipEmptyParts);  ///< 当前 OBJ 行的字段。
        if (parts.isEmpty()) {
            continue;
        }

        if (parts[0] == "usemtl") {
            if (!currentShapePoints.isEmpty()) {
                QJsonObject shape;  ///< 已完成的材质形状对象。
                shape["materialId"] = currentShapeMaterial;
                shape["points"] = QJsonArray::fromStringList(currentShapePoints);
                shape["coordIndex"] = currentShapeCoordIndex;
                shapes.append(shape);
            }
            currentShapeMaterial = parts[1];
            currentShapePoints.clear();
            currentShapeCoordIndex = QJsonArray();
            vertexIndexMap.clear();
            shapeVertexCounter = 0;
        } else if (parts[0] == 'f') {
            QJsonArray faceIndices;  ///< 当前面的局部顶点索引。
            for (int i = 1; i < parts.size(); ++i) {
                QString vertexIndexStr = parts[i].split('/')[0];  ///< 当前面的 OBJ 顶点索引文本。
                int vertexIndex = vertexIndexStr.toInt() - 1;  ///< 转换为零基的 OBJ 顶点索引。

                if (!vertexIndexMap.contains(vertexIndex)) {
                    if (vertexIndex >= 0 && vertexIndex < vertexStrings.size()) {
                        currentShapePoints.append(vertexStrings[vertexIndex]);
                    } else {
                        currentShapePoints.append("0 0 0");
                    }
                    vertexIndexMap[vertexIndex] = shapeVertexCounter;
                    faceIndices.append(shapeVertexCounter);
                    shapeVertexCounter++;
                } else {
                    faceIndices.append(vertexIndexMap[vertexIndex]);
                }
            }
            faceIndices.append(-1);
            currentShapeCoordIndex.append(QJsonArray(faceIndices));
        }
    }

    if (!currentShapePoints.isEmpty()) {
        QJsonObject shape;  ///< 最后一个材质形状对象。
        shape["materialId"] = currentShapeMaterial;
        shape["points"] = QJsonArray::fromStringList(currentShapePoints);
        shape["coordIndex"] = currentShapeCoordIndex;
        shapes.append(shape);
    }

    result["vertices"] = vertices;
    result["faces"] = faces;
    result["materials"] = materials;
    result["shapes"] = shapes;

    return result;
}

/**
 * @brief 根据模型 UUID 和格式生成远程下载地址。
 * @param uuid 稳定对象标识符。
 * @param format 目标模型格式。
 */
QString Exporter3DModel::getModelUrl(const QString& uuid, ModelFormat format) const {
    if (format == ModelFormat::OBJ) {
        return ENDPOINT_3D_MODEL.arg(uuid);
    } else {
        return ENDPOINT_3D_MODEL_STEP.arg(uuid);
    }
}

}  // namespace EasyKiConverter
