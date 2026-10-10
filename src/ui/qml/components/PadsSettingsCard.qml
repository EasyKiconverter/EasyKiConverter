import QtQuick
import QtQuick.Layouts
import EasyKiconverter_Cpp_Version.src.ui.qml.styles 1.0

/**
 * @brief PADS ASCII 库导出设置卡片。
 * @details PADS 输出 Schematic Decal、Part Type 器件关联、PCB Decal 封装和独立三维模型文件。
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
            text: qsTranslate("MainWindow", "PADS 导出说明：\n" + "- 符号输出为 ASCII Schematic Decal（.c）和 Part Type（.p）\n" + "- 封装输出为 ASCII PCB Decal（.d）\n" + "- 3D 模型由独立阶段输出，不写入原生关联\n" + "- 不支持更新、追加和重试模式\n" + "- 无法无损表达的图元会在导出诊断中报告错误")
            font.pixelSize: AppStyle.fontSizes.xs
            color: AppStyle.colors.textSecondary
            wrapMode: Text.WordWrap
            lineHeight: 1.4
        }
    }
}
