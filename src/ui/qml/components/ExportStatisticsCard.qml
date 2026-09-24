import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import EasyKiconverter_Cpp_Version.src.ui.qml.styles 1.0

Card {
    id: exportStatisticsCard
    // 外部依赖
    property var exportProgressController
    property var exportSettingsController
    ConfirmDialog {
        id: clearCacheDialog
        title: qsTranslate("MainWindow", "确认清空缓存")
        confirmText: qsTranslate("MainWindow", "移入回收站")
        cancelText: qsTranslate("MainWindow", "取消")
        message: qsTranslate("MainWindow", "将处理 %1 个已验证的 EasyKiConverter 缓存条目，并移入系统回收站。未知文件、目录和无法验证的内容会保留；如果回收站不可用，原数据不会被删除。是否继续？").arg(exportStatisticsCard.exportProgressController ? exportStatisticsCard.exportProgressController.cacheEntryCount() : 0)
        onAccepted: {
            if (exportStatisticsCard.exportProgressController)
                exportStatisticsCard.exportProgressController.clearCache();
        }
    }
    title: qsTranslate("MainWindow", "导出统计")
    visible: exportStatisticsCard.exportProgressController ? exportStatisticsCard.exportProgressController.hasCompletedExport : false
    ColumnLayout {
        width: parent.width
        spacing: AppStyle.spacing.md
        // 基本统计信息
        Text {
            text: qsTranslate("MainWindow", "基本统计")
            font.pixelSize: AppStyle.fontSizes.md
            font.bold: true
            color: AppStyle.colors.textPrimary
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: AppStyle.spacing.lg
            StatItem {
                label: qsTranslate("MainWindow", "总数")
                value: exportStatisticsCard.exportProgressController ? exportStatisticsCard.exportProgressController.totalCount : 0
                Layout.fillWidth: true
            }
            StatItem {
                label: qsTranslate("MainWindow", "成功")
                value: exportStatisticsCard.exportProgressController ? exportStatisticsCard.exportProgressController.successCount : 0
                valueColor: AppStyle.colors.success
                Layout.fillWidth: true
            }
            StatItem {
                label: qsTranslate("MainWindow", "失败")
                value: exportStatisticsCard.exportProgressController ? exportStatisticsCard.exportProgressController.failureCount : 0
                valueColor: AppStyle.colors.danger
                Layout.fillWidth: true
            }
            StatItem {
                label: qsTranslate("MainWindow", "成功率")
                value: {
                    var total = exportStatisticsCard.exportProgressController ? exportStatisticsCard.exportProgressController.totalCount : 0;
                    var success = exportStatisticsCard.exportProgressController ? exportStatisticsCard.exportProgressController.successCount : 0;
                    if (total === 0)
                        return "0%";
                    return ((success / total) * 100).toFixed(1) + "%";
                }
                Layout.fillWidth: true
            }
        }
        // 导出进度
        RowLayout {
            Layout.fillWidth: true
            spacing: AppStyle.spacing.lg
            StatItem {
                label: qsTranslate("MainWindow", "抓取进度")
                value: exportStatisticsCard.exportProgressController ? exportStatisticsCard.exportProgressController.fetchProgress + "%" : "0%"
                Layout.fillWidth: true
            }
            StatItem {
                label: qsTranslate("MainWindow", "处理进度")
                value: exportStatisticsCard.exportProgressController ? exportStatisticsCard.exportProgressController.processProgress + "%" : "0%"
                Layout.fillWidth: true
            }
            StatItem {
                label: qsTranslate("MainWindow", "写入进度")
                value: exportStatisticsCard.exportProgressController ? exportStatisticsCard.exportProgressController.writeProgress + "%" : "0%"
                Layout.fillWidth: true
            }
        }
        // 导出成果统计
        Text {
            text: qsTranslate("MainWindow", "导出详情")
            font.pixelSize: AppStyle.fontSizes.md
            font.bold: true
            color: AppStyle.colors.textPrimary
            Layout.topMargin: AppStyle.spacing.sm
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: AppStyle.spacing.lg
            StatItem {
                label: qsTranslate("MainWindow", "符号")
                value: exportStatisticsCard.exportProgressController ? (exportStatisticsCard.exportProgressController.symbolSuccessCount || 0) : 0
                Layout.fillWidth: true
            }
            StatItem {
                label: qsTranslate("MainWindow", "封装")
                value: exportStatisticsCard.exportProgressController ? (exportStatisticsCard.exportProgressController.footprintSuccessCount || 0) : 0
                Layout.fillWidth: true
            }
            StatItem {
                label: qsTranslate("MainWindow", "3D模型")
                value: exportStatisticsCard.exportProgressController ? (exportStatisticsCard.exportProgressController.model3DSuccessCount || 0) : 0
                Layout.fillWidth: true
            }
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: AppStyle.spacing.lg
            StatItem {
                label: qsTranslate("MainWindow", "预览图")
                value: exportStatisticsCard.exportProgressController ? (exportStatisticsCard.exportProgressController.previewSuccessCount || 0) : 0
                Layout.fillWidth: true
            }
            StatItem {
                label: qsTranslate("MainWindow", "手册")
                value: exportStatisticsCard.exportProgressController ? (exportStatisticsCard.exportProgressController.datasheetSuccessCount || 0) : 0
                Layout.fillWidth: true
            }
            Item {
                Layout.fillWidth: true
            }
        }
        // 底部按钮组（居中排列）
        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: AppStyle.spacing.sm
            spacing: AppStyle.spacing.lg
            // 打开输出目录按钮
            ModernButton {
                text: qsTranslate("MainWindow", "打开输出目录")
                iconName: "folder"
                backgroundColor: AppStyle.colors.surface
                textColor: AppStyle.colors.textPrimary
                hoverColor: AppStyle.colors.border
                pressedColor: AppStyle.colors.borderFocus
                onClicked: {
                    if (exportStatisticsCard.exportProgressController) {
                        exportStatisticsCard.exportProgressController.openLastExportedFolder();
                    }
                }
            }
            // 清空缓存按钮
            ModernButton {
                text: qsTranslate("MainWindow", "清空缓存")
                iconName: "trash"
                backgroundColor: AppStyle.colors.danger
                hoverColor: AppStyle.colors.dangerDark
                pressedColor: AppStyle.colors.danger
                onClicked: {
                    clearCacheDialog.open();
                }
            }
            // 打开缓存目录按钮（只在调试模式下显示）
            ModernButton {
                text: qsTranslate("MainWindow", "打开缓存目录")
                iconName: "folder"
                backgroundColor: AppStyle.colors.surface
                textColor: AppStyle.colors.textPrimary
                hoverColor: AppStyle.colors.border
                pressedColor: AppStyle.colors.borderFocus
                visible: exportStatisticsCard.exportSettingsController ? exportStatisticsCard.exportSettingsController.debugMode : false
                onClicked: {
                    if (exportStatisticsCard.exportProgressController) {
                        Qt.openUrlExternally(exportStatisticsCard.exportProgressController.cacheDirUrl);
                    }
                }
            }
        }
    }
}
