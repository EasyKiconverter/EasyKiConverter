# EasyKiConverter 发布说明样例

本文档展示 GitHub Release 页面使用的发布说明样式。正式发布时，发布工作流会根据版本、构建制品和 Conventional Commits 自动生成对应内容。

发布说明模板位于：`.github/workflows/release.yml`。

---

# EasyKiConverter `<version>`

EasyKiConverter `<version>` 改善了 EasyEDA 到 KiCad/Altium 的库转换质量，并完善了跨平台发布流程。

## 下载

| 平台 | 架构 | 格式 | 文件 |
| --- | --- | --- | --- |
| Windows | x86_64 | MSIX | `EasyKiConverter-<version>-windows-x86_64.msix` |
| Windows | x86_64 | 安装程序 | `EasyKiConverter-<version>-windows-x86_64-installer.exe` |
| Windows | x86_64 | 便携版 | `EasyKiConverter-<version>-windows-x86_64-portable.zip` |
| Windows | arm64 | MSIX | `EasyKiConverter-<version>-windows-arm64.msix` |
| Windows | arm64 | 安装程序 | `EasyKiConverter-<version>-windows-arm64-installer.exe` |
| Windows | arm64 | 便携版 | `EasyKiConverter-<version>-windows-arm64-portable.zip` |
| Linux | x86_64 | AppImage | `EasyKiConverter-<version>-linux-x86_64.AppImage` |
| Linux | arm64 | AppImage | `EasyKiConverter-<version>-linux-aarch64.AppImage` |
| Linux | amd64 | DEB/RPM/Arch | 对应架构安装包 |
| Linux | arm64 | DEB/RPM/Arch | 对应架构安装包 |
| macOS | intel | DMG | `EasyKiConverter-<version>-macos-intel.dmg` |
| macOS | arm64 | DMG | `EasyKiConverter-<version>-macos-arm64.dmg` |

> 每个发布制品都提供对应的 `.sha256sum` 文件，Release 还提供 `SHA256SUMS` 汇总清单。下载后建议先验证文件完整性，再执行安装或运行。
>
> Windows 文件名使用 `x86_64` 和 `arm64` 标识；`x86_64` 对应 Intel/AMD 64 位设备，`arm64` 对应 Windows on Arm。

## 重要变化

- 改进符号原点归一化、Pin Name/Pin Number 原始坐标及网格对齐。
- 改进 EasyEDA SVG 圆弧、曲线和连续参数解析。
- 修复 KiCad 封装弧线格式兼容性问题。

## 变更分类

### 新功能

- `feat(...)` 变更摘要（提交链接）

### 问题修复

- `fix(...)` 变更摘要（提交链接）

### 重构与性能

- `refactor(...)` 变更摘要（提交链接）

### 测试

- `test(...)` 变更摘要（提交链接）

### 构建与 CI

- `ci(...)` / `build(...)` 变更摘要（提交链接）

### 文档

- `docs(...)` 变更摘要（提交链接）

## 转换格式

- KiCad 符号库和封装库
- Altium SchLib 和 PcbLib
- STEP 三维模型

## 构建与验证

- Linux amd64 构建任务：`success`
- Linux ARM64 构建任务：`success`
- Windows x64 构建任务：`success`
- Windows ARM64 构建任务：`success`
- macOS Intel 构建任务：`success`
- macOS ARM64 构建任务：`success`
- 发布前已验证全部制品及 SHA256 校验和。

## 校验和

请下载与目标制品同名的 `.sha256sum` 文件，在对应平台执行 SHA256 校验。

Linux/macOS 示例：

```bash
sha256sum -c EasyKiConverter-<version>-linux-x86_64.AppImage.sha256sum
```

Windows PowerShell 示例：

```powershell
Get-FileHash .\EasyKiConverter-<version>-windows-x86_64-installer.exe -Algorithm SHA256
```

## 已知问题

- 部分 EasyEDA 非标准 SVG 路径仍可能需要人工复核。
- Altium 对极少数特殊引脚装饰的表达能力有限。
- LoongArch64 当前未纳入正式发布包，待具备可用的交叉编译或第三方 CI 基础设施后支持。

## 贡献者

- `<contributor>`

构建提交：`<full commit SHA>`
