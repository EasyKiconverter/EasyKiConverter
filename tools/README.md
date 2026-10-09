# 工具 (Tools)

此目录包含 EasyKiConverter 项目的各种开发、构建和维护工具。

> [!IMPORTANT]
> **使用须知**:
>
> - 运行脚本前，请确保您已满足该工具的所有先决条件。
> - 建议始终在项目的根目录执行工具，以确保路径引用正确。
> - 请在执行可能修改源代码的工具（如格式化工具）前，确保您的工作区是干净的（已提交或备份）。

## 目录结构

```
tools/
├── windows/          # Windows 平台特定工具
│   ├── format_code.bat           # C++ 代码格式化工具
│   ├── format_qml.bat            # QML 代码格式化工具
│   └── build_project.bat         # Python 构建工具包装器
├── linux/            # Linux 平台特定工具
│   ├── build-packages.sh         # Linux 打包脚本
│   ├── fix-desktop-icon.sh       # 桌面图标修复脚本
│   └── BUILD_PACKAGES_README.md  # Linux 打包指南
├── macos/            # macOS 平台特定工具
└── python/           # 跨平台 Python 工具
    ├── build_project.py          # 项目构建管理器
    ├── manage_version.py         # 版本号管理工具
    ├── manage_translations.py    # 翻译文件管理工具
    ├── format_code.py            # 代码格式化工具
    ├── check_comment_rate.py     # C++ 注释率与注释策略检查工具
    ├── count_lines.py            # 代码行数统计工具
    ├── analyze_lines.py          # 代码复杂度分析工具
    ├── build_docs.py             # 文档构建工具
    ├── check_env.py              # 环境依赖检查工具
    ├── convert_to_utf8.py        # 文件编码转换工具
    └── fix_qml_translations.py   # QML 翻译修复工具
```

## Linux 工具

### [build-packages.sh](linux/build-packages.sh)

Linux 平台打包脚本，支持 AppImage、DEB、RPM、Arch Linux 和 Flatpak 打包。

**快速用法:**
```bash
# 构建所有包
./tools/linux/build-packages.sh all

# 仅构建 AppImage
./tools/linux/build-packages.sh appimage

# 仅构建 DEB 包
./tools/linux/build-packages.sh deb

# 指定版本
VERSION=3.0.14 ./tools/linux/build-packages.sh all
```

### [fix-desktop-icon.sh](linux/fix-desktop-icon.sh)

Linux 桌面图标修复脚本，用于修复用户级别桌面文件覆盖系统配置的问题。

**快速用法:**
```bash
sudo ./tools/linux/fix-desktop-icon.sh
```

## Windows 工具

### [format_code.bat](windows/format_code.bat)

基于 `clang-format` 的 C++ 代码格式化工具。

### [build_project.bat](windows/build_project.bat)

Windows 下的一键构建脚本，支持 Debug/Release 模式。

**快速用法:**
```bash
# 默认 Debug 构建
tools\windows\build_project.bat
# Release 构建
tools\windows\build_project.bat Release
```

> [!TIP]
> 更多关于环境配置、功能细节和注意事项，请参阅对应脚本头部的详细注释。

## Python 工具

### [build_project.py](python/build_project.py)

项目构建管理工具，支持环境检查、CMake 配置、并行编译和日志管理。

**快速用法:**
```bash
# 默认 Debug 构建
python tools/python/build_project.py

# Release 构建
python tools/python/build_project.py -t Release --parallel

# 仅检查环境
python tools/python/build_project.py --check
```

### [manage_version.py](python/manage_version.py)

跨平台版本同步管理工具，统一更新 CMakeLists.txt、vcpkg.json 和源码中的版本号。

**快速用法:**
```bash
# 检查当前版本
python tools/python/manage_version.py --check

# 更新到新版本
python tools/python/manage_version.py 3.0.8
```

### [manage_translations.py](python/manage_translations.py)

自动化 Qt 翻译流程（lupdate/lrelease），支持自动提取和编译翻译文件。

**快速用法:**
```bash
# 更新并编译所有翻译文件
python tools/python/manage_translations.py --all

# 仅更新翻译源文件
python tools/python/manage_translations.py --update

# 仅编译翻译文件
python tools/python/manage_translations.py --release
```

### [format_code.py](python/format_code.py)

统一的代码格式化工具，支持 C++ 和 QML 文件格式化。

**快速用法:**
```bash
# 格式化所有代码
.venv/bin/python tools/python/format_code.py --all

# 仅格式化 C++ 文件
.venv/bin/python tools/python/format_code.py --cpp

# 仅格式化 QML 文件
.venv/bin/python tools/python/format_code.py --qml

# 检查格式而不修改（CI 模式）
.venv/bin/python tools/python/format_code.py --check --all
```

### [count_lines.py](python/count_lines.py)

跨平台代码行数统计工具，以 KLoC (千行代码) 为单位输出项目规模。

**快速用法:**
```bash
# 统计项目代码行数
python tools/python/count_lines.py

# JSON 格式输出
python tools/python/count_lines.py --json
```

### [analyze_lines.py](python/analyze_lines.py)

代码复杂度分析工具，识别高耦合风险文件。

**快速用法:**
```bash
# 分析整个项目
python tools/python/analyze_lines.py

# 启用终端超链接（点击文件可打开）
python tools/python/analyze_lines.py --link

# 分析指定目录
python tools/python/analyze_lines.py tests
```

### [build_docs.py](python/build_docs.py)

文档构建工具，支持 Doxygen API 文档和 MkDocs 站点生成。

**快速用法:**
```bash
# 构建所有文档
python tools/python/build_docs.py --all

# 仅生成 API 文档
python tools/python/build_docs.py --doxygen

# 本地预览文档
python tools/python/build_docs.py --serve
```

### [check_env.py](python/check_env.py)

开发依赖环境检查工具，验证 CMake、编译器、Qt6 等依赖。

**快速用法:**
```bash
python tools/python/check_env.py
```

### [convert_to_utf8.py](python/convert_to_utf8.py)

文件编码转换工具，将文件转换为 UTF-8 编码（无 BOM）。

**快速用法:**
```bash
# 转换整个 src 目录
python tools/python/convert_to_utf8.py src/
```

### [check_comment_rate.py](python/check_comment_rate.py)

检查 C++ 逻辑代码的注释覆盖率，并可拒绝源码注释中的外部项目引用；使用 `--check-variables` 时还会检查可识别的变量声明注释，使用 `--check-params` 时检查函数 Doxygen 的 `@param` 覆盖情况，使用 `--check-doxygen` 时检查每个函数定义是否紧邻包含 `@brief` 的多行 Doxygen 块。

**快速用法:**
```bash
# 检查指定文件，注释率不得低于 90%
python tools/python/check_comment_rate.py --threshold 90 src/core/utils/GeometryUtils.cpp

# 同时检查可识别的变量声明是否有注释
python tools/python/check_comment_rate.py --check-variables src/services/export/TempFileManager.cpp

# 同时检查函数参数是否有 Doxygen @param 说明
python tools/python/check_comment_rate.py --check-params src/core/allegro/ExporterAllegroFootprint.cpp

# 同时检查函数 Doxygen 注释是否包含 @brief
python tools/python/check_comment_rate.py --check-doxygen src/core/allegro/ExporterAllegroFootprint.cpp

# 同时检查外部项目引用注释
python tools/python/check_comment_rate.py --threshold 100 --forbid-external-references \
  $(git diff --name-only master...HEAD -- '*.cpp' '*.h')
```

该工具只检查源码注释文本，不会修改源文件；外部项目名称和“参考实现”等表达会被报告为策略违规。

### [fix_qml_translations.py](python/fix_qml_translations.py)

QML 翻译修复工具，修复 QML 文件中的翻译相关问题。

**快速用法:**
```bash
python tools/python/fix_qml_translations.py
```

> [!TIP]
> 以上 Python 工具均支持优先使用环境变量中的工具链，若未配置则回退到脚本内定义的路径变量。详情请参阅各脚本顶部的说明文档。

## 命令行帮助支持

所有命令行工具必须支持 `--help` 或 `-h` 参数，以提供使用说明。

### 实现要求

| 工具类型 | 推荐实现方式 | 示例 |
|----------|-------------|------|
| Python 脚本 | 使用 `argparse` 模块 | `parser = argparse.ArgumentParser(...)` |
| Windows 批处理 | 检测 `-h`, `--help`, `/h` 参数 | `if "%~1"=="--help" goto help` |
| Shell 脚本 | 检测 `-h`, `--help` 参数 | `case "$1" in --help) show_help ;; esac` |

### 帮助内容要求

帮助信息应包含：
1. **工具名称和简介** - 一句话说明工具用途
2. **用法格式** - 命令行语法 (Usage)
3. **参数说明** - 每个参数的含义和默认值
4. **使用示例** - 常见使用场景
5. **环境要求** - 依赖的工具或库

### 示例 (Python)

```python
import argparse

def main():
    parser = argparse.ArgumentParser(
        description="工具简述",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
示例:
  python tool.py --option value
  python tool.py --help
        """
    )
    parser.add_argument("--input", help="输入文件路径")
    parser.add_argument("--verbose", action="store_true", help="详细输出")
    args = parser.parse_args()
```

## 添加新工具

为了保持项目工具链的可维护性和一致性，添加新工具时必须遵循以下严格要求：

1.  **目录规范**: 根据工具类型和目标平台，将其放置在合适的子目录中 (`windows/`, `linux/`, `macos/` 或 `python/`)。
2.  **帮助支持**: 所有命令行工具**必须**支持 `-h` 和 `--help` 参数，显示用法说明。实现方式请参考上方的"命令行帮助支持"章节。
3.  **详细文档**:
    - 必须在下方“工具列表”中添加该工具的详细描述。
    - 必须明确列出所有环境要求（如 Python 版本、依赖库、特定环境变量或第三方二进制文件）。
    - 必须提供清晰的使用示例和命令行说明。
3.  **多平台考虑**: 如果工具逻辑是通用的，应优先使用 Python 等跨平台语言实现。如果是平台特定的，请确保在对应目录中提供清晰的文档。
4.  **错误处理**: 工具应包含基本的错误检查（例如：检查依赖是否存在），并提供友好的错误提示。
5.  **代码风格**: 新添加的脚本或代码必须符合项目的整体编码风格。
6.  **同步更新**: 更新此 README 文档的中文和英文版本（如果存在），确保信息同步。
7.  **验证**: 在提交前，请在干净的环境中验证工具的功能及其文档的准确性。
