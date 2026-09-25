# AI 协作开发入口

[English version](README_en.md)

本文面向参与 EasyKiConverter 开发的贡献者和 AI Agent，用于快速了解项目背景、权威规则、开发流程、验证入口和可复用任务型 Skill。

## 先读什么

1. [AI 协作政策](policy/AI_POLICY.md) 是仓库内唯一权威的 AI 协作规则源；根目录的 `AGENTS.md`、`PROJECT_INSTRUCTIONS.md` 和 `CLAUDE.md` 是工具适配层。
2. [项目上下文](PROJECT_CONTEXT.md) 说明架构边界和当前能力。
3. [开发工作流](DEVELOPMENT_WORKFLOW.md) 说明从任务理解到本地提交前的步骤。
4. [AI 测试指南](TESTING_GUIDE.md) 根据改动类型选择验证项；详细测试原则仍以 [测试开发指南](../TESTING_GUIDE.md) 为准。
5. [Skills 目录](SKILLS_CATALOG.md) 说明可手动调用的任务流程。
6. [Evidence Report 模板](EVIDENCE_REPORT_TEMPLATE.md) 统一记录基线、命令、阻塞项和商业 EDA 验证边界。
7. [Agent 适配层说明](ADAPTERS.md) 说明本地入口与仓库规则的边界。

## 权威规则与辅助资料

| 内容 | 权威文件 |
| --- | --- |
| AI 协作规则 | [AI 协作政策](policy/AI_POLICY.md) |
| 工具适配层 | `AGENTS.md`、`PROJECT_INSTRUCTIONS.md`、`CLAUDE.md` |
| 架构和模块职责 | [架构文档](../ARCHITECTURE.md) |
| 构建环境 | [构建指南](../BUILD.md) |
| 测试约束 | [测试开发指南](../TESTING_GUIDE.md) |
| 编码规范 | [编码规范](../CODING_STYLE.md) |
| 贡献和 PR | [贡献指南](../CONTRIBUTING.md) |
| 文档维护 | [文档维护指南](../DOCUMENTATION_MAINTENANCE.md) |

本目录提供规则入口、AI 协作导航和任务流程，不复制工具适配层的完整内容。出现冲突时，以当前分支的源码、测试、工作流和 [AI 协作政策](policy/AI_POLICY.md) 为准。

## 适用范围

- Skill 适用于需要重复执行、边界明确的开发辅助任务。
- Skill 不会自动改变项目规则，也不会替代代码、测试或商业 EDA 工具的实际验证。
- 当前仓库没有统一的跨 Agent Skill 自动发现配置；调用方应按 [Skills 目录](SKILLS_CATALOG.md) 选择并读取对应的 `SKILL.md` 和 `SKILL.meta.json`。

## 机器可验证的控制面

- [验证策略](verification-policy.json) 是 CI 与 Testing Skill 共用的最低验证事实来源；变更分类器只选择范围，不替代模块级验证。
- [EDA 能力台账](eda-capabilities.json) 区分代码入口、自动测试、结构校验和商业 EDA 实机验证。
- [Fixture provenance](fixture-provenance.json) 记录本地 fixture 的哈希、测试用途和未知来源，不从内容猜造来源。
- [AI Agent 回归评测](AGENT_EVAL.md) 提供不调用付费模型的规则边界样例。

## 维护要求

- 面向项目的长期文档应维护中文和英文对应版本，并提供互相跳转。
- 架构、流程、依赖和状态关系使用 Mermaid；简单步骤不强制绘图。
- 事实以当前源码、工作流和实际测试结果为准，不把规划能力写成已实现能力。
- 规则正文和机器可读事实文件必须由 Git 跟踪；个人 Agent 配置可以保留在本地，但不能替代仓库规则。
