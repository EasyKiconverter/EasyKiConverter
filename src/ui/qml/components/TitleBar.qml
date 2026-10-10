import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import EasyKiconverter_Cpp_Version.src.ui.qml.styles 1.0

Rectangle {
    id: titleBar
    // 外部属性
    property int windowRadius: 0
    property var windowController
    property string appVersion: ""
    signal versionClicked
    width: parent.width
    height: 38
    color: AppStyle.colors.surface
    topLeftRadius: windowRadius
    topRightRadius: windowRadius
    z: 1000
    // Bottom separator line
    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width
        height: 1
        color: AppStyle.colors.border
    }

    // 拖动区域
    MouseArea {
        anchors.fill: parent
        property point clickPos: "0,0"
        onPressed: mouse => {
            clickPos = Qt.point(mouse.x, mouse.y);
            Window.window.startSystemMove();
        }
        onDoubleClicked: {
            if (titleBar.windowController) {
                titleBar.windowController.toggleMaximize();
            } else if (Window.window.visibility === Window.Maximized) {
                Window.window.showNormal();
            } else {
                Window.window.showMaximized();
            }
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0
        // 图标
        Image {
            source: "qrc:/qt/qml/EasyKiconverter_Cpp_Version/resources/icons/app_icon.svg"
            Layout.preferredWidth: 20
            Layout.preferredHeight: 20
            Layout.leftMargin: 10
            fillMode: Image.PreserveAspectFit
            mipmap: true
        }

        // 标题
        Text {
            text: "EasyKiConverter"
            color: AppStyle.colors.textPrimary
            font.pixelSize: AppStyle.titleBar.fontSize
            font.bold: true
            Layout.leftMargin: 12
            Layout.alignment: Qt.AlignVCenter
        }

        // 在顶部左侧常驻显示构建时注入的当前版本。
        Text {
            id: versionText
            text: appVersion.length > 0 ? qsTr("v%1").arg(appVersion) : ""
            color: versionMouseArea.containsMouse ? AppStyle.colors.primary : AppStyle.colors.textSecondary
            font.pixelSize: AppStyle.fontSizes.xs
            Layout.leftMargin: 8
            Layout.alignment: Qt.AlignVCenter
            ToolTip.visible: versionMouseArea.containsMouse
            ToolTip.text: qsTranslate("MainWindow", "版本更新")
            ToolTip.delay: 500
            MouseArea {
                id: versionMouseArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: titleBar.versionClicked()
            }
        }

        Item {
            Layout.fillWidth: true
        }

        // 窗口控制按钮
        Row {
            Layout.alignment: Qt.AlignRight
            // 最小化
            Button {
                width: 46
                height: 38
                flat: true
                icon.source: "qrc:/qt/qml/EasyKiconverter_Cpp_Version/resources/icons/minimize.svg"
                icon.color: "transparent"
                icon.width: 10
                icon.height: 10
                background: Rectangle {
                    color: parent.hovered ? (AppStyle.isDarkMode ? AppStyle.titleBar.hoverOverlayDark : AppStyle.titleBar.hoverOverlayLight) : "transparent"
                    Behavior on color {
                        ColorAnimation {
                            duration: 150
                        }
                    }
                }

                onClicked: {
                    if (titleBar.windowController) {
                        titleBar.windowController.requestMinimize();
                    } else if (Window.window) {
                        Window.window.showMinimized();
                    }
                }
            }

            // 最大化/还原
            Button {
                width: 46
                height: 38
                flat: true
                icon.source: "qrc:/qt/qml/EasyKiconverter_Cpp_Version/resources/icons/maximize.svg"
                icon.color: "transparent"
                icon.width: 10
                icon.height: 10
                background: Rectangle {
                    color: parent.hovered ? (AppStyle.isDarkMode ? AppStyle.titleBar.hoverOverlayDark : AppStyle.titleBar.hoverOverlayLight) : "transparent"
                    Behavior on color {
                        ColorAnimation {
                            duration: 150
                        }
                    }
                }

                onClicked: {
                    if (titleBar.windowController) {
                        titleBar.windowController.toggleMaximize();
                    } else if (Window.window.visibility === Window.Maximized) {
                        Window.window.showNormal();
                    } else {
                        Window.window.showMaximized();
                    }
                }
            }

            // 关闭
            Button {
                width: 46
                height: 38
                flat: true
                icon.source: "qrc:/qt/qml/EasyKiconverter_Cpp_Version/resources/icons/close.svg"
                icon.color: hovered ? AppStyle.colors.textOnPrimary : "transparent"
                icon.width: 10
                icon.height: 10
                background: Rectangle {
                    color: parent.hovered ? AppStyle.titleBar.closeButton : "transparent"
                    Behavior on color {
                        ColorAnimation {
                            duration: 150
                        }
                    }
                }

                onClicked: {
                    if (titleBar.windowController) {
                        titleBar.windowController.requestClose();
                    } else {
                        Window.window.close();
                    }
                }
            }
        }
    }
}
