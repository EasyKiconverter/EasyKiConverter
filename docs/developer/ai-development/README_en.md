# AI-Assisted Development

[中文版](README.md)

This entry point is for contributors and AI Agents working on EasyKiConverter. It provides project context, authoritative rules, development workflow, verification entry points, and reusable task-oriented Skills.

## Recommended reading order

1. The authoritative constraints are [project rules](https://github.com/EasyKiconverter/EasyKiConverter/blob/master/AGENTS.md), [project instructions](https://github.com/EasyKiconverter/EasyKiConverter/blob/master/PROJECT_INSTRUCTIONS.md), and [CLAUDE.md](https://github.com/EasyKiconverter/EasyKiConverter/blob/master/CLAUDE.md).
2. [Project context](PROJECT_CONTEXT_en.md) explains architecture boundaries and current capabilities.
3. [Development workflow](DEVELOPMENT_WORKFLOW_en.md) describes the path from task understanding to a local pre-commit state.
4. [AI testing guide](TESTING_GUIDE_en.md) maps change types to verification; detailed testing policy remains in the [testing guide](../TESTING_GUIDE_en.md).
5. [Skills catalog](SKILLS_CATALOG_en.md) lists manually invokable task procedures.

## Authoritative rules and supporting material

| Topic | Authoritative file |
| --- | --- |
| Project constraints and environment | `AGENTS.md`, `PROJECT_INSTRUCTIONS.md`, `CLAUDE.md` |
| Architecture and module ownership | [Architecture](../ARCHITECTURE_en.md) |
| Build environment | [Build Guide](../BUILD_en.md) |
| Testing policy | [Testing Guide](../TESTING_GUIDE_en.md) |
| Coding conventions | [Coding Style](../CODING_STYLE_en.md) |
| Contributions and PRs | [Contributing Guide](../CONTRIBUTING_en.md) |
| Documentation maintenance | [Documentation Maintenance](../DOCUMENTATION_MAINTENANCE.md) |

This directory provides AI collaboration navigation and task procedures. It does not duplicate the full content of the files above. When documents conflict, project rules and the relevant domain document take precedence.

## Scope

- Skills are for repeatable, bounded tasks with clear side effects.
- A Skill does not replace project rules, tests, or validation in commercial EDA applications.
- The repository has no unified cross-Agent Skill discovery configuration. Select and read the relevant `SKILL.md` through the [Skills catalog](SKILLS_CATALOG_en.md).

## Maintenance rules

- Long-lived project documentation should have synchronized Chinese and English versions with reciprocal links.
- Use Mermaid for architecture, process, dependency, and state relationships; simple steps do not require a diagram.
- Treat current source, workflows, and actual test results as facts. Do not describe planned capabilities as implemented.
