import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../styles"

/**
 * @brief 退出选项对话框（无任务时显示）
 *
 * **用途**：当用户点击关闭按钮且无正在进行的任务时显示
 * **场景**：用户点击关闭按钮 → 检测到 exportProgressViewModel.isExporting 为 false → 显示此对话框
 * **选项**：
 *   - 最小化到托盘：最小化窗口到系统托盘，保持后台运行
 *   - 退出程序：完全退出应用程序
 *   - 取消：关闭对话框，继续使用程序
 * **记住选择**：用户可以选择"记住我的选择"，下次自动执行相同操作
 * **动画**：取消按钮点击后向下滑动消失（fade out + slide down + scale down）
 *
 * **注意**：不要与 ConfirmDialog 混淆
 * - ExitDialog：无任务时显示，提供"最小化到托盘"/"退出程序"/"取消"选项
 * - ConfirmDialog：有任务时显示，提供"强制退出"/"继续转换"选项
 */
SliderDialogBase {
    id: root
    // ===== 属性 =====
    property bool rememberChoice: false
    property string focusArea: "button"  // "button" or "checkbox"
    property bool exitAnimationRunning: false
    property bool cancelPending: false
    readonly property int exitFillDuration: AppStyle.durations.normal
    property real exitFillTargetSize: 0
    property real exitFillOriginX: 0
    property real exitFillOriginY: 0
    // ===== 信号 =====
    signal minimizeToTray(bool remember)
    signal exitApp(bool remember)
    signal canceled
    // ===== 按钮规格 =====
    buttonSpecs: [
        {
            text: qsTr("最小化到托盘"),
            color: AppStyle.colors.primary,
            action: function () {
                root.minimizeToTray(rememberChoice);
            }
        },
        {
            isSeparator: true
        },
        {
            text: qsTr("退出程序"),
            color: AppStyle.colors.danger,
            action: function () {
                root.playExitFillAndExit();
            }
        },
        {
            isSeparator: true
        },
        {
            text: qsTr("取消"),
            color: AppStyle.colors.textSecondary,
            action: function () {
                root.cancelPending = true;
                root.closeWithAnimation();
            }
        }
    ]
    // 覆写 title 和 message
    title: qsTr("关闭程序")
    message: qsTr("您可以选择最小化到系统托盘以保持后台运行，或者完全退出程序。")
    // ===== 额外内容：记住选择复选框 ======
    extraContentSource: RowLayout {
        id: rememberLayout
        spacing: AppStyle.spacing.sm
        CheckBox {
            id: rememberCheckBox
            checked: root.rememberChoice
            onClicked: {
                root.rememberChoice = checked;
            }

            indicator: Rectangle {
                implicitWidth: 22
                implicitHeight: 22
                x: rememberCheckBox.leftPadding
                y: parent.height / 2 - height / 2
                radius: AppStyle.radius.xs
                color: rememberCheckBox.checked ? AppStyle.colors.primary : "transparent"
                border.color: rememberCheckBox.checked ? AppStyle.colors.primary : (root.focusArea === "checkbox" ? AppStyle.colors.primary : AppStyle.colors.textSecondary)
                border.width: root.focusArea === "checkbox" ? 2.5 : 1.5
                Behavior on color {
                    ColorAnimation {
                        duration: 150
                    }
                }
                Behavior on border.color {
                    ColorAnimation {
                        duration: 150
                    }
                }
                Behavior on border.width {
                    NumberAnimation {
                        duration: 150
                    }
                }

                Text {
                    anchors.centerIn: parent
                    text: "\u2713"
                    font.pixelSize: AppStyle.fontSizes.md
                    font.bold: true
                    color: AppStyle.colors.textOnPrimary
                    visible: rememberCheckBox.checked
                }
            }
        }

        Text {
            text: qsTr("记住我的选择")
            font.pixelSize: AppStyle.fontSizes.sm
            color: AppStyle.colors.textSecondary
        }
    }

    Rectangle {
        id: exitFillOverlay
        parent: root.dialogBox ? root.dialogBox : root
        x: parent ? parent.width / 2 : 0
        y: parent ? parent.height / 2 : 0
        width: 0
        height: 0
        radius: width / 2
        color: AppStyle.colors.danger
        opacity: 0
        z: 30
        visible: opacity > 0 || root.exitAnimationRunning
    }

    ParallelAnimation {
        id: exitFillAnimation
        NumberAnimation {
            target: exitFillOverlay
            property: "opacity"
            from: 0
            to: 0.92
            duration: root.exitFillDuration
            easing.type: Easing.OutCubic
        }
        NumberAnimation {
            target: exitFillOverlay
            property: "width"
            to: root.exitFillTargetSize
            duration: root.exitFillDuration
            easing.type: Easing.OutCubic
        }
        NumberAnimation {
            target: exitFillOverlay
            property: "height"
            to: root.exitFillTargetSize
            duration: root.exitFillDuration
            easing.type: Easing.OutCubic
        }
        NumberAnimation {
            target: exitFillOverlay
            property: "x"
            to: root.exitFillOriginX - root.exitFillTargetSize / 2
            duration: root.exitFillDuration
            easing.type: Easing.OutCubic
        }
        NumberAnimation {
            target: exitFillOverlay
            property: "y"
            to: root.exitFillOriginY - root.exitFillTargetSize / 2
            duration: root.exitFillDuration
            easing.type: Easing.OutCubic
        }
        NumberAnimation {
            target: root.dialogBox
            property: "scale"
            to: 1.015
            duration: root.exitFillDuration
            easing.type: Easing.OutCubic
        }
    }

    Timer {
        id: exitFillCommitTimer
        interval: root.exitFillDuration
        repeat: false
        onTriggered: {
            root.exitAnimationRunning = false;
            root.exitApp(root.rememberChoice);
        }
    }

    Connections {
        target: root
        function onCloseAnimationFinished() {
            if (!root.cancelPending)
                return;
            root.cancelPending = false;
            root.canceled();
        }
    }

    // ===== 键盘事件 ======
    // 注意：这个对话框没有 Escape 键处理，让 Main.qml 的 Shortcut 处理
    function getButtonByIndex(index) {
        var btnIndex = 0;
        for (var i = 0; i < buttonSpecs.length; i++) {
            if (!buttonSpecs[i].isSeparator) {
                if (btnIndex === index) {
                    return buttonColLayout.children[i].actualButton;
                }
                btnIndex++;
            }
        }
        return null;
    }

    function resetExitFill() {
        exitFillAnimation.stop();
        exitFillCommitTimer.stop();
        exitAnimationRunning = false;
        exitFillOverlay.opacity = 0;
        exitFillOverlay.width = 0;
        exitFillOverlay.height = 0;
        exitFillOverlay.x = root.dialogBox ? root.dialogBox.width / 2 : root.width / 2;
        exitFillOverlay.y = root.dialogBox ? root.dialogBox.height / 2 : root.height / 2;
    }

    function exitFillDiameterFromOrigin(originX, originY) {
        var w = root.dialogBox ? root.dialogBox.width : root.width;
        var h = root.dialogBox ? root.dialogBox.height : root.height;
        var distances = [Math.sqrt(originX * originX + originY * originY), Math.sqrt((w - originX) * (w - originX) + originY * originY), Math.sqrt(originX * originX + (h - originY) * (h - originY)), Math.sqrt((w - originX) * (w - originX) + (h - originY) * (h - originY))];
        return Math.max.apply(Math, distances) * 2 + AppStyle.spacing.xl;
    }

    function playExitFillAndExit() {
        if (exitAnimationRunning)
            return;
        var exitButton = getButtonByIndex(1);
        var fillParent = root.dialogBox ? root.dialogBox : root;
        var origin = exitButton ? exitButton.mapToItem(fillParent, exitButton.width / 2, exitButton.height / 2) : Qt.point(fillParent.width / 2, fillParent.height / 2);
        exitFillTargetSize = exitFillDiameterFromOrigin(origin.x, origin.y);
        exitFillOriginX = origin.x;
        exitFillOriginY = origin.y;
        exitFillOverlay.x = origin.x;
        exitFillOverlay.y = origin.y;
        exitFillOverlay.width = 0;
        exitFillOverlay.height = 0;
        exitFillOverlay.opacity = 0;
        exitAnimationRunning = true;
        exitFillAnimation.restart();
        exitFillCommitTimer.restart();
    }

    function getButtonSpecByIndex(index) {
        var btnIndex = 0;
        for (var i = 0; i < buttonSpecs.length; i++) {
            if (!buttonSpecs[i].isSeparator) {
                if (btnIndex === index) {
                    return buttonSpecs[i];
                }
                btnIndex++;
            }
        }
        return null;
    }

    // 获取按钮在 buttonSpecs 中的实际索引
    function getSpecIndexForButton(buttonIndex) {
        var btnIndex = 0;
        for (var i = 0; i < buttonSpecs.length; i++) {
            if (!buttonSpecs[i].isSeparator) {
                if (btnIndex === buttonIndex) {
                    return i;
                }
                btnIndex++;
            }
        }
        return -1;
    }

    Keys.onPressed: event => {
        if (event.key === Qt.Key_Up || event.key === Qt.Key_Left) {
            if (focusArea === "button") {
                // 向上/左导航
                var currentIndex = 0;
                var targetIndex = 0;
                var btnCount = 0;
                for (var i = 0; i < buttonSpecs.length; i++) {
                    if (!buttonSpecs[i].isSeparator) {
                        if (buttonColLayout.children[i].actualButton === selectedButton) {
                            currentIndex = i;
                        }
                        btnCount++;
                    }
                }
                // 找到上一个按钮
                var prevBtnIndex = -1;
                for (var j = currentIndex - 1; j >= 0; j--) {
                    if (!buttonSpecs[j].isSeparator) {
                        prevBtnIndex = j;
                        break;
                    }
                }
                if (prevBtnIndex >= 0) {
                    selectedButton = buttonColLayout.children[prevBtnIndex].actualButton;
                    updateSlider(selectedButton, buttonSpecs[prevBtnIndex].color);
                }
            }
            event.accepted = true;
        } else if (event.key === Qt.Key_Down || event.key === Qt.Key_Right) {
            if (focusArea === "button") {
                // 向下/右导航
                var currentIdx = 0;
                var nextIdx = -1;
                for (var k = 0; k < buttonSpecs.length; k++) {
                    if (!buttonSpecs[k].isSeparator) {
                        if (buttonColLayout.children[k].actualButton === selectedButton) {
                            currentIdx = k;
                        }
                    }
                }
                for (var l = currentIdx + 1; l < buttonSpecs.length; l++) {
                    if (!buttonSpecs[l].isSeparator) {
                        nextIdx = l;
                        break;
                    }
                }
                if (nextIdx >= 0) {
                    selectedButton = buttonColLayout.children[nextIdx].actualButton;
                    updateSlider(selectedButton, buttonSpecs[nextIdx].color);
                }
            }
            event.accepted = true;
        } else if (event.key === Qt.Key_Tab) {
            // Tab 键：在按钮和复选框之间切换
            if (focusArea === "button") {
                focusArea = "checkbox";
                hideSlider();
            } else {
                focusArea = "button";
                if (selectedButton) {
                    for (var m = 0; m < buttonSpecs.length; m++) {
                        if (buttonColLayout.children[m].actualButton === selectedButton) {
                            updateSlider(selectedButton, buttonSpecs[m].color);
                            break;
                        }
                    }
                }
            }
            event.accepted = true;
        } else if (event.key === Qt.Key_Space) {
            if (focusArea === "checkbox") {
                root.rememberChoice = !root.rememberChoice;
                event.accepted = true;
            }
        } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
            if (focusArea === "checkbox") {
                root.rememberChoice = !root.rememberChoice;
                event.accepted = true;
            } else if (selectedButton) {
                // 执行选中按钮的动作
                for (var n = 0; n < buttonSpecs.length; n++) {
                    if (buttonColLayout.children[n].actualButton === selectedButton) {
                        if (buttonSpecs[n].action) {
                            buttonSpecs[n].action();
                        }
                        break;
                    }
                }
            }
            event.accepted = true;
        } else if (event.key === Qt.Key_Escape) {
            // ESC 键不处理，让 Main.qml 的 Shortcut 处理
            event.accepted = false;
        } else {
            event.accepted = true;
        }
    }

    // ===== 覆写 open 函数 =====
    function open() {
        cancelPending = false;
        visible = true;
        resetExitFill();
        root.dialogBox.rotation = 0;
        root.dialogBox.scale = 1.0;
        root.dialogBox.opacity = 1.0;
        root.dialogBoxTranslate.y = 0;
        // 默认选中第一个按钮（最小化到托盘）
        // 遍历 buttonSpecs，使用单独计数器追踪 children 索引
        var childIndex = 0;
        for (var i = 0; i < buttonSpecs.length; i++) {
            if (buttonSpecs[i].isSeparator) {
                continue;
            }
            // 找到第一个非分隔线的按钮
            var loader = buttonColLayout.children[childIndex];
            if (loader && loader.actualButton) {
                selectedButton = loader.actualButton;
                updateSlider(selectedButton, buttonSpecs[i].color);
                break;
            }
            childIndex++;
        }
        focusArea = "button";
        root.forceActiveFocus();
    }
}
