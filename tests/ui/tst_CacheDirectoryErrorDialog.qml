import QtQuick
import QtTest
import "../../src/ui/qml/components"

TestCase {
    name: "CacheDirectoryErrorDialog"
    width: 720
    height: 640
    visible: true
    when: windowShown

    property bool retryRequested: false

    CacheDirectoryErrorDialog {
        id: dialog
        selectedPath: "/home/example/cache"
        rejectionReason: "目录不是空目录"
        onRetrySelection: retryRequested = true
    }

    function findButton(item, label) {
        if (!item)
            return null;
        if (item.btnText === label)
            return item;
        var children = item.children || [];
        for (var i = 0; i < children.length; ++i) {
            var result = findButton(children[i], label);
            if (result)
                return result;
        }
        return null;
    }

    function init() {
        retryRequested = false;
        dialog.close();
        wait(0);
    }

    function test_rejectionDetailsRemainVisible() {
        dialog.open();
        tryCompare(dialog, "visible", true);
        compare(dialog.selectedPath, "/home/example/cache");
        compare(dialog.rejectionReason, "目录不是空目录");
    }

    function test_retryClosesDialogAndEmitsSelectionRequest() {
        dialog.open();
        var retryButton = findButton(dialog, "重新选择");
        verify(retryButton !== null);
        retryButton.clicked();
        compare(retryRequested, true);
        tryCompare(dialog, "visible", false, 500);
    }

    function test_closeAllowsDialogToOpenAgain() {
        dialog.open();
        var closeButton = findButton(dialog, "关闭");
        verify(closeButton !== null);
        closeButton.clicked();
        tryCompare(dialog, "visible", false, 500);
        dialog.open();
        tryCompare(dialog, "visible", true);
    }
}
