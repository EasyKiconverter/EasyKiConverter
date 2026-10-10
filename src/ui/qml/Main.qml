import QtQuick
import QtQuick.Controls
import QtQuick.Window
import "components"
import "styles"

ApplicationWindow {
    id: appWindow
    property alias windowController: appWindowController
    property int defaultWidth: Math.max(900, Screen.desktopAvailableWidth * 0.68)
    property int defaultHeight: Math.max(680, Screen.desktopAvailableHeight * 0.72)
    property int windowRadius: visibility === Window.Maximized ? 0 : AppStyle.radius.lg
    property int dynamicMinimumWidth: 855
    width: defaultWidth
    height: defaultHeight
    x: 0
    y: 0
    minimumWidth: dynamicMinimumWidth
    minimumHeight: 620
    visible: false
    title: qsTr("EasyKiConverter - 元器件转换工具")
    color: "transparent"
    flags: Qt.Window | Qt.FramelessWindowHint | Qt.CustomizeWindowHint | Qt.WindowMinimizeButtonHint | Qt.WindowMaximizeButtonHint
    // 将 context property 显式绑定到 ApplicationWindow 属性，避免 WindowController 内同名属性自引用
    property var _configService: configService
    WindowController {
        id: appWindowController
        window: appWindow
        configService: appWindow._configService
        exportProgressController: exportProgressViewModel
        closeConfirmDialog: closeConfirmDialog
        exitOptionDialog: exitOptionDialog
    }

    ConfirmDialog {
        id: closeConfirmDialog
        title: qsTr("确认退出")
        message: qsTr("转换正在进行中。退出将取消当前转换，已导出的文件会保留。确定要退出吗？")
        confirmText: qsTr("强制退出")
        cancelText: qsTr("继续转换")
        confirmColor: AppStyle.colors.danger
        onAccepted: appWindowController.confirmExit()
        onRejected: appWindowController.resumeExport()
    }

    ExitDialog {
        id: exitOptionDialog
        onMinimizeToTray: function (remember) {
            if (remember && configService) {
                configService.setExitPreference("minimize");
            }
            appWindowController.requestMinimize();
        }
        onExitApp: function (remember) {
            if (remember && configService) {
                configService.setExitPreference("exit");
            }
            appWindowController.confirmExit();
        }
        onCanceled: appWindowController.resumeExport()
    }

    CacheDirectoryErrorDialog {
        id: cacheDirectoryErrorDialog
        property string pendingPath: ""
        property string pendingReason: ""
        selectedPath: pendingPath
        rejectionReason: pendingReason
        onRetrySelection: {
            if (mainWindowLoader.item && mainWindowLoader.item.cacheFolderDialog)
                mainWindowLoader.item.cacheFolderDialog.open();
        }
    }

    Connections {
        target: exportSettingsViewModel
        function onCacheDirChangeRejected(path, reason) {
            cacheDirectoryErrorDialog.pendingPath = path;
            cacheDirectoryErrorDialog.pendingReason = reason;
            cacheDirectoryErrorDialog.open();
        }
    }

    Shortcut {
        sequence: "Esc"
        onActivated: appWindowController.handleEsc()
    }

    onClosing: close => appWindowController.handleClosing(close)
    Component.onCompleted: {
        appWindowController.initializeWindow();
    }

    Item {
        anchors.fill: parent
        Loader {
            id: mainWindowLoader
            anchors.fill: parent
            source: "qrc:/qt/qml/EasyKiconverter_Cpp_Version/src/ui/qml/MainWindow.qml"
            onLoaded: {
                if (item && item.calculatedMinimumWindowWidth) {
                    appWindow.dynamicMinimumWidth = item.calculatedMinimumWindowWidth;
                }
            }
        }

        Binding {
            target: appWindow
            property: "dynamicMinimumWidth"
            value: mainWindowLoader.item ? mainWindowLoader.item.calculatedMinimumWindowWidth : 855
            when: mainWindowLoader.status === Loader.Ready
        }
    }
}
