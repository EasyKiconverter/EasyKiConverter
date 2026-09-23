import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../styles"

ColumnLayout {
    id: fieldRoot
    property string label: ""
    property string text: ""
    property string placeholder: ""
    property string browseIcon: ""
    signal browseClicked
    signal textEdited(string text)
    signal editingFinished(string text)
    Layout.fillWidth: true
    spacing: 2
    Text {
        text: fieldRoot.label
        font.pixelSize: AppStyle.fontSizes.xs
        color: AppStyle.colors.textSecondary
    }

    RowLayout {
        spacing: AppStyle.spacing.xs
        TextField {
            id: tf
            Layout.fillWidth: true
            text: fieldRoot.text
            placeholderText: fieldRoot.placeholder
            font.pixelSize: AppStyle.fontSizes.sm
            color: AppStyle.colors.textPrimary
            onTextChanged: {
                if (tf.activeFocus && text !== fieldRoot.text) {
                    fieldRoot.textEdited(text);
                }
            }
            onEditingFinished: fieldRoot.editingFinished(text)
            background: Rectangle {
                color: AppStyle.colors.surface
                border.color: tf.focus ? AppStyle.colors.primary : AppStyle.colors.border
                radius: AppStyle.radius.sm
            }
        }
        ModernButton {
            visible: fieldRoot.browseIcon !== ""
            iconName: fieldRoot.browseIcon
            Layout.preferredWidth: 32
            Layout.preferredHeight: 32
            backgroundColor: AppStyle.colors.border
            onClicked: {
                tf.text = Qt.binding(function () {
                    return fieldRoot.text;
                });
                fieldRoot.browseClicked();
            }
        }
    }
}
