# ADR-011: Folder Dialog Path Binding Fix

## Status
Accepted

## Date
2026-06-27

## Related Issue
- [GitHub #179](https://github.com/EasyKiconverter/EasyKiConverter/issues/179)

## Context

In version 3.1.11, users reported that after clicking the "Browse" button to select a folder, the output path/cache directory input fields did not update to the selected path. Manually entering paths in the input fields worked correctly.

This issue affects both sidebar mode (SidebarTextField) and compact mode (ExportSettingsCard) UI paths.

### Root Cause Analysis

After tracing the complete signal chain, **two independent root causes** were identified:

#### Root Cause 1: `QUrl` Undefined in QML Context [Critical]

The `urlToLocalPath()` function in `MainWindow.qml` used `QUrl(url).toLocalFile()` for URL to local path conversion:

```qml
function urlToLocalPath(url) {
    return QUrl(url).toLocalFile();  // ← ReferenceError: QUrl is not defined
}
```

`QUrl` is a C++ type that requires `import QtQml` to be used in QML, but `MainWindow.qml` did not import this module. Runtime log:

```
MainWindow.qml:158: ReferenceError: QUrl is not defined
```

**Impact**: `urlToLocalPath(selectedFolder)` in `FolderDialog.onAccepted` always returns `undefined`, causing `setOutputPath(undefined)` to be called. Since the C++ side has an `if (m_outputPath != path)` guard and the comparison result of `m_outputPath` with `undefined` is indeterminate, the signal chain breaks here.

**Important Finding**: This means the folder selection feature **has never worked** since the `urlToLocalPath` function was introduced.

#### Root Cause 2: `currentFolder` Binding Format Error [Medium]

`FolderDialog.currentFolder` expects URL format (`file:///home/user/path`), but the binding passes a local path (`/home/user/path`):

```qml
currentFolder: exportSettingsController ? exportSettingsController.outputPath : ""
```

**Impact**: On some Qt versions/platforms, passing non-URL format may cause the dialog to open incorrectly or have wrong initial directory.

#### Root Cause 3: `SidebarTextField` Binding Fragility [Low]

The internal TextField binding pattern in `SidebarTextField.qml` may silently break on some Qt 6 / platform combinations:

```qml
text: fieldRoot.text       // Binding
onTextChanged: {
    if (tf.activeFocus) {  // Only propagate on user input
        fieldRoot.textEdited(text);
    }
}
```

When TextField briefly gains and loses `activeFocus` (e.g., clicking the browse button) without actual input, Qt 6's binding mechanism may "absorb" the `text` from the binding expression into a static value, preventing subsequent programmatic updates from reaching the display layer.

## Decision

Implement the following fixes, prioritized by severity:

### Fix 1: Replace `QUrl` with Pure JavaScript (Resolves Root Cause 1)

**Problem**: `QUrl` is not available in QML context

**Solution**: Implement cross-platform URL-to-path conversion using regex and `decodeURIComponent`:

```qml
function urlToLocalPath(url) {
    if (!url)
        return "";
    var s = url.toString();
    if (Qt.platform.os === "windows") {
        s = s.replace(/^file:\/\/\//, "");   // file:///C:/... → C:/...
    } else {
        s = s.replace(/^file:\/\//, "");     // file:///home/... → /home/...
    }
    return decodeURIComponent(s);
}
```

**Modified File**: `src/ui/qml/MainWindow.qml`

---

### Fix 2: Use Correct URL Format for `currentFolder` (Resolves Root Cause 2)

**Problem**: `FolderDialog.currentFolder` receives local path instead of URL

**Solution**: Convert local path to `file://` URL:

```qml
currentFolder: {
    var p = exportSettingsController ? exportSettingsController.outputPath : "";
    return p ? "file://" + p : "";
}
```

Also add null guard in `onAccepted`:

```qml
onAccepted: {
    var localPath = urlToLocalPath(selectedFolder);
    if (localPath) {
        exportSettingsController.setOutputPath(localPath);
    }
}
```

**Modified File**: `src/ui/qml/MainWindow.qml`

---

### Fix 3: Browse Button Binding Restoration + Brace Fix (Resolves Root Cause 3)

**Problem**: `SidebarTextField`'s `text` binding may break after focus changes

**Solution**: Explicitly restore binding in browse button's `onClicked`:

```qml
onClicked: {
    tf.text = Qt.binding(function() { return fieldRoot.text; });
    fieldRoot.browseClicked();
}
```

**Critical Detail**: Must use braces `{}` to wrap multiple statements. In QML, `onClicked: stmt1; stmt2` syntax means only `stmt1` belongs to the signal handler; `stmt2` is parsed as a property binding executed at instantiation.

**Modified Files**:
- `src/ui/qml/components/SidebarTextField.qml`
- `src/ui/qml/components/ExportSettingsCard.qml` (output path + cache directory browse buttons)

---

### Fix 4: `ExportSettingsCard` Binding Function Null Guard

**Problem**: `Qt.binding` function directly accesses `exportSettingsController.outputPath`, throws error when controller is null

**Solution**: Add ternary operator guard:

```qml
onClicked: {
    outputPathInput.text = Qt.binding(function () {
        return exportSettingsCard.exportSettingsController
            ? exportSettingsCard.exportSettingsController.outputPath : "";
    });
    exportSettingsCard.openOutputFolderDialog();
}
```

**Modified File**: `src/ui/qml/components/ExportSettingsCard.qml`

## Consequences

### Positive Impacts

1. **Folder selection functionality restored**: URL to local path conversion no longer depends on unavailable `QUrl` type
2. **Cross-platform compatibility**: Pure JS implementation correctly handles Linux (`file:///home/...`) and Windows (`file:///C:/...`) path formats
3. **Correct dialog initial directory**: `currentFolder` uses correct URL format, dialog opens from current configured path
4. **Improved binding robustness**: Browse button restores potentially broken bindings before opening dialog
5. **Error elimination**: Eliminates `QUrl is not defined` and `Cannot read property 'outputPath' of null` runtime errors

### Negative Impacts

1. **Simplified URL parsing**: Pure JS implementation doesn't handle all edge cases (e.g., UNC paths `\\server\share`, special Unicode characters), but sufficient for this project
2. **Debug logging**: `console.log` remains in `FolderDialog.onAccepted`, needs cleanup before release

### Regression Checklist

When modifying folder/cache directory selection paths in the future, verify these scenarios:

1. Sidebar mode: Click browse → Select folder → Path displays correctly
2. Compact mode: Click browse → Select folder → Path displays correctly
3. Manual path input → Enter → Settings saved correctly
4. Dialog initial directory is the current configured path
5. Paths with spaces or Chinese characters handled correctly
6. No `ReferenceError` or `TypeError` output in terminal

## Modified Files List

| File | Change Type | Description |
|------|-------------|-------------|
| `src/ui/qml/MainWindow.qml` | Fix | `urlToLocalPath` rewritten in pure JS; `FolderDialog.currentFolder` changed to URL format; `onAccepted` added null guard |
| `src/ui/qml/components/SidebarTextField.qml` | Fix | `onTextChanged` added `text !== fieldRoot.text` guard; browse button `onClicked` wrapped in braces and restored binding |
| `src/ui/qml/components/ExportSettingsCard.qml` | Fix | Two browse button `onClicked` restored binding + null guard + brace fix |

## Related Documents

- [ADR-001: MVVM Architecture](001-mvvm-architecture_en.md)
- [Architecture Design Document](../../developer/ARCHITECTURE_en.md)
