import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Shapes
import EasyKiconverter_Cpp_Version.src.ui.qml.styles 1.0
import EasyKiconverter_Cpp_Version 1.0

ColumnLayout {
    id: headerSection
    // 外部依赖
    property var themeController
    property var componentListController
    spacing: 0
    // 欢迎标题
    Text {
        Layout.fillWidth: true
        Layout.topMargin: 30
        text: "EasyKiConverter"
        font.pixelSize: AppStyle.fontSizes.display
        font.bold: true
        color: AppStyle.colors.textPrimary
        horizontalAlignment: Text.AlignHCenter
    }
    Text {
        Layout.fillWidth: true
        text: qsTranslate("MainWindow", "将嘉立创EDA元器件转换为KiCad格式")
        font.pixelSize: AppStyle.fontSizes.lg
        color: AppStyle.isDarkMode ? AppStyle.colors.textPrimary : AppStyle.colors.textSecondary
        horizontalAlignment: Text.AlignHCenter
    }
    // 深色模式切换按钮和 GitHub 图标
    RowLayout {
        Layout.alignment: Qt.AlignRight
        spacing: AppStyle.spacing.sm
        z: 10  // 确保在其他元素之上
        // 语言选择器
        ComboBox {
            id: languageComboBox
            Layout.preferredWidth: 120
            model: [
                {
                    text: "简体中文",
                    value: "zh_CN"
                },
                {
                    text: "English",
                    value: "en"
                }
            ]
            textRole: "text"
            valueRole: "value"
            // 记录上次有效索引，防止弹窗关闭时 currentIndex 被重置为 -1
            property int lastValidIndex: 0
            Component.onCompleted: {
                currentIndex = langToIndex(LanguageManager.currentLanguage);
                lastValidIndex = currentIndex;
            }
            // currentIndex 变为 -1（弹窗关闭未选择）时恢复上次有效值
            onCurrentIndexChanged: {
                if (currentIndex < 0) {
                    currentIndex = lastValidIndex;
                }
            }
            // 语言变化时同步 currentIndex
            Connections {
                target: LanguageManager
                function onLanguageChanged(language) {
                    var idx = languageComboBox.langToIndex(language);
                    languageComboBox.lastValidIndex = idx;
                    languageComboBox.currentIndex = idx;
                }
            }
            function langToIndex(lang) {
                for (var i = 0; i < model.length; i++) {
                    if (model[i].value === lang)
                        return i;
                }
                return 0;
            }

            onActivated: function (index) {
                if (index >= 0 && index < count) {
                    lastValidIndex = index;
                    var langValue = model[index].value;
                    LanguageManager.setLanguage(langValue);
                }
            }

            background: Rectangle {
                color: AppStyle.colors.surface
                border.color: languageComboBox.hovered ? AppStyle.colors.borderFocus : AppStyle.colors.border
                border.width: AppStyle.borderWidths.thin
                radius: AppStyle.radius.md
                Behavior on border.color {
                    ColorAnimation {
                        duration: 150
                    }
                }
            }

            contentItem: Text {
                text: languageComboBox.displayText
                font.pixelSize: AppStyle.fontSizes.xs
                color: AppStyle.colors.textPrimary
                verticalAlignment: Text.AlignVCenter
                leftPadding: 8
                rightPadding: 8
            }

            indicator: Shape {
                id: indicatorShape
                x: languageComboBox.width - width - languageComboBox.rightPadding
                y: languageComboBox.topPadding + (languageComboBox.availableHeight - height) / 2
                width: 12
                height: 8
                ShapePath {
                    strokeWidth: 0
                    fillColor: AppStyle.colors.textSecondary
                    PathMove {
                        x: 0
                        y: 0
                    }
                    PathLine {
                        x: indicatorShape.width / 2
                        y: indicatorShape.height
                    }
                    PathLine {
                        x: indicatorShape.width
                        y: 0
                    }
                    PathLine {
                        x: 0
                        y: 0
                    }
                }
            }

            delegate: ItemDelegate {
                width: languageComboBox.width
                height: 36
                contentItem: Text {
                    text: modelData.text
                    font.pixelSize: AppStyle.fontSizes.xs
                    color: AppStyle.colors.textPrimary
                    verticalAlignment: Text.AlignVCenter
                    leftPadding: 8
                }

                background: Rectangle {
                    color: parent.highlighted ? (AppStyle.isDarkMode ? "#334155" : "#e2e8f0") : "transparent"
                }
            }

            popup: Popup {
                y: languageComboBox.height - 1
                width: languageComboBox.width
                implicitHeight: listview.contentHeight
                padding: 0
                contentItem: ListView {
                    id: listview
                    clip: true
                    model: languageComboBox.popup.visible ? languageComboBox.delegateModel : null
                    currentIndex: languageComboBox.highlightedIndex
                    ScrollBar.vertical: ScrollBar {
                        policy: ScrollBar.AsNeeded
                    }

                    delegate: ItemDelegate {
                        width: languageComboBox.width
                        height: 36
                        contentItem: Text {
                            text: modelData.text
                            font.pixelSize: AppStyle.fontSizes.xs
                            color: AppStyle.colors.textPrimary
                            verticalAlignment: Text.AlignVCenter
                            leftPadding: 8
                        }

                        background: Rectangle {
                            color: parent.hovered ? (AppStyle.isDarkMode ? "#334155" : "#e2e8f0") : "transparent"
                        }
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

        // GitHub 图标按钮
        MouseArea {
            id: githubButton
            Layout.preferredWidth: 32
            Layout.preferredHeight: 32
            cursorShape: Qt.PointingHandCursor
            hoverEnabled: true
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            z: 100
            property bool hoverScaleActive: false
            property int hoverScaleDelay: AppStyle.interactions.hoverIntentDelay
            onContainsMouseChanged: {
                if (containsMouse) {
                    githubHoverScaleTimer.restart();
                } else {
                    githubHoverScaleTimer.stop();
                    hoverScaleActive = false;
                }
            }
            Timer {
                id: githubHoverScaleTimer
                interval: githubButton.hoverScaleDelay
                repeat: false
                onTriggered: githubButton.hoverScaleActive = githubButton.containsMouse
            }
            Rectangle {
                anchors.fill: parent
                radius: AppStyle.radius.md
                color: githubButton.containsMouse ? (AppStyle.isDarkMode ? "#334155" : "#e2e8f0") : "transparent"
                Behavior on color {
                    ColorAnimation {
                        duration: AppStyle.durations.fast
                        easing.type: AppStyle.easings.easeOut
                    }
                }
            }
            // 发光效果
            Rectangle {
                anchors.centerIn: parent
                width: parent.width + 10
                height: parent.height + 10
                radius: AppStyle.radius.lg
                color: AppStyle.isDarkMode ? "#ffffff" : "#000000"
                opacity: githubButton.hoverScaleActive ? 0.15 : 0.0
                scale: githubButton.hoverScaleActive ? 1.3 : 0.8
                Behavior on opacity {
                    NumberAnimation {
                        duration: AppStyle.durations.fast
                        easing.type: AppStyle.easings.easeOut
                    }
                }
                Behavior on scale {
                    NumberAnimation {
                        duration: AppStyle.durations.normal
                        easing.type: AppStyle.easings.easeOut
                    }
                }
            }
            Image {
                anchors.centerIn: parent
                width: 22
                height: 22
                source: AppStyle.isDarkMode ? "qrc:/qt/qml/EasyKiconverter_Cpp_Version/resources/icons/github-mark-white.svg" : "qrc:/qt/qml/EasyKiconverter_Cpp_Version/resources/icons/github-mark.svg"
                fillMode: Image.PreserveAspectFit
                opacity: githubButton.pressed ? 0.7 : (githubButton.hoverScaleActive ? 1.0 : 0.8)
                scale: githubButton.hoverScaleActive ? 1.2 : 1.0
                rotation: githubButton.hoverScaleActive ? 8 : 0
                Behavior on opacity {
                    NumberAnimation {
                        duration: AppStyle.durations.fast
                        easing.type: AppStyle.easings.easeOut
                    }
                }
                Behavior on scale {
                    NumberAnimation {
                        duration: AppStyle.durations.fast
                        easing.type: AppStyle.easings.easeOut
                    }
                }
                Behavior on rotation {
                    NumberAnimation {
                        duration: AppStyle.durations.normal
                        easing.type: AppStyle.easings.easeOut
                    }
                }
            }
            onClicked: mouse => {
                const projectUrl = "https://github.com/EasyKiconverter/EasyKiConverter";
                if (mouse.button === Qt.LeftButton) {
                    Qt.openUrlExternally(projectUrl);
                } else if (mouse.button === Qt.RightButton) {
                    headerSection.componentListController.copyToClipboard(projectUrl);
                    console.log("Project link copied to clipboard:", projectUrl);
                }
            }
        }
        // 深色模式切换按钮（灯泡图标）
        MouseArea {
            id: themeSwitchButton
            Layout.preferredWidth: 32
            Layout.preferredHeight: 32
            cursorShape: Qt.PointingHandCursor
            hoverEnabled: true
            acceptedButtons: Qt.LeftButton
            z: 100
            property bool hoverScaleActive: false
            property int hoverScaleDelay: AppStyle.interactions.hoverIntentDelay
            onContainsMouseChanged: {
                if (containsMouse) {
                    themeSwitchHoverScaleTimer.restart();
                } else {
                    themeSwitchHoverScaleTimer.stop();
                    hoverScaleActive = false;
                }
            }
            Timer {
                id: themeSwitchHoverScaleTimer
                interval: themeSwitchButton.hoverScaleDelay
                repeat: false
                onTriggered: themeSwitchButton.hoverScaleActive = themeSwitchButton.containsMouse
            }
            Rectangle {
                id: themeSwitchBackground
                anchors.fill: parent
                radius: AppStyle.radius.md
                color: themeSwitchButton.containsMouse ? (AppStyle.isDarkMode ? "#334155" : "#e2e8f0") : "transparent"
                Behavior on color {
                    ColorAnimation {
                        duration: AppStyle.durations.fast
                        easing.type: AppStyle.easings.easeOut
                    }
                }
            }
            // 发光效果
            Rectangle {
                anchors.centerIn: parent
                width: parent.width + 10
                height: parent.height + 10
                radius: AppStyle.radius.lg
                color: AppStyle.isDarkMode ? "#fbbf24" : "#3b82f6"
                opacity: themeSwitchButton.hoverScaleActive ? 0.2 : 0.0
                scale: themeSwitchButton.hoverScaleActive ? 1.3 : 0.8
                Behavior on opacity {
                    NumberAnimation {
                        duration: AppStyle.durations.fast
                        easing.type: AppStyle.easings.easeOut
                    }
                }
                Behavior on scale {
                    NumberAnimation {
                        duration: AppStyle.durations.normal
                        easing.type: AppStyle.easings.easeOut
                    }
                }
            }
            Image {
                id: themeSwitchIcon
                anchors.centerIn: parent
                width: 22
                height: 22
                source: AppStyle.isDarkMode ? "qrc:/qt/qml/EasyKiconverter_Cpp_Version/resources/icons/Grey_light_bulb.svg" : "qrc:/qt/qml/EasyKiconverter_Cpp_Version/resources/icons/Blue_light_bulb.svg"
                fillMode: Image.PreserveAspectFit
                opacity: themeSwitchButton.pressed ? 0.7 : (themeSwitchButton.hoverScaleActive ? 1.0 : 0.85)
                scale: themeSwitchButton.hoverScaleActive ? 1.2 : 1.0
                rotation: themeSwitchButton.hoverScaleActive ? -12 : 0
                Behavior on opacity {
                    NumberAnimation {
                        duration: AppStyle.durations.fast
                        easing.type: AppStyle.easings.easeOut
                    }
                }
                Behavior on scale {
                    NumberAnimation {
                        duration: AppStyle.durations.fast
                        easing.type: AppStyle.easings.easeOut
                    }
                }
                Behavior on rotation {
                    NumberAnimation {
                        duration: AppStyle.durations.normal
                        easing.type: AppStyle.easings.easeOut
                    }
                }
                Behavior on source {
                    PropertyAnimation {
                        duration: 0
                    }
                }
            }
            onClicked: {
                console.log("Theme button clicked!");
                console.log("themeController:", headerSection.themeController);
                console.log("themeController.isDarkMode:", headerSection.themeController ? headerSection.themeController.isDarkMode : "null");
                if (headerSection.themeController) {
                    console.log("Calling setDarkMode...");
                    headerSection.themeController.setDarkMode(!headerSection.themeController.isDarkMode);
                } else {
                    console.error("themeController is null!");
                }
            }
        }
    }
    // 分隔线
    Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: 1
        color: AppStyle.colors.border
    }
}
