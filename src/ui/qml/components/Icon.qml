import QtQuick
import EasyKiconverter_Cpp_Version.src.ui.qml.styles 1.0

Item {
    id: root
    property string iconName: ""
    property string iconNameDark: ""
    property int size: 24
    // 图标的显示颜色，用于 SVG 加载失败时的文本回退。
    property color iconColor: AppStyle.iconColors.primary
    width: size
    height: size
    implicitWidth: size
    implicitHeight: size
    // 资源目录的基础 URL。
    readonly property string iconBasePath: Qt.resolvedUrl("../../../../resources/icons/")
    // 根据当前主题选择图标资源路径。
    function getIconSource() {
        if (iconName.length === 0)
            return "";
        if (AppStyle.isDarkMode && iconNameDark.length > 0) {
            return iconBasePath + iconNameDark + ".svg";
        }
        return iconBasePath + iconName + ".svg";
    }

    Image {
        id: iconImage
        anchors.fill: parent
        fillMode: Image.PreserveAspectFit
        source: getIconSource()
        cache: true
        smooth: true
        antialiasing: true
        visible: status === Image.Ready && iconName.length > 0
        onStatusChanged: {
            if (status === Image.Error && iconName.length > 0) {
                iconImage.source = iconBasePath + iconName + ".svg";
            }
        }
    }

    Text {
        anchors.centerIn: parent
        text: getIconSymbol(iconName)
        font.pixelSize: size * 0.6
        color: iconColor
        visible: !iconImage.visible
        font.bold: true
    }

    // 将图标名称映射为 SVG 加载失败时使用的文本符号。
    function getIconSymbol(name) {
        // 根据图标名称选择可读的文本回退符号。
        switch (name) {
        case "play":
            return "▶";
        case "folder":
            return "📁";
        case "trash":
            return "🗑";
        case "upload":
            return "↑";
        case "add":
            return "+";
        case "loading":
            return "⟳";
        default:
            return "?";
        }
    }
}
