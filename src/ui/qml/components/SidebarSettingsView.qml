import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import EasyKiconverter_Cpp_Version.src.ui.qml.styles 1.0

Item {
    id: root
    property var exportSettingsController
    property var exportTargetModel
    signal openOutputFolderDialog
    signal openCacheFolderDialog
    implicitHeight: mainColumn.implicitHeight
    ColumnLayout {
        id: mainColumn
        width: parent.width
        spacing: AppStyle.spacing.lg
        // ==================== 目标格式选择 ====================
        TargetFormatSelector {
            Layout.fillWidth: true
            targetModel: root.exportTargetModel
        }

        // ==================== 导出路径与库名 ====================
        SidebarSection {
            title: qsTranslate("MainWindow", "输出配置")
            SidebarTextField {
                label: qsTranslate("MainWindow", "输出路径")
                text: root.exportSettingsController ? root.exportSettingsController.outputPath : ""
                placeholder: qsTranslate("MainWindow", "选择目录...")
                onTextEdited: txt => {
                    if (root.exportSettingsController)
                        root.exportSettingsController.setOutputPath(txt);
                }
                browseIcon: "folder"
                onBrowseClicked: root.openOutputFolderDialog()
            }

            Text {
                visible: root.exportTargetModel && root.exportTargetModel.currentTargetId === "librepcb"
                Layout.fillWidth: true
                Layout.leftMargin: AppStyle.spacing.lg
                text: qsTranslate("MainWindow", "LibrePCB：选择项目根目录或项目的 library 目录可自动安装到项目和 Workspace 本地库；选择普通目录时，将生成 .lplib 目录，需手动复制到 Workspace/data/libraries/local/。")
                color: AppStyle.colors.textSecondary
                font.pixelSize: AppStyle.fontSizes.xs
                wrapMode: Text.WordWrap
                lineHeight: 1.25
            }

            SidebarTextField {
                label: qsTranslate("MainWindow", "库名称")
                text: root.exportSettingsController ? root.exportSettingsController.libName : ""
                placeholder: "EasyEDA_Lib"
                onTextEdited: txt => {
                    if (root.exportSettingsController)
                        root.exportSettingsController.setLibName(txt);
                }
            }

            SidebarTextField {
                label: qsTranslate("MainWindow", "缓存目录")
                text: root.exportSettingsController ? root.exportSettingsController.cacheDir : ""
                placeholder: qsTranslate("MainWindow", "选择目录...")
                onEditingFinished: txt => {
                    if (root.exportSettingsController)
                        root.exportSettingsController.setCacheDir(txt);
                }
                browseIcon: "folder"
                onBrowseClicked: root.openCacheFolderDialog()
            }
        }

        // ==================== 导出选项 ====================
        SidebarSection {
            title: qsTranslate("MainWindow", "导出内容")
            SidebarToggleRow {
                label: qsTranslate("MainWindow", "符号库")
                enabled: !(root.exportTargetModel && root.exportTargetModel.currentTargetId === "orcad")
                checked: root.exportSettingsController ? root.exportSettingsController.exportSymbol : false
                onToggled: val => {
                    if (root.exportSettingsController)
                        root.exportSettingsController.setExportSymbol(val);
                }
            }

            SidebarToggleRow {
                label: qsTranslate("MainWindow", "封装库")
                enabled: !(root.exportTargetModel && root.exportTargetModel.currentTargetId === "orcad")
                checked: root.exportSettingsController ? root.exportSettingsController.exportFootprint : false
                onToggled: val => {
                    if (root.exportSettingsController)
                        root.exportSettingsController.setExportFootprint(val);
                }
            }

            Text {
                visible: root.exportTargetModel && root.exportTargetModel.currentTargetId === "orcad"
                Layout.fillWidth: true
                Layout.leftMargin: AppStyle.spacing.lg
                text: qsTranslate("MainWindow", "OrCAD Capture XML 只保存符号和封装名称关联，PCB 封装几何需要单独导出")
                color: AppStyle.colors.textSecondary
                font.pixelSize: AppStyle.fontSizes.xs
                wrapMode: Text.WordWrap
            }

            // 3D 模型 - 扁平化子选项布局
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 0
                SidebarToggleRow {
                    id: model3dToggle
                    label: qsTranslate("MainWindow", "3D 模型")
                    property bool isAllegroTarget: root.exportTargetModel && root.exportTargetModel.currentTargetId === "allegro"
                    checked: root.exportSettingsController ? root.exportSettingsController.exportModel3D : false
                    onToggled: val => {
                        if (root.exportSettingsController)
                            root.exportSettingsController.setExportModel3D(val);
                    }
                }

                // 子选项区域（高度动画 + clip）
                Item {
                    Layout.fillWidth: true
                    Layout.preferredHeight: model3dToggle.checked ? subOptionsContainer.implicitHeight + AppStyle.spacing.sm : 0
                    clip: true
                    enabled: model3dToggle.checked
                    Behavior on Layout.preferredHeight {
                        NumberAnimation {
                            duration: AppStyle.durations.fast
                            easing.type: Easing.OutQuart
                        }
                    }

                    // L 型连接线（带脉冲动画）
                    Item {
                        id: connectorLine
                        x: AppStyle.spacing.lg + 2
                        width: 16
                        height: parent.height
                        Rectangle {
                            id: connectorVert
                            x: 0
                            y: 0
                            width: 2
                            height: parent.height
                            color: AppStyle.colors.border
                            opacity: 0.5
                        }
                        Rectangle {
                            id: connectorHoriz
                            x: 0
                            y: Math.min(parent.height * 0.35, 28)
                            width: 12
                            height: 2
                            color: AppStyle.colors.border
                            opacity: 0.5
                        }
                        // 脉冲动画：切换格式/路径时触发
                        SequentialAnimation {
                            id: connectorPulse
                            PropertyAction {
                                targets: [connectorVert, connectorHoriz]
                                property: "color"
                                value: AppStyle.colors.primary
                            }
                            PropertyAction {
                                targets: [connectorVert, connectorHoriz]
                                property: "opacity"
                                value: 0.9
                            }
                            PauseAnimation {
                                duration: 120
                            }
                            ColorAnimation {
                                targets: [connectorVert, connectorHoriz]
                                property: "color"
                                to: AppStyle.colors.border
                                duration: 400
                                easing.type: Easing.OutQuart
                            }
                            NumberAnimation {
                                targets: [connectorVert, connectorHoriz]
                                property: "opacity"
                                to: 0.5
                                duration: 400
                                easing.type: Easing.OutQuart
                            }
                        }
                    }

                    // 子选项内容（Item 容器解耦背景与布局）
                    Item {
                        id: subOptionsContainer
                        anchors.left: connectorLine.right
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.topMargin: AppStyle.spacing.xs
                        anchors.rightMargin: AppStyle.spacing.xs
                        implicitHeight: subOptionsColumn.implicitHeight + AppStyle.spacing.sm
                        // 背景浸润（独立于布局）
                        Rectangle {
                            anchors.fill: parent
                            radius: AppStyle.radius.sm
                            color: AppStyle.isDarkMode ? Qt.rgba(255, 255, 255, 0.04) : Qt.rgba(0, 0, 0, 0.04)
                            border.width: AppStyle.isDarkMode ? 1 : 0
                            border.color: Qt.rgba(255, 255, 255, 0.06)
                            z: -1
                        }

                        ColumnLayout {
                            id: subOptionsColumn
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.margins: AppStyle.spacing.xs
                            spacing: AppStyle.spacing.sm
                            // 格式选择（滑块式）
                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.leftMargin: AppStyle.spacing.sm
                                Layout.rightMargin: AppStyle.spacing.sm
                                spacing: 2
                                opacity: model3dToggle.checked ? 1 : 0
                                x: model3dToggle.checked ? 0 : 12
                                Behavior on opacity {
                                    NumberAnimation {
                                        duration: AppStyle.durations.fast
                                    }
                                }
                                Behavior on x {
                                    NumberAnimation {
                                        duration: AppStyle.durations.fast
                                        easing.type: Easing.OutQuart
                                    }
                                }

                                Text {
                                    text: qsTranslate("MainWindow", "格式")
                                    color: AppStyle.colors.textSecondary
                                    font.pixelSize: AppStyle.fontSizes.xs
                                }

                                Rectangle {
                                    Layout.fillWidth: true
                                    height: 30
                                    radius: AppStyle.radius.sm
                                    property bool isStepOnlyTarget: root.exportTargetModel && (root.exportTargetModel.currentTargetId === "altium" || root.exportTargetModel.currentTargetId === "librepcb" || root.exportTargetModel.currentTargetId === "horizon")
                                    color: AppStyle.isDarkMode ? Qt.rgba(255, 255, 255, 0.06) : Qt.rgba(0, 0, 0, 0.06)
                                    property int currentFormatIndex: {
                                        if (!root.exportSettingsController)
                                            return 0;
                                        // 这些原生库只允许 STEP，指示器也必须固定在 STEP，
                                        // 避免旧配置值让滑块视觉上移动到 WRL/Both。
                                        if (isStepOnlyTarget)
                                            return 1;
                                        var fmt = root.exportSettingsController.exportModel3DFormat;
                                        if (fmt === 3)
                                            return 2;
                                        if (fmt === 2)
                                            return 1;
                                        return 0;
                                    }
                                    Rectangle {
                                        width: parent.width / 3
                                        height: parent.height - 4
                                        anchors.verticalCenter: parent.verticalCenter
                                        radius: AppStyle.radius.sm - 1
                                        color: AppStyle.colors.surface
                                        border.width: AppStyle.borderWidths.thin
                                        border.color: AppStyle.colors.border
                                        x: parent.currentFormatIndex * (parent.width / 3) + 2
                                        Behavior on x {
                                            NumberAnimation {
                                                duration: 200
                                                easing.type: Easing.OutQuart
                                            }
                                        }
                                    }

                                    Row {
                                        anchors.fill: parent
                                        anchors.margins: 2
                                        Repeater {
                                            model: ["WRL", "STEP", "ALL"]
                                            Item {
                                                width: parent.width / 3
                                                height: parent.height
                                                // 这些原生库仅支持嵌入 STEP，WRL 和 Both 都不可选。
                                                opacity: parent.parent.isStepOnlyTarget && index !== 1 ? 0.4 : 1
                                                Text {
                                                    anchors.centerIn: parent
                                                    text: modelData
                                                    font.pixelSize: AppStyle.fontSizes.xs - 1
                                                    font.bold: index === parent.parent.parent.currentFormatIndex
                                                    color: index === parent.parent.parent.currentFormatIndex ? AppStyle.colors.primary : AppStyle.colors.textSecondary
                                                    Behavior on color {
                                                        ColorAnimation {
                                                            duration: 200
                                                        }
                                                    }
                                                }

                                                MouseArea {
                                                    anchors.fill: parent
                                                    enabled: !(parent.parent.isStepOnlyTarget && index !== 1)
                                                    cursorShape: Qt.PointingHandCursor
                                                    onClicked: {
                                                        if (!root.exportSettingsController)
                                                            return;
                                                        var val = index === 2 ? 3 : (index === 1 ? 2 : 1);
                                                        root.exportSettingsController.setExportModel3DFormat(val);
                                                        connectorPulse.restart();
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }

                            // 路径模式（滑块式）
                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.leftMargin: AppStyle.spacing.sm
                                Layout.rightMargin: AppStyle.spacing.sm
                                spacing: 2
                                opacity: model3dToggle.checked ? 1 : 0
                                x: model3dToggle.checked ? 0 : 12
                                Behavior on opacity {
                                    NumberAnimation {
                                        duration: AppStyle.durations.fast
                                    }
                                }
                                Behavior on x {
                                    NumberAnimation {
                                        duration: AppStyle.durations.fast
                                        easing.type: Easing.OutQuart
                                    }
                                }

                                Text {
                                    text: qsTranslate("MainWindow", "路径")
                                    color: AppStyle.colors.textSecondary
                                    font.pixelSize: AppStyle.fontSizes.xs
                                }

                                Rectangle {
                                    Layout.fillWidth: true
                                    height: 30
                                    radius: AppStyle.radius.sm
                                    color: AppStyle.isDarkMode ? Qt.rgba(255, 255, 255, 0.06) : Qt.rgba(0, 0, 0, 0.06)
                                    property int currentPathIndex: root.exportSettingsController ? root.exportSettingsController.exportModel3DPathMode : 0
                                    Rectangle {
                                        width: parent.width / 2
                                        height: parent.height - 4
                                        anchors.verticalCenter: parent.verticalCenter
                                        radius: AppStyle.radius.sm - 1
                                        color: AppStyle.colors.surface
                                        border.width: AppStyle.borderWidths.thin
                                        border.color: AppStyle.colors.border
                                        x: parent.currentPathIndex * (parent.width / 2) + 2
                                        Behavior on x {
                                            NumberAnimation {
                                                duration: 200
                                                easing.type: Easing.OutQuart
                                            }
                                        }
                                    }

                                    Row {
                                        anchors.fill: parent
                                        anchors.margins: 2
                                        Repeater {
                                            model: [qsTranslate("MainWindow", "相对"), qsTranslate("MainWindow", "绝对")]
                                            Item {
                                                width: parent.width / 2
                                                height: parent.height
                                                Text {
                                                    anchors.centerIn: parent
                                                    text: modelData
                                                    font.pixelSize: AppStyle.fontSizes.xs - 1
                                                    font.bold: index === parent.parent.parent.currentPathIndex
                                                    color: index === parent.parent.parent.currentPathIndex ? AppStyle.colors.primary : AppStyle.colors.textSecondary
                                                    Behavior on color {
                                                        ColorAnimation {
                                                            duration: 200
                                                        }
                                                    }
                                                }

                                                MouseArea {
                                                    anchors.fill: parent
                                                    cursorShape: Qt.PointingHandCursor
                                                    onClicked: {
                                                        if (root.exportSettingsController)
                                                            root.exportSettingsController.setExportModel3DPathMode(index);
                                                        connectorPulse.restart();
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            SidebarToggleRow {
                label: qsTranslate("MainWindow", "预览图")
                checked: root.exportSettingsController ? root.exportSettingsController.exportPreviewImages : false
                onToggled: val => {
                    if (root.exportSettingsController)
                        root.exportSettingsController.setExportPreviewImages(val);
                }
            }

            SidebarToggleRow {
                label: qsTranslate("MainWindow", "数据手册")
                checked: root.exportSettingsController ? root.exportSettingsController.exportDatasheet : false
                onToggled: val => {
                    if (root.exportSettingsController)
                        root.exportSettingsController.setExportDatasheet(val);
                }
            }
        }

        // ==================== 目标格式说明（单容器避免隐藏项重复占用间距） ====================
        ColumnLayout {
            id: targetInfoStack
            Layout.fillWidth: true
            spacing: 0
            // ==================== Altium 导出说明（仅 Altium 格式显示，带动画） ====================
            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: root.exportTargetModel !== null && root.exportTargetModel !== undefined && root.exportTargetModel.currentTargetId === "altium" ? altiumInfoBox.implicitHeight + AppStyle.spacing.md * 2 : 0
                clip: true
                Behavior on Layout.preferredHeight {
                    NumberAnimation {
                        duration: 400
                        easing.type: Easing.OutQuart
                    }
                }

                Rectangle {
                    id: altiumInfoBox
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    implicitHeight: altiumInfoText.implicitHeight + AppStyle.spacing.md * 2
                    radius: AppStyle.radius.sm
                    color: AppStyle.colors.surface
                    border.color: AppStyle.colors.border
                    border.width: 1
                    opacity: (root.exportTargetModel !== null && root.exportTargetModel.currentTargetId === "altium") ? 1 : 0
                    scale: (root.exportTargetModel !== null && root.exportTargetModel.currentTargetId === "altium") ? 1 : 0.97
                    y: (root.exportTargetModel !== null && root.exportTargetModel.currentTargetId === "altium") ? 0 : 20
                    Behavior on opacity {
                        NumberAnimation {
                            duration: 500
                            easing.type: Easing.OutCubic
                        }
                    }
                    Behavior on scale {
                        NumberAnimation {
                            duration: 500
                            easing.type: Easing.OutQuart
                        }
                    }
                    Behavior on y {
                        NumberAnimation {
                            duration: 500
                            easing.type: Easing.OutQuart
                        }
                    }

                    Text {
                        id: altiumInfoText
                        anchors.fill: parent
                        anchors.margins: AppStyle.spacing.md
                        text: qsTranslate("MainWindow", "Altium 导出说明：\n" + "- 符号库导出为 .SchLib 格式\n" + "- 封装库导出为 .PcbLib 格式\n" + "- 3D 模型以 STEP 格式嵌入封装\n" + "- 生成的文件可直接在 Altium Designer 中打开")
                        font.pixelSize: AppStyle.fontSizes.xs
                        color: AppStyle.colors.textSecondary
                        wrapMode: Text.WordWrap
                        lineHeight: 1.4
                    }
                }
            }

            // ==================== Xpedition 导出说明（仅 Xpedition 格式显示，带动画） ====================
            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: root.exportTargetModel !== null && root.exportTargetModel !== undefined && root.exportTargetModel.currentTargetId === "xpedition" ? xpeditionInfoBox.implicitHeight + AppStyle.spacing.md * 2 : 0
                clip: true
                Behavior on Layout.preferredHeight {
                    NumberAnimation {
                        duration: 400
                        easing.type: Easing.OutQuart
                    }
                }

                Rectangle {
                    id: xpeditionInfoBox
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    implicitHeight: xpeditionInfoText.implicitHeight + AppStyle.spacing.md * 2
                    radius: AppStyle.radius.sm
                    color: AppStyle.colors.surface
                    border.color: AppStyle.colors.border
                    border.width: 1
                    opacity: (root.exportTargetModel !== null && root.exportTargetModel.currentTargetId === "xpedition") ? 1 : 0
                    scale: (root.exportTargetModel !== null && root.exportTargetModel.currentTargetId === "xpedition") ? 1 : 0.97
                    y: (root.exportTargetModel !== null && root.exportTargetModel.currentTargetId === "xpedition") ? 0 : 20
                    Behavior on opacity {
                        NumberAnimation {
                            duration: 500
                            easing.type: Easing.OutCubic
                        }
                    }
                    Behavior on scale {
                        NumberAnimation {
                            duration: 500
                            easing.type: Easing.OutQuart
                        }
                    }
                    Behavior on y {
                        NumberAnimation {
                            duration: 500
                            easing.type: Easing.OutQuart
                        }
                    }

                    Text {
                        id: xpeditionInfoText
                        anchors.fill: parent
                        anchors.margins: AppStyle.spacing.md
                        text: qsTranslate("MainWindow", "Xpedition 导出说明：\n" + "- 符号库导出为 _Symbols.zip\n" + "- 封装库导出为 _Footprints.zip\n" + "- 三维模型由独立阶段输出为 WRL/STEP 文件，不写入原生关联\n" + "- 当前仅支持覆盖导出，不支持追加、更新或重试\n" + "- 当前支持基础引脚、矩形、折线、圆形和圆弧图元")
                        font.pixelSize: AppStyle.fontSizes.xs
                        color: AppStyle.colors.textSecondary
                        wrapMode: Text.WordWrap
                        lineHeight: 1.4
                    }
                }
            }

            // ==================== Allegro 导出说明（符号、封装和三维语义包） ====================
            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: root.exportTargetModel !== null && root.exportTargetModel !== undefined && root.exportTargetModel.currentTargetId === "allegro" ? allegroInfoBox.implicitHeight + AppStyle.spacing.md * 2 : 0
                clip: true
                Behavior on Layout.preferredHeight {
                    NumberAnimation {
                        duration: 400
                        easing.type: Easing.OutQuart
                    }
                }

                Rectangle {
                    id: allegroInfoBox
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    implicitHeight: allegroInfoText.implicitHeight + AppStyle.spacing.md * 2
                    radius: AppStyle.radius.sm
                    color: AppStyle.colors.surface
                    border.color: AppStyle.colors.border
                    border.width: 1
                    opacity: root.exportTargetModel !== null && root.exportTargetModel !== undefined && root.exportTargetModel.currentTargetId === "allegro" ? 1 : 0
                    Text {
                        id: allegroInfoText
                        anchors.fill: parent
                        anchors.margins: AppStyle.spacing.md
                        text: qsTranslate("MainWindow", "Allegro 导出说明：\n" + "- Import Package 包含规范化 Symbol、Footprint、Pin-Pad 关联和 STEP 数据\n" + "- 不生成原生 Allegro Symbol、OLB、.dra/.psm/.pad\n" + "- 需要在 Cadence Allegro 环境中继续生成目标库\n" + "- 不支持更新和重试模式\n" + "- Place Bound 缺失时会在诊断中说明回退策略")
                        font.pixelSize: AppStyle.fontSizes.xs
                        color: AppStyle.colors.textSecondary
                        wrapMode: Text.WordWrap
                        lineHeight: 1.4
                    }
                }
            }

            // ==================== LibrePCB 导出说明（仅 LibrePCB 格式显示） ====================
            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: root.exportTargetModel !== null && root.exportTargetModel !== undefined && root.exportTargetModel.currentTargetId === "librepcb" ? librePcbInfoBox.implicitHeight + AppStyle.spacing.md * 2 : 0
                clip: true
                Behavior on Layout.preferredHeight {
                    NumberAnimation {
                        duration: 400
                        easing.type: Easing.OutQuart
                    }
                }

                Rectangle {
                    id: librePcbInfoBox
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    implicitHeight: librePcbInfoText.implicitHeight + AppStyle.spacing.md * 2
                    radius: AppStyle.radius.sm
                    color: AppStyle.colors.surface
                    border.color: AppStyle.colors.border
                    border.width: 1
                    Text {
                        id: librePcbInfoText
                        anchors.fill: parent
                        anchors.margins: AppStyle.spacing.md
                        text: qsTranslate("MainWindow", "LibrePCB 导出说明：\n" + "- 原生库只支持完整覆盖导出\n" + "- 不支持追加、更新或失败重试\n" + "- 符号必须是单部件，缺失或重复引脚编号会拒绝导出\n" + "- 不可表达的焊盘和图元会产生诊断或拒绝导出\n" + "- 三维模型使用 IR 中的 STEP 数据，WRL 选项不可用")
                        font.pixelSize: AppStyle.fontSizes.xs
                        color: AppStyle.colors.textSecondary
                        wrapMode: Text.WordWrap
                        lineHeight: 1.4
                    }
                }
            }
        }

        // ==================== 运行策略（滑块式导出模式选择） ====================
        SidebarSection {
            title: qsTranslate("MainWindow", "运行策略")
            // 导出模式：滑块式三选一
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                Text {
                    text: qsTranslate("MainWindow", "导出模式")
                    font.pixelSize: AppStyle.fontSizes.xs
                    color: AppStyle.colors.textSecondary
                }

                Rectangle {
                    Layout.fillWidth: true
                    height: 36
                    radius: AppStyle.radius.sm
                    color: AppStyle.isDarkMode ? Qt.rgba(255, 255, 255, 0.06) : Qt.rgba(0, 0, 0, 0.06)
                    // 滑块指示器
                    Rectangle {
                        id: modeSlider
                        width: parent.width / 3
                        height: parent.height - 4
                        anchors.verticalCenter: parent.verticalCenter
                        radius: AppStyle.radius.sm - 1
                        color: AppStyle.colors.surface
                        border.width: AppStyle.borderWidths.thin
                        border.color: AppStyle.colors.border
                        x: (root.exportSettingsController ? root.exportSettingsController.exportMode : 0) * parent.width / 3 + 2
                        Behavior on x {
                            NumberAnimation {
                                duration: 200
                                easing.type: Easing.OutQuart
                            }
                        }
                    }

                    Row {
                        anchors.fill: parent
                        anchors.margins: 2
                        Repeater {
                            model: root.exportSettingsController && root.exportSettingsController.requiresFullReplacement ? [qsTranslate("MainWindow", "追加（不支持）"), qsTranslate("MainWindow", "更新（不支持）"), qsTranslate("MainWindow", "完整覆盖")] : [qsTranslate("MainWindow", "追加"), qsTranslate("MainWindow", "更新"), qsTranslate("MainWindow", "覆盖")]
                            Item {
                                width: parent.width / 3
                                height: parent.height
                                enabled: !(root.exportSettingsController && root.exportSettingsController.requiresFullReplacement && index !== 2)
                                opacity: enabled ? 1 : 0.45
                                Text {
                                    anchors.centerIn: parent
                                    text: modelData
                                    font.pixelSize: AppStyle.fontSizes.xs
                                    font.bold: index === (root.exportSettingsController ? root.exportSettingsController.exportMode : 0)
                                    color: index === (root.exportSettingsController ? root.exportSettingsController.exportMode : 0) ? AppStyle.colors.primary : AppStyle.colors.textSecondary
                                    Behavior on color {
                                        ColorAnimation {
                                            duration: 200
                                        }
                                    }
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    enabled: parent.enabled
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        if (root.exportSettingsController)
                                            root.exportSettingsController.setExportMode(index);
                                    }
                                }
                            }
                        }
                    }
                }

                Text {
                    Layout.fillWidth: true
                    text: {
                        var mode = root.exportSettingsController ? root.exportSettingsController.exportMode : 0;
                        if (root.exportSettingsController && root.exportSettingsController.requiresFullReplacement)
                            return qsTranslate("MainWindow", "覆盖：重新生成完整库；当前目标不支持追加、更新或失败重试");
                        if (mode === 0)
                            return qsTranslate("MainWindow", "追加：保留已有库内容，只加入新的元器件；已有同名内容不会被覆盖");
                        if (mode === 1)
                            return qsTranslate("MainWindow", "更新：在已有库基础上处理缺失或变化内容，并保留未参与本次导出的内容");
                        return qsTranslate("MainWindow", "覆盖：重新生成完整库并替换已有输出，请确认旧库可以被替换");
                    }
                    font.pixelSize: AppStyle.fontSizes.xs - 1
                    color: AppStyle.colors.textSecondary
                    opacity: 0.7
                    wrapMode: Text.WordWrap
                    lineHeight: 1.25
                }
            }

            SidebarToggleRow {
                label: qsTranslate("MainWindow", "弱网模式")
                checked: root.exportSettingsController ? root.exportSettingsController.weakNetworkSupport : false
                onToggled: val => {
                    if (root.exportSettingsController)
                        root.exportSettingsController.setWeakNetworkSupport(val);
                }
            }
        }

        // ==================== 库信息（短窗口时自动隐藏，仅 KiCad 格式显示） ====================
        SidebarSection {
            title: qsTranslate("MainWindow", "库信息 (可选)")
            visible: !ResponsiveHelper.isShortWindow && (root.exportTargetModel === null || root.exportTargetModel === undefined || root.exportTargetModel.currentTargetId === "kicad")
            SidebarTextField {
                label: qsTranslate("MainWindow", "符号库描述")
                text: root.exportSettingsController ? root.exportSettingsController.symbolLibraryDescription : ""
                onTextEdited: txt => {
                    if (root.exportSettingsController)
                        root.exportSettingsController.setSymbolLibraryDescription(txt);
                }
            }

            SidebarTextField {
                label: qsTranslate("MainWindow", "封装库描述")
                text: root.exportSettingsController ? root.exportSettingsController.footprintLibraryDescription : ""
                onTextEdited: txt => {
                    if (root.exportSettingsController)
                        root.exportSettingsController.setFootprintLibraryDescription(txt);
                }
            }
        }
    }
}
