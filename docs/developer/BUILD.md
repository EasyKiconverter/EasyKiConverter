# 本地编译指南

本文档介绍如何在本地编译 EasyKiConverter 项目。

发布说明样例请参阅 [RELEASE_NOTES_TEMPLATE.md](RELEASE_NOTES_TEMPLATE.md)。

## 环境要求

### 操作系统

- Windows 10/11（推荐）
- macOS
- Linux

### Qt 版本

- Qt 6.6 或更高版本
- 推荐 Qt 6.10.2（CI 使用版本）

### CMake 版本

- CMake 3.16 或更高版本

### 编译器

**Windows**
- MinGW 13.10（推荐）
- MSVC 2019+

**macOS**
- Clang（Xcode 12+）

**Linux**
- GCC 9+
- Clang 10+

### Qt 模块

需要安装以下 Qt 模块：

- Qt Quick
- Qt Network
- Qt Core
- Qt Gui
- Qt Widgets
- Qt Quick Controls 2

### 第三方库

- zlib（用于 GZIP 解压缩）

## 安装依赖

### 项目本地开发环境（Linux）

本项目不使用系统 Qt，也不向系统 Python 安装项目依赖。Python 工具统一使用仓库根目录的 `.venv`，Qt 使用独立安装的 6.10.2。

```bash
# 创建并启用项目虚拟环境
python3 -m venv .venv
source .venv/bin/activate

# 安装 Python 构建辅助工具
python -m pip install aqtinstall==3.2.1

# 使用项目专用 Qt（不要使用 /usr/lib 下的系统 Qt）
export Qt6_DIR=/home/dennis/software/QT/6.10.2/gcc_64/lib/cmake/Qt6
export CMAKE_PREFIX_PATH=/home/dennis/software/QT/6.10.2/gcc_64
export PATH=/home/dennis/software/QT/6.10.2/gcc_64/bin:$PATH
```

`format_code.py` 会自动从 `tools/config/build_config.json` 查找项目 Qt 的 `qmlformat`；上述 `PATH` 设置仍推荐用于直接调用 Qt 工具。

验证 Qt 版本：

```bash
qmake -query QT_VERSION       # 必须为 6.10.2
```

### Windows

#### 安装 Qt

1. 访问 [Qt 官网](https://www.qt.io/download)
2. 下载 Qt Online Installer
3. 运行安装程序
4. 选择 Qt 6.10.2 或更高版本
5. 选择 MinGW 13.10 编译器
6. 安装所需的 Qt 模块：
   - Qt Quick
   - Qt Network
   - Qt Core
   - Qt Gui
   - Qt Widgets
   - Qt Quick Controls 2

#### 安装 CMake

1. 访问 [CMake 官网](https://cmake.org/download/)
2. 下载 Windows 安装包
3. 运行安装程序
4. 将 CMake 添加到系统 PATH

#### 安装编译器

**MinGW**
1. Qt 安装程序会自动安装 MinGW
2. 或从 [MinGW-w64](https://www.mingw-w64.org/) 下载

**MSVC**
1. 安装 Visual Studio 2019 或更高版本
2. 在安装时选择 "使用 C++ 的桌面开发" 工作负载

Windows 发布构建同时支持以下架构：

- x86_64：Qt `win64_msvc2022_64`，vcpkg triplet `x64-windows`；发布文件名使用 `windows-x86_64`
- ARM64：Windows 11 on ARM、Qt `win64_msvc2022_arm64`，vcpkg triplet `arm64-windows`

ARM64 发布包由 GitHub Actions 的 `windows-11-arm` runner 构建，生成 `windows-arm64` 便携版、安装程序和 MSIX 包。x86_64 发布包使用 `windows-x86_64` 标识；MSIX 清单内部仍使用 Windows 要求的 `x64` 架构值。

### LoongArch64（龙架构）

LoongArch64 暂未提供可执行的 GitHub Actions 构建工作流或正式发布包。GitHub 官方 runner 不支持 LoongArch64，项目也尚未配置可用的第三方原生 CI 或 x86_64/ARM64 交叉编译工具链，因此暂不保留不可执行的 workflow 配置。

后续具备可运行的构建基础设施后，再增加独立的 Qt、工具链、测试和 `loongarch64` 发布包流程。

### macOS

#### 使用 Homebrew 安装

```bash
# 安装 Homebrew（如果尚未安装）
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# Qt 不通过 Homebrew 安装；请使用下方的项目专用 Qt 安装方式

# 安装 CMake
brew install cmake

# 安装 Xcode 命令行工具
xcode-select --install
```

#### 从 Qt 官网安装

1. 访问 [Qt 官网](https://www.qt.io/download)
2. 下载 macOS 安装包
3. 运行安装程序
4. 将 Qt 6.10.2 或更高版本安装到项目专用目录，不要使用 `/usr/local/Qt`、Homebrew 或系统自带 Qt
5. 设置 `Qt6_DIR`、`CMAKE_PREFIX_PATH` 和 `PATH` 指向该目录

### Linux

**Ubuntu/Debian**

```bash
# 更新包管理器
sudo apt-get update

# 安装 CMake
sudo apt-get install cmake

# 安装编译器
sudo apt-get install build-essential g++

# 安装 zlib
sudo apt-get install zlib1g-dev

# 安装 XKB 支持和其他 Qt 运行时依赖
sudo apt-get install libxkbcommon-dev libxkbcommon-x11-dev

# Qt 请按照上面的“项目本地开发环境”安装到独立目录，禁止使用系统 Qt
```

**Fedora**

```bash
# 安装 CMake
sudo dnf install cmake ninja-build

# 安装编译器
sudo dnf install gcc-c++

# 安装 zlib
sudo dnf install zlib-devel
```

**Arch Linux**

```bash
# 安装编译依赖；Qt 必须使用上文配置的项目专用安装，不要使用系统 Qt
sudo pacman -S --needed base-devel cmake ninja zlib
```

Fedora 和 Arch Linux 的系统包管理器仅用于安装编译器、CMake、Ninja、zlib 等基础依赖。Qt 必须通过独立安装目录提供，并在配置前确认 `qmake -query QT_VERSION` 输出项目要求的版本。

## 获取源代码

### Clone 仓库

```bash
git clone https://github.com/EasyKiconverter/EasyKiConverter.git
cd EasyKiConverter
```

### Fork 并 Clone（如果您计划贡献）

1. 在 GitHub 上 Fork 本项目
2. Clone 您的 Fork：

```bash
git clone https://github.com/your-username/EasyKiConverter.git
cd EasyKiConverter
```

## 编译项目

### 使用 Python 构建工具（推荐）

项目提供了统一的 Python 构建管理工具，支持自动化构建、环境检查和日志管理。

#### Windows

```bash
# 使用批处理包装器（推荐）
tools\windows\build_project.bat                # 默认 Debug 构建
tools\windows\build_project.bat -t Release     # Release 构建
tools\windows\build_project.bat -c -y          # 强制清理并构建
tools\windows\build_project.bat --check        # 仅检查依赖环境
tools\windows\build_project.bat --config-only  # 仅执行 CMake 配置
tools\windows\build_project.bat -t Release -i  # 构建并安装
tools\windows\build_project.bat -v             # 详细日志模式
```

#### 跨平台（Python）

```bash
# 直接调用 Python 构建脚本
.venv/bin/python tools/python/build_project.py -t Release --parallel
.venv/bin/python tools/python/build_project.py --check
.venv/bin/python tools/python/build_project.py -c -y
```

**Python 构建脚本参数**:

| 参数 | 说明 |
|------|------|
| `-t, --type` | 构建类型: Debug/Release/RelWithDebInfo/MinSizeRel |
| `-c, --clean` | 清理构建目录 |
| `-y, --yes` | 自动确认（与 -c 配合使用）|
| `-i, --install` | 构建后执行安装 |
| `-v, --verbose` | 详细日志输出 |
| `--check` | 仅检查环境依赖（CMake/编译器/Qt6）|
| `--config-only` | 仅执行 CMake 配置 |
| `-j, --jobs N` | 并行编译线程数（默认 CPU 核心数）|
| `--test` | 构建后运行测试 (ctest) |
| `--env-check` | 完整环境检查（包含 lupdate、clang-format、qmlformat）|

### 手动构建（CMake）

如果您需要手动控制构建过程，可以使用以下方法：

#### Windows + MinGW

```bash
# 创建构建目录
mkdir build
cd build

# 配置项目
cmake .. -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH="C:/Qt/6.10.2/mingw_64"

# 编译项目（Debug 版本）
cmake --build . --config Debug

# 编译项目（Release 版本）
cmake --build . --config Release

# 运行应用程序
./bin/EasyKiConverter.exe
```

### Windows + MSVC

```bash
# 创建构建目录
mkdir build
cd build

# 配置项目
cmake .. -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH="C:/Qt/6.10.2/msvc2019_64"

# 编译项目（Debug 版本）
cmake --build . --config Debug

# 编译项目（Release 版本）
cmake --build . --config Release

# 运行应用程序
./bin/Debug/EasyKiConverter.exe
```

### macOS

```bash
# 设置项目专用 Qt 根目录（替换为实际安装路径）
export PROJECT_QT_ROOT=/path/to/project/Qt/6.10.2/macos
export Qt6_DIR="$PROJECT_QT_ROOT/lib/cmake/Qt6"
export CMAKE_PREFIX_PATH="$PROJECT_QT_ROOT"
export PATH="$PROJECT_QT_ROOT/bin:$PATH"

# 创建构建目录
mkdir build
cd build

# 配置项目
cmake .. -DCMAKE_PREFIX_PATH="$PROJECT_QT_ROOT"

# 编译项目（Debug 版本）
cmake --build . --config Debug

# 编译项目（Release 版本）
cmake --build . --config Release

# 运行应用程序
./bin/EasyKiConverter
```

### Linux

```bash
# 创建构建目录
mkdir build
cd build

# 配置项目
cmake .. -DCMAKE_PREFIX_PATH="/opt/Qt/6.10.2/gcc_64"

# 编译项目（Debug 版本）
cmake --build . --config Debug

# 编译项目（Release 版本）
cmake --build . --config Release

# 运行应用程序
./bin/EasyKiConverter
```

## 编译项目（含测试）

### Windows + MinGW

```bash
# 创建构建目录
mkdir build
cd build

# 配置项目（启用测试构建）
cmake .. -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH="C:/Qt/6.10.2/mingw_64" -DEASYKICONVERTER_BUILD_TESTS=ON

# 编译项目（Debug 版本）
cmake --build . --config Debug

# 编译项目（Release 版本）
cmake --build . --config Release

# 运行应用程序
./bin/EasyKiConverter.exe

# 运行测试
ctest --output-on-failure
```

### Windows + MSVC

```bash
# 创建构建目录
mkdir build
cd build

# 配置项目（启用测试构建）
cmake .. -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH="C:/Qt/6.10.2/msvc2019_64" -DEASYKICONVERTER_BUILD_TESTS=ON

# 编译项目（Debug 版本）
cmake --build . --config Debug

# 编译项目（Release 版本）
cmake --build . --config Release

# 运行应用程序
./bin/Debug/EasyKiConverter.exe

# 运行测试
ctest --output-on-failure
```

### macOS/Linux

```bash
# 创建构建目录
mkdir build
cd build

# 配置项目（启用测试构建和覆盖率支持）
cmake .. -DCMAKE_PREFIX_PATH="/opt/Qt/6.10.2/gcc_64" -DEASYKICONVERTER_BUILD_TESTS=ON -DENABLE_COVERAGE=ON

# 编译项目（Debug 版本）
cmake --build . --config Debug

# 编译项目（Release 版本）
cmake --build . --config Release

# 运行应用程序
./bin/EasyKiConverter

# 运行测试
ctest --output-on-failure
```

### 测试选项说明

- `-DEASYKICONVERTER_BUILD_TESTS=ON`: 启用测试构建，编译测试代码
- `-DENABLE_COVERAGE=ON`: 启用代码覆盖率支持（仅 GCC/MinGW）
- `ctest --output-on-failure`: 运行所有测试，失败时显示详细输出

详见: [测试指南](TESTING_GUIDE.md)

## 使用 Qt Creator 编译

1. 安装 Qt Creator
2. 打开 Qt Creator
3. 选择 "文件" -> "打开文件或项目"
4. 选择项目根目录下的 `CMakeLists.txt`
5. 配置构建套件（Kit）：
   - 选择 Qt 版本（Qt 6.10.2 或更高）
   - 选择编译器（MinGW、Clang 或 GCC）
6. 点击 "配置项目"
7. 点击 "构建"按钮（或按 Ctrl+B）
8. 点击 "运行"按钮（或按 F5）启动应用程序

## 编译选项

### 启用调试导出功能

```bash
cmake .. -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH="C:/Qt/6.10.2/mingw_64" -DENABLE_SYMBOL_FOOTPRINT_DEBUG_EXPORT=ON
```

这将启用调试数据导出功能，用于调试转换过程。

### 多线程编译

**Windows (MSVC)**

MSVC 编译器会自动使用 /MP 选项进行多线程编译，无需额外参数。

**Windows (MinGW)**

```bash
# 使用 16 个并行任务编译
cmake --build . --config Debug -- -j 16

# 或使用系统最大核心数
cmake --build . --config Debug -- -j
```

**Linux/macOS**

```bash
# 使用 16 个并行任务编译
cmake --build . --config Debug -- -j 16

# 或使用系统最大核心数
cmake --build . --config Debug -- -j$(nproc)
```

**注意：**
- 并行编译可以显著加快编译速度，特别是在多核 CPU 上
- 建议根据 CPU 核心数调整并行任务数（通常设置为 CPU 核心数的 1-2 倍）
- 如果遇到内存不足问题，可以减少并行任务数

### 指定构建类型

```bash
# Debug 版本
cmake .. -DCMAKE_BUILD_TYPE=Debug

# Release 版本
cmake .. -DCMAKE_BUILD_TYPE=Release

# RelWithDebInfo 版本
cmake .. -DCMAKE_BUILD_TYPE=RelWithDebInfo
```

## 常见问题

### 找不到 Qt

**错误信息：**
```
Could not find Qt6
```

**解决方案：**
确保 CMAKE_PREFIX_PATH 指向正确的 Qt 安装路径。

```bash
cmake .. -DCMAKE_PREFIX_PATH="/path/to/Qt/6.10.2/gcc_64"
```

### 找不到 zlib

**错误信息：**
```
Could not find ZLIB
```

**解决方案：**

**Windows：**
Qt 通常自带 zlib，确保 Qt 安装完整。

**macOS：**
```bash
brew install zlib
```

**Linux：**
```bash
sudo apt-get install zlib1g-dev  # Ubuntu/Debian
sudo dnf install zlib-devel       # Fedora
sudo pacman -S zlib               # Arch Linux
```

### 找不到 XKB 或 GuiPrivate 相关错误

**错误信息：**
```
Could NOT find XKB (missing: XKB_LIBRARY XKB_INCLUDE_DIR)
CMake Error: The link interface of target "Qt6::GuiPrivate" contains XKB::XKB but the target was not found.
```

**解决方案：**

**Ubuntu/Debian：**
```bash
sudo apt-get install libxkbcommon-dev libxkbcommon-x11-dev
```

**Fedora：**
```bash
sudo dnf install libxkbcommon-devel libxkbcommon-x11-devel
```

**Arch Linux：**
```bash
sudo pacman -S libxkbcommon
```

`Qt6::GuiPrivate` 的头文件和 CMake 配置必须来自项目专用 Qt 安装，不要通过系统 Qt 私有开发包补齐版本不一致的依赖。

### 找不到 Qt 动态库

**错误信息：**
```
Failed to load platform plugin "windows"
```

**解决方案：**

**Windows：**
将 Qt 的 bin 目录添加到 PATH 环境变量。

```bash
set PATH=C:\Qt\6.10.2\mingw_64\bin;%PATH%
```

**Linux/macOS：**
```bash
export PATH="/path/to/Qt/6.10.2/gcc_64/bin:$PATH"
export LD_LIBRARY_PATH="/path/to/Qt/6.10.2/gcc_64/lib:$LD_LIBRARY_PATH"
```

### 编译错误

**错误信息：**
```
error: 'xxx' was not declared in this scope
```

**解决方案：**

1. 检查 Qt 版本是否正确
2. 检查编译器版本是否支持 C++17
3. 清理构建目录重新编译

```bash
cd build
cmake --build . --target clean
cmake ..
cmake --build .
```

### 链接错误

**错误信息：**
```
undefined reference to `xxx'
```

**解决方案：**

1. 检查 CMakeLists.txt 中的链接库配置
2. 确保所有必需的 Qt 模块都已链接
3. 检查编译器是否支持所有使用的 C++ 特性

## 清理构建

### 清理构建目录

```bash
cd build
cmake --build . --target clean
```

### 完全清理

```bash
# 删除构建目录
rm -rf build

# Windows
rmdir /s /q build
```

## 安装

### 安装到指定目录

```bash
cmake --install . --prefix /path/to/install
```

### 系统范围安装（Linux）

```bash
sudo cmake --install . --prefix /usr/local
```

## 打包

### Windows

**生成便携版 (Portable Zip):**
使用 windeployqt 工具打包依赖项：

```bash
cd build/bin/Release
windeployqt --release --no-translations EasyKiConverter.exe
# 然后压缩该目录
```

**生成安装包 (Installer .exe):**
需要安装 [Inno Setup 6](https://jrsoftware.org/isinfo.php)。

1. 确保已运行 `windeployqt` 填充了依赖。
2. 将 `build/bin/Release` 内容复制到临时目录 `build/Staging`。
3. 运行编译器：
   ```bash
   iscc "/DMyAppVersion=3.0.0" "/DSourceDir=path\to\staging" "resources\windows_setup.iss"
   ```

### macOS

使用 macdeployqt 工具打包依赖项：

```bash
cd build/bin
macdeployqt EasyKiConverter.app
```

### Linux

创建 AppImage 或使用系统包管理器安装依赖项。

## 性能优化

### 启用编译器优化

```bash
cmake .. -DCMAKE_BUILD_TYPE=Release
```

### 使用链接时优化（LTO）

```bash
cmake .. -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON
```

### 使用静态链接

```bash
cmake .. -DBUILD_SHARED_LIBS=OFF
```

## 调试

### 启用调试符号

```bash
cmake .. -DCMAKE_BUILD_TYPE=Debug
```

### 使用 GDB 调试

```bash
cd build/bin
gdb ./EasyKiConverter
```

### 使用 Qt Creator 调试

1. 在 Qt Creator 中打开项目
2. 设置断点
3. 点击 "调试"按钮（或按 F5）

## 在远程服务器上运行 Qt 应用

EasyKiConverter 是一个 Qt Quick 应用，需要图形界面支持。在远程服务器上运行时，需要配置 X11 转发或使用 VNC。

### 方案 1: X11 转发（推荐用于简单测试）

**步骤：**

1. **在本地 Windows 上安装 X Server：**
   - **VcXsrv** (推荐): https://sourceforge.net/projects/vcxsrv/
   - **MobaXterm** (自带 X Server): https://mobaxterm.mobatek.net/

2. **配置 SSH 连接：**
   - 使用 PuTTY：在 `Connection → SSH → X11` 中启用 `Enable X11 forwarding`
   - 使用 MobaXterm：X11 转发已自动启用
   - 使用命令行 SSH：添加 `-X` 参数
     ```bash
     ssh -X user@server-ip
     ```

3. **运行应用：**
   ```bash
   export DISPLAY=localhost:10.0  # 通常由 SSH 自动设置
   cd build
   ./bin/EasyKiConverter
   ```

### 方案 2: VNC（更稳定）

**步骤：**

1. **在服务器上安装 VNC 服务器：**
   ```bash
   sudo apt-get install tightvncserver
   ```

2. **启动 VNC 服务器：**
   ```bash
   vncserver :1
   # 第一次运行时会提示设置密码
   ```

3. **在本地使用 VNC 客户端连接：**
   - 下载 RealVNC Viewer 或 TigerVNC
   - 连接地址: `your-server-ip:5901`

4. **运行应用：**
   ```bash
   cd build
   ./bin/EasyKiConverter
   ```

5. **停止 VNC 服务器（可选）：**
   ```bash
   vncserver -kill :1
   ```

### 方案 3: 使用虚拟显示（用于测试/CI）

**步骤：**

1. **安装 Xvfb（虚拟帧缓冲）：**
   ```bash
   sudo apt-get install xvfb
   ```

2. **在虚拟显示中运行应用：**
   ```bash
   xvfb-run ./bin/EasyKiConverter
   ```

**注意：** 此方案主要用于自动化测试，应用窗口不可见。

### 推荐做法

对于开发测试，推荐使用 **MobaXterm**，因为它：
- 自带 X Server，无需额外配置
- 支持 SSH、SFTP 等多种协议
- 界面友好，开箱即用

## 获取帮助

如果遇到编译问题：

1. 查阅本文档的常见问题部分
2. 搜索 [GitHub Issues](https://github.com/EasyKiconverter/EasyKiConverter/issues)
3. 提交新的 Issue，提供以下信息：
   - 操作系统版本
   - Qt 版本
   - CMake 版本
   - 编译器版本
   - 完整的错误信息
   - CMake 配置命令

## 版本管理

项目提供了 `tools/python/manage_version.py` 工具来同步更新版本信息，确保各处版本号一致。

### 使用方法

```bash
# 检查当前版本
python tools/python/manage_version.py --check

# 更新版本到 3.1.0（自动更新以下文件）
python tools/python/manage_version.py 3.1.0
```

### 同步的文件

- `vcpkg.json`: 更新 `"version-string"` 字段
- `CMakeLists.txt`: 更新默认的 `VERSION_FROM_CI` 和 `qt_add_qml_module` 版本
- `src/main.cpp`: 更新 `app.setApplicationVersion`

### 环境要求

- Python 3.6+
- 建议在项目根目录运行

详见: [GitHub 工具目录](https://github.com/EasyKiconverter/EasyKiConverter/tree/main/tools)

## 相关资源

- [Qt 官网](https://www.qt.io/)
- [CMake 官网](https://cmake.org/)
- [MinGW-w64](https://www.mingw-w64.org/)
- [项目主页](https://github.com/EasyKiconverter/EasyKiConverter)

## CI/CD 架构

本项目使用 GitHub Actions 实现全自动化 CI/CD。

*   **依赖管理**: 使用 `vcpkg.json` (Manifest Mode) 统一管理 CI 环境依赖。
*   **本地 Actions**:
    *   `setup-env`: 统一配置 CMake, Qt, ccache/sccache。
    *   `get-version`: 统一从 Git Tag 提取版本号。
*   **缓存策略**: 启用了 Qt 安装缓存、vcpkg 二进制缓存以及 CMake 编译器缓存 (ccache/sccache)。
*   **安全性**: 所有工作流均配置了最小权限和并发控制。

详细依赖关系请参考 `.github/workflows/WORKFLOW_DEPENDENCIES.md`。
