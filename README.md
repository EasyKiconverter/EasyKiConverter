
<h1 align="center">EasyKiConverter</h1>
<p align="center">
  <a href="README_en.md">English</a> | 中文
</p>


<p align="center">
    <a href="https://flathub.org/apps/details/io.github.tangsangsimida.easykiconverter"><img width="200" alt="Download on Flathub" src="https://flathub.org/assets/badges/flathub-badge-i-en.svg"/></a>
    <br/>
    <img src="https://github.com/EasyKiconverter/EasyKiConverter/actions/workflows/build.yml/badge.svg" alt="Build Status" />
    <img src="https://github.com/EasyKiconverter/EasyKiConverter/actions/workflows/security.yml/badge.svg" alt="Security Scan" />
    <img src="https://img.shields.io/github/v/release/EasyKiconverter/EasyKiConverter" alt="GitHub release" />
    <img src="https://img.shields.io/github/downloads/EasyKiconverter/EasyKiConverter/total" alt="GitHub downloads (total)" />
    <img src="https://img.shields.io/github/license/EasyKiconverter/EasyKiConverter" alt="License" />
    <img src="https://img.shields.io/github/stars/EasyKiconverter/EasyKiConverter" alt="Stars" />
    <img src="https://img.shields.io/github/issues/EasyKiconverter/EasyKiConverter" alt="Issues" />
</p>



**EasyKiConverter** 是一个基于 Qt 6 和 MVVM 架构的现代化 C++ 桌面工具，专为电子工程师设计，旨在将嘉立创 (LCSC) 和 EasyEDA 的元件数据转换为多种 EDA 的符号库、封装库和独立 3D 模型。支持 GUI 和 CLI 两种运行模式。

### 版本与能力边界

- 当前最新稳定版本为 **v3.1.11**，稳定版的可下载文件和功能以 [v3.1.11 Release](https://github.com/EasyKiconverter/EasyKiConverter/releases/tag/v3.1.11) 为准。
- **v3.1.12** 当前仍是预发布版本；它包含跨平台打包和 Xpedition 等后续改进，不代表稳定发布线。
- 当前 `master` 是开发分支，README 中列出的 Altium、Xpedition 和统一 IR 能力以源码分支为准，使用稳定版时请先核对对应 Release 说明。

## 主要特性

*   **多目标转换**：支持 KiCad、Altium、Xpedition、PADS、Eagle、P-CAD 和 CADSTAR 的已实现库导出能力；各目标的符号、封装、器件关联和 3D 模型范围以对应文档为准，不能表达的数据会报告诊断。
*   **分体式符号**：支持分体式符号转换
*   **高效批量处理**：支持多线程并行转换与 BOM 文件导入，充分利用多核性能。
*   **现代化体验**：基于 Qt Quick 的流畅 UI，支持深色/浅色主题切换。
*   **智能配置**：配置项实时自动保存，支持断点记忆与调试模式状态恢复。
*   **智能辅助**：支持从剪贴板智能提取元件编号。
*   **LCSC 预览图**：自动获取 LCSC 元件预览图，支持缩略图显示和悬停预览。
*   **CLI 模式**：支持纯命令行模式，适用于批量处理和自动化脚本。

## 快速开始

### 安装
请前往 [Releases](https://github.com/EasyKiconverter/EasyKiConverter/releases) 页面下载适用于您平台的版本：

*   **Windows**: 提供 x86_64 和 arm64 版本；Intel/AMD 64 位设备请选择文件名中的 `windows-x86_64`，Windows on Arm 请选择 `windows-arm64`。两种架构均提供 `.exe` 安装程序、`.zip` 便携版和 `.msix` 包。
*   **Linux**: 推荐下载 x86_64/ARM64 `.AppImage`；LoongArch64 构建目前需要单独配置构建基础设施，暂未纳入正式发布包。
*   **macOS**: 下载 `.dmg` 镜像文件。
*   **Arch Linux**: `yay -S easykiconverter`

### 从源码构建
如果您是开发者或希望自行编译，请参考 [构建指南](docs/developer/BUILD.md)。

## 文档中心

**用户指南**
*   [快速入门](docs/user/GETTING_STARTED.md) | [用户手册](docs/user/USER_GUIDE.md) | [常见问题 (FAQ)](docs/user/FAQ.md)
*   [详细功能特性](docs/user/FEATURES.md)

**开发者资源**

*   [贡献指南](docs/developer/CONTRIBUTING.md) | [架构设计](docs/developer/ARCHITECTURE.md) | [构建说明](docs/developer/BUILD.md)


## 贡献与致谢

### 贡献者

感谢以下开发者对 EasyKiConverter 的贡献：

<a href="https://github.com/EasyKiconverter/EasyKiConverter/graphs/contributors">
  <img src="https://contrib.rocks/image?repo=EasyKiconverter/EasyKiConverter&max=50" />
</a>

欢迎提交 Issue 或 Pull Request 参与改进！详细请阅 [贡献指南](docs/developer/CONTRIBUTING.md)。

## 许可证

本项目采用 **GPL-3.0** 许可证。详情请参阅 [LICENSE](LICENSE) 文件。
