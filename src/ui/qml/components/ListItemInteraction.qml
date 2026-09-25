import QtQuick
import EasyKiconverter_Cpp_Version.src.ui.qml.styles 1.0

/**
 * @brief 列表项共享的复制和外链交互层。
 * @details 统一处理右键复制、Ctrl 加左键打开商城页面以及复制成功提示，
 *          不参与宿主列表项的布局、状态或动画绑定。
 */
Item {
    id: interactionLayer
    /** @brief 当前列表项对应的元件编号。 */
    property string itemId: ""
    /** @brief 指示鼠标是否悬停在共享交互层上。 */
    readonly property bool containsMouse: itemMouseArea.containsMouse
    /** @brief 用户完成复制操作后发出的信号。 */
    signal copyClicked
    MouseArea {
        id: itemMouseArea
        objectName: "listItemInteraction"
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.ArrowCursor
        acceptedButtons: Qt.RightButton | Qt.LeftButton
        onClicked: mouse => {
            if (mouse.button === Qt.RightButton) {
                if (interactionLayer.itemId) {
                    interactionLayer.copyClicked();
                }
            } else if (mouse.button === Qt.LeftButton && (mouse.modifiers & Qt.ControlModifier)) {
                if (interactionLayer.itemId) {
                    Qt.openUrlExternally("https://so.szlcsc.com/global.html?k=" + interactionLayer.itemId);
                }
            }
        }
    }
}
