#include "DebugExportHelper.h"

#include "ComponentData.h"
#include "core/utils/AtomicFileWriter.h"

#include <QDebug>
#include <QDir>
#include <QFile>

namespace EasyKiConverter {

namespace {

static const QString kCinfoRawFile = QStringLiteral("cinfo_raw.json");  ///< 原始元器件信息文件名。
static const QString kCadRawFile = QStringLiteral("cad_raw.json");  ///< 原始 CAD 数据文件名。
static const QString kModel3dRawFile = QStringLiteral("model3d_raw.obj");  ///< 原始 OBJ 三维模型文件名。
static const QString kSymbolFile = QStringLiteral("symbol.json");  ///< 规范化符号数据文件名。
static const QString kFootprintFile = QStringLiteral("footprint.json");  ///< 规范化封装数据文件名。
static const QString kComponentInfoFile = QStringLiteral("component_info.json");  ///< 元器件概要信息文件名。

}  // anonymous namespace

/**
 * @brief 将元器件原始数据和规范化数据写入调试目录。
 * @param componentId 元器件编号。
 * @param data 待导出的元器件数据。
 * @param outputPath 调试输出根目录。
 * @return 所有应写入的文件均成功写入时返回 true。
 */
bool DebugExportHelper::exportDebugData(const QString& componentId,
                                        const QSharedPointer<ComponentData>& data,
                                        const QString& outputPath) {
    if (!data) {
        qWarning() << "DebugExportHelper: data is null, skip export for" << componentId;
        return false;
    }

    QString debugDirPath = QStringLiteral("%1/debug/%2").arg(outputPath, componentId);  ///< 元器件调试目录路径。
    if (!AtomicFileWriter::createDirectory(debugDirPath)) {
        qWarning() << "DebugExportHelper: Failed to create debug directory:" << debugDirPath;
        return false;
    }

    qDebug() << "DebugExportHelper: Exporting debug data for" << componentId << "to" << debugDirPath;

    bool success = true;  ///< 当前元器件的调试数据是否全部写入成功。

    if (!data->cinfoJsonRaw().isEmpty()) {
        QString filePath = debugDirPath + QStringLiteral("/") + kCinfoRawFile;  ///< 原始元器件信息目标路径。
        if (!writeAtomically(debugDirPath, filePath, data->cinfoJsonRaw())) {
            success = false;
        }
    }

    if (!data->cadJsonRaw().isEmpty()) {
        QString filePath = debugDirPath + QStringLiteral("/") + kCadRawFile;  ///< 原始 CAD 数据目标路径。
        if (!writeAtomically(debugDirPath, filePath, data->cadJsonRaw())) {
            success = false;
        }
    }

    if (!data->model3DObjRaw().isEmpty()) {
        QString filePath = debugDirPath + QStringLiteral("/") + kModel3dRawFile;  ///< 原始 OBJ 模型目标路径。
        if (!writeAtomically(debugDirPath, filePath, data->model3DObjRaw())) {
            success = false;
        }
    }

    if (data->symbolData()) {
        QJsonObject symbolObj = data->symbolData()->toJson();  ///< 符号数据的 JSON 对象。
        if (!symbolObj.isEmpty()) {
            QString filePath = debugDirPath + QStringLiteral("/") + kSymbolFile;  ///< 符号 JSON 目标路径。
            QJsonDocument doc(symbolObj);  ///< 待写入的格式化符号 JSON 文档。
            if (!writeAtomically(debugDirPath, filePath, doc.toJson(QJsonDocument::Indented))) {
                success = false;
            }
        }
    }

    if (data->footprintData()) {
        QJsonObject footprintObj = data->footprintData()->toJson();  ///< 封装数据的 JSON 对象。
        if (!footprintObj.isEmpty()) {
            QString filePath = debugDirPath + QStringLiteral("/") + kFootprintFile;  ///< 封装 JSON 目标路径。
            QJsonDocument doc(footprintObj);  ///< 待写入的格式化封装 JSON 文档。
            if (!writeAtomically(debugDirPath, filePath, doc.toJson(QJsonDocument::Indented))) {
                success = false;
            }
        }
    }

    QJsonObject componentInfo;  ///< 汇总元器件基本属性和预览图地址。
    componentInfo[QStringLiteral("lcscId")] = data->lcscId();
    componentInfo[QStringLiteral("name")] = data->name();
    componentInfo[QStringLiteral("prefix")] = data->prefix();
    componentInfo[QStringLiteral("package")] = data->package();
    componentInfo[QStringLiteral("manufacturer")] = data->manufacturer();
    componentInfo[QStringLiteral("manufacturerPart")] = data->manufacturerPart();
    if (!data->previewImages().isEmpty()) {
        QJsonArray previewArray;  ///< 预览图地址数组。
        for (const QString& url : data->previewImages()) {  // 预览图资源地址。
            previewArray.append(url);
        }
        componentInfo[QStringLiteral("previewImages")] = previewArray;
    }
    QString filePath = debugDirPath + QStringLiteral("/") + kComponentInfoFile;  ///< 元器件概要 JSON 目标路径。
    QJsonDocument doc(componentInfo);  ///< 待写入的格式化概要 JSON 文档。
    if (!writeAtomically(debugDirPath, filePath, doc.toJson(QJsonDocument::Indented))) {
        success = false;
    }

    return success;
}

/**
 * @brief 使用原子写入方式保存调试文件。
 * @param tempDir 临时文件所在目录。
 * @param filePath 最终文件路径。
 * @param data 待写入的文件内容。
 * @return 文件成功提交时返回 true。
 */
bool DebugExportHelper::writeAtomically(const QString& tempDir, const QString& filePath, const QByteArray& data) {
    return AtomicFileWriter::writeAtomically(tempDir, filePath, QStringLiteral(".tmp"), data);
}

}  // namespace EasyKiConverter
