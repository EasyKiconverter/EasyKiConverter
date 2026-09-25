# EasyKiConverter AI Collaboration Policy

[中文版](AI_POLICY.md)

This tracked document is the single authoritative entry point for AI collaboration in the repository. It applies to Codex, Claude, Gemini, and other agents. Root-level `AGENTS.md`, `CLAUDE.md`, `PROJECT_INSTRUCTIONS.md`, and the Skill files are adapters or task procedures; they must not create a conflicting copy of this policy.

## Sources and precedence

1. Source code, tests, workflows, and configuration on the current branch are the factual baseline.
2. This policy defines collaboration boundaries; domain documents add format and module details.
3. Issues, pull-request comments, and historical plans are leads, not substitutes for current evidence.
4. Opening, saving, and round-tripping in a commercial EDA application count as software validation only when the exact environment and result are recorded.

Project rule files may be ignored by `.gitignore` for local convenience, but Git does not forbid tracking them; tracked rule files remain versioned. Untracked personal Agent configuration must not replace repository rules.

## Task boundaries

- Check the branch, baseline, and `git status --short` before work, and preserve existing changes.
- Read relevant source, tests, documentation, and workflows first; use CodeGraph before textual search when `.codegraph/` exists.
- Keep parsing and exporting deterministic. An LLM must not infer fields or perform coordinate, layer, or writer mapping.
- New formats follow `source Parser/Model → IR → target Exporter/Writer`; format-specific data must not bypass IR or disappear silently.
- Use mocks for network tests and repository fixture-path helpers with temporary directories for file tests. Do not access live services.
- Do not infer complete format support from a code entry point. Record automated tests, structural validation, and commercial EDA validation separately.
- By default, do not change unrelated runtime behavior, application identities, release artifacts, remote settings, or branch protection.

## Code, documentation, and verification

- Public C++ interfaces use Simplified Chinese Doxygen comments. Maintain Chinese/English documentation pairs.
- Use Mermaid for non-trivial architecture, process, dependency, and state relationships.
- Evidence reports must distinguish executed, not executed, environment-blocked, and inferred results. Passing tests does not prove commercial EDA compatibility.
- The minimum verification policy is [`verification-policy.json`](../verification-policy.json). A Testing Skill may add checks for risk, but may not lower the minimum.
- Use [`EVIDENCE_REPORT_TEMPLATE_en.md`](../EVIDENCE_REPORT_TEMPLATE_en.md) for standard evidence reporting.

## Git operations

- Normal development modifies only the local worktree: no commit, push, or pull request by default.
- When the user explicitly requests a commit or pull request, confirm the target branch, scope, and active account first, then use the repository's Chinese Conventional Commit rules.
- Do not use destructive resets, force pushes, or removal of existing user changes.

## Adapter layers

| Entry | Purpose |
| --- | --- |
| `AGENTS.md` | Short entry for Agent environment and repository gotchas |
| `CLAUDE.md` / `PROJECT_INSTRUCTIONS.md` | Local tool compatibility entries; not independent policy sources |
| `docs/developer/ai-development/skills/*/SKILL.md` | Task-specific procedures |
| `SKILL.meta.json` | Machine-readable Skill boundaries and verification requirements |

This policy uses repository-relative links and does not depend on `master` or another mutable branch, so historical tags can reproduce the rules that existed at that revision.
