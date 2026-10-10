#include "ComponentListDataCoordinator.h"

#include "ComponentListViewModel.h"
#include "ComponentValidationErrorPolicy.h"
#include "ValidationStateManager.h"

#include <QCoreApplication>
#include <QDebug>
#include <QMutexLocker>
#include <QTimer>

namespace EasyKiConverter {

/**
 * @brief 将基础信息合并到列表项并安排批量界面更新。
 * @param viewModel 元件列表视图模型。
 * @param componentId 元器件编号。
 * @param data 元器件基础数据。
 */
void ComponentListDataCoordinator::handleComponentInfo(ComponentListViewModel& viewModel,
                                                       const QString& componentId,
                                                       const ComponentData& data) {
    // 查找需要更新基础信息的列表项。
    auto item = viewModel.findItemData(componentId);
    if (item) {
        item->setNameSilent(data.name());
        item->setPackageSilent(data.package());
        viewModel.m_batchUpdateItems.add(item);
        if (!viewModel.m_batchUpdateTimer->isActive()) {
            viewModel.m_batchUpdateTimer->start();
        }
    }
}

/**
 * @brief 写入 CAD 数据并按原有顺序通知验证状态管理器。
 * @param viewModel 元件列表视图模型。
 * @param componentId 元器件编号。
 * @param data CAD 数据。
 */
void ComponentListDataCoordinator::handleCadData(ComponentListViewModel& viewModel,
                                                 const QString& componentId,
                                                 const ComponentData& data) {
    // 查找需要更新 CAD 数据的列表项。
    auto item = viewModel.findItemData(componentId);
    if (!item) {
        qWarning() << "handleCadDataReady: item not found for componentId:" << componentId
                   << ", m_componentIdIndex size:" << viewModel.m_componentIdIndex.size();
        return;
    }

    // 已经验证通过的项目只更新数据，避免重复触发验证完成和队列推进。
    if (item->isValid()) {
        item->setComponentData(QSharedPointer<ComponentData>::create(data));
        return;
    }

    item->setComponentData(QSharedPointer<ComponentData>::create(data));
    item->setFetching(false);
    item->setValid(true);
    item->setRetryable(true);
    item->setValidationPhase("completed");
    item->setErrorMessage("");
    viewModel.scheduleListUpdate();

    viewModel.m_validationStateManager->onComponentValidated(componentId);
    QTimer::singleShot(0, &viewModel, [&viewModel, componentId]() { viewModel.onValidationComplete(componentId); });
}

/**
 * @brief 将请求错误分类为 CAD 验证失败或预览图失败。
 * @param viewModel 元件列表视图模型。
 * @param componentId 元器件编号。
 * @param error 请求错误信息。
 */
void ComponentListDataCoordinator::handleFetchError(ComponentListViewModel& viewModel,
                                                    const QString& componentId,
                                                    const QString& error) {
    qWarning() << "Fetch error for:" << componentId << "-" << error;

    // 查找需要记录错误的列表项。
    auto item = viewModel.findItemData(componentId);
    if (!item) {
        return;
    }

    // 预览图失败不应改变已经完成的 CAD 验证状态。
    // 记录 CAD 验证是否已经完成。
    const bool wasAlreadyValid = item->isValid();
    item->setFetching(false);

    if (!wasAlreadyValid) {
        // 判断错误是否属于 CAD 数据失败。
        const bool isCadDataFailure = ComponentValidationErrorPolicy::isCadDataFailure(error);
        if (isCadDataFailure) {
            item->setValid(false);
            item->setValidationPhase("failed");
            // 判断该错误是否不应重试。
            const bool nonRetryable = ComponentValidationErrorPolicy::isNonRetryable(error);
            item->setRetryable(!nonRetryable);
            if (ComponentValidationErrorPolicy::isNotFound(error)) {
                item->setErrorMessage(QCoreApplication::translate("ComponentListViewModel", "元器件不存在（404）"));
            } else {
                item->setErrorMessage(error);
            }
            viewModel.m_validationStateManager->onComponentFailed(componentId);
            viewModel.scheduleListUpdate();
            QTimer::singleShot(
                0, &viewModel, [&viewModel, componentId]() { viewModel.onValidationComplete(componentId); });
        } else {
            // 预览图失败只更新提示文本，不改变 CAD 验证结果。
            switch (ComponentValidationErrorPolicy::classifyPreviewError(error)) {
                case ComponentValidationErrorPolicy::PreviewErrorKind::Timeout:
                    item->setErrorMessage(
                        QCoreApplication::translate("ComponentListViewModel", "预览图获取超时（网络不稳定）"));
                    break;
                case ComponentValidationErrorPolicy::PreviewErrorKind::NotFound:
                    item->setErrorMessage(QCoreApplication::translate("ComponentListViewModel", "预览图不存在"));
                    break;
                case ComponentValidationErrorPolicy::PreviewErrorKind::Forbidden:
                    item->setErrorMessage(QCoreApplication::translate("ComponentListViewModel", "预览图获取被拒绝"));
                    break;
                case ComponentValidationErrorPolicy::PreviewErrorKind::Other:
                    item->setErrorMessage(QCoreApplication::translate("ComponentListViewModel", "预览图获取失败"));
                    break;
            }
        }
    }

    viewModel.scheduleListUpdate();
}

/**
 * @brief 合并 LCSC 元数据并安排名称字段的批量刷新。
 * @param viewModel 元件列表视图模型。
 * @param componentId 元器件编号。
 * @param manufacturerPart 制造商料号。
 * @param datasheetUrl 数据手册地址。
 * @param imageUrls 预览图地址列表。
 */
void ComponentListDataCoordinator::handleLcscData(ComponentListViewModel& viewModel,
                                                  const QString& componentId,
                                                  const QString& manufacturerPart,
                                                  const QString& datasheetUrl,
                                                  const QStringList& imageUrls) {
    // 查找需要更新 LCSC 元数据的列表项。
    auto item = viewModel.findItemData(componentId);
    if (!item) {
        qWarning() << "Component" << componentId << "not found in list, cannot update LCSC data";
        return;
    }

    if (item->componentData()) {
        // 获取列表项中的共享元器件数据。
        auto data = item->componentData();
        if (!manufacturerPart.isEmpty()) {
            data->setManufacturerPart(manufacturerPart);
        }
        if (!datasheetUrl.isEmpty()) {
            data->setDatasheet(datasheetUrl);
            data->setDatasheetFormat(datasheetUrl.toLower().contains(".html") ? "html" : "pdf");
        }
        if (!imageUrls.isEmpty()) {
            data->setPreviewImages(imageUrls);
        }
    }

    if (!manufacturerPart.isEmpty()) {
        item->setNameSilent(manufacturerPart);
        viewModel.m_batchUpdateItems.add(item);
        if (!viewModel.m_batchUpdateTimer->isActive()) {
            viewModel.m_batchUpdateTimer->start();
        }
    }
}

/**
 * @brief 保存数据手册内容并在缺少格式时根据文件头推断格式。
 * @param viewModel 元件列表视图模型。
 * @param componentId 元器件编号。
 * @param datasheetData 数据手册内容。
 */
void ComponentListDataCoordinator::handleDatasheet(ComponentListViewModel& viewModel,
                                                   const QString& componentId,
                                                   const QByteArray& datasheetData) {
    // 查找需要更新数据手册的列表项。
    auto item = viewModel.findItemData(componentId);
    if (!item) {
        qWarning() << "Component" << componentId << "not found in list, cannot update datasheet data";
        return;
    }

    if (item->componentData()) {
        // 获取列表项中的共享元器件数据。
        auto data = item->componentData();
        data->setDatasheetData(datasheetData);
        if (data->datasheetFormat().isEmpty()) {
            data->setDatasheetFormat(datasheetData.startsWith("%PDF-") ? "pdf" : "html");
        }
    }
}

}  // namespace EasyKiConverter
