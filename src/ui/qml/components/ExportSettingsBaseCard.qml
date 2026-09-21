import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import EasyKiconverter_Cpp_Version.src.ui.qml.styles 1.0

/**
 * @brief 通用导出设置卡片
 * @details 包含所有目标格式共享的设置项，以及目标格式切换下拉框。
 *          目标特定的设置通过 Loader 动态加载对应子卡片组件。
 */
Card {
    id: baseCard
    /** @brief 导出设置控制器（ExportSettingsViewModel） */
    property var exportSettingsController
    /** @brief 导出目标模型（ExportTargetModel） */
    property var exportTargetModel
    signal openOutputFolderDialog
    signal openCacheFolderDialog
    title: qsTranslate("MainWindow", "导出设置")
    ColumnLayout {
        id: rootLayout
        width: parent.width
        spacing: AppStyle.spacing.lg
        anchors.margins: AppStyle.spacing.md
        // ==================== 目标格式选择 ====================
        Text {
            Layout.fillWidth: true
            text: qsTranslate("MainWindow", "目标格式")
            font.pixelSize: AppStyle.fontSizes.md
            font.bold: true
            color: AppStyle.colors.textPrimary
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: AppStyle.spacing.lg
            Repeater {
                model: baseCard.exportTargetModel ? baseCard.exportTargetModel.availableTargets : []
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 72
                    radius: AppStyle.radius.lg
                    property bool isActive: baseCard.exportTargetModel ? baseCard.exportTargetModel.currentIndex === index : false
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

                    // 选中指示条
                    Rectangle {
                        width: 3
                        height: parent.height * 0.5
                        anchors.left: parent.left
                        anchors.leftMargin: 6
                        anchors.verticalCenter: parent.verticalCenter
                        radius: 2
                        color: AppStyle.colors.primary
                        visible: isActive
                        opacity: isActive ? 1 : 0
                        Behavior on opacity {
                            NumberAnimation {
                                duration: AppStyle.durations.fast
                            }
                        }
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: AppStyle.spacing.md
                        anchors.leftMargin: AppStyle.spacing.lg
                        spacing: 2
                        Text {
                            text: modelData.displayName || ""
                            font.pixelSize: AppStyle.fontSizes.md
                            font.bold: true
                            color: isActive ? AppStyle.colors.primary : AppStyle.colors.textPrimary
                        }

                        Text {
                            text: {
                                if (targetId === "kicad")
                                    return qsTranslate("MainWindow", ".kicad_sym / .kicad_mod");
                                if (targetId === "altium")
                                    return qsTranslate("MainWindow", ".SchLib / .PcbLib");
                                if (targetId === "xpedition")
                                    return qsTranslate("MainWindow", "_Symbols.zip / _Footprints.zip");
                                if (targetId === "pads")
                                    return qsTranslate("MainWindow", "PCB Decal .d");
                                if (targetId === "eagle")
                                    return qsTranslate("MainWindow", ".lbr package library");
                                return "";
                            }
                            font.pixelSize: AppStyle.fontSizes.xs
                            color: AppStyle.colors.textSecondary
                        }
                    }

                    // 选中勾
                    Text {
                        anchors.right: parent.right
                        anchors.rightMargin: AppStyle.spacing.md
                        anchors.verticalCenter: parent.verticalCenter
                        text: "✓"
                        font.pixelSize: AppStyle.fontSizes.lg
                        font.bold: true
                        color: AppStyle.colors.primary
                        visible: isActive
                        opacity: isActive ? 1 : 0
                        scale: isActive ? 1 : 0.5
                        Behavior on opacity {
                            NumberAnimation {
                                duration: AppStyle.durations.fast
                            }
                        }
                        Behavior on scale {
                            NumberAnimation {
                                duration: AppStyle.durations.fast
                                easing.type: Easing.BackOut
                            }
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (baseCard.exportTargetModel) {
                                baseCard.exportTargetModel.currentIndex = index;
                            }
                        }
                    }
                }
            }
        }

        // 分隔线
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: AppStyle.colors.border
        }

        // ==================== 基础配置 ====================
        SettingsSectionHeader {
            title: qsTranslate("MainWindow", "基础配置")
        }

        GridLayout {
            Layout.fillWidth: true
            columns: ResponsiveHelper.isCompact ? 1 : 2
            columnSpacing: AppStyle.spacing.xl
            rowSpacing: AppStyle.spacing.md
            // 输出路径
            ColumnLayout {
                Layout.fillWidth: true
                spacing: AppStyle.spacing.xs
                Text {
                    Layout.fillWidth: true
                    text: qsTranslate("MainWindow", "输出路径")
                    font.pixelSize: AppStyle.fontSizes.sm
                    font.bold: true
                    color: AppStyle.colors.textPrimary
                    horizontalAlignment: Text.AlignHCenter
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: AppStyle.spacing.md
                    TextField {
                        id: outputPathInput
                        Layout.fillWidth: true
                        Layout.preferredHeight: 36
                        text: baseCard.exportSettingsController ? baseCard.exportSettingsController.outputPath : ""
                        onTextChanged: {
                            if (baseCard.exportSettingsController) {
                                baseCard.exportSettingsController.setOutputPath(text);
                            }
                        }
                        placeholderText: qsTranslate("MainWindow", "选择输出目录")
                        font.pixelSize: AppStyle.fontSizes.sm
                        color: AppStyle.colors.textPrimary
                        placeholderTextColor: AppStyle.colors.textSecondary
                        background: Rectangle {
                            color: AppStyle.colors.surface
                            border.color: outputPathInput.focus ? AppStyle.colors.borderFocus : AppStyle.colors.border
                            border.width: outputPathInput.focus ? 2 : 1
                            radius: AppStyle.radius.md
                        }
                    }
                    ModernButton {
                        text: qsTranslate("MainWindow", "浏览")
                        iconName: "folder"
                        font.pixelSize: AppStyle.fontSizes.sm
                        backgroundColor: AppStyle.colors.textSecondary
                        hoverColor: AppStyle.colors.textPrimary
                        pressedColor: AppStyle.colors.textPrimary
                        onClicked: baseCard.openOutputFolderDialog()
                    }
                }
            }

            // 库名称
            ColumnLayout {
                Layout.fillWidth: true
                spacing: AppStyle.spacing.xs
                Text {
                    Layout.fillWidth: true
                    text: qsTranslate("MainWindow", "库名称")
                    font.pixelSize: AppStyle.fontSizes.sm
                    font.bold: true
                    color: AppStyle.colors.textPrimary
                    horizontalAlignment: Text.AlignHCenter
                }
                TextField {
                    id: libNameInput
                    Layout.fillWidth: true
                    Layout.preferredHeight: 36
                    text: baseCard.exportSettingsController ? baseCard.exportSettingsController.libName : ""
                    onTextChanged: {
                        if (baseCard.exportSettingsController) {
                            baseCard.exportSettingsController.setLibName(text);
                        }
                    }
                    placeholderText: qsTranslate("MainWindow", "输入库名称")
                    font.pixelSize: AppStyle.fontSizes.sm
                    color: AppStyle.colors.textPrimary
                    placeholderTextColor: AppStyle.colors.textSecondary
                    background: Rectangle {
                        color: AppStyle.colors.surface
                        border.color: libNameInput.focus ? AppStyle.colors.borderFocus : AppStyle.colors.border
                        border.width: libNameInput.focus ? 2 : 1
                        radius: AppStyle.radius.md
                    }
                }
            }
        }

        // ==================== 缓存配置 ====================
        SettingsSectionHeader {
            title: qsTranslate("MainWindow", "缓存配置")
        }

        GridLayout {
            Layout.fillWidth: true
            columns: ResponsiveHelper.isCompact ? 1 : 2
            columnSpacing: AppStyle.spacing.xl
            rowSpacing: AppStyle.spacing.md
            // 缓存目录
            ColumnLayout {
                Layout.fillWidth: true
                spacing: AppStyle.spacing.xs
                Text {
                    Layout.fillWidth: true
                    text: qsTranslate("MainWindow", "缓存目录")
                    font.pixelSize: AppStyle.fontSizes.sm
                    font.bold: true
                    color: AppStyle.colors.textPrimary
                    horizontalAlignment: Text.AlignHCenter
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: AppStyle.spacing.md
                    TextField {
                        id: cacheDirInput
                        Layout.fillWidth: true
                        Layout.preferredHeight: 36
                        text: baseCard.exportSettingsController ? baseCard.exportSettingsController.cacheDir : ""
                        onTextChanged: {
                            if (baseCard.exportSettingsController) {
                                baseCard.exportSettingsController.setCacheDir(text);
                            }
                        }
                        placeholderText: qsTranslate("MainWindow", "默认缓存目录")
                        font.pixelSize: AppStyle.fontSizes.sm
                        color: AppStyle.colors.textPrimary
                        placeholderTextColor: AppStyle.colors.textSecondary
                        background: Rectangle {
                            color: AppStyle.colors.surface
                            border.color: cacheDirInput.focus ? AppStyle.colors.borderFocus : AppStyle.colors.border
                            border.width: cacheDirInput.focus ? 2 : 1
                            radius: AppStyle.radius.md
                        }
                    }
                    ModernButton {
                        text: qsTranslate("MainWindow", "浏览")
                        iconName: "folder"
                        font.pixelSize: AppStyle.fontSizes.sm
                        backgroundColor: AppStyle.colors.textSecondary
                        hoverColor: AppStyle.colors.textPrimary
                        pressedColor: AppStyle.colors.textPrimary
                        onClicked: baseCard.openCacheFolderDialog()
                    }
                }
            }

            // 磁盘缓存上限
            ColumnLayout {
                Layout.fillWidth: true
                spacing: AppStyle.spacing.xs
                Text {
                    Layout.fillWidth: true
                    text: qsTranslate("MainWindow", "磁盘缓存上限 (MB)")
                    font.pixelSize: AppStyle.fontSizes.sm
                    font.bold: true
                    color: AppStyle.colors.textPrimary
                    horizontalAlignment: Text.AlignHCenter
                }
                TextField {
                    id: diskCacheLimitInput
                    objectName: "diskCacheLimitInput"
                    Layout.fillWidth: true
                    Layout.preferredHeight: 36
                    text: baseCard.exportSettingsController ? baseCard.exportSettingsController.diskCacheLimitMB.toString() : "5120"
                    onEditingFinished: {
                        if (!baseCard.exportSettingsController)
                            return;
                        var value = parseInt(text);
                        if (isNaN(value)) {
                            text = baseCard.exportSettingsController.diskCacheLimitMB.toString();
                            return;
                        }
                        var maximum = baseCard.exportSettingsController.maxDiskCacheLimitMB;
                        var clamped = Math.max(1, Math.min(maximum, value));
                        baseCard.exportSettingsController.setDiskCacheLimitMB(clamped);
                        text = clamped.toString();
                    }
                    validator: IntValidator {
                        bottom: 1
                        top: baseCard.exportSettingsController ? baseCard.exportSettingsController.maxDiskCacheLimitMB : 10240
                    }
                    font.pixelSize: AppStyle.fontSizes.sm
                    color: AppStyle.colors.textPrimary
                    placeholderTextColor: AppStyle.colors.textSecondary
                    background: Rectangle {
                        color: AppStyle.colors.surface
                        border.color: diskCacheLimitInput.focus ? AppStyle.colors.borderFocus : AppStyle.colors.border
                        border.width: diskCacheLimitInput.focus ? 2 : 1
                        radius: AppStyle.radius.md
                    }
                }
            }
        }

        // ==================== 目标特定设置（动态加载） ====================
        SettingsSectionHeader {
            title: baseCard.exportTargetModel ? baseCard.exportTargetModel.currentDisplayName + qsTranslate("MainWindow", " 设置") : qsTranslate("MainWindow", "导出选项")
        }

        // 目标特定设置（动态加载，带明显过渡动画）
        Item {
            id: targetOptionsContainer
            Layout.fillWidth: true
            Layout.preferredHeight: targetOptionsLoader.item ? targetOptionsLoader.item.implicitHeight : 0
            clip: true
            Behavior on Layout.preferredHeight {
                NumberAnimation {
                    duration: 400
                    easing.type: Easing.OutQuart
                }
            }

            Loader {
                id: targetOptionsLoader
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                active: baseCard.exportTargetModel !== null && baseCard.exportTargetModel !== undefined ? baseCard.exportTargetModel.currentOptionsComponent !== "" : false
                source: {
                    if (!baseCard.exportTargetModel)
                        return "";
                    var component = baseCard.exportTargetModel.currentOptionsComponent;
                    if (!component)
                        return "";
                    return "qrc:/qt/qml/EasyKiconverter_Cpp_Version/src/ui/qml/components/" + component;
                }

                onStatusChanged: {
                    if (status === Loader.Ready && item) {
                        // 初始状态：透明 + 下移 + 轻微缩放
                        item.opacity = 0;
                        item.y = 20;
                        item.scale = 0.97;
                        // 启动入场动画
                        enterAnimation.start();
                    }
                }

                Binding {
                    target: targetOptionsLoader.item
                    property: "exportSettingsController"
                    value: baseCard.exportSettingsController
                    when: targetOptionsLoader.status === Loader.Ready
                }
            }

            // 入场动画：淡入 + 上滑 + 缩放恢复
            ParallelAnimation {
                id: enterAnimation
                NumberAnimation {
                    target: targetOptionsLoader.item
                    property: "opacity"
                    from: 0
                    to: 1
                    duration: 500
                    easing.type: Easing.OutCubic
                }
                NumberAnimation {
                    target: targetOptionsLoader.item
                    property: "y"
                    from: 20
                    to: 0
                    duration: 500
                    easing.type: Easing.OutQuart
                }
                NumberAnimation {
                    target: targetOptionsLoader.item
                    property: "scale"
                    from: 0.97
                    to: 1.0
                    duration: 500
                    easing.type: Easing.OutQuart
                }
            }
        }

        // ==================== 通用导出选项 ====================
        SettingsSectionHeader {
            title: qsTranslate("MainWindow", "通用选项")
        }

        Flow {
            Layout.fillWidth: true
            spacing: AppStyle.spacing.lg
            StyledCheckBox {
                text: qsTranslate("MainWindow", "预览图")
                ToolTip.text: qsTranslate("MainWindow", "导出元件预览图")
                checked: baseCard.exportSettingsController ? baseCard.exportSettingsController.exportPreviewImages : false
                onCheckedChanged: {
                    if (baseCard.exportSettingsController)
                        baseCard.exportSettingsController.setExportPreviewImages(checked);
                }
            }

            StyledCheckBox {
                text: qsTranslate("MainWindow", "数据手册")
                ToolTip.text: qsTranslate("MainWindow", "导出元件数据手册")
                checked: baseCard.exportSettingsController ? baseCard.exportSettingsController.exportDatasheet : false
                onCheckedChanged: {
                    if (baseCard.exportSettingsController)
                        baseCard.exportSettingsController.setExportDatasheet(checked);
                }
            }
        }

        // ==================== 导出模式 ====================
        SettingsSectionHeader {
            title: qsTranslate("MainWindow", "导出模式")
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: AppStyle.spacing.xl
            RadioButton {
                id: appendModeRadio
                text: qsTranslate("MainWindow", "追加模式")
                checked: baseCard.exportSettingsController ? baseCard.exportSettingsController.exportMode === 0 : true
                onCheckedChanged: {
                    if (checked && baseCard.exportSettingsController) {
                        baseCard.exportSettingsController.setExportMode(0);
                    }
                }
                font.pixelSize: AppStyle.fontSizes.sm
                indicator: Rectangle {
                    implicitWidth: AppStyle.sizes.radioButton
                    implicitHeight: AppStyle.sizes.radioButton
                    x: appendModeRadio.leftPadding
                    y: parent.height / 2 - height / 2
                    radius: AppStyle.sizes.radioButton / 2
                    color: "transparent"
                    border.color: appendModeRadio.checked ? AppStyle.colors.primary : AppStyle.colors.textSecondary
                    border.width: AppStyle.borderWidths.normal
                    Rectangle {
                        width: AppStyle.sizes.radioButtonIndicator
                        height: AppStyle.sizes.radioButtonIndicator
                        anchors.centerIn: parent
                        radius: AppStyle.sizes.radioButtonIndicator / 2
                        color: AppStyle.colors.primary
                        visible: appendModeRadio.checked
                    }
                }
                contentItem: Text {
                    text: appendModeRadio.text
                    font: appendModeRadio.font
                    color: AppStyle.colors.textPrimary
                    verticalAlignment: Text.AlignVCenter
                    leftPadding: appendModeRadio.indicator.width + appendModeRadio.spacing
                }
            }

            RadioButton {
                id: updateModeRadio
                text: qsTranslate("MainWindow", "更新模式")
                checked: baseCard.exportSettingsController ? baseCard.exportSettingsController.exportMode === 1 : false
                onCheckedChanged: {
                    if (checked && baseCard.exportSettingsController) {
                        baseCard.exportSettingsController.setExportMode(1);
                    }
                }
                font.pixelSize: AppStyle.fontSizes.sm
                indicator: Rectangle {
                    implicitWidth: AppStyle.sizes.radioButton
                    implicitHeight: AppStyle.sizes.radioButton
                    x: updateModeRadio.leftPadding
                    y: parent.height / 2 - height / 2
                    radius: AppStyle.sizes.radioButton / 2
                    color: "transparent"
                    border.color: updateModeRadio.checked ? AppStyle.colors.primary : AppStyle.colors.textSecondary
                    border.width: AppStyle.borderWidths.normal
                    Rectangle {
                        width: AppStyle.sizes.radioButtonIndicator
                        height: AppStyle.sizes.radioButtonIndicator
                        anchors.centerIn: parent
                        radius: AppStyle.sizes.radioButtonIndicator / 2
                        color: AppStyle.colors.primary
                        visible: updateModeRadio.checked
                    }
                }
                contentItem: Text {
                    text: updateModeRadio.text
                    font: updateModeRadio.font
                    color: AppStyle.colors.textPrimary
                    verticalAlignment: Text.AlignVCenter
                    leftPadding: updateModeRadio.indicator.width + updateModeRadio.spacing
                }
            }
        }
    }
}
