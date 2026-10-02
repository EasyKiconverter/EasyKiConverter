import QtQuick
import QtTest
import "../../src/ui/qml/components"

TestCase {
    name: "ExportFlow"
    width: 720
    height: 260
    visible: true
    when: windowShown

    QtObject {
        id: progressController
        property bool isExporting: false
        property bool isStopping: false
        property bool hasCompletedExport: false
        property bool retrySupported: true
        property int failureCount: 0
        property int startExportCallCount: 0
        property int cancelExportCallCount: 0
        property int retryFailedCallCount: 0
        property int openFolderCallCount: 0
        property var lastComponentIds: []
        property string lastOutputPath: ""
        property string lastLibName: ""
        property bool lastExportSymbol: false
        property bool lastExportFootprint: false
        property bool lastExportModel3D: false
        property int lastExportModel3DFormat: -1
        property int lastExportModel3DPathMode: -1
        property bool lastExportPreviewImages: false
        property bool lastExportDatasheet: false
        property bool lastOverwriteExistingFiles: false
        property bool lastUpdateMode: false
        property bool lastDebugMode: false
        property string lastSymbolLibraryDescription: ""
        property string lastFootprintLibraryDescription: ""
        property string lastFootprintLibraryKeywords: ""
        property bool openLastExportedFolderResult: true

        function startExport(componentIds, outputPath, libName, exportSymbol, exportFootprint, exportModel3D, exportModel3DFormat, exportModel3DPathMode, exportPreviewImages, exportDatasheet, overwriteExistingFiles, updateMode, debugMode, symbolLibraryDescription, footprintLibraryDescription, footprintLibraryKeywords) {
            startExportCallCount += 1
            lastComponentIds = componentIds
            lastOutputPath = outputPath
            lastLibName = libName
            lastExportSymbol = exportSymbol
            lastExportFootprint = exportFootprint
            lastExportModel3D = exportModel3D
            lastExportModel3DFormat = exportModel3DFormat
            lastExportModel3DPathMode = exportModel3DPathMode
            lastExportPreviewImages = exportPreviewImages
            lastExportDatasheet = exportDatasheet
            lastOverwriteExistingFiles = overwriteExistingFiles
            lastUpdateMode = updateMode
            lastDebugMode = debugMode
            lastSymbolLibraryDescription = symbolLibraryDescription
            lastFootprintLibraryDescription = footprintLibraryDescription
            lastFootprintLibraryKeywords = footprintLibraryKeywords
        }

        function cancelExport() {
            cancelExportCallCount += 1
            isStopping = true
        }

        function retryFailedComponents() {
            retryFailedCallCount += 1
        }

        function openLastExportedFolder() {
            openFolderCallCount += 1
            return openLastExportedFolderResult
        }
    }

    QtObject {
        id: settingsController
        property string outputPath: "/tmp/easykiconverter-ui-flow"
        property string libName: "UiFlowLib"
        property bool exportSymbol: true
        property bool exportFootprint: true
        property bool exportModel3D: false
        property int exportModel3DFormat: 3
        property int exportModel3DPathMode: 0
        property bool exportPreviewImages: false
        property bool exportDatasheet: false
        property bool overwriteExistingFiles: false
        property int exportMode: 0
        property bool debugMode: false
        property string symbolLibraryDescription: ""
        property string footprintLibraryDescription: ""
        property string footprintLibraryKeywords: ""
    }

    QtObject {
        id: componentListController
        property var ids: ["C2040", "C25804"]
        property int componentCount: ids.length

        function getAllComponentIds() {
            return ids
        }
    }

    ExportButtonsSection {
        id: buttons
        width: parent.width
        exportProgressController: progressController
        exportSettingsController: settingsController
        componentListController: componentListController
    }

    function init() {
        progressController.isExporting = false
        progressController.isStopping = false
        progressController.hasCompletedExport = false
        progressController.retrySupported = true
        progressController.failureCount = 0
        progressController.startExportCallCount = 0
        progressController.cancelExportCallCount = 0
        progressController.retryFailedCallCount = 0
        progressController.openFolderCallCount = 0
        progressController.lastComponentIds = []
        progressController.lastOutputPath = ""
        progressController.lastLibName = ""
        progressController.lastExportSymbol = false
        progressController.lastExportFootprint = false
        progressController.lastExportModel3D = false
        progressController.lastExportModel3DFormat = -1
        progressController.lastExportModel3DPathMode = -1
        progressController.lastExportPreviewImages = false
        progressController.lastExportDatasheet = false
        progressController.lastOverwriteExistingFiles = false
        progressController.lastUpdateMode = false
        progressController.lastDebugMode = false
        progressController.lastSymbolLibraryDescription = ""
        progressController.lastFootprintLibraryDescription = ""
        progressController.lastFootprintLibraryKeywords = ""
        progressController.openLastExportedFolderResult = true

        settingsController.exportSymbol = true
        settingsController.exportFootprint = true
        settingsController.exportModel3D = false
        settingsController.exportModel3DFormat = 3
        settingsController.exportModel3DPathMode = 0
        settingsController.exportPreviewImages = false
        settingsController.exportDatasheet = false
        settingsController.overwriteExistingFiles = false
        settingsController.exportMode = 0
        settingsController.debugMode = false
        settingsController.symbolLibraryDescription = ""
        settingsController.footprintLibraryDescription = ""
        settingsController.footprintLibraryKeywords = ""
        componentListController.ids = ["C2040", "C25804"]
        buttons.exportErrorDialog.close()
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

    function startButton() {
        var button = findByObjectName(buttons, "startExportButton")
        verify(button !== null, "start export button should be discoverable")
        return button
    }

    function stopButton() {
        var button = findByObjectName(buttons, "stopExportButton")
        verify(button !== null, "stop export button should be discoverable")
        return button
    }

    function openFolderButton() {
        var button = findByObjectName(buttons, "openExportFolderButton")
        verify(button !== null, "open export folder button should be discoverable")
        return button
    }

    function errorDialog() {
        var dialog = buttons.exportErrorDialog
        verify(dialog !== null, "export error dialog should be discoverable")
        return dialog
    }

    function test_startExportButtonDisabledWithoutComponents() {
        componentListController.ids = []
        wait(0)

        compare(startButton().enabled, false)
    }

    function test_startExportButtonDisabledWithoutLibraryExportType() {
        settingsController.exportSymbol = false
        settingsController.exportFootprint = false
        settingsController.exportModel3D = false
        wait(0)

        compare(startButton().enabled, false)
    }

    function test_startExportPassesSelectedComponentsAndSettings() {
        var button = startButton()
        compare(button.enabled, true)

        mouseClick(button)

        compare(progressController.startExportCallCount, 1)
        compare(progressController.lastComponentIds.length, 2)
        compare(progressController.lastComponentIds[0], "C2040")
        compare(progressController.lastComponentIds[1], "C25804")
        compare(progressController.lastOutputPath, settingsController.outputPath)
        compare(progressController.lastLibName, settingsController.libName)
        compare(progressController.lastExportSymbol, true)
        compare(progressController.lastExportFootprint, true)
        compare(progressController.lastExportModel3D, false)
    }

    function test_startExportPassesAdvancedExportSettings() {
        settingsController.exportSymbol = false
        settingsController.exportFootprint = true
        settingsController.exportModel3D = true
        settingsController.exportModel3DFormat = 1
        settingsController.exportModel3DPathMode = 2
        settingsController.exportPreviewImages = true
        settingsController.exportDatasheet = true
        settingsController.overwriteExistingFiles = true
        settingsController.exportMode = 1
        settingsController.debugMode = true
        settingsController.symbolLibraryDescription = "symbol library description"
        settingsController.footprintLibraryDescription = "footprint library description"
        settingsController.footprintLibraryKeywords = "resistor capacitor"
        wait(0)

        mouseClick(startButton())

        compare(progressController.startExportCallCount, 1)
        compare(progressController.lastExportSymbol, false)
        compare(progressController.lastExportFootprint, true)
        compare(progressController.lastExportModel3D, true)
        compare(progressController.lastExportModel3DFormat, 1)
        compare(progressController.lastExportModel3DPathMode, 2)
        compare(progressController.lastExportPreviewImages, true)
        compare(progressController.lastExportDatasheet, true)
        compare(progressController.lastOverwriteExistingFiles, true)
        compare(progressController.lastUpdateMode, true)
        compare(progressController.lastDebugMode, true)
        compare(progressController.lastSymbolLibraryDescription, "symbol library description")
        compare(progressController.lastFootprintLibraryDescription, "footprint library description")
        compare(progressController.lastFootprintLibraryKeywords, "resistor capacitor")
    }

    function test_model3DOnlyExportIsAllowed() {
        settingsController.exportSymbol = false
        settingsController.exportFootprint = false
        settingsController.exportModel3D = true
        wait(0)

        compare(startButton().enabled, true)

        mouseClick(startButton())

        compare(progressController.startExportCallCount, 1)
        compare(progressController.lastExportSymbol, false)
        compare(progressController.lastExportFootprint, false)
        compare(progressController.lastExportModel3D, true)
    }

    function test_exportingStateShowsStopAndDisablesStart() {
        progressController.isExporting = true
        wait(0)

        compare(startButton().enabled, false)
        compare(stopButton().visible, true)
        compare(stopButton().enabled, true)
    }

    function test_stopExportCallsCancelAndDisablesWhileStopping() {
        progressController.isExporting = true
        progressController.isStopping = false
        wait(0)

        compare(stopButton().text, qsTranslate("MainWindow", "停止转换"))

        mouseClick(stopButton())

        compare(progressController.cancelExportCallCount, 1)
        compare(stopButton().enabled, false)
        compare(stopButton().text, qsTranslate("MainWindow", "正在停止..."))
    }

    function test_failedStateRetriesInsteadOfStartingNewExport() {
        progressController.failureCount = 2
        wait(0)

        mouseClick(startButton())

        compare(progressController.retryFailedCallCount, 1)
        compare(progressController.startExportCallCount, 0)
    }

    function test_completedExportShowsOpenFolderAction() {
        progressController.hasCompletedExport = true
        wait(0)

        var button = openFolderButton()
        compare(button.visible, true)
        compare(button.enabled, true)

        button.clicked()

        compare(progressController.openFolderCallCount, 1)
    }

    function test_openFolderFailureShowsErrorDialog() {
        progressController.hasCompletedExport = true
        progressController.openLastExportedFolderResult = false
        wait(0)

        var dialog = errorDialog()
        compare(dialog.visible, false)

        openFolderButton().clicked()
        wait(0)

        compare(progressController.openFolderCallCount, 1)
        compare(dialog.visible, true)

        dialog.close()
    }
}
