import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Shapes
import EasyKiconverter_Cpp_Version.src.ui.qml.styles 1.0

ColumnLayout {
    id: root
    property var targetModel
    spacing: AppStyle.spacing.sm
    function descriptionFor(targetId) {
        if (targetId === "kicad")
            return qsTranslate("MainWindow", ".kicad_sym / .kicad_mod");
        if (targetId === "altium")
            return qsTranslate("MainWindow", ".SchLib / .PcbLib");
        if (targetId === "xpedition")
            return qsTranslate("MainWindow", "_Symbols.zip / _Footprints.zip");
        if (targetId === "pads")
            return qsTranslate("MainWindow", ".c/.p/.d 符号、器件和 PCB 封装");
        if (targetId === "eagle")
            return qsTranslate("MainWindow", ".lbr 符号、封装和器件");
        if (targetId === "pcad")
            return qsTranslate("MainWindow", ".lia 原理图和 PCB 库");
        if (targetId === "cadstar")
            return qsTranslate("MainWindow", ".lib 符号、封装和器件");
        if (targetId === "allegro")
            return qsTranslate("MainWindow", "符号、封装导入包和 3D 数据");
        if (targetId === "orcad")
            return qsTranslate("MainWindow", "OrCAD XML 符号库（封装名称关联）");
        return "";
    }

    Text {
        Layout.fillWidth: true
        text: qsTranslate("MainWindow", "目标格式")
        font.pixelSize: AppStyle.fontSizes.sm
        font.bold: true
        color: AppStyle.colors.textPrimary
    }

    ComboBox {
        id: targetCombo
        Layout.fillWidth: true
        Layout.preferredHeight: 44
        model: root.targetModel ? root.targetModel.availableTargets : []
        textRole: "displayName"
        currentIndex: root.targetModel ? root.targetModel.currentIndex : -1
        font.pixelSize: AppStyle.fontSizes.sm
        onActivated: function (index) {
            if (root.targetModel)
                root.targetModel.currentIndex = index;
        }

        background: Rectangle {
            color: AppStyle.colors.surface
            border.color: targetCombo.activeFocus || targetCombo.hovered ? AppStyle.colors.borderFocus : AppStyle.colors.border
            border.width: AppStyle.borderWidths.thin
            radius: AppStyle.radius.md
            Behavior on border.color {
                ColorAnimation {
                    duration: 150
                }
            }
        }

        contentItem: Text {
            leftPadding: AppStyle.spacing.md
            rightPadding: targetCombo.indicator.width + AppStyle.spacing.md
            text: targetCombo.displayText
            font: targetCombo.font
            color: AppStyle.colors.textPrimary
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        indicator: Shape {
            id: targetIndicator
            x: targetCombo.width - width - targetCombo.rightPadding
            y: targetCombo.topPadding + (targetCombo.availableHeight - height) / 2
            width: 12
            height: 8
            rotation: targetCombo.popup.visible ? 180 : 0
            Behavior on rotation {
                NumberAnimation {
                    duration: 150
                }
            }

            ShapePath {
                strokeWidth: 0
                fillColor: AppStyle.colors.textSecondary
                PathMove {
                    x: 0
                    y: 0
                }
                PathLine {
                    x: targetIndicator.width / 2
                    y: targetIndicator.height
                }
                PathLine {
                    x: targetIndicator.width
                    y: 0
                }
                PathLine {
                    x: 0
                    y: 0
                }
            }
        }

        delegate: ItemDelegate {
            width: targetCombo.width
            height: 40
            contentItem: Text {
                text: modelData.displayName
                font.pixelSize: AppStyle.fontSizes.sm
                color: AppStyle.colors.textPrimary
                verticalAlignment: Text.AlignVCenter
                leftPadding: AppStyle.spacing.md
                elide: Text.ElideRight
            }

            background: Rectangle {
                color: parent.highlighted ? (AppStyle.isDarkMode ? "#334155" : "#e2e8f0") : "transparent"
            }
        }

        popup: Popup {
            y: targetCombo.height - 1
            width: targetCombo.width
            padding: 0
            implicitHeight: Math.min(targetListView.contentHeight, 9 * 40)
            contentItem: ListView {
                id: targetListView
                clip: true
                model: targetCombo.popup.visible ? targetCombo.delegateModel : null
                currentIndex: targetCombo.highlightedIndex
                ScrollBar.vertical: ScrollBar {
                    policy: ScrollBar.AsNeeded
                }
            }

            background: Rectangle {
                color: AppStyle.colors.surface
                border.color: AppStyle.colors.border
                border.width: AppStyle.borderWidths.thin
                radius: AppStyle.radius.md
            }
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: AppStyle.spacing.sm
        visible: targetModel && targetModel.currentIndex >= 0 && targetModel.currentTargetId.length > 0
        Rectangle {
            Layout.preferredWidth: 3
            Layout.fillHeight: true
            Layout.minimumHeight: 20
            radius: 2
            color: AppStyle.colors.primary
        }

        Text {
            Layout.fillWidth: true
            text: root.targetModel ? root.descriptionFor(root.targetModel.currentTargetId) : ""
            font.pixelSize: AppStyle.fontSizes.xs
            color: AppStyle.colors.textSecondary
            wrapMode: Text.WordWrap
        }
    }
}
