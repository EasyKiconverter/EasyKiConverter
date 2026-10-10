import QtQuick
import QtTest
import "../../src/ui/qml/components"

TestCase {
    name: "UpdateBanner"
    width: 720
    height: 120
    visible: true
    when: windowShown

    QtObject {
        id: updateChecker
        property bool checking: false
        property bool hasUpdate: true
        property bool dismissed: false
        property string statusText: "update_available"
        property string currentVersion: "3.1.12"
        property string latestVersion: "3.1.13"
        property string releaseName: "EasyKiConverter 3.1.13"
        property string releaseUrl: "https://example.com/release"
        property string assetUrl: "https://example.com/package"
        property string error: ""
        property int dismissCount: 0
        property int ignoreCount: 0
        property int checkCount: 0
        function dismissUpdate() { dismissCount++ }
        function ignoreUpdate() { ignoreCount++ }
        function checkForUpdates() { checkCount++ }
    }

    UpdateBanner {
        id: banner
        width: parent.width
        updateChecker: updateChecker
    }

    function init() {
        updateChecker.checking = false
        updateChecker.hasUpdate = true
        updateChecker.dismissed = false
        updateChecker.statusText = "update_available"
        updateChecker.error = ""
        updateChecker.dismissCount = 0
        updateChecker.ignoreCount = 0
        updateChecker.checkCount = 0
        wait(0)
    }

    function findByObjectName(item, objectName) {
        if (!item)
            return null
        if (item.objectName === objectName)
            return item
        for (var i = 0; i < item.children.length; ++i) {
            var child = findByObjectName(item.children[i], objectName)
            if (child)
                return child
        }
        return null
    }

    function button(name) {
        var result = findByObjectName(banner, name)
        verify(result !== null, name + " should be discoverable")
        return result
    }

    function test_updateActionsAreVisible() {
        compare(banner.visible, true)
        compare(button("remindLaterButton").visible, true)
        compare(button("ignoreVersionButton").visible, true)
    }

    function test_remindLaterAndIgnoreCallService() {
        button("remindLaterButton").clicked()
        button("ignoreVersionButton").clicked()
        compare(updateChecker.dismissCount, 1)
        compare(updateChecker.ignoreCount, 1)
    }

    function test_failedStateShowsRetry() {
        updateChecker.hasUpdate = false
        updateChecker.statusText = "failed"
        updateChecker.error = "offline"
        wait(0)
        compare(button("updateActionButton").text, "重试")
        button("updateActionButton").clicked()
        compare(updateChecker.checkCount, 1)
    }
}
