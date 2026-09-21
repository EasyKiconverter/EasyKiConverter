import QtQuick
import QtQuick.Layouts
import EasyKiconverter_Cpp_Version.src.ui.qml.styles 1.0

/**
 * @brief PADS PCB Decal 导出设置卡片。
 * @details PADS 第一阶段只生成 ASCII 封装文件，不提供符号和三维模型选项。
 */
ColumnLayout {
    id: padsCard
    property var exportSettingsController
    spacing: AppStyle.spacing.md

    StyledCheckBox {
        text: qsTranslate("MainWindow", "PADS PCB Decal 封装")
        ToolTip.text: qsTranslate("MainWindow", "生成 PADS Parts Library ASCII .d 封装文件")
        checked: padsCard.exportSettingsController ? padsCard.exportSettingsController.exportFootprint : false
        onCheckedChanged: {
            if (padsCard.exportSettingsController)
                padsCard.exportSettingsController.setExportFootprint(checked);
        }
    }

    Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: padsInfo.implicitHeight + AppStyle.spacing.md * 2
        radius: AppStyle.radius.sm
        color: AppStyle.colors.surface
        border.color: AppStyle.colors.border
        border.width: 1

        Text {
            id: padsInfo
            anchors.fill: parent
            anchors.margins: AppStyle.spacing.md
            text: qsTranslate("MainWindow", "PADS 导出说明：\n" + "- 第一阶段仅支持 ASCII PCB Decal 封装\n" + "- 不生成原理图、原生二进制库或 3D 模型关联\n" + "- 不支持更新、追加和重试模式\n" + "- 无法无损表达的图元会在导出诊断中报告错误")
            font.pixelSize: AppStyle.fontSizes.xs
            color: AppStyle.colors.textSecondary
            wrapMode: Text.WordWrap
            lineHeight: 1.4
        }
    }
}
