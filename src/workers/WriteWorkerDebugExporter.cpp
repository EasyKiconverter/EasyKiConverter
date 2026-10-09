#include "WriteWorkerDebugExporter.h"

#include "WriteWorker.h"
#include "models/FootprintDataSerializer.h"
#include "models/SymbolDataSerializer.h"

#include <QDebug>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace EasyKiConverter {

/** @brief 保存所属写入工作线程的引用。 */
WriteWorkerDebugExporter::WriteWorkerDebugExporter(WriteWorker& owner) : m_owner(owner) {}

/**
 * @brief 写入原始响应、模型数据和结构化调试摘要。
 * @param status 当前元器件的导出状态和调试数据。
 * @return 调试目录及摘要文件写入流程启动成功时返回 true。
 */
bool WriteWorkerDebugExporter::exportData(ComponentExportStatus& status) {
    QString debugDirPath = QString("%1/debug").arg(m_owner.m_outputPath);  // 调试输出根目录。
    if (!m_owner.createOutputDirectory(debugDirPath)) {
        status.addDebugLog(QString("ERROR: Failed to create debug directory: %1").arg(debugDirPath));
        return false;
    }

    QString componentDebugDir = QString("%1/%2").arg(debugDirPath, status.componentId);  // 当前元器件调试目录。
    if (!m_owner.createOutputDirectory(componentDebugDir)) {
        status.addDebugLog(QString("ERROR: Failed to create component debug directory: %1").arg(componentDebugDir));
        return false;
    }

    status.addDebugLog(QString("Exporting debug data to: %1").arg(componentDebugDir));

    if (!status.cinfoJsonRaw.isEmpty()) {
        QString cinfoFilePath = QString("%1/cinfo_raw.json").arg(componentDebugDir);  // 原始元器件信息文件路径。
        QFile cinfoFile(cinfoFilePath);  // 原始元器件信息输出文件。
        if (cinfoFile.open(QIODevice::WriteOnly)) {
            cinfoFile.write(status.cinfoJsonRaw);
            cinfoFile.close();
            status.addDebugLog("Debug: cinfo_raw.json written");
        }
    }

    if (!status.cadJsonRaw.isEmpty()) {
        QString cadFilePath = QString("%1/cad_raw.json").arg(componentDebugDir);  // 原始 CAD 数据文件路径。
        QFile cadFile(cadFilePath);  // 原始 CAD 数据输出文件。
        if (cadFile.open(QIODevice::WriteOnly)) {
            cadFile.write(status.cadJsonRaw);
            cadFile.close();
            status.addDebugLog("Debug: cad_raw.json written");
        }
    }

    if (!status.advJsonRaw.isEmpty()) {
        QString advFilePath = QString("%1/adv_raw.json").arg(componentDebugDir);  // 原始高级数据文件路径。
        QFile advFile(advFilePath);  // 原始高级数据输出文件。
        if (advFile.open(QIODevice::WriteOnly)) {
            advFile.write(status.advJsonRaw);
            advFile.close();
            qDebug() << "Debug: adv_raw.json written";
        }
    }

    if (!status.model3DObjRaw.isEmpty()) {
        QString objFilePath = QString("%1/model3d_raw.obj").arg(componentDebugDir);  // 原始 OBJ 文件路径。
        QFile objFile(objFilePath);  // 原始 OBJ 输出文件。
        if (objFile.open(QIODevice::WriteOnly)) {
            objFile.write(status.model3DObjRaw);
            objFile.close();
            status.addDebugLog("Debug: model3d_raw.obj written");
        }
    }

    if (!status.model3DStepRaw.isEmpty()) {
        QString stepFilePath = QString("%1/model3d_raw.step").arg(componentDebugDir);  // 原始 STEP 文件路径。
        QFile stepFile(stepFilePath);  // 原始 STEP 输出文件。
        if (stepFile.open(QIODevice::WriteOnly)) {
            stepFile.write(status.model3DStepRaw);
            stepFile.close();
            status.addDebugLog("Debug: model3d_raw.step written");
        }
    }

    QJsonObject debugInfo;  // 当前元器件的结构化调试摘要。
    debugInfo["componentId"] = status.componentId;
    debugInfo["fetchSuccess"] = status.fetchSuccess;
    debugInfo["fetchMessage"] = status.fetchMessage;
    debugInfo["processSuccess"] = status.processSuccess;
    debugInfo["processMessage"] = status.processMessage;
    debugInfo["writeSuccess"] = status.writeSuccess;
    debugInfo["writeMessage"] = status.writeMessage;

    if (!status.debugLog.isEmpty()) {
        QJsonArray logArray;  // 调试日志 JSON 数组。
        for (const QString& log : status.debugLog) {
            logArray.append(log);
        }
        debugInfo["debugLog"] = logArray;
    }

    if (status.symbolData) {
        QJsonObject symbolInfo = SymbolDataSerializer::toJson(status.symbolData->info());  // 符号摘要对象。
        symbolInfo["pinCount"] = status.symbolData->pins().size();
        symbolInfo["rectangleCount"] = status.symbolData->rectangles().size();
        symbolInfo["circleCount"] = status.symbolData->circles().size();
        symbolInfo["arcCount"] = status.symbolData->arcs().size();
        symbolInfo["polylineCount"] = status.symbolData->polylines().size();
        symbolInfo["polygonCount"] = status.symbolData->polygons().size();
        symbolInfo["pathCount"] = status.symbolData->paths().size();
        symbolInfo["ellipseCount"] = status.symbolData->ellipses().size();

        QJsonObject bbox;  // 符号边界框对象。
        bbox["x"] = status.symbolData->bbox().x;
        bbox["y"] = status.symbolData->bbox().y;
        bbox["width"] = status.symbolData->bbox().width;
        bbox["height"] = status.symbolData->bbox().height;
        symbolInfo["bbox"] = bbox;

        QJsonArray pinsArray;  // 符号引脚 JSON 数组。
        for (const SymbolPin& pin : status.symbolData->pins()) {
            pinsArray.append(SymbolDataSerializer::toJson(pin));
        }
        symbolInfo["pins"] = pinsArray;

        QJsonArray rectanglesArray;  // 符号矩形 JSON 数组。
        for (const SymbolRectangle& rect : status.symbolData->rectangles()) {
            rectanglesArray.append(SymbolDataSerializer::toJson(rect));
        }
        symbolInfo["rectangles"] = rectanglesArray;

        QJsonArray circlesArray;  // 符号圆形 JSON 数组。
        for (const SymbolCircle& circle : status.symbolData->circles()) {
            circlesArray.append(SymbolDataSerializer::toJson(circle));
        }
        symbolInfo["circles"] = circlesArray;

        QJsonArray arcsArray;  // 符号圆弧 JSON 数组。
        for (const SymbolArc& arc : status.symbolData->arcs()) {
            arcsArray.append(SymbolDataSerializer::toJson(arc));
        }
        symbolInfo["arcs"] = arcsArray;

        QJsonArray polylinesArray;  // 符号折线 JSON 数组。
        for (const SymbolPolyline& polyline : status.symbolData->polylines()) {
            polylinesArray.append(SymbolDataSerializer::toJson(polyline));
        }
        symbolInfo["polylines"] = polylinesArray;

        QJsonArray polygonsArray;  // 符号多边形 JSON 数组。
        for (const SymbolPolygon& polygon : status.symbolData->polygons()) {
            polygonsArray.append(SymbolDataSerializer::toJson(polygon));
        }
        symbolInfo["polygons"] = polygonsArray;

        QJsonArray pathsArray;  // 符号路径 JSON 数组。
        for (const SymbolPath& path : status.symbolData->paths()) {
            pathsArray.append(SymbolDataSerializer::toJson(path));
        }
        symbolInfo["paths"] = pathsArray;

        QJsonArray ellipsesArray;  // 符号椭圆 JSON 数组。
        for (const SymbolEllipse& ellipse : status.symbolData->ellipses()) {
            ellipsesArray.append(SymbolDataSerializer::toJson(ellipse));
        }
        symbolInfo["ellipses"] = ellipsesArray;

        debugInfo["symbolData"] = symbolInfo;
    }

    if (status.footprintData) {
        QJsonObject footprintInfo = FootprintDataSerializer::toJson(status.footprintData->info());  // 封装摘要对象。
        footprintInfo["padCount"] = status.footprintData->pads().size();
        footprintInfo["trackCount"] = status.footprintData->tracks().size();
        footprintInfo["holeCount"] = status.footprintData->holes().size();
        footprintInfo["circleCount"] = status.footprintData->circles().size();
        footprintInfo["arcCount"] = status.footprintData->arcs().size();
        footprintInfo["rectangleCount"] = status.footprintData->rectangles().size();
        footprintInfo["textCount"] = status.footprintData->texts().size();
        footprintInfo["solidRegionCount"] = status.footprintData->solidRegions().size();
        footprintInfo["outlineCount"] = status.footprintData->outlines().size();

        QJsonObject bbox;  // 封装边界框对象。
        bbox["x"] = status.footprintData->bbox().x;
        bbox["y"] = status.footprintData->bbox().y;
        bbox["width"] = status.footprintData->bbox().width;
        bbox["height"] = status.footprintData->bbox().height;
        footprintInfo["bbox"] = bbox;

        QJsonArray padsArray;  // 封装焊盘 JSON 数组。
        for (const FootprintPad& pad : status.footprintData->pads()) {
            padsArray.append(FootprintDataSerializer::toJson(pad));
        }
        footprintInfo["pads"] = padsArray;

        QJsonArray tracksArray;  // 封装走线 JSON 数组。
        for (const FootprintTrack& track : status.footprintData->tracks()) {
            tracksArray.append(FootprintDataSerializer::toJson(track));
        }
        footprintInfo["tracks"] = tracksArray;

        QJsonArray holesArray;  // 封装孔 JSON 数组。
        for (const FootprintHole& hole : status.footprintData->holes()) {
            holesArray.append(FootprintDataSerializer::toJson(hole));
        }
        footprintInfo["holes"] = holesArray;

        QJsonArray circlesArray;  // 封装圆形 JSON 数组。
        for (const FootprintCircle& circle : status.footprintData->circles()) {
            circlesArray.append(FootprintDataSerializer::toJson(circle));
        }
        footprintInfo["circles"] = circlesArray;

        QJsonArray arcsArray;  // 封装圆弧 JSON 数组。
        for (const FootprintArc& arc : status.footprintData->arcs()) {
            arcsArray.append(FootprintDataSerializer::toJson(arc));
        }
        footprintInfo["arcs"] = arcsArray;

        QJsonArray rectanglesArray;  // 封装矩形 JSON 数组。
        for (const FootprintRectangle& rect : status.footprintData->rectangles()) {
            rectanglesArray.append(FootprintDataSerializer::toJson(rect));
        }
        footprintInfo["rectangles"] = rectanglesArray;

        QJsonArray textsArray;  // 封装文本 JSON 数组。
        for (const FootprintText& text : status.footprintData->texts()) {
            textsArray.append(FootprintDataSerializer::toJson(text));
        }
        footprintInfo["texts"] = textsArray;

        QJsonArray solidRegionsArray;  // 封装实心区域 JSON 数组。
        for (const FootprintSolidRegion& region : status.footprintData->solidRegions()) {
            solidRegionsArray.append(FootprintDataSerializer::toJson(region));
        }
        footprintInfo["solidRegions"] = solidRegionsArray;

        QJsonArray outlinesArray;  // 封装外形 JSON 数组。
        for (const FootprintOutline& outline : status.footprintData->outlines()) {
            outlinesArray.append(FootprintDataSerializer::toJson(outline));
        }
        footprintInfo["outlines"] = outlinesArray;

        debugInfo["footprintData"] = footprintInfo;
    }

    if (status.model3DData) {
        QJsonObject model3DInfo;  // 三维模型摘要对象。
        model3DInfo["uuid"] = status.model3DData->uuid();
        model3DInfo["objSize"] = status.model3DObjRaw.size();
        model3DInfo["stepSize"] = status.model3DStepRaw.size();
        debugInfo["model3DData"] = model3DInfo;
    }

    QString debugInfoFilePath =  // 结构化调试摘要文件路径。
        QString("%1/%2_debug_info.json").arg(componentDebugDir, status.componentId);
    QFile debugInfoFile(debugInfoFilePath);  // 结构化调试摘要输出文件。
    if (debugInfoFile.open(QIODevice::WriteOnly)) {
        QJsonDocument debugDoc(debugInfo);  // 待写入的调试摘要文档。
        debugInfoFile.write(debugDoc.toJson(QJsonDocument::Indented));
        debugInfoFile.close();
        status.addDebugLog("Debug: debug_info.json written");
    }

    status.addDebugLog(QString("Debug data export completed for component: %1").arg(status.componentId));
    return true;
}

}  // namespace EasyKiConverter
