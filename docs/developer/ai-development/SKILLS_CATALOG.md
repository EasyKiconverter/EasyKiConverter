# AI Skills 目录

[English version](SKILLS_CATALOG_en.md)

仓库通过 [`skill-index.json`](skill-index.json) 提供机器可读的 Skill 发现清单。它只描述仓库内实际存在的 Skill，不代表所有 Agent 会自动加载；具体 Agent 仍需按自身机制读取 `SKILL.md` 和 `SKILL.meta.json`。

| Skill | 适用场景 | 机器可读边界 | 默认副作用 |
| --- | --- | --- |
| [development](skills/development/SKILL.md) | 需要理解架构、定位责任边界并实施代码改动 | [元数据](skills/development/SKILL.meta.json) | 可能修改工作区；不自动 Git 写入或远端操作 |
| [testing](skills/testing/SKILL.md) | 根据改动类型选择并执行验证 | [元数据](skills/testing/SKILL.meta.json) | 可能生成 `build/` 或日志；不修改 Git 历史 |
| [code-review](skills/code-review/SKILL.md) | 审查工作区、暂存区、提交或 PR | [元数据](skills/code-review/SKILL.meta.json) | 只读；不实现修复、不提交、不推送 |
| [documentation](skills/documentation/SKILL.md) | 维护双语文档、导航、链接和 Mermaid | [元数据](skills/documentation/SKILL.meta.json) | 可能修改文档；不自动提交或发布 |
| [format-research](skills/format-research/SKILL.md) | 研究 EDA 格式、样本、许可证和第三方算法思想 | [元数据](skills/format-research/SKILL.meta.json) | 可能修改研究记录；不复制第三方代码 |
| [parser-development](skills/parser-development/SKILL.md) | 实现源格式 Parser/Model 到 IR 的确定性链路 | [元数据](skills/parser-development/SKILL.meta.json) | 可能修改源码、测试和 fixture |
| [exporter-development](skills/exporter-development/SKILL.md) | 实现 IR 到目标格式的导出、诊断和结构验证 | [元数据](skills/exporter-development/SKILL.meta.json) | 可能修改源码、测试和 fixture |

## 选择原则

- 普通开发任务先用 `development`，完成实现后用 `testing`。
- EDA 格式研究先用 `format-research`；进入 Parser 实现后使用 `parser-development`，进入目标导出后使用 `exporter-development`。
- 用户明确要求审查时只用 `code-review`，不要顺手修改。
- 文档任务使用 `documentation`，并按项目事实核对源码和工作流。
- 提交和 PR 不作为默认 Skill；只有用户明确要求时，按项目规则手动执行 Git/GitHub 操作。

Skills 只补充任务专属步骤，规则正文仍以 [AI 协作政策](policy/AI_POLICY.md) 为准；`skill-index.json`、front matter 和机器可读元数据由 docs-check 校验。
