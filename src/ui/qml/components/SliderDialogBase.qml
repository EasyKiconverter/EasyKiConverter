import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import "../styles"

/**
 * @brief 滑块UI对话框基类
 *
 * 提供共用的滑块动画、按钮容器和动画效果。
 * 子类通过 buttonSpecs 属性定义按钮，通过 extraContent 添加额外内容。
 *
 * 按钮规格格式:
 * - { text: "按钮文本", color: 颜色, action: function() }
 * - { isSeparator: true }  // 分隔线
 *
 * 共用组件:
 * - sliderBackground: 果冻滑块效果
 * - buttonBox: 按钮容器
 * - hideAnim: 关闭动画
 * - showAnim: 打开动画（hasOverlay 控制）
 */
FocusScope {
    id: root
    /** 关闭动画完成且组件已隐藏后发出。 */
    signal closeAnimationFinished
    anchors.fill: parent
    visible: false
    z: 9999
    focus: visible
    // ===== 子类可覆盖的属性 =====
    property bool hasOverlay: false
    /** @property Component mainContentSource - 子类可通过此属性在按钮上方添加主要内容组件（如输入框） */
    property Component mainContentSource: null
    /** @property Component extraContentSource - 子类可通过此属性添加额外内容组件（如复选框） */
    property Component extraContentSource: null
    // ===== 共用常量 =====
    readonly property int buttonHeight: 48
    readonly property real sliderOpacity: 0.25
    readonly property int hoverScaleDelay: AppStyle.interactions.sliderIntentDelay
    readonly property int sliderVelocitySettleDelay: 70
    property bool sliderVisible: false
    property real sliderTargetY: AppStyle.spacing.sm
    property real sliderPreviousY: AppStyle.spacing.sm
    property real sliderVelocity: 0
    readonly property real sliderStretch: Math.min(buttonHeight * 0.5, Math.abs(sliderBackground.y - sliderTargetY) * 0.15 + sliderVelocity * 0.06)
    readonly property real sliderMotionAmount: Math.min(1, sliderStretch / (buttonHeight * 0.5))
    readonly property real sliderMovingOpacity: Math.max(0.2, sliderOpacity - sliderMotionAmount * 0.05)
    // ===== 子类可使用的属性 =====
    property Item activeButton: null  // 当前悬停的按钮
    property Item selectedButton: null  // 当前键盘选中的按钮（用于 ExitDialog 键盘导航）
    property string title: qsTr("对话框")  // 对话框标题
    property string message: qsTr("提示信息")  // 对话框消息
    // ===== 内部元素引用（供子类使用）=====
    property alias dialogBox: dialogBox  // 对话框容器
    property alias dialogBoxTranslate: dialogBoxTranslate  // 对话框位移
    property alias buttonColLayout: buttonColLayout  // 按钮列布局
    property alias showAnimation: showAnim  // 打开动画
    // ===== 按钮规格列表 =====
    // 格式: { text, color, action } 或 { isSeparator: true }
    property list<var> buttonSpecs
    // ===== 公共函数 =====
    function mixColor(fromColor, toColor, amount) {
        var t = Math.max(0, Math.min(1, amount));
        return Qt.rgba(fromColor.r + (toColor.r - fromColor.r) * t, fromColor.g + (toColor.g - fromColor.g) * t, fromColor.b + (toColor.b - fromColor.b) * t, fromColor.a + (toColor.a - fromColor.a) * t);
    }

    function sliderCoverageForButton(targetButton) {
        if (!targetButton || !buttonBox || sliderBackground.opacity <= 0)
            return 0;
        var buttonY = targetButton.mapToItem(buttonBox, 0, 0).y;
        var sliderTop = sliderBackground.y;
        var sliderBottom = sliderBackground.y + sliderBackground.height;
        var buttonBottom = buttonY + buttonHeight;
        var overlap = Math.max(0, Math.min(sliderBottom, buttonBottom) - Math.max(sliderTop, buttonY));
        return Math.max(0, Math.min(1, overlap / buttonHeight));
    }

    function updateSlider(targetButton, targetColor) {
        if (!buttonBox)
            return;
        var buttonY = targetButton.mapToItem(buttonBox, 0, 0).y;
        var travel = Math.abs(buttonY - sliderBackground.y);
        slotHighlight.y = buttonY;
        slotHighlight.color = targetColor;
        slotHighlight.opacity = 0.18;
        sliderTrail.y = Math.min(buttonY, sliderBackground.y);
        sliderTrail.height = Math.max(buttonHeight, travel + buttonHeight);
        sliderTrail.color = targetColor;
        sliderTrail.opacity = travel > 1 ? 0.06 : 0.025;
        sliderGhost.y = sliderBackground.y;
        sliderGhost.color = targetColor;
        sliderGhost.opacity = travel > 1 ? 0.035 : 0;
        sliderBackground.scale = 0.98;
        sliderTargetY = buttonY;
        sliderBackground.y = buttonY;
        sliderBackground.color = targetColor;
        sliderVisible = true;
        sliderCompressionTimer.restart();
        activeButton = targetButton;
    }

    function hideSlider() {
        sliderVisible = false;
        sliderTrail.opacity = 0;
        sliderGhost.opacity = 0;
        slotHighlight.opacity = 0;
        sliderCompressionTimer.stop();
        sliderVelocitySettleTimer.stop();
        sliderBackground.scale = 1.0;
        sliderVelocity = 0;
        activeButton = null;
    }

    function open() {
        hideAnim.stop();
        showAnim.stop();
        visible = true;
        overlay.opacity = 0;
        dialogBox.opacity = hasOverlay ? 0 : 1.0;
        dialogBox.scale = hasOverlay ? 0.9 : 1.0;
        dialogBoxTranslate.y = 0;
        hideSlider();
        if (hasOverlay) {
            showAnimation.start();
        } else {
            dialogBox.rotation = 0;
            dialogBox.scale = 1.0;
            dialogBox.opacity = 1.0;
            dialogBoxTranslate.y = 0;
        }
        root.forceActiveFocus();
    }

    function close() {
        hideAnim.stop();
        showAnim.stop();
        visible = false;
    }

    function closeWithAnimation() {
        hideAnim.start();
    }

    Timer {
        id: sliderCompressionTimer
        interval: 45
        repeat: false
        onTriggered: {
            sliderBackground.scale = 1.0;
        }
    }

    Timer {
        id: sliderVelocitySettleTimer
        interval: root.sliderVelocitySettleDelay
        repeat: false
        onTriggered: {
            root.sliderVelocity = 0;
            sliderTrail.opacity = 0;
            sliderGhost.opacity = 0;
        }
    }

    // ===== 遮罩层 =====
    Rectangle {
        id: overlay
        anchors.fill: parent
        color: AppStyle.isDarkMode ? "#000000" : "#1e293b"
        opacity: hasOverlay ? 0 : 0
        visible: hasOverlay
        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            onClicked: {}
        }
    }

    // ===== 对话框主体 =====
    Rectangle {
        id: dialogBox
        width: Math.min(parent.width * (hasOverlay ? 0.8 : 0.9), hasOverlay ? 400 : 440)
        implicitHeight: contentCol.implicitHeight + AppStyle.spacing.xxxl * 2
        anchors.centerIn: parent
        color: AppStyle.isDarkMode ? "#1e293b" : "#ffffff"
        radius: AppStyle.radius.xl
        scale: hasOverlay ? 0.9 : 1.0
        opacity: hasOverlay ? 0 : 1.0
        transform: Translate {
            id: dialogBoxTranslate
            y: 0
        }

        // 边框
        Rectangle {
            anchors.fill: parent
            radius: AppStyle.radius.xl
            color: "transparent"
            border.color: AppStyle.isDarkMode ? Qt.rgba(255, 255, 255, 0.1) : Qt.rgba(0, 0, 0, 0.1)
            border.width: AppStyle.borderWidths.thin
            z: 1
        }

        // 阴影
        Rectangle {
            anchors.fill: parent
            radius: AppStyle.radius.xl
            color: "transparent"
            z: -1
            layer.enabled: true
            layer.effect: MultiEffect {
                shadowEnabled: true
                shadowColor: AppStyle.shadows.lg.color
                shadowBlur: 1.5
                shadowHorizontalOffset: AppStyle.shadows.lg.horizontalOffset
                shadowVerticalOffset: AppStyle.shadows.lg.verticalOffset
            }
        }

        ColumnLayout {
            id: contentCol
            anchors.fill: parent
            anchors.margins: AppStyle.spacing.xxxl
            spacing: AppStyle.spacing.lg
            // 标题（子类可覆盖 ID）
            Text {
                id: titleText
                Layout.fillWidth: true
                font.pixelSize: AppStyle.fontSizes.xl
                font.bold: true
                color: AppStyle.colors.textPrimary
                horizontalAlignment: Text.AlignHCenter
                text: root.title
            }

            // 内容（子类可覆盖 ID）
            Text {
                id: messageText
                Layout.fillWidth: true
                font.pixelSize: AppStyle.fontSizes.md
                color: AppStyle.colors.textSecondary
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
                lineHeight: 1.3
                text: root.message
            }

            // 主内容（由子类通过 mainContentSource 设置，位于按钮组上方）
            Loader {
                id: mainContentLoader
                Layout.fillWidth: true
                Layout.topMargin: AppStyle.spacing.sm
                sourceComponent: root.mainContentSource
                visible: root.mainContentSource
            }

            // 按钮组容器
            Rectangle {
                id: buttonBox
                Layout.fillWidth: true
                Layout.topMargin: AppStyle.spacing.md
                implicitHeight: buttonColLayout.implicitHeight + AppStyle.spacing.md * 2
                color: AppStyle.isDarkMode ? Qt.rgba(255, 255, 255, 0.05) : Qt.rgba(0, 0, 0, 0.03)
                radius: AppStyle.radius.lg
                border.color: AppStyle.isDarkMode ? Qt.rgba(255, 255, 255, 0.1) : Qt.rgba(0, 0, 0, 0.08)
                border.width: AppStyle.borderWidths.thin
                Rectangle {
                    id: slotHighlight
                    x: AppStyle.spacing.sm
                    y: AppStyle.spacing.sm
                    width: buttonBox.width - AppStyle.spacing.sm * 2
                    height: buttonHeight
                    radius: AppStyle.radius.md
                    color: AppStyle.colors.primary
                    opacity: 0
                    z: 0
                    Behavior on y {
                        SpringAnimation {
                            spring: 4.5
                            damping: 0.45
                            mass: 1.0
                            epsilon: 0.25
                        }
                    }
                    Behavior on color {
                        ColorAnimation {
                            duration: AppStyle.durations.fast
                            easing.type: AppStyle.easings.easeOut
                        }
                    }
                    Behavior on opacity {
                        NumberAnimation {
                            duration: AppStyle.durations.fast
                            easing.type: AppStyle.easings.easeOut
                        }
                    }
                }
                Rectangle {
                    id: sliderTrail
                    x: AppStyle.spacing.sm
                    y: sliderBackground.y
                    width: buttonBox.width - AppStyle.spacing.sm * 2
                    height: buttonHeight
                    radius: AppStyle.radius.md
                    color: AppStyle.colors.primary
                    opacity: 0
                    z: 1
                    layer.enabled: opacity > 0
                    layer.effect: MultiEffect {
                        shadowEnabled: true
                        shadowColor: sliderTrail.color
                        shadowBlur: Math.max(0.25, 0.6 - root.sliderMotionAmount * 0.35)
                        shadowVerticalOffset: 0
                        shadowHorizontalOffset: 0
                    }
                    Behavior on opacity {
                        NumberAnimation {
                            duration: 220
                            easing.type: AppStyle.easings.easeOut
                        }
                    }
                }
                Rectangle {
                    id: sliderGhost
                    x: AppStyle.spacing.sm
                    y: sliderBackground.y
                    width: buttonBox.width - AppStyle.spacing.sm * 2
                    height: sliderBackground.height
                    radius: AppStyle.radius.md
                    color: AppStyle.colors.primary
                    opacity: 0
                    z: 1.5
                    Behavior on y {
                        NumberAnimation {
                            duration: 55
                            easing.type: Easing.OutCubic
                        }
                    }
                    Behavior on height {
                        NumberAnimation {
                            duration: 55
                            easing.type: Easing.OutCubic
                        }
                    }
                    Behavior on opacity {
                        NumberAnimation {
                            duration: 180
                            easing.type: AppStyle.easings.easeOut
                        }
                    }
                }
                // 滑块背景
                Rectangle {
                    id: sliderBackground
                    x: AppStyle.spacing.sm
                    y: AppStyle.spacing.sm
                    width: buttonBox.width - AppStyle.spacing.sm * 2
                    height: buttonHeight + root.sliderStretch
                    radius: AppStyle.radius.md
                    color: AppStyle.colors.primary
                    opacity: root.sliderVisible ? root.sliderMovingOpacity : 0
                    z: 2
                    onYChanged: {
                        root.sliderVelocity = Math.min(root.buttonHeight * 2.2, Math.abs(y - root.sliderPreviousY));
                        root.sliderPreviousY = y;
                        sliderVelocitySettleTimer.restart();
                    }
                    Rectangle {
                        anchors.fill: parent
                        radius: parent.radius
                        color: "#ffffff"
                        opacity: 0
                        SequentialAnimation on opacity {
                            running: root.visible && root.activeButton && root.selectedButton && root.activeButton === root.selectedButton
                            loops: Animation.Infinite
                            NumberAnimation {
                                from: 0.04
                                to: 0.16
                                duration: 900
                                easing.type: Easing.InOutSine
                            }
                            NumberAnimation {
                                from: 0.16
                                to: 0.04
                                duration: 900
                                easing.type: Easing.InOutSine
                            }
                        }
                    }
                    Behavior on y {
                        enabled: sliderBackground.visible
                        SpringAnimation {
                            spring: 12.0
                            damping: 0.95
                            mass: 1.0
                            epsilon: 0.1
                        }
                    }

                    Behavior on scale {
                        enabled: sliderBackground.visible
                        NumberAnimation {
                            duration: 130
                            easing.type: Easing.OutBack
                        }
                    }

                    Behavior on color {
                        ColorAnimation {
                            duration: AppStyle.durations.fast
                            easing.type: Easing.OutCubic
                        }
                    }

                    Behavior on opacity {
                        NumberAnimation {
                            duration: 100
                            easing.type: Easing.OutCubic
                        }
                    }

                    layer.enabled: sliderBackground.opacity > 0
                    layer.effect: MultiEffect {
                        shadowEnabled: true
                        shadowColor: sliderBackground.color
                        shadowBlur: Math.max(0.45, 0.9 - root.sliderMotionAmount * 0.4)
                        shadowVerticalOffset: 0
                        shadowHorizontalOffset: 0
                    }
                }

                // 按钮列
                ColumnLayout {
                    id: buttonColLayout
                    anchors.fill: parent
                    anchors.topMargin: AppStyle.spacing.sm
                    anchors.bottomMargin: AppStyle.spacing.sm
                    spacing: 0
                    z: 3
                    // 动态生成按钮
                    Repeater {
                        model: root.buttonSpecs
                        delegate: buttonDelegate
                    }
                }
            }

            // 额外内容（由子类通过 extraContentSource 设置）
            Loader {
                id: extraContentLoader
                Layout.fillWidth: true
                Layout.topMargin: AppStyle.spacing.md
                sourceComponent: root.extraContentSource
                visible: root.extraContentSource
            }
        }
    }

    // 按钮/分隔线代理
    Component {
        id: buttonDelegate
        Loader {
            Layout.fillWidth: true
            Layout.preferredHeight: modelData.visible === false ? 0 : (modelData.isSeparator ? 1 : buttonHeight)
            visible: modelData.visible !== false
            property var specData: modelData  // 传递数据
            sourceComponent: modelData.isSeparator ? separatorComponent : buttonComponent
            // 暴露内部按钮引用，以便外部（ExitDialog键盘导航）可以获取实际按钮
            property Item actualButton: item
            onLoaded: {
                if (item && specData.objectName)
                    item.objectName = specData.objectName;
            }
        }
    }

    // 分隔线组件
    Component {
        id: separatorComponent
        Rectangle {
            color: AppStyle.isDarkMode ? Qt.rgba(255, 255, 255, 0.08) : Qt.rgba(0, 0, 0, 0.06)
        }
    }

    // 按钮组件
    Component {
        id: buttonComponent
        ModernButton {
            id: btn
            // 标记自身以便父组件识别
            property bool isSliderButton: true
            // 关联的颜色 - parent是Loader，Loader.parent是ColumnLayout
            property color sliderColor: parent.specData ? parent.specData.color : AppStyle.colors.primary
            property string btnText: parent.specData ? parent.specData.text : ""
            text: btnText
            backgroundColor: "transparent"
            textColor: {
                var coverage = root.sliderCoverageForButton(btn);
                if (coverage > 0) {
                    return root.mixColor(AppStyle.colors.textPrimary, Qt.lighter(sliderColor, 1.35), coverage);
                }
                // 悬停状态优先
                if (root.activeButton === btn) {
                    return Qt.lighter(sliderColor, 1.3);
                }
                // 键盘选中状态
                if (root.selectedButton === btn) {
                    return sliderColor;
                }
                return AppStyle.colors.textPrimary;
            }
            hoverColor: "transparent"
            Behavior on textColor {
                ColorAnimation {
                    duration: 150
                    easing.type: Easing.OutCubic
                }
            }

            visualScale: pressed ? 0.95 : ((root.activeButton === btn) || (root.selectedButton === btn)) ? 1.05 : 1.0
            Timer {
                id: sliderHoverDelayTimer
                interval: root.hoverScaleDelay
                repeat: false
                onTriggered: {
                    if (btn.hovered) {
                        root.activeButton = btn;
                        root.updateSlider(btn, sliderColor);
                    }
                }
            }

            onHoveredChanged: {
                if (hovered) {
                    sliderHoverDelayTimer.restart();
                } else {
                    sliderHoverDelayTimer.stop();
                    if (root.activeButton === btn)
                        root.hideSlider();
                }
            }

            onClicked: {
                if (parent.specData && parent.specData.action) {
                    parent.specData.action();
                }
            }
        }
    }

    // ===== 打开动画 =====
    ParallelAnimation {
        id: showAnim
        running: hasOverlay
        NumberAnimation {
            target: overlay
            property: "opacity"
            from: 0
            to: 0.6
            duration: 250
            easing.type: AppStyle.easings.easeOut
        }
        NumberAnimation {
            target: dialogBox
            property: "opacity"
            from: 0
            to: 1
            duration: 250
            easing.type: AppStyle.easings.easeOut
        }
        NumberAnimation {
            target: dialogBox
            property: "scale"
            from: 0.8
            to: 1
            duration: 300
            easing.type: Easing.OutBack
        }
    }

    // ===== 关闭动画 =====
    SequentialAnimation {
        id: hideAnim
        ParallelAnimation {
            NumberAnimation {
                target: overlay
                property: "opacity"
                from: hasOverlay ? 0.6 : 0
                to: 0
                duration: 200
                easing.type: Easing.OutQuad
            }
            NumberAnimation {
                target: dialogBox
                property: "opacity"
                from: 1
                to: 0
                duration: 200
                easing.type: Easing.OutQuad
            }
            NumberAnimation {
                target: dialogBoxTranslate
                property: "y"
                from: 0
                to: root.height
                duration: 200
                easing.type: Easing.OutQuad
            }
            NumberAnimation {
                target: dialogBox
                property: "scale"
                from: 1
                to: 0.9
                duration: 200
                easing.type: Easing.OutQuad
            }
        }
        PropertyAction {
            target: root
            property: "visible"
            value: false
        }
        ScriptAction {
            script: root.closeAnimationFinished()
        }
    }
}
