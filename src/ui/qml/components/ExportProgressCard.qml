import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import EasyKiconverter_Cpp_Version.src.ui.qml.styles 1.0

Card {
    id: exportProgressCard
    objectName: "exportProgressCard"
    // 外部依赖
    property var exportProgressController
    title: qsTranslate("MainWindow", "转换进度")
    visible: exportProgressCard.exportProgressController ? (exportProgressCard.exportProgressController.isExporting || exportProgressCard.exportProgressController.progress > 0) : false
    ColumnLayout {
        width: parent.width
        spacing: 12
        // 1. 流程指示器 (Step Indicators)
        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 8
            Layout.bottomMargin: 8
            spacing: 0
            // 步骤 1: 抓取
            StepItem {
                // 移除 Layout.fillWidth，让它保持最小宽度
                Layout.preferredWidth: implicitWidth
                label: qsTranslate("MainWindow", "数据抓取")
                index: 1
                progress: exportProgressCard.exportProgressController ? exportProgressCard.exportProgressController.fetchProgress : 0
                activeColor: "#22c55e" // 绿色
            }

            // 连接线 1-2
            Rectangle {
                Layout.fillWidth: true // 让线条占据所有剩余空间
                Layout.preferredHeight: 2
                Layout.alignment: Qt.AlignVCenter
                Layout.bottomMargin: 14
                color: (exportProgressCard.exportProgressController && exportProgressCard.exportProgressController.fetchProgress >= 100) ? AppStyle.colors.success : AppStyle.colors.border
                Behavior on color {
                    ColorAnimation {
                        duration: 300
                    }
                }
            }

            // 步骤 2: 处理
            StepItem {
                Layout.preferredWidth: implicitWidth
                label: qsTranslate("MainWindow", "数据处理")
                index: 2
                progress: exportProgressCard.exportProgressController ? exportProgressCard.exportProgressController.processProgress : 0
                activeColor: "#3b82f6" // 蓝色
            }

            // 连接线 2-3
            Rectangle {
                Layout.fillWidth: true // 让线条占据所有剩余空间
                Layout.preferredHeight: 2
                Layout.alignment: Qt.AlignVCenter
                Layout.bottomMargin: 14
                color: (exportProgressCard.exportProgressController && exportProgressCard.exportProgressController.processProgress >= 100) ? AppStyle.colors.success : AppStyle.colors.border
                Behavior on color {
                    ColorAnimation {
                        duration: 300
                    }
                }
            }

            // 步骤 3: 写入
            StepItem {
                Layout.preferredWidth: implicitWidth
                label: qsTranslate("MainWindow", "文件写入")
                index: 3
                progress: exportProgressCard.exportProgressController ? exportProgressCard.exportProgressController.writeProgress : 0
                activeColor: "#f59e0b" // 橙色
            }
        }

        // 2. 总进度 (多色拼接) - 改为水平布局
        RowLayout {
            Layout.fillWidth: true
            spacing: 12
            // 自定义多色进度条容器
            Rectangle {
                id: progressBar
                objectName: "exportProgressBar"
                Layout.fillWidth: true
                height: 12
                color: AppStyle.colors.border
                radius: AppStyle.radius.md
                clip: true
                // 移除 visible 限制，使其在完成后依然可见

                Row {
                    anchors.fill: parent
                    spacing: 0
                    // 抓取部分 (Green, 占比 1/3)
                    Rectangle {
                        height: parent.height
                        width: (parent.width / 3) * ((exportProgressCard.exportProgressController ? exportProgressCard.exportProgressController.fetchProgress : 0) / 100)
                        color: AppStyle.colors.success
                        visible: width > 0
                        Behavior on width {
                            NumberAnimation {
                                duration: 100
                            }
                        }
                    }

                    // 处理部分 (Blue, 占比 1/3)
                    Rectangle {
                        height: parent.height
                        width: (parent.width / 3) * ((exportProgressCard.exportProgressController ? exportProgressCard.exportProgressController.processProgress : 0) / 100)
                        color: AppStyle.colors.primary
                        visible: width > 0
                        Behavior on width {
                            NumberAnimation {
                                duration: 100
                            }
                        }
                    }

                    // 写入部分 (Orange, 占比 1/3)
                    Rectangle {
                        height: parent.height
                        width: (parent.width / 3) * ((exportProgressCard.exportProgressController ? exportProgressCard.exportProgressController.writeProgress : 0) / 100)
                        color: AppStyle.colors.warning
                        visible: width > 0
                        Behavior on width {
                            NumberAnimation {
                                duration: 100
                            }
                        }
                    }
                }
            }

            // 总进度文字 (放在右侧)
            Text {
                objectName: "exportProgressPercentLabel"
                text: Math.round(exportProgressCard.exportProgressController ? exportProgressCard.exportProgressController.progress : 0) + "%"
                font.pixelSize: AppStyle.fontSizes.sm
                font.bold: true
                color: AppStyle.colors.textPrimary
                Layout.alignment: Qt.AlignVCenter
            }
        }

        Text {
            id: statusLabel
            objectName: "exportStatusLabel"
            Layout.fillWidth: true
            text: exportProgressCard.exportProgressController ? exportProgressCard.exportProgressController.status : ""
            font.pixelSize: AppStyle.fontSizes.sm
            color: AppStyle.colors.textSecondary
            horizontalAlignment: Text.AlignHCenter
            visible: exportProgressCard.exportProgressController ? (exportProgressCard.exportProgressController.status ? exportProgressCard.exportProgressController.status.length > 0 : false) : false
        }
    }
}
