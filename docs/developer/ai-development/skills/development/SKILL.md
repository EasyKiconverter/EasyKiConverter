# Development Skill

## 适用场景

当任务需要理解 EasyKiConverter 架构、定位代码责任边界并实施功能或修复时使用。本 Skill 可能修改工作区，但默认不提交、不推送、不创建 PR。

## 必读资料

- `AGENTS.md`
- `PROJECT_INSTRUCTIONS.md`
- `CLAUDE.md`
- `docs/developer/ai-development/PROJECT_CONTEXT.md`
- `docs/developer/ai-development/policy/AI_POLICY.md`
- 与任务相关的架构、格式、缓存、导出或 UI 文档

仓库存在 `.codegraph/` 时，先运行 CodeGraph 查询相关符号和调用关系，再阅读具体文件。

## 执行步骤

1. 运行 `git status --short --branch`，记录并保护已有修改。
2. 记录基线 ref/commit，并使用 `verification-policy.json` 确认最低验证范围。
3. 明确任务边界、当前实现、目标行为和不在范围内的功能。
4. 按 `QML → ViewModel → Service → Core/IR/Exporter` 追踪调用，不跨层塞入业务逻辑。
5. 先补最小失败测试或 fixture，再实现代码；网络使用 Mock，文件测试使用临时目录。
6. 公共 C++ 接口补简体中文 Doxygen，诊断不可静默丢失数据。
7. 使用 [AI 测试指南](../../TESTING_GUIDE.md) 选择验证项。
8. 检查 `git diff --check`、改动范围和工作区状态，并生成 Evidence Report。

## 停止条件

- 无法确认目标格式语义、接口契约或用户已有修改范围时，停止扩大改动并报告证据。
- 需要修改远端设置、提交、推送或创建 PR 时，停止并等待用户明确授权。
- 商业 EDA 验证工具不存在时，只报告结构测试和自动化测试结果。

## 禁止事项

- 不使用系统 Qt 或系统 Python 代替项目环境。
- 不直接复制第三方项目代码，不让格式专用字段绕过 IR。
- 不使用真实网络测试，不修改无关功能，不用测试删除或弱化来绕过失败。
