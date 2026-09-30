---
name: format-research
description: 研究 EDA 文件格式、第三方实现和样本证据，为 EasyKiConverter 的解析或导出设计提供可追溯依据。
---

# Format Research Skill

## 适用场景

当任务需要确认 EDA 文件结构、许可证、格式语义、真实样本或第三方算法时使用。本 Skill 只负责研究和证据整理，不直接把第三方代码复制进仓库，也不把代码入口推断为完整格式支持。

## 必读资料

- `docs/developer/ai-development/policy/AI_POLICY.md`
- `docs/developer/ai-development/PROJECT_CONTEXT.md`
- `docs/developer/ai-development/eda-capabilities.json`
- 与目标格式对应的当前解析/导出文档、源码和测试

## 执行边界

1. 记录当前分支、HEAD 和工作区状态，先确认样本、规范或第三方仓库的精确版本。
2. 分开记录格式结构事实、算法思想、许可证限制、样本来源和未验证假设。
3. 只提取数据关系、边界处理和测试策略；按照 EasyKiConverter 的 Parser/Model → IR → Exporter 架构独立实现。
4. 记录缺失字段、非法输入、编码、单位、坐标、重名和未知图元的处理要求。
5. 更新能力台账时分别填写代码入口、自动测试、结构验证和商业 EDA 实机验证；未知信息填写 `unknown`。

## 停止条件

- 许可证、格式语义或样本来源无法确认时，不复制实现或宣称支持。
- 需要真实网络、商业 EDA 或远端写入时停止，并把所需证据列入报告。

## 验收要求

- 研究结论能链接到当前源码、测试、fixture、规范或精确版本的可信资料。
- 新增解析行为由本地 fixture 和失败输入测试覆盖，并运行对应验证计划。
- 报告明确列出未实现、降级和未进行的商业 EDA 验证。
