import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import QtQml.Models
import EasyKiconverter_Cpp_Version.src.ui.qml.styles 1.0

Card {
    id: componentListCard
    // 外部依赖
    property var componentListController
    property var exportProgressController
    readonly property bool showAttentionHint: showValidationHint || showPreviewHint
    readonly property bool showValidationHint: componentListController ? componentListController.validationReadyHint : false
    readonly property bool showPreviewHint: componentListController ? componentListController.previewReadyHint : false
    readonly property color hintColor: showPreviewHint ? "#31c36b" : (showValidationHint ? "#2f7ef8" : "transparent")
    property string editingDescriptionComponentId: ""
    // 导出状态查找表（由 exportProgressController 驱动）
    property var exportStatusMap: ({})
    // 外部传入的导出状态（由 MainWindow 绑定）
    property bool isExporting: false
    title: qsTranslate("MainWindow", "元器件列表")
    isCollapsed: componentListController ? componentListController.componentCount === 0 : true
    // 导出开始时自动折叠（结束后保持折叠，用户点击标题可手动展开）
    onIsExportingChanged: {
        if (isExporting) {
            isCollapsed = true;
        }
    }
    resources: [
        // 防抖定时器，避免频繁调用 updateFilter()
        Timer {
            id: filterUpdateDebounceTimer
            interval: 120
            onTriggered: componentListView.updateFilter()
        },
        Timer {
            id: previewPrefetchTimer
            interval: 120
            repeat: false
            onTriggered: {
                if (componentListCard.componentListController) {
                    componentListCard.componentListController.fetchPreviewImages(componentListCard.componentListController.getAllComponentIds());
                }
            }
        },
        // 监听组件数量变化，自动展开
        Connections {
            target: componentListCard.componentListController
            // 组件数量变化后自动展开列表卡片。
            function onComponentCountChanged() {
                if (componentListCard.componentListController.componentCount > 0) {
                    componentListCard.isCollapsed = false;
                }
            }
        },
        // 监听筛选模式变化，自动更新过滤
        Connections {
            target: componentListCard.componentListController
            // 筛选模式变化后重新计算显示项。
            function onFilterModeChanged() {
                filterUpdateDebounceTimer.restart();
            }
        },
        // 监听筛选数量变化，自动更新过滤
        Connections {
            target: componentListCard.componentListController
            // 筛选数量变化后重新计算显示项。
            function onFilteredCountChanged() {
                filterUpdateDebounceTimer.restart();
            }
        },
        Connections {
            target: componentListCard.componentListController
            // 预览图请求到达后启动预取定时器。
            function onPreviewFetchRequested() {
                previewPrefetchTimer.restart();
            }
        },
        // 监听导出结果变化，更新导出状态查找表
        Connections {
            target: componentListCard.exportProgressController
            // 导出结果列表变化后刷新状态查找表。
            function onResultsListChanged() {
                componentListCard.updateExportStatusMap();
            }
            // 导出状态变化后刷新状态查找表。
            function onIsExportingChanged() {
                componentListCard.updateExportStatusMap();
            }
        },
        Connections {
            target: componentListCard.componentListController
            // 清空元件列表时重置搜索文本和导出状态。
            function onListCleared() {
                componentToolbar.clearSearch();
                if (componentListCard.exportProgressController) {
                    componentListCard.exportProgressController.resetExport(); // 重置导出状态
                }
            }
        },
        Connections {
            target: componentToolbar
            // 搜索文本变化后重新计算过滤结果。
            function onSearchChanged() {
                filterUpdateDebounceTimer.restart();
            }
        },
        Connections {
            target: componentListCard.exportProgressController
            // 导出开始后移除元件列表中的提示状态。
            function onIsExportingChanged() {
                if (componentListCard.exportProgressController && componentListCard.exportProgressController.isExporting && componentListCard.componentListController) {
                    componentListCard.componentListController.dismissAttentionHints();
                }
            }
        },
        DescriptionEditDialog {
            id: descriptionDialog
            parent: componentListCard.Window.window ? componentListCard.Window.window.contentItem : componentListCard
            // 接受编辑结果后保存元件描述并清除编辑目标。
            onAccepted: function (description) {
                if (componentListCard.componentListController && componentListCard.editingDescriptionComponentId !== "") {
                    componentListCard.componentListController.updateComponentDescription(componentListCard.editingDescriptionComponentId, description);
                }
                componentListCard.editingDescriptionComponentId = "";
            }
            onRejected: componentListCard.editingDescriptionComponentId = ""
        },
        // 窗口尺寸变化时重新定位预览弹窗；Connections 必须放入 resources，避免被 Card 的默认内容属性接收。
        Connections {
            target: componentListCard.Window.window
            // 窗口宽度变化时重新定位预览弹窗。
            function onWidthChanged() {
                if (_previewPopup.visible)
                    Qt.callLater(positionPreviewPopup);
            }
            // 窗口高度变化时重新定位预览弹窗。
            function onHeightChanged() {
                if (_previewPopup.visible)
                    Qt.callLater(positionPreviewPopup);
            }
        }
    ]
    overlayContent: [
        Rectangle {
            anchors.fill: parent
            anchors.margins: -12
            radius: componentListCard.radius + 12
            color: "transparent"
            border.width: componentListCard.showAttentionHint ? 4 : 0
            border.color: Qt.alpha(componentListCard.hintColor, 0.42)
            opacity: componentListCard.showAttentionHint ? 0.75 : 0.0
            visible: opacity > 0
            SequentialAnimation on opacity {
                loops: Animation.Infinite
                running: componentListCard.showAttentionHint
                NumberAnimation {
                    from: 0.24
                    to: 0.78
                    duration: 900
                    easing.type: Easing.InOutQuad
                }
                NumberAnimation {
                    from: 0.78
                    to: 0.28
                    duration: 900
                    easing.type: Easing.InOutQuad
                }
            }

            Behavior on border.color {
                ColorAnimation {
                    duration: 180
                }
            }
        },
        Rectangle {
            anchors.fill: parent
            anchors.margins: -22
            radius: componentListCard.radius + 22
            color: "transparent"
            border.width: componentListCard.showAttentionHint ? 10 : 0
            border.color: Qt.alpha(componentListCard.hintColor, 0.14)
            opacity: componentListCard.showAttentionHint ? 0.50 : 0.0
            visible: opacity > 0
            SequentialAnimation on opacity {
                loops: Animation.Infinite
                running: componentListCard.showAttentionHint
                NumberAnimation {
                    from: 0.05
                    to: 0.34
                    duration: 1250
                    easing.type: Easing.OutCubic
                }
                NumberAnimation {
                    from: 0.34
                    to: 0.03
                    duration: 1250
                    easing.type: Easing.InCubic
                }
            }

            SequentialAnimation on scale {
                loops: Animation.Infinite
                running: componentListCard.showAttentionHint
                NumberAnimation {
                    from: 0.992
                    to: 1.018
                    duration: 1250
                    easing.type: Easing.OutCubic
                }
                NumberAnimation {
                    from: 1.018
                    to: 0.996
                    duration: 1250
                    easing.type: Easing.InCubic
                }
            }
        },
        // === 全局单例预览弹窗 ===
        ComponentPreviewPopup {
            id: _previewPopup
            parent: componentListCard.Window.window ? componentListCard.Window.window.contentItem : componentListCard
        }
    ]
    ComponentToolbar {
        id: componentToolbar
        width: parent.width
        componentListController: componentListCard.componentListController
        exportProgressController: componentListCard.exportProgressController
    }
    // === 全局单例预览弹窗 ===
    // 暴露全局预览弹窗实例供外部调用。
    property alias previewPopup: _previewPopup
    // 将弹窗坐标限制在父窗口边界内。
    function clampPopupCoordinate(value, size, boundary) {
        var margin = AppStyle.spacing.xs;
        var maxValue = Math.max(margin, boundary - size - margin);
        return Math.max(margin, Math.min(value, maxValue));
    }

    // 按设备像素比对弹窗坐标取整，避免边缘模糊。
    function snapPopupCoordinate(value) {
        var win = componentListCard.Window.window;
        var dpr = win && win.devicePixelRatio > 0 ? win.devicePixelRatio : 1;
        return Math.round(value * dpr) / dpr;
    }

    // 判断鼠标是否位于预览弹窗及其安全间距内。
    function cursorOverPopup(x, y, width, height, cursorX, cursorY, safetyGap) {
        if (cursorX === undefined || cursorY === undefined || isNaN(cursorX) || isNaN(cursorY))
            return false;
        return cursorX >= x - safetyGap && cursorX <= x + width + safetyGap && cursorY >= y - safetyGap && cursorY <= y + height + safetyGap;
    }

    // 在鼠标重叠时计算一个不遮挡鼠标的弹窗位置。
    function avoidCursorOverlap(x, y, width, height, boundaryWidth, boundaryHeight, cursorX, cursorY) {
        var safetyGap = AppStyle.interactions.safeCursorGap;
        if (!cursorOverPopup(x, y, width, height, cursorX, cursorY, safetyGap))
            return Qt.point(x, y);
        var candidates = [Qt.point(cursorX + safetyGap, y), Qt.point(cursorX - width - safetyGap, y), Qt.point(x, cursorY + safetyGap), Qt.point(x, cursorY - height - safetyGap)];
        var bestPoint = Qt.point(x, y);
        var bestDistance = Number.MAX_VALUE;
        for (var i = 0; i < candidates.length; ++i) {
            var candidateX = clampPopupCoordinate(candidates[i].x, width, boundaryWidth);
            var candidateY = clampPopupCoordinate(candidates[i].y, height, boundaryHeight);
            if (cursorOverPopup(candidateX, candidateY, width, height, cursorX, cursorY, safetyGap))
                continue;
            var distance = Math.abs(candidateX - x) + Math.abs(candidateY - y);
            if (distance < bestDistance) {
                bestDistance = distance;
                bestPoint = Qt.point(candidateX, candidateY);
            }
        }
        return bestPoint;
    }

    // 更新导出状态查找表
    // 将导出结果转换为按元件编号索引的查找表。
    function updateExportStatusMap() {
        var map = {};
        var pc = componentListCard.exportProgressController;
        if (pc && pc.resultsList) {
            var list = pc.resultsList;
            for (var i = 0; i < list.length; ++i) {
                var item = list[i];
                if (item && item.componentId) {
                    map[item.componentId] = item;
                }
            }
        }
        exportStatusMap = map;
    }

    // 根据缩略图位置和窗口边界定位预览弹窗。
    function positionPreviewPopup() {
        var popupParent = _previewPopup.parent || componentListCard;
        var winW = popupParent.width;
        var winH = popupParent.height;
        var gap = AppStyle.spacing.lg + 8;
        var popupW = _previewPopup.width;
        var popupH = _previewPopup.height;
        if (winW <= 0 || winH <= 0 || popupW <= 0 || popupH <= 0)
            return;
        var thumbX = _previewPopup.anchorX;
        var thumbY = _previewPopup.anchorY;
        var thumbW = _previewPopup.anchorWidth;
        var thumbH = _previewPopup.anchorHeight;
        var spaceRight = winW - (thumbX + thumbW);
        var spaceLeft = thumbX;
        var targetX;
        if (spaceRight >= popupW + gap) {
            targetX = thumbX + thumbW + gap;
        } else if (spaceLeft >= popupW + gap) {
            targetX = thumbX - popupW - gap;
        } else {
            targetX = (winW - popupW) / 2;
        }
        targetX = clampPopupCoordinate(targetX, popupW, winW);
        var targetY = thumbY + (thumbH - popupH) / 2;
        targetY = clampPopupCoordinate(targetY, popupH, winH);
        var adjustedPos = avoidCursorOverlap(targetX, targetY, popupW, popupH, winW, winH, _previewPopup.cursorX, _previewPopup.cursorY);
        _previewPopup.x = snapPopupCoordinate(adjustedPos.x);
        _previewPopup.y = snapPopupCoordinate(adjustedPos.y);
        var thumbCenterX = thumbX + thumbW / 2;
        var thumbCenterY = thumbY + thumbH / 2;
        _previewPopup.scaleOriginX = Math.max(0, Math.min(popupW, thumbCenterX - _previewPopup.x));
        _previewPopup.scaleOriginY = Math.max(0, Math.min(popupH, thumbCenterY - _previewPopup.y));
    }

    // 弹窗显示/隐藏辅助函数（供 delegate 调用）
    // 保存预览上下文并在尺寸更新后显示弹窗。
    function showPreviewPopup(imageSources, itemData, thumbX, thumbY, thumbW, thumbH, cursorX, cursorY) {
        _previewPopup.cancelHide();
        _previewPopup.imageSources = imageSources || [];
        _previewPopup.currentItemData = itemData;
        _previewPopup.anchorX = thumbX;
        _previewPopup.anchorY = thumbY;
        _previewPopup.anchorWidth = thumbW;
        _previewPopup.anchorHeight = thumbH;
        _previewPopup.cursorX = cursorX;
        _previewPopup.cursorY = cursorY;
        // imageSources 改变后 Popup 尺寸由绑定表达式重新计算，下一轮事件循环再定位。
        Qt.callLater(function () {
            if (_previewPopup.currentItemData !== itemData)
                return;
            positionPreviewPopup();
            _previewPopup.open();
        });
    }

    // 延迟隐藏预览弹窗，避免鼠标移动时立即闪烁。
    function hidePreviewPopup() {
        _previewPopup.scheduleHide();
    }

    ComponentListView {
        id: componentListView
        Layout.fillWidth: true
        componentListController: componentListCard.componentListController
        exportStatusMap: componentListCard.exportStatusMap
        searchText: componentToolbar.searchText
        // 接收列表项的描述编辑请求，并交给宿主对话框处理。
        onDescriptionEditRequested: function (componentId, description) {
            componentListCard.editingDescriptionComponentId = componentId;
            descriptionDialog.descriptionText = description;
            descriptionDialog.open();
        }
    }
}
