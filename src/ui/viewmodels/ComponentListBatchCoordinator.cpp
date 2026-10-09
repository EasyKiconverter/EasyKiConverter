#include "ComponentListBatchCoordinator.h"

#include "ComponentListViewModel.h"
#include "ValidationStateManager.h"

#include <QDebug>
#include <QFutureWatcher>
#include <QMutexLocker>
#include <QPointer>
#include <QUrl>
#include <QtConcurrent>

namespace EasyKiConverter {

/**
 * @brief 收集有效且未重复的编号并启动批处理定时器。
 * @param viewModel 元件列表视图模型。
 * @param componentIds 待添加的元器件编号列表。
 */
void ComponentListBatchCoordinator::addComponents(ComponentListViewModel& viewModel, const QStringList& componentIds) {
    viewModel.clearAttentionHints();
    // 保存规范化后待加入的元器件编号。
    QStringList newIds;
    for (const QString& rawId : componentIds) {
        // 规范化当前输入的元器件编号。
        QString id = rawId.trimmed().toUpper();
        if (id.isEmpty()) {
            continue;
        }

        if (!viewModel.validateComponentId(id)) {
            // 从非标准输入中提取可能的元器件编号。
            const QStringList extracted = viewModel.extractComponentIdFromText(id);
            for (const QString& extractedId : extracted) {
                if (!viewModel.componentExists(extractedId) && !newIds.contains(extractedId)) {
                    newIds.append(extractedId);
                }
            }
            continue;
        }

        if (!viewModel.componentExists(id) && !newIds.contains(id)) {
            newIds.append(id);
        }
    }

    if (newIds.isEmpty()) {
        return;
    }

    viewModel.m_bomImportComplete = false;
    // 大量元器件导入时启用 BOM 导入模式，降低 UI 更新频率。
    if (newIds.count() >= 20 && !viewModel.m_bomImportMode) {
        viewModel.m_bomImportMode = true;
    }

    viewModel.m_pendingComponentIds.append(newIds);
    viewModel.m_pendingBatchValidationCount += newIds.count();
    if (!viewModel.m_batchAddTimer->isActive()) {
        viewModel.m_batchAddTimer->start();
    }
}

/**
 * @brief 从批处理队列取出一批元件并完成模型行插入。
 * @param viewModel 元件列表视图模型。
 */
void ComponentListBatchCoordinator::processNext(ComponentListViewModel& viewModel) {
    if (viewModel.m_pendingComponentIds.isEmpty()) {
        viewModel.m_batchAddTimer->stop();
        return;
    }

    // 保存本次从待处理队列取出的编号批次。
    QStringList batch;
    // 计算本次批处理的编号数量。
    const int count = qMin(ComponentListViewModel::BATCH_ADD_SIZE, viewModel.m_pendingComponentIds.size());
    for (int i = 0; i < count; ++i) {
        batch.append(viewModel.m_pendingComponentIds.takeFirst());
    }

    // 保存本批次待插入的列表项数量。
    const int pendingCount = batch.count();
    // 保存列表插入起始位置。
    int startIndex;
    // 标记当前批次是否为最后一批。
    bool isLastBatch;
    {
        QMutexLocker locker(&viewModel.m_listMutex);
        startIndex = viewModel.m_componentList.count();
        isLastBatch = viewModel.m_pendingComponentIds.isEmpty();
    }

    // 在锁外创建列表项，避免构造对象时持有共享列表锁。
    QList<ComponentListItemData*> newItems;
    for (const QString& id : batch) {
        // 创建并初始化当前编号对应的列表项。
        auto item = new ComponentListItemData(id, &viewModel);
        item->setFetching(true);
        item->setValid(false);
        item->setValidationPhase("validating");
        newItems.append(item);
    }

    viewModel.beginInsertRows(QModelIndex(), startIndex, startIndex + pendingCount - 1);
    {
        QMutexLocker locker(&viewModel.m_listMutex);
        for (auto item : newItems) {
            viewModel.m_componentList.append(item);
            viewModel.m_componentIdIndex.insert(item->componentId(), viewModel.m_componentList.count() - 1);
        }
    }
    viewModel.endInsertRows();

    if (isLastBatch) {
        // 保存本轮批处理产生的验证任务数量。
        const int validationCount = viewModel.m_pendingBatchValidationCount;
        viewModel.m_pendingBatchValidationCount = 0;

        // BOM 导入结束后恢复普通 UI 更新，并合并已累积的状态变更。
        if (viewModel.m_bomImportMode) {
            // 标记 BOM 导入期间是否积累了列表更新。
            const bool hadPendingUpdates = viewModel.m_bomImportPendingUpdates > 0;
            viewModel.m_bomImportMode = false;
            if (hadPendingUpdates) {
                viewModel.m_bomImportPendingUpdates = 0;
                viewModel.scheduleListUpdate();
            }
        }

        if (validationCount > 0) {
            if (viewModel.m_validationStateManager->pendingCount() > 0 || !viewModel.m_validationQueue.isEmpty() ||
                viewModel.m_validationQueue.hasInFlight()) {
                viewModel.m_validationStateManager->addValidation(validationCount);
            } else {
                viewModel.m_validationStateManager->startValidation(validationCount);
            }
        }

        viewModel.m_bomImportComplete = true;
        viewModel.startValidationQueue();
    }

    viewModel.scheduleListUpdate();
}

/**
 * @brief 启动后台 BOM 解析并在界面线程合并去重结果。
 * @param viewModel 元件列表视图模型。
 * @param filePath BOM 文件路径或文件 URL。
 */
void ComponentListBatchCoordinator::selectBomFile(ComponentListViewModel& viewModel, const QString& filePath) {
    qDebug() << "BOM file selected:" << filePath;

    if (viewModel.m_bomFilePath != filePath) {
        viewModel.m_bomFilePath = filePath;
        emit viewModel.bomFilePathChanged();
    }

    // 保存转换为本地文件系统格式后的 BOM 路径。
    QString localPath = filePath;
    if (filePath.startsWith("file:///")) {
        localPath = QUrl(filePath).toLocalFile();
        qDebug() << "Converted URL to local path:" << localPath;
    }

    viewModel.m_bomResult = "Parsing BOM file...";
    emit viewModel.bomResultChanged();

    // 保存后台 BOM 解析任务。
    QFuture<QStringList> future =
        QtConcurrent::run([&viewModel, localPath]() { return viewModel.m_service->parseBomFile(localPath); });

    // 监视后台 BOM 解析任务的完成状态。
    auto* watcher = new QFutureWatcher<QStringList>(&viewModel);
    QPointer<ComponentListViewModel> self(&viewModel);
    QObject::connect(watcher, &QFutureWatcher<QStringList>::finished, &viewModel, [self, watcher, localPath]() {
        if (!self) {
            return;
        }
        // 读取后台解析得到的元器件编号。
        const QStringList componentIds = watcher->result();
        watcher->deleteLater();

        if (componentIds.isEmpty()) {
            self->m_bomResult = "No valid component IDs found in BOM file";
            qWarning() << "No component IDs found in BOM file:" << localPath;
            emit self->bomResultChanged();
            return;
        }

        // 保存去重后待加入列表的编号。
        QStringList newIds;
        // 统计已存在而跳过的编号数量。
        int skipped = 0;
        for (const QString& id : componentIds) {
            if (!self->componentExists(id)) {
                newIds.append(id);
            } else {
                ++skipped;
            }
        }

        if (!newIds.isEmpty()) {
            ComponentListBatchCoordinator::addComponents(*self, newIds);
        }

        const QString resultMessage =
            QString("BOM file imported: %1 components added, %2 skipped").arg(newIds.count()).arg(skipped);
        self->m_bomResult = resultMessage;
        qDebug() << resultMessage;
        emit self->bomResultChanged();
    });
    watcher->setFuture(future);
}

}  // namespace EasyKiConverter
