# 贡献指南

感谢您对 EasyKiConverter 项目的关注！我们欢迎各种形式的贡献，包括但不限于代码提交、Bug 报告、功能建议和文档改进。

## 如何贡献

### 报告 Bug

在提交 Bug 报告之前，请先搜索现有的 Issue，确认该问题是否已经被报告。如果未被发现，请创建一个新的 Issue，并提供以下信息：

- 问题描述：清晰、简洁地描述遇到的问题
- 复现步骤：详细的步骤列表，以便其他人能够复现问题
- 预期行为：您期望发生什么
- 实际行为：实际发生了什么
- 环境信息：
  - 操作系统版本
  - Qt 版本
  - CMake 版本
  - 编译器版本
- 截图或日志：如果适用，提供截图或错误日志

### 提出功能建议

在提交功能建议之前，请先搜索现有的 Issue，确认该建议是否已经被提出。如果未被发现，请创建一个新的 Issue，并提供以下信息：

- 功能描述：清晰、简洁地描述您希望添加的功能
- 使用场景：描述该功能的使用场景和好处
- 实现建议（可选）：如果您有实现思路，请分享
- 替代方案（可选）：如果您知道其他实现方式，请分享

### 提交代码

#### 开发环境设置

1. Fork 本项目到您的 GitHub 账户
2. Clone 您的 Fork 到本地：

```bash
git clone https://github.com/your-username/EasyKiConverter.git
cd EasyKiConverter
```

3. 添加上游仓库：

```bash
git remote add upstream https://github.com/EasyKiconverter/EasyKiConverter.git
```

4. 从问题所属的版本分支创建工作分支：

```bash
git fetch origin
git switch <version-branch>
git pull --ff-only origin <version-branch>
git switch -c <topic-branch>
```

一个相对完整的问题或开发主题对应一个工作分支。一个工作分支可以包含该问题所需的多个相关修改和多个提交；不要为每个小改动、子任务或单个提交创建新分支。当前仓库没有发现强制的分支命名校验，`fix/cache-safety-v3.1.13`、`feat/allegro-export-v3.1.13` 等仅作为便于识别的命名建议。

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
- **文件编码**: 所有文件必须使用 **UTF-8 (无 BOM)** 编码。严禁使用带有 BOM 的 UTF-8 或本地编码（如 GBK）。
- **工具校验**: 在提交代码前，请务必使用项目提供的编码转换工具对代码进行格式化处理。
  - 工具路径：`tools/python/convert_to_utf8.py`
  - 使用命令：`python tools/python/convert_to_utf8.py`
  - 该工具将自动确保所有 `.cpp`, `.h`, `.qml`, `.cmake` 等文件均符合 UTF-8 无 BOM 规范。

#### 架构要求

- 遵循 MVVM 架构模式
- View 层（QML）只负责界面展示
- ViewModel 层负责 UI 状态管理
- Service 层负责业务逻辑
- Model 层负责数据存储

**导出服务架构**：

- 使用两阶段流水线并行架构（Fetch → Export）
- Fetch 阶段：网络 I/O 并发任务，默认并发度由网络层统一控制（当前为 10）
- Export 阶段：并行完成 IR 构建、格式转换和文件写入
- 阶段间通过线程安全的有界队列通信
- 详见：[ADR-002: 流水线并行架构](../project/adr/002-pipeline-parallelism-for-export.md)

#### 分支和 PR 流程

项目按问题或主题组织开发，不采用固定的 `feature/* → dev → master` 流程。工作分支应从该问题所属的版本分支创建，完成后 PR 必须回到创建它时对应的同一版本分支；不要直接向版本分支提交。

```mermaid
flowchart LR
    V[所属版本分支] --> W[问题或主题工作分支]
    W --> C[多个相关提交]
    C --> P[Pull Request 回到同一版本分支]
    P --> R[审查与 CI]
    R --> M[维护者合并]
```

版本分支由维护者根据发布计划确定，例如 `v3.1.13`。贡献者不应自行推断所有工作都应合并到 `master` 或 `dev`；创建分支前应确认目标版本分支。

#### 提交流程

1. 提交您的更改：

```bash
git add .
git commit -m "feat: add your feature description"
```

2. 推送工作分支：

```bash
git push origin <topic-branch>
```

3. 在 GitHub 上创建 Pull Request：目标分支选择创建工作分支时对应的 `<version-branch>`，不要默认选择 `master`。

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
- Update documentation
```

#### Pull Request 要求

- 清晰描述更改的目的和内容
- 引用相关的 Issue（如果适用）
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

- 在 Issue 中提问
- 在 Pull Request 中请求帮助
- 联系项目维护者

## 许可证

通过提交代码，您同意您的贡献将根据项目的 GPL-3.0 许可证进行许可。

## 致谢

感谢所有为 EasyKiConverter 项目做出贡献的开发者！您的贡献使这个项目变得更好。
