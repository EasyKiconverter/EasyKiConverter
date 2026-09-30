---
name: parser-development
description: 在 EasyKiConverter 中实现或修复格式 Parser/Model 到统一 IR 的确定性解析流程。
---

# Parser Development Skill

## 适用场景

当任务涉及源格式解析、格式专用模型、Importer、Parser、编码、单位或坐标转换时使用。目标是形成“原始文件 → 专用 Parser/Model → IR”的可测试链路，不负责目标格式 writer 的实现。

## 必读资料

- `docs/developer/ai-development/policy/AI_POLICY.md`
- `docs/developer/ai-development/PROJECT_CONTEXT.md`
- `docs/developer/ai-development/verification-policy.json`
- `docs/developer/ai-development/fixture-provenance.json`
- `docs/developer/ai-development/skills/format-research/SKILL.md`

## 执行边界

1. 用 CodeGraph 先确认 Parser、格式模型、IR Adapter 和调用方，再读取具体文件。
2. 先补正常、空、损坏、缺失字段、非法数字、未知图元、重复名称、缺失关联和坐标变换测试。
3. 非法输入必须产生诊断或失败，不能用 `parseFloat(value) || 0` 一类逻辑静默吞错。
4. 单位、旋转、镜像、编码、路径和多文件关联必须有明确测试；网络测试只使用 Mock。
5. 格式专用字段不能绕过 IR；无法表达的字段必须保留、降级并记录诊断。

## 停止条件

- 真实格式语义或字段含义无法从证据确认时，不猜测字段。
- 缺少样本、fixture 或 IR 表达能力时，先记录限制，不以扩大解析范围替代证据。

## 验收要求

- 解析测试使用仓库 fixture 和临时目录，不访问真实服务。
- 定向测试、格式检查和适用的全量验证结果可复核。
- 能力台账和双语格式文档与实际实现同步，未验证能力保持 `unknown`。
