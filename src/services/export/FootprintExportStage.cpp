#include "FootprintExportStage.h"

#include "FootprintModel3DPreparation.h"
#include "KiCadLibraryTableManager.h"
#include "core/ExporterFactory.h"
#include "core/horizon/HorizonPoolIntegration.h"
#include "core/interfaces/ISymbolExporter.h"
#include "core/ir/ComponentDataConverter.h"
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

namespace {

/**
 * @brief 查找输出路径所属的 LibrePCB 项目根目录。
 * @param outputPath 输出路径。
 * @return LibrePCB 项目根目录，未找到时返回空字符串。
 */
QString librePcbProjectRoot(const QString& outputPath) {
    QFileInfo outputInfo(outputPath);  ///< 输出路径信息。
    QString candidate =
        outputInfo.isFile() ? outputInfo.absolutePath() : outputInfo.absoluteFilePath();  ///< 当前搜索路径。
    candidate = QDir::cleanPath(candidate);
    for (int level = 0; level < 2; ++level) {  // 当前向上搜索层级。
        const QFileInfo marker(
            QDir(candidate).filePath(QStringLiteral(".librepcb-project")));  ///< LibrePCB 项目标记文件。
        if (marker.isFile())
            return marker.absolutePath();
        if (QFileInfo(candidate).fileName() != QStringLiteral("library"))
            break;
        candidate = QDir(candidate).filePath(QStringLiteral(".."));
        candidate = QDir::cleanPath(candidate);
    }
    return QString();
}

/**
 * @brief 从 LibrePCB 项目根目录向上查找工作区根目录。
 * @param projectRoot LibrePCB 项目根目录。
 * @return 工作区根目录，未找到时返回空字符串。
 */
QString librePcbWorkspaceRoot(const QString& projectRoot) {
    QString candidate = QDir::cleanPath(projectRoot);  ///< 当前搜索路径。
    for (int level = 0; level < 6; ++level) {  // 当前向上搜索层级。
        const QDir workspace(candidate);  ///< 当前候选工作区目录。
        const QString librariesPath = workspace.filePath(QStringLiteral("data/libraries"));  ///< 工作区库目录路径。
        const QString projectsPath = workspace.filePath(QStringLiteral("projects"));  ///< 工作区项目目录路径。
        if (QFileInfo(librariesPath).isDir() && QFileInfo(projectsPath).isDir())
            return candidate;

        const QString parent = QDir(candidate).filePath(QStringLiteral(".."));  ///< 候选路径的父目录。
        const QString normalizedParent = QDir::cleanPath(parent);  ///< 规范化后的父目录路径。
        if (normalizedParent == candidate)
            break;
        candidate = normalizedParent;
    }
    return QString();
}

/**
 * @brief 根据库名称生成工作区本地库目录路径。
 * @param workspaceRoot LibrePCB 工作区根目录。
 * @param libraryName 库名称。
 * @return 工作区本地库目录路径。
 */
QString librePcbWorkspaceLibraryPath(const QString& workspaceRoot, const QString& libraryName) {
    QString safeName = QFileInfo(libraryName.trimmed()).fileName();  ///< 去除路径部分后的安全库名。
    if (safeName.isEmpty() || safeName == QStringLiteral(".") || safeName == QStringLiteral(".."))
        safeName = QStringLiteral("EasyKiConverter");
    if (!safeName.endsWith(QStringLiteral(".lplib"), Qt::CaseInsensitive))
        safeName += QStringLiteral(".lplib");
    return QDir(workspaceRoot).filePath(QStringLiteral("data/libraries/local/%1").arg(safeName));
}

/**
 * @brief 递归复制目录树并拒绝复制符号链接。
 * @param sourcePath 源目录路径。
 * @param targetPath 目标目录路径。
 * @return 复制成功时返回 true。
 */
bool copyDirectoryTree(const QString& sourcePath, const QString& targetPath) {
    const QDir source(sourcePath);  ///< 源目录对象。
    if (!source.exists())
        return true;
    if (!QDir().mkpath(targetPath))
        return false;

    const QFileInfoList entries =  // 源目录中的所有可见及隐藏目录项。
        source.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries | QDir::Hidden | QDir::System);
    for (const QFileInfo& entry : entries) {  // 当前目录项。
        if (entry.isSymLink())
            return false;

        const QString target = QDir(targetPath).filePath(entry.fileName());  ///< 当前目录项的目标路径。
        if (entry.isDir()) {
            if (!copyDirectoryTree(entry.absoluteFilePath(), target))
                return false;
        } else {
            if (QFile::exists(target) && !QFile::remove(target))
                return false;
            if (!QFile::copy(entry.absoluteFilePath(), target))
                return false;
        }
    }
    return true;
}

/**
 * @brief 将新生成的 LibrePCB 库内容合并到项目库临时目录。
 * @param generatedPath 新生成的库目录。
 * @param projectLibraryPath 项目已有库目录。
 * @param mergedPath 合并后的临时目录。
 * @return 合并成功时返回 true。
 */
bool mergeLibrePcbLibrary(const QString& generatedPath, const QString& projectLibraryPath, const QString& mergedPath) {
    if (!copyDirectoryTree(projectLibraryPath, mergedPath))
        return false;

    for (const QString& kind :  // 当前 LibrePCB 库元素目录类型。
         {QStringLiteral("cmp"), QStringLiteral("dev"), QStringLiteral("pkg"), QStringLiteral("sym")}) {
        const QString source = QDir(generatedPath).filePath(kind);  ///< 新生成元素目录。
        const QString target = QDir(mergedPath).filePath(kind);  ///< 合并目标元素目录。
        if (!copyDirectoryTree(source, target))
            return false;
    }
    return true;
}

}  // namespace

/**
 * @brief 初始化封装导出阶段，限制并发以保证库级写入顺序稳定。
 * @param parent QObject 父对象。
 */
FootprintExportStage::FootprintExportStage(QObject* parent)
    // 使用单并发写入，避免同一库文件的并发修改。
    : ExportTypeStage("Footprint", 1, parent) {  // maxConcurrent=1 因为是库级别导出
}

FootprintExportStage::~FootprintExportStage() {
    waitForWorkerThread(m_workerThread, 30000);
}

/**
 * @brief 初始化封装导出进度并启动库级导出线程。
 * @param componentIds 待导出的元器件编号列表。
 * @param cachedData 已预加载的元器件数据。
 */
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
 * @brief 请求取消封装导出并回滚尚未提交的临时文件。
 */
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

/**
 * @brief 等待封装导出线程结束，并同步阶段运行状态。
 * @param timeoutMs 最长等待时间（毫秒）。
 * @return 导出线程已结束时返回 true。
 */
bool FootprintExportStage::waitForFinished(int timeoutMs) {
    const bool finished = waitForWorkerThread(m_workerThread, timeoutMs);  ///< 工作线程是否已在超时前结束。
    if (finished) {
        m_isRunning.store(false);
        m_isExporting.store(false);
    }
    return finished;
}

/**
 * @brief 在工作线程中收集数据、调用库导出器并提交输出。
 * @param componentIds 待导出的元器件编号列表。
 * @param cachedData 已预加载的元器件数据。
 */
void FootprintExportStage::doLibraryExport(const QStringList& componentIds,
                                           const QMap<QString, QSharedPointer<ComponentData>>& cachedData) {
    qDebug() << "FootprintExportStage: Starting library export in worker thread for" << componentIds.size()
             << "components";
    const uint64_t gen = ComponentCacheService::instance()->currentGeneration();  ///< 当前缓存代次。

    QList<FootprintData> footprintList;  ///< 已收集的封装数据列表。
    QList<IR::ComponentIR> componentIrList;  ///< 已收集的组合器件 IR 列表。
    QList<IR::SymbolComponentIR> symbolList;  ///< 已收集的符号 IR 列表。
    QStringList collectedIds;  ///< 已成功收集数据的元器件编号。
    QStringList failedIds;  ///< 收集或导出失败的元器件编号。
    int successCount = 0;  ///< 已成功完成导出的元器件数量。
    int skippedCount = 0;  ///< 被跳过的元器件数量。

    // 统一更新阶段进度，确保库级导出与普通 Worker 阶段提供一致的快照。
    const auto publishItemStatus = [this](const QString& componentId, const ExportItemStatus& status) {
        ExportTypeProgress progressSnapshot;  ///< 状态发布时的阶段进度快照。
        {
            QMutexLocker locker(&m_progressMutex);
            auto item = m_progress.itemStatus.find(componentId);  ///< 当前元器件状态记录迭代器。
            if (item == m_progress.itemStatus.end())
                return;
            item.value() = status;
            m_progress.completedCount = 0;
            m_progress.successCount = 0;
            m_progress.failedCount = 0;
            m_progress.skippedCount = 0;
            m_progress.inProgressCount = 0;
            for (const ExportItemStatus& itemStatus : std::as_const(m_progress.itemStatus)) {  // 当前状态记录。
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

    // Altium、Allegro、LibrePCB 和 Horizon 将 3D 模型写入封装输出，阶段提前失败时也必须结束对应状态。
    const auto publishEmbeddedModelFailure = [this](const QString& componentId, const QString& errorMessage) {
        if ((m_options.targetFormat != TargetEdaFormat::Altium && m_options.targetFormat != TargetEdaFormat::Allegro &&
             m_options.targetFormat != TargetEdaFormat::LibrePcb &&
             m_options.targetFormat != TargetEdaFormat::Horizon) ||
            !m_options.exportModel3D) {
            return;
        }
        ExportItemStatus modelStatus;  ///< 嵌入式三维模型的失败状态。
        modelStatus.status = ExportItemStatus::Status::Failed;
        modelStatus.errorMessage = errorMessage;
        modelStatus.endTime = QDateTime::currentDateTime();
        emit embeddedModel3DStatusChanged(componentId, modelStatus);
    };

    for (const QString& componentId : componentIds) {  // 当前元器件编号。
        if (m_cancelled.load()) {
            qDebug() << "FootprintExportStage: Export cancelled during data collection";
            break;
        }

        auto it = cachedData.find(componentId);  ///< 当前元器件缓存数据迭代器。
        if (it == cachedData.end() || !it.value()) {
            qWarning() << "FootprintExportStage: No data for component:" << componentId;
            failedIds.append(componentId);
            ExportItemStatus status;  ///< 当前元器件的数据缺失状态。
            status.status = ExportItemStatus::Status::Failed;
            status.errorMessage = "No component data";
            publishItemStatus(componentId, status);
            publishEmbeddedModelFailure(componentId, QStringLiteral("No component data"));
            continue;
        }

        QSharedPointer<ComponentData> data = it.value();  ///< 当前元器件缓存数据。

        const bool combinedTarget =  // 是否为需要组合符号和封装输出的目标格式。
            m_options.targetFormat == TargetEdaFormat::Eagle || m_options.targetFormat == TargetEdaFormat::Cadstar ||
            m_options.targetFormat == TargetEdaFormat::Allegro || m_options.targetFormat == TargetEdaFormat::LibrePcb ||
            m_options.targetFormat == TargetEdaFormat::Horizon;
        const bool needsSymbol = combinedTarget && m_options.exportSymbol;  ///< 当前任务是否需要符号数据。
        const bool needsFootprint =
            m_options.exportFootprint || m_options.exportModel3D;  ///< 当前任务是否需要封装数据。
        if (needsSymbol && !data->symbolData()) {
            qWarning() << "FootprintExportStage: No symbol data for component:" << componentId;
            failedIds.append(componentId);
            ExportItemStatus status;  ///< 当前元器件的封装数据缺失状态。
            status.status = ExportItemStatus::Status::Failed;
            status.errorMessage = "No symbol data";
            publishItemStatus(componentId, status);
            publishEmbeddedModelFailure(componentId, QStringLiteral("No symbol data"));
            continue;
        }
        if (needsFootprint && !data->footprintData()) {
            qWarning() << "FootprintExportStage: No footprint data for component:" << componentId;
            failedIds.append(componentId);
            ExportItemStatus status;  ///< 当前元器件的封装数据缺失状态。
            status.status = ExportItemStatus::Status::Failed;
            status.errorMessage = "No footprint data";
            publishItemStatus(componentId, status);
            publishEmbeddedModelFailure(componentId, QStringLiteral("No footprint data"));
            continue;
        }

        FootprintData footprint;  ///< 当前元器件将要导出的封装副本。
        if (data->footprintData()) {
            footprint = *data->footprintData();
        }
        if (needsFootprint && m_options.needsEmbeddedModel3DStep()) {
            FootprintModel3DPreparation::prepare(footprint, data, componentId, gen);
        }

        if (data->footprintData())
            footprintList.append(footprint);
        if (combinedTarget && data->symbolData()) {
            // 组合库必须使用完成 STEP 准备后的封装副本，否则模型准备结果会在重新转换组件时丢失。
            IR::ComponentIR componentIr = IR::toComponentIR(*data);  ///< 当前元器件的组合 IR。
            if (data->footprintData())
                componentIr.footprint = IR::toFootprintIR(footprint);
            componentIrList.append(componentIr);
            symbolList.append(IR::toSymbolIR(*data->symbolData()));
        }
        collectedIds.append(componentId);
        successCount++;

        ExportItemStatus status;  ///< 当前元器件收集完成状态。
        status.status = ExportItemStatus::Status::Success;
        if (data->symbolData())
            status.diagnostics.append(data->symbolData()->validationErrors());
        if (data->footprintData())
            status.diagnostics.append(footprint.validationErrors());
        if (!status.diagnostics.isEmpty())
            qWarning() << "FootprintExportStage: Input diagnostics for" << componentId << status.diagnostics;
        publishItemStatus(componentId, status);

        if ((m_options.targetFormat == TargetEdaFormat::Altium || m_options.targetFormat == TargetEdaFormat::Allegro ||
             m_options.targetFormat == TargetEdaFormat::LibrePcb ||
             m_options.targetFormat == TargetEdaFormat::Horizon) &&
            m_options.exportModel3D && data->footprintData()) {
            ExportItemStatus modelStatus;  ///< 当前元器件的嵌入式模型状态。
            if (!footprint.model3D().step().isEmpty()) {
                modelStatus.status = ExportItemStatus::Status::Success;
            } else {
                modelStatus.status = ExportItemStatus::Status::Failed;
                modelStatus.errorMessage = m_options.targetFormat == TargetEdaFormat::Allegro
                                               ? QStringLiteral("Allegro Import Package 缺少 STEP 3D 模型")
                                           : m_options.targetFormat == TargetEdaFormat::LibrePcb
                                               ? QStringLiteral("LibrePCB 库缺少 STEP 3D 模型")
                                           : m_options.targetFormat == TargetEdaFormat::Horizon
                                               ? QStringLiteral("Horizon Pool 缺少 STEP 3D 模型")
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
        QSet<QString> usedNames;  ///< 已使用的 Altium 封装名称集合。
        for (int index = 0; index < footprintList.size(); ++index) {  // 当前封装索引。
            FootprintData& footprint = footprintList[index];  ///< 当前待处理的封装。
            FootprintInfo info = footprint.info();  ///< 当前封装基本信息。
            const QString baseName = info.name.trimmed();  ///< 当前封装的原始名称。
            QString uniqueName = baseName;  ///< 当前封装的候选唯一名称。
            int suffix = 1;  ///< 名称冲突时使用的后缀序号。
            while (usedNames.contains(uniqueName.toCaseFolded())) {
                const QString componentId = collectedIds.value(index);  ///< 用于区分重名封装的元器件编号。
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

    if (footprintList.isEmpty() && componentIrList.isEmpty() && symbolList.isEmpty()) {
        qWarning() << "FootprintExportStage: No valid library data to export";
        m_isExporting.store(false);
        m_isRunning.store(false);
        emit completed(0, componentIds.size(), 0);
        return;
    }

    // 标记所有已收集的封装为失败（参照 SymbolExportStage 的 failCollectedSymbols 模式）
    const auto failCollectedFootprints = [this, &collectedIds, &failedIds, &successCount, &publishItemStatus](
                                             const QString& errorMessage) {
        for (const QString& componentId : collectedIds) {  // 当前已收集元器件编号。
            failedIds.append(componentId);
            ExportItemStatus status;  ///< 当前元器件的最终失败状态。
            status.status = ExportItemStatus::Status::Failed;
            status.errorMessage = errorMessage;
            status.endTime = QDateTime::currentDateTime();
            publishItemStatus(componentId, status);

            // Altium 的 3D 状态可能已经在收集阶段报告成功，但最终 PcbLib
            // 写入或提交失败时，模型实际上没有进入最终库，必须同步回写失败。
            if ((m_options.targetFormat == TargetEdaFormat::Altium ||
                 m_options.targetFormat == TargetEdaFormat::Allegro ||
                 (m_options.targetFormat == TargetEdaFormat::LibrePcb ||
                  m_options.targetFormat == TargetEdaFormat::Horizon)) &&
                m_options.exportModel3D) {
                ExportItemStatus modelStatus;  ///< 当前元器件嵌入式模型的最终失败状态。
                modelStatus.status = ExportItemStatus::Status::Failed;
                modelStatus.errorMessage = m_options.targetFormat == TargetEdaFormat::Allegro
                                               ? QStringLiteral("Allegro Import Package 导出失败，3D 模型未写入最终包")
                                           : m_options.targetFormat == TargetEdaFormat::LibrePcb
                                               ? QStringLiteral("LibrePCB 导出失败，3D 模型未写入最终库")
                                           : m_options.targetFormat == TargetEdaFormat::Horizon
                                               ? QStringLiteral("Horizon Pool 导出失败，3D 模型未写入最终库")
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
            const ExportTypeProgress progressSnapshot = m_progress;  ///< 记录诊断后的阶段进度快照。
            locker.unlock();
            emit progressChanged(progressSnapshot);
        }
        m_tempManager.rollbackAll();
        m_isExporting.store(false);
        m_isRunning.store(false);
        emit completed(0, failedIds.size(), skippedCount);
    };

    QString libName =
        m_options.libName.isEmpty() ? QStringLiteral("EasyKiConverter") : m_options.libName;  ///< 输出库名称。
    QString outputDir = m_options.outputPath;  ///< 用户指定的输出目录。
    if (outputDir.isEmpty()) {
        outputDir = QDir::currentPath() + QStringLiteral("/export");
    }

    const QString librePcbRoot =  // 检测到的 LibrePCB 项目根目录。
        m_options.targetFormat == TargetEdaFormat::LibrePcb ? librePcbProjectRoot(outputDir) : QString();
    const bool exportIntoLibrePcbProject = !librePcbRoot.isEmpty();  ///< 是否直接安装到 LibrePCB 项目。
    const QString librePcbWorkspace =
        exportIntoLibrePcbProject ? librePcbWorkspaceRoot(librePcbRoot) : QString();  ///< LibrePCB 工作区根目录。
    const QString librePcbWorkspaceLibrary =  // 工作区本地库目标路径。
        !librePcbWorkspace.isEmpty() ? librePcbWorkspaceLibraryPath(librePcbWorkspace, libName) : QString();
    const QString librePcbProjectLibrary =  // 项目本地库目标路径。
        exportIntoLibrePcbProject ? QDir(librePcbRoot).filePath(QStringLiteral("library")) : QString();
    const QString effectiveOutputDir =
        exportIntoLibrePcbProject ? librePcbProjectLibrary : outputDir;  ///< 实际生成库的目录。
    if (exportIntoLibrePcbProject)
        m_tempManager.setOutputPath(librePcbRoot);

    QDir dir;  ///< 用于创建输出目录的目录对象。
    if (!dir.mkpath(effectiveOutputDir)) {
        abortExport(QStringLiteral("Failed to create output directory: %1").arg(outputDir));
        return;
    }

    // 创建导出器（提前创建以获取格式自描述信息）
    auto exporter = ExporterFactory::createFootprintExporter(m_options.targetFormat);  ///< 当前目标格式的封装导出器。
    if (!exporter) {
        abortExport(QStringLiteral("Failed to create footprint exporter for target format"));
        return;
    }

    const QString fileExt = exporter->libraryFileExtension();  ///< 导出库文件扩展名。
    const bool isDirOutput = exporter->isDirectoryOutput();  ///< 输出是否为目录库。

    // 根据输出结构类型选择路径
    QString finalPath;  ///< 最终库路径。
    QString tempPath;  ///< 临时库路径。
    if (isDirOutput) {
        finalPath =
            exportIntoLibrePcbProject ? librePcbProjectLibrary : outputDir + QDir::separator() + libName + fileExt;
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
    if (m_options.targetFormat == TargetEdaFormat::Cadstar && (m_options.updateMode || m_options.retryMode)) {
        abortExport(QStringLiteral("CADSTAR ASCII 库不支持更新或重试模式，请选择完整覆盖导出"));
        return;
    }
    if (m_options.targetFormat == TargetEdaFormat::Cadstar && QFile::exists(finalPath) &&
        !m_options.overwriteExistingFiles) {
        abortExport(QStringLiteral("CADSTAR ASCII 库已存在且当前禁止覆盖: %1").arg(finalPath));
        return;
    }
    if (m_options.targetFormat == TargetEdaFormat::LibrePcb && (m_options.updateMode || m_options.retryMode)) {
        abortExport(QStringLiteral("LibrePCB 原生库暂不支持更新或重试模式，请选择完整覆盖导出"));
        return;
    }
    if (m_options.targetFormat == TargetEdaFormat::LibrePcb && QDir(finalPath).exists() &&
        !m_options.overwriteExistingFiles) {
        abortExport(QStringLiteral("LibrePCB 原生库已存在且当前禁止覆盖: %1").arg(finalPath));
        return;
    }
    if (exportIntoLibrePcbProject && !librePcbWorkspaceLibrary.isEmpty() && QDir(librePcbWorkspaceLibrary).exists() &&
        !m_options.overwriteExistingFiles) {
        abortExport(QStringLiteral("LibrePCB 工作区库已存在且当前禁止覆盖: %1").arg(librePcbWorkspaceLibrary));
        return;
    }
    if (m_options.targetFormat == TargetEdaFormat::Horizon && QDir(finalPath).exists() &&
        !m_options.overwriteExistingFiles) {
        qInfo() << "FootprintExportStage: Horizon append/update will preserve existing Pool entries:" << finalPath;
    }

    if (tempPath.isEmpty()) {
        abortExport(QStringLiteral("Failed to create temp path"));
        return;
    }

    // 目录输出时：追加/更新封装库，先将已有文件复制到临时目录
    if (isDirOutput) {
        if (m_options.targetFormat == TargetEdaFormat::Horizon && QDir(finalPath).exists() &&
            (!m_options.overwriteExistingFiles || m_options.updateMode || m_options.retryMode)) {
            if (!copyDirectoryTree(finalPath, tempPath)) {
                abortExport(
                    QStringLiteral("Horizon Pool 现有内容无法安全复制到临时目录，已保留原库: %1").arg(finalPath));
                return;
            }
        }
        const bool preserveExistingFootprints =
            QDir(finalPath).exists() &&
            (!m_options.overwriteExistingFiles || m_options.updateMode || m_options.retryMode);
        if (preserveExistingFootprints) {
            if (!QDir().mkpath(tempPath)) {
                abortExport(QStringLiteral("Failed to create temp dir for merge: %1").arg(tempPath));
                return;
            }
            if (m_options.targetFormat != TargetEdaFormat::Horizon) {
                const QStringList existingFiles =
                    QDir(finalPath).entryList({"*.kicad_mod"}, QDir::Files);  ///< 已存在的 KiCad 封装文件名。
                for (const QString& file : existingFiles) {  // 当前既有封装文件名。
                    if (!QFile::copy(finalPath + QDir::separator() + file, tempPath + QDir::separator() + file)) {
                        qWarning() << "FootprintExportStage: Failed to copy existing footprint:" << file;
                    }
                }
                qDebug() << "FootprintExportStage: Preserved" << existingFiles.size() << "existing footprints";
            }
        }
    }

    qDebug() << "FootprintExportStage: Exporting" << footprintList.size() << "footprints to temp:" << tempPath;
    qDebug() << "FootprintExportStage: fileExt:" << fileExt << "isDirOutput:" << isDirOutput;

    bool exportSuccess = false;  ///< 目标导出器是否成功生成库内容。
    QStringList exporterDiagnostics;  ///< 导出器产生的诊断信息。
    QString libraryDescription = m_options.footprintLibraryDescription;  ///< 封装库描述。
    {
        // 当前目标格式是否优先使用 WRL 模型。
        const bool preferWrl = m_options.needsModel3DWrl() &&
                               m_options.targetFormat != TargetEdaFormat::Altium;  ///< 是否优先导出 WRL 模型。
        const bool exportStep = m_options.needsEmbeddedModel3DStep();  ///< 是否嵌入 STEP 模型。
        QString libraryKeywords = m_options.footprintLibraryKeywords;  ///< 封装库关键词。
        // 转换旧类型列表到 IR 类型
        QList<IR::FootprintComponentIR> irFootprintList;  ///< 转换后的封装 IR 列表。
        irFootprintList.reserve(footprintList.size());
        for (const FootprintData& fd : footprintList) {  // 当前封装数据。
            irFootprintList.append(IR::toFootprintIR(fd));
        }
        const bool combinedTarget =
            m_options.targetFormat == TargetEdaFormat::Eagle || m_options.targetFormat == TargetEdaFormat::Cadstar ||
            m_options.targetFormat == TargetEdaFormat::Allegro || m_options.targetFormat == TargetEdaFormat::LibrePcb ||
            m_options.targetFormat == TargetEdaFormat::Horizon;
        if (combinedTarget && m_options.exportSymbol && m_options.exportFootprint) {
            exportSuccess = exporter->exportComponentLibrary(
                componentIrList, libName, tempPath, m_options.exportModel3D, outputDir);
        } else if (combinedTarget && m_options.exportSymbol) {
            auto* symbolExporter = dynamic_cast<ISymbolExporter*>(exporter.get());  ///< 目标导出器的符号库接口。
            if (!symbolExporter) {
                exporterDiagnostics.append(QStringLiteral("目标格式未提供组合库符号导出器"));
            } else {
                exportSuccess = symbolExporter->exportSymbolLibrary(
                    symbolList, libName, tempPath, false, m_options.updateMode, libraryDescription);
            }
        } else {
            exportSuccess = exporter->exportFootprintLibrary(
                irFootprintList,
                libName,
                tempPath,
                preferWrl,
                exportStep,
                libraryDescription,
                libraryKeywords,
                m_options.exportModel3DPathMode == ExportOptions::MODEL_3D_PATH_ABSOLUTE,
                outputDir);
        }
        const QStringList exporterReportedDiagnostics = exporter->diagnostics();  ///< 导出器返回的诊断列表。
        for (const QString& diagnostic : exporterReportedDiagnostics) {  // 当前导出诊断。
            if (!exporterDiagnostics.contains(diagnostic))
                exporterDiagnostics.append(diagnostic);
        }
        if (!exporterDiagnostics.isEmpty()) {
            QMutexLocker locker(&m_progressMutex);
            for (const QString& diagnostic : exporterDiagnostics) {  // 当前待发布诊断。
                if (!m_progress.diagnostics.contains(diagnostic))
                    m_progress.diagnostics.append(diagnostic);
            }
            const ExportTypeProgress progressSnapshot = m_progress;  ///< 发布诊断后的进度快照。
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
        QString errorMessage = QStringLiteral("Failed to export footprint library");  ///< 面向用户的导出失败摘要。
        if (!exporterDiagnostics.isEmpty())
            errorMessage += QStringLiteral(": ") + exporterDiagnostics.join(QStringLiteral("; "));
        abortExport(errorMessage);
        return;
    }

    if (m_options.targetFormat == TargetEdaFormat::Horizon) {
        QStringList horizonDiagnostics;  ///< Horizon Pool 更新诊断。
        if (!HorizonPoolIntegration::updatePool(tempPath, horizonDiagnostics)) {
            abortExport(QStringLiteral("Horizon Pool 官方更新失败，原有库未修改：%1")
                            .arg(horizonDiagnostics.join(QStringLiteral("; "))));
            return;
        }
    }

    QString workspaceTempPath;  ///< LibrePCB 工作区库的临时目录。
    if (exportIntoLibrePcbProject && !librePcbWorkspaceLibrary.isEmpty()) {
        workspaceTempPath = m_tempManager.createTempDirectoryPath(libName + QStringLiteral("-workspace-library"));
        if (workspaceTempPath.isEmpty() || !copyDirectoryTree(tempPath, workspaceTempPath)) {
            abortExport(
                QStringLiteral("LibrePCB 工作区库准备失败，未修改项目或工作区库: %1").arg(librePcbWorkspaceLibrary));
            return;
        }
    } else if (exportIntoLibrePcbProject) {
        qWarning() << "FootprintExportStage: LibrePCB workspace root not found for project:" << librePcbRoot;
    }

    if (exportIntoLibrePcbProject) {
        const QString mergedTempPath =  // LibrePCB 项目库合并后的临时目录。
            m_tempManager.createTempDirectoryPath(libName + QStringLiteral("-project-library"));
        if (mergedTempPath.isEmpty() || !mergeLibrePcbLibrary(tempPath, finalPath, mergedTempPath)) {
            abortExport(QStringLiteral("LibrePCB 项目库合并失败，已保留原项目库: %1").arg(finalPath));
            return;
        }
        tempPath = mergedTempPath;
    }

    // 提交临时文件/目录到最终路径
    bool commitSuccess = false;  ///< 临时输出是否已成功提交到最终路径。
    if (exportIntoLibrePcbProject) {
        QVector<TempFileManager::CommitItem> commitItems;  ///< 待提交的项目库和工作区库事务项。
        commitItems.append({tempPath, finalPath, true});
        if (!workspaceTempPath.isEmpty())
            commitItems.append({workspaceTempPath, librePcbWorkspaceLibrary, true});
        commitSuccess = m_tempManager.commitBatch(commitItems);
    } else if (!isDirOutput) {
        commitSuccess = m_tempManager.commitWithBackup(tempPath, finalPath);
    } else {
        commitSuccess = m_tempManager.commitDirectoryWithBackup(tempPath, finalPath);
    }

    if (commitSuccess) {
        qDebug() << "FootprintExportStage: Successfully exported to:" << finalPath;
        if (!librePcbWorkspaceLibrary.isEmpty())
            qDebug() << "FootprintExportStage: Installed LibrePCB workspace library to:" << librePcbWorkspaceLibrary;
    } else {
        abortExport(QStringLiteral("Failed to commit temp path"));
        return;
    }

    if (m_options.targetFormat == TargetEdaFormat::Horizon) {
        QStringList horizonDiagnostics;  ///< Horizon Pool 注册诊断。
        if (!HorizonPoolIntegration::registerPool(finalPath, horizonDiagnostics)) {
            abortExport(QStringLiteral("Horizon Pool 已生成，但官方注册失败：%1")
                            .arg(horizonDiagnostics.join(QStringLiteral("; "))));
            return;
        }
        if (!horizonDiagnostics.isEmpty()) {
            QMutexLocker locker(&m_progressMutex);
            for (const QString& diagnostic : std::as_const(horizonDiagnostics)) {
                if (!m_progress.diagnostics.contains(diagnostic))
                    m_progress.diagnostics.append(diagnostic);
            }
            const ExportTypeProgress progressSnapshot = m_progress;  ///< 注册诊断后的进度快照。
            locker.unlock();
            emit progressChanged(progressSnapshot);
            for (const QString& diagnostic : std::as_const(horizonDiagnostics))
                qWarning() << "FootprintExportStage:" << diagnostic;
        }
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
