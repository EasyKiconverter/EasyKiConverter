#ifndef COMPONENTLISTVIEWMODEL_H
#define COMPONENTLISTVIEWMODEL_H

#include "models/ComponentListItemData.h"
#include "services/ComponentService.h"
#include "ui/viewmodels/ComponentListIndex.h"
#include "ui/viewmodels/ComponentListItemUpdateBuffer.h"
#include "ui/viewmodels/ComponentListPreviewUpdateBuffer.h"
#include "ui/viewmodels/ComponentListStateTracker.h"
#include "ui/viewmodels/ComponentValidationQueue.h"

#include <QAbstractListModel>
#include <QMutex>
#include <QPointer>
#include <QSet>
#include <QStringList>
#include <QTimer>

namespace EasyKiConverter {

class ValidationStateManager;
class ComponentValidationCoordinator;
class ComponentListDataCoordinator;
class ComponentListBatchCoordinator;
class ComponentListClipboardCoordinator;
class ComponentListRetryCoordinator;
class ComponentListMutationCoordinator;
class ComponentListPreviewCoordinator;

// 元件列表视图模型
class ComponentListViewModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int componentCount READ componentCount NOTIFY componentCountChanged)
    Q_PROPERTY(QString bomFilePath READ bomFilePath NOTIFY bomFilePathChanged)
    Q_PROPERTY(QString bomResult READ bomResult NOTIFY bomResultChanged)
    Q_PROPERTY(bool hasInvalidComponents READ hasInvalidComponents NOTIFY hasInvalidComponentsChanged)
    Q_PROPERTY(QString filterMode READ filterMode NOTIFY filterModeChanged)
    Q_PROPERTY(int filteredCount READ filteredCount NOTIFY filteredCountChanged)
    Q_PROPERTY(int validatingCount READ validatingCount NOTIFY filteredCountChanged)
    Q_PROPERTY(int validCount READ validCount NOTIFY filteredCountChanged)
    Q_PROPERTY(int invalidCount READ invalidCount NOTIFY filteredCountChanged)
    Q_PROPERTY(bool isScrolling READ isScrolling NOTIFY isScrollingChanged)
    Q_PROPERTY(bool validationReadyHint READ validationReadyHint NOTIFY attentionStateChanged)
    Q_PROPERTY(bool previewReadyHint READ previewReadyHint NOTIFY attentionStateChanged)

public:
    enum ComponentRoles { ItemDataRole = Qt::UserRole + 1 };

    explicit ComponentListViewModel(ComponentService* service, QObject* parent = nullptr);
    ~ComponentListViewModel() override;

    // QAbstractListModel
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    /**
     * @brief 返回当前列表中的元件数量。
     */
    int componentCount() const {
        QMutexLocker locker(&m_listMutex);
        return m_componentList.count();
    }

    /**
     * @brief 返回当前选择的 BOM 文件路径。
     */
    QString bomFilePath() const {
        return m_bomFilePath;
    }

    /**
     * @brief 返回最近一次 BOM 解析结果。
     */
    QString bomResult() const {
        return m_bomResult;
    }

    /**
     * @brief 返回是否存在可重试的失败元件。
     */
    bool hasInvalidComponents() const {
        return m_hasInvalidComponents;
    }

    QSharedPointer<ComponentData> getPreloadedData(const QString& componentId) const;
    QMap<QString, QSharedPointer<ComponentData>> getAllPreloadedData() const;

public slots:
    Q_INVOKABLE void addComponent(const QString& componentId);
    Q_INVOKABLE void removeComponent(int index);
    Q_INVOKABLE void removeComponentById(const QString& componentId);
    Q_INVOKABLE void clearComponentList();
    Q_INVOKABLE void addComponentsBatch(const QStringList& componentIds);
    void processNextBatchAdd();
    Q_INVOKABLE void pasteFromClipboard();
    Q_INVOKABLE void copyAllComponentIds();
    Q_INVOKABLE void selectBomFile(const QString& filePath);
    void fetchComponentData(const QString& componentId, bool fetch3DModel = true);
    void setOutputPath(const QString& path);

    /**
     * @brief 返回当前导出输出目录。
     */
    QString outputPath() const {
        return m_outputPath;
    }

    Q_INVOKABLE void refreshComponentInfo(int index);
    Q_INVOKABLE void retryAllInvalidComponents();
    Q_INVOKABLE QStringList getAllComponentIds() const;
    Q_INVOKABLE void copyToClipboard(const QString& text);
    Q_INVOKABLE void setFilterMode(const QString& mode);

    // Getter 方法
    /**
     * @brief 返回当前列表筛选模式。
     */
    QString filterMode() const {
        return m_stateTracker.filterMode();
    }

    int filteredCount() const;
    int validatingCount() const;
    int validCount() const;
    int invalidCount() const;

    /**
     * @brief 返回列表是否正在滚动。
     */
    bool isScrolling() const {
        return m_isScrolling;
    }

    /**
     * @brief 返回验证完成提示是否可见。
     */
    bool validationReadyHint() const {
        return m_validationReadyHint;
    }

    /**
     * @brief 返回预览图完成提示是否可见。
     */
    bool previewReadyHint() const {
        return m_previewReadyHint;
    }

    Q_INVOKABLE void setScrolling(bool scrolling);
    Q_INVOKABLE void fetchPreviewImages(const QStringList& componentIds);
    Q_INVOKABLE void updateExportStatus(const QString& componentId, int previewImageExported, int datasheetExported);
    Q_INVOKABLE void dismissAttentionHints();
    Q_INVOKABLE void updateComponentDescription(const QString& componentId, const QString& description);

signals:
    void componentCountChanged();
    void bomFilePathChanged();
    void bomResultChanged();
    void hasInvalidComponentsChanged();
    void filterModeChanged();
    void filteredCountChanged();
    void isScrollingChanged();
    void componentAdded(const QString& componentId, bool success, const QString& message);
    void componentRemoved(const QString& componentId);
    void listCleared();
    void pasteCompleted(int added, int skipped);
    void outputPathChanged();
    void attentionStateChanged();
    void previewFetchRequested();

private slots:
    void handleComponentInfoReady(const QString& componentId, const ComponentData& data);
    void handleCadDataReady(const QString& componentId, const ComponentData& data);
    void handleModel3DReady(const QString& uuid, const QString& filePath);
    void handleFetchError(const QString& componentId, const QString& error);
    void handleLcscDataUpdated(const QString& componentId,
                               const QString& manufacturerPart,
                               const QString& datasheetUrl,
                               const QStringList& imageUrls);
    void handleDatasheetReady(const QString& componentId, const QByteArray& datasheetData);

private:
    friend class ComponentValidationCoordinator;
    friend class ComponentListDataCoordinator;
    friend class ComponentListBatchCoordinator;
    friend class ComponentListTimerCoordinator;
    friend class ComponentListServiceConnectionCoordinator;
    friend class ComponentListClipboardCoordinator;
    friend class ComponentListRetryCoordinator;
    friend class ComponentListMutationCoordinator;
    friend class ComponentListPreviewCoordinator;

    /**
     * @brief 创建批处理、预览和延迟调度所需的定时器。
     */
    void initializeTimers();

    /**
     * @brief 连接验证服务和组件服务的异步信号。
     */
    void initializeServiceConnections();

    bool componentExists(const QString& componentId) const;
    bool validateComponentId(const QString& componentId) const;
    QStringList extractComponentIdFromText(const QString& text) const;
    ComponentListItemData* findItemData(const QString& componentId) const;
    void recomputeStateCounters();
    void updateHasInvalidComponents();
    void rebuildComponentIdIndex();
    void startValidationQueue();
    void processNextValidation();
    void onValidationComplete(const QString& componentId);
    void fetchAllPreviewImages();
    void processCachePreviewImages();  // 批量处理缓存预览图（防抖）
    void scheduleListUpdate();  // 批量模式下的列表更新调度
    void delayedFetchPreviewImages();  // 延迟获取预览图，等待所有验证完成
    void markPreviewFetchCompleted(const QString& componentId);
    void clearAttentionHints();

private:
    ComponentService* m_service;  ///< 组件数据服务引用。
    QList<ComponentListItemData*> m_componentList;  ///< 当前列表项集合。
    ComponentListIndex m_componentIdIndex;  ///< 元器件编号索引。
    mutable QMutex m_listMutex;  // 保护 m_componentList 和对应索引
    QString m_outputPath;  ///< 当前导出输出目录。
    QString m_bomFilePath;  ///< 当前选择的 BOM 文件路径。
    QString m_bomResult;  ///< 最近一次 BOM 解析结果。
    bool m_hasInvalidComponents = false;  ///< 是否存在可重试的失败元件。

    ValidationStateManager* m_validationStateManager;  ///< 验证状态管理器。

    int m_pendingValidationCount = 0;  ///< 尚未完成验证的元件数量。
    QStringList m_validatedComponentIds;  ///< 已完成验证的元件编号。

    ComponentListStateTracker m_stateTracker;  ///< 列表筛选与计数状态跟踪器。
    bool m_isScrolling = false;  ///< 列表是否正在滚动。

    ComponentValidationQueue m_validationQueue;  ///< 待验证和验证中的元件队列。
    int m_validationPendingCount = 0;  ///< 待启动验证的元件数量。
    int m_validationCompletedCount = 0;  ///< 已完成验证的元件数量。
    int m_validationTotalCount = 0;  ///< 本轮验证的元件总数量。

    // 缓存预览图批量更新（防抖）
    ComponentListPreviewUpdateBuffer m_previewUpdateBuffer;
    QTimer* m_cachePreviewImageTimer;  ///< 缓存预览图批量更新定时器。

    QStringList m_pendingComponentIds;  ///< 等待批量加入并验证的元件编号。
    int m_pendingBatchValidationCount = 0;  ///< 当前批量加入任务待验证数量。
    QTimer* m_batchAddTimer;  ///< 批量加入任务调度定时器。
    // 批处理大小（BOM导入时增大以减少调度开销，配合m_bomImportMode降低UI更新频率）
    static constexpr int BATCH_ADD_SIZE = 50;

    // 批量更新模式（验证期间暂停 UI 更新）
    ComponentListItemUpdateBuffer m_batchUpdateItems;  ///< 待合并通知的列表项更新缓冲区。
    QTimer* m_batchUpdateTimer;  ///< 列表项批量更新定时器。

    // 列表更新批处理（合并 componentCountChanged 和 filteredCountChanged）
    bool m_batchListUpdateMode = false;  ///< 是否处于列表统计批量更新模式。
    QTimer* m_batchListUpdateTimer;  ///< 列表统计批量更新定时器。

    // BOM 导入模式（限制 UI 更新频率）
    bool m_bomImportMode = false;  ///< 是否处于 BOM 批量导入模式。
    QTimer* m_bomImportUpdateTimer;  ///< BOM 导入模式更新定时器。
    int m_bomImportPendingUpdates = 0;  ///< BOM 导入模式下待处理的验证完成计数。
    bool m_listUpdatePending = false;  ///< 列表更新防抖标志，防止定时器级联重启。
    bool m_bomImportComplete = false;  ///< 是否已为所有 BOM 元件启动验证。

    // 延迟获取预览图定时器（等待验证全部完成）
    QTimer* m_delayedFetchPreviewTimer;  ///< 等待验证完成后获取预览图的定时器。
    QSet<QString> m_pendingPreviewFetchIds;  ///< 等待获取预览图的元件编号集合。
    bool m_validationReadyHint = false;  ///< 是否显示验证完成提示。
    bool m_previewReadyHint = false;  ///< 是否显示预览图完成提示。
};

}  // namespace EasyKiConverter

#endif  // COMPONENTLISTVIEWMODEL_H
