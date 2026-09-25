import QtQuick
import QtQuick.Layouts
import "../styles"

/**
 * @brief 使用项目统一滑块对话框样式显示版本信息和更新操作。
 *
 * 版本号位于标题栏，因此更新相关操作集中在此对话框中，侧边栏只保留导出设置。
 */
SliderDialogBase {
    id: root
    hasOverlay: true
    title: qsTranslate("MainWindow", "版本更新")
    message: updateChecker ? qsTranslate("MainWindow", "当前版本 %1").arg(updateChecker.currentVersion) : ""
    property var updateChecker
    buttonSpecs: [
        {
            text: updateChecker && updateChecker.statusText === "failed" ? qsTranslate("MainWindow", "重试") : qsTranslate("MainWindow", "检查更新"),
            color: updateChecker && updateChecker.statusText === "failed" ? AppStyle.colors.warning : AppStyle.colors.primary,
            objectName: "dialogUpdateActionButton",
            action: function () {
                if (updateChecker)
                    updateChecker.checkForUpdates();
            }
        },
        {
            isSeparator: true,
            visible: updateChecker && updateChecker.hasUpdate
        },
        {
            text: qsTranslate("MainWindow", "打开 Release 页面"),
            color: AppStyle.colors.textSecondary,
            objectName: "dialogViewReleaseButton",
            visible: updateChecker && updateChecker.hasUpdate,
            action: function () {
                if (updateChecker) {
                    Qt.openUrlExternally(updateChecker.releaseUrl);
                    root.close();
                }
            }
        },
        {
            isSeparator: true,
            visible: updateChecker && updateChecker.statusText === "failed"
        },
        {
            text: qsTranslate("MainWindow", "打开 Release 页面"),
            color: AppStyle.colors.textSecondary,
            objectName: "dialogOpenLatestReleaseButton",
            visible: updateChecker && updateChecker.statusText === "failed",
            action: function () {
                if (updateChecker) {
                    var url = updateChecker.releaseUrl && updateChecker.releaseUrl.length > 0 ? updateChecker.releaseUrl : updateChecker.latestReleasePageUrl;
                    Qt.openUrlExternally(url);
                    root.close();
                }
            }
        },
        {
            isSeparator: true,
            visible: updateChecker && updateChecker.hasUpdate
        },
        {
            text: qsTranslate("ExitDialog", "取消"),
            color: AppStyle.colors.textSecondary,
            objectName: "closeUpdateDialogButton",
            action: function () {
                root.closeWithAnimation();
            }
        }
    ]
    mainContentSource: ColumnLayout {
        spacing: AppStyle.spacing.md
        Text {
            Layout.fillWidth: true
            text: {
                if (!root.updateChecker)
                    return "";
                if (root.updateChecker.statusText === "checking")
                    return qsTranslate("MainWindow", "正在检查更新");
                if (root.updateChecker.statusText === "failed")
                    return qsTranslate("MainWindow", "更新检查失败，可稍后重试。");
                if (root.updateChecker.statusText === "up_to_date")
                    return qsTranslate("MainWindow", "已是最新版本");
                if (root.updateChecker.statusText === "ignored")
                    return qsTranslate("MainWindow", "已忽略当前版本");
                if (root.updateChecker.statusText === "update_available")
                    return qsTranslate("MainWindow", "发现新版本 %1").arg(root.updateChecker.latestVersion);
                return qsTranslate("MainWindow", "尚未检查");
            }
            color: root.updateChecker && root.updateChecker.statusText === "failed" ? AppStyle.colors.danger : AppStyle.colors.textPrimary
            font.pixelSize: AppStyle.fontSizes.md
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
        }

        Text {
            Layout.fillWidth: true
            text: {
                if (!root.updateChecker)
                    return "";
                if (root.updateChecker.error && root.updateChecker.statusText === "failed")
                    return root.updateChecker.rateLimited ? qsTranslate("MainWindow", "GitHub 暂时限制了更新检查，请稍后重试。") : qsTranslate("MainWindow", "更新检查失败，请稍后重试。");
                if (root.updateChecker.releaseName)
                    return qsTranslate("MainWindow", "当前版本 %1，最新发布：%2").arg(root.updateChecker.currentVersion).arg(root.updateChecker.releaseName);
                return qsTranslate("MainWindow", "当前版本 %1，可前往 GitHub 查看发布说明。").arg(root.updateChecker.currentVersion);
            }
            color: AppStyle.colors.textSecondary
            font.pixelSize: AppStyle.fontSizes.sm
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
        }
    }

    extraContentSource: SidebarToggleRow {
        label: qsTranslate("MainWindow", "启动时自动检查")
        checked: root.updateChecker ? root.updateChecker.autoCheckEnabled : false
        onToggled: checked => {
            if (root.updateChecker)
                root.updateChecker.setAutoCheckEnabled(checked);
        }
    }
}
