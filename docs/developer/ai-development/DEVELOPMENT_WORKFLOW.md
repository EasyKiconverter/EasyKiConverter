# AI 开发工作流

[English version](DEVELOPMENT_WORKFLOW_en.md)

本流程用于代码、测试、CI 和文档任务。它不授权远端写入；提交、推送或创建 PR 必须由用户明确要求。

## 1. 理解任务和边界

- 先读取 `AGENTS.md`、`CLAUDE.md`、`PROJECT_INSTRUCTIONS.md` 和相关领域文档。
- 仓库存在 `.codegraph/` 时，先使用 CodeGraph 定位符号、调用关系和责任边界。
- 先确认任务影响的是 Importer、IR、Exporter、Service、ViewModel、QML、工具、CI 还是文档。
- 不把“规划中”“有类名”“有 UI 入口”当成已完成能力。

## 2. 检查工作区和分支

```bash
git status --short --branch
git diff --stat
git branch --show-current
```

保留用户已有修改，不使用破坏性 reset 或 checkout 覆盖它们。新工作应从合适的基线创建独立分支，不直接在版本分支上提交。

## 3. 实现和控制范围

- 先读取实际文件，再修改；不要凭文件名猜测接口或配置。
- 保持现有层次：QML → ViewModel → Service → Core/IR/Exporter。
- 源格式导入遵循“原始文件 → 专用 Parser/Model → IR”；目标格式导出遵循“IR → 专用 Exporter/Writer → 目标文件”。无法表达的数据必须保留、降级并给出诊断。
- 所有 HTTP 通过 `NetworkClient`；测试使用 Mock。
- 公共 C++ 接口使用简体中文 Doxygen 注释，新增代码遵循现有格式和注释率要求。
- 文档改动保持中英文同步；架构和流程图使用 Mermaid。

## 4. 按风险验证

根据 [AI 测试指南](TESTING_GUIDE.md) 选择验证项。至少执行与改动直接相关的格式检查、定向测试和文档检查；涉及 C++、CMake、测试基础设施或 CI 时，执行构建和相应全量测试。

## 5. 审查改动

```bash
git diff --check
git diff --stat
git status --short --branch
```

确认没有无关文件、系统 Qt、系统 Python、真实网络测试、误改应用身份或未记录的降级行为。商业 EDA、真实桌面窗口和跨平台 CI 只能在实际执行后报告为已验证。

## 6. 提交和 PR

- 只有用户明确要求时才提交、推送或创建 PR。
- 提交使用中文 Conventional Commit，正文每条使用 `-` 说明实际变更。
- 创建 PR 前确认源分支、目标分支、活跃 GitHub 账号、提交范围和验证结果。
- 远端治理、分支保护和组织设置不属于普通代码任务的隐含授权。
