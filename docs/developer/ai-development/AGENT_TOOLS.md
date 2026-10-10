# Agent JSON 工具

[English version](AGENT_TOOLS_en.md)

本工具层为 Agent 提供结构化、可复核且权限有限的项目接口。实现位于 [`tools/python/agent_tools.py`](../../../tools/python/agent_tools.py)，复用现有变更分类器、验证策略、AI 文档校验器、EDA 能力台账和 fixture provenance，不维护第二套规则。

## 调用方式

```bash
python3 tools/python/agent_tools.py <tool> [arguments]
```

所有结果输出 JSON，并通过退出码表示 `ok`。工具不会访问网络、提交、推送或修改 Git 历史。

| 工具 | 用途 | 默认副作用 |
| --- | --- | --- |
| `inspect_changes` | 查询基线、HEAD、工作区和完整变更分类 | 只读 |
| `plan_verification` | 根据分类和验证策略生成步骤、理由和风险 | 只规划 |
| `validate_project_docs` | 校验 AI 文档、Skill、工作流、能力和 fixture 元数据 | 只读 |
| `query_capability` | 查询格式和 artifact 的代码、测试、结构验证和实机验证状态 | 只读 |
| `query_fixture` | 查询 fixture 来源、哈希、测试用途和不变量 | 只读 |
| `run_check` | 执行固定白名单中的格式、文档、Python、构建或 CTest 检查 | 仅白名单检查 |
| `generate_evidence_report` | 根据实际输入结果生成结构化 Evidence Report | 默认 stdout；指定 `--output` 写入仓库内文件，覆盖已有文件必须显式使用 `--force` |

## 安全边界

- `run_check` 只接受固定检查名，不接受任意 Shell、任意脚本或任意测试路径。
- 工具不自动联网，不调用商业 EDA 软件，不把自动测试结果转换成商业软件兼容性结论。
- `inspect_changes`、查询和校验工具只读；`run_check` 可能生成构建目录和日志。
- `generate_evidence_report` 不补造缺失事实，未提供的商业 EDA 验证保持 `unknown`。
- Evidence Report 输出路径必须位于仓库目录内；已有文件默认不会覆盖，只有显式 `--force` 才允许覆盖。
- Python CLI 是当前稳定接口；MCP 暴露层属于后续阶段，当前不宣称已实现跨 Agent 自动发现。

## 示例

```bash
python3 tools/python/agent_tools.py inspect_changes \
  --base origin/v3.1.13 --head HEAD
python3 tools/python/agent_tools.py plan_verification --classification full
python3 tools/python/agent_tools.py query_capability --format kicad --artifact symbol
python3 tools/python/agent_tools.py run_check --name ai_consistency
```

`plan_verification` 的分类结果只用于选择最低验证范围，Importer、Exporter、IR 和具体 EDA 格式仍必须依据测试指南和能力台账补充模块级验证。
