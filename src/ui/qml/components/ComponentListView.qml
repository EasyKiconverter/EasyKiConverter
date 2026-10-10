import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQml.Models
import EasyKiconverter_Cpp_Version.src.ui.qml.styles 1.0

/**
 * @brief 元件列表过滤模型和网格视图。
 *
 * 负责委托创建、筛选、虚拟化布局以及可见区域预览图预取，业务状态由宿主传入。
 */
Item {
    id: componentListView
    property var componentListController
    property var exportStatusMap: ({})
    property string searchText: ""
    signal descriptionEditRequested(string componentId, string description)
    implicitHeight: listLayout.implicitHeight
    /** @brief 请求过滤模型重新计算当前显示项。 */
    function updateFilter() {
        visualModel.updateFilter();
    }

    resources: [
        DelegateModel {
            id: visualModel
            model: componentListView.componentListController ?? null
            groups: [
                DelegateModelGroup {
                    id: displayGroup
                    includeByDefault: true
                    name: "display"
                },
                DelegateModelGroup {
                    id: filterGroup
                    name: "filter"
                }
            ]
            filterOnGroup: "display"
            delegate: Item {
                width: componentList.cellWidth
                height: componentList.cellHeight
                // 别名：供 requestVisiblePreviewImages 通过 itemAtIndex 访问
                property alias itemData: delegateItem.itemData
                ComponentListItem {
                    id: delegateItem
                    width: parent.width - AppStyle.spacing.md
                    height: parent.height - AppStyle.spacing.md
                    anchors.centerIn: parent
                    // 绑定数据和搜索词
                    // 注意：QAbstractListModel 暴露的角色名为 "itemData"
                    itemData: model.itemData
                    searchText: componentListView.searchText // 传递搜索词用于高亮
                    // 绑定导出状态（由 ComponentListCard 维护的查找表）
                    exportStatus: {
                        var id = itemData ? itemData.componentId : "";
                        return id && componentListView.exportStatusMap[id] ? componentListView.exportStatusMap[id] : null;
                    }
                    onDeleteClicked: {
                        if (itemData) {
                            componentListView.componentListController.removeComponentById(itemData.componentId);
                        }
                    }
                    onRetryClicked: {
                        if (itemData) {
                            componentListView.componentListController.refreshComponentInfo(index);
                        }
                    }
                    // 打开元件描述编辑对话框并记录当前元件。
                    onDescriptionEditRequested: function (componentId, description) {
                        componentListView.descriptionEditRequested(componentId, description);
                    }
                }
            }

            // 过滤函数
            // 根据筛选模式和搜索文本更新委托的显示状态。
            function updateFilter() {
                // 移除所有空格，实现更宽容的搜索 (例如 "C 2040" -> "c2040")
                var searchTerm = componentListView.searchText.toLowerCase().replace(/\s+/g, '');
                var filterMode = componentListView.componentListController ? componentListView.componentListController.filterMode : "all";
                // 遍历所有项进行处理
                for (var i = 0; i < items.count; i++) {
                    var item = items.get(i);
                    // 获取数据对象
                    var dataObj = item.model.itemData;
                    var idStr = dataObj && dataObj.componentId !== undefined ? dataObj.componentId : "";
                    var validationPhase = dataObj && dataObj.validationPhase !== undefined ? dataObj.validationPhase : "idle";
                    // 验证状态筛选（使用 validationPhase）
                    var passFilter = false;
                    if (filterMode === "all") {
                        passFilter = true;
                    } else if (filterMode === "validating") {
                        // 验证中：仅表示 CAD 验证尚未完成
                        passFilter = (validationPhase === "validating");
                    } else if (filterMode === "valid") {
                        // 有效：验证已完成或正在获取预览图的项目都属于"有效"
                        passFilter = (validationPhase === "completed" || validationPhase === "fetching_preview");
                    } else if (filterMode === "invalid") {
                        passFilter = (validationPhase === "failed");
                    }

                    // 搜索词筛选
                    var passSearch = true;
                    if (searchTerm !== "" && idStr.toLowerCase().indexOf(searchTerm) === -1) {
                        passSearch = false;
                    }

                    // 同时满足筛选和搜索条件才显示
                    item.inDisplay = passFilter && passSearch;
                }
            }
        }
    ]
    ColumnLayout {
        id: listLayout
        width: parent.width
        // 元件列表视图（自适应网格）
        GridView {
            id: componentList
            Layout.fillWidth: true
            Layout.preferredHeight: ResponsiveHelper.isShortWindow ? 200 : ResponsiveHelper.responsive(240, 300, 360, 400)
            Layout.topMargin: AppStyle.spacing.md
            clip: true
            // 启用虚拟化，缓存上下各一屏的项
            cacheBuffer: 500
            // 启用 Item 回收，减少创建/销毁开销
            reuseItems: true
            cellWidth: {
                var w = width - AppStyle.spacing.md;
                var minCellW = ResponsiveHelper.responsive(180, 220, 230, 250);
                var c = Math.max(1, Math.floor(w / minCellW));
                // 向下取整，确保所有列的总宽度不超过可用宽度。
                return Math.max(1, Math.floor(w / c));
            }
            cellHeight: ResponsiveHelper.isShortWindow ? 60 : 76
            flow: GridView.FlowLeftToRight
            layoutDirection: Qt.LeftToRight
            // 使用 DelegateModel
            model: visualModel
            // 监听滚动状态
            onMovingChanged: {
                if (componentListView.componentListController) {
                    componentListView.componentListController.setScrolling(moving);
                }
            }

            // 请求当前可见区域及缓冲区域内元件的预览图。
            function requestVisiblePreviewImages() {
                if (!componentListView.componentListController) {
                    return;
                }

                var ids = [];
                var top = componentList.contentY;
                var bottom = top + componentList.height;
                var prefetchMargin = componentList.cellHeight;
                for (var i = 0; i < componentList.count; ++i) {
                    var delegate = componentList.itemAtIndex(i);
                    if (!delegate || !delegate.itemData || !delegate.itemData.componentId) {
                        continue;
                    }

                    var itemTop = delegate.y;
                    var itemBottom = delegate.y + delegate.height;
                    if (itemBottom < top - prefetchMargin || itemTop > bottom + prefetchMargin) {
                        continue;
                    }

                    if (!delegate.itemData.previewImageCount || delegate.itemData.previewImageCount === 0) {
                        ids.push(delegate.itemData.componentId);
                    }
                }

                if (ids.length > 0) {
                    componentListView.componentListController.fetchPreviewImages(ids);
                }
            }

            ScrollBar.vertical: ScrollBar {
                policy: ScrollBar.AsNeeded
            }
        }
    }
}
