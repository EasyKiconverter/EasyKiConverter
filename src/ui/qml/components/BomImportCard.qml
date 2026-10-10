import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import EasyKiconverter_Cpp_Version.src.ui.qml.styles 1.0

Card {
    id: bomImportCard
    // 外部依赖
    property var componentListController
    property var exportSettingsController
    // 信号：请求打开BOM文件对话框
    signal openBomFileDialog
    title: qsTranslate("MainWindow", "导入BOM文件")
    RowLayout {
        width: parent.width
        spacing: 12
        ModernButton {
            text: qsTranslate("MainWindow", "选择BOM文件")
            iconName: "upload"
            backgroundColor: AppStyle.colors.success
            hoverColor: AppStyle.colors.successDark
            pressedColor: AppStyle.colors.successDark
            onClicked: {
                bomImportCard.openBomFileDialog();
            }
        }
        Text {
            id: bomFileLabel
            Layout.fillWidth: true
            text: bomImportCard.componentListController && bomImportCard.componentListController.bomFilePath && bomImportCard.componentListController.bomFilePath.length > 0 ? bomImportCard.componentListController.bomFilePath.split("/").pop() : qsTranslate("MainWindow", "未选择文件")
            font.pixelSize: AppStyle.fontSizes.sm
            color: AppStyle.colors.textSecondary
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideMiddle
        }
    }
    RowLayout {
        width: parent.width
        spacing: 12
        CheckBox {
            id: weakNetworkCheckbox
            Layout.fillWidth: false
            text: qsTranslate("MainWindow", "客户端弱网络适配")
            checked: bomImportCard.exportSettingsController ? bomImportCard.exportSettingsController.weakNetworkSupport : false
            onCheckedChanged: {
                if (bomImportCard.exportSettingsController) {
                    bomImportCard.exportSettingsController.setWeakNetworkSupport(checked);
                }
            }
            font.pixelSize: AppStyle.fontSizes.sm
            ToolTip.visible: hovered
            ToolTip.text: qsTranslate("MainWindow", "仅影响本机网络请求策略，不代表服务器限流；开启后会使用更保守的并发、超时和重试配置")
            ToolTip.delay: 500
            indicator: Rectangle {
                implicitWidth: 22
                implicitHeight: 22
                x: weakNetworkCheckbox.leftPadding
                y: parent.height / 2 - height / 2
                radius: AppStyle.radius.xs
                color: weakNetworkCheckbox.checked ? AppStyle.colors.primary : "transparent"
                border.color: weakNetworkCheckbox.checked ? AppStyle.colors.primary : AppStyle.colors.textSecondary
                border.width: AppStyle.borderWidths.normal
                Behavior on color {
                    ColorAnimation {
                        duration: 150
                    }
                }
                Behavior on border.color {
                    ColorAnimation {
                        duration: 150
                    }
                }

                Text {
                    anchors.centerIn: parent
                    text: "✓"
                    font.pixelSize: AppStyle.fontSizes.sm
                    color: AppStyle.colors.textOnPrimary
                    visible: weakNetworkCheckbox.checked
                }
            }

            contentItem: Text {
                text: parent.text
                font: parent.font
                color: AppStyle.colors.textPrimary
                verticalAlignment: Text.AlignVCenter
                leftPadding: parent.indicator.width + parent.spacing
            }
        }

        Text {
            Layout.fillWidth: true
            text: qsTranslate("MainWindow", "导入 BOM 前可先开启，验证和预览图加载也会使用该策略")
            font.pixelSize: AppStyle.fontSizes.xs
            color: AppStyle.colors.textSecondary
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
        }
    }
    // BOM导入结果
    Text {
        id: bomResultLabel
        Layout.fillWidth: true
        Layout.topMargin: AppStyle.spacing.md
        text: bomImportCard.componentListController ? bomImportCard.componentListController.bomResult : ""
        font.pixelSize: AppStyle.fontSizes.sm
        color: AppStyle.colors.success
        horizontalAlignment: Text.AlignHCenter
        visible: bomImportCard.componentListController ? (bomImportCard.componentListController.bomResult ? bomImportCard.componentListController.bomResult.length > 0 : false) : false
    }
}
