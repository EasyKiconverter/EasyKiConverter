#include "ComponentListClipboardCoordinator.h"

#include "ComponentListViewModel.h"

#include <QClipboard>
#include <QGuiApplication>

namespace EasyKiConverter {

/**
 * @brief 从剪贴板文本提取编号、过滤重复项并发起批量添加。
 * @param owner 元件列表视图模型。
 */
void ComponentListClipboardCoordinator::paste(ComponentListViewModel& owner) {
    // 获取系统剪贴板对象。
    QClipboard* clipboard = QGuiApplication::clipboard();
    // 读取剪贴板中的原始文本。
    const QString text = clipboard->text();

    if (text.isEmpty()) {
        qWarning() << "Clipboard is empty";
        return;
    }

    // 从剪贴板文本中提取元器件编号。
    const QStringList extractedIds = owner.extractComponentIdFromText(text);
    if (extractedIds.isEmpty()) {
        qWarning() << "No valid component IDs found in clipboard";
        emit owner.pasteCompleted(0, 0);
        return;
    }

    // 统计已存在而被跳过的编号数量。
    int skipped = 0;
    // 保存待新增的元器件编号。
    QStringList newIds;
    for (const QString& id : extractedIds) {
        if (owner.componentExists(id)) {
            skipped++;
        } else {
            newIds.append(id);
        }
    }

    if (!newIds.isEmpty()) {
        owner.addComponentsBatch(newIds);
    }

    emit owner.pasteCompleted(newIds.count(), skipped);
}

/**
 * @brief 将列表中仍然有效的元件编号按换行格式复制到剪贴板。
 * @param owner 元件列表视图模型。
 */
void ComponentListClipboardCoordinator::copyAll(ComponentListViewModel& owner) {
    if (owner.m_componentList.isEmpty()) {
        qWarning() << "Component list is empty, nothing to copy";
        return;
    }

    // 收集当前列表中的元器件编号。
    QStringList componentIds;
    for (const auto* item : owner.m_componentList) {
        if (item) {
            componentIds.append(item->componentId());
        }
    }

    // 获取系统剪贴板对象。
    QClipboard* clipboard = QGuiApplication::clipboard();
    clipboard->setText(componentIds.join("\n"));

    qDebug() << "Copied" << componentIds.size() << "component IDs to clipboard";
}

}  // namespace EasyKiConverter
