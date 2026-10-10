# 本地打包指南

本指南帮助您在本地测试 DEB 和 AppImage 打包流程，避免使用过多的 GitHub Actions 使用量。

## 前提条件

### 方式 1: 使用 Docker（推荐）

使用 Docker 可以避免在本地安装复杂的依赖，并且与 CI 环境完全一致。

```bash
# 使用项目构建环境容器
docker run --rm -it \
  -v $(pwd):/workspace \
  ghcr.io/tangsangsimida/easykiconverter/build-env:latest \
  bash
```

在容器内运行：
```bash
cd /workspace
./tools/linux/build-packages.sh all
```

### 方式 2: 本地安装依赖（自动安装）

脚本会自动检测并安装缺失的工具（需要 sudo 权限）。只需运行：

```bash
./tools/linux/build-packages.sh all
```

脚本会自动：
1. 检测缺失的工具（cmake、linuxdeploy、linuxdeploy-plugin-qt、nfpm）
2. 提示自动安装
3. 使用与 CI 环境一致的方法安装（从 AppImage 提取、从官方源安装）

### 方式 3: 本地手动安装依赖

如果您想手动安装，请按照以下步骤操作。这些方法与 CI 环境（`ghcr.io/tangsangsimida/easykiconverter/build-env:latest`）保持一致。

#### 1. 安装系统依赖

```bash
# Ubuntu/Debian
sudo apt install -y cmake ninja-build

# 或使用系统已有的 Qt
# 确保 Qt 6.6+ 已安装并设置 CMAKE_PREFIX_PATH
```

#### 2. 安装 linuxdeploy（从 AppImage 提取）

使用与 CI 一致的方法：从 AppImage 提取并安装到系统目录。

```bash
# 创建安装目录
sudo mkdir -p /opt/linuxdeploy

# 下载 AppImage（x86_64 主机用 x86_64，arm64/aarch64 主机用 aarch64）
ARCH=$(uname -m)
case "$ARCH" in
  x86_64|amd64) LINUXDEPLOY_ARCH=x86_64 ;;
  aarch64|arm64) LINUXDEPLOY_ARCH=aarch64 ;;
  *) echo "Unsupported arch: $ARCH" >&2; exit 1 ;;
esac
wget https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-${LINUXDEPLOY_ARCH}.AppImage
chmod +x linuxdeploy-${LINUXDEPLOY_ARCH}.AppImage

# 提取 AppImage 内容
./linuxdeploy-${LINUXDEPLOY_ARCH}.AppImage --appimage-extract

# 复制到系统目录并创建符号链接
sudo cp -r squashfs-root/* /opt/linuxdeploy/
sudo ln -s /opt/linuxdeploy/AppRun /usr/local/bin/linuxdeploy

# 清理临时文件
rm -rf squashfs-root linuxdeploy-${LINUXDEPLOY_ARCH}.AppImage

# 验证安装
linuxdeploy --version
```

#### 3. 安装 linuxdeploy-plugin-qt（从 AppImage 提取）

同样使用从 AppImage 提取的方法。

```bash
# 创建安装目录
sudo mkdir -p /opt/linuxdeploy-plugin-qt

# 下载 AppImage（复用上一步的 LINUXDEPLOY_ARCH）
wget https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-${LINUXDEPLOY_ARCH}.AppImage
chmod +x linuxdeploy-plugin-qt-${LINUXDEPLOY_ARCH}.AppImage

# 提取 AppImage 内容
./linuxdeploy-plugin-qt-${LINUXDEPLOY_ARCH}.AppImage --appimage-extract

# 复制到系统目录并创建符号链接
sudo cp -r squashfs-root/* /opt/linuxdeploy-plugin-qt/
sudo ln -s /opt/linuxdeploy-plugin-qt/AppRun /usr/local/bin/linuxdeploy-plugin-qt

# 清理临时文件
rm -rf squashfs-root linuxdeploy-plugin-qt-${LINUXDEPLOY_ARCH}.AppImage

# 验证安装
linuxdeploy-plugin-qt --version
```

#### 4. 安装 nfpm（从 goreleaser 官方源）

从 goreleaser 官方 APT 源安装，这样可以获得最新版本。

```bash
# 添加 goreleaser 官方源
echo 'deb [trusted=yes] https://repo.goreleaser.com/apt/ /' | sudo tee /etc/apt/sources.list.d/goreleaser.list

# 更新包列表
sudo apt-get update

# 安装 nfpm
sudo apt-get install -y nfpm

# 验证安装
nfpm --version
```

## 使用方法

### 基本用法

```bash
# 构建所有包（AppImage + DEB）
./tools/linux/build-packages.sh all

# 仅构建 AppImage
./tools/linux/build-packages.sh appimage

# 仅构建 DEB 包
./tools/linux/build-packages.sh deb
```

### 设置版本

```bash
# 使用当前 git tag
./tools/linux/build-packages.sh all

# 指定版本
VERSION=3.0.8 ./tools/linux/build-packages.sh all
```

## 输出位置

打包完成后，文件位于 `build/packages/` 目录：

```
build/packages/
├── EasyKiConverter-3.0.7-linux-x86_64.AppImage
├── EasyKiConverter-3.0.7-linux-x86_64.AppImage.sha256sum
├── EasyKiConverter-3.0.7-linux-amd64.deb
├── EasyKiConverter-3.0.7-linux-amd64.deb.sha256sum
├── EasyKiConverter-3.0.7-linux-aarch64.AppImage
├── EasyKiConverter-3.0.7-linux-aarch64.AppImage.sha256sum
├── EasyKiConverter-3.0.7-linux-arm64.deb
└── EasyKiConverter-3.0.7-linux-arm64.deb.sha256sum
```

## 测试打包结果

### 测试 AppImage

```bash
# 赋予执行权限
chmod +x build/packages/EasyKiConverter-*.AppImage

# 运行
./build/packages/EasyKiConverter-*.AppImage
```

### 测试 DEB 包

```bash
# 安装
sudo dpkg -i build/packages/EasyKiConverter-*.deb

# 如果有依赖问题
sudo apt-get install -f

# 运行
EasyKiConverter

# 卸载
sudo dpkg -r easykiconverter
```

## 常见问题

### 1. 缺少 Qt 库

如果运行 AppImage 时提示缺少 Qt 库：

```bash
# 检查 linuxdeploy 是否正确包含 Qt 库
./build/packages/EasyKiConverter-*.AppImage --appimage-extract
ls squashfs-root/usr/lib/
```

### 2. 桌面集成问题

如果任务栏图标不显示，检查：

```bash
# 查看桌面文件
cat squashfs-root/io.github.tangsangsimida.easykiconverter.desktop

# 查看图标文件
ls squashfs-root/usr/share/icons/hicolor/*/apps/
```

### 3. DEB 包依赖问题

```bash
# 查看 DEB 包依赖
dpkg -I build/packages/EasyKiConverter-*.deb | grep Depends
```

### 4. 工具安装失败

如果自动安装失败，可以尝试手动安装：

```bash
# 检查网络连接
ping -c 3 github.com

# 手动安装工具
cd /tmp
ARCH=$(uname -m)
case "$ARCH" in
  x86_64|amd64) LINUXDEPLOY_ARCH=x86_64 ;;
  aarch64|arm64) LINUXDEPLOY_ARCH=aarch64 ;;
  *) echo "Unsupported arch: $ARCH" >&2; exit 1 ;;
esac
wget https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-${LINUXDEPLOY_ARCH}.AppImage
chmod +x linuxdeploy-${LINUXDEPLOY_ARCH}.AppImage
./linuxdeploy-${LINUXDEPLOY_ARCH}.AppImage --appimage-extract
sudo cp -r squashfs-root/* /opt/linuxdeploy/
sudo ln -s /opt/linuxdeploy/AppRun /usr/local/bin/linuxdeploy
```

### 5. AppImage 提取失败

如果 AppImage 提取失败，可能是因为 FUSE 权限问题：

```bash
# 检查 FUSE 是否可用
lsmod | grep fuse

# 如果没有 FUSE，使用 --appimage-extract-and-run 模式
# 或者安装 fuse
sudo apt-get install -y fuse libfuse2
```

### 6. linuxdeploy 命令找不到

如果安装后仍然找不到命令：

```bash
# 检查符号链接
ls -l /usr/local/bin/linuxdeploy

# 检查文件权限
ls -l /opt/linuxdeploy/AppRun

# 如果权限有问题，修复权限
sudo chmod +x /opt/linuxdeploy/AppRun

# 重新创建符号链接
sudo ln -sf /opt/linuxdeploy/AppRun /usr/local/bin/linuxdeploy

# 更新 PATH（如果需要）
export PATH="/usr/local/bin:$PATH"
```

### 7. nfpm 安装失败

如果从 goreleaser 官方源安装 nfpm 失败：

```bash
# 清理旧的源文件
sudo rm /etc/apt/sources.list.d/goreleaser.list

# 重新添加源
echo 'deb [trusted=yes] https://repo.goreleaser.com/apt/ /' | sudo tee /etc/apt/sources.list.d/goreleaser.list

# 更新并安装
sudo apt-get update
sudo apt-get install -y nfpm

# 如果仍然失败，使用手动下载的方式
wget https://github.com/goreleaser/nfpm/releases/download/v2.36.0/nfpm_2.36.0_linux_amd64.tar.gz
tar xzf nfpm_2.36.0_linux_amd64.tar.gz
sudo mv nfpm /usr/local/bin/
```

### 8. 版本不匹配

如果本地工具版本与 CI 不一致导致问题：

```bash
# 检查工具版本
linuxdeploy --version
linuxdeploy-plugin-qt --version
nfpm --version

# 如果需要重新安装，先删除旧版本
sudo rm -rf /opt/linuxdeploy /opt/linuxdeploy-plugin-qt
sudo rm /usr/local/bin/linuxdeploy /usr/local/bin/linuxdeploy-plugin-qt

# 然后重新安装
# 按照上面的步骤重新安装
```

## 工作流程对比

### GitHub Actions 流程

```
1. 触发 workflow
2. 克隆代码
3. 构建项目
4. 创建 AppDir
5. 并行打包 AppImage 和 DEB
6. 上传到 Release
7. 耗时：约 10-15 分钟
8. 费用：消耗 GitHub Actions 使用量
```

### 本地打包流程

```
1. 运行打包脚本
2. 构建项目
3. 创建 AppDir
4. 打包 AppImage 和 DEB
5. 本地测试
6. 确认无误后提交代码
7. 耗时：约 3-5 分钟
8. 费用：0
```

## 建议

1. **开发阶段**：使用本地打包快速验证
2. **测试阶段**：在本地打包并测试，确认无误后再推送
3. **发布阶段**：使用 GitHub Actions 自动构建和发布
4. **持续集成**：仅在 PR 或 release 时触发 GitHub Actions

这样可以大大减少 GitHub Actions 的使用量，同时保持代码质量。
