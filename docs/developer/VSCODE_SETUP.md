# VS Code 配置指南

[English version](VSCODE_SETUP_en.md)

本文档说明仓库共享的 VS Code 配置和开发者个人配置的边界。仓库默认使用 clangd 提供 C++ 语言服务，CMake Tools 只负责 CMake 配置、构建和运行目标；两者不重复提供诊断、补全和跳转。

## 配置归属

| 配置 | 位置 | 归属 |
| --- | --- | --- |
| CMake 源码目录和 `build/` 构建目录 | `.vscode/settings.json` | 仓库共享 |
| 生成 `build/compile_commands.json` | `.vscode/settings.json` | 仓库共享 |
| 构建产物搜索和监视排除 | `.vscode/settings.json` | 仓库共享 |
| C++ 编译参数 | `build/compile_commands.json` 和 `.clangd` | 由构建生成，仓库共享入口 |
| C++ 语言服务 | `llvm-vs-code-extensions.vscode-clangd` 和 `.clangd` | 仓库默认 |
| CMake Tools、Qt QML 等扩展推荐 | `.vscode/extensions.json` | 仓库共享 |
| 字体、主题、光标、滚动、自动保存、终端字号 | VS Code User Settings | 开发者个人 |
| Qt、编译器和 clangd 可执行文件路径 | 环境变量、VS Code User Settings 或被 Git 忽略的本地 workspace | 开发者个人 |

`.vscode/c_cpp_properties.json` 已移除，因为它与 clangd、CMake Tools 和 `compile_commands.json` 重复维护同一套 C++ 配置。

## 从干净克隆开始

1. 安装 VS Code，并在扩展推荐中安装 CMake Tools、clangd、Qt QML、Python、YAML、GitHub Actions 和 Markdown 工具。
2. 按[本地编译指南](BUILD.md)准备项目 `.venv`、Qt 6.6+、CMake 和编译器。
3. 打开仓库根目录。CMake Tools 会使用共享设置配置 `build/`，并请求生成编译数据库。
4. 执行项目规定的环境检查和构建：

```bash
.venv/bin/python tools/python/build_project.py --check
.venv/bin/python tools/python/build_project.py --test
```

Linux/macOS 应将 `Qt6_DIR`、`CMAKE_PREFIX_PATH` 和 `PATH` 指向项目专用 Qt；Windows 使用本机 Qt 安装路径设置对应的环境变量。不要把这些路径写入仓库文件。

## 本机路径配置

### 环境变量方式

这是项目构建工具支持的方式。启动 VS Code 前在终端中设置环境变量，或通过操作系统的用户环境配置设置。不要依赖 shell 的 `~` 展开，也不要提交绝对路径：

```bash
export Qt6_DIR=/your/qt/6.10.x/gcc_64/lib/cmake/Qt6
export CMAKE_PREFIX_PATH=/your/qt/6.10.x/gcc_64
export PATH=/your/qt/6.10.x/gcc_64/bin:$PATH
```

Windows 示例应使用本机实际路径，例如 `C:\Qt\6.10.x\msvc2022_64`，并在用户环境变量中配置，而不是复制到共享 `settings.json`。

### 被 Git 忽略的本地 workspace

需要为 CMake Tools 或 clangd 指定本机路径时，可以在本地创建 `.vscode/EasyKiConverter.local.code-workspace`。该目录下的 workspace 文件已被 `.gitignore` 忽略；不要创建 `settings.local.json`，VS Code 不会自动合并这个文件。

示例结构如下，路径必须替换为本机真实路径：

```json
{
    "folders": [{ "path": ".." }],
    "settings": {
        "cmake.configureEnvironment": {
            "Qt6_DIR": "/your/qt/6.10.x/gcc_64/lib/cmake/Qt6",
            "CMAKE_PREFIX_PATH": "/your/qt/6.10.x/gcc_64"
        },
        "clangd.path": "/your/llvm/bin/clangd"
    }
}
```

Windows、macOS 和 Linux 的路径格式不同；示例中的 `/your/...` 只是占位符。不要在共享仓库配置中固定编译器、Qt 或 clangd 的绝对路径。

## C++ F12 跳转条件

F12 跳转由 clangd 提供，需要满足：

- 推荐扩展 `llvm-vs-code-extensions.vscode-clangd` 已安装；
- CMake 已成功配置项目，并生成 `build/compile_commands.json`；
- `.clangd` 的 `CompilationDatabase: ./build` 能从仓库根目录找到该数据库；
- 当前文件属于项目源码或测试目录，并且数据库中存在对应编译参数。

如果跳转结果过期，先重新运行项目构建工具或在 CMake Tools 中重新配置，再重启 clangd 索引。不要同时启用 cpptools 的 IntelliSense；如果个人环境必须使用 cpptools，应在本地 workspace 中选择它，并关闭 clangd，避免两个语言服务同时报告诊断。

仓库没有在 VS Code 中自动验证 F12 跳转；实际跳转结果取决于开发者安装的扩展、编译器和本机 Qt 环境。

## 共享配置的范围

仓库只共享 CMake 路径、编译数据库生成、文件关联、编码和构建产物排除规则。编辑器外观、交互习惯、自动保存和工具链路径都属于开发者个人设置，修改它们不会成为项目配置变更。
