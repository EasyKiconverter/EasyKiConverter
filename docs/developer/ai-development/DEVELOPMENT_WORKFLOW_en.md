# AI Development Workflow

[中文版](DEVELOPMENT_WORKFLOW.md)

This workflow covers code, tests, CI, and documentation tasks. It does not authorize remote writes; commits, pushes, and PR creation require an explicit user request.

Read the [AI Collaboration Policy](policy/AI_POLICY_en.md) first and use the [Evidence Report template](EVIDENCE_REPORT_TEMPLATE_en.md) to record actual evidence at the end.

## 1. Understand the task and boundary

- Read `AGENTS.md`, `CLAUDE.md`, `PROJECT_INSTRUCTIONS.md`, and the relevant domain documents first.
- Root rule files are tool adapters; the repository [AI Collaboration Policy](policy/AI_POLICY_en.md) defines AI collaboration boundaries.
- When `.codegraph/` exists, use CodeGraph first to locate symbols, call paths, and ownership boundaries.
- Identify whether the task affects importers, IR, exporters, services, view models, QML, tools, CI, or documentation.
- Do not treat a plan, a class name, or a UI entry as proof that a capability is complete.

## 2. Check the worktree and branch

```bash
git status --short --branch
git diff --stat
git branch --show-current
```

Preserve existing user changes. Do not use destructive reset or checkout commands to overwrite them. Start new work on a dedicated branch from the appropriate baseline rather than committing directly on a version branch.

## 3. Implement within the architecture

- Read the actual files before editing; do not infer interfaces or settings from filenames.
- Preserve the existing layers: QML → ViewModel → Service → Core/IR/Exporter.
- Source-format import follows “raw file → format-specific parser/model → IR”; target-format export follows “IR → target exporter/writer → target file”. Data that cannot be expressed must be retained, degraded with a diagnostic, or explicitly rejected.
- Route all HTTP through `NetworkClient`; use mocks in tests.
- Add Simplified Chinese Doxygen comments to public C++ interfaces and follow the existing comment-rate policy.
- Keep Chinese and English documentation synchronized; use Mermaid for architecture and process diagrams.

## 4. Verify by risk

Use the [AI testing guide](TESTING_GUIDE_en.md) and the machine-readable [verification policy](verification-policy.json). Run format checks, focused tests, and documentation checks relevant to the change. Changes to C++, CMake, test infrastructure, or CI require a build and the applicable full tests.

## 5. Review the change

```bash
git diff --check
git diff --stat
git status --short --branch
```

Check for unrelated files, system Qt, system Python, real-network tests, accidental application-identity changes, and undocumented degradation. Report commercial EDA, real desktop, and cross-platform CI validation only when actually executed.
Record the baseline ref/commit, actual commands with exit status, and a reason for every check not run.

## 6. Commit and PR

- Commit, push, and create PRs only after an explicit user request.
- Use Chinese Conventional Commits with a `-` bullet for each body item.
- Before creating a PR, confirm source branch, target branch, active GitHub account, commit scope, and verification results.
- Remote governance, branch protection, and organization settings are not implied authorization in a normal code task.
