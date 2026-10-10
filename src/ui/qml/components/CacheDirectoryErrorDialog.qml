import QtQuick
import QtQuick.Layouts
import EasyKiconverter_Cpp_Version.src.ui.qml.styles 1.0

/**
 * @brief 缓存目录拒绝或迁移失败提示对话框。
 *
 * 该对话框挂载在 ApplicationWindow 的窗口级 overlay 中，避免被 Card
 * 的 ColumnLayout 当作布局子项处理。关闭后不会改变当前生效的缓存目录。
 */
SliderDialogBase {
    id: root
    hasOverlay: true
    property string selectedPath: ""
    property string rejectionReason: ""
    signal retrySelection
    title: qsTranslate("MainWindow", "缓存目录无法使用")
    message: qsTranslate("MainWindow", "所选目录未切换，当前缓存目录保持不变。")
    mainContentSource: ColumnLayout {
        spacing: AppStyle.spacing.sm
        Text {
            Layout.fillWidth: true
            text: qsTranslate("MainWindow", "选择路径")
            color: AppStyle.colors.textSecondary
            font.pixelSize: AppStyle.fontSizes.xs
        }
        Text {
            Layout.fillWidth: true
            text: root.selectedPath
            color: AppStyle.colors.textPrimary
            font.pixelSize: AppStyle.fontSizes.sm
            wrapMode: Text.WrapAnywhere
            maximumLineCount: 3
            elide: Text.ElideMiddle
        }
        Text {
            Layout.fillWidth: true
            text: qsTranslate("MainWindow", "拒绝原因")
            color: AppStyle.colors.textSecondary
            font.pixelSize: AppStyle.fontSizes.xs
        }
        Text {
            Layout.fillWidth: true
            text: root.rejectionReason
            color: AppStyle.colors.danger
            font.pixelSize: AppStyle.fontSizes.sm
            wrapMode: Text.WordWrap
        }
        Text {
            Layout.fillWidth: true
            text: qsTranslate("MainWindow", "可选目录条件")
            color: AppStyle.colors.textSecondary
            font.pixelSize: AppStyle.fontSizes.xs
        }
        Text {
            Layout.fillWidth: true
            text: qsTranslate("MainWindow", "空目录（应用会创建并写入所有权标记）；已包含 EasyKiConverter 所有权标记的目录；或可安全接管的旧缓存目录。主目录本身、文件系统根目录、包含符号链接的路径，以及非空且无法证明归属的目录不会被接受。")
            color: AppStyle.colors.textPrimary
            font.pixelSize: AppStyle.fontSizes.sm
            wrapMode: Text.WordWrap
            lineHeight: 1.2
        }
    }
    buttonSpecs: [
        {
            text: qsTranslate("MainWindow", "重新选择"),
            color: AppStyle.colors.primary,
            action: function () {
                root.retrySelection();
                root.closeWithAnimation();
            }
        },
        {
            isSeparator: true
        },
        {
            text: qsTranslate("MainWindow", "关闭"),
            color: AppStyle.colors.textSecondary,
            action: function () {
                root.closeWithAnimation();
            }
        }
    ]
}
