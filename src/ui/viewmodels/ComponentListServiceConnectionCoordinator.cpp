#include "ComponentListServiceConnectionCoordinator.h"

#include "ComponentListViewModel.h"
#include "ValidationStateManager.h"

#include <QBuffer>
#include <QDebug>
#include <QIODevice>
#include <QImage>

namespace EasyKiConverter {

/**
 * @brief 创建验证、组件数据和预览图相关的服务信号连接。
 * @param owner 元件列表视图模型。
 */
void ComponentListServiceConnectionCoordinator::initialize(ComponentListViewModel& owner) {
    // 所有元件验证完成后再请求预览图，避免验证和媒体请求同时争抢资源。
    QObject::connect(owner.m_validationStateManager,
                     &ValidationStateManager::validationCompleted,
                     &owner,
                     [&owner](const QStringList& validatedIds) {
                         qDebug() << "All validations completed, validated component count:" << validatedIds.size();
                         // 获取当前列表中的元件总数。
                         const int totalCount = owner.componentCount();
                         owner.m_validationReadyHint = !validatedIds.isEmpty() && validatedIds.size() == totalCount;
                         owner.m_previewReadyHint = false;
                         emit owner.attentionStateChanged();
                         emit owner.previewFetchRequested();
                     });

    QObject::connect(owner.m_service,
                     &ComponentService::componentInfoReady,
                     &owner,
                     &ComponentListViewModel::handleComponentInfoReady);
    QObject::connect(
        owner.m_service, &ComponentService::cadDataReady, &owner, &ComponentListViewModel::handleCadDataReady);
    QObject::connect(
        owner.m_service, &ComponentService::model3DReady, &owner, &ComponentListViewModel::handleModel3DReady);
    QObject::connect(
        owner.m_service, &ComponentService::lcscDataUpdated, &owner, &ComponentListViewModel::handleLcscDataUpdated);
    QObject::connect(
        owner.m_service, &ComponentService::datasheetReady, &owner, &ComponentListViewModel::handleDatasheetReady);
    QObject::connect(owner.m_service, &ComponentService::fetchError, &owner, &ComponentListViewModel::handleFetchError);

    QObject::connect(owner.m_service,
                     &ComponentService::previewImageReady,
                     &owner,
                     [&owner](const QString& componentId, const QImage& image, int imageIndex) {
                         if (image.isNull()) {
                             return;
                         }

                         // 保存预览图编码前的 PNG 数据。
                         QByteArray byteArray;
                         QBuffer buffer(&byteArray);
                         if (!buffer.open(QIODevice::WriteOnly)) {
                             return;
                         }
                         image.save(&buffer, "PNG");

                         owner.m_previewUpdateBuffer.addIncremental(
                             componentId, imageIndex, QString::fromLatin1(byteArray.toBase64().constData()));
                         if (!owner.m_cachePreviewImageTimer->isActive()) {
                             owner.m_cachePreviewImageTimer->start();
                         }
                     });

    QObject::connect(owner.m_service,
                     &ComponentService::previewImageFailed,
                     &owner,
                     [&owner](const QString& componentId, const QString& error) {
                         Q_UNUSED(error);
                         // 预览图获取失败不影响验证状态，只记录日志并结束预览请求状态。
                         auto item = owner.findItemData(componentId);
                         if (item && item->isValid() && item->validationPhase() == "fetching_preview") {
                             item->setValidationPhase("completed");
                             owner.scheduleListUpdate();
                         }
                         owner.markPreviewFetchCompleted(componentId);
                     });

    QObject::connect(owner.m_service,
                     &ComponentService::previewImagesReady,
                     &owner,
                     [&owner](const QString& componentId, const QStringList& encodedImages) {
                         // 收集到待处理列表，使用防抖避免频繁 UI 更新。
                         owner.m_previewUpdateBuffer.addComplete(componentId, encodedImages);
                         if (!owner.m_cachePreviewImageTimer->isActive()) {
                             owner.m_cachePreviewImageTimer->start();
                         }
                         owner.markPreviewFetchCompleted(componentId);
                     });
}

}  // namespace EasyKiConverter
