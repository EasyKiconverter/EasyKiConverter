#ifndef COMPONENTLISTITEMDATA_H
#define COMPONENTLISTITEMDATA_H

#include "ComponentData.h"

#include <QImage>
#include <QMutex>
#include <QObject>
#include <QSharedPointer>
#include <QString>

namespace EasyKiConverter {

/**
 * @brief 元器件列表项数据模型
 *
 * 扩展 ComponentData，增加 UI 相关的状态信息
 */
class ComponentListItemData : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString componentId READ componentId CONSTANT)
    Q_PROPERTY(QString name READ name NOTIFY dataChanged)
    Q_PROPERTY(QString package READ package NOTIFY dataChanged)
    Q_PROPERTY(QString description READ description NOTIFY dataChanged)
    Q_PROPERTY(QVariantList previewImages READ previewImages NOTIFY previewImagesChanged)
    Q_PROPERTY(int previewImageCount READ previewImageCount NOTIFY previewImagesChanged)
    Q_PROPERTY(bool isValid READ isValid NOTIFY validationStatusChanged)
    Q_PROPERTY(bool isFetching READ isFetching NOTIFY fetchingStatusChanged)
    Q_PROPERTY(QString validationPhase READ validationPhase NOTIFY validationPhaseChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY validationStatusChanged)
    Q_PROPERTY(bool retryable READ retryable NOTIFY validationStatusChanged)
    Q_PROPERTY(QString datasheetUrl READ datasheetUrl NOTIFY datasheetChanged)
    Q_PROPERTY(bool previewImageExported READ previewImageExported NOTIFY exportStatusChanged)
    Q_PROPERTY(bool datasheetExported READ datasheetExported NOTIFY exportStatusChanged)

public:
    explicit ComponentListItemData(const QString& componentId, QObject* parent = nullptr);

    /**
     * @brief 批量更新预览图并按需延迟通知。
     * @param encodedImages 已编码的预览图列表。
     */
    void setEncodedPreviewImagesHoldNotify(const QStringList& encodedImages);
    /**
     * @brief 批量更新预览图但不发出通知。
     * @param encodedImages 已编码的预览图列表。
     */
    void setEncodedPreviewImagesSilent(const QStringList& encodedImages);
    /**
     * @brief 暂停所有列表项的预览图通知。
     */
    static void holdPreviewImageNotifications();
    /**
     * @brief 恢复列表项的预览图通知。
     */
    static void flushPreviewImageNotifications();

    /**
     * @brief 返回元器件编号。
     * @return 元器件编号。
     */
    QString componentId() const {
        return m_componentId;
    }

    /**
     * @brief 返回元器件名称。
     * @return 元器件名称。
     */
    QString name() const {
        return m_name;
    }

    /**
     * @brief 返回封装名称。
     * @return 封装名称。
     */
    QString package() const {
        return m_package;
    }

    /**
     * @brief 返回元器件描述。
     * @return 元器件描述。
     */
    QString description() const {
        return m_description;
    }

    /**
     * @brief 返回供 QML 使用的预览图列表。
     */
    QVariantList previewImages() const;

    /**
     * @brief 返回有效预览图数量。
     */
    int previewImageCount() const;

    /**
     * @brief 返回元器件是否验证有效。
     * @return 验证有效时返回 true。
     */
    bool isValid() const {
        return m_isValid;
    }

    /**
     * @brief 返回元器件是否正在获取数据。
     * @return 正在获取数据时返回 true。
     */
    bool isFetching() const {
        return m_isFetching;
    }

    /**
     * @brief 返回当前验证阶段。
     * @return 验证阶段名称。
     */
    QString validationPhase() const {
        return m_validationPhase;
    }

    /**
     * @brief 返回最近一次错误信息。
     * @return 最近一次错误信息。
     */
    QString errorMessage() const {
        return m_errorMessage;
    }

    /**
     * @brief 返回失败后是否允许重试。
     * @return 允许重试时返回 true。
     */
    bool retryable() const {
        return m_retryable;
    }

    /**
     * @brief 返回数据手册地址。
     */
    QString datasheetUrl() const;

    /**
     * @brief 返回预览图是否已导出。
     * @return 预览图已经导出时返回 true。
     */
    bool previewImageExported() const {
        return m_previewImageExported;
    }

    /**
     * @brief 返回数据手册是否已导出。
     * @return 数据手册已经导出时返回 true。
     */
    bool datasheetExported() const {
        return m_datasheetExported;
    }

    /**
     * @brief 返回关联的元器件详细数据。
     * @return 关联的元器件详细数据。
     */
    QSharedPointer<ComponentData> componentData() const {
        return m_componentData;
    }

    /**
     * @brief 设置元器件名称并通知界面。
     * @param name 元器件名称。
     */
    void setName(const QString& name);
    /**
     * @brief 设置封装名称并通知界面。
     * @param package 封装名称。
     */
    void setPackage(const QString& package);
    /**
     * @brief 设置描述并同步到符号和封装数据。
     * @param description 元器件描述。
     */
    void setDescription(const QString& description);
    /**
     * @brief 静默设置元器件名称。
     * @param name 元器件名称。
     */
    void setNameSilent(const QString& name);
    /**
     * @brief 静默设置封装名称。
     * @param package 封装名称。
     */
    void setPackageSilent(const QString& package);
    /**
     * @brief 追加一张预览图。
     * @param image 待追加的预览图。
     */
    void addPreviewImage(const QImage& image);
    /**
     * @brief 在指定位置插入预览图。
     * @param image 待插入的预览图。
     * @param index 目标索引。
     */
    void insertPreviewImage(const QImage& image, int index);
    /**
     * @brief 替换全部原始预览图。
     * @param images 原始预览图列表。
     */
    void setPreviewImages(const QList<QImage>& images);
    /**
     * @brief 设置关联的元器件详细数据。
     * @param data 元器件详细数据。
     */
    void setComponentData(const QSharedPointer<ComponentData>& data);
    /**
     * @brief 设置验证状态。
     * @param valid 是否验证有效。
     */
    void setValid(bool valid);
    /**
     * @brief 设置数据获取状态。
     * @param fetching 是否正在获取数据。
     */
    void setFetching(bool fetching);
    /**
     * @brief 设置验证阶段。
     * @param phase 验证阶段名称。
     */
    void setValidationPhase(const QString& phase);
    /**
     * @brief 设置错误信息。
     * @param error 错误信息。
     */
    void setErrorMessage(const QString& error);
    /**
     * @brief 设置是否允许重试。
     * @param retryable 是否允许重试。
     */
    void setRetryable(bool retryable);
    /**
     * @brief 设置预览图导出状态。
     * @param exported 是否已经导出。
     */
    void setPreviewImageExported(bool exported);
    /**
     * @brief 设置数据手册导出状态。
     * @param exported 是否已经导出。
     */
    void setDatasheetExported(bool exported);

    /**
     * @brief 设置已经编码的全部预览图。
     * @param encodedImages 已编码的预览图列表。
     */
    void setEncodedPreviewImages(const QStringList& encodedImages);
    /**
     * @brief 设置指定位置的编码预览图。
     * @param encodedImage 已编码的预览图。
     * @param index 目标索引。
     * @param notify 是否立即发送变更通知。
     */
    void setEncodedPreviewImageAt(const QString& encodedImage, int index, bool notify = true);
    /**
     * @brief 通知界面预览图内容已变化。
     */
    void notifyPreviewImagesChanged();

signals:
    void dataChanged();
    void previewImagesChanged();
    void validationStatusChanged();
    void fetchingStatusChanged();
    void validationPhaseChanged();
    void datasheetChanged();
    void exportStatusChanged();

private:
    void updatePreviewImagesCache() const;
    void applyDescriptionToComponentData();

    QString m_componentId;  ///< 元器件编号。
    QString m_name;  ///< 元器件名称。
    QString m_package;  ///< 封装名称。
    QString m_description;  ///< 元器件描述。
    bool m_descriptionEdited = false;  ///< 描述是否被用户编辑过。
    QList<QImage> m_previewImages;  ///< 原始预览图列表。
    mutable QVariantList m_previewImagesCache;  ///< 面向 QML 的编码预览图缓存。
    bool m_isValid;  ///< 当前元器件是否验证有效。
    bool m_isFetching;  ///< 当前元器件是否正在获取数据。
    QString m_validationPhase;  // "idle" | "validating" | "fetching_preview" | "completed" | "failed"
    QString m_errorMessage;  ///< 最近一次错误信息。
    bool m_retryable = true;  ///< 失败后是否允许重试。
    QSharedPointer<ComponentData> m_componentData;  ///< 关联的元器件详细数据。
    bool m_previewImageExported = false;  ///< 预览图是否已经导出。
    bool m_datasheetExported = false;  ///< 数据手册是否已经导出。

    // 静态标志：暂停预览图更新通知（用于批量更新）
    // 保护：s_holdPreviewImageNotifications 和 s_previewImageMutex 共同保护此标志
    static bool s_holdPreviewImageNotifications;  ///< 是否暂停预览图通知。
    static QMutex s_previewImageMutex;  ///< 保护静态预览图通知状态的互斥锁。
};

}  // namespace EasyKiConverter

#endif  // COMPONENTLISTITEMDATA_H
