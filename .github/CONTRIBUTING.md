# 贡献指南

感谢您对 EasyKiConverter 项目的关注！我们欢迎各种形式的贡献，包括但不限于代码提交、Bug 报告、功能建议和文档改进。

## 快速开始

如果您是第一次贡献，请阅读以下内容：

1. 阅读 [项目文档](https://github.com/EasyKiconverter/EasyKiConverter#readme)
2. 搜索现有的 [Issue](https://github.com/EasyKiconverter/EasyKiConverter/issues)
3. 在 [Discussions](https://github.com/EasyKiconverter/EasyKiConverter/discussions) 中讨论您的想法
4. 创建一个 fork，并提交您的更改，开始贡献！

## 如何贡献

### 报告 Bug

在提交 Bug 报告之前，请先搜索现有的 Issue，确认该问题是否已经被报告。如果未被发现，请使用 [Bug 报告模板](https://github.com/EasyKiconverter/EasyKiConverter/issues/new?template=bug_report.md) 创建一个新的 Issue。

### 提出功能建议

在提交功能建议之前，请先搜索现有的 Issue，确认该建议是否已经被提出。如果未被发现，请使用 [功能建议模板](https://github.com/EasyKiconverter/EasyKiConverter/issues/new?template=feature_request.md) 创建一个新的 Issue。

### 提交代码

#### 开发环境设置

1. **Fork 本项目**到您的 GitHub 账户
2. **Clone 您的 Fork**到本地：

```bash
git clone https://github.com/your-username/EasyKiConverter.git
cd EasyKiConverter
```

3. **添加上游仓库**：

```bash
git remote add upstream https://github.com/EasyKiconverter/EasyKiConverter.git
```

4. **从所属版本分支创建问题工作分支**：

```bash
git fetch upstream <version-branch>
git switch -c <topic-branch> --track upstream/<version-branch>
```

一个相对完整的问题或主题对应一个工作分支；同一分支可以包含多个相关提交。不要为每个小改动、子任务或单个提交创建新分支。当前仓库没有发现强制的分支命名校验，`fix/cache-safety-v3.1.13` 等只是命名建议。完成后 PR 回到创建该分支时使用的同一版本分支，不要直接提交到版本分支，也不要默认将所有 PR 指向 `master`。

在 Fork 流程中，`upstream` 用于获取正式仓库的版本分支，`origin` 仅用于推送工作分支。

#### 代码规范

**C++ 编码规范：**

- 类名使用大驼峰命名法（PascalCase）：`ComponentService`
- 变量名使用小驼峰命名法（camelCase）：`m_componentList`
- 常量使用全大写下划线分隔（UPPER_SNAKE_CASE）：`MAX_RETRIES`
- 成员变量使用 `m_` 前缀：`m_isDarkMode`
- 命名空间使用小写：`EasyKiConverter`

**QML 编码规范：**

- 组件命名使用大驼峰命名法（PascalCase）：`ModernButton`
- 属性命名使用小驼峰命名法（camelCase）：`componentId`
- ID 命名使用小驼峰命名法（camelCase）：`componentInput`
- 使用 4 空格缩进

**注释规范：**

- 类和公共方法添加 Doxygen 风格注释
- 复杂逻辑添加行内注释
- 使用 `///` 或 `/** */` 格式的注释

#### 架构要求

- 遵循 MVVM 架构模式
- View 层（QML）只负责界面展示
- ViewModel 层负责 UI 状态管理
- Service 层负责业务逻辑
- Model 层负责数据存储

详细的架构说明请参考：[架构文档](docs/developer/ARCHITECTURE.md)

#### 测试要求

- 为新功能添加单元测试
- 确保所有测试通过
- 测试覆盖率不得降低
- 遵循测试指南：[测试指南](tests/TESTING_GUIDE.md)

#### 提交流程

1. **提交您的更改**：

```bash
git add .
git commit -m "feat(export): 修复封装解析"
```

2. **推送工作分支**：

```bash
git push origin <topic-branch>
```

3. **在 GitHub 上创建 Pull Request**：目标分支选择创建工作分支时对应的 `<version-branch>`。

使用 [Pull Request 模板](https://github.com/EasyKiconverter/EasyKiConverter/compare) 创建 PR。

#### 提交信息规范

使用语义化提交信息：

- `feat:` 新功能
- `fix:` Bug 修复
- `docs:` 文档更新
- `style:` 代码格式调整（不影响功能）
- `refactor:` 重构（既不是新功能也不是 Bug 修复）
- `perf:` 性能优化
- `test:` 测试相关
- `chore:` 构建过程或辅助工具的变动

示例：

```
feat: add support for custom layer mapping

- Implement LayerMapper class for EasyEDA to KiCad layer conversion
- Add unit tests for layer mapping logic
- Update documentation
```

#### Pull Request 要求

- 清晰描述更改的目的和内容
- 引用相关的 Issue（如果适用）
- 确保所有测试通过
- 更新相关文档
- 保持代码风格一致
- 确保与现有代码的兼容性

### 改进文档

文档是项目的重要组成部分。如果您发现文档有错误、不清晰或需要补充，欢迎提交改进：

- 修正拼写和语法错误
- 添加缺失的说明
- 改进示例代码
- 添加图表或截图
- 翻译文档到其他语言

## 代码审查

所有提交的代码都需要经过代码审查。审查者会检查：

- 代码质量和风格
- 架构设计
- 测试覆盖率
- 文档完整性
- 与现有代码的兼容性

请耐心等待审查反馈，并根据反馈进行必要的修改。

## 行为准则

- 尊重所有贡献者
- 建设性地提出意见
- 接受不同的观点
- 保持专业和礼貌
- 避免使用攻击性语言

## 获取帮助

如果您在贡献过程中遇到问题，可以通过以下方式获取帮助：

- 在 [Issue](https://github.com/EasyKiconverter/EasyKiConverter/issues) 中提问
- 在 [Pull Request](https://github.com/EasyKiconverter/EasyKiConverter/pulls) 中请求帮助
- 在 [Discussions](https://github.com/EasyKiconverter/EasyKiConverter/discussions) 中讨论
- 联系项目维护者

## 许可证

通过提交代码，您同意您的贡献将根据项目的 GPL-3.0 许可证进行许可。

## 致谢

感谢所有为 EasyKiConverter 项目做出贡献的开发者！您的贡献使这个项目变得更好。

## 相关资源

- [项目文档](docs/)
- [构建指南](docs/developer/BUILD.md)
- [架构文档](docs/developer/ARCHITECTURE.md)
- [测试指南](tests/TESTING_GUIDE.md)
- [用户手册](docs/user/USER_GUIDE.md)
