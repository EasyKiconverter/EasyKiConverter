import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import EasyKiconverter_Cpp_Version.src.ui.qml.styles 1.0

/**
 * @brief KiCad 特有设置卡片
 * @details 包含 KiCad 格式独有的导出选项：符号库/封装库开关、3D 模型格式、库描述等。
 */
ColumnLayout {
    id: kiCadCard
    /** @brief 导出设置控制器 */
    property var exportSettingsController
    /** @brief 当前导出目标模型，用于隐藏不适用的外部模型路径设置 */
    property var exportTargetModel
    spacing: AppStyle.spacing.md
    Flow {
        Layout.fillWidth: true
        spacing: AppStyle.spacing.lg
        StyledCheckBox {
            id: symbolCheckbox
            text: qsTranslate("MainWindow", "符号库")
            ToolTip.text: qsTranslate("MainWindow", "导出 .kicad_sym 符号库文件")
            checked: kiCadCard.exportSettingsController ? kiCadCard.exportSettingsController.exportSymbol : false
            onCheckedChanged: {
                if (kiCadCard.exportSettingsController)
                    kiCadCard.exportSettingsController.setExportSymbol(checked);
            }
        }

        StyledCheckBox {
            id: footprintCheckbox
            text: qsTranslate("MainWindow", "封装库")
            ToolTip.text: qsTranslate("MainWindow", "导出 .kicad_mod 封装库文件")
            checked: kiCadCard.exportSettingsController ? kiCadCard.exportSettingsController.exportFootprint : false
            onCheckedChanged: {
                if (kiCadCard.exportSettingsController)
                    kiCadCard.exportSettingsController.setExportFootprint(checked);
            }
        }

        Flow {
            spacing: AppStyle.spacing.sm
            StyledCheckBox {
                id: model3dCheckbox
                text: qsTranslate("MainWindow", "3D模型")
                ToolTip.text: qsTranslate("MainWindow", "导出 WRL/STEP 3D 模型文件")
                checked: kiCadCard.exportSettingsController ? kiCadCard.exportSettingsController.exportModel3D : false
                onCheckedChanged: {
                    if (kiCadCard.exportSettingsController)
                        kiCadCard.exportSettingsController.setExportModel3D(checked);
                }
            }

            // 3D模型格式按钮组
            RowLayout {
                visible: model3dCheckbox.checked
                spacing: AppStyle.spacing.xs
                Rectangle {
                    width: 46
                    height: 26
                    radius: AppStyle.radius.xs
                    property bool wrlActive: kiCadCard.exportSettingsController ? (kiCadCard.exportSettingsController.exportModel3DFormat & 1) !== 0 : false
                    color: wrlActive ? AppStyle.colors.primary : "transparent"
                    border.color: wrlActive ? AppStyle.colors.primary : AppStyle.colors.textSecondary
                    border.width: AppStyle.borderWidths.normal
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            var current = kiCadCard.exportSettingsController.exportModel3DFormat;
                            if ((current & 1) !== 0) {
                                var newFormat = current & ~1;
                                if (newFormat === 0)
                                    kiCadCard.exportSettingsController.setExportModel3D(false);
                                else
                                    kiCadCard.exportSettingsController.setExportModel3DFormat(newFormat);
                            } else {
                                kiCadCard.exportSettingsController.setExportModel3DFormat(current | 1);
                            }
                        }
                    }
                    Text {
                        anchors.centerIn: parent
                        text: "WRL"
                        font.pixelSize: AppStyle.fontSizes.xs
                        color: parent.wrlActive ? AppStyle.colors.textOnPrimary : AppStyle.colors.textSecondary
                    }
                }
                Rectangle {
                    width: 50
                    height: 26
                    radius: AppStyle.radius.xs
                    property bool stepActive: kiCadCard.exportSettingsController ? (kiCadCard.exportSettingsController.exportModel3DFormat & 2) !== 0 : false
                    color: stepActive ? AppStyle.colors.primary : "transparent"
                    border.color: stepActive ? AppStyle.colors.primary : AppStyle.colors.textSecondary
                    border.width: AppStyle.borderWidths.normal
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            var current = kiCadCard.exportSettingsController.exportModel3DFormat;
                            if ((current & 2) !== 0) {
                                var newFormat = current & ~2;
                                if (newFormat === 0)
                                    kiCadCard.exportSettingsController.setExportModel3D(false);
                                else
                                    kiCadCard.exportSettingsController.setExportModel3DFormat(newFormat);
                            } else {
                                kiCadCard.exportSettingsController.setExportModel3DFormat(current | 2);
                            }
                        }
                    }
                    Text {
                        anchors.centerIn: parent
                        text: "STEP"
                        font.pixelSize: AppStyle.fontSizes.xs
                        color: parent.stepActive ? AppStyle.colors.textOnPrimary : AppStyle.colors.textSecondary
                    }
                }
            }

            // 3D模型路径模式
            RowLayout {
                visible: model3dCheckbox.checked && (!kiCadCard.exportTargetModel || (kiCadCard.exportTargetModel.currentTargetId !== "altium" && kiCadCard.exportTargetModel.currentTargetId !== "allegro" && kiCadCard.exportTargetModel.currentTargetId !== "librepcb" && kiCadCard.exportTargetModel.currentTargetId !== "horizon"))
                spacing: AppStyle.spacing.xs
                Rectangle {
                    width: 56
                    height: 26
                    radius: AppStyle.radius.xs
                    property bool isActive: kiCadCard.exportSettingsController ? kiCadCard.exportSettingsController.exportModel3DPathMode === 0 : true
                    color: isActive ? AppStyle.colors.primary : "transparent"
                    border.color: isActive ? AppStyle.colors.primary : AppStyle.colors.textSecondary
                    border.width: AppStyle.borderWidths.normal
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (kiCadCard.exportSettingsController)
                                kiCadCard.exportSettingsController.setExportModel3DPathMode(0);
                        }
                    }
                    Text {
                        anchors.centerIn: parent
                        text: qsTranslate("MainWindow", "相对")
                        font.pixelSize: AppStyle.fontSizes.xs
                        color: parent.isActive ? AppStyle.colors.textOnPrimary : AppStyle.colors.textSecondary
                    }
                }
                Rectangle {
                    width: 56
                    height: 26
                    radius: AppStyle.radius.xs
                    property bool isActive: kiCadCard.exportSettingsController ? kiCadCard.exportSettingsController.exportModel3DPathMode === 1 : false
                    color: isActive ? AppStyle.colors.primary : "transparent"
                    border.color: isActive ? AppStyle.colors.primary : AppStyle.colors.textSecondary
                    border.width: AppStyle.borderWidths.normal
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (kiCadCard.exportSettingsController)
                                kiCadCard.exportSettingsController.setExportModel3DPathMode(1);
                        }
                    }
                    Text {
                        anchors.centerIn: parent
                        text: qsTranslate("MainWindow", "绝对")
                        font.pixelSize: AppStyle.fontSizes.xs
                        color: parent.isActive ? AppStyle.colors.textOnPrimary : AppStyle.colors.textSecondary
                    }
                }
            }
        }
    }

    // 库描述
    GridLayout {
        Layout.fillWidth: true
        columns: ResponsiveHelper.isCompact ? 1 : 2
        columnSpacing: AppStyle.spacing.xl
        rowSpacing: AppStyle.spacing.md
        ColumnLayout {
            Layout.fillWidth: true
            spacing: AppStyle.spacing.xs
            Text {
                Layout.fillWidth: true
                text: qsTranslate("MainWindow", "符号库描述")
                font.pixelSize: AppStyle.fontSizes.sm
                font.bold: true
                color: AppStyle.colors.textPrimary
                horizontalAlignment: Text.AlignHCenter
            }
            TextField {
                id: symbolDescInput
                Layout.fillWidth: true
                Layout.preferredHeight: 36
                text: kiCadCard.exportSettingsController ? kiCadCard.exportSettingsController.symbolLibraryDescription : ""
                onTextChanged: {
                    if (kiCadCard.exportSettingsController)
                        kiCadCard.exportSettingsController.setSymbolLibraryDescription(text);
                }
                placeholderText: qsTranslate("MainWindow", "输入符号库描述")
                font.pixelSize: AppStyle.fontSizes.sm
                color: AppStyle.colors.textPrimary
                placeholderTextColor: AppStyle.colors.textSecondary
                background: Rectangle {
                    color: AppStyle.colors.surface
                    border.color: symbolDescInput.focus ? AppStyle.colors.borderFocus : AppStyle.colors.border
                    border.width: symbolDescInput.focus ? 2 : 1
                    radius: AppStyle.radius.md
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: AppStyle.spacing.xs
            Text {
                Layout.fillWidth: true
                text: qsTranslate("MainWindow", "封装库描述")
                font.pixelSize: AppStyle.fontSizes.sm
                font.bold: true
                color: AppStyle.colors.textPrimary
                horizontalAlignment: Text.AlignHCenter
            }
            TextField {
                id: footprintDescInput
                Layout.fillWidth: true
                Layout.preferredHeight: 36
                text: kiCadCard.exportSettingsController ? kiCadCard.exportSettingsController.footprintLibraryDescription : ""
                onTextChanged: {
                    if (kiCadCard.exportSettingsController)
                        kiCadCard.exportSettingsController.setFootprintLibraryDescription(text);
                }
                placeholderText: qsTranslate("MainWindow", "输入封装库描述")
                font.pixelSize: AppStyle.fontSizes.sm
                color: AppStyle.colors.textPrimary
                placeholderTextColor: AppStyle.colors.textSecondary
                background: Rectangle {
                    color: AppStyle.colors.surface
                    border.color: footprintDescInput.focus ? AppStyle.colors.borderFocus : AppStyle.colors.border
                    border.width: footprintDescInput.focus ? 2 : 1
                    radius: AppStyle.radius.md
                }
            }
        }
    }
}
