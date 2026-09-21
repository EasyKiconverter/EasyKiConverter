import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import EasyKiconverter_Cpp_Version.src.ui.qml.styles 1.0

/**
 * @brief Allegro 符号、封装和三维语义 Import Package 设置卡片。
 * @details 输出规范化语义包，不直接写入 Allegro 原生数据库。
 */
ColumnLayout {
    id: allegroCard
    property var exportSettingsController
    spacing: AppStyle.spacing.md

    RowLayout {
        Layout.fillWidth: true
        spacing: AppStyle.spacing.lg
        StyledCheckBox {
            text: qsTranslate("MainWindow", "符号 Import Package")
            ToolTip.text: qsTranslate("MainWindow", "将符号和引脚语义写入 Allegro Import Package")
            checked: allegroCard.exportSettingsController ? allegroCard.exportSettingsController.exportSymbol : false
            onCheckedChanged: {
                if (allegroCard.exportSettingsController)
                    allegroCard.exportSettingsController.setExportSymbol(checked);
            }
        }
        StyledCheckBox {
            text: qsTranslate("MainWindow", "封装 Import Package")
            ToolTip.text: qsTranslate("MainWindow", "生成 Allegro 封装导入包，不直接生成 .dra/.psm/.pad")
            checked: allegroCard.exportSettingsController ? allegroCard.exportSettingsController.exportFootprint : false
            onCheckedChanged: {
                if (allegroCard.exportSettingsController)
                    allegroCard.exportSettingsController.setExportFootprint(checked);
            }
        }
        StyledCheckBox {
            text: qsTranslate("MainWindow", "3D模型 (STEP)")
            ToolTip.text: qsTranslate("MainWindow", "将 STEP 数据和坐标变换写入 Allegro Import Package")
            checked: allegroCard.exportSettingsController ? allegroCard.exportSettingsController.exportModel3D : false
            onCheckedChanged: {
                if (allegroCard.exportSettingsController) {
                    allegroCard.exportSettingsController.setExportModel3D(checked);
                    if (checked)
                        allegroCard.exportSettingsController.setExportModel3DFormat(2);
                }
            }
        }
    }

    Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: allegroInfo.implicitHeight + AppStyle.spacing.md * 2
        radius: AppStyle.radius.sm
        color: AppStyle.colors.surface
        border.color: AppStyle.colors.border
        border.width: 1
        Text {
            id: allegroInfo
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
