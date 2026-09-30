# AI Skills Catalog

[中文版](SKILLS_CATALOG.md)

The repository provides a machine-readable Skill discovery list in [`skill-index.json`](skill-index.json). It describes only Skills present in this checkout and does not claim that every Agent loads them automatically; each Agent still reads `SKILL.md` and `SKILL.meta.json` through its supported mechanism.

| Skill | Use when | Machine-readable boundary | Default side effects |
| --- | --- | --- |
| [development](skills/development/SKILL.md) | Understanding architecture, ownership boundaries, and implementing code changes | [Metadata](skills/development/SKILL.meta.json) | May modify the worktree; does not perform Git or remote writes automatically |
| [testing](skills/testing/SKILL.md) | Selecting and running verification for a change | [Metadata](skills/testing/SKILL.meta.json) | May create `build/` or logs; does not rewrite Git history |
| [code-review](skills/code-review/SKILL.md) | Reviewing a worktree, index, commit, or PR | [Metadata](skills/code-review/SKILL.meta.json) | Read-only; does not implement fixes, commit, or push |
| [documentation](skills/documentation/SKILL.md) | Maintaining bilingual docs, navigation, links, and Mermaid | [Metadata](skills/documentation/SKILL.meta.json) | May modify docs; does not commit or publish automatically |
| [format-research](skills/format-research/SKILL.md) | Researching EDA formats, samples, licenses, and third-party algorithm ideas | [Metadata](skills/format-research/SKILL.meta.json) | May modify research notes; never copies third-party code |
| [parser-development](skills/parser-development/SKILL.md) | Implementing deterministic source-format Parser/Model to IR flows | [Metadata](skills/parser-development/SKILL.meta.json) | May modify source, tests, and fixtures |
| [exporter-development](skills/exporter-development/SKILL.md) | Implementing IR-to-target export, diagnostics, and structural validation | [Metadata](skills/exporter-development/SKILL.meta.json) | May modify source, tests, and fixtures |

## Selection rules

- Use `development` for normal implementation work, followed by `testing`.
- Use `format-research` for EDA format research, then `parser-development` for Parser work or `exporter-development` for target export work.
- Use only `code-review` for an explicit review request; do not implement fixes as a side effect.
- Use `documentation` for documentation tasks and verify claims against source and workflows.
- Commit and PR are not default Skills. Perform Git/GitHub operations only after an explicit request and according to project rules.

Skills add task-specific steps only. The [AI Collaboration Policy](policy/AI_POLICY_en.md) remains authoritative, and docs-check validates `skill-index.json`, front matter, and machine-readable metadata.
