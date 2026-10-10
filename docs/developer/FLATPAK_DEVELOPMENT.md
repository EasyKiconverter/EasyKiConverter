# Flatpak 开发指南

本文档提供在本地使用 Flatpak 编译、测试和配置文件管理的完整指南。

## 目录

- [环境准备](#环境准备)
- [Flatpak 编译](#flatpak-编译)
- [运行和测试](#运行和测试)
- [配置文件管理](#配置文件管理)
- [调试技巧](#调试技巧)
- [常见问题](#常见问题)

## 环境准备

### 安装 Flatpak Builder

```bash
# Ubuntu/Debian
sudo apt-get install flatpak-builder

# Fedora
sudo dnf install flatpak-builder

# Arch Linux
sudo pacman -S flatpak-builder
```

### 添加 Flatpak 远程仓库

```bash
# 添加 Flathub（如果还没有添加）
flatpak remote-add --if-not-exists flathub https://flathub.org/repo/flathub.flatpakrepo

# 添加 KDE Platform 6.10 运行时
flatpak remote-add --if-not-exists kdeapps https://distribute.kde.org/kdeapps.flatpakrepo
```

### 安装 KDE Platform 6.10 运行时和 SDK

```bash
# 安装运行时
flatpak install flathub org.kde.Platform//6.10

# 安装 SDK
flatpak install flathub org.kde.Sdk//6.10
```

## Flatpak 编译

### 方法 1：使用 Flatpak Builder

```bash
# 进入项目根目录
cd /path/to/EasyKiConverter

# 构建 Flatpak 包（使用本地测试版清单）
flatpak-builder --force-clean --repo=flatpak-repo build-flatpak ../../deploy/flatpak/io.github.tangsangsimida.easykiconverter.local.yml

# 安装构建的包
flatpak --user remote-add --no-gpg-verify local-repo flatpak-repo
flatpak --user install local-repo io.github.tangsangsimida.easykiconverter

# 运行
flatpak run io.github.tangsangsimida.easykiconverter
```

### 方法 2：使用项目脚本

```bash
# 检查 Flatpak 构建环境
python tools/python/flatpak_tool.py --check

# 构建 Flatpak
python tools/python/flatpak_tool.py --build

# 执行完整流程（检查+构建+安装+运行）
python tools/python/flatpak_tool.py --all --force

# 清理构建目录
python tools/python/flatpak_tool.py --clean

# 查看应用信息
python tools/python/flatpak_tool.py --info
```

### 清理构建缓存

```bash
# 清理 Flatpak 构建缓存
rm -rf flatpak_build/
```

## 运行和测试

### 运行已安装的 Flatpak 应用

```bash
# 基本运行
flatpak run io.github.tangsangsimida.easykiconverter

# 带调试信息运行
flatpak run --devel io.github.tangsangsimida.easykiconverter

# 启用调试模式
flatpak run io.github.tangsangsimida.easykiconverter --debug

# 设置语言
flatpak run io.github.tangsangsimida.easykiconverter --language en
flatpak run io.github.tangsangsimida.easykiconverter --language zh_CN
```

### 查看日志

```bash
# 查看应用日志
flatpak run --command=sh io.github.tangsangsimida.easykiconverter

# 在 Flatpak 环境中查看日志
journalctl --user -u flatpak-io.github.tangsangsimida.easykiconverter
```

### 卸载和重新安装

```bash
# 卸载应用
flatpak --user uninstall io.github.tangsangsimida.easykiconverter

# 移除本地仓库
flatpak --user remote-delete local-repo

# 清理 Flatpak 缓存
flatpak repair --user
```

## 配置文件管理

### 配置文件位置

**Flatpak 应用的配置文件位置与本地应用不同：**

```bash
# Flatpak 应用配置文件位置
~/.var/app/io.github.tangsangsimida.easykiconverter/config/EasyKiConverter/EasyKiConverter/config.json

# 本地应用配置文件位置
~/.config/EasyKiConverter/EasyKiConverter/config.json
```

### 查看配置文件

```bash
# 查看当前配置
cat ~/.var/app/io.github.tangsangsimida.easykiconverter/config/EasyKiConverter/EasyKiConverter/config.json | python3 -m json.tool

# 或者使用 jq（如果已安装）
cat ~/.var/app/io.github.tangsangsimida.easykiconverter/config/EasyKiConverter/EasyKiConverter/config.json | jq '.'
```

### 修改配置文件

```bash
# 编辑配置文件
nano ~/.var/app/io.github.tangsangsimida.easykiconverter/config/EasyKiConverter/EasyKiConverter/config.json

# 或者使用 vim
vim ~/.var/app/io.github.tangsangsimida.easykiconverter/config/EasyKiConverter/EasyKiConverter/config.json
```

### 常用配置示例

```json
{
  "outputPath": "/home/username/Desktop",
  "libName": "easyeda_convertlib",
  "exportSymbol": true,
  "exportFootprint": true,
  "exportModel3D": true,
  "exportPreviewImages": false,
  "exportDatasheet": false,
  "overwriteExistingFiles": false,
  "darkMode": false,
  "language": "en",
  "windowWidth": 1664,
  "windowHeight": 936,
  "windowX": 0,
  "windowY": 0,
  "exitPreference": ""
}
```

### 重置配置文件

```bash
# 备份当前配置
cp ~/.var/app/io.github.tangsangsimida.easykiconverter/config/EasyKiConverter/EasyKiConverter/config.json ~/backup_config.json

# 删除配置文件（应用将使用默认值）
rm ~/.var/app/io.github.tangsangsimida.easykiconverter/config/EasyKiConverter/EasyKiConverter/config.json

# 或者重置为默认值
cat > ~/.var/app/io.github.tangsangsimida.easykiconverter/config/EasyKiConverter/EasyKiConverter/config.json << EOF
{
  "outputPath": "/home/$(whoami)/Desktop",
  "libName": "easyeda_convertlib",
  "exportSymbol": true,
  "exportFootprint": true,
  "exportModel3D": true,
  "exportPreviewImages": false,
  "exportDatasheet": false,
  "overwriteExistingFiles": false,
  "darkMode": false,
  "language": "en",
  "windowWidth": -1,
  "windowHeight": -1,
  "windowX": -9999,
  "windowY": -9999,
  "exitPreference": ""
}
EOF
```

## 调试技巧

### 进入 Flatpak 沙箱

```bash
# 进入 Flatpak 应用的沙箱环境
flatpak run --command=sh io.github.tangsangsimida.easykiconverter

# 在沙箱内可以：
# - 查看文件系统
# - 检查配置文件
# - 运行调试工具
ls -la ~/.var/app/io.github.tangsangsimida.easykiconverter/
```

### 使用 GDB 调试

```bash
# 使用 GDB 调试
flatpak run --devel --command=gdb io.github.tangsangsimida.easykiconverter

# 在 GDB 中设置断点
(gdb) break main
(gdb) run
(gdb) backtrace
```

### 查看权限

```bash
# 查看应用的权限
flatpak info --show-permissions io.github.tangsangsimida.easykiconverter

# 临时覆盖权限（仅用于调试）
flatpak run --filesystem=home io.github.tangsangsimida.easykiconverter
```

### 性能分析

```bash
# 使用 perf 进行性能分析
flatpak run --devel --command=perf io.github.tangsangsimida.easykiconverter record -g /tmp/perf.data
flatpak run --devel --command=perf io.github.tangsangsimida.easykiconverter report --stdio /tmp/perf.data
```

## 常见问题

### 问题 1：构建失败 - 找不到运行时

**错误信息：**
```
error: Remote 'kdeapps' not found
```

**解决方案：**
```bash
# 添加 KDE 运行时仓库
flatpak remote-add --if-not-exists kdeapps https://distribute.kde.org/kdeapps.flatpakrepo

# 或者从 Flathub 安装
flatpak install flathub org.kde.Platform//6.10 org.kde.Sdk//6.10
```

### 问题 2：构建失败 - QXlsx 依赖问题

**错误信息：**
```
error: Build failed
```

**解决方案：**
Flatpak 配置文件中已经配置了 QXlsx 的内置源码构建。检查 `deploy/flatpak/io.github.tangsangsimida.easykiconverter.local.yml` 中的 QXlsx 模块配置。

### 问题 3：运行时无法访问文件系统

**错误信息：**
```
Permission denied
```

**解决方案：**
```bash
# 授予访问主目录的权限
flatpak override --filesystem=home io.github.tangsangsimida.easykiconverter

# 授予访问特定目录的权限
flatpak override --filesystem=/home/username/Documents io.github.tangsangsimida.easykiconverter
```

### 问题 4：配置文件未保存

**可能原因：**
- Flatpak 应用的配置文件位置与本地应用不同
- 权限问题

**解决方案：**
```bash
# 检查配置文件目录是否存在
ls -la ~/.var/app/io.github.tangsangsimida.easykiconverter/config/

# 如果不存在，手动创建目录
mkdir -p ~/.var/app/io.github.tangsangsimida.easykiconverter/config/EasyKiConverter/EasyKiConverter/
```

### 问题 5：语言切换后重启未生效

**检查配置文件：**
```bash
cat ~/.var/app/io.github.tangsangsimida.easykiconverter/config/EasyKiConverter/EasyKiConverter/config.json | grep language
```

**预期输出：**
```json
"language": "en"
```

如果语言设置正确但界面未更新，可能是翻译文件未正确加载。检查日志：
```bash
flatpak run --devel io.github.tangsangsimida.easykiconverter 2>&1 | grep -i language
```

### 问题 6：内存不足

**错误信息：**
```
error: Out of memory
```

**解决方案：**
```bash
# 增加构建缓存大小
flatpak-builder --force-clean --repo=flatpak-repo --keep-build-dirs build-flatpak ../../deploy/flatpak/io.github.tangsangsimida.easykiconverter.local.yml

# 清理旧的构建缓存
rm -rf ~/.var/app/io.github.tangsangsimida.easykiconverter/cache/
```

## 高级技巧

### 自定义构建选项

```bash
# 启用调试构建
flatpak-builder --force-clean --repo=flatpak-repo --build-args="--env=CMAKE_BUILD_TYPE=Debug" build-flatpak ../../deploy/flatpak/io.github.tangsangsimida.easykiconverter.local.yml

# 启用测试
flatpak-builder --force-clean --repo=flatpak-repo --build-args="--env=EASYKICONVERTER_BUILD_TESTS=ON" build-flatpak ../../deploy/flatpak/io.github.tangsangsimida.easykiconverter.local.yml
```

### 导出为单文件

```bash
# 导出为单文件 bundle
flatpak build-bundle flatpak-repo easykiconverter.flatpak io.github.tangsangsimida.easykiconverter

# 运行 bundle
flatpak install easykiconverter.flatpak
flatpak run io.github.tangsangsimida.easykiconverter
```

### 开发环境设置

```bash
# 创建开发沙箱
flatpak build-init dev-sandbox org.kde.Sdk//6.10 org.kde.Platform//6.10

# 挂载项目目录
flatpak build --filesystem=home dev-sandbox

# 在开发沙箱中运行
flatpak build dev-sandbox /path/to/easykiconverter/build/bin/easykiconverter
```

## 相关资源

- [Flatpak 官方文档](https://docs.flatpak.org/)
- [KDE Flatpak 指南](https://community.kde.org/Guidelines_and_HOWTOs/Flatpak)
- [项目 Flatpak 配置文件](../../deploy/flatpak/io.github.tangsangsimida.easykiconverter.local.yml)
- [Flatpak manifest 格式](https://docs.flatpak.org/en/latest/flatpak-manifest.html)

## 贡献

如果你在 Flatpak 构建或使用过程中遇到问题，请：

1. 检查本文档的"常见问题"部分
2. 查看项目的 [GitHub Issues](https://github.com/EasyKiconverter/EasyKiConverter/issues)
3. 提交新的 Issue，包含详细的错误信息和日志

## 更新日志

- 2026-03-15: 初始版本，添加 Flatpak 开发指南
- 包含环境准备、编译、运行、配置文件管理和调试技巧
