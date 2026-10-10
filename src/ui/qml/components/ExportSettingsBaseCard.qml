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
        TargetFormatSelector {
            Layout.fillWidth: true
            targetModel: baseCard.exportTargetModel
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
                Text {
                    visible: baseCard.exportTargetModel && baseCard.exportTargetModel.currentTargetId === "librepcb"
                    Layout.fillWidth: true
                    text: qsTranslate("MainWindow", "LibrePCB：项目根目录或 project/library 可自动安装；普通目录需将生成的 .lplib 复制到 Workspace/data/libraries/local/。")
                    font.pixelSize: AppStyle.fontSizes.xs
                    color: AppStyle.colors.textSecondary
                    wrapMode: Text.WordWrap
                    lineHeight: 1.25
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
                        onEditingFinished: {
                            if (baseCard.exportSettingsController) {
                                baseCard.exportSettingsController.setCacheDir(text);
                                text = baseCard.exportSettingsController.cacheDir;
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
                text: baseCard.exportSettingsController && baseCard.exportSettingsController.requiresFullReplacement ? qsTranslate("MainWindow", "追加模式（不支持）") : qsTranslate("MainWindow", "追加模式")
                enabled: !(baseCard.exportSettingsController && baseCard.exportSettingsController.requiresFullReplacement)
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
                    border.color: !appendModeRadio.enabled ? AppStyle.colors.textDisabled : appendModeRadio.checked ? AppStyle.colors.primary : AppStyle.colors.textSecondary
                    border.width: AppStyle.borderWidths.normal
                    Rectangle {
                        width: AppStyle.sizes.radioButtonIndicator
                        height: AppStyle.sizes.radioButtonIndicator
                        anchors.centerIn: parent
                        radius: AppStyle.sizes.radioButtonIndicator / 2
                        color: appendModeRadio.enabled ? AppStyle.colors.primary : AppStyle.colors.textDisabled
                        visible: appendModeRadio.checked
                    }
                }
                contentItem: Text {
                    text: appendModeRadio.text
                    font: appendModeRadio.font
                    color: appendModeRadio.enabled ? AppStyle.colors.textPrimary : AppStyle.colors.textDisabled
                    verticalAlignment: Text.AlignVCenter
                    leftPadding: appendModeRadio.indicator.width + appendModeRadio.spacing
                }
            }

            RadioButton {
                id: updateModeRadio
                text: qsTranslate("MainWindow", "更新模式")
                enabled: !(baseCard.exportSettingsController && baseCard.exportSettingsController.requiresFullReplacement)
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
                    border.color: !updateModeRadio.enabled ? AppStyle.colors.textDisabled : updateModeRadio.checked ? AppStyle.colors.primary : AppStyle.colors.textSecondary
                    border.width: AppStyle.borderWidths.normal
                    Rectangle {
                        width: AppStyle.sizes.radioButtonIndicator
                        height: AppStyle.sizes.radioButtonIndicator
                        anchors.centerIn: parent
                        radius: AppStyle.sizes.radioButtonIndicator / 2
                        color: updateModeRadio.enabled ? AppStyle.colors.primary : AppStyle.colors.textDisabled
                        visible: updateModeRadio.checked
                    }
                }
                contentItem: Text {
                    text: updateModeRadio.text
                    font: updateModeRadio.font
                    color: updateModeRadio.enabled ? AppStyle.colors.textPrimary : AppStyle.colors.textDisabled
                    verticalAlignment: Text.AlignVCenter
                    leftPadding: updateModeRadio.indicator.width + updateModeRadio.spacing
                }
            }

            RadioButton {
                id: overwriteModeRadio
                text: baseCard.exportSettingsController && baseCard.exportSettingsController.requiresFullReplacement ? qsTranslate("MainWindow", "完整覆盖") : qsTranslate("MainWindow", "覆盖模式")
                checked: baseCard.exportSettingsController ? baseCard.exportSettingsController.exportMode === 2 : false
                onCheckedChanged: {
                    if (checked && baseCard.exportSettingsController)
                        baseCard.exportSettingsController.setExportMode(2);
                }
                font.pixelSize: AppStyle.fontSizes.sm
                indicator: Rectangle {
                    implicitWidth: AppStyle.sizes.radioButton
                    implicitHeight: AppStyle.sizes.radioButton
                    x: overwriteModeRadio.leftPadding
                    y: parent.height / 2 - height / 2
                    radius: AppStyle.sizes.radioButton / 2
                    color: "transparent"
                    border.color: overwriteModeRadio.checked ? AppStyle.colors.primary : AppStyle.colors.textSecondary
                    border.width: AppStyle.borderWidths.normal
                    Rectangle {
                        width: AppStyle.sizes.radioButtonIndicator
                        height: AppStyle.sizes.radioButtonIndicator
                        anchors.centerIn: parent
                        radius: AppStyle.sizes.radioButtonIndicator / 2
                        color: AppStyle.colors.primary
                        visible: overwriteModeRadio.checked
                    }
                }
                contentItem: Text {
                    text: overwriteModeRadio.text
                    font: overwriteModeRadio.font
                    color: AppStyle.colors.textPrimary
                    verticalAlignment: Text.AlignVCenter
                    leftPadding: overwriteModeRadio.indicator.width + overwriteModeRadio.spacing
                }
            }
        }

        Text {
            Layout.fillWidth: true
            text: {
                var mode = baseCard.exportSettingsController ? baseCard.exportSettingsController.exportMode : 0;
                if (baseCard.exportSettingsController && baseCard.exportSettingsController.requiresFullReplacement)
                    return qsTranslate("MainWindow", "当前目标仅支持覆盖模式；追加和更新不可用");
                if (mode === 0)
                    return qsTranslate("MainWindow", "追加：保留已有库内容，只加入新的元器件；已有同名内容不会被覆盖");
                if (mode === 1)
                    return qsTranslate("MainWindow", "更新：在已有库基础上处理缺失或变化内容，并保留未参与本次导出的内容");
                return qsTranslate("MainWindow", "覆盖：重新生成完整库并替换已有输出，请确认输出路径中的旧库可以被替换");
            }
            font.pixelSize: AppStyle.fontSizes.xs
            color: AppStyle.colors.textSecondary
            wrapMode: Text.WordWrap
        }
    }
}
