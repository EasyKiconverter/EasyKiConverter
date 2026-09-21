#include "FootprintExportStage.h"

#include "FootprintModel3DPreparation.h"
#include "KiCadLibraryTableManager.h"
#include "core/ExporterFactory.h"
#include "core/ir/FootprintDataConverter.h"
#include "models/ComponentData.h"
#include "services/ComponentCacheService.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QMutexLocker>
#include <QSet>
#include <QThread>

namespace EasyKiConverter {

// 初始化封装导出阶段，限制并发以保证库级写入顺序稳定。
FootprintExportStage::FootprintExportStage(QObject* parent)
    // 使用单并发写入，避免同一库文件的并发修改。
    : ExportTypeStage("Footprint", 1, parent) {  // maxConcurrent=1 因为是库级别导出
}

FootprintExportStage::~FootprintExportStage() {
    waitForWorkerThread(m_workerThread, 30000);
}

// 初始化封装导出进度并启动库级导出线程。
void FootprintExportStage::start(const QStringList& componentIds,
                                 const QMap<QString, QSharedPointer<ComponentData>>& cachedData) {
    if (m_isExporting.load()) {
        qWarning() << "FootprintExportStage: Export already in progress";
        return;
    }

    if (componentIds.isEmpty()) {
        qWarning() << "FootprintExportStage: No components to export";
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
        m_progress.typeName = QStringLiteral("Footprint");
        m_progress.totalCount = componentIds.size();
        m_progress.completedCount = 0;
        m_progress.successCount = 0;
        m_progress.failedCount = 0;
        m_progress.skippedCount = 0;
        m_progress.inProgressCount = componentIds.size();
        for (const QString& componentId : componentIds) {
            ExportItemStatus itemStatus;
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

// 请求取消封装导出并回滚尚未提交的临时文件。
void FootprintExportStage::cancel() {
    if (!m_isExporting.load()) {
        return;
    }

    qDebug() << "FootprintExportStage: Cancelling...";

    m_cancelled.store(true);
    m_tempManager.rollbackAll();
    m_isExporting.store(false);

    qDebug() << "FootprintExportStage: Cancelled";
}

// 等待封装导出线程结束，并同步阶段运行状态。
bool FootprintExportStage::waitForFinished(int timeoutMs) {
    const bool finished = waitForWorkerThread(m_workerThread, timeoutMs);
    if (finished) {
        m_isRunning.store(false);
        m_isExporting.store(false);
    }
    return finished;
}

void FootprintExportStage::doLibraryExport(const QStringList& componentIds,
                                           const QMap<QString, QSharedPointer<ComponentData>>& cachedData) {
    qDebug() << "FootprintExportStage: Starting library export in worker thread for" << componentIds.size()
             << "components";
    const uint64_t gen = ComponentCacheService::instance()->currentGeneration();

    QList<FootprintData> footprintList;
    QStringList collectedIds;
    QStringList failedIds;
    int successCount = 0;
    int skippedCount = 0;

    // 统一更新阶段进度，确保库级导出与普通 Worker 阶段提供一致的快照。
    const auto publishItemStatus = [this](const QString& componentId, const ExportItemStatus& status) {
        ExportTypeProgress progressSnapshot;
        {
            QMutexLocker locker(&m_progressMutex);
            auto item = m_progress.itemStatus.find(componentId);
            if (item == m_progress.itemStatus.end())
                return;
            item.value() = status;
            m_progress.completedCount = 0;
            m_progress.successCount = 0;
            m_progress.failedCount = 0;
            m_progress.skippedCount = 0;
            m_progress.inProgressCount = 0;
            for (const ExportItemStatus& itemStatus : std::as_const(m_progress.itemStatus)) {
                if (itemStatus.status == ExportItemStatus::Status::Success) {
                    ++m_progress.successCount;
                } else if (itemStatus.status == ExportItemStatus::Status::Failed) {
                    ++m_progress.failedCount;
                } else if (itemStatus.status == ExportItemStatus::Status::Skipped) {
                    ++m_progress.skippedCount;
                } else if (itemStatus.status == ExportItemStatus::Status::InProgress) {
                    ++m_progress.inProgressCount;
                }
            }
            m_progress.completedCount = m_progress.successCount + m_progress.failedCount + m_progress.skippedCount;
            progressSnapshot = m_progress;
        }
        emit itemStatusChanged(componentId, status);
        emit progressChanged(progressSnapshot);
    };

    // Altium 和 Allegro 将 3D 模型写入封装输出，阶段提前失败时也必须结束对应状态。
    const auto publishEmbeddedModelFailure = [this](const QString& componentId, const QString& errorMessage) {
        if ((m_options.targetFormat != TargetEdaFormat::Altium && m_options.targetFormat != TargetEdaFormat::Allegro) ||
            !m_options.exportModel3D) {
            return;
        }
        ExportItemStatus modelStatus;
        modelStatus.status = ExportItemStatus::Status::Failed;
        modelStatus.errorMessage = errorMessage;
        modelStatus.endTime = QDateTime::currentDateTime();
        emit embeddedModel3DStatusChanged(componentId, modelStatus);
    };

    for (const QString& componentId : componentIds) {
        if (m_cancelled.load()) {
            qDebug() << "FootprintExportStage: Export cancelled during data collection";
            break;
        }

        auto it = cachedData.find(componentId);
        if (it == cachedData.end() || !it.value()) {
            qWarning() << "FootprintExportStage: No data for component:" << componentId;
            failedIds.append(componentId);
            ExportItemStatus status;
            status.status = ExportItemStatus::Status::Failed;
            status.errorMessage = "No component data";
            publishItemStatus(componentId, status);
            publishEmbeddedModelFailure(componentId, QStringLiteral("No component data"));
            continue;
        }

        QSharedPointer<ComponentData> data = it.value();

        if (!data->footprintData()) {
            qWarning() << "FootprintExportStage: No footprint data for component:" << componentId;
            failedIds.append(componentId);
            ExportItemStatus status;
            status.status = ExportItemStatus::Status::Failed;
            status.errorMessage = "No footprint data";
            publishItemStatus(componentId, status);
            publishEmbeddedModelFailure(componentId, QStringLiteral("No footprint data"));
            continue;
        }

        FootprintData footprint = *data->footprintData();
        if (m_options.needsEmbeddedModel3DStep()) {
            FootprintModel3DPreparation::prepare(footprint, data, componentId, gen);
        }

        footprintList.append(footprint);
        collectedIds.append(componentId);
        successCount++;

        ExportItemStatus status;
        status.status = ExportItemStatus::Status::Success;
        status.diagnostics = footprint.validationErrors();
        if (!status.diagnostics.isEmpty())
            qWarning() << "FootprintExportStage: Input diagnostics for" << componentId << status.diagnostics;
        publishItemStatus(componentId, status);

        if ((m_options.targetFormat == TargetEdaFormat::Altium || m_options.targetFormat == TargetEdaFormat::Allegro) &&
            m_options.exportModel3D) {
            ExportItemStatus modelStatus;
            if (!footprint.model3D().step().isEmpty()) {
                modelStatus.status = ExportItemStatus::Status::Success;
            } else {
                modelStatus.status = ExportItemStatus::Status::Failed;
                modelStatus.errorMessage = m_options.targetFormat == TargetEdaFormat::Allegro
                                               ? QStringLiteral("Allegro Import Package 缺少 STEP 3D 模型")
                                               : QStringLiteral("STEP 3D model was not embedded in PcbLib");
            }
            emit embeddedModel3DStatusChanged(componentId, modelStatus);
        }

        qDebug() << "FootprintExportStage: Collected footprint for" << componentId;
    }

    if (m_options.targetFormat == TargetEdaFormat::Altium) {
        // Altium PcbLib 要求封装名称（不区分大小写）唯一。实际 BOM 中多个
        // 元件经常共用同一个封装，例如多个 C0603，但它们仍可能关联不同
        // 的元件数据或三维模型，因此不能简单丢弃重复项。
        QSet<QString> usedNames;
        for (int index = 0; index < footprintList.size(); ++index) {
            FootprintData& footprint = footprintList[index];
            FootprintInfo info = footprint.info();
            const QString baseName = info.name.trimmed();
            QString uniqueName = baseName;
            int suffix = 1;
            while (usedNames.contains(uniqueName.toCaseFolded())) {
                const QString componentId = collectedIds.value(index);
                uniqueName = QStringLiteral("%1_%2").arg(baseName, componentId);
                if (suffix > 1)
                    uniqueName += QStringLiteral("_%1").arg(suffix);
                ++suffix;
            }

            if (uniqueName != info.name) {
                info.name = uniqueName;
                footprint.setInfo(info);
                qWarning() << "FootprintExportStage: Renamed duplicate Altium footprint" << baseName << "to"
                           << uniqueName;
            }
            usedNames.insert(uniqueName.toCaseFolded());
        }
    }

    if (m_cancelled.load()) {
        m_isExporting.store(false);
        m_isRunning.store(false);
        emit completed(0, componentIds.size(), 0);
        return;
    }

    if (footprintList.isEmpty()) {
        qWarning() << "FootprintExportStage: No valid footprints to export";
        m_isExporting.store(false);
        m_isRunning.store(false);
        emit completed(0, componentIds.size(), 0);
        return;
    }

    // 标记所有已收集的封装为失败（参照 SymbolExportStage 的 failCollectedSymbols 模式）
    const auto failCollectedFootprints = [this, &collectedIds, &failedIds, &successCount, &publishItemStatus](
                                             const QString& errorMessage) {
        for (const QString& componentId : collectedIds) {
            failedIds.append(componentId);
            ExportItemStatus status;
            status.status = ExportItemStatus::Status::Failed;
            status.errorMessage = errorMessage;
            status.endTime = QDateTime::currentDateTime();
            publishItemStatus(componentId, status);

            // Altium 的 3D 状态可能已经在收集阶段报告成功，但最终 PcbLib
            // 写入或提交失败时，模型实际上没有进入最终库，必须同步回写失败。
            if ((m_options.targetFormat == TargetEdaFormat::Altium ||
                 m_options.targetFormat == TargetEdaFormat::Allegro) &&
                m_options.exportModel3D) {
                ExportItemStatus modelStatus;
                modelStatus.status = ExportItemStatus::Status::Failed;
                modelStatus.errorMessage = m_options.targetFormat == TargetEdaFormat::Allegro
                                               ? QStringLiteral("Allegro Import Package 导出失败，3D 模型未写入最终包")
                                               : QStringLiteral("Altium PcbLib 导出失败，3D 模型未写入最终库");
                modelStatus.endTime = status.endTime;
                emit embeddedModel3DStatusChanged(componentId, modelStatus);
            }
        }
        successCount = 0;
    };

    // 统一的中止导出 lambda
    const auto abortExport = [&](const QString& errorMessage) {
        qCritical() << "FootprintExportStage:" << errorMessage;
        failCollectedFootprints(errorMessage);
        {
            QMutexLocker locker(&m_progressMutex);
            if (!m_progress.diagnostics.contains(errorMessage))
                m_progress.diagnostics.append(errorMessage);
            const ExportTypeProgress progressSnapshot = m_progress;
            locker.unlock();
            emit progressChanged(progressSnapshot);
        }
        m_tempManager.rollbackAll();
        m_isExporting.store(false);
        m_isRunning.store(false);
        emit completed(0, failedIds.size(), skippedCount);
    };

    QString libName = m_options.libName.isEmpty() ? QStringLiteral("EasyKiConverter") : m_options.libName;
    QString outputDir = m_options.outputPath;
    if (outputDir.isEmpty()) {
        outputDir = QDir::currentPath() + QStringLiteral("/export");
    }

    QDir dir;
    if (!dir.mkpath(outputDir)) {
        abortExport(QStringLiteral("Failed to create output directory: %1").arg(outputDir));
        return;
    }

    // 创建导出器（提前创建以获取格式自描述信息）
    auto exporter = ExporterFactory::createFootprintExporter(m_options.targetFormat);
    if (!exporter) {
        abortExport(QStringLiteral("Failed to create footprint exporter for target format"));
        return;
    }

    const QString fileExt = exporter->libraryFileExtension();
    const bool isDirOutput = exporter->isDirectoryOutput();

    // 根据输出结构类型选择路径
    QString finalPath;
    QString tempPath;
    if (isDirOutput) {
        finalPath = outputDir + QDir::separator() + libName + fileExt;
        tempPath = m_tempManager.createTempDirectoryPath(libName + fileExt);
    } else {
        finalPath = outputDir + QDir::separator() + libName + fileExt;
        tempPath = m_tempManager.createSymbolTempPath(libName, fileExt);
    }

    if (m_options.targetFormat == TargetEdaFormat::Altium && QFile::exists(finalPath) &&
        (!m_options.overwriteExistingFiles || m_options.updateMode)) {
        abortExport(QStringLiteral("Altium PcbLib 暂不支持在已有库上追加或更新，已拒绝覆盖: %1").arg(finalPath));
        return;
    }

    if (m_options.targetFormat == TargetEdaFormat::Xpedition && (m_options.updateMode || m_options.retryMode)) {
        abortExport(QStringLiteral("Xpedition 封装 ZIP 暂不支持更新或重试模式，请选择覆盖导出"));
        return;
    }
    if (m_options.targetFormat == TargetEdaFormat::Xpedition && QFile::exists(finalPath) &&
        !m_options.overwriteExistingFiles) {
        abortExport(QStringLiteral("Xpedition 封装 ZIP 已存在且当前禁止覆盖: %1").arg(finalPath));
        return;
    }

    if (m_options.targetFormat == TargetEdaFormat::Allegro && (m_options.updateMode || m_options.retryMode)) {
        abortExport(QStringLiteral("Allegro Import Package 不支持更新或重试模式，请选择完整覆盖导出"));
        return;
    }
    if (m_options.targetFormat == TargetEdaFormat::Allegro && QFile::exists(finalPath) &&
        !m_options.overwriteExistingFiles) {
        abortExport(QStringLiteral("Allegro Import Package 已存在且当前禁止覆盖: %1").arg(finalPath));
        return;
    }

    if (m_options.targetFormat == TargetEdaFormat::Pads && (m_options.updateMode || m_options.retryMode)) {
        abortExport(QStringLiteral("PADS ASCII 封装库不支持更新或重试模式，请选择完整覆盖导出"));
        return;
    }
    if (m_options.targetFormat == TargetEdaFormat::Pads && QDir(finalPath).exists() &&
        !m_options.overwriteExistingFiles) {
        abortExport(QStringLiteral("PADS ASCII 封装库已存在且当前禁止覆盖: %1").arg(finalPath));
        return;
    }
    if (m_options.targetFormat == TargetEdaFormat::Eagle && (m_options.updateMode || m_options.retryMode)) {
        abortExport(QStringLiteral("Eagle XML 封装库不支持更新或重试模式，请选择完整覆盖导出"));
        return;
    }
    if (m_options.targetFormat == TargetEdaFormat::Eagle && QFile::exists(finalPath) &&
        !m_options.overwriteExistingFiles) {
        abortExport(QStringLiteral("Eagle XML 封装库已存在且当前禁止覆盖: %1").arg(finalPath));
        return;
    }
    if (m_options.targetFormat == TargetEdaFormat::Pcad && (m_options.updateMode || m_options.retryMode)) {
        abortExport(QStringLiteral("P-CAD ASCII 封装库不支持更新或重试模式，请选择完整覆盖导出"));
        return;
    }
    if (m_options.targetFormat == TargetEdaFormat::Pcad && QFile::exists(finalPath) &&
        !m_options.overwriteExistingFiles) {
        abortExport(QStringLiteral("P-CAD ASCII 封装库已存在且当前禁止覆盖: %1").arg(finalPath));
        return;
    }

    if (tempPath.isEmpty()) {
        abortExport(QStringLiteral("Failed to create temp path"));
        return;
    }

    // 目录输出时：追加/更新封装库，先将已有文件复制到临时目录
    if (isDirOutput) {
        const bool preserveExistingFootprints =
            QDir(finalPath).exists() &&
            (!m_options.overwriteExistingFiles || m_options.updateMode || m_options.retryMode);
        if (preserveExistingFootprints) {
            if (!QDir().mkpath(tempPath)) {
                abortExport(QStringLiteral("Failed to create temp dir for merge: %1").arg(tempPath));
                return;
            }
            const QStringList existingFiles = QDir(finalPath).entryList({"*.kicad_mod"}, QDir::Files);
            for (const QString& file : existingFiles) {
                if (!QFile::copy(finalPath + QDir::separator() + file, tempPath + QDir::separator() + file)) {
                    qWarning() << "FootprintExportStage: Failed to copy existing footprint:" << file;
                }
            }
            qDebug() << "FootprintExportStage: Preserved" << existingFiles.size() << "existing footprints";
        }
    }

    qDebug() << "FootprintExportStage: Exporting" << footprintList.size() << "footprints to temp:" << tempPath;
    qDebug() << "FootprintExportStage: fileExt:" << fileExt << "isDirOutput:" << isDirOutput;

    bool exportSuccess = false;
    QString libraryDescription = m_options.footprintLibraryDescription;
    {
        const bool preferWrl = m_options.needsModel3DWrl() && m_options.targetFormat != TargetEdaFormat::Altium;
        const bool exportStep = m_options.needsEmbeddedModel3DStep();
        QString libraryKeywords = m_options.footprintLibraryKeywords;
        // 转换旧类型列表到 IR 类型
        QList<IR::FootprintComponentIR> irFootprintList;
        irFootprintList.reserve(footprintList.size());
        for (const FootprintData& fd : footprintList) {
            irFootprintList.append(IR::toFootprintIR(fd));
        }
        exportSuccess =
            exporter->exportFootprintLibrary(irFootprintList,
                                             libName,
                                             tempPath,
                                             preferWrl,
                                             exportStep,
                                             libraryDescription,
                                             libraryKeywords,
                                             m_options.exportModel3DPathMode == ExportOptions::MODEL_3D_PATH_ABSOLUTE,
                                             outputDir);
        const QStringList exporterDiagnostics = exporter->diagnostics();
        if (!exporterDiagnostics.isEmpty()) {
            QMutexLocker locker(&m_progressMutex);
            for (const QString& diagnostic : exporterDiagnostics) {
                if (!m_progress.diagnostics.contains(diagnostic))
                    m_progress.diagnostics.append(diagnostic);
            }
            const ExportTypeProgress progressSnapshot = m_progress;
            locker.unlock();
            emit progressChanged(progressSnapshot);
        }
        qDebug() << "FootprintExportStage: exportFootprintLibrary result:" << exportSuccess;
    }

    if (m_cancelled.load()) {
        abortExport(QStringLiteral("Export cancelled"));
        return;
    }

    if (!exportSuccess) {
        abortExport(QStringLiteral("Failed to export footprint library"));
        return;
    }

    // 提交临时文件/目录到最终路径
    bool commitSuccess = false;
    if (!isDirOutput) {
        commitSuccess = m_tempManager.commitWithBackup(tempPath, finalPath);
    } else {
        commitSuccess = m_tempManager.commitDirectoryWithBackup(tempPath, finalPath);
    }

    if (commitSuccess) {
        qDebug() << "FootprintExportStage: Successfully exported to:" << finalPath;
    } else {
        abortExport(QStringLiteral("Failed to commit temp path"));
        return;
    }

    // 目录输出模式下注册库（如 KiCad 库表）
    if (isDirOutput && m_options.targetFormat == TargetEdaFormat::KiCad && !libraryDescription.isEmpty()) {
        KiCadLibraryTableManager::registerFootprintLibrary(outputDir, libName, finalPath, libraryDescription);
    }

    // 注意：不在这里清理临时目录，由 ParallelExportService 统一管理

    qDebug() << "FootprintExportStage: Completed. Success:" << successCount << "Failed:" << failedIds.size();

    m_isExporting.store(false);
    m_isRunning.store(false);
    emit completed(successCount, failedIds.size(), skippedCount);
}

}  // namespace EasyKiConverter
