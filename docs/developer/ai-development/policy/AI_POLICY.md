# EasyKiConverter AI 协作政策

[English version](AI_POLICY_en.md)

本文是仓库内由 Git 跟踪的 AI 协作规则唯一权威入口。它适用于 Codex、Claude、Gemini 以及其他参与本仓库工作的 Agent。
根目录的 `AGENTS.md`、`CLAUDE.md`、`PROJECT_INSTRUCTIONS.md` 和本目录下的 Skill 文件是适配层或任务说明，不得复制一套与本文冲突的规则。

## 规则来源与优先级

1. 当前分支中实际存在的源码、测试、工作流和配置是事实依据。
2. 本政策规定 AI 协作的边界；领域文档补充格式和模块细节。
3. Issue、PR 评论和历史规划只能作为线索，不能覆盖当前代码和测试事实。
4. 商业 EDA 软件中的打开、保存和回读只有在明确记录实机环境和结果时才算实机验证。

项目规则文件可以因为工具兼容性被 `.gitignore` 忽略，但这不代表 Git 禁止跟踪它们；已跟踪的规则文件仍由仓库历史管理。个人机器上的未跟踪 Agent 配置不得替代仓库规则。

## 任务边界

- 开始前检查分支、基线和 `git status --short`，保留已有修改。
- 修改前先读取与任务相关的源码、测试、文档和工作流；仓库存在 `.codegraph/` 时优先使用 CodeGraph。
- 解析和导出必须保持确定性，不得让 LLM 参与字段推断、坐标/图层映射或 writer 输出。
- 新增格式遵循“源格式 Parser/Model → IR → 目标格式 Exporter/Writer”；格式专用字段不得绕过 IR 静默流失。
- 网络测试使用 Mock，测试 fixture 使用仓库路径工具和临时目录，不访问真实服务。
- 不凭代码入口推断完整格式支持；必须分别记录自动测试、结构校验和商业 EDA 实机验证。
- 默认不修改运行时无关内容，不修改应用身份、发布产物、远端设置或分支保护。

## 代码、文档与验证

- 公共 C++ 接口使用简体中文 Doxygen 注释；保持现有格式和中文/英文文档成对维护。
- 复杂架构、流程、依赖和状态关系使用 Mermaid。
- 验证报告区分实际执行、未执行、环境阻塞和推断结果；测试通过不等于商业 EDA 兼容。
- 最低验证策略见 [`verification-policy.json`](../verification-policy.json)，Testing Skill 只能在风险更高时增加验证，不能降低最低要求。
- 标准交付证据见 [`EVIDENCE_REPORT_TEMPLATE.md`](../EVIDENCE_REPORT_TEMPLATE.md)。

## Git 操作

- 普通开发默认只修改本地工作区，不提交、不推送、不创建 PR。
- 一个相对完整的问题或主题对应一个工作分支；一个工作分支可以包含多个相关提交，不为每个小改动、子任务或单个提交创建分支。
- 工作分支从该问题所属的版本分支创建，完成后 PR 回到创建它时对应的同一版本分支；不得直接向版本分支提交。
- 仓库未发现强制分支命名格式；命名建议不能替代对所属版本分支的确认。
- 用户明确要求提交或 PR 时，先确认目标分支、提交范围和当前账号，再执行仓库规定的中文 Conventional Commit。
- 禁止破坏性重置、强制推送和删除用户已有修改。

## 适配层

| 入口 | 用途 |
| --- | --- |
| `AGENTS.md` | Agent 环境和仓库特有陷阱的短入口 |
| `CLAUDE.md` / `PROJECT_INSTRUCTIONS.md` | 本地工具兼容入口；不得作为独立规则源 |
| `docs/developer/ai-development/skills/*/SKILL.md` | 任务专属流程 |
| `SKILL.meta.json` | Skill 的机器可读边界和验证要求 |

本政策使用仓库相对链接，不依赖 `master` 或其他可变分支，因此历史 tag 可以复现当时的规则版本。
