---
name: exporter-development
description: 在 EasyKiConverter 中实现或修复 IR 到目标 EDA 格式的确定性导出、诊断和结构验证。
---

# Exporter Development Skill

## 适用场景

当任务涉及目标格式 Exporter、Writer、图层映射、伴随文件、引用完整性或导出诊断时使用。第一阶段只处理统一 IR 到目标格式的链路，不把未验证的目标格式宣传为完整支持。

## 必读资料

- `docs/developer/ai-development/policy/AI_POLICY.md`
- `docs/developer/ai-development/PROJECT_CONTEXT.md`
- `docs/developer/ai-development/verification-policy.json`
- `docs/developer/ai-development/eda-capabilities.json`
- 目标格式文档、源码、测试和 golden/结构回读证据

## 执行边界

1. 用 CodeGraph 确认 IR、Adapter、ExporterFactory、CLI/GUI 路由和输出阶段的调用关系。
2. 先建立字段映射和引用不变量；Padstack、Pin、Shape、模型、伴随文件和 manifest 的引用缺失必须可见。
3. 不能表达的制造或几何语义必须失败、降级并进入 ConversionReport，禁止静默改成另一种形状。
4. 文件名、路径、编码、特殊字符和覆盖策略必须有安全测试；路径测试使用临时目录。
5. 结构回读、golden、自动测试和商业 EDA 实机验证分别记录，不能相互替代。

## 停止条件

- 目标格式数据库版本或关键语义无法确认时，不伪造 native writer。
- 缺少商业 EDA 环境时，只报告 Import Package、结构校验或自动化测试结果。
- 关键引用或数据完整性无法保证时，导出必须失败而不是继续生成误导性文件。

## 验收要求

- 新增导出器接入现有 IR 和流水线，不从 QML 直接调用 writer。
- 定向测试、结构/golden 测试、诊断测试、格式检查和适用构建均有实际退出状态。
- 能力台账记录当前 commit/ref、已知损失和未验证边界。
