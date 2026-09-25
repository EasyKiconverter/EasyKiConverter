# AI-Assisted Development

[中文版](README.md)

This entry point is for contributors and AI Agents working on EasyKiConverter. It provides project context, authoritative rules, development workflow, verification entry points, and reusable task-oriented Skills.

## Recommended reading order

1. The [AI Collaboration Policy](policy/AI_POLICY_en.md) is the repository's single authoritative AI policy source; root `AGENTS.md`, `PROJECT_INSTRUCTIONS.md`, and `CLAUDE.md` are tool adapters.
2. [Project context](PROJECT_CONTEXT_en.md) explains architecture boundaries and current capabilities.
3. [Development workflow](DEVELOPMENT_WORKFLOW_en.md) describes the path from task understanding to a local pre-commit state.
4. [AI testing guide](TESTING_GUIDE_en.md) maps change types to verification; detailed testing policy remains in the [testing guide](../TESTING_GUIDE_en.md).
5. [Skills catalog](SKILLS_CATALOG_en.md) lists manually invokable task procedures.
6. The [Evidence Report template](EVIDENCE_REPORT_TEMPLATE_en.md) standardizes baselines, commands, blockers, and commercial EDA evidence.
7. [Agent adapter notes](ADAPTERS_en.md) explain the boundary between local entries and repository policy.
8. [Agent JSON tools](AGENT_TOOLS_en.md) provide read-only queries, verification planning, and allowlisted checks.

## Authoritative rules and supporting material

| Topic | Authoritative file |
| --- | --- |
| AI collaboration policy | [AI Collaboration Policy](policy/AI_POLICY_en.md) |
| Tool adapter files | `AGENTS.md`, `PROJECT_INSTRUCTIONS.md`, `CLAUDE.md` |
| Architecture and module ownership | [Architecture](../ARCHITECTURE_en.md) |
| Build environment | [Build Guide](../BUILD_en.md) |
| Testing policy | [Testing Guide](../TESTING_GUIDE_en.md) |
| Coding conventions | [Coding Style](../CODING_STYLE_en.md) |
| Contributions and PRs | [Contributing Guide](../CONTRIBUTING_en.md) |
| Documentation maintenance | [Documentation Maintenance](../DOCUMENTATION_MAINTENANCE.md) |

This directory provides the policy entry point, AI navigation, and task procedures; it does not copy the complete adapter files. When documents conflict, follow the current branch's source, tests, workflows, and the [AI Collaboration Policy](policy/AI_POLICY_en.md).

## Scope

- Skills are for repeatable, bounded tasks with clear side effects.
- A Skill does not replace project rules, tests, or validation in commercial EDA applications.
- The repository has no unified cross-Agent Skill discovery configuration. Select and read the relevant `SKILL.md` and `SKILL.meta.json` through the [Skills catalog](SKILLS_CATALOG_en.md).

## Machine-checkable control plane

- The [verification policy](verification-policy.json) is the shared minimum-verification source for CI and the Testing Skill; the change classifier selects scope but does not replace module-level verification.
- `tools/python/verification_plan.py` consumes this policy and runs in Docs Check to ensure every minimum step has an executable command or an explicit selector.
- The [EDA capability ledger](eda-capabilities.json) separates code entry points, automated tests, structural checks, and commercial EDA validation.
- [Fixture provenance](fixture-provenance.json) records local fixture hashes, test use, and unknown provenance without inferring facts from content.
- [AI Agent regression evaluation](AGENT_EVAL_en.md) provides rule-boundary examples that do not call paid models.

## Maintenance rules

- Long-lived project documentation should have synchronized Chinese and English versions with reciprocal links.
- Use Mermaid for architecture, process, dependency, and state relationships; simple steps do not require a diagram.
- Treat current source, workflows, and actual test results as facts. Do not describe planned capabilities as implemented.
- Policy text and machine-readable fact files must be tracked by Git. Personal Agent configuration may remain local but cannot replace repository policy.
