import QtQuick
import QtQuick.Controls
import QtQuick.Effects

import EasyKiconverter_Cpp_Version.src.ui.qml.styles 1.0

/**
 * @brief 元件预览图弹窗。
 *
 * 负责预览图展示、缩略图切换以及鼠标悬停期间的延迟隐藏，弹窗定位由宿主卡片负责。
 */
Popup {
    id: previewPopup
    property var imageSources: []
    property var currentItemData: null
    property int mainIndex: 0
    property real popupScale: 1
    property real scaleOriginX: 0
    property real scaleOriginY: 0
    property real anchorX: 0
    property real anchorY: 0
    property real anchorWidth: 0
    property real anchorHeight: 0
    property real cursorX: NaN
    property real cursorY: NaN
    readonly property bool hasImages: imageSources.length > 0
    readonly property int panelPadding: AppStyle.spacing.md
    readonly property int headerHeight: 34
    readonly property int mainImageSize: Math.min(Math.max(180, parent ? parent.width * 0.22 : 220), 260)
    readonly property int thumbSize: 42
    readonly property int thumbGap: AppStyle.spacing.xs
    readonly property int stripHeight: imageSources.length > 1 ? thumbSize : 0
    readonly property int galleryWidth: Math.max(mainImageSize, imageSources.length * thumbSize + Math.max(0, imageSources.length - 1) * thumbGap) + panelPadding * 2
    readonly property int galleryHeight: hasImages ? headerHeight + mainImageSize + stripHeight + panelPadding * 2 + (stripHeight > 0 ? AppStyle.spacing.sm : 0) : 184
    width: galleryWidth
    height: galleryHeight
    padding: 0
    visible: false
    closePolicy: Popup.NoAutoClose
    modal: false
    focus: false
    dim: false
    opacity: 1
    /** @brief 取消已经安排的延迟隐藏。 */
    function cancelHide() {
        popupHideTimer.stop();
    }

    /** @brief 安排鼠标离开后的延迟隐藏。 */
    function scheduleHide() {
        popupHideTimer.restart();
    }

    Timer {
        id: popupHideTimer
        interval: AppStyle.interactions.popupHideDelay
        repeat: false
        onTriggered: {
            if (popupHoverHandler.hovered) {
                return;
            }
            previewPopup.close();
        }
    }

    onVisibleChanged: {
        if (visible) {
            mainIndex = 0;
        }
    }

    enter: Transition {
        ParallelAnimation {
            NumberAnimation {
                property: "opacity"
                from: 0
                to: 1
                duration: AppStyle.durations.fast
                easing.type: AppStyle.easings.easeOut
            }
            NumberAnimation {
                property: "popupScale"
                from: 0.38
                to: 1
                duration: AppStyle.durations.normal
                easing.type: AppStyle.easings.easeOut
            }
        }
    }

    exit: Transition {
        ParallelAnimation {
            NumberAnimation {
                property: "opacity"
                from: 1
                to: 0
                duration: AppStyle.durations.fast
                easing.type: AppStyle.easings.easeIn
            }
            NumberAnimation {
                property: "popupScale"
                from: 1
                to: 0.92
                duration: AppStyle.durations.fast
                easing.type: AppStyle.easings.easeIn
            }
        }
    }

    background: Rectangle {
        color: "transparent"
    }

    contentItem: Item {
        implicitWidth: previewPopup.width
        implicitHeight: previewPopup.height
        visible: previewPopup.currentItemData ? previewPopup.currentItemData.isValid : false
        transform: Scale {
            origin.x: previewPopup.scaleOriginX
            origin.y: previewPopup.scaleOriginY
            xScale: previewPopup.popupScale
            yScale: previewPopup.popupScale
        }

        Rectangle {
            anchors.fill: parent
            color: AppStyle.isDarkMode ? Qt.rgba(15 / 255, 23 / 255, 42 / 255, 0.9) : Qt.rgba(1, 1, 1, 0.92)
            border.width: AppStyle.borderWidths.thin
            border.color: AppStyle.isDarkMode ? Qt.rgba(1, 1, 1, 0.12) : Qt.rgba(15 / 255, 23 / 255, 42 / 255, 0.08)
            radius: AppStyle.radius.lg
            layer.enabled: true
            layer.effect: MultiEffect {
                shadowEnabled: true
                shadowBlur: 0.82
                shadowColor: AppStyle.isDarkMode ? "#cc000000" : "#2a0f172a"
                shadowVerticalOffset: 10
                shadowHorizontalOffset: 0
            }
        }

        HoverHandler {
            id: popupHoverHandler
            onHoveredChanged: {
                if (hovered) {
                    popupHideTimer.stop();
                } else {
                    popupHideTimer.restart();
                }
            }
        }

        Column {
            id: galleryColumn
            anchors.fill: parent
            anchors.margins: previewPopup.panelPadding
            spacing: previewPopup.stripHeight > 0 ? AppStyle.spacing.sm : 0
            visible: previewPopup.hasImages
            Rectangle {
                width: parent.width
                height: previewPopup.headerHeight
                color: "transparent"
                Text {
                    anchors.left: parent.left
                    anchors.right: imageCounter.left
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.rightMargin: AppStyle.spacing.md
                    text: previewPopup.currentItemData ? previewPopup.currentItemData.componentId : ""
                    color: AppStyle.colors.textPrimary
                    font.pixelSize: AppStyle.fontSizes.sm
                    font.family: "Courier New"
                    font.bold: true
                    elide: Text.ElideRight
                }
                Text {
                    id: imageCounter
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    text: previewPopup.imageSources.length > 1 ? (previewPopup.mainIndex + 1) + "/" + previewPopup.imageSources.length : ""
                    color: AppStyle.colors.textSecondary
                    font.pixelSize: AppStyle.fontSizes.xs
                }
            }

            Rectangle {
                id: mainImageSurface
                width: parent.width
                height: previewPopup.mainImageSize
                color: AppStyle.isDarkMode ? Qt.rgba(1, 1, 1, 0.035) : Qt.rgba(15 / 255, 23 / 255, 42 / 255, 0.025)
                radius: AppStyle.radius.md
                clip: true
                border.width: AppStyle.borderWidths.thin
                border.color: AppStyle.isDarkMode ? Qt.rgba(1, 1, 1, 0.08) : Qt.rgba(15 / 255, 23 / 255, 42 / 255, 0.06)
                Canvas {
                    anchors.fill: parent
                    opacity: AppStyle.isDarkMode ? 0.28 : 0.42
                    onPaint: {
                        var ctx = getContext("2d");
                        ctx.clearRect(0, 0, width, height);
                        ctx.strokeStyle = AppStyle.isDarkMode ? "rgba(148, 163, 184, 0.24)" : "rgba(100, 116, 139, 0.24)";
                        ctx.lineWidth = 1;
                        var step = 16;
                        for (var xPos = 0; xPos <= width; xPos += step) {
                            ctx.beginPath();
                            ctx.moveTo(xPos + 0.5, 0);
                            ctx.lineTo(xPos + 0.5, height);
                            ctx.stroke();
                        }
                        for (var yPos = 0; yPos <= height; yPos += step) {
                            ctx.beginPath();
                            ctx.moveTo(0, yPos + 0.5);
                            ctx.lineTo(width, yPos + 0.5);
                            ctx.stroke();
                        }
                    }
                }
                Image {
                    anchors.fill: parent
                    anchors.margins: AppStyle.spacing.sm
                    sourceSize: Qt.size(previewPopup.mainImageSize * 1.5, previewPopup.mainImageSize * 1.5)
                    source: previewPopup.imageSources.length > 0 ? previewPopup.imageSources[Math.min(previewPopup.mainIndex, previewPopup.imageSources.length - 1)] : ""
                    fillMode: Image.PreserveAspectFit
                    cache: true
                    asynchronous: true
                    smooth: true
                    mipmap: true
                }
            }

            Row {
                id: thumbnailStrip
                anchors.horizontalCenter: parent.horizontalCenter
                height: previewPopup.stripHeight
                spacing: previewPopup.thumbGap
                visible: previewPopup.imageSources.length > 1
                Repeater {
                    model: previewPopup.imageSources.length
                    Rectangle {
                        width: previewPopup.thumbSize
                        height: previewPopup.thumbSize
                        color: index === previewPopup.mainIndex ? (AppStyle.isDarkMode ? Qt.rgba(59 / 255, 130 / 255, 246 / 255, 0.18) : Qt.rgba(59 / 255, 130 / 255, 246 / 255, 0.1)) : "transparent"
                        radius: AppStyle.radius.xs
                        border.width: AppStyle.borderWidths.thin
                        border.color: index === previewPopup.mainIndex ? AppStyle.colors.primary : (AppStyle.isDarkMode ? Qt.rgba(1, 1, 1, 0.12) : Qt.rgba(15 / 255, 23 / 255, 42 / 255, 0.08))
                        clip: true
                        Image {
                            anchors.fill: parent
                            anchors.margins: 3
                            sourceSize: Qt.size(previewPopup.thumbSize, previewPopup.thumbSize)
                            source: previewPopup.imageSources[index]
                            fillMode: Image.PreserveAspectFit
                            cache: true
                            asynchronous: true
                            smooth: true
                        }
                        MouseArea {
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onEntered: previewPopup.mainIndex = index
                            onClicked: previewPopup.mainIndex = index
                        }
                    }
                }
            }
        }

        Item {
            anchors.fill: parent
            visible: !previewPopup.currentItemData || previewPopup.imageSources.length === 0
            Item {
                id: emptyChipShape
                anchors.centerIn: parent
                anchors.verticalCenterOffset: -AppStyle.spacing.lg
                width: 74
                height: 54
                Rectangle {
                    anchors.centerIn: parent
                    width: 38
                    height: 30
                    radius: AppStyle.radius.xs
                    color: "transparent"
                    border.width: AppStyle.borderWidths.thin
                    border.color: AppStyle.colors.textDisabled
                }
                Repeater {
                    model: 4
                    Rectangle {
                        x: 8
                        y: 15 + index * 7
                        width: 10
                        height: 1
                        color: AppStyle.colors.textDisabled
                    }
                }
                Repeater {
                    model: 4
                    Rectangle {
                        x: 56
                        y: 15 + index * 7
                        width: 10
                        height: 1
                        color: AppStyle.colors.textDisabled
                    }
                }
            }
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: emptyChipShape.bottom
                anchors.topMargin: AppStyle.spacing.sm
                text: qsTr("无预览图")
                font.pixelSize: AppStyle.fontSizes.sm
                color: AppStyle.colors.textSecondary
            }
        }
    }
}
