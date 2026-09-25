# AI 协作开发入口

[English version](README_en.md)

本文面向参与 EasyKiConverter 开发的贡献者和 AI Agent，用于快速了解项目背景、权威规则、开发流程、验证入口和可复用任务型 Skill。

## 先读什么

1. [项目规则](https://github.com/EasyKiconverter/EasyKiConverter/blob/master/AGENTS.md)、[项目指令](https://github.com/EasyKiconverter/EasyKiConverter/blob/master/PROJECT_INSTRUCTIONS.md) 和 [CLAUDE.md](https://github.com/EasyKiconverter/EasyKiConverter/blob/master/CLAUDE.md) 是约束的权威来源。
2. [项目上下文](PROJECT_CONTEXT.md) 说明架构边界和当前能力。
3. [开发工作流](DEVELOPMENT_WORKFLOW.md) 说明从任务理解到本地提交前的步骤。
4. [AI 测试指南](TESTING_GUIDE.md) 根据改动类型选择验证项；详细测试原则仍以 [测试开发指南](../TESTING_GUIDE.md) 为准。
5. [Skills 目录](SKILLS_CATALOG.md) 说明可手动调用的任务流程。

## 权威规则与辅助资料

| 内容 | 权威文件 |
| --- | --- |
| 项目约束和环境 | `AGENTS.md`、`PROJECT_INSTRUCTIONS.md`、`CLAUDE.md` |
| 架构和模块职责 | [架构文档](../ARCHITECTURE.md) |
| 构建环境 | [构建指南](../BUILD.md) |
| 测试约束 | [测试开发指南](../TESTING_GUIDE.md) |
| 编码规范 | [编码规范](../CODING_STYLE.md) |
| 贡献和 PR | [贡献指南](../CONTRIBUTING.md) |
| 文档维护 | [文档维护指南](../DOCUMENTATION_MAINTENANCE.md) |

本目录只提供 AI 协作导航和任务流程，不复制上述文件的完整内容。出现冲突时，以项目规则和对应领域文档为准。

## 适用范围

- Skill 适用于需要重复执行、边界明确的开发辅助任务。
- Skill 不会自动改变项目规则，也不会替代代码、测试或商业 EDA 工具的实际验证。
- 当前仓库没有统一的跨 Agent Skill 自动发现配置；调用方应按 [Skills 目录](SKILLS_CATALOG.md) 选择并读取对应的 `SKILL.md`。

## 维护要求

- 面向项目的长期文档应维护中文和英文对应版本，并提供互相跳转。
- 架构、流程、依赖和状态关系使用 Mermaid；简单步骤不强制绘图。
- 事实以当前源码、工作流和实际测试结果为准，不把规划能力写成已实现能力。
