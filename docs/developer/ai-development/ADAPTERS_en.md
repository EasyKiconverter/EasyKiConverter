# Agent Adapter Notes

[中文版](ADAPTERS.md)

## Single policy source

The only authoritative repository policy is [`policy/AI_POLICY_en.md`](policy/AI_POLICY_en.md). Local entries for Codex, Claude, Gemini, or other tools may only discover and point to this policy; they must not copy project rules or link to a mutable branch such as `master`.

## Current repository facts

At this revision, root `AGENTS.md`, `CLAUDE.md`, `PROJECT_INSTRUCTIONS.md`, and `opencode.json` are ignored by `.gitignore` and are not tracked by Git. They are local tool-adapter configuration, not reproducible repository policy. This task does not overwrite those existing local files.

The reproducible entry point is therefore:

1. Read the [AI development entry point](README_en.md).
2. Read the [AI Collaboration Policy](policy/AI_POLICY_en.md).
3. Select a Skill from the [Skills catalog](SKILLS_CATALOG_en.md), then read its body and `SKILL.meta.json`.

If maintainers enable automatic discovery for a particular Agent, its local entry should point to the entry point above without copying policy text, and docs-check should validate the reference. Until then, do not claim cross-Agent automatic discovery is implemented.
