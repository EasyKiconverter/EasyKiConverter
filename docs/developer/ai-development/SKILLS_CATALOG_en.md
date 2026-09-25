# AI Skills Catalog

[中文版](SKILLS_CATALOG.md)

The repository has no unified cross-Agent Skill discovery mechanism. When using a Skill, read its `SKILL.md` and `SKILL.meta.json` first, then follow its stop conditions.

| Skill | Use when | Machine-readable boundary | Default side effects |
| --- | --- | --- |
| [development](skills/development/SKILL.md) | Understanding architecture, ownership boundaries, and implementing code changes | [Metadata](skills/development/SKILL.meta.json) | May modify the worktree; does not perform Git or remote writes automatically |
| [testing](skills/testing/SKILL.md) | Selecting and running verification for a change | [Metadata](skills/testing/SKILL.meta.json) | May create `build/` or logs; does not rewrite Git history |
| [code-review](skills/code-review/SKILL.md) | Reviewing a worktree, index, commit, or PR | [Metadata](skills/code-review/SKILL.meta.json) | Read-only; does not implement fixes, commit, or push |
| [documentation](skills/documentation/SKILL.md) | Maintaining bilingual docs, navigation, links, and Mermaid | [Metadata](skills/documentation/SKILL.meta.json) | May modify docs; does not commit or publish automatically |

## Selection rules

- Use `development` for normal implementation work, followed by `testing`.
- Use only `code-review` for an explicit review request; do not implement fixes as a side effect.
- Use `documentation` for documentation tasks and verify claims against source and workflows.
- Commit and PR are not default Skills. Perform Git/GitHub operations only after an explicit request and according to project rules.

Skills add task-specific steps only. The [AI Collaboration Policy](policy/AI_POLICY_en.md) remains authoritative, and docs-check validates the machine-readable metadata.
