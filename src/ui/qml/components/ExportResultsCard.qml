import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQml.Models
import EasyKiconverter_Cpp_Version.src.ui.qml.styles 1.0

Loader {
    id: resultsLoader
    objectName: "exportResultsCard"
    // 外部依赖
    property var exportProgressController
    active: resultsLoader.exportProgressController ? (resultsLoader.exportProgressController.isExporting || resultsLoader.exportProgressController.resultsList.length > 0) : false
    visible: active  // 确保 Loader 在没有结果时不占用空间
    sourceComponent: Card {
        title: qsTranslate("MainWindow", "转换结果")
        resources: [
            // 防抖定时器，避免频繁调用 updateFilter()
            Timer {
                id: filterUpdateTimer
                interval: 50
                onTriggered: visualModel.updateFilter()
            },
            // 监听筛选模式变化，更新过滤
            Connections {
                target: resultsLoader.exportProgressController
                function onFilterModeChanged() {
                    filterUpdateTimer.restart();
                }
                function onResultsListChanged() {
                    filterUpdateTimer.restart();
                }
            }
        ]
        ColumnLayout {
            id: resultsContent
            width: parent.width
            spacing: AppStyle.spacing.md
            visible: true
            // 筛选滑块

            Rectangle {
                id: filterSegmentedControl
                objectName: "exportResultsFilterSegmentedControl"
                Layout.fillWidth: true
                Layout.preferredHeight: 40
                Layout.maximumWidth: 560
                Layout.alignment: Qt.AlignLeft
                color: AppStyle.isDarkMode ? Qt.rgba(255, 255, 255, 0.05) : Qt.rgba(0, 0, 0, 0.05)
                radius: AppStyle.radius.lg
                visible: resultsLoader.exportProgressController.resultsList.length > 0
                clip: true
                // 滑块背景指示器

                Rectangle {
                    id: sliderIndicator
                    width: (filterSegmentedControl.width - 8) / 4
                    height: filterSegmentedControl.height - 8
                    anchors.verticalCenter: parent.verticalCenter
                    radius: AppStyle.radius.md
                    color: {
                        var mode = resultsLoader.exportProgressController.filterMode;
                        if (mode === "exporting")
                            return AppStyle.colors.warning;
                        if (mode === "success")
                            return AppStyle.colors.success;
                        if (mode === "failed")
                            return AppStyle.colors.danger;
                        return AppStyle.colors.primary;
                    }

                    // 计算位置：根据 filterMode 移动

                    x: {
                        var step = (filterSegmentedControl.width - 8) / 4;
                        if (resultsLoader.exportProgressController.filterMode === "exporting")
                            return 4 + step;
                        if (resultsLoader.exportProgressController.filterMode === "success")
                            return 4 + step * 2;
                        if (resultsLoader.exportProgressController.filterMode === "failed")
                            return 4 + step * 3;
                        return 4;
                    }

                    Behavior on x {
                        NumberAnimation {
                            duration: 450
                            easing.type: AppStyle.easings.easeInOut
                        }
                    }

                    Behavior on color {
                        ColorAnimation {
                            duration: 450
                            easing.type: AppStyle.easings.easeInOut
                        }
                    }
                }

                Row {
                    anchors.fill: parent
                    anchors.margins: 4
                    // 全部

                    Item {
                        objectName: "exportResultsFilterAllButton"
                        width: (filterSegmentedControl.width - 8) / 4
                        height: parent.height
                        Text {
                            anchors.centerIn: parent
                            text: qsTr("全部 (%1)").arg(resultsLoader.exportProgressController.resultsList.length)
                            color: resultsLoader.exportProgressController.filterMode === "all" ? "#ffffff" : AppStyle.colors.textSecondary
                            font.pixelSize: AppStyle.fontSizes.sm
                            font.bold: resultsLoader.exportProgressController.filterMode === "all"
                            Behavior on color {
                                ColorAnimation {
                                    duration: AppStyle.durations.fast
                                }
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: resultsLoader.exportProgressController.setFilterMode("all")
                        }
                    }

                    // 导出中

                    Item {
                        objectName: "exportResultsFilterExportingButton"
                        width: (filterSegmentedControl.width - 8) / 4
                        height: parent.height
                        enabled: resultsLoader.exportProgressController.filteredPendingCount > 0
                        opacity: enabled ? 1.0 : 0.4
                        Text {
                            anchors.centerIn: parent
                            text: qsTr("导出中 (%1)").arg(resultsLoader.exportProgressController.filteredPendingCount)
                            color: resultsLoader.exportProgressController.filterMode === "exporting" ? "#ffffff" : AppStyle.colors.textSecondary
                            font.pixelSize: AppStyle.fontSizes.sm
                            font.bold: resultsLoader.exportProgressController.filterMode === "exporting"
                            Behavior on color {
                                ColorAnimation {
                                    duration: AppStyle.durations.fast
                                }
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                            onClicked: resultsLoader.exportProgressController.setFilterMode("exporting")
                        }
                    }

                    // 成功

                    Item {
                        objectName: "exportResultsFilterSuccessButton"
                        width: (filterSegmentedControl.width - 8) / 4
                        height: parent.height
                        enabled: resultsLoader.exportProgressController.successCount > 0
                        opacity: enabled ? 1.0 : 0.4
                        Text {
                            anchors.centerIn: parent
                            text: qsTr("成功 (%1)").arg(resultsLoader.exportProgressController.successCount)
                            color: resultsLoader.exportProgressController.filterMode === "success" ? "#ffffff" : AppStyle.colors.textSecondary
                            font.pixelSize: AppStyle.fontSizes.sm
                            font.bold: resultsLoader.exportProgressController.filterMode === "success"
                            Behavior on color {
                                ColorAnimation {
                                    duration: AppStyle.durations.fast
                                }
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                            onClicked: resultsLoader.exportProgressController.setFilterMode("success")
                        }
                    }

                    // 失败

                    Item {
                        objectName: "exportResultsFilterFailedButton"
                        width: (filterSegmentedControl.width - 8) / 4
                        height: parent.height
                        enabled: resultsLoader.exportProgressController.failureCount > 0
                        opacity: enabled ? 1.0 : 0.4
                        Text {
                            anchors.centerIn: parent
                            text: qsTr("失败 (%1)").arg(resultsLoader.exportProgressController.failureCount)
                            color: resultsLoader.exportProgressController.filterMode === "failed" ? "#ffffff" : AppStyle.colors.textSecondary
                            font.pixelSize: AppStyle.fontSizes.sm
                            font.bold: resultsLoader.exportProgressController.filterMode === "failed"
                            Behavior on color {
                                ColorAnimation {
                                    duration: AppStyle.durations.fast
                                }
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                            onClicked: resultsLoader.exportProgressController.setFilterMode("failed")
                        }
                    }
                }
            }

            // 结果列表（使用 GridView 实现五列显示）
            GridView {
                id: resultsList
                objectName: "exportResultsList"
                Layout.fillWidth: true
                Layout.minimumHeight: 200
                Layout.preferredHeight: visible ? ResponsiveHelper.responsive(300, 400, 500, 560) : 0
                Layout.topMargin: AppStyle.spacing.md
                visible: resultsLoader.exportProgressController.resultsList.length > 0
                clip: true
                // 启用虚拟化，缓存上下各一屏的项
                cacheBuffer: 500
                // 启用 Item 回收，减少创建/销毁开销
                reuseItems: true
                cellWidth: {
                    var w = width - AppStyle.spacing.md;
                    var minCellW = ResponsiveHelper.responsive(180, 220, 230, 250);
                    var c = Math.max(1, Math.floor(w / minCellW));
                    return w / c;
                }
                cellHeight: 80
                flow: GridView.FlowLeftToRight
                layoutDirection: Qt.LeftToRight
                // 使用 DelegateModel 来保持滚动位置
                model: visualModel
                ScrollBar.vertical: ScrollBar {
                    policy: ScrollBar.AsNeeded
                }
            }

            // DelegateModel 用于过滤，避免每次过滤都重置滚动位置
            DelegateModel {
                id: visualModel
                model: resultsLoader.exportProgressController ? resultsLoader.exportProgressController.resultsList : []
                groups: [
                    DelegateModelGroup {
                        id: displayGroup
                        includeByDefault: true
                        name: "display"
                    }
                ]
                filterOnGroup: "display"
                delegate: ResultListItem {
                    objectName: "exportResultItem_" + (modelData.componentId || "")
                    width: resultsList.cellWidth - AppStyle.spacing.md
                    anchors.horizontalCenter: parent ? undefined : undefined
                    componentId: modelData.componentId || ""
                    status: modelData.status || "pending"
                    message: modelData.error || modelData.message || ""
                    exportSymbol: modelData.symbolStatus !== "disabled"
                    exportFootprint: modelData.footprintStatus !== "disabled"
                    exportModel3D: modelData.model3DStatus !== "disabled"
                    exportPreviewImages: modelData.previewStatus !== "disabled"
                    exportDatasheet: modelData.datasheetStatus !== "disabled"
                    symbolSuccess: !!modelData.symbolSuccess
                    footprintSuccess: !!modelData.footprintSuccess
                    model3DSuccess: !!modelData.model3DSuccess
                    previewSuccess: !!modelData.previewSuccess
                    datasheetSuccess: !!modelData.datasheetSuccess
                    symbolStatus: modelData.symbolStatus || "pending"
                    footprintStatus: modelData.footprintStatus || "pending"
                    model3DStatus: modelData.model3DStatus || "pending"
                    previewStatus: modelData.previewStatus || "pending"
                    datasheetStatus: modelData.datasheetStatus || "pending"
                    retryEnabled: resultsLoader.exportProgressController ? resultsLoader.exportProgressController.retrySupported : true
                    onRetryClicked: resultsLoader.exportProgressController.retryComponent(componentId)
                    onDeleteClicked: resultsLoader.exportProgressController.removeResult(componentId)
                }

                // 过滤函数
                function updateFilter() {
                    var filterMode = resultsLoader.exportProgressController ? resultsLoader.exportProgressController.filterMode : "all";
                    for (var i = 0; i < items.count; i++) {
                        var item = items.get(i);
                        var model = item.model;
                        var dataObj = model.modelData || model;
                        var status = dataObj.status || "pending";
                        var passFilter = false;
                        if (filterMode === "all") {
                            passFilter = true;
                        } else if (filterMode === "exporting") {
                            passFilter = (status === "pending" || status === "in_progress");
                        } else if (filterMode === "success") {
                            passFilter = (status === "success");
                        } else if (filterMode === "failed") {
                            passFilter = (status === "failed");
                        }
                        item.inDisplay = passFilter;
                    }
                }
            }
        }
    }
}
