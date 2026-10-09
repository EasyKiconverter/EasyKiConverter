#include "WriteWorker.h"

#include "WriteWorkerDebugExporter.h"
#include "core/ir/FootprintDataConverter.h"
#include "core/ir/Model3DDataConverter.h"
#include "core/ir/SymbolDataConverter.h"
#include "core/kicad/Exporter3DModel.h"
#include "core/kicad/ExporterFootprint.h"
#include "core/kicad/ExporterSymbol.h"
#include "core/utils/AtomicFileWriter.h"
#include "models/FootprintDataSerializer.h"
#include "models/SymbolDataSerializer.h"
#include "utils/PathSecurity.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutex>
#include <QMutexLocker>
#include <QRegularExpression>
#include <QThread>
#include <QThreadPool>
#include <QWaitCondition>
#include <QtConcurrent>

namespace EasyKiConverter {

const QRegularExpression INVALID_FILENAME_CHARS("[<>:\"/\\|?*]");

WriteWorker::WriteWorker(QSharedPointer<ComponentExportStatus> status,
                         const QString& outputPath,
                         const QString& libName,
                         bool exportSymbol,
                         bool exportFootprint,
                         bool exportModel3D,
                         bool exportPreviewImages,
                         bool exportDatasheet,
                         bool debugMode,
                         const QString& tempDir,
                         QObject* parent)
    : QObject(parent)
    , m_status(status)
    , m_outputPath(outputPath)
    , m_libName(libName)
    , m_exportSymbol(exportSymbol)
    , m_exportFootprint(exportFootprint)
    , m_exportModel3D(exportModel3D)
    , m_exportPreviewImages(exportPreviewImages)
    , m_exportDatasheet(exportDatasheet)
    , m_debugMode(debugMode)
    , m_tempDir(tempDir)
    , m_isAborted(0) {}

WriteWorker::~WriteWorker() {}

/**
 * @brief 并行执行当前元件的各类文件写入任务并汇总结果。
 */
void WriteWorker::run() {
    if (!m_status) {
        qWarning() << "WriteWorker: null export status, skip run";
        return;
    }

    // 检查是否已被取消 (v3.0.5+)
    if (m_isAborted.loadRelaxed()) {
        m_status->writeSuccess = false;
        m_status->writeMessage = "Export cancelled";
        m_status->isCancelled = true;
        m_status->addDebugLog(QString("WriteWorker cancelled for component: %1").arg(m_status->componentId));
        emit writeCompleted(m_status);
        return;
    }

    // 记录当前元件写入阶段的耗时。
    QElapsedTimer writeTimer;
    writeTimer.start();

    m_status->addDebugLog(QString("WriteWorker started for component: %1").arg(m_status->componentId));

    // 初始化所有写入状态
    m_status->symbolWritten = false;
    m_status->footprintWritten = false;
    m_status->model3DWritten = false;
    m_status->previewImageWritten = false;
    m_status->datasheetWritten = false;

    if (!m_status->fetchSuccess || !m_status->processSuccess) {
        m_status->writeDurationMs = 0;
        m_status->writeSuccess = false;
        m_status->writeMessage = "Skipped writing due to previous stage failure";
        m_status->addDebugLog("Skipping write stage because fetch or process failed.");
        emit writeCompleted(m_status);
        return;
    }

    if ((m_exportSymbol && (!m_status->symbolData || m_status->symbolData->info().name.isEmpty())) &&
        (m_exportFootprint && (!m_status->footprintData || m_status->footprintData->info().name.isEmpty()))) {
        m_status->writeDurationMs = writeTimer.elapsed();
        m_status->writeSuccess = false;
        m_status->writeMessage = "No valid symbol or footprint data to write";
        m_status->addDebugLog("ERROR: Symbol and Footprint data are empty or invalid.");
        emit writeCompleted(m_status);
        return;
    }

    if (!createOutputDirectory(m_outputPath)) {
        m_status->writeDurationMs = writeTimer.elapsed();
        m_status->writeSuccess = false;
        m_status->writeMessage = "Failed to create output directory";
        m_status->addDebugLog(
            QString("ERROR: Failed to create output directory, Duration: %1ms").arg(m_status->writeDurationMs));
        emit writeCompleted(m_status);
        return;
    }

    // 并行写入文件
    // 使用 QtConcurrent::run() + QFutureWatcher 实现并行执行
    // 每项写完后立即发出 itemWriteCompleted 信号，实现实时进度更新
    // 注意：QtConcurrent 不支持工作线程抛出异常，所有 lambda 必须捕获异常
    QList<QFutureWatcher<bool>*> watchers;

    // 符号写入任务
    if (m_exportSymbol && m_status->symbolData) {
        QFuture<bool> future = QtConcurrent::run([this]() {
            try {
                QMutexLocker locker(&m_symbolMutex);
                return writeSymbolFile(*m_status);
            } catch (const std::bad_alloc& e) {
                qCritical() << "Memory allocation failed in symbol write:" << e.what();
                return false;
            } catch (const std::exception& e) {
                qCritical() << "Exception in symbol write:" << e.what();
                return false;
            }
        });
        // 监视符号写入任务的完成状态。
        QFutureWatcher<bool>* watcher = new QFutureWatcher<bool>();
        connect(watcher, &QFutureWatcher<bool>::finished, this, [this, watcher]() {
            // 读取符号写入任务的结果。
            bool result = watcher->result();
            m_status->symbolWritten = result;
            emit itemWriteCompleted(m_status->componentId, static_cast<int>(ExportItemType::Symbol), result);
            // deleteLater removed - watchers are cleaned up by qDeleteAll below
        });
        watcher->setFuture(future);
        watchers.append(watcher);
    }

    // 封装写入任务
    if (m_exportFootprint && m_status->footprintData) {
        QFuture<bool> future = QtConcurrent::run([this]() {
            try {
                QMutexLocker locker(&m_footprintMutex);
                return writeFootprintFile(*m_status);
            } catch (const std::bad_alloc& e) {
                qCritical() << "Memory allocation failed in footprint write:" << e.what();
                return false;
            } catch (const std::exception& e) {
                qCritical() << "Exception in footprint write:" << e.what();
                return false;
            }
        });
        // 监视封装写入任务的完成状态。
        QFutureWatcher<bool>* watcher = new QFutureWatcher<bool>();
        connect(watcher, &QFutureWatcher<bool>::finished, this, [this, watcher]() {
            // 读取封装写入任务的结果。
            bool result = watcher->result();
            m_status->footprintWritten = result;
            emit itemWriteCompleted(m_status->componentId, static_cast<int>(ExportItemType::Footprint), result);
            // deleteLater removed - watchers are cleaned up by qDeleteAll below
        });
        watcher->setFuture(future);
        watchers.append(watcher);
    }

    // 3D模型写入任务
    if (m_exportModel3D && m_status->model3DData &&
        (!m_status->model3DData->rawObj().isEmpty() || !m_status->cachedModel3DWrlPath.isEmpty())) {
        QFuture<bool> future = QtConcurrent::run([this]() {
            try {
                QMutexLocker locker(&m_model3DMutex);
                return write3DModelFile(*m_status);
            } catch (const std::bad_alloc& e) {
                qCritical() << "Memory allocation failed in 3D model write:" << e.what();
                return false;
            } catch (const std::exception& e) {
                qCritical() << "Exception in 3D model write:" << e.what();
                return false;
            }
        });
        // 监视三维模型写入任务的完成状态。
        QFutureWatcher<bool>* watcher = new QFutureWatcher<bool>();
        connect(watcher, &QFutureWatcher<bool>::finished, this, [this, watcher]() {
            // 读取三维模型写入任务的结果。
            bool result = watcher->result();
            m_status->model3DWritten = result;
            emit itemWriteCompleted(m_status->componentId, static_cast<int>(ExportItemType::Model3D), result);
            // deleteLater removed - watchers are cleaned up by qDeleteAll below
        });
        watcher->setFuture(future);
        watchers.append(watcher);
    }

    // 预览图写入任务
    if (m_exportPreviewImages && !m_status->previewImageDataList.isEmpty()) {
        QFuture<bool> future = QtConcurrent::run([this]() {
            try {
                return writePreviewImageFile(*m_status);
            } catch (const std::bad_alloc& e) {
                qCritical() << "Memory allocation failed in preview image write:" << e.what();
                return false;
            } catch (const std::exception& e) {
                qCritical() << "Exception in preview image write:" << e.what();
                return false;
            }
        });
        // 监视预览图写入任务的完成状态。
        QFutureWatcher<bool>* watcher = new QFutureWatcher<bool>();
        connect(watcher, &QFutureWatcher<bool>::finished, this, [this, watcher]() {
            // 读取预览图写入任务的结果。
            bool result = watcher->result();
            m_status->previewImageWritten = result;
            emit itemWriteCompleted(m_status->componentId, static_cast<int>(ExportItemType::PreviewImage), result);
            // deleteLater removed - watchers are cleaned up by qDeleteAll below
        });
        watcher->setFuture(future);
        watchers.append(watcher);
    }

    // 手册写入任务
    if (m_exportDatasheet && !m_status->datasheetData.isEmpty()) {
        QFuture<bool> future = QtConcurrent::run([this]() {
            try {
                return writeDatasheetFile(*m_status);
            } catch (const std::bad_alloc& e) {
                qCritical() << "Memory allocation failed in datasheet write:" << e.what();
                return false;
            } catch (const std::exception& e) {
                qCritical() << "Exception in datasheet write:" << e.what();
                return false;
            }
        });
        // 监视数据手册写入任务的完成状态。
        QFutureWatcher<bool>* watcher = new QFutureWatcher<bool>();
        connect(watcher, &QFutureWatcher<bool>::finished, this, [this, watcher]() {
            // 读取数据手册写入任务的结果。
            bool result = watcher->result();
            m_status->datasheetWritten = result;
            emit itemWriteCompleted(m_status->componentId, static_cast<int>(ExportItemType::Datasheet), result);
            // deleteLater removed - watchers are cleaned up by qDeleteAll below
        });
        watcher->setFuture(future);
        watchers.append(watcher);
    }

    // 等待所有写入任务完成
    for (QFutureWatcher<bool>* watcher : watchers) {
        watcher->waitForFinished();
    }

    // 清理 watchers
    qDeleteAll(watchers);
    watchers.clear();

    // 检查是否被取消
    if (m_isAborted.loadRelaxed()) {
        m_status->writeDurationMs = writeTimer.elapsed();
        m_status->writeSuccess = false;
        m_status->writeMessage = "Export cancelled";
        m_status->isCancelled = true;
        m_status->addDebugLog(QString("WriteWorker cancelled for component: %1").arg(m_status->componentId));
        emit writeCompleted(m_status);
        return;
    }

    // 根据用户请求的导出项和实际完成情况，决定最终的成功状态
    // 计算符号写入是否满足用户请求。
    bool symbolSuccess = (!m_exportSymbol) || m_status->symbolWritten;
    // 计算封装写入是否满足用户请求。
    bool footprintSuccess = (!m_exportFootprint) || m_status->footprintWritten;
    // 计算三维模型写入是否满足用户请求。
    bool model3DSuccess = (!m_exportModel3D) || m_status->model3DWritten;
    // 计算预览图写入是否满足用户请求。
    bool previewImageSuccess = (!m_exportPreviewImages) || m_status->previewImageWritten;
    // 计算数据手册写入是否满足用户请求。
    bool datasheetSuccess = (!m_exportDatasheet) || m_status->datasheetWritten;

    // 汇总所有已请求导出项的成功状态。
    bool overallSuccess =
        symbolSuccess && footprintSuccess && model3DSuccess && previewImageSuccess && datasheetSuccess;

    // 收集成功和失败的导出项缩写，供诊断日志使用。
    QStringList successItems;
    // 保存失败导出项的缩写。
    QStringList failedItems;
    if (m_exportSymbol) {
        symbolSuccess ? successItems.append("S") : failedItems.append("S");
    }
    if (m_exportFootprint) {
        footprintSuccess ? successItems.append("F") : failedItems.append("F");
    }
    if (m_exportModel3D) {
        model3DSuccess ? successItems.append("3D") : failedItems.append("3D");
    }
    if (m_exportPreviewImages) {
        previewImageSuccess ? successItems.append("P") : failedItems.append("P");
    }
    if (m_exportDatasheet) {
        datasheetSuccess ? successItems.append("D") : failedItems.append("D");
    }

    m_status->writeSuccess = overallSuccess;
    if (overallSuccess) {
        m_status->writeMessage = "Write completed successfully";
    } else {
        m_status->writeMessage = QString("Export completed with failures: %1").arg(failedItems.join(", "));
    }

    m_status->addDebugLog(QString("WriteWorker completed - Symbol:%1 Footprint:%2 3D:%3 Preview:%4 Datasheet:%5")
                              .arg(symbolSuccess ? "OK" : "FAIL")
                              .arg(footprintSuccess ? "OK" : "FAIL")
                              .arg(model3DSuccess ? "OK" : "FAIL")
                              .arg(previewImageSuccess ? "OK" : "FAIL")
                              .arg(datasheetSuccess ? "OK" : "FAIL"));

    if (m_debugMode) {
        exportDebugData(*m_status);
    }

    m_status->writeDurationMs = writeTimer.elapsed();
    m_status->addDebugLog(QString("WriteWorker completed for component: %1, Success: %2, Duration: %3ms")
                              .arg(m_status->componentId)
                              .arg(m_status->writeSuccess)
                              .arg(m_status->writeDurationMs));

    emit writeCompleted(m_status);
}

/**
 * @brief 将符号 IR 原子写入临时目录中的 KiCad 符号文件。
 * @param status 当前元件的导出状态。
 * @return 写入成功时返回 true，否则返回 false。
 */
bool WriteWorker::writeSymbolFile(ComponentExportStatus& status) {
    if (!status.symbolData) {
        return true;
    }

    if (!QDir(m_tempDir).exists()) {
        status.addDebugLog(QString("ERROR: Temp directory does not exist: %1").arg(m_tempDir));
        return false;
    }
    if (!PathSecurity::isSafePath(m_tempDir, m_outputPath)) {
        status.addDebugLog(QString("SECURITY ERROR: Temp dir outside output path: %1").arg(m_tempDir));
        return false;
    }

    // 构造符号文件的临时输出路径。
    QString finalFilePath = QString("%1/%2.kicad_sym").arg(m_tempDir, status.componentId);

    // 通过临时文件写入并原子替换目标符号文件。
    bool success = AtomicFileWriter::writeAtomically(
        m_tempDir, finalFilePath, ".kicad_sym.tmp", [this, &status](const QString& tempPath) -> bool {
            // 将组件模型转换为符号 IR。
            IR::SymbolComponentIR symbolIR = IR::toSymbolIR(*status.symbolData);
            return m_symbolExporter.exportSymbol(symbolIR, tempPath);
        });

    if (success) {
        status.addDebugLog(QString("Symbol file written atomically: %1").arg(finalFilePath));
        status.symbolWritten = true;
        return true;
    } else {
        status.addDebugLog(QString("ERROR: Failed to write symbol file"));
        return false;
    }
}

/**
 * @brief 将封装 IR 和关联模型路径原子写入 KiCad 封装库。
 * @param status 当前元件的导出状态。
 * @return 写入成功时返回 true，否则返回 false。
 */
bool WriteWorker::writeFootprintFile(ComponentExportStatus& status) {
    if (!status.footprintData) {
        return true;
    }

    if (!QDir(m_tempDir).exists()) {
        status.addDebugLog(QString("ERROR: Temp directory does not exist: %1").arg(m_tempDir));
        return false;
    }
    if (!PathSecurity::isSafePath(m_tempDir, m_outputPath)) {
        status.addDebugLog(QString("SECURITY ERROR: Temp dir outside output path: %1").arg(m_tempDir));
        return false;
    }

    // 构造 KiCad 封装库目录路径。
    QString footprintLibPath = QString("%1/%2.pretty").arg(m_outputPath, m_libName);
    if (!createOutputDirectory(footprintLibPath)) {
        status.addDebugLog(QString("ERROR: Failed to create footprint library directory: %1").arg(footprintLibPath));
        return false;
    }

    // 构造三维模型输出目录路径。
    QString modelsDirPath = QString("%1/%2.3dmodels").arg(m_outputPath, m_libName);
    if (m_exportModel3D) {
        if (!createOutputDirectory(modelsDirPath)) {
            status.addDebugLog(QString("WARNING: Failed to create 3D models directory: %1").arg(modelsDirPath));
        }
    }

    // 生成安全的封装名称和目标文件路径。
    QString footprintName = PathSecurity::sanitizeFilename(status.footprintData->info().name);
    // 构造封装文件的完整输出路径。
    QString filePath = QString("%1/%2.kicad_mod").arg(footprintLibPath, footprintName);

    // 保存封装文件中引用的 WRL 和 STEP 相对路径。
    QString model3DWrlPath;
    // 保存封装中引用的 STEP 模型相对路径。
    QString model3DStepPath;
    if (m_exportModel3D && status.model3DData && !status.model3DData->uuid().isEmpty()) {
        model3DWrlPath = QString("../%1.3dmodels/%2.wrl").arg(m_libName, footprintName);
        // 检查是否有STEP数据（无论是原始数据还是缓存路径）
        if (!status.model3DStepRaw.isEmpty() || !status.cachedModel3DStepPath.isEmpty()) {
            model3DStepPath = QString("../%1.3dmodels/%2.step").arg(m_libName, footprintName);
        }
    }

    // 通过临时文件写入并原子替换目标封装文件。
    bool success = AtomicFileWriter::writeAtomically(
        m_tempDir,
        filePath,
        ".kicad_mod.tmp",
        [this, &status, &model3DWrlPath, &model3DStepPath](const QString& tempPath) -> bool {
            // 将组件模型转换为封装 IR。
            IR::FootprintComponentIR footprintIR = IR::toFootprintIR(*status.footprintData);
            if (!model3DStepPath.isEmpty()) {
                return m_footprintExporter.exportFootprint(footprintIR, tempPath, model3DWrlPath, model3DStepPath);
            } else if (!model3DWrlPath.isEmpty()) {
                return m_footprintExporter.exportFootprint(footprintIR, tempPath, model3DWrlPath);
            } else {
                return m_footprintExporter.exportFootprint(footprintIR, tempPath);
            }
        });

    if (success) {
        status.addDebugLog(QString("Footprint file written atomically: %1").arg(filePath));
        status.footprintWritten = true;
        return true;
    } else {
        status.addDebugLog(QString("ERROR: Failed to write footprint file"));
        return false;
    }
}

/**
 * @brief 写入或复制当前元件的 WRL 与 STEP 三维模型文件。
 * @param status 当前元件的导出状态。
 * @return 至少一个请求的三维模型文件成功写入时返回 true。
 */
bool WriteWorker::write3DModelFile(ComponentExportStatus& status) {
    // 如果既没有原始数据也没有缓存路径，则跳过
    const bool hasWrlData =
        status.model3DData && (!status.model3DData->rawObj().isEmpty() || !status.cachedModel3DWrlPath.isEmpty());
    // 判断是否存在可写入的 STEP 模型数据。
    const bool hasStepData = !status.model3DStepRaw.isEmpty() || !status.cachedModel3DStepPath.isEmpty();
    if (!hasWrlData && !hasStepData) {
        return true;
    }

    if (!QDir(m_tempDir).exists()) {
        status.addDebugLog(QString("ERROR: Temp directory does not exist: %1").arg(m_tempDir));
        return false;
    }
    if (!PathSecurity::isSafePath(m_tempDir, m_outputPath)) {
        status.addDebugLog(
            QString("SECURITY ERROR: Temp dir %1 is outside output path %2").arg(m_tempDir, m_outputPath));
        return false;
    }

    // 构造三维模型输出目录路径。
    QString modelsDirPath = QString("%1/%2.3dmodels").arg(m_outputPath, m_libName);
    if (!createOutputDirectory(modelsDirPath)) {
        status.addDebugLog(QString("ERROR: Failed to create 3D models directory: %1").arg(modelsDirPath));
        return false;
    }

    // 使用封装名称作为模型文件名，缺少封装时回退到元器件编号。
    QString footprintName = status.footprintData ? status.footprintData->info().name : status.componentId;
    footprintName = PathSecurity::sanitizeFilename(footprintName);

    // 记录 WRL 模型写入结果。
    bool wrlSuccess = false;

    // WRL 是 OBJ 派生文件；如果有 OBJ 原始数据，始终重新生成，避免旧 WRL 缓存携带过期坐标归一化。
    if (status.model3DData && !status.model3DData->rawObj().isEmpty()) {
        // 构造 WRL 模型目标路径。
        QString wrlFilePath = QString("%1/%2.wrl").arg(modelsDirPath, footprintName);
        wrlSuccess = AtomicFileWriter::writeAtomically(
            m_tempDir, wrlFilePath, ".wrl.tmp", [this, &status](const QString& tempPath) -> bool {
                // 将模型数据转换为统一的三维模型 IR。
                IR::Model3DIR modelIR = IR::toModel3DIR(*status.model3DData);
                return m_model3DExporter.exportToWrl(modelIR, tempPath);
            });
        if (wrlSuccess) {
            status.addDebugLog(QString("3D model WRL file written atomically: %1").arg(wrlFilePath));
        } else {
            status.addDebugLog(QString("ERROR: Failed to write WRL file"));
        }
    } else if (hasWrlData && !status.cachedModel3DWrlPath.isEmpty()) {
        // 构造缓存 WRL 模型的目标路径。
        QString wrlFilePath = QString("%1/%2.wrl").arg(modelsDirPath, footprintName);
        wrlSuccess = AtomicFileWriter::copyAtomically(status.cachedModel3DWrlPath, wrlFilePath, m_tempDir);
        if (wrlSuccess) {
            status.addDebugLog(QString("3D model WRL file copied from cache: %1").arg(status.cachedModel3DWrlPath));
        } else {
            status.addDebugLog(QString("ERROR: Failed to copy WRL file from cache"));
        }
    }

    // STEP 文件保持服务器原始坐标系，避免破坏它与 WRL 模型原本一致的装配关系。
    // 记录 STEP 模型写入结果。
    bool stepSuccess = false;
    if (!status.cachedModel3DStepPath.isEmpty()) {
        // 构造缓存 STEP 模型的目标路径。
        QString stepFilePath = QString("%1/%2.step").arg(modelsDirPath, footprintName);
        stepSuccess = AtomicFileWriter::copyAtomically(status.cachedModel3DStepPath, stepFilePath, m_tempDir);
        if (stepSuccess) {
            status.addDebugLog(QString("3D model STEP file copied from cache: %1").arg(status.cachedModel3DStepPath));
        } else {
            status.addDebugLog(QString("WARNING: Failed to copy STEP file from cache: %1").arg(stepFilePath));
        }
    } else if (!status.model3DStepRaw.isEmpty()) {
        // 构造原始 STEP 模型的目标路径。
        QString stepFilePath = QString("%1/%2.step").arg(modelsDirPath, footprintName);
        stepSuccess = AtomicFileWriter::writeAtomically(
            m_tempDir, stepFilePath, ".step.tmp", [&status](const QString& tempPath) -> bool {
                // 打开原子写入阶段使用的临时 STEP 文件。
                QFile file(tempPath);
                if (!file.open(QIODevice::WriteOnly)) {
                    return false;
                }
                // 记录实际写入的 STEP 字节数。
                qint64 written = file.write(status.model3DStepRaw);
                file.close();
                return written == status.model3DStepRaw.size();
            });

        if (stepSuccess) {
            status.addDebugLog(QString("3D model STEP file written"));
        } else {
            status.addDebugLog(QString("WARNING: Failed to write STEP file: %1").arg(stepFilePath));
        }
        status.clearStepData();
    }

    status.model3DWritten = wrlSuccess || stepSuccess;
    return status.model3DWritten;
}

/**
 * @brief 确保指定输出目录存在。
 * @param path 待创建或检查的目录路径。
 * @return 目录存在或创建成功时返回 true，否则返回 false。
 */
bool WriteWorker::createOutputDirectory(const QString& path) {
    // 使用目录对象检查并创建输出目录。
    QDir dir;
    if (!dir.exists(path)) {
        if (!dir.mkpath(path)) {
            qWarning() << "Failed to create directory:" << path;
            return false;
        }
    }
    return true;
}

/**
 * @brief 将调试数据委托给独立的调试导出协作者。
 * @param status 当前元件的导出状态。
 * @return 调试数据导出成功时返回 true，否则返回 false。
 */
bool WriteWorker::exportDebugData(ComponentExportStatus& status) {
    return WriteWorkerDebugExporter(*this).exportData(status);
}

/**
 * @brief 将预览图数据写入 images 目录。
 * @param status 当前元件的导出状态。
 * @return 所有非空预览图写入成功时返回 true，否则返回 false。
 */
bool WriteWorker::writePreviewImageFile(ComponentExportStatus& status) {
    // 定位当前导出的根目录。
    QDir outputDir(m_outputPath);
    // 构造预览图输出目录路径。
    QString imagesDir = outputDir.filePath("images");
    if (!createOutputDirectory(imagesDir)) {
        status.addDebugLog(QString("ERROR: Failed to create images directory: %1").arg(imagesDir));
        return false;
    }

    // 将元器件编号转换为安全文件名。
    QString safeName = status.componentId;
    safeName.replace(INVALID_FILENAME_CHARS, "_");

    // 记录所有预览图是否均写入成功。
    bool allSuccess = true;
    // 统计实际写入的预览图数量。
    int exportedCount = 0;

    for (int i = 0; i < status.previewImageDataList.size(); ++i) {
        if (status.previewImageDataList[i].isEmpty()) {
            continue;
        }

        // 构造当前预览图的文件名。
        QString filename = QString("%1_%2.jpg").arg(safeName).arg(i);
        // 构造当前预览图的完整输出路径。
        QString filePath = QDir(imagesDir).filePath(filename);

        QFile file(filePath);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(status.previewImageDataList[i]);
            file.close();
            exportedCount++;
        } else {
            allSuccess = false;
            status.addDebugLog(QString("ERROR: Failed to write preview image: %1").arg(filePath));
        }
    }

    if (exportedCount > 0) {
        status.addDebugLog(QString("Preview images written: %1 files to %2").arg(exportedCount).arg(imagesDir));
    }
    status.previewImageWritten = allSuccess;
    return allSuccess;
}

/**
 * @brief 根据内容格式将数据手册写入 datasheets 目录。
 * @param status 当前元件的导出状态。
 * @return 数据手册写入成功时返回 true，否则返回 false。
 */
bool WriteWorker::writeDatasheetFile(ComponentExportStatus& status) {
    if (status.datasheetData.isEmpty()) {
        return true;
    }

    // 定位当前导出的根目录。
    QDir outputDir(m_outputPath);
    // 构造数据手册输出目录路径。
    QString datasheetsDir = outputDir.filePath("datasheets");
    if (!createOutputDirectory(datasheetsDir)) {
        status.addDebugLog(QString("ERROR: Failed to create datasheets directory: %1").arg(datasheetsDir));
        return false;
    }

    // 将元器件编号转换为安全文件名。
    QString safeName = status.componentId;
    safeName.replace(INVALID_FILENAME_CHARS, "_");

    // 根据数据内容判断格式
    // 默认按 PDF 保存，检测到 HTML 时切换扩展名。
    QString extension = "pdf";
    if (status.datasheetData.startsWith("<!DOCTYPE html") || status.datasheetData.startsWith("<html")) {
        extension = "html";
    }

    // 构造数据手册文件名。
    QString filename = QString("%1.%2").arg(safeName).arg(extension);
    // 构造数据手册的完整输出路径。
    QString filePath = QDir(datasheetsDir).filePath(filename);

    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(status.datasheetData);
        file.close();
        status.addDebugLog(QString("Datasheet written: %1").arg(filePath));
        status.datasheetWritten = true;
        return true;
    }

    status.addDebugLog(QString("ERROR: Failed to write datasheet: %1").arg(filePath));
    return false;
}

/**
 * @brief 设置取消标志并记录当前元件的取消请求。
 */
void WriteWorker::abort() {
    m_isAborted.storeRelaxed(1);
    m_status->addDebugLog(QString("WriteWorker abort requested for component: %1").arg(m_status->componentId));
}

}  // namespace EasyKiConverter
