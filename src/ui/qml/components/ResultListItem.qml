import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import EasyKiconverter_Cpp_Version.src.ui.qml.styles 1.0

Rectangle {
    id: item
    property string componentId
    property string status
    property string message
    property bool exportSymbol: true
    property bool exportFootprint: true
    property bool exportModel3D: true
    property bool exportPreviewImages: false
    property bool exportDatasheet: false
    property bool symbolSuccess: false
    property bool footprintSuccess: false
    property bool model3DSuccess: false
    property bool previewSuccess: false
    property bool datasheetSuccess: false
    property string symbolStatus: "pending"
    property string footprintStatus: "pending"
    property string model3DStatus: "pending"
    property string previewStatus: "pending"
    property string datasheetStatus: "pending"
    property bool retryEnabled: true
    signal retryClicked
    signal copyClicked
    signal deleteClicked
    function badgeColor(typeStatus) {
        if (typeStatus === "success")
            return AppStyle.colors.success;
        if (typeStatus === "failed")
            return AppStyle.colors.danger;
        if (typeStatus === "in_progress")
            return AppStyle.colors.warning;
        if (typeStatus === "skipped" || typeStatus === "disabled")
            return AppStyle.colors.textSecondary;
        return AppStyle.colors.border;
    }

    function statusText(typeStatus, success) {
        if (typeStatus === "success")
            return qsTr("已导出");
        if (typeStatus === "failed")
            return qsTr("失败");
        if (typeStatus === "in_progress")
            return qsTr("处理中");
        if (typeStatus === "skipped")
            return qsTr("已跳过");
        if (typeStatus === "disabled")
            return qsTr("未启用");
        if (success)
            return qsTr("已导出");
        return qsTr("等待中");
    }

    function overallIcon() {
        if (status === "success")
            return "🌟";
        if (status === "failed")
            return "❌";
        if (status === "in_progress")
            return "⚙️";
        if (status === "skipped")
            return "⏭";
        return "⏳";
    }

    height: 72
    color: itemHover.hovered ? AppStyle.colors.background : AppStyle.colors.surface
    radius: AppStyle.radius.md
    border.color: AppStyle.colors.border
    border.width: AppStyle.borderWidths.thin
    clip: true
    Behavior on color {
        ColorAnimation {
            duration: AppStyle.durations.fast
            easing.type: AppStyle.easings.easeOut
        }
    }

    TextEdit {
        id: copyHelper
        visible: false
        text: ""
    }

    ToolTip {
        id: copyFeedback
        parent: item
        x: parent.width - width - 8
        y: 4
        delay: 0
        timeout: 1500
        visible: false
        text: ""
        background: Rectangle {
            color: AppStyle.isDarkMode ? "#1e293b" : "#ffffff"
            radius: AppStyle.radius.md
            border.width: AppStyle.borderWidths.thin
            border.color: AppStyle.isDarkMode ? Qt.rgba(255, 255, 255, 0.1) : Qt.rgba(0, 0, 0, 0.1)
            layer.enabled: true
            layer.effect: MultiEffect {
                shadowEnabled: true
                shadowBlur: 8
                shadowColor: AppStyle.isDarkMode ? "#00000088" : "#33000000"
                shadowVerticalOffset: 2
            }
        }
        contentItem: Row {
            spacing: 4
            Text {
                text: "✓"
                color: AppStyle.colors.success
                font.bold: true
            }
            Text {
                text: qsTr("已复制")
                color: AppStyle.isDarkMode ? "#ffffff" : "#1e293b"
            }
        }
    }

    Timer {
        id: copyFeedbackTimer
        interval: 1500
        onTriggered: copyFeedback.visible = false
    }

    HoverHandler {
        id: itemHover
    }

    // 共享复制和外链点击判定，提示动画仍由本列表项保留。
    ListItemInteraction {
        id: itemInteraction
        anchors.fill: parent
        itemId: componentId
        onCopyClicked: {
            copyHelper.text = componentId;
            copyHelper.selectAll();
            copyHelper.copy();
            item.copyClicked();
            copyFeedback.visible = true;
            copyFeedbackTimer.start();
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: AppStyle.spacing.lg
        anchors.rightMargin: AppStyle.spacing.lg
        spacing: AppStyle.spacing.md
        Text {
            Layout.preferredWidth: 20
            Layout.preferredHeight: 20
            font.pixelSize: AppStyle.fontSizes.md
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            text: item.overallIcon()
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: AppStyle.spacing.xs
            RowLayout {
                Layout.fillWidth: true
                spacing: AppStyle.spacing.sm
                Text {
                    text: componentId
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                    font.pixelSize: AppStyle.fontSizes.sm
                    font.bold: true
                    font.family: "Courier New"
                    color: AppStyle.colors.textPrimary
                }

                Row {
                    spacing: 4
                    visible: exportSymbol || exportFootprint || exportModel3D || exportPreviewImages || exportDatasheet
                    Rectangle {
                        width: 14
                        height: 14
                        radius: width / 2
                        visible: exportSymbol
                        color: item.badgeColor(symbolStatus)
                        Text {
                            anchors.centerIn: parent
                            text: "S"
                            font.pixelSize: AppStyle.fontSizes.xxs
                            color: AppStyle.colors.textOnPrimary
                            font.bold: true
                        }
                        ToolTip.text: qsTr("符号: %1").arg(item.statusText(symbolStatus, symbolSuccess))
                        ToolTip.delay: 400
                        HoverHandler {
                            id: symHH
                        }
                        ToolTip.visible: symHH.hovered
                    }

                    Rectangle {
                        width: 14
                        height: 14
                        radius: width / 2
                        visible: exportFootprint
                        color: item.badgeColor(footprintStatus)
                        Text {
                            anchors.centerIn: parent
                            text: "F"
                            font.pixelSize: AppStyle.fontSizes.xxs
                            color: AppStyle.colors.textOnPrimary
                            font.bold: true
                        }
                        ToolTip.text: qsTr("封装: %1").arg(item.statusText(footprintStatus, footprintSuccess))
                        ToolTip.delay: 400
                        HoverHandler {
                            id: ftHH
                        }
                        ToolTip.visible: ftHH.hovered
                    }

                    Rectangle {
                        width: 14
                        height: 14
                        radius: width / 2
                        visible: exportModel3D
                        color: item.badgeColor(model3DStatus)
                        Text {
                            anchors.centerIn: parent
                            text: "3"
                            font.pixelSize: AppStyle.fontSizes.xxs
                            color: AppStyle.colors.textOnPrimary
                            font.bold: true
                        }
                        ToolTip.text: qsTr("3D模型: %1").arg(item.statusText(model3DStatus, model3DSuccess))
                        ToolTip.delay: 400
                        HoverHandler {
                            id: m3HH
                        }
                        ToolTip.visible: m3HH.hovered
                    }

                    Rectangle {
                        width: 14
                        height: 14
                        radius: width / 2
                        visible: exportPreviewImages
                        color: item.badgeColor(previewStatus)
                        Text {
                            anchors.centerIn: parent
                            text: "P"
                            font.pixelSize: AppStyle.fontSizes.xxs
                            color: AppStyle.colors.textOnPrimary
                            font.bold: true
                        }
                        ToolTip.text: qsTr("预览图: %1").arg(item.statusText(previewStatus, previewSuccess))
                        ToolTip.delay: 400
                        HoverHandler {
                            id: prevHH
                        }
                        ToolTip.visible: prevHH.hovered
                    }

                    Rectangle {
                        width: 14
                        height: 14
                        radius: width / 2
                        visible: exportDatasheet
                        color: item.badgeColor(datasheetStatus)
                        Text {
                            anchors.centerIn: parent
                            text: "D"
                            font.pixelSize: AppStyle.fontSizes.xxs
                            color: AppStyle.colors.textOnPrimary
                            font.bold: true
                        }
                        ToolTip.text: qsTr("手册: %1").arg(item.statusText(datasheetStatus, datasheetSuccess))
                        ToolTip.delay: 400
                        HoverHandler {
                            id: datasheetHH
                        }
                        ToolTip.visible: datasheetHH.hovered
                    }
                }
            }

            Text {
                Layout.fillWidth: true
                text: message
                font.pixelSize: AppStyle.fontSizes.xs
                color: AppStyle.colors.textSecondary
                visible: message.length > 0
                wrapMode: Text.WordWrap
            }
        }

        Item {
            id: retryButton
            objectName: "retryResultButton"
            visible: status === "failed"
            opacity: retryEnabled ? 1 : 0.45
            Layout.preferredWidth: 28
            Layout.preferredHeight: 28
            Layout.alignment: Qt.AlignVCenter
            Rectangle {
                anchors.fill: parent
                color: retryButtonMouseArea.containsMouse ? AppStyle.colors.warningLight : "transparent"
                radius: AppStyle.radius.sm
            }
            Text {
                anchors.centerIn: parent
                text: "↻"
                font.pixelSize: AppStyle.fontSizes.xxl
                font.bold: true
                color: AppStyle.colors.warning
            }
            ToolTip.text: qsTr("重试")
            ToolTip.delay: 500
            HoverHandler {
                id: retryHH
            }
            ToolTip.visible: retryHH.hovered
            MouseArea {
                id: retryButtonMouseArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                acceptedButtons: Qt.LeftButton
                enabled: item.retryEnabled
                onClicked: item.retryClicked()
            }
        }

        Item {
            id: deleteButton
            objectName: "deleteResultButton"
            visible: status === "failed"
            Layout.preferredWidth: 28
            Layout.preferredHeight: 28
            Layout.alignment: Qt.AlignVCenter
            Rectangle {
                anchors.fill: parent
                color: deleteButtonMouseArea.containsMouse ? AppStyle.colors.dangerLight : "transparent"
                radius: AppStyle.radius.sm
            }
            Text {
                anchors.centerIn: parent
                text: "×"
                font.pixelSize: AppStyle.fontSizes.xxl
                font.bold: true
                color: AppStyle.colors.danger
            }
            ToolTip.text: qsTr("删除")
            ToolTip.delay: 500
            HoverHandler {
                id: deleteHH
            }
            ToolTip.visible: deleteHH.hovered
            MouseArea {
                id: deleteButtonMouseArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                acceptedButtons: Qt.LeftButton
                onClicked: item.deleteClicked()
            }
        }
    }
}
