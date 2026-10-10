# ADR-011: 文件夹对话框路径绑定修复

## 状态
已接受 (Accepted)

## 日期
2026-06-27

## 关联 Issue
- [GitHub #179](https://github.com/EasyKiconverter/EasyKiConverter/issues/179)

## 上下文

在 v3.1.11 版本中，用户报告点击"浏览"按钮选择文件夹后，输出路径/缓存目录输入框未更新为所选路径。手动在输入框中输入路径则正常工作。

该问题影响侧边栏模式（SidebarTextField）和紧凑模式（ExportSettingsCard）两条 UI 路径。

### 根因分析

追踪完整信号链后发现**两个独立的根因**：

#### 根因 1：`QUrl` 在 QML 上下文中未定义 [严重]

`MainWindow.qml` 中的 `urlToLocalPath()` 函数使用了 `QUrl(url).toLocalFile()` 进行 URL 到本地路径的转换：

```qml
function urlToLocalPath(url) {
    return QUrl(url).toLocalFile();  // ← ReferenceError: QUrl is not defined
}
```

`QUrl` 是 C++ 类型，需要 `import QtQml` 才能在 QML 中使用，但 `MainWindow.qml` 未导入该模块。运行时日志：

```
MainWindow.qml:158: ReferenceError: QUrl is not defined
```

**影响**：`FolderDialog.onAccepted` 中调用 `urlToLocalPath(selectedFolder)` 始终返回 `undefined`，导致 `setOutputPath(undefined)` 被调用。由于 C++ 侧有 `if (m_outputPath != path)` 守卫且 `m_outputPath` 与 `undefined` 的比较结果不确定，信号链在此处断裂。

**重要发现**：此问题意味着文件夹选择功能在引入 `urlToLocalPath` 函数后**从未正常工作过**。

#### 根因 2：`currentFolder` 绑定格式错误 [中等]

`FolderDialog.currentFolder` 期望 URL 格式（`file:///home/user/path`），但绑定传入的是本地路径（`/home/user/path`）：

```qml
currentFolder: exportSettingsController ? exportSettingsController.outputPath : ""
```

**影响**：在某些 Qt 版本/平台上，传入非 URL 格式可能导致对话框打开异常或初始目录不正确。

#### 根因 3：`SidebarTextField` 绑定脆弱性 [低]

`SidebarTextField.qml` 内部 TextField 的绑定模式在某些 Qt 6 / 平台组合下可能静默断裂：

```qml
text: fieldRoot.text       // 绑定
onTextChanged: {
    if (tf.activeFocus) {  // 仅用户输入时传播
        fieldRoot.textEdited(text);
    }
}
```

当 TextField 短暂获得又失去 `activeFocus`（如点击浏览按钮）但无实际输入时，Qt 6 的绑定机制可能将 `text` 从绑定表达式"吸收"为静态值，导致后续程序化更新无法到达显示层。

## 决策

实施以下修复方案，按优先级排列：

### 修复 1：用纯 JavaScript 替代 `QUrl`（解决根因 1）

**问题**：`QUrl` 在 QML 上下文中不可用

**解决方案**：用正则表达式和 `decodeURIComponent` 实现跨平台 URL 转路径：

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

**修改文件**：`src/ui/qml/MainWindow.qml`

---

### 修复 2：`currentFolder` 使用正确的 URL 格式（解决根因 2）

**问题**：`FolderDialog.currentFolder` 收到本地路径而非 URL

**解决方案**：将本地路径转换为 `file://` URL：

```qml
currentFolder: {
    var p = exportSettingsController ? exportSettingsController.outputPath : "";
    return p ? "file://" + p : "";
}
```

同时增加 `onAccepted` 中的空值守卫：

```qml
onAccepted: {
    var localPath = urlToLocalPath(selectedFolder);
    if (localPath) {
        exportSettingsController.setOutputPath(localPath);
    }
}
```

**修改文件**：`src/ui/qml/MainWindow.qml`

---

### 修复 3：浏览按钮绑定恢复 + 大括号修复（解决根因 3）

**问题**：`SidebarTextField` 的 `text` 绑定可能在焦点切换后断裂

**解决方案**：在浏览按钮的 `onClicked` 中显式恢复绑定：

```qml
onClicked: {
    tf.text = Qt.binding(function() { return fieldRoot.text; });
    fieldRoot.browseClicked();
}
```

**关键细节**：必须使用大括号 `{}` 包裹多条语句。QML 中 `onClicked: stmt1; stmt2` 的写法只有 `stmt1` 属于信号处理器，`stmt2` 会被解析为属性绑定在实例化时执行。

**修改文件**：
- `src/ui/qml/components/SidebarTextField.qml`
- `src/ui/qml/components/ExportSettingsCard.qml`（输出路径 + 缓存目录两处浏览按钮）

---

### 修复 4：`ExportSettingsCard` 绑定函数 null 守卫

**问题**：`Qt.binding` 函数中直接访问 `exportSettingsController.outputPath`，当 controller 为 null 时报错

**解决方案**：增加三元运算守卫：

```qml
onClicked: {
    outputPathInput.text = Qt.binding(function () {
        return exportSettingsCard.exportSettingsController
            ? exportSettingsCard.exportSettingsController.outputPath : "";
    });
    exportSettingsCard.openOutputFolderDialog();
}
```

**修改文件**：`src/ui/qml/components/ExportSettingsCard.qml`

## 后果

### 正面影响

1. **文件夹选择功能恢复正常**：URL 到本地路径的转换不再依赖不可用的 `QUrl` 类型
2. **跨平台兼容**：纯 JS 实现正确处理 Linux（`file:///home/...`）和 Windows（`file:///C:/...`）路径格式
3. **对话框初始目录正确**：`currentFolder` 使用正确的 URL 格式，对话框从当前配置路径打开
4. **绑定鲁棒性提升**：浏览按钮在弹出对话框前恢复可能断裂的绑定
5. **错误消除**：消除了 `QUrl is not defined` 和 `Cannot read property 'outputPath' of null` 运行时错误

### 负面影响

1. **URL 解析简化**：纯 JS 实现不处理所有边界情况（如 UNC 路径 `\\server\share`、特殊 Unicode 字符），但对本项目够用
2. **调试日志**：`FolderDialog.onAccepted` 中保留了 `console.log`，正式发布前需清理

### 回归检查清单

后续修改文件夹/缓存目录选择链路时，验证以下场景：

1. 侧边栏模式：点击浏览 → 选择文件夹 → 路径正确显示
2. 紧凑模式：点击浏览 → 选择文件夹 → 路径正确显示
3. 手动输入路径 → 回车 → 设置正确保存
4. 对话框初始目录为当前已配置路径
5. 路径包含空格或中文字符时正确处理
6. 终端无 `ReferenceError` 或 `TypeError` 输出

## 修改文件清单

| 文件 | 改动类型 | 说明 |
|------|---------|------|
| `src/ui/qml/MainWindow.qml` | 修复 | `urlToLocalPath` 用纯 JS 重写；`FolderDialog.currentFolder` 改为 URL 格式；`onAccepted` 增加空值守卫 |
| `src/ui/qml/components/SidebarTextField.qml` | 修复 | `onTextChanged` 增加 `text !== fieldRoot.text` 守卫；浏览按钮 `onClicked` 用大括号包裹并恢复绑定 |
| `src/ui/qml/components/ExportSettingsCard.qml` | 修复 | 两处浏览按钮 `onClicked` 恢复绑定 + null 守卫 + 大括号修复 |

## 相关文档

- [ADR-001: MVVM 架构](001-mvvm-architecture.md)
- [架构设计文档](../../developer/ARCHITECTURE.md)
