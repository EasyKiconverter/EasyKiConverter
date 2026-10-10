#include "ComponentListItemData.h"

#include <QBuffer>

namespace EasyKiConverter {

// 静态成员初始化
bool ComponentListItemData::s_holdPreviewImageNotifications = false;
QMutex ComponentListItemData::s_previewImageMutex;

/**
 * @brief 创建元器件列表项并初始化验证状态。
 * @param componentId 元器件编号。
 * @param parent Qt 父对象。
 */
ComponentListItemData::ComponentListItemData(const QString& componentId, QObject* parent)
    : QObject(parent)
    , m_componentId(componentId)
    , m_isValid(true)  // 默认为 true，直到验证失败
    , m_isFetching(false)
    , m_validationPhase("idle") {}

/**
 * @brief 设置元器件名称并发送数据变化通知。
 * @param name 元器件名称。
 */
void ComponentListItemData::setName(const QString& name) {
    if (m_name != name) {
        m_name = name;
        emit dataChanged();
    }
}

/**
 * @brief 设置封装名称并发送数据变化通知。
 * @param package 封装名称。
 */
void ComponentListItemData::setPackage(const QString& package) {
    if (m_package != package) {
        m_package = package;
        emit dataChanged();
    }
}

/**
 * @brief 设置描述并同步到关联的符号和封装。
 * @param description 元器件描述。
 */
void ComponentListItemData::setDescription(const QString& description) {
    m_descriptionEdited = true;
    if (m_description != description) {
        m_description = description;
        applyDescriptionToComponentData();
        emit dataChanged();
        return;
    }
    applyDescriptionToComponentData();
}

/**
 * @brief 静默设置元器件名称，供批量更新使用。
 * @param name 元器件名称。
 */
void ComponentListItemData::setNameSilent(const QString& name) {
    m_name = name;
}

/**
 * @brief 静默设置封装名称，供批量更新使用。
 * @param package 封装名称。
 */
void ComponentListItemData::setPackageSilent(const QString& package) {
    m_package = package;
}

/**
 * @brief 绑定详细数据并同步列表项展示字段。
 * @param data 元器件详细数据。
 */
void ComponentListItemData::setComponentData(const QSharedPointer<ComponentData>& data) {
    m_componentData = data;
    if (data) {
        if (!data->name().isEmpty())
            setName(data->name());
        if (!data->package().isEmpty())
            setPackage(data->package());
        if (!m_descriptionEdited) {
            m_description = data->name();
        }
        applyDescriptionToComponentData();
    }
    emit dataChanged();
}

/**
 * @brief 将列表项描述同步到符号和封装数据。
 */
void ComponentListItemData::applyDescriptionToComponentData() {
    if (!m_componentData) {
        return;
    }

    if (m_componentData->symbolData()) {
        SymbolInfo info = m_componentData->symbolData()->info();  // 当前符号基本信息副本。
        info.description = m_description;
        m_componentData->symbolData()->setInfo(info);
    }
    if (m_componentData->footprintData()) {
        FootprintInfo info = m_componentData->footprintData()->info();  // 当前封装基本信息副本。
        info.description = m_description;
        m_componentData->footprintData()->setInfo(info);
    }
}

/**
 * @brief 更新验证状态并通知界面。
 * @param valid 是否验证有效。
 */
void ComponentListItemData::setValid(bool valid) {
    if (m_isValid != valid) {
        m_isValid = valid;
        emit validationStatusChanged();
    }
}

/**
 * @brief 更新数据获取状态并通知界面。
 * @param fetching 是否正在获取数据。
 */
void ComponentListItemData::setFetching(bool fetching) {
    if (m_isFetching != fetching) {
        m_isFetching = fetching;
        emit fetchingStatusChanged();
    }
}

/**
 * @brief 更新验证阶段并通知界面。
 * @param phase 验证阶段名称。
 */
void ComponentListItemData::setValidationPhase(const QString& phase) {
    if (m_validationPhase != phase) {
        m_validationPhase = phase;
        emit validationPhaseChanged();
    }
}

/**
 * @brief 更新错误信息并通知界面。
 * @param error 错误信息。
 */
void ComponentListItemData::setErrorMessage(const QString& error) {
    if (m_errorMessage != error) {
        m_errorMessage = error;
        emit validationStatusChanged();
    }
}

/**
 * @brief 更新失败后的可重试状态。
 * @param retryable 是否允许重试。
 */
void ComponentListItemData::setRetryable(bool retryable) {
    if (m_retryable != retryable) {
        m_retryable = retryable;
        emit validationStatusChanged();
    }
}

/**
 * @brief 更新预览图导出状态。
 * @param exported 是否已经导出。
 */
void ComponentListItemData::setPreviewImageExported(bool exported) {
    if (m_previewImageExported != exported) {
        m_previewImageExported = exported;
        emit exportStatusChanged();
    }
}

/**
 * @brief 更新数据手册导出状态。
 * @param exported 是否已经导出。
 */
void ComponentListItemData::setDatasheetExported(bool exported) {
    if (m_datasheetExported != exported) {
        m_datasheetExported = exported;
        emit exportStatusChanged();
    }
}

/**
 * @brief 将原始预览图编码并刷新 QML 缓存。
 */
void ComponentListItemData::updatePreviewImagesCache() const {
    m_previewImagesCache.clear();
    for (const QImage& image : m_previewImages) {
        if (image.isNull()) {
            m_previewImagesCache.append(QVariant());
            continue;
        }
        QByteArray byteArray;  // 当前预览图的 PNG 编码字节。
        QBuffer buffer(&byteArray);
        buffer.open(QIODevice::WriteOnly);
        image.save(&buffer, "PNG");
        m_previewImagesCache.append(QString::fromLatin1(byteArray.toBase64().data()));
    }
}

/**
 * @brief 返回预览图编码缓存，必要时按需生成。
 * @return 面向 QML 的预览图编码列表。
 */
QVariantList ComponentListItemData::previewImages() const {
    if (m_previewImagesCache.isEmpty() && !m_previewImages.isEmpty()) {
        updatePreviewImagesCache();
    }
    return m_previewImagesCache;
}

/**
 * @brief 统计原始图和编码缓存中的有效预览图。
 * @return 有效预览图数量。
 */
int ComponentListItemData::previewImageCount() const {
    int cacheCount = 0;  // 编码缓存中的有效预览图数量。
    for (const QVariant& cached : m_previewImagesCache) {
        if (cached.isValid() && !cached.toString().isEmpty()) {
            ++cacheCount;
        }
    }

    int rawCount = 0;  // 原始图像列表中的有效预览图数量。
    for (const QImage& image : m_previewImages) {
        if (!image.isNull()) {
            ++rawCount;
        }
    }

    return qMax(cacheCount, rawCount);
}

/**
 * @brief 追加预览图并通知界面。
 * @param image 待追加的预览图。
 */
void ComponentListItemData::addPreviewImage(const QImage& image) {
    if (!image.isNull()) {
        m_previewImages.append(image);
        m_previewImagesCache.clear();
        emit previewImagesChanged();
    }
}

/**
 * @brief 在指定索引写入预览图并通知界面。
 * @param image 待写入的预览图。
 * @param index 目标索引。
 */
void ComponentListItemData::insertPreviewImage(const QImage& image, int index) {
    if (image.isNull()) {
        return;
    }

    while (m_previewImages.size() <= index) {
        m_previewImages.append(QImage());
    }

    if (m_previewImages[index].isNull()) {
        m_previewImages[index] = image;
        qDebug() << "Inserted preview image at index:" << index;
    } else {
        m_previewImages[index] = image;
        qDebug() << "Replaced preview image at index:" << index;
    }

    m_previewImagesCache.clear();
    emit previewImagesChanged();
}

/**
 * @brief 替换全部预览图编码缓存并通知界面。
 * @param encodedImages 已编码的预览图列表。
 */
void ComponentListItemData::setEncodedPreviewImages(const QStringList& encodedImages) {
    m_previewImagesCache.clear();
    for (const QString& encoded : encodedImages) {
        if (encoded.isEmpty()) {
            m_previewImagesCache.append(QVariant());
        } else {
            m_previewImagesCache.append(encoded);
        }
    }
    emit previewImagesChanged();
}

/**
 * @brief 更新指定索引的预览图编码并按需通知界面。
 * @param encodedImage 已编码的预览图。
 * @param index 目标索引。
 * @param notify 是否发送界面通知。
 */
void ComponentListItemData::setEncodedPreviewImageAt(const QString& encodedImage, int index, bool notify) {
    if (index < 0) {
        return;
    }

    while (m_previewImagesCache.size() <= index) {
        m_previewImagesCache.append(QVariant());
    }

    if (encodedImage.isEmpty()) {
        m_previewImagesCache[index] = QVariant();
    } else {
        m_previewImagesCache[index] = encodedImage;
    }

    if (notify) {
        emit previewImagesChanged();
    }
}

/**
 * @brief 发出预览图内容变化通知。
 */
void ComponentListItemData::notifyPreviewImagesChanged() {
    emit previewImagesChanged();
}

/**
 * @brief 替换全部原始预览图并清空编码缓存。
 * @param images 原始预览图列表。
 */
void ComponentListItemData::setPreviewImages(const QList<QImage>& images) {
    m_previewImages = images;
    m_previewImagesCache.clear();
    emit previewImagesChanged();
}

/**
 * @brief 从关联详细数据中返回数据手册地址。
 * @return 数据手册地址。
 */
QString ComponentListItemData::datasheetUrl() const {
    if (m_componentData) {
        return m_componentData->datasheet();
    }
    return QString();
}

/**
 * @brief 更新编码预览图并根据全局批处理状态决定是否通知。
 * @param encodedImages 已编码的预览图列表。
 */
void ComponentListItemData::setEncodedPreviewImagesHoldNotify(const QStringList& encodedImages) {
    m_previewImagesCache.clear();
    for (const QString& encoded : encodedImages) {
        if (encoded.isEmpty()) {
            m_previewImagesCache.append(QVariant());
        } else {
            m_previewImagesCache.append(encoded);
        }
    }
    // 如果没有暂停通知，则发射信号
    bool shouldNotify = false;  // 当前是否需要发送预览图变更通知。
    {
        QMutexLocker locker(&s_previewImageMutex);
        shouldNotify = !s_holdPreviewImageNotifications;
    }
    if (shouldNotify) {
        emit previewImagesChanged();
    }
}

/**
 * @brief 更新编码预览图但不发出界面通知。
 * @param encodedImages 已编码的预览图列表。
 */
void ComponentListItemData::setEncodedPreviewImagesSilent(const QStringList& encodedImages) {
    m_previewImagesCache.clear();
    for (const QString& encoded : encodedImages) {
        if (encoded.isEmpty()) {
            m_previewImagesCache.append(QVariant());
        } else {
            m_previewImagesCache.append(encoded);
        }
    }
    // 不发射信号，由其他更新触发 UI 刷新
}

/**
 * @brief 设置全局标志以暂停预览图通知。
 */
void ComponentListItemData::holdPreviewImageNotifications() {
    QMutexLocker locker(&s_previewImageMutex);
    s_holdPreviewImageNotifications = true;
}

/**
 * @brief 清除全局暂停标志，允许后续预览图通知。
 */
void ComponentListItemData::flushPreviewImageNotifications() {
    // 静态方法无法发射实例信号，改为发射所有待更新项的信号
    // 使用锁保护确保线程安全
    QMutexLocker locker(&s_previewImageMutex);
    s_holdPreviewImageNotifications = false;
}

}  // namespace EasyKiConverter
