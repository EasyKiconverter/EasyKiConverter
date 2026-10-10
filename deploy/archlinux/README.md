# Arch Linux 打包

本目录包含用于在 Arch Linux 上构建 EasyKiConverter 的 PKGBUILD 文件。

## 从源码构建

### 前置条件

安装所需的构建依赖：

```bash
sudo pacman -S --needed base-devel cmake ninja qt6-base qt6-declarative qt6-quickcontrols2 qt6-svg qt6-shadertools qt6-tools zlib
```

### 构建并安装

```bash
# 克隆 AUR 仓库或直接使用此 PKGBUILD
git clone https://aur.archlinux.org/easykiconverter.git
cd easykiconverter

# 构建软件包
makepkg -si

# 或仅构建不安装
makepkg -s
```

### 使用本仓库的 PKGBUILD

```bash
cd deploy/archlinux

# 构建软件包
makepkg -si

# 或仅构建不安装
makepkg -s
```

## AUR 提交

要将此软件包提交到 AUR：

1. 在 https://aur.archlinux.org/ 创建 AUR 账户
2. 设置 AUR 的 SSH 密钥
3. 克隆 AUR 仓库：
   ```bash
   git clone ssh://aur@aur.archlinux.org/easykiconverter.git
   cd easykiconverter
   ```
4. 复制 PKGBUILD 和 .SRCINFO 文件
5. 更新 sha256sums（运行 `updpkgsums` 或开发时使用 `SKIP`）
6. 提交并推送：
   ```bash
   git add PKGBUILD .SRCINFO
   git commit -m "Update to version 3.1.12"
   git push
   ```

## 依赖项

### 运行时依赖

- `qt6-base` - Qt 6 基础模块
- `qt6-declarative` - Qt Quick/QML 引擎
- `qt6-quickcontrols2` - Qt Quick Controls 2
- `qt6-svg` - SVG 支持
- `qt6-shadertools` - Qt Quick 着色器工具
- `zlib` - 压缩库

### 可选依赖

- `qt6-5compat` - 用于某些旧版 QML 组件

### 构建依赖

- `cmake` - 构建系统
- `gcc` - C++ 编译器
- `git` - 版本控制
- `ninja` - 构建工具（可选，可使用 make）
- `qt6-tools` - Qt 6 开发工具

## 故障排除

### Qt 版本问题

如果遇到 Qt 版本冲突，请确保使用系统 Qt 6 包：

```bash
# 检查已安装的 Qt 版本
qmake6 --version

# 列出已安装的 Qt 包
pacman -Qs qt6
```

### 构建失败

如果构建失败，请尝试：

1. 清理构建目录：
   ```bash
   rm -rf src/
   ```

2. 确保所有依赖已安装：
   ```bash
   sudo pacman -S --needed base-devel cmake ninja qt6-base qt6-declarative qt6-quickcontrols2 qt6-svg qt6-shadertools qt6-tools zlib
   ```

3. 检查构建日志中的具体错误

## 链接

- [AUR 软件包页面](https://aur.archlinux.org/packages/easykiconverter)
- [GitHub 仓库](https://github.com/EasyKiconverter/EasyKiConverter)
- [Arch Linux 打包标准](https://wiki.archlinux.org/title/Arch_package_guidelines)
