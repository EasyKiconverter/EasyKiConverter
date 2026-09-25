# Documentation Skill

## 适用场景

当任务需要新增、修订或同步 EasyKiConverter 文档时使用。本 Skill 可能修改文档和导航文件，但默认不提交、不推送、不创建 PR。

## 必读资料

- `AGENTS.md`
- `PROJECT_INSTRUCTIONS.md`
- `docs/README.md` 和 `docs/README_en.md`
- `docs/developer/DOCUMENTATION_MAINTENANCE.md`
- `mkdocs.yml`
- 对应源码、测试和 workflow

## 执行步骤

1. 检查工作区状态，保留无关修改。
2. 先读取源码、测试、CI 和已有文档，确认事实、版本和能力边界。
3. 中文和英文文档保持相同结构、结论和范围，并互相链接。
4. 架构、流程、时序、依赖或数据流使用 Mermaid；正文说明图的范围和未实现部分。
5. 更新必要的 `docs/README*`、`docs/index*` 和 `mkdocs.yml` 导航，避免孤立页面。
6. 使用 `.venv/bin/python tools/python/build_docs.py --mkdocs` 构建文档，并运行 `git diff --check`。
7. 检查旧链接、过期版本、应用身份和历史记录；历史事实不要机械改写。

## 停止条件

- 无法从源码或实际 Release/CI 证实的能力不得写成已支持。
- 需要修改运行时行为、应用身份或远端 Pages/组织设置时，停止并拆分为独立任务。

## 验收要求

- 中文和英文文件存在且互链。
- MkDocs 构建通过，Mermaid 代码块可解析。
- 新页面从现有开发者入口可达。
- 文档明确区分已实现、进行中、计划和未验证能力。
