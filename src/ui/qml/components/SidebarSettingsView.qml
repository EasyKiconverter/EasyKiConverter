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
        ColumnLayout {
            Layout.fillWidth: true
            spacing: AppStyle.spacing.sm
            visible: root.exportTargetModel !== null && root.exportTargetModel !== undefined
            Text {
                text: qsTranslate("MainWindow", "目标格式")
                font.pixelSize: AppStyle.fontSizes.sm
                font.bold: true
                color: AppStyle.colors.textPrimary
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: AppStyle.spacing.sm
                Repeater {
                    model: root.exportTargetModel ? root.exportTargetModel.availableTargets : []
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        Layout.preferredHeight: 76
                        radius: AppStyle.radius.md
                        property bool isActive: root.exportTargetModel ? root.exportTargetModel.currentIndex === index : false
                        property string targetId: modelData.id || ""
                        color: isActive ? Qt.rgba(AppStyle.colors.primary.r, AppStyle.colors.primary.g, AppStyle.colors.primary.b, 0.12) : AppStyle.colors.surface
                        border.color: isActive ? AppStyle.colors.primary : AppStyle.colors.border
                        border.width: isActive ? 2 : 1
                        Behavior on color {
                            ColorAnimation {
                                duration: AppStyle.durations.fast
                            }
                        }
                        Behavior on border.color {
                            ColorAnimation {
                                duration: AppStyle.durations.fast
                            }
                        }

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: AppStyle.spacing.sm
                            spacing: 4
                            Text {
                                text: modelData.displayName || ""
                                font.pixelSize: AppStyle.fontSizes.xs
                                font.bold: true
                                color: isActive ? AppStyle.colors.primary : AppStyle.colors.textPrimary
                                Layout.fillWidth: true
                                Layout.alignment: Qt.AlignHCenter
                                horizontalAlignment: Text.AlignHCenter
                                elide: Text.ElideRight
                            }

                            Text {
                                text: {
                                    if (targetId === "kicad")
                                        return ".kicad_sym";
                                    if (targetId === "altium")
                                        return ".SchLib";
                                    if (targetId === "xpedition")
                                        return "_Symbols.zip / _Footprints.zip";
                                    if (targetId === "allegro")
                                        return "Footprint Import Package";
                                    return "";
                                }
                                font.pixelSize: 9
                                color: AppStyle.colors.textSecondary
                                Layout.fillWidth: true
                                Layout.alignment: Qt.AlignHCenter
                                horizontalAlignment: Text.AlignHCenter
                                maximumLineCount: 2
                                wrapMode: Text.WordWrap
                                elide: Text.ElideRight
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (root.exportTargetModel) {
                                    root.exportTargetModel.currentIndex = index;
                                }
                            }
                        }
                    }
                }
            }
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
                enabled: !(root.exportTargetModel && root.exportTargetModel.currentIndex === 8)
                checked: root.exportSettingsController ? root.exportSettingsController.exportSymbol : false
                onToggled: val => {
                    if (root.exportSettingsController)
                        root.exportSettingsController.setExportSymbol(val);
                }
            }

            SidebarToggleRow {
                label: qsTranslate("MainWindow", "封装库")
                enabled: !(root.exportTargetModel && root.exportTargetModel.currentIndex === 8)
                checked: root.exportSettingsController ? root.exportSettingsController.exportFootprint : false
                onToggled: val => {
                    if (root.exportSettingsController)
                        root.exportSettingsController.setExportFootprint(val);
                }
            }

            Text {
                visible: root.exportTargetModel && root.exportTargetModel.currentIndex === 8
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
                    property bool isAllegroTarget: root.exportTargetModel && root.exportTargetModel.currentIndex === 3
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
                                    property bool isAltiumTarget: root.exportTargetModel && root.exportTargetModel.currentIndex === 1
                                    color: AppStyle.isDarkMode ? Qt.rgba(255, 255, 255, 0.06) : Qt.rgba(0, 0, 0, 0.06)
                                    property int currentFormatIndex: {
                                        if (!root.exportSettingsController)
                                            return 0;
                                        // Altium 只允许 STEP，指示器也必须固定在 STEP，
                                        // 避免旧配置值让滑块视觉上移动到 WRL/Both。
                                        if (isAltiumTarget)
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
                                                // Altium PcbLib 仅支持嵌入 STEP，WRL 和 Both 都不可选。
                                                opacity: parent.parent.isAltiumTarget && index !== 1 ? 0.4 : 1
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
                                                    enabled: !(parent.parent.isAltiumTarget && index !== 1)
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

        // ==================== Altium 导出说明（仅 Altium 格式显示，带动画） ====================
        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: root.exportTargetModel !== null && root.exportTargetModel !== undefined && root.exportTargetModel.currentIndex === 1 ? altiumInfoBox.implicitHeight + AppStyle.spacing.md * 2 : 0
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
                opacity: (root.exportTargetModel !== null && root.exportTargetModel.currentIndex === 1) ? 1 : 0
                scale: (root.exportTargetModel !== null && root.exportTargetModel.currentIndex === 1) ? 1 : 0.97
                y: (root.exportTargetModel !== null && root.exportTargetModel.currentIndex === 1) ? 0 : 20
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
            Layout.preferredHeight: root.exportTargetModel !== null && root.exportTargetModel !== undefined && root.exportTargetModel.currentIndex === 2 ? xpeditionInfoBox.implicitHeight + AppStyle.spacing.md * 2 : 0
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
                opacity: (root.exportTargetModel !== null && root.exportTargetModel.currentIndex === 2) ? 1 : 0
                scale: (root.exportTargetModel !== null && root.exportTargetModel.currentIndex === 2) ? 1 : 0.97
                y: (root.exportTargetModel !== null && root.exportTargetModel.currentIndex === 2) ? 0 : 20
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
            Layout.preferredHeight: root.exportTargetModel !== null && root.exportTargetModel !== undefined && root.exportTargetModel.currentIndex === 3 ? allegroInfoBox.implicitHeight + AppStyle.spacing.md * 2 : 0
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
                opacity: root.exportTargetModel !== null && root.exportTargetModel !== undefined && root.exportTargetModel.currentIndex === 3 ? 1 : 0
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

        // ==================== 运行策略（滑块式导出模式选择） ====================
        SidebarSection {
            title: qsTranslate("MainWindow", "运行策略")
            // 导出模式：滑块式二选一
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
                        width: parent.width / 2
                        height: parent.height - 4
                        anchors.verticalCenter: parent.verticalCenter
                        radius: AppStyle.radius.sm - 1
                        color: AppStyle.colors.surface
                        border.width: AppStyle.borderWidths.thin
                        border.color: AppStyle.colors.border
                        x: (root.exportSettingsController ? root.exportSettingsController.exportMode : 0) === 0 ? 2 : parent.width / 2
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
                            model: [qsTranslate("MainWindow", "追加"), qsTranslate("MainWindow", "覆盖")]
                            Item {
                                width: parent.width / 2
                                height: parent.height
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
                    text: (root.exportSettingsController ? root.exportSettingsController.exportMode : 0) === 0 ? qsTranslate("MainWindow", "保留已有元器件，追加新的") : qsTranslate("MainWindow", "覆盖已有元器件，追加新的")
                    font.pixelSize: AppStyle.fontSizes.xs - 1
                    color: AppStyle.colors.textSecondary
                    opacity: 0.7
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
            visible: !ResponsiveHelper.isShortWindow && (root.exportTargetModel === null || root.exportTargetModel === undefined || root.exportTargetModel.currentIndex === 0)
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
