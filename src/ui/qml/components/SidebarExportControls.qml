import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../styles"

Rectangle {
    id: root
    property var exportProgressController
    property var exportSettingsController
    property var exportTargetModel
    property var componentListController
    readonly property bool isExporting: exportProgressController ? exportProgressController.isExporting : false
    readonly property int failureCount: exportProgressController ? exportProgressController.failureCount : 0
    readonly property bool hasCompletedExport: exportProgressController ? exportProgressController.hasCompletedExport : false
    implicitHeight: controlColumn.implicitHeight + AppStyle.spacing.lg * 2
    color: AppStyle.isDarkMode ? Qt.rgba(30 / 255, 41 / 255, 59 / 255, 0.5) : Qt.rgba(241 / 255, 245 / 255, 249 / 255, 0.5)
    ColumnLayout {
        id: controlColumn
        anchors.fill: parent
        anchors.margins: AppStyle.spacing.lg
        spacing: AppStyle.spacing.md
        RowLayout {
            Layout.fillWidth: true
            spacing: AppStyle.spacing.md
            ModernButton {
                Layout.fillWidth: true
                Layout.preferredHeight: 44
                text: {
                    if (root.isExporting)
                        return qsTranslate("MainWindow", "导出中");
                    if (root.failureCount > 0)
                        return qsTranslate("MainWindow", "重试失败");
                    return qsTranslate("MainWindow", "开始导出");
                }
                backgroundColor: root.failureCount > 0 ? AppStyle.colors.warning : AppStyle.colors.primary
                enabled: {
                    if (root.isExporting)
                        return false;
                    if (!root.componentListController || root.componentListController.componentCount <= 0)
                        return false;
                    if (!root.exportSettingsController)
                        return false;
                    return root.exportSettingsController.exportSymbol || root.exportSettingsController.exportFootprint || root.exportSettingsController.exportModel3D;
                }
                onClicked: {
                    var pc = root.exportProgressController;
                    var sc = root.exportSettingsController;
                    var lc = root.componentListController;
                    var tm = root.exportTargetModel;
                    var tf = tm ? tm.currentTargetFormat : 0;
                    if (pc && pc.failureCount > 0) {
                        pc.retryFailedComponents();
                    } else if (pc && sc && lc) {
                        pc.startExport(lc.getAllComponentIds(), sc.outputPath || "", sc.libName || "", sc.exportSymbol || false, sc.exportFootprint || false, sc.exportModel3D || false, sc.exportModel3DFormat || 3, sc.exportModel3DPathMode || 0, sc.exportPreviewImages || false, sc.exportDatasheet || false, sc.overwriteExistingFiles || false, (sc.exportMode || 0) === 1, sc.debugMode || false, sc.symbolLibraryDescription || "", sc.footprintLibraryDescription || "", sc.footprintLibraryKeywords || "", tf);
                    }
                }

                ToolTip.visible: hovered && !enabled
                ToolTip.text: {
                    if (root.isExporting)
                        return qsTranslate("MainWindow", "正在导出中...");
                    if (!root.componentListController || root.componentListController.componentCount <= 0)
                        return qsTranslate("MainWindow", "请先添加元器件");
                    if (!root.exportSettingsController)
                        return "";
                    if (!root.exportSettingsController.exportSymbol && !root.exportSettingsController.exportFootprint && !root.exportSettingsController.exportModel3D)
                        return qsTranslate("MainWindow", "请至少选择一种导出类型");
                    return "";
                }
                ToolTip.delay: 600
            }

            ModernButton {
                visible: root.isExporting
                Layout.preferredWidth: 80
                Layout.preferredHeight: 44
                iconName: "close"
                backgroundColor: AppStyle.colors.danger
                onClicked: {
                    if (root.exportProgressController)
                        root.exportProgressController.cancelExport();
                }
            }
        }

        ModernButton {
            Layout.fillWidth: true
            Layout.preferredHeight: 36
            visible: root.hasCompletedExport
            text: qsTranslate("MainWindow", "打开导出目录")
            iconName: "folder"
            backgroundColor: AppStyle.colors.textSecondary
            onClicked: {
                if (root.exportProgressController)
                    root.exportProgressController.openLastExportedFolder();
            }
        }
    }
}
