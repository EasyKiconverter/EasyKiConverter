import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import EasyKiconverter_Cpp_Version.src.ui.qml.styles 1.0

Rectangle {
    id: updateBanner
    property var updateChecker
    radius: AppStyle.radius.lg
    color: AppStyle.colors.surface
    border.width: AppStyle.borderWidths.thin
    border.color: AppStyle.colors.border
    visible: updateChecker ? (updateChecker.checking || (updateChecker.hasUpdate && !updateChecker.dismissed) || updateChecker.statusText === "failed") : false
    implicitHeight: visible ? bannerLayout.implicitHeight + AppStyle.spacing.lg * 2 : 0
    Behavior on implicitHeight {
        NumberAnimation {
            duration: 220
            easing.type: Easing.OutCubic
        }
    }

    RowLayout {
        id: bannerLayout
        anchors.fill: parent
        anchors.margins: AppStyle.spacing.lg
        spacing: AppStyle.spacing.lg
        Rectangle {
            Layout.preferredWidth: 4
            Layout.preferredHeight: 42
            radius: width / 2
            color: updateChecker && updateChecker.statusText === "failed" ? AppStyle.colors.danger : AppStyle.colors.primary
        }

        Rectangle {
            Layout.preferredWidth: 36
            Layout.preferredHeight: 36
            radius: width / 2
            color: AppStyle.colors.infoLight
            Text {
                anchors.centerIn: parent
                text: updateChecker && updateChecker.statusText === "failed" ? "!" : "i"
                color: AppStyle.colors.primary
                font.pixelSize: AppStyle.fontSizes.lg
                font.bold: true
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 4
            Text {
                text: updateChecker && updateChecker.checking ? qsTranslate("MainWindow", "正在检查更新") : updateChecker && updateChecker.statusText === "failed" ? qsTranslate("MainWindow", "更新检查失败") : qsTranslate("MainWindow", "发现新版本 %1").arg(updateChecker ? updateChecker.latestVersion : "")
                color: AppStyle.colors.textPrimary
                font.pixelSize: AppStyle.fontSizes.md
                font.bold: true
            }

            Text {
                text: updateChecker && updateChecker.checking ? qsTranslate("MainWindow", "正在从 GitHub 获取最新发布信息...") : updateChecker && updateChecker.statusText === "failed" ? qsTranslate("MainWindow", "更新检查失败，可稍后重试。") : updateChecker && updateChecker.releaseName && updateChecker.releaseName.length > 0 ? qsTranslate("MainWindow", "当前版本 %1，最新发布：%2").arg(updateChecker.currentVersion).arg(updateChecker.releaseName) : qsTranslate("MainWindow", "当前版本 %1，可前往 GitHub 查看发布说明。").arg(updateChecker ? updateChecker.currentVersion : "")
                color: AppStyle.colors.textSecondary
                font.pixelSize: AppStyle.fontSizes.sm
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
        }

        RowLayout {
            Layout.alignment: Qt.AlignVCenter
            spacing: AppStyle.spacing.xs
            ModernButton {
                objectName: "updateActionButton"
                text: updateChecker && updateChecker.statusText === "failed" ? qsTranslate("MainWindow", "重试") : qsTranslate("MainWindow", "打开 Release 页面")
                Layout.preferredHeight: 36
                backgroundColor: updateChecker && updateChecker.statusText === "failed" ? AppStyle.colors.warning : AppStyle.colors.primary
                hoverColor: updateChecker && updateChecker.statusText === "failed" ? AppStyle.colors.warningDark : AppStyle.colors.primaryHover
                onClicked: {
                    if (updateChecker && updateChecker.statusText === "failed") {
                        updateChecker.checkForUpdates();
                    } else if (updateChecker) {
                        Qt.openUrlExternally(updateChecker.assetUrl || updateChecker.releaseUrl);
                    }
                }
            }

            Button {
                objectName: "remindLaterButton"
                text: qsTranslate("MainWindow", "稍后提醒")
                visible: updateChecker && updateChecker.hasUpdate
                padding: AppStyle.spacing.sm
                contentItem: Text {
                    text: parent.text
                    color: AppStyle.colors.textSecondary
                    font.pixelSize: AppStyle.fontSizes.xs
                    verticalAlignment: Text.AlignVCenter
                    horizontalAlignment: Text.AlignHCenter
                }
                background: Rectangle {
                    color: parent.hovered ? AppStyle.colors.background : "transparent"
                    border.color: parent.hovered ? AppStyle.colors.borderHover : "transparent"
                    border.width: AppStyle.borderWidths.thin
                    radius: AppStyle.radius.sm
                }
                onClicked: {
                    if (updateChecker)
                        updateChecker.dismissUpdate();
                }
            }

            Button {
                objectName: "ignoreVersionButton"
                text: qsTranslate("MainWindow", "忽略此版本")
                visible: updateChecker && updateChecker.hasUpdate
                padding: AppStyle.spacing.sm
                contentItem: Text {
                    text: parent.text
                    color: AppStyle.colors.textSecondary
                    font.pixelSize: AppStyle.fontSizes.xs
                    verticalAlignment: Text.AlignVCenter
                    horizontalAlignment: Text.AlignHCenter
                }
                background: Rectangle {
                    color: parent.hovered ? AppStyle.colors.background : "transparent"
                    border.color: parent.hovered ? AppStyle.colors.borderHover : "transparent"
                    border.width: AppStyle.borderWidths.thin
                    radius: AppStyle.radius.sm
                }
                onClicked: {
                    if (updateChecker)
                        updateChecker.ignoreUpdate();
                }
            }
        }
    }
}
