---
name: comment-maintenance
description: 审查并修复 EasyKiConverter C++ 注释、中文 Doxygen、变量说明和注释检查规则。
---

# Comment Maintenance Skill

## 适用场景

当任务要求补齐、修正或审查 C++ 注释，尤其涉及 Doxygen、函数参数、成员变量、局部变量、常量、枚举值或注释检查器时使用。本 Skill 只处理注释可维护性，不把注释检查误认为业务语义、编译、测试或商业 EDA 验证。

## 必读资料

- `AGENTS.md`
- `PROJECT_INSTRUCTIONS.md`
- `docs/developer/ai-development/policy/AI_POLICY.md`
- `docs/developer/CODING_STYLE.md`
- `tools/python/check_comment_rate.py`
- `tools/python/format_code.py`

## 执行边界

- 开始前运行 `git status --short --branch`，保留其他 Agent 或用户的修改；默认不提交、不推送、不创建 PR。
- 只修改任务涉及的文件。审查已有脏工作区时，用 `git diff HEAD --name-only -- '*.cpp' '*.h'` 明确范围，不覆盖未授权的工作树内容。
- 公共函数、类、结构体、枚举和修改过的接口使用简体中文多行 Doxygen；函数有参数时逐一使用 `@param`，有返回值时使用 `@return`。
- 变量注释覆盖成员变量、函数参数、局部变量、临时中间结果、常量和枚举值。参数说明写入函数 Doxygen，不在参数列表中插入注释；局部变量用紧邻的 `//`，声明或成员用 `///<`。
- 函数、构造函数和析构函数的注释统一使用多行形式：`/**`、缩进的 `* @brief`、可选的 `@param`/`@return`、`*/`；禁止使用 `/** @brief ... */` 单行形式，也不要把 `///<` 写在 `return` 语句或参数列表中。
- 注释必须描述真实语义、所有权、单位、生命周期、映射目的或安全边界；禁止用“当前值”“待处理对象”等无信息的模板文本掩盖缺少理解。
- 只为解释意图、约束或非显然关系添加注释；不要给每个自解释表达式添加重复噪声，也不要把第三方实现说明伪装成本项目事实。

## 推荐流程

1. 先阅读调用关系和目标文件，必要时在存在 `.codegraph/` 时使用 CodeGraph；区分文件级、类型级、函数级和变量级注释。
2. 先修复注释格式和语义，再修改检查器。检查器规则必须服务于项目规范，不能为了提高数字而放宽规则、删除测试或添加无意义注释。
3. 使用项目工具验证当前范围：

   ```bash
   .venv/bin/python tools/python/check_comment_rate.py \
       --threshold 100 --check-doxygen --check-variables --check-params \
       --forbid-generic-comments \
       <涉及的 .cpp/.h 文件>
   .venv/bin/python tools/python/format_code.py --check --cpp --dirs src tests
   git diff --check
   ```

4. 若修改了检查器或公共 C++ 文件，运行对应 Python 定向测试、目标 C++ 测试；按 `verification-policy.json` 决定是否构建和运行更大范围测试。无头 Qt 测试设置 `QT_QPA_PLATFORM=offscreen`。
5. 报告检查器统计、格式化、构建和测试的实际退出状态，并明确未运行项目验证、GUI 验证或商业 EDA 验证。

## 停止条件

- 无法确认变量或函数的真实语义时，停止自动补注并保留待人工确认项；不要编造用途、单位或生命周期。
- 发现目标文件包含其他 Agent 未完成的代码改动时，只处理注释边界；若无法避免冲突，停止并报告文件和行范围。
- 检查器需要放宽规则、跳过失败测试或改变运行时行为才能通过时，停止并报告原因。

## 验收要求

- 修改范围内的函数 Doxygen、参数、变量和控制流注释符合项目规则，且注释与实现语义一致。
- 检查器有正向和反向测试；失败诊断包含文件和行号。
- 格式检查、相关测试和 `git diff --check` 的结果均被如实记录；注释检查通过不等于功能验证通过。
