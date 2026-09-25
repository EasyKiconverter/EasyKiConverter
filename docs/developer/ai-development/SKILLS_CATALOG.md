# AI Skills 目录

[English version](SKILLS_CATALOG_en.md)

仓库当前没有统一的跨 Agent Skill 自动发现机制。使用某个 Skill 时，先读取对应的 `SKILL.md`，再按其中的停止条件执行。

| Skill | 适用场景 | 默认副作用 |
| --- | --- | --- |
| [development](skills/development/SKILL.md) | 需要理解架构、定位责任边界并实施代码改动 | 可能修改工作区；不自动 Git 写入或远端操作 |
| [testing](skills/testing/SKILL.md) | 根据改动类型选择并执行验证 | 可能生成 `build/` 或日志；不修改 Git 历史 |
| [code-review](skills/code-review/SKILL.md) | 审查工作区、暂存区、提交或 PR | 只读；不实现修复、不提交、不推送 |
| [documentation](skills/documentation/SKILL.md) | 维护双语文档、导航、链接和 Mermaid | 可能修改文档；不自动提交或发布 |

## 选择原则

- 普通开发任务先用 `development`，完成实现后用 `testing`。
- 用户明确要求审查时只用 `code-review`，不要顺手修改。
- 文档任务使用 `documentation`，并按项目事实核对源码和工作流。
- 提交和 PR 不作为默认 Skill；只有用户明确要求时，按项目规则手动执行 Git/GitHub 操作。

Skills 只补充任务专属步骤，项目规则仍以 [AI 协作开发入口](README.md) 列出的权威文件为准。
