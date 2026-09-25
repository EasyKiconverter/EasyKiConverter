# Agent 适配层说明

[English version](ADAPTERS_en.md)

## 唯一规则源

仓库内唯一权威规则是 [`policy/AI_POLICY.md`](policy/AI_POLICY.md)。Codex、Claude、Gemini 等工具的本地入口只能负责发现并指向该文件，不得复制项目规则正文，也不得链接到 `master` 等可变分支。

## 当前仓库事实

根目录的 `AGENTS.md`、`CLAUDE.md`、`PROJECT_INSTRUCTIONS.md` 和 `opencode.json` 在当前工作环境中被 `.gitignore` 忽略且未被 Git 跟踪；它们属于本机工具适配配置，不是可复现的仓库规则源。本任务不覆盖这些已有本地文件。

因此当前可复现的入口是：

1. 读取 [AI 协作入口](README.md)。
2. 读取 [AI 协作政策](policy/AI_POLICY.md)。
3. 按 [Skills 目录](SKILLS_CATALOG.md) 选择 Skill，并读取正文和 `SKILL.meta.json`。

维护者若要启用某个 Agent 的自动发现，应在不复制规则的前提下，将其本地入口改为指向上述入口，并通过 docs-check 验证引用；在完成前不得宣称跨 Agent 自动发现已经实现。
