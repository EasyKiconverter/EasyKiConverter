import QtQuick
import QtTest
import "../../src/ui/qml/components"

TestCase {
    name: "UpdateDialog"
    width: 720
    height: 480
    visible: true
    when: windowShown

    QtObject {
        id: updateChecker
        property bool checking: false
        property bool hasUpdate: true
        property bool autoCheckEnabled: true
        property string statusText: "update_available"
        property string currentVersion: "3.1.12"
        property string latestVersion: "3.1.13"
        property string releaseName: "EasyKiConverter 3.1.13"
        property string releaseUrl: "https://example.com/release"
        property string latestReleasePageUrl: "https://github.com/EasyKiconverter/EasyKiConverter/releases/latest"
        property string assetUrl: "https://example.com/package"
        property string error: ""
        property int checkCount: 0
        property bool changedAutoCheck: false
        function checkForUpdates() { checkCount++ }
        function setAutoCheckEnabled(enabled) { changedAutoCheck = true; autoCheckEnabled = enabled }
    }

    UpdateDialog {
        id: dialog
        updateChecker: updateChecker
    }

    function findByObjectName(item, objectName) {
        if (!item)
            return null
        if (item.objectName === objectName)
            return item
        var children = item.children || []
        if (children.length === 0 && item.contentItem)
            children = [item.contentItem]
        for (var i = 0; i < children.length; ++i) {
            var child = findByObjectName(children[i], objectName)
            if (child)
                return child
        }
        return null
    }

    function init() {
        dialog.close()
        updateChecker.statusText = "update_available"
        updateChecker.checking = false
        updateChecker.hasUpdate = true
        updateChecker.checkCount = 0
        wait(0)
    }

    function test_versionDialogOpensFromVersionContext() {
        dialog.open()
        tryCompare(dialog, "visible", true)
        verify(findByObjectName(dialog, "dialogUpdateActionButton") !== null)
        verify(findByObjectName(dialog, "dialogViewReleaseButton") !== null)
    }

    function test_manualCheckUsesUpdateService() {
        dialog.open()
        var action = findByObjectName(dialog, "dialogUpdateActionButton")
        verify(action !== null)
        action.clicked()
        compare(updateChecker.checkCount, 1)
    }

    function test_failedCheckProvidesReleasePageAction() {
        updateChecker.statusText = "failed"
        updateChecker.hasUpdate = false
        dialog.open()
        var releaseButton = findByObjectName(dialog, "dialogOpenLatestReleaseButton")
        verify(releaseButton !== null)
    }

    function test_cancelCanOpenDialogAgain() {
        dialog.open()
        var closeButton = findByObjectName(dialog, "closeUpdateDialogButton")
        verify(closeButton !== null)
        closeButton.clicked()
        tryCompare(dialog, "visible", false, 500)
        dialog.open()
        tryCompare(dialog, "visible", true)
    }
}
