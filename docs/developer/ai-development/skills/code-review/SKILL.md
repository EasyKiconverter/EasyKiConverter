# Code Review Skill

## 适用场景

当用户要求审查工作区、暂存区、提交或 Pull Request 时使用。本 Skill 严格只读，不实现修复、不提交、不推送、不修改远端设置。

## 必读资料

- `AGENTS.md`
- `PROJECT_INSTRUCTIONS.md`
- `CLAUDE.md`
- `docs/developer/ai-development/PROJECT_CONTEXT.md`
- 与改动领域相关的架构和测试文档

## 执行步骤

1. 先运行 `git status --short`、`git diff --stat`、`git diff --name-only`；暂存区审查另加 `git diff --cached --stat` 和 `git diff --cached --name-only`。
2. 确认审查基线、HEAD、目标分支和改动范围，不把工作区已有改动归因于当前提交。
3. 仅读取关键 diff 和相关调用链；仓库存在 `.codegraph/` 时优先查询符号关系。
4. 按 P0/P1/P2/P3 风险排序，说明文件、位置、触发条件、用户影响和证据。
5. 检查测试是否真正覆盖失败模式，区分静态推断、自动化验证和商业工具验证。
6. 最后说明未发现的问题、未执行的验证和剩余风险。

## 禁止事项

- 不因为发现问题就直接编辑文件。
- 不把测试通过推断为跨平台、安装包或商业 EDA 实机通过。
- 不凭文件名、提交标题或用户描述猜测代码行为。
- 不输出无证据的确定性根因；证据不足时明确说明需要的日志或复现。
