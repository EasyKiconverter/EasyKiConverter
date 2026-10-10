#include "SymbolExportStage.h"

#include "DebugExportHelper.h"
#include "KiCadLibraryTableManager.h"
#include "core/ExporterFactory.h"
#include "core/horizon/HorizonPoolIntegration.h"
#include "core/ir/SymbolDataConverter.h"
#include "models/ComponentData.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutexLocker>
#include <QThread>
#include <QThreadPool>

namespace EasyKiConverter {

/**
 * @brief 初始化符号导出阶段，限制并发以保证库级写入顺序稳定。
 * @param parent QObject 父对象。
 */
SymbolExportStage::SymbolExportStage(QObject* parent)
    // 使用单并发写入，避免同一库文件的并发修改。
    : ExportTypeStage("Symbol", 1, parent) {  // maxConcurrent=1 因为是库级别导出
}

SymbolExportStage::~SymbolExportStage() {
    waitForWorkerThread(m_workerThread, 30000);
}

/**
 * @brief 初始化符号导出进度并启动库级导出线程。
 * @param componentIds 待导出的元器件编号列表。
 * @param cachedData 已预加载的元器件数据。
 */
void SymbolExportStage::start(const QStringList& componentIds,
                              const QMap<QString, QSharedPointer<ComponentData>>& cachedData) {
    if (m_isExporting.load()) {
        qWarning() << "SymbolExportStage: Export already in progress";
        return;
    }

    if (componentIds.isEmpty()) {
        qWarning() << "SymbolExportStage: No components to export";
        emit completed(0, 0, 0);
        return;
    }

    m_componentIds = componentIds;
    m_cachedData = cachedData;
    m_cancelled.store(false);
    m_isRunning.store(true);

    {
        QMutexLocker locker(&m_progressMutex);
        m_progress = ExportTypeProgress();
        m_progress.typeName = QStringLiteral("Symbol");
        m_progress.totalCount = componentIds.size();
        m_progress.completedCount = 0;
        m_progress.successCount = 0;
        m_progress.failedCount = 0;
        m_progress.skippedCount = 0;
        m_progress.inProgressCount = componentIds.size();
        for (const QString& componentId : componentIds) {  // 当前元器件编号。
            ExportItemStatus itemStatus;  ///< 当前元器件的初始进度状态。
            itemStatus.status = ExportItemStatus::Status::InProgress;
            itemStatus.startTime = QDateTime::currentDateTime();
            m_progress.itemStatus[componentId] = itemStatus;
        }
    }

    m_tempManager.setOutputPath(m_options.outputPath);

    m_isExporting.store(true);
    m_workerThread = QThread::create([this, componentIds, cachedData]() { doLibraryExport(componentIds, cachedData); });
    connect(m_workerThread, &QThread::finished, this, [this]() { m_workerThread = nullptr; });
    m_workerThread->start();
}

/**
 * @brief 请求取消符号导出并回滚尚未提交的临时文件。
 */
void SymbolExportStage::cancel() {
    if (!m_isExporting.load()) {
        return;
    }

    qDebug() << "SymbolExportStage: Cancelling...";

    m_cancelled.store(true);
    m_tempManager.rollbackAll();
    m_isExporting.store(false);

    qDebug() << "SymbolExportStage: Cancelled";
}

/**
 * @brief 等待符号导出线程结束，并同步阶段运行状态。
 * @param timeoutMs 最长等待时间（毫秒）。
 * @return 导出线程已结束时返回 true。
 */
bool SymbolExportStage::waitForFinished(int timeoutMs) {
    const bool finished = waitForWorkerThread(m_workerThread, timeoutMs);  ///< 工作线程是否已在超时前结束。
    if (finished) {
        m_isRunning.store(false);
        m_isExporting.store(false);
    }
    return finished;
}

/**
 * @brief 在工作线程中收集符号、调用导出器并提交库文件。
 * @param componentIds 待导出的元器件编号列表。
 * @param cachedData 已预加载的元器件数据。
 */
void SymbolExportStage::doLibraryExport(const QStringList& componentIds,
                                        const QMap<QString, QSharedPointer<ComponentData>>& cachedData) {
    qDebug() << "SymbolExportStage: Starting library export in worker thread for" << componentIds.size()
             << "components";

    QList<SymbolData> symbolList;  ///< 已收集的符号数据列表。
    QStringList collectedIds;  ///< 已成功收集符号的元器件编号。
    QStringList failedIds;  ///< 收集或导出失败的元器件编号。
    int successCount = 0;  ///< 已成功完成导出的元器件数量。
    int skippedCount = 0;  ///< 被跳过的元器件数量。

    // 库级导出只有一次 writer 调用，必须在提交前把每个组件的最终状态固化到阶段快照。
    const auto finalizeProgress = [this, &componentIds, &collectedIds, &failedIds](bool librarySucceeded,
                                                                                   const QString& errorMessage) {
        ExportTypeProgress progressSnapshot;  ///< 状态收敛后的阶段进度快照。
        {
            QMutexLocker locker(&m_progressMutex);
            for (const QString& componentId : componentIds) {  // 当前元器件编号。
                auto statusIt = m_progress.itemStatus.find(componentId);  ///< 当前元器件状态记录迭代器。
                if (statusIt == m_progress.itemStatus.end() || statusIt->isComplete()) {
                    continue;
                }

                const bool failed =  // 当前元器件是否应标记为失败。
                    failedIds.contains(componentId) || (!librarySucceeded && collectedIds.contains(componentId));
                statusIt->status = failed ? ExportItemStatus::Status::Failed : ExportItemStatus::Status::Success;
                if (failed && !errorMessage.isEmpty()) {
                    statusIt->errorMessage = errorMessage;
                }
                statusIt->endTime = QDateTime::currentDateTime();
                ++m_progress.completedCount;
                if (failed) {
                    ++m_progress.failedCount;
                } else {
                    ++m_progress.successCount;
                }
                m_progress.inProgressCount = qMax(0, m_progress.inProgressCount - 1);
            }
            progressSnapshot = m_progress;
        }
        emit progressChanged(progressSnapshot);
    };

    for (const QString& componentId : componentIds) {  // 当前元器件编号。
        if (m_cancelled.load()) {
            qDebug() << "SymbolExportStage: Export cancelled during data collection";
            break;
        }

        auto it = cachedData.find(componentId);  ///< 当前元器件缓存数据迭代器。
        if (it == cachedData.end() || !it.value()) {
            qWarning() << "SymbolExportStage: No data for component:" << componentId;
            failedIds.append(componentId);
            ExportItemStatus status;  ///< 当前元器件的数据缺失状态。
            status.status = ExportItemStatus::Status::Failed;
            status.errorMessage = "No component data";
            emit itemStatusChanged(componentId, status);
            continue;
        }

        QSharedPointer<ComponentData> data = it.value();  ///< 当前元器件缓存数据。

        if (!data->symbolData()) {
            qWarning() << "SymbolExportStage: No symbol data for component:" << componentId;
            failedIds.append(componentId);
            ExportItemStatus status;  ///< 当前元器件的符号数据缺失状态。
            status.status = ExportItemStatus::Status::Failed;
            status.errorMessage = "No symbol data";
            emit itemStatusChanged(componentId, status);
            continue;
        }

        symbolList.append(*data->symbolData());
        collectedIds.append(componentId);
        successCount++;

        ExportItemStatus status;  ///< 当前元器件收集完成状态。
        status.status = ExportItemStatus::Status::Success;
        status.diagnostics = data->symbolData()->validationErrors();
        if (!status.diagnostics.isEmpty()) {
            qWarning() << "SymbolExportStage: Input diagnostics for" << componentId << status.diagnostics;
        }
        emit itemStatusChanged(componentId, status);

        if (m_options.debugMode) {
            QThreadPool::globalInstance()->start([componentId, data, outputPath = m_options.outputPath]() {
                DebugExportHelper::exportDebugData(componentId, data, outputPath);
            });
        }

        qDebug() << "SymbolExportStage: Collected symbol for" << componentId;
    }

    if (m_cancelled.load()) {
        m_isExporting.store(false);
        m_isRunning.store(false);
        emit completed(0, componentIds.size(), 0);
        return;
    }

    if (symbolList.isEmpty()) {
        qWarning() << "SymbolExportStage: No valid symbols to export";
        m_isExporting.store(false);
        m_isRunning.store(false);
        emit completed(0, componentIds.size(), 0);
        return;
    }

    const auto failCollectedSymbols = [this, &collectedIds, &failedIds, &successCount](const QString& errorMessage) {
        for (const QString& componentId : collectedIds) {  // 当前已收集元器件编号。
            failedIds.append(componentId);
            ExportItemStatus status;  ///< 当前元器件的最终失败状态。
            status.status = ExportItemStatus::Status::Failed;
            status.errorMessage = errorMessage;
            status.endTime = QDateTime::currentDateTime();
            emit itemStatusChanged(componentId, status);
        }
        successCount = 0;
    };

    const auto abortExport = [&](const QString& errorMessage) {
        qCritical() << "SymbolExportStage:" << errorMessage;
        failCollectedSymbols(errorMessage);
        finalizeProgress(false, errorMessage);
        {
            QMutexLocker locker(&m_progressMutex);
            if (!m_progress.diagnostics.contains(errorMessage))
                m_progress.diagnostics.append(errorMessage);
            const ExportTypeProgress progressSnapshot = m_progress;  ///< 诊断后的阶段进度快照。
            locker.unlock();
            emit progressChanged(progressSnapshot);
        }
        m_tempManager.rollbackAll();
        m_isExporting.store(false);
        m_isRunning.store(false);
        emit completed(0, failedIds.size(), 0);
    };

    QString libName =
        m_options.libName.isEmpty() ? QStringLiteral("EasyKiConverter") : m_options.libName;  ///< 输出库名称。

    // 创建导出器（提前创建以获取格式自描述信息）
    auto exporter = ExporterFactory::createSymbolExporter(m_options.targetFormat);  ///< 当前目标格式的符号导出器。
    if (!exporter) {
        abortExport(QStringLiteral("Failed to create symbol exporter for target format"));
        return;
    }

    // 从导出器获取文件扩展名
    const QString fileExt = exporter->libraryFileExtension();  ///< 导出库文件扩展名。
    QString fileName = libName + fileExt;  ///< 导出库文件名。
    QString outputDir = m_options.outputPath;  ///< 用户指定的输出目录。
    if (outputDir.isEmpty()) {
        outputDir = QDir::currentPath() + QStringLiteral("/export");
    }
    QString finalPath = outputDir + QDir::separator() + fileName;  ///< 最终库文件路径。

    QDir dir;  ///< 用于创建输出目录的目录对象。
    if (!dir.mkpath(outputDir)) {
        abortExport(QStringLiteral("Failed to create output directory: %1").arg(outputDir));
        return;
    }

    const bool isDirOutput = exporter->isDirectoryOutput();  ///< 输出是否为目录库。
    const bool finalOutputExists = QFileInfo::exists(finalPath);  ///< 最终库是否已存在。

    if (m_options.targetFormat == TargetEdaFormat::Xpedition && (m_options.updateMode || m_options.retryMode)) {
        abortExport(QStringLiteral("Xpedition 符号 ZIP 暂不支持更新或重试模式，请选择覆盖导出"));
        return;
    }
    if (m_options.targetFormat == TargetEdaFormat::Xpedition && finalOutputExists &&
        !m_options.overwriteExistingFiles) {
        abortExport(QStringLiteral("Xpedition 符号 ZIP 已存在且当前禁止覆盖: %1").arg(finalPath));
        return;
    }

    if (m_options.targetFormat == TargetEdaFormat::Horizon && (m_options.updateMode || m_options.retryMode)) {
        abortExport(QStringLiteral("Horizon 符号 Pool 暂不支持更新或重试模式，请选择覆盖导出"));
        return;
    }
    if (m_options.targetFormat == TargetEdaFormat::Horizon && finalOutputExists && !m_options.overwriteExistingFiles) {
        abortExport(QStringLiteral("Horizon 符号 Pool 已存在且当前禁止覆盖: %1").arg(finalPath));
        return;
    }

    // 路径。
    QString tempPath = isDirOutput ? m_tempManager.createTempDirectoryPath(fileName)
                                   : m_tempManager.createSymbolTempPath(libName, fileExt);  ///< 临时库路径。
    if (tempPath.isEmpty()) {
        abortExport(isDirOutput ? QStringLiteral("Failed to create temp directory path")
                                : QStringLiteral("Failed to create temp file path"));
        return;
    }

    // 追加/更新到现有符号库时，先把最终文件复制到临时文件，再在临时文件上执行 merge。
    if (!isDirOutput && finalOutputExists &&
        (!m_options.overwriteExistingFiles || m_options.updateMode || m_options.retryMode)) {
        const QString tempDirPath = QFileInfo(tempPath).absolutePath();  ///< 临时库文件所在目录。
        if (!QDir().mkpath(tempDirPath)) {
            abortExport(QStringLiteral("Failed to create temp symbol library directory: %1").arg(tempDirPath));
            return;
        }

        QFile tempFile(tempPath);  ///< 临时符号库文件对象。
        if (tempFile.exists() && !tempFile.remove()) {
            abortExport(QStringLiteral("Failed to remove stale temp symbol library: %1").arg(tempPath));
            return;
        }

        QFile sourceFile(finalPath);  ///< 现有符号库文件对象。
        if (!sourceFile.open(QIODevice::ReadOnly)) {
            if (!QFileInfo::exists(finalPath)) {
                qWarning()
                    << "SymbolExportStage: Existing symbol library disappeared before copy, creating new library:"
                    << finalPath;
            } else {
                abortExport(QStringLiteral("Failed to open existing symbol library for copy: %1").arg(finalPath));
                return;
            }
        } else if (!tempFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            abortExport(QStringLiteral("Failed to create temp symbol library for copy: %1").arg(tempPath));
            return;
        } else {
            constexpr qint64 kChunkSize = 65536;  ///< 分块复制的缓冲区大小。
            char buf[kChunkSize];  ///< 符号库分块复制缓冲区。
            bool copyError = false;  ///< 复制过程中是否发生错误。
            while (!sourceFile.atEnd()) {
                qint64 bytesRead = sourceFile.read(buf, kChunkSize);  ///< 本轮读取的字节数。
                if (bytesRead < 0) {
                    abortExport(QStringLiteral("Failed to read existing symbol library: %1").arg(finalPath));
                    copyError = true;
                    break;
                }
                if (tempFile.write(buf, bytesRead) != bytesRead) {
                    abortExport(QStringLiteral("Failed to write temp symbol library copy: %1").arg(tempPath));
                    copyError = true;
                    break;
                }
            }
            if (copyError) {
                return;
            }
            tempFile.close();
        }
    }

    qDebug() << "SymbolExportStage: Exporting" << symbolList.size() << "symbols to temp:" << tempPath;
    qDebug() << "SymbolExportStage: targetFormat:" << static_cast<int>(m_options.targetFormat);

    bool exportSuccess = false;  ///< 目标导出器是否成功生成符号库。
    ISymbolExporter::CompanionFiles companionFiles;  ///< 导出器生成的伴随文件内容。
    QString libraryDescription = m_options.symbolLibraryDescription;  ///< 符号库描述。
    {
        // 只有目标库已经存在且禁止覆盖时才表示追加；首次创建空库不能被误判为追加模式。
        const bool appendMode = finalOutputExists && !m_options.overwriteExistingFiles;  ///< 是否在已有库上追加内容。
        // 转换旧类型列表到 IR 类型
        QList<IR::SymbolComponentIR> irSymbolList;  ///< 转换后的符号 IR 列表。
        irSymbolList.reserve(symbolList.size());
        for (const SymbolData& sd : symbolList) {  // 当前符号数据。
            irSymbolList.append(IR::toSymbolIR(sd));
        }
        exportSuccess = exporter->exportSymbolLibrary(
            irSymbolList, libName, tempPath, appendMode, m_options.updateMode, libraryDescription);
        companionFiles = exporter->companionFiles();
        const QStringList exporterDiagnostics = exporter->diagnostics();  ///< 导出器诊断信息。
        if (!exporterDiagnostics.isEmpty()) {
            QMutexLocker locker(&m_progressMutex);
            for (const QString& diagnostic : exporterDiagnostics) {  // 当前导出诊断。
                if (!m_progress.diagnostics.contains(diagnostic))
                    m_progress.diagnostics.append(diagnostic);
            }
            const ExportTypeProgress progressSnapshot = m_progress;  ///< 诊断后的阶段进度快照。
            locker.unlock();
            emit progressChanged(progressSnapshot);
        }
    }

    if (m_cancelled.load()) {
        m_tempManager.rollbackAll();
        m_isExporting.store(false);
        m_isRunning.store(false);
        emit completed(0, symbolList.size(), 0);
        return;
    }

    if (!exportSuccess) {
        abortExport(QStringLiteral("Failed to export symbol library"));
        return;
    }

    QStringList horizonDiagnostics;  ///< Horizon Pool 更新和注册诊断。
    if (m_options.targetFormat == TargetEdaFormat::Horizon &&
        !HorizonPoolIntegration::updatePool(tempPath, horizonDiagnostics)) {
        abortExport(QStringLiteral("Horizon Pool 官方更新失败，原有库未修改：%1")
                        .arg(horizonDiagnostics.join(QStringLiteral("; "))));
        return;
    }

    QVector<TempFileManager::CommitItem> commitItems;  ///< 主库和伴随文件的提交事务项。
    commitItems.append({tempPath, finalPath, isDirOutput});
    for (auto it = companionFiles.cbegin(); it != companionFiles.cend(); ++it) {  // 当前伴随文件记录。
        if (isDirOutput) {
            abortExport(QStringLiteral("目录型符号库不支持伴随文件: %1").arg(it.key()));
            return;
        }
        const QString companionName = it.key();  ///< 伴随文件名。
        if (companionName.isEmpty() || QFileInfo(companionName).fileName() != companionName ||
            companionName == QFileInfo(finalPath).fileName()) {
            abortExport(QStringLiteral("Invalid symbol companion file name: %1").arg(companionName));
            return;
        }
        const QString companionFinalPath = QDir(outputDir).filePath(companionName);  ///< 伴随文件最终路径。
        // 主库不存在时没有可合并的旧内容，禁止覆盖残留的伴随文件，避免部分库被静默替换。
        if (!finalOutputExists && !m_options.overwriteExistingFiles && QFile::exists(companionFinalPath)) {
            abortExport(QStringLiteral("Symbol companion file already exists and overwrite is disabled: %1")
                            .arg(companionFinalPath));
            return;
        }
        const QString companionTempPath =
            QDir(m_tempManager.tempDirectory()).filePath(companionName);  ///< 伴随文件临时路径。
        QFile companionFile(companionTempPath);  ///< 伴随文件临时写入对象。
        if (!companionFile.open(QIODevice::WriteOnly | QIODevice::Truncate) ||
            companionFile.write(it.value()) != it.value().size()) {
            abortExport(QStringLiteral("Failed to write symbol companion file: %1").arg(companionName));
            return;
        }
        companionFile.close();
        m_tempManager.registerTempFile(companionTempPath);
        commitItems.append({companionTempPath, companionFinalPath, false});
    }
    // 提交临时文件是否成功。
    const bool commitSuccess = isDirOutput ? m_tempManager.commitDirectoryWithBackup(tempPath, finalPath)
                                           : m_tempManager.commitBatch(commitItems);  ///< 库文件是否已提交成功。
    if (!commitSuccess) {
        abortExport(QStringLiteral("Failed to commit symbol library files"));
        return;
    }
    qDebug() << "SymbolExportStage: Successfully exported to:" << finalPath;

    if (m_options.targetFormat == TargetEdaFormat::Horizon) {
        if (!HorizonPoolIntegration::registerPool(finalPath, horizonDiagnostics)) {
            abortExport(QStringLiteral("Horizon Pool 已生成，但官方注册失败：%1")
                            .arg(horizonDiagnostics.join(QStringLiteral("; "))));
            return;
        }
    }

    // KiCad 特有：生成 sym-lib-table 并注册库
    if (m_options.targetFormat == TargetEdaFormat::KiCad && !libraryDescription.isEmpty()) {
        KiCadLibraryTableManager::registerSymbolLibrary(outputDir, libName, finalPath, libraryDescription);
    }

    // 注意：不在这里清理临时目录，由 ParallelExportService 统一管理
    // 避免与其他 Stage（如 FootprintExportStage）的临时文件冲突

    qDebug() << "SymbolExportStage: Completed. Success:" << successCount << "Failed:" << failedIds.size();

    finalizeProgress(true, QString());

    m_isExporting.store(false);
    m_isRunning.store(false);
    emit completed(successCount, failedIds.size(), skippedCount);
}

}  // namespace EasyKiConverter
