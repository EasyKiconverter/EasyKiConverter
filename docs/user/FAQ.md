# 常见问题解答

本文档收集了用户最常遇到的问题和解决方案。

## 一般问题

### EasyKiConverter 是什么？

EasyKiConverter 是一个基于 Qt 6 Quick 和 MVVM 架构的 C++ 桌面应用程序，用于将嘉立创（LCSC）和 EasyEDA 元件转换为多种 EDA 库格式。它提供符号库、封装库和 3D 模型文件的转换功能；Xpedition 当前输出独立 3D 文件，不写入原生 3D 关联。

### EasyKiConverter 是免费的吗？

是的，EasyKiConverter 采用 GNU General Public License v3.0 (GPL-3.0) 许可证，完全免费开源。

### 支持哪些操作系统？

EasyKiConverter 支持：
- Windows 10/11（推荐）
- macOS 10.15+
- Linux（Ubuntu 20.04+、Debian 10+、Fedora 33+）

### 需要 EasyEDA 账户吗？

不需要。EasyKiConverter 使用公开的 EasyEDA API，不需要账户即可使用。但是，如果您需要访问私有元件，则需要 EasyEDA 账户。

## 安装和设置

### 如何安装 EasyKiConverter？

请参考[安装指南](USER_GUIDE.md)。

### 系统要求是什么？

- 操作系统：Windows 10/11、macOS 10.15+ 或 Linux
- 内存：4GB RAM 最低，8GB 推荐
- 磁盘空间：100MB 用于应用程序，额外空间用于转换后的文件
- 网络：用于下载元件数据和 3D 模型的互联网连接

### 如何从源代码构建？

请参考[构建指南](../developer/BUILD.md)。

### 构建时遇到 Qt 找不到的错误怎么办？

确保：
1. Qt 已正确安装
2. CMAKE_PREFIX_PATH 设置正确
3. Qt 版本为 6.8 或更高

## 使用问题

### 如何添加元件？

有三种方法：
1. **手动输入**：在输入框中输入元件编号，点击"添加"
2. **智能提取**：复制元件编号，点击"智能提取"按钮
3. **BOM 导入**：点击"BOM 导入"按钮，选择 BOM 文件

详细步骤请参考[用户手册](USER_GUIDE.md)。

### 支持哪些元件编号格式？

支持标准的 LCSC 元件编号格式，例如：
- C123456
- C789012
- C345678

### 可以批量转换元件吗？

是的，EasyKiConverter 支持批量转换。您可以：
- 使用智能提取添加多个元件
- 导入 BOM 文件进行批量转换
- 并行转换提高效率

### 转换后的文件在哪里？

转换后的文件保存在您在导出设置中指定的目录中。您可以：
1. 在结果列表中点击文件位置链接
2. 或者手动导航到导出目录

### 如何在 KiCad 中使用转换后的元件？

详细步骤请参考[用户手册](USER_GUIDE.md)。

### 支持哪些 KiCad 版本？

支持 KiCad 6.0+、7.0+、8.0+ 和 9.0+。

### 支持哪些 EasyEDA 版本？

支持 EasyEDA Standard、EasyEDA Professional 和 EasyEDA Pro。

## 转换问题

### 为什么转换失败？

可能的原因：
1. 元件编号不正确
2. 元件不存在于 LCSC 数据库中
3. 网络连接问题
4. 元件数据不可用

**解决方案：**
- 验证元件编号是否正确
- 检查网络连接
- 查看结果列表中的错误消息
- 尝试再次转换

### 为什么 3D 模型没有下载？

可能的原因：
1. 元件没有 3D 模型
2. "3D 模型"选项未启用
3. 网络连接问题

**解决方案：**
- 确保在导出设置中启用了"3D 模型"
- 检查元件是否有 3D 模型
- 验证网络连接

### 转换后的文件打不开怎么办？

可能的原因：
1. 文件损坏
2. KiCad 版本不兼容
3. 文件格式不正确

**解决方案：**
- 重新转换元件
- 更新 KiCad 到最新版本
- 检查文件格式是否正确

### 转换速度慢怎么办？

优化建议：
- 批量转换多个元件
- 关闭其他应用程序
- 使用更快的网络连接
- 确保系统资源充足

### 转换结果与 EasyEDA 不一致怎么办？

EasyKiConverter 努力保持与 EasyEDA 的一致性，但可能存在细微差异。如果您发现重大差异，请：
1. 在 GitHub 上报告问题
2. 提供详细的对比信息
3. 附上元件编号和截图

## 功能问题

### 什么是智能提取？

智能提取功能可以从剪贴板文本中自动提取有效的元件编号。它支持：
- 自动识别元件编号格式
- 批量提取
- 格式验证

### 什么是层映射？

层映射是将 EasyEDA 图层自动映射到 KiCad 图层的过程。EasyKiConverter 支持 50+ 图层的映射。

### 支持哪些 3D 模型格式？

支持：
- WRL（VRML）
- STEP
- OBJ

### 可以自定义层映射吗？

目前版本不支持自定义层映射。未来版本可能会添加此功能。

### 如何启用调试模式？

**方法 1：命令行参数（推荐）**
```bash
# Windows
.\build\bin\EasyKiConverter.exe --debug

# Linux/macOS
./build/bin/EasyKiConverter --debug
```

**方法 2：环境变量**
```bash
# Windows PowerShell
$env:EASYKICONVERTER_DEBUG_MODE="true"
.\build\bin\EasyKiConverter.exe

# Linux/macOS
export EASYKICONVERTER_DEBUG_MODE=true
./build/bin/EasyKiConverter
```

请参考 [调试模式配置文档](DEBUG_MODE.md) 了解详细的配置方法和所有可用的命令行参数。

### 如何更改主题？

点击右上角的主题切换按钮，可以在深色模式和浅色模式之间切换。

## 性能问题

### 如何提高转换速度？

优化建议：
- 批量转换多个元件
- 使用并行转换
- 关闭不必要的应用程序
- 使用更快的网络连接

### 内存占用过高怎么办？

优化建议：
- 减少批量转换的元件数量
- 关闭其他应用程序
- 重启应用程序

### 应用程序崩溃怎么办？

**解决方案：**
1. 检查系统要求
2. 更新到最新版本
3. 查看错误日志
4. 在 GitHub 上报告问题

## 兼容性问题

### 支持哪些 KiCad 版本？

支持 KiCad 6.0+、7.0+、8.0+ 和 9.0+。

### 支持哪些操作系统？

支持 Windows 10/11、macOS 10.15+ 和 Linux。

### 在 Linux 上运行有问题怎么办？

确保：
1. 安装了所有依赖
2. Qt 版本正确
3. 编译器版本兼容
4. 系统库完整

## 错误消息

### "元件未找到"

**原因：** 元件编号不正确或元件不存在

**解决方案：**
- 验证元件编号
- 检查元件是否存在于 LCSC 数据库中

### "网络连接失败"

**原因：** 网络连接问题

**解决方案：**
- 检查网络连接
- 尝试使用 VPN
- 检查防火墙设置

### 弱网环境下转换频繁失败怎么办？

如果您的网络环境不稳定（高延迟、高丢包率），可能会遇到以下问题：
- 批量导出时大量元件标记为失败
- 3D 模型下载超时
- 请求长时间无响应

**解决方案：**
- 使用更稳定的网络连接（有线优于无线）
- 减少同时导出的元件数量
- 对失败的元件使用"重试导出"功能
- 如果问题持续存在，请在 GitHub 上报告问题并附上网络环境描述

### "文件写入失败"

**原因：** 磁盘空间不足或权限问题

**解决方案：**
- 检查磁盘空间
- 检查写入权限
- 选择其他导出目录

### "转换失败"

**原因：** 元件数据不可用或格式不正确

**解决方案：**
- 查看详细错误消息
- 尝试重新转换
- 在 GitHub 上报告问题

## 高级问题

### 如何集成到构建系统？

EasyKiConverter 支持命令行接口，可以集成到构建系统中：

```bash
# 批量转换
easykiconverter convert batch -i <component_list_file> -o <output_dir>

# 转换单个元件
easykiconverter convert component -c <lcsc_id> -o <output_dir>

# 转换 BOM 表
easykiconverter convert bom -i <bom_file> -o <output_dir>
```

详细说明请参考 [CLI 使用说明](../CLI_USAGE.md)。

### 如何自定义导出选项？

在导出设置中配置：
- 导出目录
- 导出类型
- 覆盖行为
- 调试选项

### 如何贡献代码？

请参考[贡献指南](../developer/CONTRIBUTING.md)。

### 如何报告 Bug？

1. 搜索现有的 GitHub Issues
2. 创建新的 Issue，包含：
   - 问题描述
   - 复现步骤
   - 预期行为
   - 实际行为
   - 环境信息
   - 截图（如果适用）

### 如何请求新功能？

1. 搜索现有的 GitHub Issues
2. 创建新的 Issue，包含：
   - 功能描述
   - 使用场景
   - 预期行为
   - 示例（如果适用）

## 其他问题

### 如何获取帮助？

1. 查看文档
2. 搜索 GitHub Issues
3. 创建新的 Issue
4. 参与 GitHub Discussions

### 如何联系开发者？

通过 GitHub Issues 联系项目维护者。

### 如何捐赠？

目前不接受捐赠。您可以通过以下方式支持项目：
- 贡献代码
- 报告 Bug
- 请求新功能
- 分享项目

### 有视频教程吗？

目前没有视频教程。您可以通过以下方式学习：
- 阅读文档
- 查看 GitHub Issues
- 参与 GitHub Discussions

## 更多资源

- [用户手册](USER_GUIDE.md) - 详细使用说明
- [快速开始](GETTING_STARTED.md) - 快速入门
- [功能特性](FEATURES.md) - 了解所有功能
- [构建指南](../developer/BUILD.md) - 从源代码构建
- [贡献指南](../developer/CONTRIBUTING.md) - 贡献代码
- [开发者常见问题](../developer/FAQ.md) - 技术问题、根因分析和回归预防
- [GitHub Issues](https://github.com/EasyKiconverter/EasyKiConverter/issues) - 报告问题
- [GitHub Discussions](https://github.com/EasyKiconverter/EasyKiConverter/discussions) - 问答和讨论

如果您的问题不在这里，请随时在 GitHub 上创建新的 Issue。
